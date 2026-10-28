"""Actual Doom palette upload applies selected gamma to VGA and LCD channels."""
from pathlib import Path
import subprocess,tempfile,os
r=Path(__file__).resolve().parents[1]
s=(r/'Doom/components/prboom-esp32-compat/i_video.c').read_text()
body=s[s.index('int16_t lcdpal[256];'):s.index('unsigned char *screenbuf;')]
pre=r'''
#include <stdint.h>
#include <string.h>
#include <assert.h>
#include <freslest_video.h>
#include <GAMMATBL.h>
typedef uint8_t byte;
#define LO_INFO 0
void lprintf(int level,const char*fmt,...){(void)level;(void)fmt;}
static int rawCalls,lcdCalls,unlocks;static int vga=1;
int usegamma;
static uint8_t playpal[2*768],captured[768];
static int16_t captured565[256];
int W_GetNumForName(const char*n){assert(!strcmp(n,"PLAYPAL"));return 1;}
const byte* W_CacheLumpNum(int n){assert(n==1);return playpal;}
void W_UnlockLumpNum(int n){assert(n==1);++unlocks;}
freslest_display_t freslest_video_output(void){return vga?FRESLEST_DISPLAY_VGA:FRESLEST_DISPLAY_LCD;}
void spi_lcd_set_palette(const int16_t*p){assert(p);memcpy(captured565,p,sizeof(captured565));++lcdCalls;}
void spi_lcd_set_palette_rgb888(const uint8_t*p){assert(p);memcpy(captured,p,768);++rawCalls;}
'''
test=r'''
int main(void){
 for(unsigned i=0;i<sizeof(playpal);i++)playpal[i]=i;
 const uint8_t fixture[]={64,128,192,0,255,31};memcpy(playpal+768,fixture,sizeof(fixture));
 uint8_t original[sizeof(playpal)];memcpy(original,playpal,sizeof(playpal));
 const uint8_t want[5][6]={{65,128,192,1,255,32},{77,140,199,2,255,41},{91,153,206,4,255,54},{108,166,214,8,255,70},{128,181,221,16,255,90}};
 const uint16_t wire565[]={0x1844,0x784c,0xd95c,0x3a6d,0xbb85};
 for(int gamma=0;gamma<5;gamma++){
  usegamma=gamma;vga=1;I_SetPalette(1);assert(!memcmp(captured,want[gamma],6));
  assert(rawCalls==gamma+1&&lcdCalls==gamma&&unlocks==2*gamma+1);
  vga=0;I_SetPalette(1);assert((uint16_t)captured565[0]==wire565[gamma]);
  assert(rawCalls==gamma+1&&lcdCalls==gamma+1&&unlocks==2*gamma+2);
  assert(!memcmp(original,playpal,sizeof(playpal)));
 }
 // Bad persisted settings must not index outside the five gamma tables.
 vga=1;usegamma=-1;I_SetPalette(1);assert(!memcmp(captured,want[0],6));
 usegamma=5;I_SetPalette(1);assert(!memcmp(captured,want[0],6));
 usegamma=4;I_SetPalette(0);assert(captured[0]==16&&captured[1]==23&&captured[2]==28);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'palette.c';p.write_text(pre+body+test);exe=p.with_suffix('')
 subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-Wno-unused-variable','-g','-fsanitize=address,undefined','-fno-pie','-no-pie','-I'+str(r/'shared_components/freslest_video/include'),'-I'+str(r/'Doom/components/prboom-wad-tables/include'),str(p),str(r/'Doom/components/prboom-wad-tables/GAMMATBL.c'),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('PASS: actual I_SetPalette gamma 0..4, VGA RGB888, LCD RGB565/endian, selected PLAYPAL, invalid gamma fallback and unchanged WAD bytes')
