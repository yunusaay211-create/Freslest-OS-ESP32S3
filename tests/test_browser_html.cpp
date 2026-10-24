#include "browser_html.h"
#include <cassert>
#include <cstring>
#include <cstdio>
#include <memory>
#include <random>
#include <string>
using namespace web_lite;
std::string all(const Document&d){std::string s;for(unsigned i=0;i<d.lineCount;i++){s+=d.lines[i].text;s+='\n';}return s;}
int main(){
 auto doc=std::make_unique<Document>();char out[URL];
 for(auto s:{"https://example.org/","http://127.0.0.1:8080/a?b=1"})assert(validUrl(s));
 for(auto s:{"https://","file:///etc/passwd","javascript:alert(1)","https://u:p@example.org/","https://a b/","https://a\\b"})assert(!validUrl(s));
 assert(resolveUrl("https://example.org/a/b?old=1","../image.jpg?x=1&amp;y=2",out)&&!strcmp(out,"https://example.org/image.jpg?x=1&amp;y=2"));
 assert(resolveUrl("https://example.org/a/b","/x/./y/../z",out)&&!strcmp(out,"https://example.org/x/z"));
 assert(resolveUrl("https://example.org/a/b","//other.org/c",out)&&!strcmp(out,"https://other.org/c"));
 assert(resolveUrl("https://example.org/a/b?x=1","?q=2",out)&&!strcmp(out,"https://example.org/a/b?q=2"));
 assert(!resolveUrl("https://example.org/","javascript:alert(1)",out));
 const char*html="<!doctype html><head><title>Fixture &amp; title</title><style>.bad{}</style></head><body><!-- secret comment --><p>Direct &lt;HTML&gt; &#304;stanbul</p><script>document.body.innerText='WRONG'; if(a < b){}</script><a href='../target?a=1&amp;b=2'>Open link</a><img alt='Photo' src='/photo.jpg'><form action='/search' method='get'><input name=q value='old'><input type=hidden name=lang value=en><input type=submit value=Search></form><form action='/login' method=post><input type=password name=p value=secret><button>Sign in</button></form><video src='/video.mp4'></video></body>";
 std::string expected;
 for(unsigned chunk:{1u,2u,3u,7u,1024u}){
  Parser p(*doc,"https://example.org/folder/page");for(size_t i=0;i<strlen(html);i+=chunk)p.feed(html+i,std::min((size_t)chunk,strlen(html)-i));p.finish();auto text=all(*doc);if(expected.empty())expected=text;assert(text==expected);
  assert(!strcmp(doc->title,"Fixture & title")&&doc->scripts&&doc->video);assert(text.find("Direct <HTML> Istanbul")!=std::string::npos&&text.find("WRONG")==std::string::npos&&text.find("secret")==std::string::npos);
  assert(doc->links[0].kind==Anchor&&!strcmp(doc->links[0].url,"https://example.org/target?a=1&b=2"));
  assert(doc->links[1].kind==Image&&!strcmp(doc->links[1].url,"https://example.org/photo.jpg"));
  assert(doc->formCount==2&&doc->fieldCount==3);strcpy(doc->fields[0].value,"hello & world");assert(formUrl(*doc,0,out)&&!strcmp(out,"https://example.org/search?q=hello%20%26%20world&lang=en"));assert(!formUrl(*doc,1,out));
 }
 Parser plain(*doc,"http://local/text");plain.plain("one\ntwo",7);plain.finish();assert(!strcmp(doc->lines[0].text,"one")&&!strcmp(doc->lines[1].text,"two"));
 std::string huge(100000,'X');Parser limit(*doc,"https://example.org/");limit.feed(huge.data(),huge.size());limit.finish();assert(doc->lineCount==LINES&&doc->truncated);
 // Bound the parser through arbitrary chunk boundaries, long tags/entities,
 // nested fake markup and invalid UTF-8. Sanitizers check every array.
 std::mt19937 rng(612);for(int round=0;round<100;round++){std::string input(20000,0);for(char&c:input)c=(char)(rng()%256);Parser fuzz(*doc,"http://example.org/");for(size_t i=0;i<input.size();i+=31)fuzz.feed(input.data()+i,std::min((size_t)31,input.size()-i));fuzz.finish();assert(doc->lineCount>=1&&doc->lineCount<=LINES&&doc->linkCount<=LINKS&&doc->fieldCount<=FIELDS&&doc->formCount<=FORMS);}
 puts("PASS: production standalone HTML parser: chunk-independent title/text/entities/UTF-8, scripts/styles/comments excluded, safe relative URLs, links/JPEG, GET form encoding and hidden fields, POST/video rejection, large-page limits and 2 MB malformed-input sanitizer coverage.");
}
