#include <stdio.h>
#include <assert.h>
#include "host_idf.h"
int64_t host_now;
unsigned host_delay_calls,host_pwm_updates;
int host_gpio[64],host_ledc_fail;
uint32_t host_duty,host_freq;
int freslest_fullscreen=1;
#include <freslest_video.h>
static bool composite,cursorVisible;static int cursorX,cursorY,compositeCalls,compositeW,compositeH;
freslest_display_t freslest_video_boot_output(void){return composite?FRESLEST_DISPLAY_COMPOSITE:FRESLEST_DISPLAY_LCD;}
freslest_display_t freslest_video_output(void){return freslest_video_boot_output();}
bool freslest_video_begin(void){return true;}
void freslest_video_indexed(const uint8_t* src,int w,int h,const uint32_t* pal,bool c,int x,int y){assert(src&&pal);(void)c;(void)x;(void)y;compositeCalls++;compositeW=w;compositeH=h;}
bool usbDoomPointer(int* x,int* y){*x=cursorX;*y=cursorY;return cursorVisible;}
#include "../components/prboom-esp32-compat/spi_lcd.c"
static unsigned command;
static int x0,y0,x1,y1;
static size_t pixel_bytes,total_pixel_bytes;
static unsigned char captured[480*320*3],display[480*320*3];
static spi_transaction_t *pending[2];
static unsigned pending_count;
static void transfer(spi_transaction_t *t)
{
 const uint8_t *p=t->tx_buffer;size_t n=t->length/8;
 if(!host_gpio[LCD_DC]){assert(n==1);command=p[0];if(command==0x2c)pixel_bytes=0;return;}
 if(command==0x2a){assert(n==4);x0=(p[0]<<8)|p[1];x1=(p[2]<<8)|p[3];}
 else if(command==0x2b){assert(n==4);y0=(p[0]<<8)|p[1];y1=(p[2]<<8)|p[3];}
 else if(command==0x2c){assert(pixel_bytes+n<=sizeof(captured));memcpy(captured+pixel_bytes,p,n);
 for(size_t i=0;i<n;i++){size_t pos=pixel_bytes+i;size_t pixel=pos/3;int xx=x0+pixel%(x1-x0+1),yy=y0+pixel/(x1-x0+1);assert(xx<480&&yy<320);display[(yy*480+xx)*3+pos%3]=p[i];}pixel_bytes+=n;total_pixel_bytes+=n;}
}
esp_err_t spi_device_transmit(spi_device_handle_t h,spi_transaction_t *t){(void)h;assert(!pending_count);transfer(t);return 0;}
esp_err_t spi_device_queue_trans(spi_device_handle_t h,spi_transaction_t *t,TickType_t wait){(void)h;(void)wait;assert(pending_count<2);pending[pending_count++]=t;transfer(t);return 0;}
esp_err_t spi_device_get_trans_result(spi_device_handle_t h,spi_transaction_t **t,TickType_t wait){(void)h;(void)wait;assert(pending_count);*t=pending[0];pending[0]=pending[1];--pending_count;return 0;}
int main(void)
{
 spi_lcd_init();int16_t palette[256]={0};palette[1]=0x00f8;palette[2]=(int16_t)0xe007;spi_lcd_set_palette(palette);
 uint8_t *frame=malloc(320*240);assert(frame);memset(frame,1,320*240);
 spi_lcd_send((uint16_t*)frame);assert(x0==28&&x1==451&&y0==1&&y1==318);
 assert(pixel_bytes==424*318*3&&424*3==318*4&&!pending_count);
 assert(captured[0]==255&&captured[1]==0&&captured[2]==0);
 /* Every scaled row must equal the matching 320x240 source row, including
  * duplicate rows and 16-line DMA block boundaries. */
 for(int y=0;y<240;++y)memset(frame+y*320,y&1?2:1,320);
 spi_lcd_set_palette(palette);
 spi_lcd_send((uint16_t*)frame);
 assert(pixel_bytes==424*318*3);
 for(int y=0;y<318;++y)for(int x=0;x<424;++x){
  size_t o=((size_t)y*424+x)*3;int green=((y*240/318)&1);
  assert(captured[o]==(green?0:255)&&captured[o+1]==(green?255:0)&&captured[o+2]==0);
 }
 memset(frame,1,320*240);spi_lcd_send((uint16_t*)frame);
 total_pixel_bytes=0;spi_lcd_send((uint16_t*)frame);assert(total_pixel_bytes==0);
 memset(frame+100*320,2,320);spi_lcd_send((uint16_t*)frame);
 assert(pixel_bytes==424*3&&y0==134&&y1==134);assert(captured[0]==0&&captured[1]==255&&captured[2]==0);
 total_pixel_bytes=0;spi_lcd_set_palette(palette);spi_lcd_send((uint16_t*)frame);assert(total_pixel_bytes==424*318*3);
 freslest_fullscreen=0;spi_lcd_send((uint16_t*)frame);assert(x0==80&&x1==399&&y0==40&&y1==279&&pixel_bytes==320*240*3);
 uint8_t *update_frame=malloc(480*320);assert(update_frame);memset(update_frame,2,480*320);
 spi_lcd_draw_indexed(update_frame);assert(x0==0&&x1==479&&y0==0&&y1==319&&pixel_bytes==480*320*3&&!previous_valid);
 for(unsigned p=0;p<480*320;p++)assert(captured[p*3]==0&&captured[p*3+1]==255&&captured[p*3+2]==0);
 free(update_frame);
 puts("PASS: exact 4:3 centered scaling, unchanged-frame skip, one-row dirty transfer, palette invalidation, centered mode");
 // Pointer-only movement redraws and restores its old rows without touching base pixels.
 memset(frame,1,320*240);cursorVisible=true;cursorX=30;cursorY=40;
 spi_lcd_send((uint16_t*)frame);assert(previous_frame[40*320+30]==1&&display[((40+40)*480+80+30)*3]==0);
 cursorX=110;cursorY=90;spi_lcd_send((uint16_t*)frame);
 assert(display[(80*480+110)*3]==255&&display[(130*480+190)*3]==0);
 cursorVisible=false;spi_lcd_send((uint16_t*)frame);assert(display[(130*480+190)*3]==255);
 puts("PASS: actual Doom DMA sprite motion/unplug restore base pixels; composite routing bypasses all SPI setup");
 composite=true;unsigned before=total_pixel_bytes;spi_lcd_init();spi_lcd_send((uint16_t*)frame);
 assert(compositeCalls==1&&compositeW==320&&compositeH==240&&before==total_pixel_bytes);
 uint8_t* ui=calloc(480*320,1);spi_lcd_draw_indexed(ui);free(ui);
 assert(compositeCalls==2&&compositeW==480&&compositeH==320&&before==total_pixel_bytes);
 free(frame);free(previous_frame);free(dma_buf[0]);free(dma_buf[1]);return 0;
}
