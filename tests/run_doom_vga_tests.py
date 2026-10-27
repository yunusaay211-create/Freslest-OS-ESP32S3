"""Actual Doom palette upload must bypass LCD RGB565 for VGA."""
from pathlib import Path
import subprocess,tempfile,os
r=Path(__file__).resolve().parents[1]
s=(r/'Doom/components/prboom-esp32-compat/i_video.c').read_text()
body=s[s.index('int16_t lcdpal[256];'):s.index('unsigned char *screenbuf;')]
pre=r'''
#include <stdint.h>
#include <assert.h>
#include <freslest_video.h>
typedef uint8_t byte;
#define LO_INFO 0
void lprintf(int level,const char*fmt,...){(void)level;(void)fmt;}
static int rawCalls,lcdCalls,unlocks;static int vga=1;
static uint8_t playpal[2*768];
int W_GetNumForName(const char*n){(void)n;return 1;}
const byte* W_CacheLumpNum(int n){assert(n==1);return playpal;}
void W_UnlockLumpNum(int n){assert(n==1);++unlocks;}
freslest_display_t freslest_video_output(void){return vga?FRESLEST_DISPLAY_VGA:FRESLEST_DISPLAY_LCD;}
void spi_lcd_set_palette(const int16_t*p){assert(p);++lcdCalls;}
void spi_lcd_set_palette_rgb888(const uint8_t*p){assert(p==playpal+768&&p[0]==7&&p[1]==3&&p[2]==7);++rawCalls;}
'''
test=r'''
int main(){playpal[768]=7;playpal[769]=3;playpal[770]=7;
 I_SetPalette(1);assert(rawCalls==1&&lcdCalls==0&&unlocks==1);
 vga=0;I_SetPalette(1);assert(rawCalls==1&&lcdCalls==1&&unlocks==2);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'palette.c';p.write_text(pre+body+test);exe=p.with_suffix('')
 subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-Wno-unused-variable','-I'+str(r/'shared_components/freslest_video/include'),str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
print('PASS: actual I_SetPalette sends original 8-bit RGB PLAYPAL to dedicated VGA path; LCD palette unchanged')
