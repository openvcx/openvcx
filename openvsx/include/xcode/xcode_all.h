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


#ifndef __XCODE_ALL_H__
#define __XCODE_ALL_H__

#include "xcode_types.h"
#include "stream/stream_net_pes.h"

int xcodectxt_init(STREAM_XCODE_CTXT_T *pXcodeCtxt, float vidFpsOut, int aud, int allowUpsample,
                   unsigned int fpsInNHint, unsigned int fpsInDHint);
int xcodectxt_allocbuf(STREAM_XCODE_CTXT_T *pXcodeCtxt, unsigned int vidSz, const PKTQUEUE_CONFIG_T *pPipFrQCfg);
int xcodectxt_close(STREAM_XCODE_CTXT_T *pXcodeCtxt);

enum STREAM_NET_ADVFR_RC xcode_aud(STREAM_XCODE_DATA_T *pData);
enum STREAM_NET_ADVFR_RC xcode_vid(STREAM_XCODE_DATA_T *pData);
enum STREAM_NET_ADVFR_RC xcode_vid_update(STREAM_XCODE_DATA_T *pData, 
                                          const IXCODE_VIDEO_CTXT_T *pUpdateCfg);
enum STREAM_NET_ADVFR_RC xcode_aud_update(STREAM_XCODE_DATA_T *pData, 
                                          const IXCODE_AUDIO_CTXT_T *pUpdateCfg);
int xcode_update(STREAMER_CFG_T *pStreamerCfg, const char *strxcode,
                 enum STREAM_XCODE_FLAGS flags);

int xcode_resetdata(STREAM_XCODE_DATA_T *pXcode); 
int xcode_checkresetresume(STREAM_XCODE_DATA_T *pXcode);
uint64_t xcode_getFrameTm(const STREAM_XCODE_DATA_T *pData, int absolute, int64_t tm);
int xcode_setVidSeqStart(IXCODE_VIDEO_CTXT_T  *pXcode,
                           STREAM_XCODE_DATA_T *pData);
int xcode_h264_decodenalhdr(XCODE_H264_OUT_T *pXh264,
                                 const unsigned char *pNal,
                                 unsigned int len);
int xcode_h264_skipannexbhdr(BIT_STREAM_T *bs);
int xcode_h264_find_spspps(XCODE_H264_OUT_T *pH264out, const unsigned char *pData,
                           unsigned int len, SPSPPS_RAW_T *pSpspps);
int xcode_vid_h264_check(STREAM_XCODE_DATA_T *pData, unsigned int outidx);
int xcode_vid_mpg4v_check(STREAM_XCODE_DATA_T *pData, unsigned int outidx);
int xcode_mpg4v_find_seqhdrs(const unsigned char *pData, unsigned int len, MPG4V_SEQ_HDRS_T *pHdrs);
enum STREAM_NET_ADVFR_RC xcode_vid_seqhdr_h264(STREAM_XCODE_DATA_T *pData,
                           IXCODE_VIDEO_CTXT_T  *pXcode, int *pInputResChange);
int xcode_getvidheader(STREAM_XCODE_DATA_T *pData, unsigned int outidx, 
                       unsigned char *pHdr, unsigned int *plen);
int xcode_set_crop_pad(IXCODE_VIDEO_CROP_T *pCrop, const IXCODE_VIDEO_OUT_T *pXcodeOut,
                       int padAspectR, unsigned int outWidth, unsigned int outHeight,
                       unsigned int origWidth, unsigned int origHeight);



enum XCODE_CFG_ENABLED {
  XCODE_CFG_IPC_NOTRUNNING    = -1,
  XCODE_CFG_NOTCOMPILED       = 0,
  XCODE_CFG_OK                = 1,
  XCODE_CFG_COMPILED          = 2,
  XCODE_CFG_IPC_RUNNING       = 3
};
enum XCODE_CFG_ENABLED xcode_enabled(int printerror);


#endif // __XCODE_ALLH__
