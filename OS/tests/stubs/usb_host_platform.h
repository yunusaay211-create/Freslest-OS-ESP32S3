#pragma once
#include <cstdint>
#include <cstdlib>
#include <cstdio>
#include <cctype>
#include <string>
#include <vector>
#include <memory>
#include <mutex>
#include <filesystem>
#include <algorithm>

inline std::filesystem::path testRoot;
inline bool testHeapFail=false,testWriteFail=false,testRenameFail=false,testMscFail=false,testUsbConnected=false;
inline unsigned testAllocations=0,testMilliseconds=0;
inline int testFreeBytes=-1;
inline size_t testHeapMax=SIZE_MAX;inline bool testStorage=true;
inline unsigned testFullRedraws=0,testFsQueries=0;
inline uint32_t millis(){return testMilliseconds;}
inline void delay(unsigned n){testMilliseconds+=n;}
struct MockSerial {template<class... A>void printf(const char*,A...){};};inline MockSerial Serial;
class String {
 std::string value;
 public:String(const std::string& s):value(s){}String(const char* s):value(s){}
 void toLowerCase(){for(char& c:value)c=(char)std::tolower((unsigned char)c);}
 bool endsWith(const char* tail)const {std::string t(tail);return value.size()>=t.size()&&value.compare(value.size()-t.size(),t.size(),t)==0;}
};
struct FileState {
 std::filesystem::path path;FILE* file=nullptr;bool directory=false;size_t index=0;
 std::vector<std::filesystem::path> entries;
 ~FileState(){if(file)fclose(file);}
};
class File {
 std::shared_ptr<FileState> state;
 public:
 File()=default;
 File(const std::filesystem::path& path,const char* mode="r") {
  bool directory=std::filesystem::is_directory(path);
  if(!directory&&!std::filesystem::exists(path)&&mode[0]!='w')return;
  state=std::make_shared<FileState>();state->path=path;state->directory=directory;
  if(directory){for(auto& e:std::filesystem::directory_iterator(path))state->entries.push_back(e.path());}
  else {state->file=fopen(path.c_str(),mode);if(!state->file)state.reset();}
 }
 explicit operator bool()const{return state!=nullptr;}
 bool isDirectory()const{return state&&state->directory;}
 String name()const{return String(state?state->path.filename().string():"");}
 size_t size()const{return state&&!state->directory?std::filesystem::file_size(state->path):0;}
 File openNextFile(){if(!state||state->index>=state->entries.size())return File();return File(state->entries[state->index++]);}
 size_t read(uint8_t* p,size_t n){return state&&state->file?fread(p,1,n,state->file):0;}
 size_t write(const uint8_t* p,size_t n){return state&&state->file?fwrite(p,1,testWriteFail?n/2:n,state->file):0;}
 void flush(){if(state&&state->file)fflush(state->file);}
 void close(){state.reset();}
};
struct MockFS {
 std::filesystem::path path(const char* p)const {return testRoot/(p[0]=='/'?p+1:p);}
 File open(const char* p,const char* mode="r"){return File(path(p),mode);}
 bool exists(const char* p){return std::filesystem::exists(path(p));}
 bool mkdir(const char* p){return std::filesystem::create_directories(path(p));}
 bool remove(const char* p){return std::filesystem::remove(path(p));}
 bool rename(const char* from,const char* to){if(testRenameFail)return false;std::error_code e;std::filesystem::rename(path(from),path(to),e);return !e;}
 size_t totalBytes()const{++testFsQueries;return testStorage?320*1024:0;}
 size_t usedBytes()const {++testFsQueries;if(testFreeBytes>=0)return totalBytes()-(size_t)testFreeBytes;size_t n=0;for(auto& e:std::filesystem::recursive_directory_iterator(testRoot))if(e.is_regular_file())n+=e.file_size();return n;}
};inline MockFS LittleFS;
constexpr int MALLOC_CAP_SPIRAM=1,MALLOC_CAP_8BIT=2;
inline void* heap_caps_malloc(size_t n,int){if(testHeapFail||n>testHeapMax)return nullptr;void* p=malloc(n);if(p)++testAllocations;return p;}
inline void heap_caps_free(void* p){if(p){--testAllocations;free(p);}}
using SemaphoreHandle_t=std::mutex*;
constexpr unsigned portMAX_DELAY=0;
inline SemaphoreHandle_t xSemaphoreCreateMutex(){return new std::mutex;}
inline void xSemaphoreTake(SemaphoreHandle_t m,unsigned){m->lock();}
inline void xSemaphoreGive(SemaphoreHandle_t m){m->unlock();}
class USBMSC {
 public:
 bool media=false,writable=false;
 void vendorID(const char*){}void productID(const char*){}void productRevision(const char*){}
 void mediaPresent(bool n){media=n;}void isWritable(bool n){writable=n;}
 template<class T>void onRead(T){}template<class T>void onWrite(T){}template<class T>void onStartStop(T){}
 bool begin(uint32_t,uint16_t){return !testMscFail;}
};
struct MockUSB {
 void productName(const char*){}void manufacturerName(const char*){}
 void usbClass(uint8_t){}void usbSubClass(uint8_t){}void usbProtocol(uint8_t){}void firmwareVersion(uint16_t){}
 bool begin(){testUsbConnected=true;return true;}
};
inline MockUSB USB;
inline bool tud_connect(){testUsbConnected=true;return true;}
inline bool tud_disconnect(){testUsbConnected=false;return true;}
inline bool tud_mounted(){return testUsbConnected;}
struct MockTheme {uint16_t bg=0,panel=1,text=2,dim=3;};
inline const MockTheme& TH(){static MockTheme theme;return theme;}
struct MockDisplay {
 void fillScreen(uint16_t){++testFullRedraws;}void fillRect(int,int,int,int,uint16_t){}void setTextColor(uint16_t,uint16_t){}void setTextSize(int){}void setCursor(int,int){}void print(const char*){}
 template<class... A>void printf(const char*,A...){}
};inline MockDisplay tft;
struct MockButton {bool edge=false;bool pressed()const{return edge;}};inline MockButton bOk,bBack,bFire;
inline void drawHeader(const char*){}inline void clearBody(){}inline void drawFooter(const char*,const char*){}
inline void nesPlayerExit(){}inline void audioStop(){}inline void audioUpdate(uint32_t){}
inline bool nesCoreMapperSupported(unsigned mapper){return mapper==0;}
constexpr int APP_NES_LIBRARY=1;
inline int testEnteredApp=0;
inline void enterApp(int app){testEnteredApp=app;}

inline bool freslestStorageEnsure(){return LittleFS.totalBytes()!=0;}
inline const char* freslestStorageStatus(){return "E102: Storage unavailable";}
