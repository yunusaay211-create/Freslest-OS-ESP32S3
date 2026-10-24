#ifndef HOST_IDF_H
#define HOST_IDF_H
/* Host-only API doubles. These do not simulate the electrical ESP32 hardware. */
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <assert.h>
typedef int esp_err_t;
#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERROR_CHECK(x) assert((x) == ESP_OK)
typedef int portMUX_TYPE;
#define portMUX_INITIALIZER_UNLOCKED 0
#define portENTER_CRITICAL(x) ((void)(x))
#define portEXIT_CRITICAL(x) ((void)(x))
#define pdMS_TO_TICKS(x) (x)
#define pdPASS 1
typedef uint32_t TickType_t;
typedef void *TaskHandle_t;
typedef void *SemaphoreHandle_t;
extern int64_t host_now;
extern unsigned host_delay_calls;
static inline void vTaskDelay(TickType_t ms) { host_now += (int64_t)ms*1000; ++host_delay_calls; }
static inline void vTaskDelete(TaskHandle_t h) { (void)h; }
static inline int xTaskCreatePinnedToCore(void (*fn)(void *), const char *name,
 unsigned stack, void *arg, unsigned prio, TaskHandle_t *out, int core) {
 (void)fn; (void)name; (void)stack; (void)arg; (void)prio; (void)core;
 if(out) *out=(void*)1; return pdPASS;
}
static inline SemaphoreHandle_t xSemaphoreCreateBinary(void) { return (void*)1; }
static inline int xSemaphoreTake(SemaphoreHandle_t s, TickType_t t) { (void)s; (void)t; return 1; }
static inline int xSemaphoreGive(SemaphoreHandle_t s) { (void)s; return 1; }
static inline void vSemaphoreDelete(SemaphoreHandle_t s) { (void)s; }
typedef int gpio_num_t;
#define GPIO_MODE_INPUT 1
#define GPIO_MODE_OUTPUT 2
#define GPIO_PULLUP_ENABLE 1
#define GPIO_PULLUP_DISABLE 0
#define GPIO_PULLDOWN_ENABLE 1
#define GPIO_PULLDOWN_DISABLE 0
#define GPIO_INTR_DISABLE 0
typedef struct { uint64_t pin_bit_mask; int mode,pull_up_en,pull_down_en,intr_type; } gpio_config_t;
extern int host_gpio[64];
static inline esp_err_t gpio_config(const gpio_config_t *p) { (void)p; return ESP_OK; }
static inline int gpio_get_level(gpio_num_t p) { return host_gpio[p]; }
static inline esp_err_t gpio_set_level(gpio_num_t p,int v) { host_gpio[p]=v; return ESP_OK; }
#define UART_NUM_1 1
#define UART_DATA_8_BITS 8
#define UART_PARITY_DISABLE 0
#define UART_STOP_BITS_1 1
#define UART_HW_FLOWCTRL_DISABLE 0
#define UART_SCLK_DEFAULT 0
#define UART_PIN_NO_CHANGE -1
typedef struct { int baud_rate,data_bits,parity,stop_bits,flow_ctrl,source_clk; } uart_config_t;
static inline esp_err_t uart_param_config(int n,const uart_config_t *c) { (void)n;(void)c;return 0; }
static inline esp_err_t uart_set_pin(int n,int a,int b,int c,int d) { (void)n;(void)a;(void)b;(void)c;(void)d;return 0; }
static inline esp_err_t uart_driver_install(int n,int a,int b,int c,void *q,int f) { (void)n;(void)a;(void)b;(void)c;(void)q;(void)f;return 0; }
static inline esp_err_t uart_driver_delete(int n) { (void)n;return 0; }
static inline esp_err_t uart_wait_tx_done(int n,TickType_t t) { (void)n;(void)t;return 0; }
int uart_write_bytes(int n,const void *p,size_t len);
int uart_read_bytes(int n,void *p,uint32_t len,TickType_t timeout);
esp_err_t uart_flush_input(int n);
#define LEDC_LOW_SPEED_MODE 0
#define LEDC_CHANNEL_0 0
#define LEDC_TIMER_0 0
#define LEDC_TIMER_10_BIT 10
#define LEDC_INTR_DISABLE 0
#define LEDC_AUTO_CLK 0
typedef struct { int speed_mode,duty_resolution,timer_num; uint32_t freq_hz; int clk_cfg; } ledc_timer_config_t;
typedef struct { int gpio_num,speed_mode,channel,intr_type,timer_sel; uint32_t duty,hpoint; } ledc_channel_config_t;
extern uint32_t host_duty,host_freq;
extern unsigned host_pwm_updates;
extern int host_ledc_fail;
static inline esp_err_t ledc_timer_config(const ledc_timer_config_t *t) { host_freq=t->freq_hz;return 0; }
static inline esp_err_t ledc_channel_config(const ledc_channel_config_t *c) { host_duty=c->duty;return 0; }
static inline esp_err_t ledc_set_duty(int m,int c,uint32_t d) { (void)m;(void)c; if(host_ledc_fail)return -1;assert(d<=1023);host_duty=d;return 0; }
static inline esp_err_t ledc_update_duty(int m,int c) { (void)m;(void)c;++host_pwm_updates;return 0; }
static inline uint32_t ledc_get_duty(int m,int c) { (void)m;(void)c;return host_duty; }
static inline esp_err_t ledc_set_freq(int m,int t,uint32_t f) { (void)m;(void)t;host_freq=f;return 0; }
static inline uint32_t ledc_get_freq(int m,int t) { (void)m;(void)t;return host_freq; }
#define ESP_TIMER_TASK 0
typedef struct { void (*callback)(void*); void *arg; int dispatch_method; const char *name; int skip_unhandled_events; } esp_timer_create_args_t;
typedef struct { esp_timer_create_args_t args; int running; uint64_t period; } host_timer_t;
typedef host_timer_t *esp_timer_handle_t;
static inline int64_t esp_timer_get_time(void) { return host_now; }
static inline esp_err_t esp_timer_create(const esp_timer_create_args_t *a,esp_timer_handle_t *h) { *h=calloc(1,sizeof(**h));(*h)->args=*a;return 0; }
static inline esp_err_t esp_timer_start_periodic(esp_timer_handle_t h,uint64_t t) { assert(!h->running);h->running=1;h->period=t;return 0; }
static inline esp_err_t esp_timer_stop(esp_timer_handle_t h) { h->running=0;return 0; }
static inline esp_err_t esp_timer_delete(esp_timer_handle_t h) { free(h);return 0; }
#endif
