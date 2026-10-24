# Radio PSRAM usage: verified in Freslest OS 6.14 and 6.15

6.15 clarifies diagnostics. It does not introduce a new radio allocation policy.
Comparison of the final 6.14 and 6.15 ELF/map files shows the same static memory
placement: `.ext_ram.bss` is 22,880 bytes, at 0x3c1a0000..0x3c1a5960 (end exclusive).
Bluetooth examples btm_cb, l2cb, gatt_cb and lwIP tcp_active_pcbs are inside it.
The generated report is `validation/radio-memory-615.json`.

Static archive contributions, excluding alignment padding:

| Archive | Bytes in PSRAM BSS |
| --- | ---: |
| Bluetooth host libbt.a | 8,893 |
| TCP/IP liblwip.a | 3,774 |
| Wi-Fi libnet80211.a | 7,583 |
| Wi-Fi libpp.a | 1,194 |
| Security libwpa_supplicant.a | 1,326 |

These are linked static data, not the total live memory consumption. Dynamic
packet buffers, heap structures, display buffers and other allocations are
additional. Remaining internal free-memory changes alone do not show whether
PSRAM is being used.

Enabled in both releases:

```
CONFIG_SPIRAM_USE_MALLOC=y
CONFIG_SPIRAM_ALLOW_BSS_SEG_EXTERNAL_MEMORY=y
CONFIG_SPIRAM_TRY_ALLOCATE_WIFI_LWIP=y
CONFIG_BT_ALLOCATION_FROM_SPIRAM_FIRST=y
```

Actual ESP-IDF 5.4.4 paths were checked: esp_wifi/esp32s3/esp_adapter.c uses
heap_caps_malloc/calloc/realloc_prefer with PSRAM first for eligible allocations;
bt/common/osi/include/osi/allocator.h does the same for the Bluetooth host's
osi_malloc/osi_calloc routes. Fallback to internal RAM remains allowed if PSRAM
cannot satisfy an allocation. General malloc still uses the configured 1024-byte
preference threshold; this is not equivalent to forcing every allocation external.

Some radio/controller/DMA buffers explicitly need internal RAM. Standard
xTaskCreate-created stacks and task structures remain internal. Moving those
blindly to PSRAM can break cache-disabled/ROM/driver paths. No such relocation
or SDK allocator replacement is made by this release.

## What 6.15 adds

At boot UART now prints:

```
RADIO BSS: external=22880 bytes; BT=8912 bytes at 0x3c1a0000 (PSRAM)
RAM: INTERNAL free=<live bytes>; PSRAM free=<live bytes>
```

BT=8912 includes linker padding; the archive table excludes it. BLE startup
prints INTERNAL free/largest plus PSRAM free. Send `i` in the 115200 Serial
Monitor for Wi-Fi's INTERNAL and PSRAM free/largest counters and radio status.
The screen viewer must be disconnected before using Serial Monitor.

The exact free values depend on active apps/radios. The printed BSS address is
checked at runtime using esp_ptr_external_ram; the ELF/map audit independently
verifies link placement. A PC build cannot establish live allocation totals or
radio electrical/reconnect behavior on the user's board.

To audit a rebuilt image from an ESP-IDF prompt:

```
python tools/verify_radio_memory.py --elf OS/build/freslest_os_s3.elf
```

ESP-IDF's Python environment includes pyelftools. The report also checks AV ISR
IRAM placement. Doom, AV pins, partitions, storage assets and screen protocol are
retained. The viewer accepts 6.14 or later; no new PC application is required.

Primary SDK documentation:
https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32s3/api-guides/external-ram.html
https://docs.espressif.com/projects/esp-idf/en/v5.4.4/esp32s3/api-reference/kconfig.html
