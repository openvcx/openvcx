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


#include "vsx_common.h"

#if defined(VSX_HAVE_STREAMER)

//#define DEBUG_PKTZ_H264

typedef struct H264_SLICE_INFO {
  unsigned char *p;
  unsigned int len;
} H264_SLICE_INFO_T;

typedef struct H264_PKTZ_STATE {
  H264_SLICE_INFO_T   *pSlices;
  unsigned int         numSlices;
  unsigned int         idxSlice;
  unsigned int         idxInSlice;
  unsigned char       *pFrame;
  unsigned int         lenFrame;
  int                  inSTAPA;
  uint8_t              lastNRI;
  uint8_t              lastSliceType;
  //int                  haveKeyFrame;
  unsigned char       *pPayload;
  unsigned int         mtu;
  unsigned short      *pIdxInPayload;

} H264_PKTZ_STATE_T;


static int init_h264_param(PKTZ_H264_T *pPktzH264) {


  if(pPktzH264->pSdp) {
    pPktzH264->clockRateHz = pPktzH264->pSdp->common.clockHz;
  }
  if(pPktzH264->clockRateHz == 0) {
    pPktzH264->clockRateHz = H264_DEFAULT_CLOCKHZ;
  }

  // Set the clock rate - which may not have been set if using live capture
  if(pPktzH264->pRtpMulti->init.clockRateHz == 0) {
    pPktzH264->pRtpMulti->init.clockRateHz = pPktzH264->clockRateHz;
  }


  //if(pPktzH264->favoffsetrtp > 0) {
  //  pPktzH264->tsOffsetHz = pPktzH264->favoffsetrtp * pPktzH264->clockRateHz;
  //}

  if(pPktzH264->pSdp) {
    pPktzH264->mode = pPktzH264->pSdp->u.h264.packetization_mode + 1;
  } else {
    pPktzH264->mode = PKTZ_H264_MODE_1;
  }
  //fprintf(stderr, "MODE:%d SDP:%d\n", pPktzH264->mode, pPktzH264->pSdp->u.h264.packetization_mode );

  pPktzH264->rtpTs0 = htonl(pPktzH264->pRtpMulti->pRtp->timeStamp);

  pPktzH264->isInitFromSdp = 1;

  return 0;
}

int stream_pktz_h264_init(void *pArg, const void *pInitArgs, unsigned int progIdx) {
  PKTZ_H264_T *pPktzH264 = (PKTZ_H264_T *) pArg;
  PKTZ_INIT_PARAMS_H264_T *pInitParams = (PKTZ_INIT_PARAMS_H264_T *) pInitArgs;
  int rc = 0;

  if(!pPktzH264 || !pInitParams || !pInitParams->common.pRtpMulti ||
     !pInitParams->common.cbXmitPkt) {
    return -1;
  }

  pPktzH264->pRtpMulti = pInitParams->common.pRtpMulti;
  pPktzH264->pXmitNode = pInitParams->common.pXmitNode;
  pPktzH264->cbXmitPkt = pInitParams->common.cbXmitPkt;
  pPktzH264->clockRateHz = pInitParams->common.clockRateHz;
  pPktzH264->pFrameData = pInitParams->common.pFrameData;
  //pPktzH264->favoffsetrtp = pInitParams->common.favoffsetrtp;

  pPktzH264->pSdp = pInitParams->pSdp;
  pPktzH264->isInitFromSdp = 0;
  pPktzH264->consecErrors = 0;

  if(rc < 0) {
     stream_pktz_h264_close(pPktzH264); 
  }

  return rc;
}

int stream_pktz_h264_reset(void *pArg, int fullReset, unsigned int progIdx) {
  PKTZ_H264_T *pPktzH264 = (PKTZ_H264_T *) pArg;

  //pPktzH264->pRtpMulti->pktsSent = 0;
  pPktzH264->pRtpMulti->payloadLen = 0;
  pPktzH264->consecErrors = 0;

  return 0;
}

int stream_pktz_h264_close(void *pArg) {
  PKTZ_H264_T *pPktzH264 = (PKTZ_H264_T *) pArg;

  if(pPktzH264 == NULL) {
    return -1;
  }

  return 0;
}

static int sendpkt(PKTZ_H264_T *pPktzH264, H264_PKTZ_STATE_T *pState) {
  int rc = 0;

  //fprintf(stderr, "sendpkt h264 len:%d %llu, lastSliceType: 0x%x\n", pPktzH264->pRtpMulti->payloadLen, timer_GetTime(), pState->lastSliceType);

  rc = stream_rtp_preparepkt(pPktzH264->pRtpMulti);

#if defined(DEBUG_PKTZ_H264)
  LOG(X_DEBUG("sendpkt rtp h264 rc:%d pt:%u ts:%u seq:%d len:%d marker:%d"), rc, (pPktzH264->pRtpMulti->pRtp->pt&0x7f), htonl(pPktzH264->pRtpMulti->pRtp->timeStamp), htons(pPktzH264->pRtpMulti->pRtp->sequence_num), pPktzH264->pRtpMulti->payloadLen, (pPktzH264->pRtpMulti->pRtp->pt &RTP_PT_MARKER_MASK)? 1 : 0);
  //LOGHEX_DEBUG(pPktzH264->pRtpMulti->pPayload, MAX(pPktzH264->pRtpMulti->payloadLen, 80));
#endif // DEBUG_PKTZ_H264

#if 0
  //fprintf(stderr, "payloadLen:%d\n", pPktzH264->pRtpMulti->payloadLen);
  //avc_dumpHex(stderr, pPktzH264->pRtpMulti->pPayload, 16, 1); 
  //if(pPktzH264->pRtpMulti->payloadLen>=16) avc_dumpHex(stderr, &pPktzH264->pRtpMulti->pPayload[pPktzH264->pRtpMulti->payloadLen-16], 16, 1); 
  //static unsigned char nal[4]; static int g_fdh264out; if(g_fdh264out==0) g_fdh264out=open("testoutxmit.h264", O_RDWR | O_CREAT, 0660); nal[3]=0x01; write(g_fdh264out, nal, 4); write(g_fdh264out, pPktzH264->pRtpMulti->pPayload, pPktzH264->pRtpMulti->payloadLen);
#endif // 0

  //fprintf(stderr, "timestamp:%u, seq:%u %s %s, slice %d/%d\n", htonl(pPktzH264->pRtpMulti->pRtp->timeStamp), htons(pPktzH264->pRtpMulti->pRtp->sequence_num), OUTFMT_KEYFRAME(pPktzH264->pFrameData) ? "KEYFRAME" : "", (pState->lastSliceType & NAL_TYPE_MASK) == NAL_TYPE_IDR ? "NAL-IDR" : "", pState->idxSlice, pState->idxInSlice);

  if(rc >= 0 && (rc = pPktzH264->cbXmitPkt(pPktzH264->pXmitNode) < 0)) {
    LOG(X_ERROR("H.264 packetizer failed to send output packet length %d"), stream_rtp_datalen(pPktzH264->pXmitNode->pRtpMulti));
  }

  pPktzH264->pRtpMulti->payloadLen = 0;


  //
  // Only enable the keyframe for the first packet of any keyframe.  Outbound filters such as RTSP
  // will check for keyframe presence before starting transmission.  We don't want them to pick up
  // a frame's packet mid-stream, after the initial keyframe packet.
  //
  pPktzH264->pXmitNode->cur_iskeyframe = 0;

  return rc;
}

static int findSlices(PKTZ_H264_T *pPktzH264, H264_SLICE_INFO_T pSlices[]) {
  H264_STREAM_CHUNK_T stream;
  int sc;
  unsigned int prevSliceByteIdx = H264_STARTCODE_LEN;
  unsigned int idxSlice = 0;
  int startCodeLen = 0;

  memset(&stream, 0, sizeof(stream));
  //stream.bs.emulPrevSeqLen = 3;
  //stream.bs.emulPrevSeq[2] = 0x03;
  stream.bs.buf = (unsigned char *) OUTFMT_DATA(pPktzH264->pFrameData);
  stream.bs.sz = OUTFMT_LEN(pPktzH264->pFrameData); 

  //fprintf(stderr, "NAL len:%d\n", OUTFMT_LEN(pPktzH264->pFrameData));

  if((sc = h264_findStartCode(&stream, 1, &startCodeLen)) != H264_STARTCODE_LEN) {
    LOG(X_WARNING("H.264 frame length %d does not contain valid annex B start code (%d)"),
        OUTFMT_LEN(pPktzH264->pFrameData), sc);
    //avc_dumpHex(stderr, &stream.bs.buf[stream.bs.byteIdx], pPktzH264->pFrameData->len > 64 ? 64 : pPktzH264->pFrameData->len, 1);
    pSlices[0].p = &stream.bs.buf[0]; 
    pSlices[0].len = OUTFMT_LEN(pPktzH264->pFrameData);
    if(pPktzH264->consecErrors++ > 10) {
      return -1;
    }
    return 1;
  } else if(pPktzH264->consecErrors) {
    pPktzH264->consecErrors = 0;
  }

  stream.bs.byteIdx += H264_STARTCODE_LEN;

  pSlices[0].p = &stream.bs.buf[stream.bs.byteIdx]; 

  //fprintf(stderr, "\n");avc_dumpHex(stderr, OUTFMT_DATA(pPktzH264->pFrameData), MIN(500,OUTFMT_LEN(pPktzH264->pFrameData)), 0);

  while((sc = h264_findStartCode(&stream, 1, &startCodeLen)) >= 0) {

    //fprintf(stderr, "GOT START CODE %d(0x%x)/%d, sclen:%d\n", stream.bs.byteIdx,stream.bs.byteIdx, stream.bs.sz, startCodeLen);

    stream.bs.byteIdx += sc;
    prevSliceByteIdx = stream.bs.byteIdx;
    //pSlices[idxSlice++].len = sc - H264_STARTCODE_LEN;
    pSlices[idxSlice++].len = sc - startCodeLen;
    pSlices[idxSlice].p = &stream.bs.buf[prevSliceByteIdx];

    if(idxSlice >= H264_AVC_MAX_SLICES_PER_FRAME) {
      LOG(X_WARNING("Max H.264 slices (%d) per frame reached"), H264_AVC_MAX_SLICES_PER_FRAME);
      break;
    }

  }

  pSlices[idxSlice++].len = stream.bs.sz - prevSliceByteIdx;

#if 0
  fprintf(stderr, "sz:%d\n", stream.bs.byteIdx);
  for(sc = 0; sc < idxSlice; sc++) {
    fprintf(stderr, "NAL SLICE len:%d 0x%x 0x%x ... 0x%x 0x%x\n", pSlices[sc].len, pSlices[sc].p[0], pSlices[sc].p[1], pSlices[sc].p[ pSlices[sc].len-2 ], pSlices[sc].p[ pSlices[sc].len-1 ]);
  } 
#endif // 0

  return idxSlice;
}

static int pktz_h264_slice_frag_cont(PKTZ_H264_T *pPktzH264, H264_PKTZ_STATE_T *pState) {
  unsigned int szcopy;
  int rc = 1;

#if defined(DEBUG_PKTZ_H264)
  fprintf(stderr, "NAL FRAG CONT slice[%d/%d] start %d/%d pkt:%d/%d\n", pState->idxSlice, pState->numSlices,  pState->idxInSlice, pState->pSlices[pState->idxSlice].len, *pState->pIdxInPayload, pState->mtu);
#endif // DEBUG_PKTZ_H264

  pState->pPayload[(*pState->pIdxInPayload)++] = pState->lastNRI | NAL_TYPE_FRAG_A;
  pState->pPayload[(*pState->pIdxInPayload)++] = (pState->lastSliceType & NAL_TYPE_MASK);

  if((szcopy = (pState->pSlices[pState->idxSlice].len - pState->idxInSlice)) >
     pState->mtu - *pState->pIdxInPayload) {
    szcopy = pState->mtu - *pState->pIdxInPayload;
  }

  memcpy(&pState->pPayload[*pState->pIdxInPayload],
         &pState->pSlices[pState->idxSlice].p[pState->idxInSlice], szcopy);

  if(pState->idxInSlice + szcopy >= pState->pSlices[pState->idxSlice].len) {
    pState->pPayload[(*pState->pIdxInPayload) - 1] |= 0x40; // fragment end bit
  }

  (*pState->pIdxInPayload) += szcopy;
  pState->idxInSlice += szcopy;

  return rc;
}

static int pktz_h264_slice_frag_start(PKTZ_H264_T *pPktzH264, H264_PKTZ_STATE_T *pState) {
  unsigned int szcopy;

#if defined(DEBUG_PKTZ_H264)
  fprintf(stderr, "NAL FRAG START slice[%d/%d] start %d/%d pkt:%d/%d\n", pState->idxSlice, pState->numSlices, pState->idxInSlice, pState->pSlices[pState->idxSlice].len, *pState->pIdxInPayload, pState->mtu);
#endif // DEBUG_PKTZ_H264

  pState->pPayload[(*pState->pIdxInPayload)++] = pState->lastNRI | NAL_TYPE_FRAG_A;

  if((szcopy = (pState->pSlices[pState->idxSlice].len - pState->idxInSlice)) >
     pState->mtu - *pState->pIdxInPayload) {
    szcopy = pState->mtu - *pState->pIdxInPayload;
  }

  memcpy(&pState->pPayload[*pState->pIdxInPayload],
       &pState->pSlices[pState->idxSlice].p[pState->idxInSlice], szcopy);
  pState->pPayload[*pState->pIdxInPayload] &= NAL_TYPE_MASK;
  pState->lastSliceType = pState->pPayload[*pState->pIdxInPayload];
  pState->pPayload[*pState->pIdxInPayload] |= 0x80; // fragment start bit

  (*pState->pIdxInPayload) += szcopy;
  pState->idxInSlice += szcopy;

  return 1;
}

static int pktz_h264_slice_chunk(PKTZ_H264_T *pPktzH264, H264_PKTZ_STATE_T *pState) {
  int rc = 0;
  unsigned int szcopy;
  unsigned int nextslicesz = 0;

#if defined(DEBUG_PKTZ_H264)
  fprintf(stderr, "SLICE CHUNK slice[%d/%d] stapa:%d %d/%d pkt:%d/%d\n", pState->idxSlice, pState->numSlices, pState->inSTAPA, pState->idxInSlice, pState->pSlices[pState->idxSlice].len, *pState->pIdxInPayload, pState->mtu);
#endif // DEBUG_PKTZ_H264

  if(pState->idxInSlice > 0) {

    return pktz_h264_slice_frag_cont(pPktzH264, pState);

  } else if(pState->pSlices[pState->idxSlice].len > pState->mtu - 1) {

    // STAP cannot contain Fragment NALUs 
    if(*pState->pIdxInPayload > 0) {
      if(sendpkt(pPktzH264, pState) < 0) {
        return -1;
      }
      pState->inSTAPA = 0;
    }
    return pktz_h264_slice_frag_start(pPktzH264, pState);

  }

  if(pState->idxSlice + 1 < pState->numSlices) {
    nextslicesz = pState->pSlices[pState->idxSlice + 1].len;
  }

  if((pState->inSTAPA && 
      pState->pSlices[pState->idxSlice].len + 2 + *pState->pIdxInPayload < pState->mtu) ||
      (!pState->inSTAPA && nextslicesz > 0 && 
      pState->pSlices[pState->idxSlice].len + nextslicesz + 5 + 
                                            *pState->pIdxInPayload  < pState->mtu)) {
#if defined(DEBUG_PKTZ_H264)
    fprintf(stderr, "NAL STAP-A slice[%d/%d] %d/%d nextslicesz:%d  pkt:%d/%d\n", pState->idxSlice, pState->numSlices, pState->idxInSlice, pState->pSlices[pState->idxSlice].len, nextslicesz, *pState->pIdxInPayload, pState->mtu);
#endif // DEBUG_PKTZ_H264

    //
    // Add STAP-A
    //
    if(!pState->inSTAPA) {
      pState->pPayload[(*pState->pIdxInPayload)++] = pState->lastNRI | NAL_TYPE_STAP_A;
      pState->inSTAPA = 1;
    }
    *((uint16_t *) &pState->pPayload[*pState->pIdxInPayload]) = 
                         htons(pState->pSlices[pState->idxSlice].len); 
    (*pState->pIdxInPayload) += 2;
    szcopy = pState->pSlices[pState->idxSlice].len;
    memcpy(&pState->pPayload[*pState->pIdxInPayload],
           &pState->pSlices[pState->idxSlice].p[pState->idxInSlice], szcopy);
    pState->pPayload[*pState->pIdxInPayload] &= NAL_TYPE_MASK;
    pState->pPayload[*pState->pIdxInPayload] |= pState->lastNRI;
    (*pState->pIdxInPayload) += szcopy;
    pState->idxInSlice += szcopy;

  } else if(!pState->inSTAPA) {

#if defined(DEBUG_PKTZ_H264)
    fprintf(stderr, "NAL DIRECT slice[%d/%d] %d/%d pkt:%d/%d\n", pState->idxSlice, pState->numSlices, pState->idxInSlice, pState->pSlices[pState->idxSlice].len, *pState->pIdxInPayload, pState->mtu);
#endif // DEBUG_PKTZ_H264

    //
    //
    // Add NAL directly
    //
    szcopy = pState->pSlices[pState->idxSlice].len;
    memcpy(&pState->pPayload[*pState->pIdxInPayload],
           &pState->pSlices[pState->idxSlice].p[pState->idxInSlice], szcopy);
    pState->pPayload[*pState->pIdxInPayload] &= NAL_TYPE_MASK;
    pState->pPayload[*pState->pIdxInPayload] |= pState->lastNRI;
    (*pState->pIdxInPayload) += szcopy;
    pState->idxInSlice += szcopy;
    rc = 1;

  } else {
    if(*pState->pIdxInPayload == 0) {
      LOG(X_ERROR("H.264 packetization error"));
      return -1;
    }
    rc = 1; 
  }

  return rc;
}

static int pktz_h264_slice_mode0(PKTZ_H264_T *pPktzH264, H264_PKTZ_STATE_T *pState) {
  unsigned int szcopy;

  if((szcopy = pState->pSlices[pState->idxSlice].len) > pState->mtu) {
    LOG(X_WARNING("H.264 NAL slice clipped %d / %d.  Set encoding 'vslmax=' < MTU:%d"), 
        pState->mtu, szcopy, pState->mtu);
    szcopy = pState->mtu;
  }
  memcpy(pState->pPayload, pState->pSlices[pState->idxSlice].p, szcopy);
  pState->pPayload[0] &= NAL_TYPE_MASK;
  pState->pPayload[0] |= pState->lastNRI;
  (*pState->pIdxInPayload) = szcopy;

  //fprintf(stderr, "sendpkt sz:%d 0x%x 0x%x 0x%x 0x%x ... 0x%x 0x%x\n", szcopy, pState->pPayload[0], pState->pPayload[1], pState->pPayload[2], pState->pPayload[3], pState->pPayload[szcopy-2], pState->pPayload[szcopy-1]);

  //
  // Set RTP marker bit
  //
  if(pState->idxSlice + 1 >= pState->numSlices) {
    pPktzH264->pRtpMulti->pRtp->pt |= RTP_PT_MARKER_MASK;
  }

  return sendpkt(pPktzH264, pState);

}

static int pktz_h264_slice(PKTZ_H264_T *pPktzH264, H264_PKTZ_STATE_T *pState) {
  int rc;

#if defined(DEBUG_PKTZ_H264)
  fprintf(stderr, "SLICE slice[%d/%d] stapa:%d %d/%d pkt:%d/%d\n", pState->idxSlice, pState->numSlices, pState->inSTAPA, pState->idxInSlice, pState->pSlices[pState->idxSlice].len, *pState->pIdxInPayload, pState->mtu);
#endif // DEBUG_PKTZ_H264

  //
  // Send entire slice in one packet
  //
  //if(pState->numSlices == 1 && pState->pSlices[0].len <= pState->mtu) {
  //  memcpy(pState->pPayload, pState->pSlices[0].p, pState->pSlices[0].len);
  //  (*pState->pIdxInPayload) += pState->pSlices[0].len;
  //  pState->idxInSlice += pState->pSlices[0].len;
  //  return sendpkt(pPktzH264, pState);
  //}


  while(pState->idxInSlice < pState->pSlices[pState->idxSlice].len) {

    if((rc = pktz_h264_slice_chunk(pPktzH264, pState)) < 0) {
      return -1;
    }

#if defined(DEBUG_PKTZ_H264)
      fprintf(stderr, "SLICE CHUNK done rc:%d slice[%d/%d] start %d/%d pkt:%d/%d\n", rc, pState->idxSlice, pState->numSlices, pState->idxInSlice, pState->pSlices[pState->idxSlice].len, *pState->pIdxInPayload, pState->mtu);
#endif // DEBUG_PKTZ_H264

    if(rc == 1 ||
       pState->mtu - *pState->pIdxInPayload < 5) {

      //
      // Set RTP marker bit
      //
      if(pState->idxSlice + 1 >= pState->numSlices &&
         pState->idxInSlice >= pState->pSlices[pState->idxSlice].len) {
        pPktzH264->pRtpMulti->pRtp->pt |= RTP_PT_MARKER_MASK;
      }

      if(sendpkt(pPktzH264, pState) < 0) {
        return -1;
      }
      pState->inSTAPA = 0;
    }

  }

  return 0;
}

int stream_pktz_h264_addframe(void *pArg, unsigned int progIdx) {
  PKTZ_H264_T *pPktzH264 = (PKTZ_H264_T *) pArg;
  H264_PKTZ_STATE_T state;
  H264_SLICE_INFO_T pSlices[H264_AVC_MAX_SLICES_PER_FRAME];
  int numSlices;
  int rc = 0;

  if(!pPktzH264->isInitFromSdp) {
    init_h264_param(pPktzH264);
  }

  VSX_DEBUG_RTP(
    LOG(X_DEBUG("RTP - H.264 rtp-packetizer got frame pts:%.3f (dts:%.3f), len:%u, rtp-ts:%u (%uHz) - data:"), 
           PTSF(OUTFMT_PTS(pPktzH264->pFrameData)), PTSF(OUTFMT_DTS(pPktzH264->pFrameData)), OUTFMT_LEN(pPktzH264->pFrameData), 
           htonl(pPktzH264->pRtpMulti->pRtp->timeStamp), pPktzH264->clockRateHz); 
    if(pPktzH264->pFrameData) {
       LOGHEX_DEBUG(OUTFMT_DATA(pPktzH264->pFrameData), MIN(OUTFMT_LEN(pPktzH264->pFrameData), 16));
    }
  );

#if defined(DEBUG_PKTZ_H264)
if(OUTFMT_KEYFRAME(pPktzH264->pFrameData)) fprintf(stderr, "keyframe\n");
  static float s_ptslast;
  fprintf(stderr, "pktz_h264_addframe[%d] %dHz len:%d pts:%.3f dts:%.3f (%.3f) key:%d pkt:[%d/%d]\n", progIdx, pPktzH264->clockRateHz,OUTFMT_LEN(pPktzH264->pFrameData), PTSF(OUTFMT_PTS(pPktzH264->pFrameData)), PTSF(OUTFMT_DTS(pPktzH264->pFrameData)),    PTSF(OUTFMT_PTS(pPktzH264->pFrameData) - s_ptslast), OUTFMT_KEYFRAME(pPktzH264->pFrameData),  pPktzH264->pRtpMulti->payloadLen,pPktzH264->pRtpMulti->init.maxPayloadSz);
  s_ptslast=OUTFMT_PTS(pPktzH264->pFrameData);
#endif // DEBUG_PKTZ_H264

  if((numSlices = findSlices(pPktzH264, pSlices)) <= 0) {
    return -1;
  }

  //fprintf(stderr, "pts:%.3f, FRAMEDATA->keyframe: %d, IDR:%d\n", PTSF(OUTFMT_PTS(pPktzH264->pFrameData)), OUTFMT_KEYFRAME(pPktzH264->pFrameData), (state.lastSliceType & NAL_TYPE_MASK) == NAL_TYPE_IDR);


  if(OUTFMT_KEYFRAME(pPktzH264->pFrameData)) {
    pPktzH264->pXmitNode->cur_iskeyframe = 1;
  } else {
    pPktzH264->pXmitNode->cur_iskeyframe = 0;
  }


  memset(&state, 0, sizeof(state));
  state.pSlices = pSlices;
  state.numSlices = (unsigned int) numSlices;
  state.pFrame = (unsigned char *) OUTFMT_DATA(pPktzH264->pFrameData);
  state.lenFrame = OUTFMT_LEN(pPktzH264->pFrameData);
  state.pPayload = pPktzH264->pRtpMulti->pPayload;
  state.pIdxInPayload = &pPktzH264->pRtpMulti->payloadLen;
  state.mtu = pPktzH264->pRtpMulti->init.maxPayloadSz;

  pPktzH264->pRtpMulti->pRtp->pt &= ~RTP_PT_MARKER_MASK;
  pPktzH264->pRtpMulti->payloadLen = 0;
  //fprintf(stderr, "H264 TS OFFSET %d\n", pPktzH264->tsOffsetHz);

  //
  // Seems like QuickTime RTSP player is the only one that needs the DTS to be reflected in the RTP TS for B frames
  //
  pPktzH264->pRtpMulti->pRtp->timeStamp =
    htonl(pPktzH264->rtpTs0 + (pPktzH264->clockRateHz * PTSF(OUTFMT_PTS(pPktzH264->pFrameData))));
    //htonl(pPktzH264->rtpTs0 + (pPktzH264->clockRateHz * (PTSF(OUTFMT_PTS(pPktzH264->pFrameData)) + PTSF(OUTFMT_DTS(pPktzH264->pFrameData)))     ));

  //fprintf(stderr, "pktz_h264 slices:%d\n", state.numSlices);
  for(state.idxSlice = 0; state.idxSlice < state.numSlices; state.idxSlice++) {
    //fprintf(stderr, "pktz_h264 slice len:%d\n", state.pSlices[state.idxSlice].len);
    state.lastNRI = state.pSlices[state.idxSlice].p[0] & ~NAL_TYPE_MASK;
    state.idxInSlice = 0;

//if(state.lastNRI == 0) state.lastNRI = (1 << 5);
//fprintf(stderr, "NRI: 0x%x, NAL: 0x%x\n", state.lastNRI >> 5, state.pSlices[state.idxSlice].p[0] &NAL_TYPE_MASK);

    if(pPktzH264->mode == PKTZ_H264_MODE_0) {
      pktz_h264_slice_mode0(pPktzH264, &state);
    } else {
      pktz_h264_slice(pPktzH264, &state);
    }
  }

  if(pPktzH264->pRtpMulti->payloadLen > 0) {
    //
    // Set RTP marker bit
    //
    pPktzH264->pRtpMulti->pRtp->pt |= RTP_PT_MARKER_MASK;

    if(sendpkt(pPktzH264, &state) < 0) {
      return -1;
    }
  }

  //fprintf(stderr, "stream_pktz_h264 rc:%d\n", rc);
  return rc;
}

#endif // VSX_HAVE_STREAMER
