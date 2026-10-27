#pragma once
// Integer pixel layouts. Text is rendered at native size, never downsampled.
inline bool uiCompact(int width){return width==320;}
inline int uiMenuTop(int width,int legacy=54){return uiCompact(width)?42:legacy;}
inline int uiMenuRowHeight(int width,int legacy){return uiCompact(width)?(legacy==39?28:24):legacy;}
inline int uiHeaderHeight(int width){return uiCompact(width)?36:44;}
inline int uiMenuLeft(int width){return uiCompact(width)?12:20;}
inline int uiMenuTextX(int width){return uiCompact(width)?22:40;}
inline int uiMenuTextY(int width,int legacy){return uiCompact(width)?3:legacy;}
