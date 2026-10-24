"""Exercise the real IDF v5.4.4 fractional-clock helper on the host.

This predicts the PLL divider. It is not an oscilloscope measurement.
Run with IDF_PATH set to the SDK used to build the firmware.
"""
from pathlib import Path
import os,re,subprocess,tempfile
r=Path(__file__).resolve().parents[1];idf=Path(os.environ['IDF_PATH'])
ll=(idf/'components/hal/esp32s3/include/hal/lcd_ll.h').read_text()
assert re.search(r'#define LCD_LL_CLK_FRAC_DIV_N_MAX\s+256\b',ll)
assert re.search(r'#define LCD_LL_CLK_FRAC_DIV_AB_MAX\s+64\b',ll)
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp);(d/'hal').mkdir()
 (d/'hal/assert.h').write_text('#include <assert.h>\n#define HAL_ASSERT(e) assert(e)\n')
 (d/'clock.c').write_text('''
#include <assert.h>
#include <math.h>
#include <stdio.h>
#include "hal/hal_utils.h"
#include "video_pixels.h"
int main(void){
 hal_utils_clk_info_t c={.src_freq_hz=240000000,.exp_freq_hz=VIDEO_SAMPLES*2,
  .max_integ=256,.min_integ=2,.max_fract=64};
 hal_utils_clk_div_t d;unsigned reported=hal_utils_calc_clk_div_frac_fast(&c,&d)/2;
 double sample=240000000.0/(d.integer+(double)d.numerator/d.denominator)/2;
 assert(d.integer==8&&d.numerator*21==d.denominator*8);
 assert(fabs(sample/4-315000000.0/88)<.01);
 assert(reported==14318182);
 printf("PASS: real IDF fractional clock helper: PLL240M / (%%u + %%u/%%u) / 2; predicted sample %%.6f Hz, carrier %%.6f Hz, line %%.6f Hz; software prediction only\\n",
  d.integer,d.numerator,d.denominator,sample,sample/4,sample/VIDEO_LINE_SAMPLES);
}
'''.replace('%%','%'))
 exe=d/'clock'
 subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-I'+str(d),'-I'+str(idf/'components/hal/include'),'-I'+str(r/'shared_components/freslest_video/include'),str(d/'clock.c'),str(idf/'components/hal/hal_utils.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True)
