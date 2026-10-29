"""Doom-owned indexed palette to physical RGB332 words; no LCD round trip."""
from pathlib import Path
import os,subprocess,sys,tempfile,unittest
R=Path(__file__).resolve().parents[1]

@unittest.skipIf(sys.platform=='win32','Native C checks run in the firmware job')
class NativeRgb332(unittest.TestCase):
 def test_all_colors_fit_borders_bounds_palette_and_cursor(self):
  test=r'''
#include <assert.h>
#include <stdint.h>
#include <string.h>
#include "doom_rgb332.h"
static uint16_t memory[640*480+2];
static uint8_t pixels[320*240],palette[768];
int main(void){
 assert(!doom_rgb332_frame(memory+1,pixels,false,0,0,90));
 for(unsigned i=0;i<256;++i){palette[i*3]=i&224;palette[i*3+1]=(i&28)<<3;palette[i*3+2]=(i&3)<<6;}
 for(unsigned i=0;i<sizeof(pixels);++i)pixels[i]=(uint8_t)i;
 doom_rgb332_palette(palette);
 assert(!doom_rgb332_frame(NULL,pixels,false,0,0,90));
 assert(!doom_rgb332_frame(memory+1,NULL,false,0,0,90));
 for(unsigned fit=80;fit<=100;fit+=5){
  memset(memory,255,sizeof(memory));
  assert(doom_rgb332_frame(memory+1,pixels,false,0,0,fit));
  int w=640*fit/100,h=480*fit/100,left=(640-w)/2,top=(480-h)/2;
  for(int y=0;y<480;++y)for(int x=0;x<640;++x){
   unsigned want=x<left||x>=left+w||y<top||y>=top+h?0:pixels[(y-top)*240/h*320+(x-left)*320/w];
   assert(memory[1+y*640+x]==want);
  }
  assert(memory[0]==65535&&memory[640*480+1]==65535);
 }
 memset(palette,255,sizeof(palette));doom_rgb332_palette(palette);
 assert(doom_rgb332_frame(memory+1,pixels,true,0,0,100));
 assert(memory[1]==0&&memory[1+100*640+100]==255);
 uint8_t *bytes=(uint8_t*)(memory+1);
 for(unsigned i=0;i<640*480;++i)bytes[i]=(uint8_t)i;
 doom_rgb332_expand(memory+1,640*480);
 for(unsigned i=0;i<640*480;++i)assert(memory[i+1]==(uint8_t)i);
 assert(memory[0]==65535&&memory[640*480+1]==65535);
}
'''
  with tempfile.TemporaryDirectory() as folder:
   p=Path(folder)/'rgb332.c';p.write_text(test);exe=p.with_suffix('')
   subprocess.run(['gcc','-std=c17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-pie','-no-pie',
    '-I'+str(R/'Doom/components/freslest_video/include'),'-I'+str(R/'shared_components/freslest_video/include'),str(p),
    str(R/'Doom/components/freslest_video/doom_rgb332.c'),str(R/'shared_components/freslest_video/vga_pixels.c'),'-o',str(exe)],check=True)
   subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})

if __name__=='__main__':unittest.main()
