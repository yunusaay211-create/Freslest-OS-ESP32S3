#pragma once
#include "msc_common.h"
esp_err_t scsi_cmd_read10(msc_device_t*,uint8_t*,uint32_t,uint32_t,uint32_t);
esp_err_t scsi_cmd_write10(msc_device_t*,const uint8_t*,uint32_t,uint32_t,uint32_t);
