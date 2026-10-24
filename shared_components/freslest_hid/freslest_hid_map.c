// Bounded HID report descriptor parser for BLE keyboards and relative mice.
// Unknown usages still advance bit offsets; unsupported layouts fail closed.
#include "freslest_hid_map.h"
#include <string.h>
typedef struct {uint32_t page,size,count,id;int32_t minimum;} globals;
static int report(freslest_hid_map_t *m,unsigned id){
 for(unsigned i=0;i<m->reports;i++)if(m->report[i].id==id)return i;
 if(m->reports==FRESLEST_HID_MAP_REPORTS)return -1;
 int i=m->reports++;m->report[i].id=id;return i;
}
static bool field(freslest_hid_map_t *m,unsigned r,unsigned off,unsigned count,unsigned bits,unsigned usage,unsigned mode,bool sign){
 if(m->fields==FRESLEST_HID_MAP_FIELDS)return false;
 m->field[m->fields++]=(freslest_hid_field_t){off,count,usage,bits,mode,r,sign};
 unsigned kind=mode<=2?1:2;m->report[r].kind|=kind;m->kind|=kind;return true;
}
bool freslest_hid_map_parse(freslest_hid_map_t *m,const uint8_t *d,size_t n){
 if(!m){return false;}memset(m,0,sizeof(*m));if(!d||!n||n>2048)return false;
 globals g={0},stack[4];unsigned depth=0,usages[32],nu=0,umin=0,umax=0;bool range=false;
 for(size_t pos=0;pos<n;){
  uint8_t b=d[pos++];if(b==0xfe)return false;
  unsigned bytes=b&3;if(bytes==3)bytes=4;if(bytes>n-pos)return false;
  uint32_t v=0;for(unsigned j=0;j<bytes;j++)v|=(uint32_t)d[pos++]<<(8*j);
  unsigned type=(b>>2)&3,tag=b>>4;
  if(type==1){
   switch(tag){
    case 0:g.page=v;break;
    case 1:g.minimum=bytes==1?(int8_t)v:bytes==2?(int16_t)v:(int32_t)v;break;
    case 7:g.size=v;break;
    case 8:if(!v||v>255)return false;g.id=v;break;
    case 9:g.count=v;break;
    case 10:if(depth==4)return false;stack[depth++]=g;break;
    case 11:if(!depth)return false;g=stack[--depth];break;
   }
  }else if(type==2){
   if(tag==0){if(nu==32)return false;usages[nu++]=v;}
   else if(tag==1){umin=v;range=true;}else if(tag==2)umax=v;
  }else if(type==0){
   if(tag==8){
    if(!g.size||g.size>32||!g.count||g.count>512)return false;
    int r=report(m,g.id);if(r<0)return false;
    unsigned off=m->report[r].bits,total=g.size*g.count;
    if(off+total>512){return false;}m->report[r].bits+=total;
    if(!(v&1)){
     if(g.page==7&&!(v&2)){
      if(g.size>16||!field(m,r,off,g.count,g.size,0,1,false))return false;
     }else if((v&2)&&g.page==7&&range&&umax>=umin&&umax<=255&&umin+g.count-1<=umax){
      if(!field(m,r,off,g.count,g.size,umin,2,false))return false;
     }else if(v&2){
      if(!range&&g.count>nu&&nu!=1)return false;
      for(unsigned j=0;j<g.count;j++){
       unsigned u=range?(umin+j<=umax?umin+j:umax):usages[j<nu?j:nu-1];
       unsigned mode=g.page==7&&u<256?2:g.page==9&&u>=1&&u<=3?3:
        g.page==1&&(v&4)&&u==0x30?4:g.page==1&&(v&4)&&u==0x31?5:0;
       if(mode&&!field(m,r,off+j*g.size,1,g.size,u,mode,g.minimum<0))return false;
      }
     }
    }
   }
   nu=umin=umax=0;range=false;
  }
 }
 return !depth&&m->kind;
}
static uint32_t bits(const uint8_t *d,unsigned off,unsigned n){
 uint32_t v=0;for(unsigned j=0;j<n;j++)v|=(uint32_t)((d[(off+j)/8]>>((off+j)%8))&1)<<j;return v;
}
bool freslest_hid_map_decode(freslest_hid_map_t *m,unsigned id,const uint8_t *d,size_t n,freslest_hid_decoded_t *out){
 if(!m||!d||!out||n>64)return false;
 int r=-1;for(unsigned i=0;i<m->reports;i++)if(m->report[i].id==id)r=i;
 if(r<0||n<(m->report[r].bits+7u)/8)return false;
 uint8_t keys[32]={0},buttons=0;int dx=0,dy=0;
 for(unsigned i=0;i<m->fields;i++){
  const freslest_hid_field_t *f=&m->field[i];if(f->report!=r)continue;
  for(unsigned j=0;j<f->count;j++){
   uint32_t v=bits(d,f->offset+j*f->size,f->size);unsigned u=f->usage+j;
   if(f->mode==1){if(v>=1&&v<=3)return false;if(v<256&&v)keys[v/8]|=1u<<(v%8);}
   else if(f->mode==2&&u<256&&v)keys[u/8]|=1u<<(u%8);
   else if(f->mode==3&&v)buttons|=1u<<(f->usage-1);
   else if(f->mode>=4){
    int64_t value=v;if(f->sign&&(v&(1u<<(f->size-1))))value-=(int64_t)1<<f->size;
    if(value>2047){value=2047;}if(value<-2047){value=-2047;}
    if(f->mode==4)dx+=(int)value;else dy+=(int)value;
   }
  }
 }
 memcpy(m->report[r].keys,keys,32);m->report[r].buttons=buttons;
 memset(out,0,sizeof(*out));out->kind=m->kind;out->dx=dx;out->dy=dy;
 for(unsigned i=0;i<m->reports;i++){
  for(unsigned j=0;j<32;j++)out->keys[j]|=m->report[i].keys[j];
  out->buttons|=m->report[i].buttons;
 }
 return true;
}
