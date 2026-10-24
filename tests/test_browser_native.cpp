#define FRESLEST_NATIVE_HTTP_TEST
#include "../OS/freslest/src/freslest/browser_native.cpp"
#include <memory>
#include <cassert>
int main(){
 auto page=std::make_unique<NativePage>();
 fixtures["https://example.org/"]={200,"text/html","","","<title>Direct page</title><p>Standalone ESP32</p><a href='/next'>Next</a>"};
 assert(nativeBrowserOpen("https://example.org/"));auto gen=generation;fetch(*page,"https://example.org/",gen);assert(!strcmp(page->document.title,"Direct page")&&page->document.linkCount==1&&page->httpStatus==200&&!page->image);
 fixtures["https://example.org/redirect"]={302,"text/html","/","",""};fetch(*page,"https://example.org/redirect",gen);assert(!strcmp(page->document.url,"https://example.org/"));
 fixtures["https://example.org/loop"]={302,"text/html","/loop","",""};fetch(*page,"https://example.org/loop",gen);assert(strstr(page->message,"Redirect"));
 fixtures["http://example.org/chunks"]={200,"text/plain","","",std::string(3000,'a'),false,true};fetch(*page,"http://example.org/chunks",gen);assert(page->document.lineCount>1&&!strstr(page->message,"failed"));
 fixtures["http://example.org/gzip"]={200,"text/html","","gzip","data"};fetch(*page,"http://example.org/gzip",gen);assert(strstr(page->message,"compressed"));
 fixtures["https://example.org/video"]={200,"video/mp4","","","video data"};fetch(*page,"https://example.org/video",gen);assert(strstr(page->message,"Unsupported"));
 fixtures["http://example.org/large"]={200,"text/html","","",std::string(262145,'a')};fetch(*page,"http://example.org/large",gen);assert(page->document.truncated);
 fixtures["http://example.org/image"]={200,"image/jpeg","","",std::string(9000,'x')};fetch(*page,"http://example.org/image",gen);assert(page->image&&page->jpegLength==9000);
 fixtures["http://example.org/hugeimage"]={200,"image/jpeg","","",std::string(98305,'x')};fetch(*page,"http://example.org/hugeimage",gen);assert(page->image&&!page->jpegLength&&strstr(page->message,"96 KiB"));
 fixtures["http://example.org/broken"]={200,"text/plain","","",std::string(5000,'a'),true};fetch(*page,"http://example.org/broken",gen);assert(strstr(page->message,"interrupted"));
 fixtures["http://example.org/404"]={404,"text/html","","","Not found"};fetch(*page,"http://example.org/404",gen);assert(strstr(page->message,"HTTP 404"));
 afterRead=[](){nativeBrowserCancel();};fetch(*page,"https://example.org/",gen);assert(!current(gen)&&clients==closedClients);
 assert(nativeBrowserOpen("https://example.org/"));int a=reserve(),b=reserve();assert(a>=0&&b>=0&&a!=b&&reserve()==-1);buffers[a].busy=false;pending=a;int taken=-1;auto*p=nativeBrowserTake(taken);assert(p==buffers[a].page&&taken==a);nativeBrowserRelease(a);nativeBrowserRelease(b);assert(reserve()>=0);
 assert(!critical);puts("PASS: actual standalone HTTP worker: verified-TLS configuration, HTML/JPEG/chunked responses, relative redirects/loop cap, media/compression rejection, HTML/JPEG byte limits, partial reads, HTTP status, cancellation cleanup and page-buffer leases.");
}
