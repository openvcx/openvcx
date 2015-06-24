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


#ifndef __STREAM_PKTZ_FRBUF_H__
#define __STREAM_PKTZ_FRBUF_H__

#include "stream_pktz.h"


typedef struct PKTZ_PARAMS_FRBUF_VID {
  unsigned int                  width;
  unsigned int                  height;
} PKTZ_PARAMS_FRBUF_VID_T;

typedef struct PKTZ_PARAMS_FRBUF_AUD {
  unsigned int                  channels;
  unsigned int                  samplesbufdurationms;
} PKTZ_PARAMS_FRBUF_AUD_T;

typedef struct PKTZ_PARAMS_FRBUF_VIDAUD {
  XC_CODEC_TYPE_T              codecType;
  unsigned int                  clockRateHz;
  int                           isvideo;
  union {
    PKTZ_PARAMS_FRBUF_VID_T     v;
    PKTZ_PARAMS_FRBUF_AUD_T     a;
  } u;
} PKTZ_PARAMS_FRBUF_VIDAUD_T;

typedef struct PKTZ_INIT_PARAMS_FRBUF_T {
  PKTZ_INIT_PARAMS_T            common;
  PKTZ_PARAMS_FRBUF_VIDAUD_T    va;
  PKTQUEUE_T                  **ppPktzFrBufOut; 
} PKTZ_INIT_PARAMS_FRBUF_T;

typedef struct PKTZ_FRBUF {
  const OUTFMT_FRAME_DATA_T    *pFrameData;
  PKTZ_PARAMS_FRBUF_VIDAUD_T    va;
  int                           consecErrors;
  PKTQUEUE_T                   *pFrBufOut;
  PKTQUEUE_T                  **ppPktzFrBufOut;  
} PKTZ_FRBUF_T;


int stream_pktz_frbuf_init(void *pPktzFrbuf, const void *pInitArgs, unsigned int progIdx);
int stream_pktz_frbuf_reset(void *pPktzFrbuf, int fullReset, unsigned int progIdx);
int stream_pktz_frbuf_close(void *pPktzFrbuf);
int stream_pktz_frbuf_addframe(void *pPktzFrbuf, unsigned int progIdx);

#endif // __STREAM_PKTZ_FRBUF_H__

