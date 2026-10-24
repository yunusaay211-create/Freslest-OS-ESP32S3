#pragma once
#include <stdint.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
enum {LEDC_LOW_SPEED_MODE,LEDC_CHANNEL_4=4,LEDC_TIMER_2=2,LEDC_TIMER_10_BIT=10,LEDC_AUTO_CLK=0,LEDC_USE_XTAL_CLK=1,LEDC_INTR_DISABLE=0};
struct ledc_timer_config_t {int speed_mode,duty_resolution,timer_num;uint32_t freq_hz;int clk_cfg;};
struct ledc_channel_config_t {int gpio_num,speed_mode,channel,intr_type,timer_sel;uint32_t duty;int hpoint;};
esp_err_t ledc_timer_config(const ledc_timer_config_t*);
esp_err_t ledc_channel_config(const ledc_channel_config_t*);
esp_err_t ledc_set_duty(int,int,uint32_t);
esp_err_t ledc_update_duty(int,int);
