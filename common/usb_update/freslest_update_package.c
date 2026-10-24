// SPDX-License-Identifier: MIT
// Freslest ZIP update reader. Supports standard stored and raw-DEFLATE ZIPs.
#include "freslest_update_package.h"
#include <ctype.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include "cJSON.h"
#ifdef FRESLEST_UPDATE_HOST_TEST
#include <zlib.h>
#include <openssl/sha.h>
#else
#include "miniz.h"
#include "mbedtls/sha256.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#endif

static uint16_t le16(const uint8_t *p) {return p[0]|((uint16_t)p[1]<<8);}
static uint32_t le32(const uint8_t *p) {return le16(p)|((uint32_t)le16(p+2)<<16);}
static bool fail(char *e,size_t n,const char *s) {if(n)snprintf(e,n,"%s",s);return false;}
static bool read_at(FILE *f,uint32_t at,void *p,size_t n) {
    return at<=INT32_MAX&&fseek(f,(long)at,SEEK_SET)==0&&fread(p,1,n,f)==n;
}
static bool digest(const uint8_t *p,size_t n,uint8_t out[32]) {
#ifdef FRESLEST_UPDATE_HOST_TEST
    return SHA256(p,n,out)!=NULL;
#else
    return mbedtls_sha256(p,n,out,0)==0;
#endif
}
static uint32_t crc_update(uint32_t crc,const uint8_t *p,size_t n) {
    // A small table avoids platform-dependent ROM CRC conventions.
    static uint32_t table[256];static bool ready=false;
    if(!ready) {
        for(unsigned i=0;i<256;i++) {uint32_t c=i;for(int j=0;j<8;j++)c=(c>>1)^((c&1)?0xedb88320u:0);table[i]=c;}
        ready=true;
    }
    crc=~crc;while(n--)crc=table[(crc^*p++)&255]^(crc>>8);return ~crc;
}
static bool canonical(const char *s) {
    if(!*s||*s=='/')return false;
    const char *part=s;
    for(const char *p=s;;p++) {
        unsigned c=(unsigned char)*p;
        if(c&&((c<32)||(c>126)||c=='\\'||c==':'))return false;
        if(!c||c=='/') {
            size_t n=(size_t)(p-part);
            if(!n||(n==1&&part[0]=='.')||(n==2&&part[0]=='.'&&part[1]=='.'))return false;
            if(!c)return true;
            part=p+1;
        }
    }
}
typedef struct {uint32_t start,size,eocd;uint16_t count;} zip_directory;
static bool directory(FILE *f,zip_directory *d,char *e,size_t n) {
    if(fseek(f,0,SEEK_END))return fail(e,n,"Cannot read ZIP");
    long length=ftell(f);
    if(length<22||length>INT32_MAX)return fail(e,n,"Unsupported ZIP size");
    // EOCD is at most 65557 bytes from EOF; read it once, not one USB sector
    // read per byte. A false EOCD inside a comment cannot pass the length test.
    size_t tail=(size_t)length<65557?(size_t)length:65557;
    uint8_t *b=malloc(tail);if(!b)return fail(e,n,"Not enough memory");
    bool found=false;
    if(read_at(f,(uint32_t)length-(uint32_t)tail,b,tail)) {
        for(long i=(long)tail-22;i>=0;i--) {
            if(le32(b+i)!=0x06054b50||i+22+le16(b+i+20)!=(long)tail)continue;
            if(le16(b+i+4)||le16(b+i+6)||le16(b+i+8)!=le16(b+i+10))break;
            d->count=le16(b+i+10);d->size=le32(b+i+12);d->start=le32(b+i+16);
            d->eocd=(uint32_t)length-(uint32_t)tail+(uint32_t)i;
            found=d->count>0&&d->count<=8192&&(uint64_t)d->start+d->size==d->eocd;
            break;
        }
    }
    free(b);return found||fail(e,n,"Invalid, split or ZIP64 archive");
}
static bool entry_at(FILE *f,uint32_t pos,uint32_t end,freslest_zip_entry *v,uint32_t *next) {
    uint8_t h[46];if((uint64_t)pos+46>end||!read_at(f,pos,h,46)||le32(h)!=0x02014b50)return false;
    uint16_t names=le16(h+28),extra=le16(h+30),comment=le16(h+32);
    uint64_t following=(uint64_t)pos+46+names+extra+comment;
    if(following>end||!names||le16(h+34))return false;
    *next=(uint32_t)following;
    memset(v,0,sizeof(*v));
    v->compressed_size=le32(h+20);v->size=le32(h+24);v->local_offset=le32(h+42);
    v->crc=le32(h+16);v->flags=le16(h+8);v->method=le16(h+10);
    // Long unrelated source paths in the complete project ZIP are skipped.
    if(names>=sizeof(v->name))return true;
    if(!read_at(f,pos+46,v->name,names)||memchr(v->name,0,names))return false;
    v->name[names]=0;return true;
}
static bool read_entry(FILE *f,const freslest_zip_entry *v,uint32_t central,
                       uint8_t *out,size_t capacity,freslest_update_progress progress,void *context,
                       char *error,size_t error_size) {
    uint8_t h[30];
    if(!v->size||v->size>capacity||!canonical(v->name)||
       (v->flags&~0x080e)||!(v->method==0||v->method==8)||
       !read_at(f,v->local_offset,h,30)||le32(h)!=0x04034b50||
       le16(h+6)!=v->flags||le16(h+8)!=v->method||le16(h+26)!=strlen(v->name))
        return fail(error,error_size,"Invalid or encrypted ZIP entry");
    char name[192];size_t names=le16(h+26);
    uint64_t data=(uint64_t)v->local_offset+30+names+le16(h+28);
    if(data>central||(uint64_t)data+v->compressed_size>central||
       !read_at(f,v->local_offset+30,name,names)||memcmp(name,v->name,names))
        return fail(error,error_size,"ZIP entry outside archive");
    if(!(v->flags&8)&&(le32(h+14)!=v->crc||le32(h+18)!=v->compressed_size||le32(h+22)!=v->size))
        return fail(error,error_size,"ZIP headers disagree");
    if(v->method==0&&v->compressed_size!=v->size)return fail(error,error_size,"Invalid stored entry");
    uint8_t input[4096];uint32_t consumed=0,produced=0;bool ok=true,done=false;
#ifdef FRESLEST_UPDATE_HOST_TEST
    z_stream inflater={0};
    if(v->method==8&&inflateInit2(&inflater,-15)!=Z_OK)return fail(error,error_size,"Cannot start ZIP decoder");
#else
    tinfl_decompressor *inflater=NULL;
    if(v->method==8) {inflater=malloc(sizeof(*inflater));if(!inflater)return fail(error,error_size,"Cannot start ZIP decoder");tinfl_init(inflater);}
#endif
    while(ok&&consumed<v->compressed_size&&!done) {
        size_t take=v->compressed_size-consumed;if(take>sizeof(input))take=sizeof(input);
        if(!read_at(f,(uint32_t)data+consumed,input,take)) {ok=false;break;}
        if(v->method==0) {memcpy(out+produced,input,take);produced+=(uint32_t)take;consumed+=(uint32_t)take;}
        else {
            size_t in=take,out_n=v->size-produced;
#ifdef FRESLEST_UPDATE_HOST_TEST
            inflater.next_in=input;inflater.avail_in=(uInt)in;
            inflater.next_out=out+produced;inflater.avail_out=(uInt)out_n;
            int status=inflate(&inflater,Z_NO_FLUSH);
            in-=inflater.avail_in;out_n-=inflater.avail_out;
            done=status==Z_STREAM_END;ok=done||status==Z_OK;
#else
            unsigned flags=TINFL_FLAG_USING_NON_WRAPPING_OUTPUT_BUF;
            if(consumed+take<v->compressed_size)flags|=TINFL_FLAG_HAS_MORE_INPUT;
            tinfl_status status=tinfl_decompress(inflater,input,&in,out,out+produced,&out_n,flags);
            done=status==TINFL_STATUS_DONE;
            ok=done||status==TINFL_STATUS_NEEDS_MORE_INPUT||status==TINFL_STATUS_HAS_MORE_OUTPUT;
#endif
            consumed+=(uint32_t)in;produced+=(uint32_t)out_n;
            if(!in&&!out_n)ok=false;
            if(!done&&produced==v->size)ok=false;
        }
        if(progress)progress((unsigned)((uint64_t)produced*100/v->size),context);
#ifndef FRESLEST_UPDATE_HOST_TEST
        vTaskDelay(1);
#endif
    }
#ifdef FRESLEST_UPDATE_HOST_TEST
    if(v->method==8)inflateEnd(&inflater);
#else
    free(inflater);
#endif
    if(!ok||produced!=v->size||consumed!=v->compressed_size||(v->method==8&&!done))
        return fail(error,error_size,"Damaged ZIP data");
    if(crc_update(0,out,produced)!=v->crc)return fail(error,error_size,"ZIP checksum mismatch");
    return true;
}
static const cJSON *unique(const cJSON *o,const char *key) {
    const cJSON *found=NULL;
    for(const cJSON *p=o?o->child:NULL;p;p=p->next)if(p->string&&!strcmp(p->string,key)) {if(found)return NULL;found=p;}
    return found;
}
static bool string_is(const cJSON *o,const char *key,const char *value) {
    const cJSON *p=unique(o,key);return cJSON_IsString(p)&&!strcmp(p->valuestring,value);
}
static int hex(int c) {if(c>='0'&&c<='9')return c-'0';if(c>='a'&&c<='f')return c-'a'+10;if(c>='A'&&c<='F')return c-'A'+10;return -1;}
static bool shallow_json(const char *s) {
    unsigned depth=0;bool quoted=false,escaped=false;
    for(;*s;s++) {
        if(quoted) {
            if(escaped) {if(*s=='u'&&!strncmp(s+1,"0000",4))return false;escaped=false;}
            else if(*s=='\\')escaped=true;
            else if(*s=='"')quoted=false;
        } else if(*s=='"')quoted=true;
        else if(*s=='{'||*s=='[') {if(++depth>16)return false;}
        else if(*s=='}'||*s==']') {if(!depth)return false;--depth;}
    }
    return !depth&&!quoted;
}
bool freslest_update_inspect(FILE *f,freslest_update_package *p,char *e,size_t n) {
    memset(p,0,sizeof(*p));zip_directory d;
    if(!directory(f,&d,e,n))return false;
    freslest_zip_entry manifest={0},v;unsigned matches=0;uint32_t pos=d.start,next;
    for(unsigned i=0;i<d.count;i++,pos=next) {
        if(!entry_at(f,pos,d.eocd,&v,&next))return fail(e,n,"Broken ZIP directory");
        const char *base=strrchr(v.name,'/');base=base?base+1:v.name;
        if(!strcmp(base,FRESLEST_UPDATE_MANIFEST)) {manifest=v;++matches;}
    }
    if(pos!=d.eocd||matches!=1||manifest.size>2048)return fail(e,n,"No unique Freslest manifest");
    char json[2049];
    if(!read_entry(f,&manifest,d.start,(uint8_t*)json,2048,NULL,NULL,e,n))return false;
    if(memchr(json,0,manifest.size))return fail(e,n,"Invalid update manifest");
    json[manifest.size]=0;
    if(!shallow_json(json))return fail(e,n,"Invalid or deeply nested manifest");
    const char *end=NULL;cJSON *root=cJSON_ParseWithLengthOpts(json,manifest.size+1,&end,true);
    if(!root)return fail(e,n,"Invalid update manifest");
    const cJSON *format=unique(root,"format_version"),*revision=unique(root,"updater_min"),*image=unique(root,"image");
    const cJSON *version=unique(root,"version"),*size=unique(image,"size"),*sha=unique(image,"sha256"),*file=unique(image,"file");
    bool ok=cJSON_IsObject(root)&&string_is(root,"format","freslest-usb-update")&&
        string_is(root,"board",FRESLEST_UPDATE_BOARD)&&string_is(root,"layout",FRESLEST_UPDATE_LAYOUT)&&
        cJSON_IsNumber(format)&&format->valuedouble==2&&cJSON_IsNumber(revision)&&revision->valuedouble==2&&
        cJSON_IsObject(image)&&cJSON_IsString(version)&&strlen(version->valuestring)>0&&strlen(version->valuestring)<sizeof(p->version)&&
        cJSON_IsNumber(size)&&size->valuedouble>=288&&size->valuedouble<=FRESLEST_OS_SLOT_SIZE&&
        cJSON_IsString(sha)&&strlen(sha->valuestring)==64&&cJSON_IsString(file)&&
        !strcmp(file->valuestring,"firmware/freslest_os_s3.bin");
    char image_name[192]={0};
    if(ok) {
        p->image_size=(uint32_t)size->valuedouble;ok=(double)p->image_size==size->valuedouble;
        snprintf(p->version,sizeof(p->version),"%s",version->valuestring);
        for(const char *c=p->version;*c;c++)if(!isalnum((unsigned char)*c)&&*c!='.'&&*c!='-'&&*c!='_')ok=false;
        for(unsigned i=0;i<32;i++) {int a=hex(sha->valuestring[i*2]),b=hex(sha->valuestring[i*2+1]);if(a<0||b<0)ok=false;p->sha256[i]=(uint8_t)((a<0?0:a)*16+(b<0?0:b));}
        const char *slash=strrchr(manifest.name,'/');size_t prefix=slash?(size_t)(slash-manifest.name+1):0;
        if(prefix+strlen(file->valuestring)>=sizeof(image_name))ok=false;
        else {memcpy(image_name,manifest.name,prefix);strcpy(image_name+prefix,file->valuestring);}
    }
    cJSON_Delete(root);
    if(!ok)return fail(e,n,"ZIP is not compatible with this Freslest board");
    matches=0;pos=d.start;
    for(unsigned i=0;i<d.count;i++,pos=next) {
        if(!entry_at(f,pos,d.eocd,&v,&next))return fail(e,n,"Broken ZIP directory");
        if(!strcmp(v.name,image_name)) {p->image=v;++matches;}
    }
    if(matches!=1||p->image.size!=p->image_size||!p->image.compressed_size||
       (p->image.flags&~0x080e)||!(p->image.method==0||p->image.method==8))
        return fail(e,n,"Missing, oversized or unsupported OS image");
    return true;
}
bool freslest_update_validate_image(const uint8_t *b,size_t size,const freslest_update_package *p,char *e,size_t n) {
    uint8_t hash[32];
    if(size!=p->image_size||size<288||size>FRESLEST_OS_SLOT_SIZE||!digest(b,size,hash)||memcmp(hash,p->sha256,32))
        return fail(e,n,"Firmware SHA256 mismatch");
    if(b[0]!=0xe9||!b[1]||b[1]>16||le16(b+12)!=9||(b[3]>>4)!=4||b[23]!=1||
       le32(b+32)!=0xabcd5432||!memchr(b+48,0,32)||!memchr(b+80,0,32)||
       strcmp((const char*)b+80,"freslest_os_s3")||strcmp((const char*)b+48,p->version))
        return fail(e,n,"Wrong chip, OS project or firmware version");
    size_t offset=24;uint8_t checksum=0xef;
    for(unsigned i=0;i<b[1];i++) {
        if(offset+8>size)return fail(e,n,"Truncated ESP image");
        size_t length=le32(b+offset+4);offset+=8;
        if(length>size-offset||(i==0&&length<256))return fail(e,n,"Invalid ESP image segment");
        for(size_t j=0;j<length;j++)checksum^=b[offset+j];
        offset+=length;
    }
    size_t aligned=(offset+16)&~(size_t)15;
    if(aligned+32!=size||b[aligned-1]!=checksum||!digest(b,aligned,hash)||memcmp(hash,b+aligned,32))
        return fail(e,n,"Invalid ESP image checksum or digest");
    return true;
}
bool freslest_update_load(FILE *f,const freslest_update_package *p,uint8_t *image,size_t capacity,
                         freslest_update_progress progress,void *context,char *e,size_t n) {
    zip_directory d;if(!directory(f,&d,e,n))return false;
    if(!read_entry(f,&p->image,d.start,image,capacity,progress,context,e,n))return false;
    return freslest_update_validate_image(image,p->image_size,p,e,n);
}
