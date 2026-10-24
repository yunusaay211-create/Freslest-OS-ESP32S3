#pragma once
#include "browser_host_platform.h"
#include <map>
#include <functional>
inline void *heap_caps_calloc(size_t n,size_t s,int){return calloc(n,s);}
enum {HTTP_EVENT_ON_HEADER=1};
struct esp_http_client_event_t {int event_id;void*user_data;char*header_key;char*header_value;};
inline int esp_crt_bundle_attach(void*){return 0;}
struct esp_http_client_config_t {const char*url;int timeout_ms;bool disable_auto_redirect;int(*crt_bundle_attach)(void*);int(*event_handler)(esp_http_client_event_t*);void*user_data;int buffer_size,buffer_size_tx;};
struct HttpFixture {int code=200;std::string type="text/html",location,encoding,body;bool partial=false,chunked=false;};
struct HttpMock {esp_http_client_config_t config;std::string url;HttpFixture fixture;size_t at=0;};
inline std::map<std::string,HttpFixture> fixtures;inline unsigned clients=0,closedClients=0;inline std::function<void()> afterRead;
inline HttpMock* esp_http_client_init(const esp_http_client_config_t*c){assert(!critical&&c->crt_bundle_attach&&c->disable_auto_redirect&&c->timeout_ms==4000);auto*p=new HttpMock{*c,c->url,{}};++clients;return p;}
inline int esp_http_client_set_header(HttpMock*,const char*,const char*){return 0;}
inline int esp_http_client_open(HttpMock*c,int){auto f=fixtures.find(c->url);if(f==fixtures.end())return -1;c->fixture=f->second;return 0;}
inline int64_t esp_http_client_fetch_headers(HttpMock*c){
 for(auto pair:{std::make_pair("Content-Type",c->fixture.type),std::make_pair("Location",c->fixture.location),std::make_pair("Content-Encoding",c->fixture.encoding)})if(!pair.second.empty()){esp_http_client_event_t event{HTTP_EVENT_ON_HEADER,c->config.user_data,(char*)pair.first,(char*)pair.second.c_str()};c->config.event_handler(&event);}
 return c->fixture.chunked?0:c->fixture.body.size();
}
inline int esp_http_client_get_status_code(HttpMock*c){return c->fixture.code;}
inline int esp_http_client_read(HttpMock*c,char*out,size_t n){if(afterRead){auto hook=afterRead;afterRead=nullptr;hook();}if(c->fixture.partial&&c->at>=c->fixture.body.size()/2)return -1;n=std::min(n,std::min((size_t)137,c->fixture.body.size()-c->at));memcpy(out,c->fixture.body.data()+c->at,n);c->at+=n;return n;}
inline bool esp_http_client_is_complete_data_received(HttpMock*c){return c->at==c->fixture.body.size();}
inline int esp_http_client_cleanup(HttpMock*c){assert(!critical);delete c;++closedClients;return 0;}
