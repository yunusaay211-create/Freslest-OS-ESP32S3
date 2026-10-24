#pragma once
#include <thread>
inline void vTaskDelay(unsigned) {std::this_thread::yield();}
