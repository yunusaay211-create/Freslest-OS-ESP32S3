/* Emacs style mode select   -*- C++ -*-
 *-----------------------------------------------------------------------------
 *
 *
 *  PrBoom: a Doom port merged with LxDoom and LSDLDoom
 *  based on BOOM, a modified and improved DOOM engine
 *  Copyright (C) 1999 by
 *  id Software, Chi Hoang, Lee Killough, Jim Flynn, Rand Phares, Ty Halderman
 *  Copyright (C) 1999-2000 by
 *  Jess Haas, Nicolas Kalkhof, Colin Phipps, Florian Schulze
 *  Copyright 2005, 2006 by
 *  Florian Schulze, Colin Phipps, Neil Stevens, Andrey Budko
 *
 *  This program is free software; you can redistribute it and/or
 *  modify it under the terms of the GNU General Public License
 *  as published by the Free Software Foundation; either version 2
 *  of the License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA
 *  02111-1307, USA.
 *
 * DESCRIPTION:
 *  Misc system stuff needed by Doom, implemented for Linux.
 *  Mainly timer handling, and ENDOOM/ENDBOOM.
 *
 *-----------------------------------------------------------------------------
 */

#include <stdio.h>

#include <stdarg.h>
#include <stdlib.h>
#include <ctype.h>
#include <signal.h>
#ifdef _MSC_VER
#define    F_OK    0    /* Check for file existence */
#define    W_OK    2    /* Check for write permission */
#define    R_OK    4    /* Check for read permission */
#include <io.h>
#include <direct.h>
#else
#include <unistd.h>
#endif
#include <sys/stat.h>



#include "config.h"
#include <unistd.h>
#include <sched.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <errno.h>

#include "m_argv.h"
#include "lprintf.h"
#include "doomtype.h"
#include "doomdef.h"
#include "lprintf.h"
#include "m_fixed.h"
#include "r_fps.h"
#include "i_system.h"
#include "i_joy.h"

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "esp_partition.h"
#include "esp_timer.h"
#include "spi_flash_mmap.h"
#include "esp_log.h"
#include <stdint.h>
#include <string.h>

#ifdef __GNUG__
#pragma implementation "i_system.h"
#endif
#include "i_system.h"

#include <sys/time.h>

int realtime=0;


void I_uSleep(unsigned long usecs)
{
	vTaskDelay(pdMS_TO_TICKS((usecs + 999)/1000) ?: 1);
}

static unsigned long getMsTicks(void) {
    return (unsigned long)(esp_timer_get_time()/1000);
}
int I_GetTime_RealTime(void) {
    return (int)(esp_timer_get_time()*TICRATE/1000000);
}

const int displaytime=0;

fixed_t I_GetTimeFrac (void)
{
  unsigned long now;
  fixed_t frac;


  now = getMsTicks();

  if (tic_vars.step == 0)
    return FRACUNIT;
  else
  {
    frac = (fixed_t)((now - tic_vars.start + displaytime) * FRACUNIT / tic_vars.step);
    if (frac < 0)
      frac = 0;
    if (frac > FRACUNIT)
      frac = FRACUNIT;
    return frac;
  }
}


void I_GetTime_SaveMS(void)
{
  if (!movement_smooth)
    return;

  tic_vars.start = getMsTicks();
  tic_vars.next = (unsigned int) ((tic_vars.start * tic_vars.msec + 1.0f) / tic_vars.msec);
  tic_vars.step = tic_vars.next - tic_vars.start;
}

unsigned long I_GetRandomTimeSeed(void)
{
	return 4; //per https://xkcd.com/221/
}

const char* I_GetVersionString(char* buf, size_t sz)
{
  sprintf(buf,"%s v%s (http://prboom.sourceforge.net/)",PACKAGE,VERSION);
  return buf;
}

const char* I_SigString(char* buf, size_t sz, int signum)
{
  return buf;
}

// Map the WAD once. Lump pointers remain valid for the whole game.
static const uint8_t *wad_data;
static size_t wad_size;
static esp_partition_mmap_handle_t wad_handle;
typedef struct { int used; size_t offset; } FileDesc;
static FileDesc fds[32];
static uint32_t read_le32(const uint8_t *p) {
    return (uint32_t)p[0] | ((uint32_t)p[1]<<8) |
           ((uint32_t)p[2]<<16) | ((uint32_t)p[3]<<24);
}
void doom_wad_init(void) {
    if (wad_data) return;
    const esp_partition_t *part=esp_partition_find_first(0x42, 0x06, "wad");
    if (!part) I_Error("WAD partition missing");
    uint8_t hdr[12];
    ESP_ERROR_CHECK(esp_partition_read(part, 0, hdr, sizeof(hdr)));
    if (memcmp(hdr,"IWAD",4)!=0) I_Error("IWAD missing: run flash_all.bat with your COM port");
    uint32_t count=read_le32(hdr+4), dir=read_le32(hdr+8);
    if (!count || dir<12 || (uint64_t)dir+(uint64_t)count*16>part->size)
        I_Error("Invalid WAD directory");
    uint64_t end=(uint64_t)dir+(uint64_t)count*16;
    uint8_t item[16];
    for (uint32_t i=0;i<count;i++) {
        ESP_ERROR_CHECK(esp_partition_read(part, dir+i*16, item, 16));
        uint64_t lump_end=(uint64_t)read_le32(item)+read_le32(item+4);
        if (lump_end>part->size) I_Error("WAD lump outside partition");
        if (lump_end>end) end=lump_end;
    }
    wad_size=(size_t)end;
    ESP_ERROR_CHECK(esp_partition_mmap(part, 0, wad_size, ESP_PARTITION_MMAP_DATA,
                                     (const void**)&wad_data, &wad_handle));
    ESP_LOGI("WAD", "Mapped %u bytes, %u lumps", (unsigned)wad_size, (unsigned)count);
}
static FileDesc *getfd(int fd) {
    if (fd<3 || fd>=32 || !fds[fd].used) I_Error("Invalid WAD descriptor %d", fd);
    return &fds[fd];
}
int I_Open(const char *wad, int flags) {
    (void)flags;

    // The ESP32 build has no real filesystem: both virtual IWAD names map
    // to the same raw "wad" flash partition.  Accept registered DOOM.WAD
    // as well as shareware DOOM1.WAD so the engine name does not matter.
    if (strcasecmp(wad, "DOOM1.WAD") != 0 &&
        strcasecmp(wad, "DOOM.WAD")  != 0)
        return -1;

    doom_wad_init();
    for (int fd=3;fd<32;fd++) if (!fds[fd].used) {
        fds[fd].used=1; fds[fd].offset=0; return fd;
    }
    return -1;
}
int I_Lseek(int fd, off_t off, int whence) {
    FileDesc *f=getfd(fd);
    int64_t pos=whence==SEEK_SET ? off :
                whence==SEEK_CUR ? (int64_t)f->offset+off :
                whence==SEEK_END ? (int64_t)wad_size+off : -1;
    if (pos<0 || pos>(int64_t)wad_size) return -1;
    f->offset=(size_t)pos; return (int)pos;
}
int I_Filelength(int fd) { getfd(fd); return (int)wad_size; }
void I_Close(int fd) { getfd(fd)->used=0; }
void *I_Mmap(void *addr,size_t len,int prot,int flags,int fd,off_t off) {
    getfd(fd);
    if (off<0 || (uint64_t)off+len>wad_size) I_Error("WAD read out of bounds");
    return (void*)(wad_data+off);
}
int I_Munmap(void *addr,size_t length) { return 0; }
void I_Read(int fd,void *buf,size_t len) {
    FileDesc *f=getfd(fd);
    memcpy(buf,I_Mmap(NULL,len,0,0,fd,f->offset),len);
    f->offset+=len;
}

const char *I_DoomExeDir(void)
{
  return "";
}



char* I_FindFile(const char* wfname, const char* ext) { return NULL; }

void I_SetAffinityMask(void)
{
}


// Doom exposes a read-only virtual IWAD; USB VFS runs in the OS app.
int access(const char *name,int mode) {
 if(name&&(!strcasecmp(name,"DOOM.WAD")||!strcasecmp(name,"DOOM1.WAD"))) {if(mode&W_OK) {errno=EROFS;return -1;}return 0;}
 errno=ENOENT;return -1;
}




