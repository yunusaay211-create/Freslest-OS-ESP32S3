#include "host_idf.h"
#define MALLOC_CAP_DMA 1
#define MALLOC_CAP_INTERNAL 2
#define MALLOC_CAP_SPIRAM 4
#define MALLOC_CAP_8BIT 8
static inline void *heap_caps_malloc(size_t n,unsigned caps){(void)caps;return malloc(n);}
