from pathlib import Path
import os,subprocess,tempfile
r=Path(__file__).resolve().parents[1];v=r/'shared_components/freslest_av'
headers=['esp_err.h','esp_attr.h','esp_heap_caps.h','esp_cache.h','esp_clk_tree.h','esp_log.h','esp_pm.h','esp_timer.h','esp_rom_gpio.h','esp_rom_sys.h','esp_private/esp_clk_tree_common.h','esp_private/gdma.h','esp_private/gdma_link.h','esp_private/periph_ctrl.h','esp_private/gpio.h','esp_lcd_common.h','hal/lcd_hal.h','hal/lcd_ll.h','hal/dma_types.h','soc/lcd_periph.h','soc/gpio_sig_map.h','soc/io_mux_reg.h','freertos/FreeRTOS.h','freertos/task.h']
with tempfile.TemporaryDirectory() as tmp:
 d=Path(tmp)
 for name in headers:
  f=d/name;f.parent.mkdir(parents=True,exist_ok=True);f.write_text('#include "host_av_driver.h"\n')
 exe=d/'driver'
 subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-Wno-unused-function','-Wno-unused-variable','-g','-fsanitize=address,undefined','-I'+str(d),'-I'+str(r/'tests/av_driver_stubs'),'-I'+str(v/'include'),str(r/'tests/test_av_engine_driver.c'),str(v/'av_signal.c'),str(v/'av_ownership.c'),'-lm','-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
