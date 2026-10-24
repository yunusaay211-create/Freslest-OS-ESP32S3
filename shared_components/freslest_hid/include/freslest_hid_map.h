#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#ifdef __cplusplus
extern "C" {
#endif
#define FRESLEST_HID_MAP_REPORTS 8
#define FRESLEST_HID_MAP_FIELDS 64
typedef struct {uint16_t offset,count,usage;uint8_t size,mode,report,sign;} freslest_hid_field_t;
typedef struct {uint16_t bits;uint8_t id,kind,keys[32],buttons;} freslest_hid_report_t;
typedef struct {
 uint8_t reports,fields,kind;
 freslest_hid_report_t report[FRESLEST_HID_MAP_REPORTS];
 freslest_hid_field_t field[FRESLEST_HID_MAP_FIELDS];
} freslest_hid_map_t;
typedef struct {uint8_t keys[32],buttons,kind;int dx,dy;} freslest_hid_decoded_t;
bool freslest_hid_map_parse(freslest_hid_map_t *,const uint8_t *,size_t);
// GATT supplies report_id separately; payload does not include an ID byte.
bool freslest_hid_map_decode(freslest_hid_map_t *,unsigned report_id,const uint8_t *,size_t,freslest_hid_decoded_t *);
#ifdef __cplusplus
}
#endif
