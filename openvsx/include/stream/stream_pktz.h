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


#ifndef __STREAM_PKTZ_H__
#define __STREAM_PKTZ_H__

#define PKTZ_DO_MP2TS        0x0001
#define PKTZ_DO_FLV          0x0002
#define PKTZ_DO_H264         0x0004
#define PKTZ_DO_MPEG4V       0x0008
#define PKTZ_DO_VP8          0x0010
#define PKTZ_DO_H263         0x0020
#define PKTZ_DO_AAC          0x0040
#define PKTZ_DO_AMR          0x0080
#define PKTZ_DO_SILK         0x0100
#define PKTZ_DO_OPUS         0x0200
#define PKTZ_DO_VORBIS       0x0400
#define PKTZ_DO_G711_MULAW   0x0800
#define PKTZ_DO_G711_ALAW    0x1000
#define PKTZ_DO_G711         (PKTZ_DO_G711_MULAW | PKTZ_DO_G711_ALAW) 
#define PKTZ_DO_FRBUFV       0x2000
#define PKTZ_DO_FRBUFA       0x4000

#define PKTZ_MASK_VID      (PKTZ_DO_H264 | PKTZ_DO_MPEG4V | PKTZ_DO_H263 | PKTZ_DO_VP8)
#define PKTZ_MASK_AUD      (PKTZ_DO_AAC | PKTZ_DO_AMR | PKTZ_DO_SILK | PKTZ_DO_OPUS | PKTZ_DO_VORBIS | PKTZ_DO_G711)


#define MAX_PACKETIZERS                  3

typedef int (* PKTZ_INIT) (void *, const void *, unsigned int);
typedef int (* PKTZ_RESET) (void *, int, unsigned int);
typedef int (* PKTZ_CLOSE) (void *);
typedef int (* PKTZ_ONFRAME) (void *, unsigned int);
typedef int (* PKTZ_XMITPKT) (STREAM_XMIT_NODE_T *);


typedef struct PKTZ_INIT_PARAMS {

  STREAM_XMIT_NODE_T           *pXmitNode;
  STREAM_RTP_MULTI_T           *pRtpMulti; 
  int                           xmitflushpkts;
  PKTZ_XMITPKT                  cbXmitPkt;
  unsigned int                  clockRateHz;
  float                         favoffsetrtp;

  // The following are auto-init directly from the values of STREAM_AV 
  int64_t                      *pavoffset_pktz; 
  uint16_t                     *pStreamId; 
  uint16_t                     *pStreamType;
  int                          *pPesLenZero;
  int                          *pHaveDts;
  const OUTFMT_FRAME_DATA_T    *pFrameData;
  uint16_t                     *pinactive;
  VIDEO_DESCRIPTION_GENERIC_T  *pvidProp;

} PKTZ_INIT_PARAMS_T;


typedef struct PKTZ_CBSET {
  int                     isactive;
  int                     pktz_do_mask;
  unsigned int            outidx;
  STREAM_RTP_MULTI_T     *pRtpMulti;
  void                   *pCbData;
  void                   *pCbInitData;
  PKTZ_INIT               cbInit;
  PKTZ_RESET              cbReset;
  PKTZ_CLOSE              cbClose;
  PKTZ_ONFRAME            cbNewFrame;
} PKTZ_CBSET_T; 

#endif // __STREAM_PKTZ_H__

