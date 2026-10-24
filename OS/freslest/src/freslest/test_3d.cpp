#include "test_3d.h"
#include "../../gfx.h"
#include "../../input.h"
#include "audio_manager.h"
#include <Arduino.h>

#if defined(ESP32)
#include "esp_system.h"
#endif

// ===== Math LUT =====
static int8_t sinLUT[256];
static bool lutInit=false;
static void initLUT(){
  if(lutInit) return;
  for(int i=0;i<256;i++)
    sinLUT[i]=(int8_t)(sin(i*2.0*PI/256.0)*127);
  lutInit=true;
}
static inline int16_t fsin(uint8_t a){ return sinLUT[a]; }
static inline int16_t fcos(uint8_t a){ return sinLUT[(uint8_t)(a+64)]; }

// ===== Shapes =====
struct V3{int16_t x,y,z;};
struct Tri{uint8_t a,b,c;};

// Cube
static const V3 cubeV[8]={
 {-40,-40,-40},{40,-40,-40},{40,40,-40},{-40,40,-40},
 {-40,-40,40},{40,-40,40},{40,40,40},{-40,40,40}
};
static const Tri cubeT[12]={
 {0,1,2},{0,2,3},{4,6,5},{4,7,6},
 {0,4,5},{0,5,1},{2,6,7},{2,7,3},
 {1,5,6},{1,6,2},{0,3,7},{0,7,4}
};

// Pyramid
static const V3 pyrV[5]={
 {-40,-40,-40},{40,-40,-40},{40,-40,40},{-40,-40,40},{0,50,0}
};
static const Tri pyrT[6]={
 {0,1,2},{0,2,3},{0,1,4},{1,2,4},{2,3,4},{3,0,4}
};

enum ShapeId{SHAPE_CUBE,SHAPE_PYRAMID,SHAPE_COUNT};
static ShapeId shape=SHAPE_CUBE;

// ===== State =====
static bool active=false, wantExit=false, autoRot=true;
static uint8_t ax=0, ay=0;
static uint16_t fps=0, frames=0;
static uint32_t fpsT0=0;
static uint32_t okPressStart=0;

static int16_t *sx=nullptr,*sy=nullptr,*rz=nullptr;

bool test3dWantsExit(){ return wantExit; }

void test3dExit(){
  audioStop();
  if(sx) free(sx);
  if(sy) free(sy);
  if(rz) free(rz);
  sx=sy=rz=nullptr;
  active=false;
  wantExit=false;
}

void test3dEnter(){
  initLUT();
  audioStop();
  wantExit=false;
  active=true;

  sx=(int16_t*)ps_malloc(sizeof(int16_t)*8);
  sy=(int16_t*)ps_malloc(sizeof(int16_t)*8);
  rz=(int16_t*)ps_malloc(sizeof(int16_t)*8);
  if(!sx||!sy||!rz){ test3dExit(); return; }

  fpsT0=millis();
  frames=0; fps=0;
  tft.fillScreen(TFT_BLACK);
}

// ===== Helpers =====
static void drawTopBar(){
  static uint32_t lastTopBar=0;
  if(millis()-lastTopBar<200) return;
  lastTopBar=millis();
  tft.fillRect(0,0,tft.width(),12,TFT_DARKGREY);
  tft.setTextSize(1);
  tft.setTextColor(TFT_WHITE,TFT_DARKGREY);
  tft.setCursor(4,2);
  tft.printf("FPS:%u",fps);
#if defined(ESP32)
  tft.setCursor(70,2);
  tft.printf("PSRAM:%uK",(unsigned)(ESP.getFreePsram()/1024));
#endif
  tft.setCursor(tft.width()-70,2);
  if(shape==SHAPE_CUBE) tft.print("CUBE");
  else tft.print("PYRAMID");
}

static void project(const V3* V,int vCount){
  int cx=tft.width()/2;
  int cy=tft.height()/2+6;
  for(int i=0;i<vCount;i++){
    int16_t x=V[i].x, y=V[i].y, z=V[i].z;
    int16_t y1=(y*fcos(ax)-z*fsin(ax))>>7;
    int16_t z1=(y*fsin(ax)+z*fcos(ax))>>7;
    int16_t x2=(x*fcos(ay)+z1*fsin(ay))>>7;
    int16_t z2=(-x*fsin(ay)+z1*fcos(ay))>>7;
    rz[i]=z2;
    sx[i]=cx+x2;
    sy[i]=cy+y1;
  }
}

static void drawTris(const Tri* T,int tCount){

// Depth-sort triangles (painter's algorithm) and fill with color, then draw white wireframe.
static int idx[16]; // enough for cube(12) / pyramid(6)
for (int k = 0; k < tCount; k++) idx[k] = k;

// simple bubble sort (small N)
for (int a = 0; a < tCount - 1; a++) {
  for (int b = a + 1; b < tCount; b++) {
    const Tri &ta = T[idx[a]];
    const Tri &tb = T[idx[b]];
    int za = (rz[ta.a] + rz[ta.b] + rz[ta.c]) / 3;
    int zb = (rz[tb.a] + rz[tb.b] + rz[tb.c]) / 3;
    // draw far first -> near last
    if (za > zb) { int tmp = idx[a]; idx[a] = idx[b]; idx[b] = tmp; }
  }
}

// palette
static const uint16_t pal[] = {
  TFT_BLUE, TFT_GREEN, TFT_RED, TFT_YELLOW, TFT_CYAN, TFT_MAGENTA, TFT_ORANGE
};
const int palN = (int)(sizeof(pal) / sizeof(pal[0]));

for (int n = 0; n < tCount; n++) {
  const Tri &t = T[idx[n]];
  uint16_t col = pal[idx[n] % palN];

  // Fill face color
  tft.fillTriangle(
    sx[t.a], sy[t.a],
    sx[t.b], sy[t.b],
    sx[t.c], sy[t.c],
    col
  );

  
// Wireframe (skip diagonal: draw only the two shortest edges)
int x1=sx[t.a], y1=sy[t.a];
int x2=sx[t.b], y2=sy[t.b];
int x3=sx[t.c], y3=sy[t.c];

int d12=(x1-x2)*(x1-x2)+(y1-y2)*(y1-y2);
int d23=(x2-x3)*(x2-x3)+(y2-y3)*(y2-y3);
int d31=(x3-x1)*(x3-x1)+(y3-y1)*(y3-y1);

if (d12 <= d23 && d12 <= d31) {
  // d12 is shortest -> draw 12 and the shorter of the remaining
  tft.drawLine(x1,y1,x2,y2,TFT_WHITE);
  if (d23 <= d31) tft.drawLine(x2,y2,x3,y3,TFT_WHITE);
  else tft.drawLine(x3,y3,x1,y1,TFT_WHITE);
} else if (d23 <= d12 && d23 <= d31) {
  tft.drawLine(x2,y2,x3,y3,TFT_WHITE);
  if (d12 <= d31) tft.drawLine(x1,y1,x2,y2,TFT_WHITE);
  else tft.drawLine(x3,y3,x1,y1,TFT_WHITE);
} else {
  tft.drawLine(x3,y3,x1,y1,TFT_WHITE);
  if (d12 <= d23) tft.drawLine(x1,y1,x2,y2,TFT_WHITE);
  else tft.drawLine(x2,y2,x3,y3,TFT_WHITE);
}
}
}

void test3dLoop(uint32_t now){
  if(!active) return;

  if(bBack.pressed()){ wantExit=true; return; }

  // OK short/long
  if(bOk.down()){
    if(okPressStart==0) okPressStart=now;
  } else {
    if(okPressStart){
      uint32_t held=now-okPressStart;
      okPressStart=0;
      if(held>600){
        shape=(ShapeId)((shape+1)%SHAPE_COUNT);
      } else {
        autoRot=!autoRot;
      }
    }
  }

  if(bUp.pressed()) ax+=2;
  if(bDown.pressed()) ax-=2;
  if(bLeft.pressed()) ay-=2;
  if(bRight.pressed()) ay+=2;
  if(autoRot){ ax++; ay++; }

  frames++;
  if(now-fpsT0>=1000){
    fps=frames;
    frames=0;
    fpsT0=now;
  }

  tft.fillScreen(TFT_BLACK);
  drawTopBar();

  if(shape==SHAPE_CUBE){
    project(cubeV,8);
    drawTris(cubeT,12);
  } else {
    project(pyrV,5);
    drawTris(pyrT,6);
  }
}