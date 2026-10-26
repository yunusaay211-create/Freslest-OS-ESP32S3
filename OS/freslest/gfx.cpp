#include "gfx.h"
#include "screen_mirror.h"
#include "usb_input.h"
#include <freslest_video.h>
#include <freslest_cursor.h>
#include <esp_heap_caps.h>
#include <cstring>

// === LGFX SINIFI ===
class LGFX : public lgfx::LGFX_Device
{
#if FRESLEST_LCD_MODEL == 9341
  lgfx::Panel_ILI9341 _panel;
#else
  lgfx::Panel_ILI9488 _panel;
#endif
  lgfx::Bus_SPI _bus;

public:
  LGFX(void)
  {
    { // BUS
      auto cfg = _bus.config();

      cfg.spi_host = SPI2_HOST;
      cfg.spi_mode = 0;
      cfg.freq_write = 40000000;   // stabil
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;

      cfg.pin_sclk = PIN_TFT_SCK;
      cfg.pin_mosi = PIN_TFT_MOSI;
      cfg.pin_miso = PIN_TFT_MISO;
      cfg.pin_dc   = PIN_TFT_DC;

      _bus.config(cfg);
      _panel.setBus(&_bus);
    }

    { // PANEL
      auto cfg = _panel.config();

      cfg.pin_cs  = PIN_TFT_CS;
      cfg.pin_rst = PIN_TFT_RST;
      cfg.pin_busy = -1;

      // 🔥 DOĞRU BOYUT
      cfg.memory_width  = FRESLEST_LCD_NATIVE_WIDTH;
      cfg.memory_height = FRESLEST_LCD_NATIVE_HEIGHT;
      cfg.panel_width   = FRESLEST_LCD_NATIVE_WIDTH;
      cfg.panel_height  = FRESLEST_LCD_NATIVE_HEIGHT;

      cfg.offset_x = 0;
      cfg.offset_y = 0;

      // 🔥 KRİTİK RENK AYARI
      cfg.rgb_order = false;
      cfg.invert    = FRESLEST_LCD_INVERT;

      _panel.config(cfg);
    }

    setPanel(&_panel);
  }
};

static LGFX _tft;
static lgfx::LGFX_Sprite frame(&_tft);
lgfx::LGFXBase& tft=frame;
static uint16_t* presented=nullptr;
static bool firstPresent=true,oldPointer=false;
static int oldX=0,oldY=0;

static uint32_t _lastFrame = 0;
int gfxUiHeight(){return freslest_video_output()==FRESLEST_DISPLAY_COMPOSITE?360:320;}

// === GFX INIT ===
void gfxBegin()
{
  Serial.println(FRESLEST_LCD_TAG);
  bool composite=freslest_video_boot_output()==FRESLEST_DISPLAY_COMPOSITE;
  if(composite){if(!freslest_video_begin())Serial.println("E411: Composite unavailable; hold BACK during reset for LCD.");}
  else {
    // No LCD/touch writes may follow RGB-DMA initialization on shared pins.
    _tft.init();_tft.setRotation(3);_tft.setColorDepth(16);
    _tft.setSwapBytes(false); // sprite storage is already wire-order RGB565
  }
  frame.setPsram(true);frame.setColorDepth(16);
  if(!frame.createSprite(480,gfxUiHeight())){Serial.println("E410: UI framebuffer unavailable");return;}
  frame.setSwapBytes(true);
  if(!composite)presented=(uint16_t*)heap_caps_malloc(480*320*2,MALLOC_CAP_SPIRAM|MALLOC_CAP_8BIT);

  backlightInit();
}

// ===== Backlight PWM =====
#ifndef BL_CH
#define BL_CH  1
#endif

#ifndef BL_FREQ
#define BL_FREQ 20000
#endif

#ifndef BL_RES
#define BL_RES 10
#endif

void backlightInit()
{
  if (PIN_TFT_BL < 0 || freslest_video_output()==FRESLEST_DISPLAY_COMPOSITE) return;

#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcAttachChannel(PIN_TFT_BL, BL_FREQ, BL_RES, BL_CH);
  ledcWriteChannel(BL_CH, (1 << BL_RES) - 1);
#else
  ledcSetup(BL_CH, BL_FREQ, BL_RES);
  ledcAttachPin(PIN_TFT_BL, BL_CH);
  ledcWrite(BL_CH, (1 << BL_RES) - 1);
#endif
}

void backlightSet(uint8_t level255)
{
  if (PIN_TFT_BL < 0 || freslest_video_output()==FRESLEST_DISPLAY_COMPOSITE) return;

  uint32_t duty = (uint32_t)level255 * ((1 << BL_RES) - 1) / 255;
#if ESP_ARDUINO_VERSION_MAJOR >= 3
  ledcWriteChannel(BL_CH, duty);
#else
  ledcWrite(BL_CH, duty);
#endif
}

// === FPS CONTROL ===
bool frameReady(uint32_t now)
{
  if (now - _lastFrame < FRAME_MS) return false;
  _lastFrame = now;
  return true;
}

// The write-only LCD cannot read pixels. Restore the software base image before
// compositing the pointer; the framebuffer never contains cursor pixels.
bool gfxHealthy(){return frame.getBuffer()&&freslest_video_healthy();}
void gfxMirrorTick(){
 screenMirrorPoll();
 if(frame.getBuffer())screenMirrorFrame((const uint16_t*)frame.getBuffer(),480,frame.height(),usbPointer.visible,usbPointer.x,usbPointer.y);
}
void gfxPresent(){
 gfxMirrorTick();
 auto* pixels=(uint16_t*)frame.getBuffer();if(!pixels)return;
 if(freslest_video_output()==FRESLEST_DISPLAY_COMPOSITE){
  freslest_video_rgb565(pixels,480,frame.height(),true,usbPointer.visible,usbPointer.x,usbPointer.y);return;
 }
 bool dirty[320];
 bool moved=oldPointer!=usbPointer.visible||(usbPointer.visible&&(oldX!=usbPointer.x||oldY!=usbPointer.y));
 for(int y=0;y<320;y++){
  dirty[y]=firstPresent||!presented||memcmp(pixels+y*480,presented+y*480,960)!=0;
  if(dirty[y]&&presented)memcpy(presented+y*480,pixels+y*480,960);
  if(moved)dirty[y]|=(oldPointer&&y>=oldY&&y<oldY+FRESLEST_CURSOR_H)||
    (usbPointer.visible&&y>=usbPointer.y&&y<usbPointer.y+FRESLEST_CURSOR_H);
 }
 // Compose before any SPI transfer. A new pointer is never exposed as a
 // background-only rectangle. Only this UI task owns the base framebuffer.
 uint16_t saved[FRESLEST_CURSOR_W*FRESLEST_CURSOR_H];
 if(usbPointer.visible)for(int y=0;y<FRESLEST_CURSOR_H;y++)for(int x=0;x<FRESLEST_CURSOR_W;x++){
  int px=usbPointer.x+x,py=usbPointer.y+y;
  if(px<0||py<0||px>=480||py>=320)continue;
  saved[y*FRESLEST_CURSOR_W+x]=pixels[py*480+px];
  unsigned color=freslest_cursor_pixel(x,y);
  if(color)pixels[py*480+px]=color==1?0:color==2?0xffff:0x7f05; // raw swap565
 }
 _tft.startWrite();
#if FRESLEST_LCD_MODEL == 9341
 // Transfer one scaled row at a time; no second full physical framebuffer.
 // Compose the cursor first so it follows the same mapping as its hit target.
 uint16_t line[FRESLEST_LCD_WIDTH];
 for(int y=0;y<FRESLEST_LCD_HEIGHT;y++){
  int sy=freslest_lcd_source_y(y);
  if(!dirty[sy])continue;
  for(int x=0;x<FRESLEST_LCD_WIDTH;x++)line[x]=pixels[sy*480+freslest_lcd_source_x(x)];
  _tft.pushImage(0,y,FRESLEST_LCD_WIDTH,1,line);
 }
#else
 int start=-1;
 for(int y=0;y<=320;y++){
  bool changed=y<320&&dirty[y];
  if(changed&&start<0)start=y;
  if(!changed&&start>=0){_tft.setClipRect(0,start,480,y-start);frame.pushSprite(&_tft,0,0);_tft.clearClipRect();start=-1;}
 }
#endif
 _tft.waitDMA();_tft.endWrite();
 // Restore the pristine base only after the last DMA consumer has finished.
 if(usbPointer.visible)for(int y=0;y<FRESLEST_CURSOR_H;y++)for(int x=0;x<FRESLEST_CURSOR_W;x++){
  int px=usbPointer.x+x,py=usbPointer.y+y;
  if(px>=0&&py>=0&&px<480&&py<320)pixels[py*480+px]=saved[y*FRESLEST_CURSOR_W+x];
 }
 firstPresent=false;oldPointer=usbPointer.visible;oldX=usbPointer.x;oldY=usbPointer.y;
}
