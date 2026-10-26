#include <assert.h>
#include <stdio.h>
#include "host_idf.h"
#include "lcd_profile.h"
#include <freslest_video.h>
int64_t host_now;
unsigned host_delay_calls,host_pwm_updates;
int host_gpio[64],host_ledc_fail;
uint32_t host_duty,host_freq;
int freslest_fullscreen=1;
static bool cursorVisible,vga;
static int cursorX,cursorY,vgaCalls;
freslest_display_t freslest_video_boot_output(void){return vga?FRESLEST_DISPLAY_VGA:FRESLEST_DISPLAY_LCD;}
freslest_display_t freslest_video_output(void){return freslest_video_boot_output();}
bool freslest_video_begin(void){return true;}
void freslest_video_indexed(const uint8_t* s,int w,int h,const uint32_t* p,bool c,int x,int y){assert(s&&p&&w==320&&h==240);(void)c;(void)x;(void)y;++vgaCalls;}
bool usbDoomPointer(int* x,int* y){*x=cursorX;*y=cursorY;return cursorVisible;}
#include "../components/prboom-esp32-compat/spi_lcd.c"
static unsigned command,pixelFormat,addressMode;
static int x0,y0,x1,y1;
static size_t bytes,total;
static uint8_t captured[480*320*3];
static spi_transaction_t* pending[2];
static unsigned queued;
static void transfer(spi_transaction_t* t){
 const uint8_t* p=t->tx_buffer;size_t n=t->length/8;
 if(!host_gpio[LCD_DC]){assert(n==1);command=p[0];if(command==0x2c)bytes=0;return;}
 if(command==0x3a){assert(n==1);pixelFormat=*p;}
 if(command==0x36){assert(n==1);addressMode=*p;}
 if(command==0x2a){x0=(p[0]<<8)|p[1];x1=(p[2]<<8)|p[3];assert(x0>=0&&x1<FRESLEST_LCD_WIDTH);}
 if(command==0x2b){y0=(p[0]<<8)|p[1];y1=(p[2]<<8)|p[3];assert(y0>=0&&y1<FRESLEST_LCD_HEIGHT);}
 if(command==0x2c){assert(bytes+n<=sizeof(captured));memcpy(captured+bytes,p,n);bytes+=n;total+=n;
  assert(bytes<=(size_t)(x1-x0+1)*(y1-y0+1)*LCD_BYTES_PER_PIXEL);}
}
esp_err_t spi_device_transmit(spi_device_handle_t h,spi_transaction_t* t){(void)h;assert(!queued);transfer(t);return 0;}
esp_err_t spi_device_queue_trans(spi_device_handle_t h,spi_transaction_t* t,TickType_t w){(void)h;(void)w;assert(queued<2);pending[queued++]=t;transfer(t);return 0;}
esp_err_t spi_device_get_trans_result(spi_device_handle_t h,spi_transaction_t** t,TickType_t w){(void)h;(void)w;assert(queued);*t=pending[0];pending[0]=pending[1];--queued;return 0;}
int main(void){
 spi_lcd_init();assert(pixelFormat==(FRESLEST_LCD_MODEL==9341?0x55:0x66));
 assert(addressMode==(FRESLEST_LCD_MODEL==9341?0xb8:0xe8));
 int16_t palette[256]={0};palette[1]=0x00f8;palette[2]=(int16_t)0xe007;spi_lcd_set_palette(palette);
 uint8_t frame[320*240];memset(frame,1,sizeof(frame));
 spi_lcd_send((uint16_t*)frame);
 assert(x0==(LCD_PANEL_W-LCD_FIT_W)/2&&y0==(LCD_PANEL_H-LCD_FIT_H)/2);
 assert(bytes==LCD_FIT_W*LCD_FIT_H*LCD_BYTES_PER_PIXEL&&!queued);
 if(FRESLEST_LCD_MODEL==9341)assert(captured[0]==0xf8&&captured[1]==0);
 else assert(captured[0]==255&&captured[1]==0&&captured[2]==0);
 size_t before=total;spi_lcd_send((uint16_t*)frame);assert(total==before);
 memset(frame+100*320,2,320);spi_lcd_send((uint16_t*)frame);
 assert(bytes==LCD_FIT_W*LCD_BYTES_PER_PIXEL);
 if(FRESLEST_LCD_MODEL==9341)assert(captured[0]==7&&captured[1]==0xe0&&y0==100&&y1==100);
 freslest_fullscreen=0;spi_lcd_send((uint16_t*)frame);
 assert(bytes==320*240*LCD_BYTES_PER_PIXEL&&x0==(LCD_PANEL_W-320)/2&&y0==(LCD_PANEL_H-240)/2);
 cursorVisible=true;cursorX=30;cursorY=40;spi_lcd_send((uint16_t*)frame);
 assert(previous_frame[40*320+30]==1);cursorVisible=false;spi_lcd_send((uint16_t*)frame);
 uint8_t ui[480*320];for(int y=0;y<320;y++)for(int x=0;x<480;x++)ui[y*480+x]=(x<240)?1:2;
 spi_lcd_draw_indexed(ui);assert(bytes==LCD_PANEL_W*LCD_PANEL_H*LCD_BYTES_PER_PIXEL&&!previous_valid);
 if(FRESLEST_LCD_MODEL==9341){assert(captured[0]==0xf8&&captured[1]==0);assert(captured[320*240*2-2]==7&&captured[320*240*2-1]==0xe0);}
 vga=true;before=total;spi_lcd_init();spi_lcd_send((uint16_t*)frame);assert(vgaCalls==1&&before==total);
 free(previous_frame);free(dma_buf[0]);free(dma_buf[1]);
 puts("PASS: LCD init, wire pixel format, bounded windows, full/centered, dirty rows, cursor, update UI and vga bypass");
}
