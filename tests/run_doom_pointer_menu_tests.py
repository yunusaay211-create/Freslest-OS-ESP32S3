from pathlib import Path
import os,subprocess,tempfile
root=Path(__file__).resolve().parents[1]
s=(root/'Doom/components/prboom/m_menu.c').read_text();start=s.index('void Freslest_MenuPointer(');end=s.index('\n}\n',start)+3
pre='''#include <assert.h>\n#include <stdio.h>\n#define SCREENHEIGHT 240\n#define LINEHEIGHT 16\nstruct Item{int status;};struct Menu{int x,y,numitems;struct Item *menuitems;};\nstatic int menuactive,messageToPrint,setup_active,itemOn;static struct Menu *currentMenu;\n'''
test='''int main(void){struct Item items[]={{1},{-1},{2},{1}};struct Menu menu={80,50,4,items};currentMenu=&menu;itemOn=3;Freslest_MenuPointer(90,60);assert(itemOn==3);menuactive=1;Freslest_MenuPointer(90,60);assert(itemOn==0);Freslest_MenuPointer(90,80);assert(itemOn==0);Freslest_MenuPointer(90,99);assert(itemOn==2);Freslest_MenuPointer(90,120);assert(itemOn==3);Freslest_MenuPointer(0,60);assert(itemOn==3);Freslest_MenuPointer(310,60);assert(itemOn==3);Freslest_MenuPointer(90,0);assert(itemOn==3);messageToPrint=1;Freslest_MenuPointer(90,60);assert(itemOn==3);messageToPrint=0;setup_active=1;Freslest_MenuPointer(90,60);assert(itemOn==3);setup_active=0;currentMenu=NULL;Freslest_MenuPointer(90,60);puts("PASS: actual Doom menu hover matches stretched rows, skips disabled entries and respects bounds/message/setup/inactive states");}\n'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'hover.c';p.write_text(pre+s[start:end]+test);exe=p.with_suffix('')
 subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
