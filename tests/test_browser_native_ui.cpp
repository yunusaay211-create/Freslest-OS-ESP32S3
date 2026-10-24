#define FRESLEST_NATIVE_UI_TEST
#include "../OS/freslest/src/freslest/app_browser.cpp"
#include <memory>
void edges(){bUp=bDown=bLeft=bRight=bBack=bOk=bFire={};usbPointer.click=false;}
int main(){
 browserEnter();assert(view==Home);assert(77+(visibleLines()-1)*13+8<=tft.height()-44);bOk.edge=true;browserLoop(0);edges();assert(view==Page&&requests.back()=="https://www.google.com/");
 auto content=std::make_unique<NativePage>();web_lite::Parser p(content->document,"https://example.org/");const char*html="<title>Test page</title><a href='/next'>Next</a><form action='/search'><input name=q value=old><input type=submit value=Search></form>";p.feed(html,strlen(html));p.finish();strcpy(content->message,"Loaded directly.");nextPage=content.get();browserLoop(1);assert(page==content.get());
 bLeft.edge=true;browserLoop(2);edges();assert(selected==2);bRight.edge=true;browserLoop(3);edges();assert(selected==0);bRight.edge=true;browserLoop(4);edges();assert(selected==1);bOk.edge=true;browserLoop(5);edges();assert(editingNow&&field==0);
 editedValue="ESP32 query";editorResult=TextEntryResult::Accepted;browserLoop(6);assert(!strcmp(content->document.fields[0].value,"ESP32 query"));
 bRight.edge=true;browserLoop(7);edges();bOk.edge=true;browserLoop(8);edges();assert(requests.back()=="https://example.org/search?q=ESP32%20query");
 usbPointer={380,54,true};browserLoop(9);edges();assert(view==Help&&beforeHelp==Page);bBack.edge=true;browserLoop(10);edges();assert(view==Page);
 bBack.edge=true;browserLoop(11);edges();assert(view==Home);bBack.edge=true;browserLoop(12);edges();assert(enteredApp==APP_APPS);
 tft.h=360;browserEnter();assert(77+(visibleLines()-1)*13+8<=tft.height()-44);drawHelp();view=Page;page=content.get();drawPage();page=nullptr;browserExit();assert(cancelled&&released);
 puts("PASS: actual standalone Browser UI: home bookmarks, link selection/wrap, form editing and encoded GET submission, loading state, Info return, BACK cleanup, and LCD/4:3 display bounds.");
}
