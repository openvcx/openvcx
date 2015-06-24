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


#ifndef __MP4_MOOF_H__
#define __MP4_MOOF_H__

#include "formats/mp4track.h"
#include "formats/mp4creator.h"
#include "formats/mpd.h"

#define MOOF_USE_INITMP4_DEFAULT            1
#define MOOF_USE_RAP_DEFAULT                1
//#define MOOF_TRAF_MAX_DURATION_SEC_DEFAULT  .5f
#define MOOF_TRAF_MAX_DURATION_SEC_DEFAULT  0.0f     // 1 moof per file
#define MOOF_MP4_MIN_DURATION_SEC_DEFAULT   5.0f
#define MOOF_MP4_MAX_DURATION_SEC_DEFAULT   10.0f

typedef struct MOOF_MDAT_WRITER {
  FILE_STREAM_T             stream;
} MOOF_MDAT_WRITER_T;

typedef struct MOOF_INIT_CTXT {
  unsigned int               clockHz;            // Suggested container clock rate
  int                        useInitMp4;
  int                        useRAP;   // Use Random Access Points
  float                      moofTrafMaxDurationSec;  // set to 0 for 1 moof / .m4s
  float                      mp4MaxDurationSec;
  float                      mp4MinDurationSec;  // if min duration is enabled, the .m4s will be created
                                                 // beginning on a keyframe after the elapsed duration
  int                        nodelete;
  unsigned int               requestOutidx;      // xcode outidx of this instance
  int                        mixAudVid;          // create one .m4s with both audio and video moof

  int                        syncMultiOutidx;    // create .m4s on same boundary when dealing
                                                 // multiple xcode outidx outputs, to accomodate
                                                 // dash multiple represetations in adaptationset
  int                        syncedByOtherOutidx;

  struct  MP4_CREATE_STATE_MOOF  *psharedCtxts[IXCODE_VIDEO_OUT_MAX];
} MOOF_INIT_CTXT_T;

typedef struct MOOF_STATE_INT {
  int                        isvid;
  int                        isaud;
  int                        haveFirst;
  BOX_TRUN_ENTRY_T          *pTrunEntryPrior;
  MOOF_MDAT_WRITER_T         mdat;
  MP4_TRAK_T                 trak;

  char                       initPath[VSX_MAX_PATH_LEN]; // optional path of the initializer .m4s 
  char                       pathtmp[VSX_MAX_PATH_LEN];  // tmp path of the current output .m4s containing moofs
  char                       path[VSX_MAX_PATH_LEN];     // path of the current output .m4s containing moofs
  char                       path2[VSX_MAX_PATH_LEN];    // path of the current output .m4s containing moofs
  char                       path_uniqidstr[64];
  BOX_MVHD_T                *pMvhd;
  MP4_CONTAINER_T           *pMp4;

  uint64_t                   mdhd_duration;          // duration counter of 
  uint32_t                   mdhd_timescale;

  int                        needptsStart;
  uint64_t                   ptsElapsed;             // pts (90KHz) used for TRUN box duration calculation
  uint64_t                   ptsMoofBoxStart;        // pts (90KHz) of first frame within each moof box
  uint64_t                   duration;               // duration counter of (current frame - ptsStart)
                                                     // The timescale is trak.pMdhd->timescale = mdhd_timescale;
  uint64_t                   durationAtMp4Start; 

  // duplicate from MP4_CREATE_STATE_MOOF
  struct CODEC_AV_CTXT      *pAvCtxt;
  MOOF_INIT_CTXT_T          *pMoofInitCtxt;

  int                        wroteMoof;
  int                        wroteInitMp4;
  int                        wroteMp4;
  unsigned int               moofIdxInMp4;      // moof index within .m4s 
  unsigned int               moofSequence;      // absolute moof sequence id 
                                                // (this should be contiguous across .m4s segments)
  MPD_ADAPTATION_T           mpdAdaptation;
} MOOF_STATE_INT_T; 

#define MOOF_AV_MAX 2

typedef struct MP4_CREATE_STATE_MOOF {
  MOOF_INIT_CTXT_T           moofInitCtxt;
  uint64_t                   ptsStart;             // pts (90KHz) of first frame within first output .m4s 
  uint64_t                   ptsMp4Start;          // pts (90KHz) of first frame within current output .m4s 

  const char                *outdir;            // media segment output dir ("html/dash")
  struct CODEC_AV_CTXT      *pAvCtxt;
  unsigned int               mp4Sequence;       // .m4s sequence id

  int                        haveFirst;
  MOOF_STATE_INT_T           stateAV[MOOF_AV_MAX];
  MPD_CREATE_CTXT_T          mpdCtxt;

} MP4_CREATE_STATE_MOOF_T;

int mp4moof_init(MP4_CREATE_STATE_MOOF_T *pCtxt, 
                 CODEC_AV_CTXT_T *pAvCtxt,
                 const MOOF_INIT_CTXT_T *pMoofInitCtxt,
                 const DASH_INIT_CTXT_T *pDashInitCtxt);

int mp4moof_close(MP4_CREATE_STATE_MOOF_T *pCtxt);
int mp4moof_addFrame(MP4_CREATE_STATE_MOOF_T *pCtxt, const struct OUTFMT_FRAME_DATA *pFrame);

#endif // __MP4_MOOF_H__
