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


#ifndef __SRV_FLV_H__
#define __SRV_FLV_H__

#define FLVLIVE_DELAY_DEFAULT    1.0f

enum FLVSRV_STATE {
  FLVSRV_STATE_ERROR               = -1,
  FLVSRV_STATE_INVALID             = 0,

};

typedef int (* FLVSRV_CB_WRITEDATA) (void *, const unsigned char *, unsigned int);


typedef struct FLVSRV_CTXT_T {
  SOCKET_DESCR_T         *pSd;
  HTTP_REQ_T             *pReq;
  int                     senthdr;

  FILE_STREAM_T           recordFile;
  int                     overwriteFile;
  //unsigned int            recordMaxFileSz;
  OUTFMT_CFG_T           *pRecordOutFmt;

  FILE_OFFSET_T           totXmit;
  FLVSRV_CB_WRITEDATA     cbWrite;
  char                   *writeErrDescr;

  FLV_BYTE_STREAM_T        out;
  enum FLVSRV_STATE        state;
  CODEC_AV_CTXT_T          av;
  float                    avBufferDelaySec;
  int                     *pnovid;
  int                     *pnoaud;

  unsigned int             requestOutIdx; // xcode outidx

  OUTFMT_QUEUED_FRAMES_T   queuedFrames;

} FLVSRV_CTXT_T;

int flvsrv_init(FLVSRV_CTXT_T *pFlv, unsigned int vidTmpFrameSz);
int flvsrv_sendHttpResp(FLVSRV_CTXT_T *pFlv);
void flvsrv_close(FLVSRV_CTXT_T *pFlv);
int flvsrv_addFrame(void *, const OUTFMT_FRAME_DATA_T *pFrame);

int flvsrv_record_start(FLVSRV_CTXT_T *pFlvCtxt, STREAMER_OUTFMT_T *pLiveFmt);
int flvsrv_record_stop(FLVSRV_CTXT_T *pFlvCtxt);

int flvsrv_create_audstart(BYTE_STREAM_T *bs, CODEC_AUD_CTXT_T *pFlvA);
int flvsrv_create_vidstart(BYTE_STREAM_T *bs, CODEC_VID_CTXT_T *pFlvV);
int flvsrv_create_vidframe_h264(BYTE_STREAM_T *bs, CODEC_VID_CTXT_T *pFlvV, 
                                const OUTFMT_FRAME_DATA_T *pFrame, 
                                uint32_t *pmspts, int32_t *pmsdts, int keyframe);
int flvsrv_create_audframe_aac(BYTE_STREAM_T *bs, CODEC_AUD_CTXT_T *pFlvA, const OUTFMT_FRAME_DATA_T *pFrame,
                                uint32_t *pmspts);

int flvsrv_check_vidseqhdr(CODEC_VID_CTXT_T *pVidCtxt, const OUTFMT_FRAME_DATA_T *pFrame,
                           int *pkeyframe);
int flvsrv_check_audseqhdr(CODEC_AUD_CTXT_T *pAudCtxt, const OUTFMT_FRAME_DATA_T *pFrame);
                           


#endif // __SRV_FLV_H__
