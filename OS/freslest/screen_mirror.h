#pragma once
#include <stdint.h>
// UI task only; the writer task owns a finished packet until its notification ends.
bool screenMirrorNativeBegin();
void screenMirrorPoll();
void screenMirrorFrame(const uint16_t*,unsigned,unsigned,bool,int,int);
bool screenMirrorActive();
unsigned screenMirrorDiagnostics();
