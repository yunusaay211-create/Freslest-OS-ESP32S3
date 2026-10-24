#include "freslest_update_package.h"
#include <assert.h>
#include <stdlib.h>
#include <string.h>
static unsigned previous;
static void progress(unsigned value,void *context) {
    (void)context;assert(value>=previous&&value<=100);previous=value;
}
int main(int argc,char **argv) {
    assert(argc==2||argc==3);
    FILE *f=fopen(argv[1],"rb");if(!f)return 10;
    freslest_update_package p;char error[160];
    if(!freslest_update_inspect(f,&p,error,sizeof(error))) {fprintf(stderr,"REJECT INSPECT: %s\n",error);fclose(f);return 2;}
    if(argc==3) {printf("COMPATIBLE %s\n",p.version);fclose(f);return 0;}
    uint8_t *image=malloc(p.image_size+32);assert(image);memset(image,0xa5,p.image_size+32);
    bool ok=freslest_update_load(f,&p,image,p.image_size,progress,NULL,error,sizeof(error));
    for(unsigned i=0;i<32;i++)assert(image[p.image_size+i]==0xa5);
    fclose(f);free(image);
    if(!ok) {fprintf(stderr,"REJECT LOAD: %s\n",error);return 3;}
    assert(previous==100);printf("VERIFIED %s %u\n",p.version,p.image_size);return 0;
}
