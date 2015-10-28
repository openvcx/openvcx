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


#ifndef __STREAM_OUTFMT_H__
#define __STREAM_OUTFMT_H__

#include "codecs/avcc.h"
#include "codecs/esds.h"
#include "util/bits.h"
#include "util/pktqueue.h"
#include "xcode/xcode_types.h"

#define STREAM_RTP_MTU_DEFAULT               1410


#define STREAMER_LIVEQ_MAX                   VSX_LIVEQ_MAX
#define STREAMER_LIVEQ_DEFAULT               VSX_LIVEQ_DEFAULT 

#define STREAMER_RTP_MAX                     VSX_RTP_MAX
#define STREAMER_RTP_DEFAULT                 VSX_RTP_DEFAULT

#define STREAMER_OUTFMT_MAX                  STREAMER_LIVEQ_MAX
#define STREAMER_OUTFMT_DEFAULT              STREAMER_LIVEQ_DEFAULT

#define STREAMER_RTMPQ_SLOTS_DEFAULT         320
#define STREAMER_RTMPQ_SZSLOT_DEFAULT        20000 
#define STREAMER_RTMPQ_SZSLOT_MAX            (STREAMER_RTMPQ_SZSLOT_DEFAULT * 16) 
#define STREAMER_RTMPQ_SZSLOT_MAX_LIMIT      IXCODE_SZFRAME_MAX 

#define STREAMER_RTSPINTERQ_SLOTS_DEFAULT    800
#define STREAMER_RTSPINTERQ_SZSLOT_DEFAULT   (STREAM_RTP_MTU_DEFAULT + 64) 
#define STREAMER_RTSPINTERQ_SZSLOT_MAX       PACKETGEN_PKT_UDP_DATA_SZ 

#define STREAMER_TSLIVEQ_SLOTS_DEFAULT       500 
#define STREAMER_TSLIVEQ_SLOTS_MAX           2000 
#define STREAMER_TSLIVEQ_SZSLOT_DEFAULT      (PACKETGEN_PKT_UDP_DATA_SZ /188 * 188)

#define STREAMER_QID_VID_FRAMES              10
#define STREAMER_QID_AUD_FRAMES              11
#define STREAMER_QID_TSLIVE                  100
#define STREAMER_QID_RTMP                    300
#define STREAMER_QID_RTMP_PUBLISH            400
#define STREAMER_QID_RTSP_INTERLEAVED        500
#define STREAMER_QID_FLV                     600
#define STREAMER_QID_FLV_RECORD              700
#define STREAMER_QID_MKV                     800
#define STREAMER_QID_MKV_RECORD              900
#define STREAMER_QID_MOOF                   1000
#define STREAMER_QID_PIPVID                 1100
#define STREAMER_QID_ORDERING               1200
#define STREAMER_QID_NACK                   1300

typedef union {
  SPSPPS_RAW_T            h264;       // H.264 specific sps/pps
  //BYTE_STREAM_T           mpg4v;      // generic sequence header blob
  MPG4V_SEQ_HDRS_T        mpg4v;
} OUTFMT_VID_HDRS_UNION_T;

#define OUTFMT_FRAME_FLAG_HAVENEXT          0x01

typedef struct OUTFMT_FRAME_DATA {
  uint16_t                    streamType;
  uint8_t                     isvid;
  uint8_t                     isaud;
  uint16_t                    channelId;
  uint16_t                    flags;
  XC_CODEC_TYPE_T             mediaType;

  XCODE_OUTBUF_T              xout;

  #define OUTFMT_IDX(pFrame) ((pFrame)->xout.outidx)

  #define OUTFMT_DATA(pFrame)   ((pFrame)->xout.outbuf.poffsets[(pFrame)->xout.outidx])
  #define OUTFMT_DATA_IDX(pFrame, idx)   ((pFrame)->xout.outbuf.poffsets[idx])

  #define OUTFMT_LEN(pFrame)   ((pFrame)->xout.outbuf.lens[(pFrame)->xout.outidx])
  #define OUTFMT_LEN_IDX(pFrame, idx)   ((pFrame)->xout.outbuf.lens[idx])

  #define OUTFMT_PTS(pFrame)   ((pFrame)->xout.tms[(pFrame)->xout.outidx].pts)
  #define OUTFMT_PTS_IDX(pFrame, idx)   ((pFrame)->xout.tms[idx].pts)

  #define OUTFMT_DTS(pFrame)   ((pFrame)->xout.tms[(pFrame)->xout.outidx].dts)
  #define OUTFMT_DTS_IDX(pFrame, idx)   ((pFrame)->xout.tms[idx].dts)

  #define OUTFMT_KEYFRAME(pFrame)   ((pFrame)->xout.keyframes[(pFrame)->xout.outidx])
  #define OUTFMT_KEYFRAME_IDX(pFrame, idx)   ((pFrame)->xout.keyframes[idx])

  #define OUTFMT_VSEQHDR(pFrame) ((pFrame)->vseqhdrs[(pFrame)->xout.outidx])
  #define OUTFMT_VSEQHDR_IDX(pFrame, idx) ((pFrame)->vseqhdrs[idx])

  OUTFMT_VID_HDRS_UNION_T        vseqhdrs[IXCODE_VIDEO_OUT_MAX];

  union {
    ESDS_DECODER_CFG_T          aac;
  } aseqhdr;

} OUTFMT_FRAME_DATA_T;

typedef int (*OUTFMT_CB_ONFRAME)(void *, const OUTFMT_FRAME_DATA_T *);

typedef struct STREAMER_LIVE_LIVEQ {
  PKTQUEUE_T                  **pQs;
  const unsigned int            max;
  unsigned int                  numActive;
  pthread_mutex_t               mtx;
  PKTQUEUE_CONFIG_T             qCfg;
} STREAMER_LIVEQ_T;

#define GOP_HISTORY_COUNT 4
typedef struct GOP_HISTORY_CTXT {
  unsigned int gopIdx;
  unsigned int gops[GOP_HISTORY_COUNT];
  int64_t gopTms[GOP_HISTORY_COUNT];
  float gopAvg;
  float gopAvgPtsDuration;
  unsigned int keyframeCnt;
  uint64_t lastKeyframePts;
} GOP_HISTORY_CTXT_T;

typedef struct LIVEQ_CB_CTXT {
  PKTQUEUE_T            **ppQ;
  PKTQUEUE_PKT_T         *pSwappedSlot;
  pthread_mutex_t        *pMtx;
  int                     running;
  OUTFMT_CB_ONFRAME       cbOnFrame;
  struct FRAME_THIN_CTXT *pFrameThinCtxt;
  GOP_HISTORY_CTXT_T     *pGOPHistory;
  void                   *pCbData;
  char                   *id;
  unsigned int            idx;
  pthread_t               tid;
} LIVEQ_CB_CTXT_T;

typedef struct OUTFMT_CFG {
  int                          do_outfmt;
  int                          paused;

  PKTQUEUE_T                  *pQ;
  STREAM_STATS_T              *pstats;
  LIVEQ_CB_CTXT_T              cbCtxt;
  pthread_mutex_t              mtx;

  struct STREAMER_OUTFMT      *pLiveFmt;  // pointer back to STERAMER_OUTFMT_T
} OUTFMT_CFG_T;


typedef struct STREAMER_OUTFMT {
  OUTFMT_CFG_T                *poutFmts;
  pthread_mutex_t              mtx;
  const unsigned int           max;
  PKTQUEUE_CONFIG_T            qCfg;
  int                          do_outfmt;
  unsigned int                 numActive;
  float                        bufferDelaySec; 
  GOP_HISTORY_CTXT_T           gopHistory;
} STREAMER_OUTFMT_T;


typedef enum STREAMER_OUTFMT_IDX {
  STREAMER_OUTFMT_IDX_RTMP         = 0,
  STREAMER_OUTFMT_IDX_RTMPPUBLISH  = 1,
  STREAMER_OUTFMT_IDX_FLV          = 2,
  STREAMER_OUTFMT_IDX_FLVRECORD    = 3,
  STREAMER_OUTFMT_IDX_MKV          = 4,
  STREAMER_OUTFMT_IDX_MKVRECORD    = 5,
  STREAMER_OUTFMT_IDX_MOOF         = 6,
  STREAMER_OUTFMT_IDX_MAX          = 7
} STREAMER_OUTFMT_IDX_T;

typedef struct STREAMER_OUTFMT_BUFFER {
  int                          active;
  struct PKTQUEUE             *pOrderingQ;
  uint64_t                     lastVidPts;
  uint64_t                     lastAudPts;
  int                          haveAudQueued;
  int                          haveVidQueued;
  int                          haveVid;
  int                          haveAud;
} STREAMER_OUTFMT_BUFFER_T;

typedef struct STREAMER_OUTFMT_LIST {
  STREAMER_OUTFMT_T            out[STREAMER_OUTFMT_IDX_MAX];
  STREAMER_OUTFMT_BUFFER_T     ordering;
} STREAMER_OUTFMT_LIST_T;

int outfmt_init(STREAMER_OUTFMT_LIST_T *pLiveFmts, int orderingQSz);
void outfmt_close(STREAMER_OUTFMT_LIST_T *pLiveFmts);

OUTFMT_CFG_T *outfmt_setCb(STREAMER_OUTFMT_T *pLiveFmt,
                           OUTFMT_CB_ONFRAME cbOnFrame,
                           void *pOutFmtCbData,
                           const PKTQUEUE_CONFIG_T *pQCfg,
                           STREAM_STATS_T *pstats,
                           int paused,
                           int frameThin,
                           unsigned int *pNumUsed);

int outfmt_removeCb(OUTFMT_CFG_T *pOutFmt);
void outfmt_pause(OUTFMT_CFG_T *pOutFmt, int paused);
int outfmt_invokeCbs(STREAMER_OUTFMT_LIST_T *pLiveFmts, OUTFMT_FRAME_DATA_T *pFrameData);
int outfmt_getoutidx(const char *url, const char **ppend);




enum PKTQUEUE_RC liveq_addpkt(STREAMER_LIVEQ_T *pLiveQ, unsigned int idxQ,
                              OUTFMT_FRAME_DATA_T *pFrame);
PKTQUEUE_T *liveq_create(unsigned int maxPkts, unsigned int szPktArg,
                         unsigned int growMaxPkts, unsigned int growSzPktsArg,
                         unsigned int prebufOffset);
int liveq_start_cb(LIVEQ_CB_CTXT_T *pCtxt);
int liveq_stop_cb(LIVEQ_CB_CTXT_T *pCtxt);



typedef struct OUTFMT_FRAME_DATA_WRAP {
  OUTFMT_FRAME_DATA_T              data;
  struct OUTFMT_FRAME_DATA_WRAP   *pnext;
} OUTFMT_FRAME_DATA_WRAP_T;

typedef struct OUTFMT_QUEUED_FRAMES {
  OUTFMT_FRAME_DATA_WRAP_T        *phead;
  OUTFMT_FRAME_DATA_WRAP_T        *ptail;
  unsigned int                     count;
  unsigned int                     skipped;
  unsigned int                     max;
} OUTFMT_QUEUED_FRAMES_T;

int outfmt_queueframe(OUTFMT_QUEUED_FRAMES_T *pQueued, const OUTFMT_FRAME_DATA_T *pFrame);
void outfmt_freequeuedframes(OUTFMT_QUEUED_FRAMES_T *pQueued);

#endif // __STREAM_OUTFMT_H__
