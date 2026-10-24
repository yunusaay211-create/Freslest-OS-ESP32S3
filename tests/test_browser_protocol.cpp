#include <cassert>
#include <string>
#include "browser_protocol.h"
#include "text_entry.h"
int main(){
 using namespace browser_protocol;
 TextEntryState s;s.begin("secret",10,true);s.moveCursor(-1);assert(s.erase(true));assert(!strcmp(s.text,"secrt"));s.cursor=0;assert(!s.erase(true));assert(s.erase(false));assert(!strcmp(s.text,"ecrt"));
 for(int p=0;p<3;p++){s.page=p;for(int i=0;i<40;i++){s.focus=i;assert(s.focusedChar()>=32&&s.focusedChar()<=126);}}
 for(int i=0;i<1000;i++){s.moveFocus(1,0);assert(s.focus<45);s.moveFocus(0,1);assert(s.focus<45);}
 s.begin("",383,false);for(int i=0;i<383;i++)assert(s.insert('a'));assert(!s.insert('b')&&s.length==383&&s.text[383]==0);
 s.begin("abc",2,false);assert(!strcmp(s.text,"ab")&&s.cursor==2);s.cursor=0;assert(s.erase(false)&&!strcmp(s.text,"b"));assert(s.insert('z')&&!strcmp(s.text,"zb"));
 char out[128];size_t n=0;assert(quoted(out,sizeof(out),n,"a\"\\\n"));assert(!strcmp(out,"\"a\\\"\\\\\\u000a\""));n=0;assert(!quoted(out,3,n,"long"));
 size_t v;assert(unsignedDecimal("4294967295",v)&&v==4294967295ULL);assert(!unsignedDecimal("1x",v)&&!unsignedDecimal("",v)&&!unsignedDecimal("-1",v)&&!unsignedDecimal("9999999999999999999999999",v));
 percentDecode(out,sizeof(out),"Hello%20world%21%00");assert(!strcmp(out,"Hello world!"));
 assert(pageUrl("https://www.google.com/")&&pageUrl("http://192.168.1.3:8000/"));assert(!pageUrl("https://")&&!pageUrl("javascript:1")&&!pageUrl("http:///a")&&!pageUrl("https://a b"));
 puts("PASS: actual text editor and Browser protocol: layout bounds, cursor/deletion/insertion, maximum length, bounded JSON escaping, numeric overflow, percent decoding and HTTP URL validation.");
}
