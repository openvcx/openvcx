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


#ifndef __XCODE_IPC_H__
#define __XCODE_IPC_H__

#include "unixcompat.h"
#include "formats/filetypes.h"
#include "ixcode.h"

#if !defined(WIN32) && !defined(ANDROID)

#include <sys/shm.h>
#include <sys/sem.h>

#endif // WIN32

#if defined(XCODE_IPC)

#include "xcode_ipc_srv.h"

XCODE_IPC_DESCR_T *xcode_ipc_open(unsigned int memsz, key_t key);
void xcode_ipc_close(XCODE_IPC_DESCR_T *pIpc);

#else

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

#endif // XCODE_IPC

#endif // __XCODE_IPC_H__
