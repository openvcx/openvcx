/** <!--
 *
 *  Copyright (C) 2014 OpenVCX openvcx@gmail.com
 *
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU Affero General Public License as
 *  published by the Free Software Foundation, either version 3 of the
 *  License, or (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Affero General Public License for more details.
 *
 *  You should have received a copy of the GNU Affero General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 *  If you would like this software to be made available to you under an 
 *  alternate license please email openvcx@gmail.com for more information.
 *
 * -->
 */

#ifndef __XCODE_IPC_SRV_H__
#define __XCODE_IPC_SRV_H__

#include "unixcompat.h"
#include "formats/filetypes.h"
#include "ixcode.h"

#if !defined(WIN32) 

#include <semaphore.h>
#include <sys/shm.h>

#endif // WIN32

#define XCODE_IPC_SHMKEY		 0x00007700
#define XCODE_IPC_MEM_SZ                 IXCODE_SZFRAME_MAX + \
                                           sizeof(XCODE_IPC_MEM_HDR_T)

#ifdef __linux__
#define XCODE_IPC_SEM_NAME_SRV           "sem_xcodex_srv"
#define XCODE_IPC_SEM_NAME_CLI           "sem_xcodex_cli"

#else // __linux__
#define XCODE_IPC_SEM_NAME_SRV           "/tmp/sem_xcodex_srv"
#define XCODE_IPC_SEM_NAME_CLI           "/tmp/sem_xcodex_cli"
#endif // __linux__


#define XCODE_IPC_MEM_DATA(p)  ((unsigned char *) (p) + sizeof(XCODE_IPC_MEM_HDR_T))
#define XCODE_IPC_MEM_DATASZ(p)  ((p)->hdr.sz >= sizeof(XCODE_IPC_MEM_HDR_T) ? \
                                  (p)->hdr.sz - sizeof(XCODE_IPC_MEM_HDR_T) : 0)

enum XCODE_IPC_CMD {
  XCODE_IPC_CMD_NONE            = 0,
  XCODE_IPC_CMD_INIT_VID        = 1,
  XCODE_IPC_CMD_INIT_AUD        = 2,
  XCODE_IPC_CMD_CLOSE_VID       = 3,
  XCODE_IPC_CMD_CLOSE_AUD       = 4,
  XCODE_IPC_CMD_ENCODE_VID      = 5,
  XCODE_IPC_CMD_ENCODE_AUD      = 6
};

typedef struct XCODE_IPC_MEM_HDR {
  unsigned int        sz;
  enum XCODE_IPC_CMD  cmd;
  enum IXCODE_RC      cmdrc;
  unsigned int        offsetOut;

  IXCODE_CTXT_T     ctxt;

} XCODE_IPC_MEM_HDR_T;

typedef struct XCODE_IPC_MEM {
  XCODE_IPC_MEM_HDR_T hdr;
} XCODE_IPC_MEM_T;


typedef struct XCODE_IPC_DESCR {
#ifdef WIN32

#define XCODE_FILE_MAPPING_NAME    "xcodexcode_mapping"
#define XCODE_SEM_SRV_NAME         "xcodesxcode_sem_srv"
#define XCODE_SEM_CLI_NAME         "xcodexcode_cli_srv"

#if !defined(key_t)
#define key_t int
#endif // key_t

  HANDLE              hFile;
  HANDLE              hSemSrv;
  HANDLE              hSemCli;
#else
  int                 shmid;
  key_t               key;
  sem_t              *sem_srv;
  sem_t              *sem_cli;
#endif // WIN32
  unsigned int        sz;
  XCODE_IPC_MEM_T    *pmem;
} XCODE_IPC_DESCR_T;


int xcode_init_vid(IXCODE_VIDEO_CTXT_T *pXcode);
int xcode_init_aud(IXCODE_AUDIO_CTXT_T *pXcode);
void xcode_close_vid(IXCODE_VIDEO_CTXT_T *pXcode);
void xcode_close_aud(IXCODE_AUDIO_CTXT_T *pXcode);
enum IXCODE_RC xcode_frame_vid(IXCODE_VIDEO_CTXT_T *pXcode,
                      unsigned char *bufIn, unsigned int lenIn,
                      IXCODE_OUTBUF_T *pout);
enum IXCODE_RC xcode_frame_aud(IXCODE_AUDIO_CTXT_T *pXcode,
                               unsigned char *bufIn, unsigned int lenIn,
                               unsigned char *bufOut, unsigned int lenOut);

#endif // __XCODE_IPC_SRV_H__
