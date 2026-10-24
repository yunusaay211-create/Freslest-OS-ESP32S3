#ifndef HOST_SPI_MASTER_H
#define HOST_SPI_MASTER_H
#include "host_idf.h"
#define SPI2_HOST 2
#define SPI_DMA_CH_AUTO 0
#define SPI_TRANS_USE_TXDATA 1
#define SPI_TRANS_USE_RXDATA 2
#define portMAX_DELAY UINT32_MAX
#define taskYIELD() ((void)0)
typedef void *spi_device_handle_t;
typedef struct {
 size_t length,rxlength;
 unsigned flags;
 const void *tx_buffer;
 uint8_t tx_data[4],rx_data[4];
} spi_transaction_t;
typedef struct { int mosi_io_num,miso_io_num,sclk_io_num,quadwp_io_num,quadhd_io_num,max_transfer_sz; } spi_bus_config_t;
typedef struct { int clock_speed_hz,mode,spics_io_num,queue_size; } spi_device_interface_config_t;
static inline esp_err_t spi_bus_initialize(int n,const spi_bus_config_t *c,int d){(void)n;(void)c;(void)d;return 0;}
static inline esp_err_t spi_bus_add_device(int n,const spi_device_interface_config_t *c,spi_device_handle_t *h){(void)n;(void)c;*h=(void*)1;return 0;}
esp_err_t spi_device_transmit(spi_device_handle_t h,spi_transaction_t *t);
esp_err_t spi_device_queue_trans(spi_device_handle_t h,spi_transaction_t *t,TickType_t wait);
esp_err_t spi_device_get_trans_result(spi_device_handle_t h,spi_transaction_t **t,TickType_t wait);
esp_err_t spi_device_polling_transmit(spi_device_handle_t h,spi_transaction_t *t);
esp_err_t spi_device_acquire_bus(spi_device_handle_t h,TickType_t wait);
void spi_device_release_bus(spi_device_handle_t h);
#endif
