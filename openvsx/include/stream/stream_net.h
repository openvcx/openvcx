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


#ifndef __STREAM_NET_H__
#define __STREAM_NET_H__

#include "util/bits.h"
#include "formats/mp4boxes.h"

enum STREAM_NET_ADVFR_RC {
  STREAM_NET_ADVFR_RC_ERROR_CRIT = -2,
  STREAM_NET_ADVFR_RC_ERROR = -1,
  STREAM_NET_ADVFR_RC_OK = 0,
  STREAM_NET_ADVFR_RC_NEWPROG = 1,
  STREAM_NET_ADVFR_RC_RESET = 2,
  STREAM_NET_ADVFR_RC_NOCONTENT = 3,
  STREAM_NET_ADVFR_RC_NOTAVAIL = 4,
  STREAM_NET_ADVFR_RC_OVERWRITE  = 5,
  STREAM_NET_ADVFR_RC_NOTENCODED = 6,
};


typedef union {
    SPSPPS_RAW_T            *pspspps;       // H.264 specific sps/pps
    BYTE_STREAM_T           *pSeqHdrs;      // generic sequence header blob
} ADVFR_VID_HDRS_UNION_T;

typedef struct STREAM_NET_ADVFR_DATA {
  void                    *pArgIn;
  uint64_t                *pPts;             // frame playout time in 90KHz
  int64_t                 *pDts;             // frame decode time in 90KHz
  unsigned int            *plen;
  int                      isvid;
  int                     *pkeyframeIn;
  ADVFR_VID_HDRS_UNION_T   hdr;
  XC_CODEC_TYPE_T          codecType;
} STREAM_NET_ADVFR_DATA_T;

typedef enum STREAM_NET_ADVFR_RC (* STREAM_NET_ADVANCEFRAME) (STREAM_NET_ADVFR_DATA_T *);
typedef int64_t (* STREAM_NET_GETFRAMEDECODEHZOFFSET) (void *);
typedef MP4_MDAT_CONTENT_NODE_T * (* STREAM_NET_GETNEXTFRAMESLICE) (void *);
typedef int (* STREAM_NET_HAVENEXTFRAMESLICE) (void *);
typedef int (* STREAM_NET_HAVEMOREDATA) (void *);
typedef int (* STREAM_NET_HAVEDATANOW) (void *);
typedef int (* STREAM_NET_SZNEXTFRAMESLICE) (void *);
typedef int (* STREAM_NET_SZREMAININGSLICE) (void *);
typedef int (* STREAM_NET_SZCOPIEDSLICE) (void *);
typedef int (* STREAM_NET_COPYDATA) (void *, unsigned char *,unsigned int);

enum STREAM_NET_TYPE {
  STREAM_NET_TYPE_UNKNOWN           = 0,
  STREAM_NET_TYPE_FILE              = 1,
  STREAM_NET_TYPE_LIVE              = 2
};

typedef struct STREAM_NET {
  enum STREAM_NET_TYPE              type;
 
  STREAM_NET_ADVANCEFRAME           cbAdvanceFrame;
  STREAM_NET_GETFRAMEDECODEHZOFFSET cbFrameDecodeHzOffset;
  STREAM_NET_GETNEXTFRAMESLICE      cbGetNextFrameSlice;
  STREAM_NET_HAVENEXTFRAMESLICE     cbHaveNextFrameSlice;
  STREAM_NET_HAVEMOREDATA           cbHaveMoreData;
  STREAM_NET_HAVEDATANOW            cbHaveDataNow;
  STREAM_NET_SZNEXTFRAMESLICE       cbSzNextFrameSlice;
  STREAM_NET_SZREMAININGSLICE       cbSzRemainingInSlice;
  STREAM_NET_SZCOPIEDSLICE          cbSzCopiedInSlice;
  STREAM_NET_COPYDATA               cbCopyData;

} STREAM_NET_T;

enum STREAM_NET_ADVFR_RC stream_net_check_time(TIME_VAL *ptvStart, uint64_t *pframeId, unsigned int clockHz, 
                                               unsigned int frameDeltaHz, uint64_t *pPtsOut, int64_t *pPtsOffset);


#endif // __STREAM_NET_H__
