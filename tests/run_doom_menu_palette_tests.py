"""Run the real status-bar palette selection through menu open/close transitions."""
from pathlib import Path
import os, subprocess, tempfile
r=Path(__file__).resolve().parents[1]
s=(r/'Doom/components/prboom/st_stuff.c').read_text()
a=s.index('static void ST_doPaletteStuff(void)'); b=s.index('\n}\n',a)+3
pre=r'''
#include <assert.h>
#include <stdbool.h>
#define pw_strength 0
#define pw_ironfeet 1
#define NUMREDPALS 8
#define STARTREDPALS 1
#define NUMBONUSPALS 4
#define STARTBONUSPALS 9
#define RADIATIONPAL 13
#define VID_MODE15 1
#define VID_MODE16 2
#define VID_MODE32 3
struct Player {int damagecount,bonuscount,powers[2];} player,*plyr=&player;
int menuactive,st_palette=-1,st_firsttime,selected=-1,calls,mode;
int V_GetMode(void){return mode;}
void V_SetPalette(int pal){selected=pal;++calls;}
'''
test=r'''
int main(void){
 player.damagecount=56;ST_doPaletteStuff();assert(selected==8);
 menuactive=1;ST_doPaletteStuff();assert(selected==0&&st_palette==0);
 int before=calls;ST_doPaletteStuff();assert(selected==0&&calls==before);
 menuactive=0;ST_doPaletteStuff();assert(selected==8);
 player.damagecount=0;player.bonuscount=24;ST_doPaletteStuff();assert(selected==12);
 menuactive=1;ST_doPaletteStuff();assert(selected==0);
 menuactive=0;ST_doPaletteStuff();assert(selected==12);
 player.bonuscount=0;player.powers[pw_ironfeet]=200;ST_doPaletteStuff();assert(selected==13);
 menuactive=1;ST_doPaletteStuff();assert(selected==0);
 menuactive=0;ST_doPaletteStuff();assert(selected==13);
 player.powers[pw_ironfeet]=0;player.powers[pw_strength]=64;ST_doPaletteStuff();assert(selected>0&&selected<9);
 menuactive=1;ST_doPaletteStuff();assert(selected==0);
 menuactive=0;player.powers[pw_strength]=0;ST_doPaletteStuff();assert(selected==0);
 mode=VID_MODE16;st_palette=8;menuactive=1;st_firsttime=0;ST_doPaletteStuff();assert(selected==0&&st_firsttime);
}
'''
with tempfile.TemporaryDirectory() as d:
 p=Path(d)/'menu.c';p.write_text(pre+s[a:b]+test);exe=p.with_suffix('')
 subprocess.run(['gcc','-std=gnu17','-Wall','-Wextra','-Werror','-g','-fsanitize=address,undefined','-fno-pie','-no-pie',str(p),'-o',str(exe)],check=True)
 subprocess.run([str(exe)],check=True,env={**os.environ,'ASAN_OPTIONS':'detect_leaks=0'})
print('PASS: actual Doom menu uses neutral palette and restores damage, pickup, radiation and berserk on close')
