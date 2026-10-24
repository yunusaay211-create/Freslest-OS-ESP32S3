#pragma once
#include <stdint.h>
#include <vector>
#include <array>
#include <deque>
#define SERIAL_8N1 0
extern std::vector<std::array<uint8_t,10>> testPackets;
inline std::deque<uint8_t> testSerialRx;
class HardwareSerial {public:explicit HardwareSerial(int){} void begin(int,int,int rx,int tx){if(rx!=17||tx!=18)__builtin_abort();} unsigned baudRate(){return 9600;} int available(){return testSerialRx.size();}int read(){if(testSerialRx.empty())return -1;int n=testSerialRx.front();testSerialRx.pop_front();return n;}size_t write(const uint8_t* p,unsigned n){if(n!=10)__builtin_abort();std::array<uint8_t,10>a;for(unsigned i=0;i<n;i++)a[i]=p[i];testPackets.push_back(a);return n;}};
