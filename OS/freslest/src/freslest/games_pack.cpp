#include "apps.h"
#include "../../usb_input.h"
#include "../../gfx.h"
#include "../../input.h"
#include "settings.h"
#include "game.h" // DinoGame
#include "audio_manager.h"

// ===== Dino wrappers (existing engine) =====
void dinoEnter() {
  game.reset();
  game.drawStatic();
}
void dinoLoop(uint32_t now) {
  (void)now;
  if (bOk.pressed()) game.jump();
  game.updatePhysics();
  game.drawFramePartial();
 
}

// ===== Snake =====
static constexpr int SN_W=24, SN_H=16;
static int snDir=1; //0 up 1 right 2 down 3 left
static int snLen=3;
static int snX[SN_W*SN_H];
static int snY[SN_W*SN_H];
static int foodX=10, foodY=8;
static bool snAlive=true;
static uint32_t snLast=0;

static void snSpawnFood(){
  while(true){
    int x=random(0,SN_W), y=random(0,SN_H);
    bool ok=true;
    for(int i=0;i<snLen;i++) if(snX[i]==x && snY[i]==y){ ok=false; break; }
    if(ok){ foodX=x; foodY=y; return; }
  }
}


static int snCs=12;
static int snOx=0, snOy=0;
static bool snUi=false;

static void drawSnakeCell(int x,int y,uint16_t c){
  tft.fillRect(snOx + x*snCs, snOy + y*snCs, snCs-1, snCs-1, c);
}
static void drawSnakeFull(){
  // Draw UI once + full redraw (used on enter / restart / death)
  drawHeader("Game: Snake");
  drawFooter("D-Pad Move", "OK Restart  BACK Menu");
  clearBody();

  snCs=12;
  snOx=(480-(SN_W*snCs))/2;
  snOy=70;
  int cs=snCs;
  int ox=snOx;
  int oy=snOy;
  snUi=true;
tft.fillRoundRect(ox-6, oy-6, SN_W*cs+12, SN_H*cs+12, 12, TH().panel);
  tft.drawRoundRect(ox-6, oy-6, SN_W*cs+12, SN_H*cs+12, 12, TH().dim);

  // draw cells
  for(int y=0;y<SN_H;y++){
    for(int x=0;x<SN_W;x++){
      uint16_t c=TH().bg;
      if(x==foodX && y==foodY) c=TH().accent;
      for(int i=0;i<snLen;i++){
        if(snX[i]==x && snY[i]==y){ c=(i==0)?TH().text:TH().dim; break; }
      }
      tft.fillRect(ox+x*cs, oy+y*cs, cs-1, cs-1, c);
    }
  }

  tft.setTextSize(1);
  tft.setTextColor(TH().dim, TH().bg);
  tft.setCursor(24, 285);
  tft.print("Length: ");
  tft.print(snLen);
  if(!snAlive){
    tft.setTextColor(0xF800, TH().bg);
    tft.setCursor(260, 285);
    tft.print("GAME OVER");
  }
}

void snakeEnter(){
  snDir=1;
  snLen=3;
  snAlive=true;
  snX[0]=12; snY[0]=8;
  snX[1]=11; snY[1]=8;
  snX[2]=10; snY[2]=8;
  snSpawnFood();
  snLast=0;
  drawSnakeFull();
}

void snakeLoop(uint32_t now){
  if(!snAlive){
    if(bOk.pressed()) snakeEnter();
    
    return;
  }

  // direction
  if(bUp.pressed()   && snDir!=2) snDir=0;
  if(bRight.pressed()&& snDir!=3) snDir=1;
  if(bDown.pressed() && snDir!=0) snDir=2;
  if(bLeft.pressed() && snDir!=1) snDir=3;

  if(now - snLast < 140) return;
  snLast=now;

  int nx=snX[0], ny=snY[0];
  if(snDir==0) ny--;
  if(snDir==1) nx++;
  if(snDir==2) ny++;
  if(snDir==3) nx--;

  if(nx<0) nx=SN_W-1;
  if(nx>=SN_W) nx=0;
  if(ny<0) ny=SN_H-1;
  if(ny>=SN_H) ny=0;

  // collision with self
  for(int i=0;i<snLen;i++){
    if(snX[i]==nx && snY[i]==ny){ snAlive=false; audioGameOver(); drawSnakeFull(); return; }
  }

  // move body
  for(int i=snLen;i>0;i--){
    snX[i]=snX[i-1];
    snY[i]=snY[i-1];
  }
  snX[0]=nx; snY[0]=ny;

  // eat
  if(nx==foodX && ny==foodY){
    audioSnake();
    snLen++;
    if(snLen>SN_W*SN_H-1) snLen=SN_W*SN_H-1;
    snSpawnFood();
  }

  // incremental redraw (no flash): erase tail, draw head + food
  int cs=snCs;
  int ox=snOx;
  int oy=snOy;
  // erase last tail cell (after move)
  int tx=snX[snLen-1];
  int ty=snY[snLen-1];
  tft.fillRect(ox+tx*cs, oy+ty*cs, cs-1, cs-1, TH().bg);
  // redraw food
  tft.fillRect(ox+foodX*cs, oy+foodY*cs, cs-1, cs-1, TH().selbg);
  // draw head
  tft.fillRect(ox+snX[0]*cs, oy+snY[0]*cs, cs-1, cs-1, TH().accent);
}

// ===== 2048 =====
static uint16_t g[4][4];
static uint32_t gLast=0;
static bool gOver=false;

static void gAdd(){
  int empty=0;
  for(int y=0;y<4;y++) for(int x=0;x<4;x++) if(g[y][x]==0) empty++;
  if(empty==0) return;
  int pick=random(0, empty);
  for(int y=0;y<4;y++){
    for(int x=0;x<4;x++){
      if(g[y][x]==0){
        if(pick==0){ g[y][x]=(random(0,10)==0)?4:2; return; }
        pick--;
      }
    }
  }
}

static bool gCanMove(){
  for(int y=0;y<4;y++) for(int x=0;x<4;x++) if(g[y][x]==0) return true;
  for(int y=0;y<4;y++) for(int x=0;x<4;x++){
    if(x<3 && g[y][x]==g[y][x+1]) return true;
    if(y<3 && g[y][x]==g[y+1][x]) return true;
  }
  return false;
}

static bool gSlide(int dx,int dy){
  bool moved=false;
  for(int pass=0; pass<3; pass++){
    for(int y=0;y<4;y++){
      for(int x=0;x<4;x++){
        int nx=x+dx, ny=y+dy;
        if(nx<0||nx>3||ny<0||ny>3) continue;
        if(g[y][x]==0) continue;
        if(g[ny][nx]==0){
          g[ny][nx]=g[y][x]; g[y][x]=0; moved=true;
        }
      }
    }
  }
  return moved;
}

static bool gMerge(int dx,int dy){
  bool merged=false;
  // order depends on direction
  int xs[4]={0,1,2,3}, ys[4]={0,1,2,3};
  if(dx==1){ xs[0]=3;xs[1]=2;xs[2]=1;xs[3]=0; }
  if(dy==1){ ys[0]=3;ys[1]=2;ys[2]=1;ys[3]=0; }

  for(int yi=0; yi<4; yi++){
    for(int xi=0; xi<4; xi++){
      int x=xs[xi], y=ys[yi];
      int nx=x+dx, ny=y+dy;
      if(nx<0||nx>3||ny<0||ny>3) continue;
      if(g[y][x]==0) continue;
      if(g[ny][nx]==g[y][x]){
        g[ny][nx]*=2;
        g[y][x]=0;
        merged=true;
      }
    }
  }
  return merged;
}

static void draw2048(){
  /* no full-screen clear (avoids flash) */
  drawHeader("Game: 2048");
  drawFooter("D-Pad Move", "OK Restart  BACK Menu");
  clearBody();

  int s=52, pad=4;
  int ox=(480-(4*s))/2;
  int oy=70;
  tft.fillRoundRect(ox-10, oy-10, 4*s+20, 4*s+20, 14, TH().panel);
  tft.drawRoundRect(ox-10, oy-10, 4*s+20, 4*s+20, 14, TH().dim);

  for(int y=0;y<4;y++){
    for(int x=0;x<4;x++){
      int v=g[y][x];
      uint16_t fill = v? TH().selbg : TH().bg;
      tft.fillRoundRect(ox+x*s+pad, oy+y*s+pad, s-2*pad, s-2*pad, 12, fill);
      tft.drawRoundRect(ox+x*s+pad, oy+y*s+pad, s-2*pad, s-2*pad, 12, TH().dim);
      if(v){
        tft.setTextColor(TH().text, fill);
        tft.setTextSize(2);
        tft.setCursor(ox+x*s+20, oy+y*s+30);
        tft.print(v);
      }
    }
  }

  if(gOver){
    tft.setTextSize(2);
    tft.setTextColor(0xF800, TH().bg);
    tft.setCursor(160, 285);
    tft.print("GAME OVER");
  }
}

void g2048Enter(){
  memset(g,0,sizeof(g));
  gAdd(); gAdd();
  gOver=false;
  draw2048();
}

void g2048Loop(uint32_t now){
  if(gOver){
    if(bOk.pressed()) g2048Enter();
    return;
  }
  bool action=false;
  int dx=0,dy=0;
  if(bUp.pressed()) dy=-1;
  if(bDown.pressed()) dy=1;
  if(bLeft.pressed()) dx=-1;
  if(bRight.pressed()) dx=1;
  if(dx||dy){
    // move is in opposite of dir for sliding (towards dir)
    // We'll implement by repeated slide+merge+slide in direction
    bool moved1, merged;
    // For our slide and merge, dx/dy indicates target cell direction, so use dx/dy as pressed direction.
    moved1 = gSlide(dx,dy);
    merged = gMerge(dx,dy);
    if(merged) gSlide(dx,dy);
    if(moved1||merged){
      audioGame();
      gAdd();
      // win check
      for(int yy=0;yy<4;yy++) for(int xx=0;xx<4;xx++) if(g[yy][xx]>=2048) { audioWin(); }

      if(!gCanMove()) { gOver=true; audioGameOver(); }
      draw2048();
    }
  }
}

// ===== Minesweeper =====
static constexpr int MN_W=10, MN_H=8, MN_BOMBS=12;
static uint8_t mnVal[MN_H][MN_W]; // 0-8, 9 bomb
static uint8_t mnOpen[MN_H][MN_W];
static uint8_t mnFlag[MN_H][MN_W];
static int mnCx=0, mnCy=0;
static bool mnDead=false;

static void mnPlace(){
  memset(mnVal,0,sizeof(mnVal));
  memset(mnOpen,0,sizeof(mnOpen));
  memset(mnFlag,0,sizeof(mnFlag));
  mnCx=0; mnCy=0;
  mnDead=false;

  int placed=0;
  while(placed<MN_BOMBS){
    int x=random(0,MN_W), y=random(0,MN_H);
    if(mnVal[y][x]==9) continue;
    mnVal[y][x]=9; placed++;
  }
  // numbers
  for(int y=0;y<MN_H;y++){
    for(int x=0;x<MN_W;x++){
      if(mnVal[y][x]==9) continue;
      int c=0;
      for(int yy=y-1;yy<=y+1;yy++)
        for(int xx=x-1;xx<=x+1;xx++)
          if(xx>=0&&xx<MN_W&&yy>=0&&yy<MN_H&&mnVal[yy][xx]==9) c++;
      mnVal[y][x]=c;
    }
  }
}

static void mnFlood(int x,int y){
  if(x<0||x>=MN_W||y<0||y>=MN_H) return;
  if(mnOpen[y][x]||mnFlag[y][x]) return;
  mnOpen[y][x]=1;
  if(mnVal[y][x]!=0) return;
  for(int yy=y-1;yy<=y+1;yy++)
    for(int xx=x-1;xx<=x+1;xx++)
      if(!(xx==x&&yy==y)) mnFlood(xx,yy);
}

static void drawMines(){
  /* no full-screen clear (avoids flash) */
  drawHeader("Game: Mines");
  drawFooter("D-Pad Move", "OK Open  Hold OK Flag  BACK Menu");
  clearBody();

  int cs=22;
  int ox=(480-(MN_W*cs))/2;
  int oy=80;

  tft.fillRoundRect(ox-8, oy-8, MN_W*cs+16, MN_H*cs+16, 12, TH().panel);
  tft.drawRoundRect(ox-8, oy-8, MN_W*cs+16, MN_H*cs+16, 12, TH().dim);

  for(int y=0;y<MN_H;y++){
    for(int x=0;x<MN_W;x++){
      int px=ox+x*cs, py=oy+y*cs;
      bool sel=(x==mnCx && y==mnCy);
      uint16_t cell = mnOpen[y][x] ? TH().bg : TH().panel;
      tft.fillRect(px, py, cs-1, cs-1, cell);
      if(sel) tft.drawRect(px, py, cs-1, cs-1, TH().accent);

      if(mnFlag[y][x]){
        tft.setTextSize(2);
        tft.setTextColor(TH().accent, cell);
        tft.setCursor(px+8, py+6);
        tft.print("F");
      } else if(mnOpen[y][x]){
        if(mnVal[y][x]==9){
          tft.setTextSize(2);
          tft.setTextColor(0xF800, cell);
          tft.setCursor(px+8, py+6);
          tft.print("*");
        } else if(mnVal[y][x]>0){
          tft.setTextSize(2);
          tft.setTextColor(TH().text, cell);
          tft.setCursor(px+10, py+6);
          tft.print((int)mnVal[y][x]);
        }
      }
    }
  }

  if(mnDead){
    tft.setTextSize(2);
    tft.setTextColor(0xF800, TH().bg);
    tft.setCursor(330, 110);
    tft.print("BOOM!");
    tft.setTextSize(1);
    tft.setCursor(330, 140);
    tft.print("OK=Restart");
  }
}

static uint32_t mnOkDownAt=0;
static bool mnOkWasDown=false;

void minesEnter(){
  mnPlace();
  drawMines();
  mnOkWasDown=false;
}

void minesLoop(uint32_t now){
  if(usbInputHit(130,80,220,176)){
    mnCx=(usbPointer.x-130)/22;mnCy=(usbPointer.y-80)/22;
    if(mnDead){minesEnter();return;}
    if(!mnFlag[mnCy][mnCx]){if(mnVal[mnCy][mnCx]==9){mnOpen[mnCy][mnCx]=1;mnDead=true;audioGameOver();}else mnFlood(mnCx,mnCy);}
    drawMines();
  }
  if(bLeft.pressed())  { mnCx=(mnCx-1+MN_W)%MN_W; drawMines(); }
  if(bRight.pressed()) { mnCx=(mnCx+1)%MN_W; drawMines(); }
  if(bUp.pressed())    { mnCy=(mnCy-1+MN_H)%MN_H; drawMines(); }
  if(bDown.pressed())  { mnCy=(mnCy+1)%MN_H; drawMines(); }

  if(mnDead){
    if(bOk.pressed()) minesEnter();
    return;
  }

  bool okDown=bOk.down();
  if(okDown && !mnOkWasDown){ mnOkWasDown=true; mnOkDownAt=now; }
  if(!okDown && mnOkWasDown){
    uint32_t held=now-mnOkDownAt;
    mnOkWasDown=false;
    if(held>=500){
      mnFlag[mnCy][mnCx] ^= 1;
      drawMines();
    } else {
      if(mnFlag[mnCy][mnCx]) return;
      if(mnVal[mnCy][mnCx]==9){
        mnOpen[mnCy][mnCx]=1;
        mnDead=true; audioGameOver();
        drawMines();
      } else {
        mnFlood(mnCx, mnCy);
        drawMines();
      }
    }
  }
}

// ===== Tetris =====
static constexpr int TW=10, THH=20;
static uint8_t tBoard[THH][TW];
static int tX=3, tY=0, tR=0, tPiece=0;
static uint32_t tLast=0;
static bool tDead=false;
static int tScore=0;

// 7 tetrominoes in 4 rotations, 4 blocks each, in 4x4 matrix bits
static const uint16_t TET[7][4] = {
  {0x0F00,0x2222,0x00F0,0x4444}, // I
  {0x8E00,0x6440,0x0E20,0x44C0}, // J
  {0x2E00,0x4460,0x0E80,0xC440}, // L
  {0x6600,0x6600,0x6600,0x6600}, // O
  {0x6C00,0x4620,0x06C0,0x8C40}, // S
  {0x4E00,0x4640,0x0E40,0x4C40}, // T
  {0xC600,0x2640,0x0C60,0x4C80}  // Z
};

static bool tCell(uint16_t m,int x,int y){
  return (m & (0x8000 >> (y*4+x))) !=0;
}

static bool tCollide(int px,int py,int pr,int pp){
  uint16_t m=TET[pp][pr];
  for(int y=0;y<4;y++){
    for(int x=0;x<4;x++){
      if(!tCell(m,x,y)) continue;
      int gx=px+x, gy=py+y;
      if(gx<0||gx>=TW||gy<0||gy>=THH) return true;
      if(tBoard[gy][gx]) return true;
    }
  }
  return false;
}

static void tLock(){
  uint16_t m=TET[tPiece][tR];
  for(int y=0;y<4;y++){
    for(int x=0;x<4;x++){
      if(!tCell(m,x,y)) continue;
      int gx=tX+x, gy=tY+y;
      if(gy>=0&&gy<THH&&gx>=0&&gx<TW) tBoard[gy][gx]=tPiece+1;
    }
  }
  // clear lines
  for(int y=0;y<THH;y++){
    bool full=true;
    for(int x=0;x<TW;x++) if(!tBoard[y][x]){ full=false; break; }
    if(full){
      for(int yy=y; yy>0; yy--)
        for(int x=0;x<TW;x++) tBoard[yy][x]=tBoard[yy-1][x];
      for(int x=0;x<TW;x++) tBoard[0][x]=0;
      tScore+=100;
    audioGame();
    }
  }
}

static void tNew(){
  tPiece=random(0,7);
  tR=0;
  tX=3; tY=0;
  if(tCollide(tX,tY,tR,tPiece)){ tDead=true; audioGameOver(); }
}

static bool tUi=false;
static void drawTetrisStatic(){
  drawHeader("Game: Tetris");
  drawFooter("L/R Move  U Rotate", "D Drop  OK Restart  BACK Menu");
  clearBody();
  int cs=10;
  int ox=(480-(TW*cs))/2;
  int oy=60;
  tft.fillRoundRect(ox-10, oy-10, TW*cs+20, THH*cs+20, 12, TH().panel);
  tft.drawRoundRect(ox-10, oy-10, TW*cs+20, THH*cs+20, 12, TH().dim);
  tUi=true;
}

static void drawTetris(){
  // Flash fix: static UI drawn once
  if(!tUi) drawTetrisStatic();
  int cs=10;
  int ox=(480-(TW*cs))/2;
  int oy=60;
// board
  for(int y=0;y<THH;y++){
    for(int x=0;x<TW;x++){
      uint16_t c = tBoard[y][x] ? TH().selbg : TH().bg;
      tft.fillRect(ox+x*cs, oy+y*cs, cs-1, cs-1, c);
    }
  }

  // piece
  if(!tDead){
    uint16_t m=TET[tPiece][tR];
    for(int y=0;y<4;y++)for(int x=0;x<4;x++){
      if(!tCell(m,x,y)) continue;
      int gx=tX+x, gy=tY+y;
      if(gy<0) continue;
      tft.fillRect(ox+gx*cs, oy+gy*cs, cs-1, cs-1, TH().accent);
    }
  }

  tft.setTextSize(2);
  tft.setTextColor(TH().text, TH().bg);
  tft.setCursor(20, 90);
  tft.print("Score");
  tft.setCursor(20, 120);
  tft.print(tScore);

  if(tDead){
    tft.setTextColor(0xF800, TH().bg);
    tft.setCursor(20, 170);
    tft.print("GAME OVER");
  }
}

void tetrisEnter(){
  audioTetris();
  tUi=false;
  memset(tBoard,0,sizeof(tBoard));
  tScore=0;
  tDead=false;
  tNew();
  tLast=0;
  drawTetris();
}

void tetrisLoop(uint32_t now){
  
  if(tDead){
    if(bOk.pressed()) tetrisEnter();
    return;
  }

  // input
  if(bLeft.pressed() && !tCollide(tX-1,tY,tR,tPiece)) { tX--; drawTetris(); }
  if(bRight.pressed()&& !tCollide(tX+1,tY,tR,tPiece)) { tX++; drawTetris(); }
  if(bUp.pressed()){
    int nr=(tR+1)%4;
    if(!tCollide(tX,tY,nr,tPiece)) { tR=nr; drawTetris(); }
  }
  if(bDown.pressed()){
    while(!tCollide(tX,tY+1,tR,tPiece)) tY++;
    drawTetris();
  }

  if(now - tLast < 350) return;
  tLast=now;

  if(!tCollide(tX,tY+1,tR,tPiece)){
    tY++;
    drawTetris();
  } else {
    tLock();
    tNew();
    drawTetris();
  }
}

// --- Dino speed scaling ---
#define DINO_SPEED_SCALE 1

// --- Snake apple pulse enabled ---
#define SNAKE_APPLE_PULSE 1

// --- Tetris block outline ---
#define TETRIS_OUTLINE 1
