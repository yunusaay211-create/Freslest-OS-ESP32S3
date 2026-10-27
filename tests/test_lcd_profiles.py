"""Host regression tests for both physical LCDs; no WAD or hardware required."""
from pathlib import Path
import os
import subprocess
import sys
import tempfile
import unittest

ROOT = Path(__file__).resolve().parents[1]


@unittest.skipIf(sys.platform == 'win32', 'C/C++ sanitizer tests run in the Linux firmware job')
class LcdProfiles(unittest.TestCase):
    def test_os_present_scales_full_ui_and_restores_pointer(self):
        source = (ROOT / 'OS/freslest/gfx.cpp').read_text()
        start = source.index('static __attribute__((noinline,noclone)) void gfxCursor(')
        body = source[start:]
        pre = r'''
#include <cassert>
#include <cstdint>
#include <cstring>
#include "lcd_profile.h"
#include "freslest_cursor.h"
#include "freslest_video.h"
void gfxMirrorTick(){}
static bool vga=false;static int vgaCalls;
freslest_display_t freslest_video_output(){return vga?FRESLEST_DISPLAY_VGA:FRESLEST_DISPLAY_LCD;}
void freslest_video_rgb565(const uint16_t*,int w,int h,bool,bool,int,int){assert((w==480&&h==320)||(w==320&&h==240));++vgaCalls;}
struct {int x=240,y=160;bool visible=false;}usbPointer;
static uint16_t buffer[480*320],shadow[480*320],lcd[320*240];
static uint16_t* presented=shadow;
static bool firstPresent=true,oldPointer=false;static int oldX=0,oldY=0;
struct Device{
 int rows=0;void startWrite(){}void endWrite(){}void waitDMA(){}
 void setClipRect(int,int,int,int){assert(false);}void clearClipRect(){}
 void pushImage(int x,int y,int w,int h,const uint16_t* p){
  assert(x==0&&w==320&&h==1&&y>=0&&y<240);
  memcpy(lcd+y*320,p,640);++rows;
 }
}_tft;
struct Frame{int w=480;int width(){return w;}int height(){return w==320?240:320;}void* getBuffer(){return buffer;}
 void pushSprite(Device*,int,int){assert(false);}}frame;
'''
        test = r'''
int main(){
 for(int y=0;y<320;y++)for(int x=0;x<480;x++)buffer[y*480+x]=(x*17+y*31)&65535;
 gfxPresent();assert(_tft.rows==240);
 for(int y=0;y<240;y++)for(int x=0;x<320;x++){
  int sx=(x*480+160)/320,sy=(y*320+120)/240;
  assert(lcd[y*320+x]==buffer[sy*480+sx]);
 }
 assert(lcd[0]==buffer[0]&&lcd[320*240-1]==buffer[480*320-1]);
 _tft.rows=0;gfxPresent();assert(_tft.rows==0);
 // A single dirty source row updates only its mapped destination row.
 buffer[160*480+240]=0xa55a;gfxPresent();assert(_tft.rows==1&&lcd[120*320+160]==0xa55a);
 usbPointer.visible=true;gfxPresent();assert(lcd[120*320+160]==0&&buffer[160*480+240]==0xa55a);
 _tft.rows=0;gfxPresent();assert(_tft.rows==0);
 usbPointer.x=300;usbPointer.y=200;gfxPresent();assert(lcd[120*320+160]==0xa55a);
 assert(lcd[150*320+200]==0);
 usbPointer.visible=false;gfxPresent();assert(lcd[150*320+200]==buffer[200*480+300]);
 usbPointer={479,319,true};gfxPresent();assert(lcd[320*240-1]==0);
 usbPointer.visible=false;gfxPresent();assert(lcd[320*240-1]==buffer[480*320-1]);
 // Native menus must preserve each glyph pixel, without any bitmap resampling.
 frame.w=320;firstPresent=true;
 for(int i=0;i<320*240;i++)buffer[i]=(i*19)&65535;
 gfxPresent();assert(!memcmp(lcd,buffer,320*240*2));
 _tft.rows=0;buffer[100*320+123]=0xa55a;gfxPresent();assert(_tft.rows==1&&lcd[100*320+123]==0xa55a);
 usbPointer={319,239,true};gfxPresent();assert(lcd[320*240-1]==0);
 usbPointer.visible=false;gfxPresent();assert(lcd[320*240-1]==buffer[320*240-1]);
 frame.w=480;vga=true;firstPresent=true;int before=_tft.rows;gfxPresent();assert(vgaCalls==1&&before==_tft.rows);
 // Static menus must not rewrite the whole PSRAM VGA buffer every UI tick.
 for(int i=0;i<60;i++)gfxPresent();assert(vgaCalls==1&&before==_tft.rows);
 buffer[0]^=1;gfxPresent();assert(vgaCalls==2);
 usbPointer={10,10,true};gfxPresent();assert(vgaCalls==3);
 gfxPresent();assert(vgaCalls==3);usbPointer.x++;gfxPresent();assert(vgaCalls==4);
 usbPointer.visible=false;gfxPresent();assert(vgaCalls==5);
 frame.w=320;firstPresent=true;gfxPresent();assert(vgaCalls==6);
 gfxPresent();assert(vgaCalls==6&&before==_tft.rows);

}
'''
        self.compile_run(pre + body + test, 'c++', 9341)

    def test_vga_uses_the_same_lcd_canvas(self):
        source=(ROOT/'OS/freslest/gfx.cpp').read_text()
        dims=source[source.index('int gfxUiWidth()'):source.index('void gfxSetCompactUi(')]
        code='#include <cassert>\nstatic bool compactUi=true;\n'+dims+r'''
int main(){assert(gfxUiWidth()==320&&gfxUiHeight()==240);compactUi=false;assert(gfxUiWidth()==480&&gfxUiHeight()==320);}
'''
        self.compile_run(code,'c++',9341)

    def test_doom_both_lcd_profiles(self):
        for model in (9341, 9488):
            with self.subTest(model=model):
                self.compile_run((ROOT / 'Doom/tests/test_lcd_profiles.c').read_text(), 'c', model)

    def test_native_navigation_and_settings_geometry(self):
        subprocess.run([sys.executable, str(ROOT / 'OS/tests/run_ui_tests.py')], check=True)
        subprocess.run([sys.executable, str(ROOT / 'tests/run_display_menu_tests.py')], check=True,
                       env={**os.environ, 'FRESLEST_TEST_NATIVE_UI':'1'})
        subprocess.run([sys.executable, str(ROOT / 'tests/run_native_wifi_ui_tests.py')], check=True)
        subprocess.run([sys.executable, str(ROOT / 'tests/run_native_app_tests.py')], check=True)
        subprocess.run([sys.executable, str(ROOT / 'tests/run_native_views_tests.py')], check=True)
        subprocess.run([sys.executable, str(ROOT / 'tests/run_canvas_color_tests.py')], check=True)

    def compile_run(self, code, language, model):
        with tempfile.TemporaryDirectory() as temp:
            src = Path(temp) / ('lcd.cpp' if language == 'c++' else 'lcd.c')
            src.write_text(code)
            exe = Path(temp) / 'lcd'
            includes = [ROOT/'OS/freslest', ROOT/'shared_components/freslest_video/include',
                        ROOT/'Doom/tests', ROOT/'Doom/tests/stubs', ROOT/'Doom/main',
                        ROOT/'Doom/components/prboom-esp32-compat/include']
            subprocess.run(['g++' if language == 'c++' else 'gcc',
                            '-std=c++17' if language == 'c++' else '-std=gnu17',
                            '-g', '-Wall', '-Wextra', '-Werror', '-Wno-misleading-indentation',
                            '-fsanitize=address,undefined', '-fno-pie', '-no-pie',
                            f'-DFRESLEST_LCD_MODEL={model}',
                            *['-I'+str(p) for p in includes], str(src), '-o', str(exe)], check=True)
            subprocess.run([str(exe)], check=True, env={**os.environ, 'ASAN_OPTIONS':'detect_leaks=0'})
            if language == 'c':
                self.assertIn(f'FRESLEST_LCD:ILI{model}'.encode(), exe.read_bytes())


if __name__ == '__main__':
    unittest.main()
