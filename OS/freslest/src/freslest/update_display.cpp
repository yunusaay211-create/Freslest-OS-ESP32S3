#include "../../gfx.h"
#include "update_display.h"
extern "C" void freslest_update_draw_indexed(const uint8_t *pixels) {
 static const uint16_t palette[]={0,0x10a2,0xffff,0xa514,0x07ff,0x2104,0xf800};uint16_t row[480];
 tft.startWrite();for(int y=0;y<320;y++) {for(int x=0;x<480;x++)row[x]=palette[pixels[y*480+x]<7?pixels[y*480+x]:0];tft.pushImage(0,y,480,1,row);}tft.endWrite();gfxPresent();
}
