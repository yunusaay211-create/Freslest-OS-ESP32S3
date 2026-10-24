#define FRESLEST_BROWSER_HOST_TEST
#include "../OS/freslest/src/freslest/browser_remote.cpp"
bool browserServerConfigValid(const BrowserServerConfig& c){return c.host[0]&&strlen(c.key)>=16&&c.port;}
int main(){
 BrowserServerConfig c{};strcpy(c.host,"192.168.1.10");strcpy(c.key,"0123456789abcdef0123456789abcdef");c.port=8765;
 assert(browserRemoteStart(c));uint32_t gen=epoch;
 auto response=[](const std::string& headers,const std::string& body=""){NetworkClient client;client.incoming="HTTP/1.1 200 OK\r\n"+headers+"\r\n"+body;return client;};
 std::string jpeg(8000,'x');jpeg[0]=(char)0xff;jpeg[1]=(char)0xd8;
 auto client=response("Content-Length: 8000\r\nContent-Type: image/jpeg\r\nX-Frame-Id: 7\r\nX-Title: Test%20page\r\nX-Page-Url: https%3A%2F%2Fexample.com%2F\r\n",jpeg);
 int i=freeSlot();assert(i>=0);Response r;assert(request(client,c,gen,"GET","/api/frame?after=0",nullptr,r,i));assert(r.id==7&&r.length==8000&&!strcmp(r.title,"Test page")&&!strcmp(r.url,"https://example.com/")&&!memcmp(slots[i].data,jpeg.data(),8000));
 assert(client.written.find("Authorization: Bearer "+std::string(c.key))!=std::string::npos);
 slots[i].id=r.id;slots[i].length=r.length;slots[i].leased=false;pending=i;
 BrowserRemoteFrame f{};assert(browserRemoteTake(f)&&f.slot==i);int j=freeSlot();assert(j!=i&&j>=0);int k=freeSlot();assert(k>=0&&k!=j&&k!=i);assert(freeSlot()==-1);browserRemoteRelease(j);assert(freeSlot()==j);browserRemoteRelease(j);browserRemoteRelease(k);browserRemoteRelease(i);
 for(const std::string& h:{"Content-Length: 98305\r\nContent-Type: image/jpeg\r\nX-Frame-Id: 1\r\n","Content-Length: 5\r\nContent-Length: 6\r\n","Transfer-Encoding: chunked\r\n","Content-Length: -1\r\n","Content-Length: 999999999999999999999999\r\n","Content-Type: image/jpeg\r\n","Content-Length: 5\r\nContent-Type: text/html\r\nX-Frame-Id: 1\r\n"}){auto bad=response(h,"12345");Response b;assert(!request(bad,c,gen,"GET","/api/frame",nullptr,b,0));}
 auto shortBody=response("Content-Length: 8\r\nContent-Type: image/jpeg\r\nX-Frame-Id: 1\r\n","1234");Response b;assert(!request(shortBody,c,gen,"GET","/api/frame",nullptr,b,0));
 NetworkClient empty;empty.incoming="HTTP/1.1 204 No Content\r\nDate: now\r\n\r\n";Response no;assert(request(empty,c,gen,"GET","/api/frame",nullptr,no,0)&&no.code==204);
 assert(browserRemoteNavigate("https://www.google.com/")&&!browserRemoteNavigate("file:///etc/passwd"));
 browserRemoteMove(-100,900);assert(pointerX==0&&pointerY==223);for(int n=0;n<32;n++)assert(browserRemoteText("a"));assert(!browserRemoteText("overflow"));assert(browserRemoteButton(false));Command cmd;assert(xQueueReceive(commands,&cmd,0)&&cmd.kind==Button&&cmd.text[0]=='0');
 browserRemoteVisible(true);assert(refresh);browserRemoteStop();assert(!current(gen)&&!status.active);auto stale=response("Content-Length: 2\r\n","{}");Response sr;assert(!request(stale,c,gen,"POST","/api/session","{}",sr)&&stale.written.empty());
 assert(!critical);puts("PASS: actual Browser TCP source: fragmented JPEG and header reads, length/type/overflow/truncation rejection, 204 support, authentication header, three frame leases, queue saturation/release priority, bounded pointer, pause refresh and stale-request cancellation.");
}
