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

#define TS_OFFSET_PKTS             7000
#define TS_OFFSET_PKTS_XCODE       (2 * (TS_OFFSET_PKTS))

//int g_h264_packetization_mode_min1 = 0;

typedef struct CODEC_PAIR {

  union {
    H264_AVC_DESCR_T           h264;
    MPG4V_DESCR_T              mpg4v;
    VP8_DESCR_T                vp8;
    IMAGE_GENERIC_DESCR_T      image;
    STREAM_VCONFERENCE_DESCR_T conferenceV;
  } v;
  H264_AVC_DESCR_T           *pH264;
  MPG4V_DESCR_T              *pMpg4v;
  VP8_DESCR_T                *pVp8;
  IMAGE_GENERIC_DESCR_T      *pImage;
  STREAM_VCONFERENCE_DESCR_T *pConferenceV;

  PKTZ_FRBUF_T            pktzFrbufV;
  PKTZ_MP2TS_T            pktzsMp2ts[IXCODE_VIDEO_OUT_MAX];
  union {
    PKTZ_H264_T           pktzsH264[IXCODE_VIDEO_OUT_MAX];
    PKTZ_MPG4V_T          pktzsMpg4v[IXCODE_VIDEO_OUT_MAX];
    PKTZ_H263_T           pktzsH263[IXCODE_VIDEO_OUT_MAX];
    PKTZ_VP8_T            pktzsVp8[IXCODE_VIDEO_OUT_MAX];
  } vp;
  union {
    STREAM_AV_T           streamAv;
  } vs;
  STREAM_AV_T            *pStreamAv;
  PKTZ_MP2TS_T           *pPktzsMp2ts[IXCODE_VIDEO_OUT_MAX];
  PKTZ_H264_T            *pPktzsH264[IXCODE_VIDEO_OUT_MAX];
  PKTZ_MPG4V_T           *pPktzsMpg4v[IXCODE_VIDEO_OUT_MAX];
  PKTZ_H263_T            *pPktzsH263[IXCODE_VIDEO_OUT_MAX];
  PKTZ_VP8_T             *pPktzsVp8[IXCODE_VIDEO_OUT_MAX];
  PKTZ_FRBUF_T           *pPktzFrbufV;

  union {
    STREAM_H264_T         netH264; 
    STREAM_MPG4V_T        netMpg4v;
    STREAM_VP8_T          netVp8;
    STREAM_IMAGE_T        netImage;
    STREAM_VCONFERENCE_T  netConferenceV;
  } vn;
  STREAM_H264_T          *pNetH264; 
  STREAM_MPG4V_T         *pNetMpg4v;
  STREAM_VP8_T           *pNetVp8;
  STREAM_IMAGE_T         *pNetImage;
  STREAM_VCONFERENCE_T   *pNetConferenceV;

  union {
    AMR_DESCR_T                amr;
    VORBIS_DESCR_T             vorbis;
    STREAM_ACONFERENCE_DESCR_T conferenceA;
  } a;
  AAC_DESCR_T                *pAac;
  AMR_DESCR_T                *pAmr;
  void                       *pSilk;
  void                       *pOpus;
  VORBIS_DESCR_T             *pVorbis;
  STREAM_ACONFERENCE_DESCR_T *pConferenceA;

  PKTZ_FRBUF_T            pktzFrbufA;
  union {
    PKTZ_AAC_T            pktzsAac[IXCODE_VIDEO_OUT_MAX];
    PKTZ_AMR_T            pktzsAmr[IXCODE_VIDEO_OUT_MAX];
    PKTZ_SILK_T           pktzsSilk[IXCODE_VIDEO_OUT_MAX];
    PKTZ_OPUS_T           pktzsOpus[IXCODE_VIDEO_OUT_MAX];
    PKTZ_PCM_T            pktzsPcm[IXCODE_VIDEO_OUT_MAX];
  } ap;
  PKTZ_AAC_T             *pPktzsAac[IXCODE_VIDEO_OUT_MAX];
  PKTZ_AMR_T             *pPktzsAmr[IXCODE_VIDEO_OUT_MAX];
  PKTZ_SILK_T            *pPktzsSilk[IXCODE_VIDEO_OUT_MAX];
  PKTZ_OPUS_T            *pPktzsOpus[IXCODE_VIDEO_OUT_MAX];
  PKTZ_PCM_T             *pPktzsPcm[IXCODE_VIDEO_OUT_MAX];
  PKTZ_FRBUF_T           *pPktzFrbufA;

  union {
    STREAM_AAC_T          netAac;
    STREAM_AMR_T          netAmr;
    STREAM_VORBIS_T       netVorbis;
    STREAM_ACONFERENCE_T  netConferenceA;
  } an;
  STREAM_AAC_T           *pNetAac;
  STREAM_AMR_T           *pNetAmr;
  STREAM_VORBIS_T        *pNetVorbis;
  STREAM_ACONFERENCE_T   *pNetConferenceA;

  int                     isInM2ts;
  XC_CODEC_TYPE_T         vidType;
  XC_CODEC_TYPE_T         audType;
} CODEC_PAIR_T;

static void codecs_close(CODEC_PAIR_T *pCodecs) {

  unsigned int outidx = 0;

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(pCodecs->pPktzsMp2ts[outidx]) {
      stream_pktz_mp2ts_close(pCodecs->pPktzsMp2ts[outidx]);
      pCodecs->pPktzsMp2ts[outidx] = NULL;
    }

    if(pCodecs->pPktzsH264[outidx]) {
      stream_pktz_h264_close(pCodecs->pPktzsH264[outidx]);
      pCodecs->pPktzsH264[outidx] = NULL;
    } else if(pCodecs->pPktzsMpg4v[outidx]) {
      stream_pktz_mpg4v_close(pCodecs->pPktzsMpg4v[outidx]);
      pCodecs->pPktzsMpg4v[outidx] = NULL;
    } else if(pCodecs->pPktzsH263[outidx]) {
      stream_pktz_h263_close(pCodecs->pPktzsH263[outidx]);
      pCodecs->pPktzsH263[outidx] = NULL;
    }

    if(pCodecs->pPktzsAac[outidx]) {
      stream_pktz_aac_close(pCodecs->pPktzsAac[outidx]);
      pCodecs->pPktzsAac[outidx] = NULL;
    } else if(pCodecs->pPktzsAmr[outidx]) {
      stream_pktz_amr_close(pCodecs->pPktzsAmr[outidx]);
      pCodecs->pPktzsAmr[outidx] = NULL;
    } else if(pCodecs->pPktzsSilk[outidx]) {
      stream_pktz_silk_close(pCodecs->pPktzsSilk[outidx]);
      pCodecs->pPktzsSilk[outidx] = NULL;
    } else if(pCodecs->pPktzsOpus[outidx]) {
      stream_pktz_opus_close(pCodecs->pPktzsOpus[outidx]);
      pCodecs->pPktzsOpus[outidx] = NULL;
    } else if(pCodecs->pPktzsPcm[outidx]) {
      stream_pktz_pcm_close(pCodecs->pPktzsPcm[outidx]);
      pCodecs->pPktzsPcm[outidx] = NULL;
    }

  }

  if(pCodecs->pPktzFrbufV) {
    stream_pktz_frbuf_close(pCodecs->pPktzFrbufV);
    pCodecs->pPktzFrbufV = NULL;
  }

  if(pCodecs->pPktzFrbufA) {
    stream_pktz_frbuf_close(pCodecs->pPktzFrbufA);
    pCodecs->pPktzFrbufA = NULL;
  }
  
  if(pCodecs->pH264) {
    h264_free(pCodecs->pH264);
    pCodecs->pH264 = NULL;
  } else if(pCodecs->pMpg4v) {
    mpg4v_free(pCodecs->pMpg4v);
    pCodecs->pMpg4v = NULL;
  } else if(pCodecs->pVp8) {
    vp8_free(pCodecs->pVp8);
    pCodecs->pVp8 = NULL;
  } else if(pCodecs->pImage) {
    image_close(pCodecs->pImage);
    pCodecs->pImage = NULL;
  }

  if(pCodecs->pAac) {
    aac_close(pCodecs->pAac);
    pCodecs->pAac = NULL;
  } else if(pCodecs->pAmr) {
    amr_free(pCodecs->pAmr);
    pCodecs->pAmr = NULL;
  } else if(pCodecs->pSilk) {
    //silk_free(pCodecs->pSilk);
    pCodecs->pSilk= NULL;
  } else if(pCodecs->pOpus) {
    //opus_free(pCodecs->pOpus);
    pCodecs->pOpus= NULL;
  } else if(pCodecs->pVorbis) {
    vorbis_free(pCodecs->pVorbis);
    pCodecs->pVorbis = NULL;
  }

}


static int stream_av_xcode(CODEC_PAIR_T *pCodecs,
                           STREAMER_CFG_T *pCfg,
                           STREAM_XCODE_DATA_T **ppXcodeDataVid,
                           STREAM_XCODE_DATA_T **ppXcodeDataAud,
                           int **ppHaveSeqStartVid,
                           int **ppHaveSeqStartAud,
                           XC_CODEC_TYPE_T vidCodecType,
                           XC_CODEC_TYPE_T audCodecType);



static void stream_setstate(STREAMER_CFG_T *pCfg, STREAMER_STATE_T runningState, int lock) {

  //LOG(X_DEBUG("\n\nstreamer_state running:%d -> running:%d, tid:0x%x"), pCfg->running, runningState, pthread_self());

  if(lock) {
    pthread_mutex_lock(&pCfg->mtxStrmr);
  }

  pCfg->running = runningState;

  if(lock) {
    pthread_mutex_unlock(&pCfg->mtxStrmr);
  }
}

static int initTransportParams(STREAM_RTP_INIT_T *pInit,
                               unsigned char payloadType,
                               uint32_t ssrc,
                               uint16_t rtpseqStart,
                               RTP_TIMESTAMP_INIT_T *pTimeStampStart,
                               unsigned int mtu,
                               float frtcp_sr_intervalsec,
                               int rtcplistener,
                               int fir_recv_via_rtcp,
                               unsigned int clockRateHz,
                               unsigned int frameDeltaHz,
                               int rawxmit,
                               uint8_t dscp) {
                            
  pInit->clockRateHz = clockRateHz;
  pInit->frameDeltaHz = frameDeltaHz;
  pInit->pt = payloadType;
  pInit->seqStart = rtpseqStart;
  memcpy(&pInit->timeStampStart, pTimeStampStart, sizeof(pInit->timeStampStart));
  pInit->ssrc = ssrc;
  pInit->maxPayloadSz = mtu;
  pInit->fir_recv_via_rtcp = fir_recv_via_rtcp;

  //fprintf(stderr, "SSRC: 0x%x, fir_recv_via_rtcp:%d\n", ssrc, fir_recv_via_rtcp);

  if(frtcp_sr_intervalsec > 0) {
    pInit->sendrtcpsr = 1;
  } else {
    pInit->sendrtcpsr = 0;
  }

  pInit->rtcplistener = rtcplistener;

  pInit->dscpRtp = dscp;
  pInit->dscpRtcp = dscp;

  memset(&pInit->raw, 0, sizeof(pInit->raw));
  pInit->raw.haveRaw = rawxmit;

  return 0;
}

static void init_readframes_av(STREAM_XMIT_NODE_T *pNode, STREAM_AV_T *pAv) {
  pNode->pCbData = pAv;
  pNode->cbPreparePkt = stream_av_preparepkt;
  pNode->cbCanSend = stream_av_cansend;
  pNode->pNext = NULL;

}

#define PDEST_TRANS_DTLS(pdest) (STREAM_RTP_DTLS_CTXT((pdest)->pRtpMulti)->active && !STREAM_RTP_DTLS_CTXT((pdest)->pRtpMulti)->dtls_srtp)
#define PDEST_TRANS_DTLSSRTP(pdest) (STREAM_RTP_DTLS_CTXT((pdest)->pRtpMulti)->active && STREAM_RTP_DTLS_CTXT((pdest)->pRtpMulti)->dtls_srtp)
#define PDEST_TRANS_SDES(pdest) ((pdest)->srtps[0].kt.k.lenKey > 0 ? 1 : 0)
#define PDEST_TRANS(pdest) (PDEST_TRANS_DTLSSRTP(pdest) ? "DTLS-SRTP" : PDEST_TRANS_DTLS(pdest) ? "DTLS" : PDEST_TRANS_SDES(pdest) ? "SRTP" : "RTP")
#define RTP_TRANS_DESCR2(pXmitAction, pdest) ((pXmitAction)->do_output_rtphdr ? ((pdest) ? PDEST_TRANS(pdest) : "RTP") : "UDP")
#define RTP_TRANS_DESCR3(pXmitAction) ((pXmitAction)->do_output_rtphdr ? "RTP" : "UDP")


static void init_xmit_mp2tsraw(STREAM_XMIT_NODE_T *pNode,
                               STREAM_RTP_MP2TS_REPLAY_T *pRtpMp2ts) {

  pNode->rawSend = pRtpMp2ts->rtpMulti.init.raw.haveRaw;
  pNode->pCbData = pRtpMp2ts;
  pNode->pRtpMulti = &pRtpMp2ts->rtpMulti;
  pNode->cbPreparePkt = stream_rtp_mp2tsraw_preparepkt;
  pNode->cbCanSend = stream_rtp_mp2tsraw_cansend;
  pNode->pNext = NULL;
  snprintf(pNode->descr, sizeof(pNode->descr), "%s MPEG-2 TS (ISO/IEC 13818-1) replay",
          RTP_TRANS_DESCR3(pNode->pXmitAction));
}

//
// Callback after transcoding to update output SDP info 
//
int cbSdpUpdated(void *pArg) {
  int rc = 0;
  unsigned int outidx = 0;
  STREAM_SDP_ACTION_T *pSdpAction = (STREAM_SDP_ACTION_T *) pArg;
  STREAMER_CFG_T *pCfg = (STREAMER_CFG_T *) pSdpAction->pCfg;

  if(!pSdpAction || !pCfg) {
    return -1;
  }

  if(pSdpAction->outidx < IXCODE_VIDEO_OUT_MAX) {
    outidx = pSdpAction->outidx;
  }

  if(pCfg->verbosity > VSX_VERBOSITY_NORMAL) {
    sdp_write(&pCfg->sdpsx[1][outidx], NULL, S_DEBUG);
  }

  if(pSdpAction->pPathOutFile) {
    rc = sdp_write_path(&pCfg->sdpsx[1][outidx], pSdpAction->pPathOutFile);
  }

  pSdpAction->calledCb = 1;

  return rc;
}

static int streamer_add_recipient(STREAMER_CFG_T *pCfg, 
                                  STREAM_RTP_MULTI_T *pRtpMultis, 
                                  unsigned int idxDest,
                                  unsigned int numPorts,
                                  int startingmediaport,
                                  int useSockFromCapture,
                                  int doAbrAuto) {
  int rc = 0;
  unsigned int idxPort;
  STREAM_DEST_CFG_T destCfg;
  STREAM_RTP_DEST_T *pDest, *pDestPrior = NULL;
  int doAbrAutoVid = doAbrAuto;

  memset(&destCfg, 0, sizeof(destCfg));

  for(idxPort = 0; idxPort < numPorts; idxPort++) {

    //
    // Use the capture_socket created socket(s) for RTP output to preserve
    // the source bind local port(s).  We can't just bind the output socket
    // to the desired local port because that will inhibit UDP socket capture
    // reception.
    //
    if((destCfg.useSockFromCapture = useSockFromCapture)) {
      destCfg.psharedCtxt = &pCfg->sharedCtxt.av[idxPort];
    }

    destCfg.haveDstAddr = 0;
    destCfg.u.pDstHost = pCfg->pdestsCfg[idxDest].dstHost;
    destCfg.dstPort = pCfg->pdestsCfg[idxDest].ports[idxPort];
    memcpy(&destCfg.srtp, &pCfg->pdestsCfg[idxDest].srtps[idxPort], sizeof(destCfg.srtp));
    memcpy(&destCfg.dtlsCfg, &pCfg->pdestsCfg[idxDest].dtlsCfg, sizeof(destCfg.dtlsCfg));
    memcpy(&destCfg.stunCfg, &pCfg->pdestsCfg[idxDest].stunCfgs[idxPort], sizeof(destCfg.stunCfg));
    memcpy(&destCfg.turnCfg, &pCfg->pdestsCfg[idxDest].turnCfgs[idxPort], sizeof(destCfg.turnCfg));
    if((destCfg.dstPortRtcp = pCfg->pdestsCfg[idxDest].portsRtcp[idxPort]) == 0) {
      destCfg.dstPortRtcp = RTCP_PORT_FROM_RTP(destCfg.dstPort);
    }

    if(pCfg->pMonitor && pCfg->pMonitor->active) {
      destCfg.pMonitor = pCfg->pMonitor;
      if(doAbrAuto && !pRtpMultis[idxPort].isaud) {
        destCfg.doAbrAuto = doAbrAutoVid;
      }
    }
    destCfg.pFbReq = &pCfg->fbReq;
    destCfg.xcodeOutidx = idxDest;
    destCfg.pDestPeer = pDestPrior;

    //TODO: check if we're doing UDP output.. and disable any rtcp stuff

    if(pRtpMultis[idxPort].init.rtcplistener) {
      destCfg.localPort = (uint16_t) (startingmediaport + (2 * idxPort + (4 * idxDest)));
    } else {
      destCfg.localPort = 0;
    }

    //fprintf(stderr, "ADDDEST[idxDest:%d, idxPort:%d/%d] %s:%d (rtcp:%d) localport:%d idxPort:%d, rtcplistener:%d startp:%d\n", idxDest, idxPort, numPorts, destCfg.u.pDstHost, destCfg.dstPort, destCfg.dstPortRtcp, destCfg.localPort, idxPort, pRtpMultis[idxPort].init.rtcplistener, startingmediaport);

    if(!(pDest = stream_rtp_adddest(&pRtpMultis[idxPort], &destCfg))) {
      rc = -1;
      break;
    } else if(pCfg->pMonitor && pCfg->pMonitor->active && pDest->pstreamStats && !pCfg->action.do_output_rtphdr) {
      pDest->pstreamStats->method = STREAM_METHOD_UDP; 
    }

    if(pDestPrior && idxPort > 0 && pCfg->pdestsCfg[idxDest].ports[idxPort - 1] == destCfg.dstPort) {
      pDestPrior->pDestPeer = pDest;
      pDest->pDestPeer = pDestPrior;
    }

    pDestPrior = pDest;

    doAbrAutoVid = 0;

  }

  return rc;
}

static int streamer_add_recipients(STREAMER_CFG_T *pCfg,
                                   STREAM_RTP_MULTI_T *pRtpMultis, 
                                   unsigned int numPorts) {

  int rc = 0;
  unsigned int outidx;
  unsigned int idxDest;
  unsigned int iterOut = 0;
  int multixcodeout = 0;
  int startingmediaport;
  STREAM_RTP_MULTI_T *pRtpMultisTmp = pRtpMultis;

  if((startingmediaport = net_getlocalmediaport(numPorts)) < 0) {
    LOG(X_ERROR("Unable to obtain local UDP media port"));
    return -1;
  }

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(pCfg->xcode.vid.out[outidx].active) {
      multixcodeout++;
    }
  }

  if(multixcodeout > 1 && pCfg->numDests > 1) {

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
        continue;
      }
      //fprintf(stderr, "ADD R[%d] p:0x%x (%d) numports:%d ports[0]:%d,%d ports[1]:%d,%d\n", outidx, p, sizeof(STREAM_RTP_MULTI_T), numPorts, pCfg->pdestsCfg[0].ports[0], pCfg->pdestsCfg[0].ports[1], pCfg->pdestsCfg[1].ports[0], pCfg->pdestsCfg[1].ports[1]);

      if((rc = streamer_add_recipient(pCfg, pRtpMultis, outidx, numPorts, startingmediaport,
                                      (pCfg->cfgrtp.rtp_useCaptureSrcPort > 0 && iterOut == 0 ? 1 : 0), 0)) < 0) {

        break;
      }

      pRtpMultisTmp += numPorts;
      iterOut++;
     
    }

  } else {
    for(idxDest = 0; idxDest < pCfg->numDests; idxDest++) {

      if((rc = streamer_add_recipient(pCfg, &pRtpMultis[0], idxDest, numPorts, startingmediaport,
                                      (pCfg->cfgrtp.rtp_useCaptureSrcPort > 0 && iterOut == 0 ? 1 : 0),
                                      idxDest == 0 ? pCfg->doAbrAuto : 0)) < 0) {
        break;
      }
      iterOut++;
    }
  }


  return rc;
}

static int rtpmultiset_init(STREAM_RTP_MULTI_T *pRtpMultis, unsigned int num, 
                            float favoffsetrtcp, unsigned int maxRtp) {
  unsigned int idxMulti;
  unsigned int idx;

  for(idxMulti = 0; idxMulti < num; idxMulti++) {
    memset(&pRtpMultis[idxMulti], 0, sizeof(STREAM_RTP_MULTI_T));

    for(idx = 0; idx < maxRtp; idx++) {
      
      if(idx == 0) {
        if(!(pRtpMultis[idxMulti].pdests = (STREAM_RTP_DEST_T *) avc_calloc(maxRtp, sizeof(STREAM_RTP_DEST_T)))) {
          return -1;
        }
        //fprintf(stderr, "ALLOC RTP MULTIS %d, idxMulti:%d / num:%d, 0x%x, 0x%x, 0x%x, 0x%x\n", maxRtp, idxMulti, num, &pRtpMultis[idxMulti].pdests[0], &pRtpMultis[idxMulti].pdests[1], &pRtpMultis[idxMulti].pdests[2], &pRtpMultis[idxMulti].pdests[3]);
      }

      *((unsigned int *) &pRtpMultis[idxMulti].maxDests) = maxRtp;

      NETIOSOCK_FD(pRtpMultis[idxMulti].pdests[idx].xmit.netsock) = INVALID_SOCKET;
      NETIOSOCK_FD(pRtpMultis[idxMulti].pdests[idx].xmit.netsockRtcp) = INVALID_SOCKET;
      pRtpMultis[idxMulti].pdests[idx].xmit.pnetsock = NULL;
      pRtpMultis[idxMulti].pdests[idx].xmit.pnetsockRtcp = NULL;

      if(favoffsetrtcp > 0 && idxMulti == 1) {
        // positive audio to video offset should delay audio timestamps
        if(idx == 0) {
          LOG(X_DEBUG("Setting RTCP audio offset forward %.3f sec"), favoffsetrtcp);
        }
        pRtpMultis[idxMulti].pdests[idx].rtcp.ftsoffsetdelay = favoffsetrtcp;

      } else if(favoffsetrtcp < 0 && idxMulti == 0) {
        // negative audio to video offset should video timestamps
        if(idx == 0) {
          LOG(X_DEBUG("Setting RTCP video offset forward %.3f sec"), -1 * favoffsetrtcp);
        }
        pRtpMultis[idxMulti].pdests[idx].rtcp.ftsoffsetdelay = -1 * favoffsetrtcp;
      }
    }
  }

  return 0;
}

static int stream_init_params(STREAMER_CFG_T *pCfg,
                             STREAM_RTP_MULTI_T *pRtpMulti,
                             XC_CODEC_TYPE_T codecType,
                             unsigned int clockRateHz,
                             unsigned int frameDeltaHz,
                             float frtcp_sr_intervalsec,
                             int fir_recv_via_rtcp,
                             unsigned int idxProg) {
  int rc = 0;
  STREAM_RTP_INIT_T rtpInit;
  unsigned int streamIdx = 0;
  uint32_t ssrc;
  int payloadType;

  payloadType = codecType_getRtpPT(codecType, pCfg->cfgrtp.payloadTypesMin1);
  if(payloadType < 0 || payloadType > 0x7f) {
    LOG(X_ERROR("Invalid RTP payload type %d"), payloadType);
    return -1;
  }

  memset(&rtpInit, 0, sizeof(rtpInit));

  if(codecType != MEDIA_FILE_TYPE_MP2TS && idxProg > 0) {
    streamIdx = 1;
  }
  if((ssrc = pCfg->cfgrtp.ssrcs[streamIdx]) == 0) {
    ssrc = CREATE_RTP_SSRC();
  }

  if(initTransportParams(&rtpInit,
                         (uint8_t) payloadType,
                         ssrc,
                         pCfg->cfgrtp.sequence_nums[streamIdx],
                         &pCfg->cfgrtp.timeStamps[streamIdx],
                         pCfg->cfgrtp.maxPayloadSz,
                         frtcp_sr_intervalsec,
                         1,
                         fir_recv_via_rtcp,
                         clockRateHz,
                         frameDeltaHz,
                         pCfg->rawxmit,
                         DSCP_AF36) < 0) {
    return -1;
  }

  if(stream_rtp_init(pRtpMulti, &rtpInit, 
     (pCfg->pdestsCfg[0].dtlsCfg.active && !pCfg->pdestsCfg[0].dtlsCfg.dtls_srtp) ? 1 : 0) < 0) {
    return -1;
  }

  return rc;
}

static char *output_descr(char *buf, unsigned int sz, const STREAM_RTP_MULTI_T *pRtpMulti, 
                        const STREAM_XMIT_NODE_T *pStreamOut) {
  int rc = 0;
  unsigned int idx = 0;
  char tmp[32];

  if(pStreamOut->pXmitAction->do_output_rtphdr) {
    if(pRtpMulti->pStreamerCfg->fbReq.nackRtpRetransmit) {
      snprintf(tmp, sizeof(tmp), ", NACK-rtp-rtx");
    } else {
      tmp[0] = '\0';
    }
    rc = snprintf(&buf[idx], sz - idx, " pt: %u, ssrc: 0x%x%s", pRtpMulti->init.pt, pRtpMulti->init.ssrc, tmp);
  } else {
    buf[0] = '\0';
  } 
  return buf;
}

static char *output_descr_stream(char *buf, unsigned int sz, const STREAM_RTP_DEST_T *pDest,
                                 const STREAM_XMIT_NODE_T *pStreamOut) {
  int rc = 0;
  unsigned int idx = 0;
  char descr[2][128];
  char tmp[128];
  const NETIO_SOCK_T *pnetsock = STREAM_RTP_PNETIOSOCK(*pDest);
  const struct sockaddr *psadst = (const struct sockaddr *) &pDest->saDsts;

  descr[0][0] = descr[1][0] = '\0';

  if(pnetsock->turn.use_turn_indication_out) {
    psadst = (const struct sockaddr *) &pnetsock->turn.saTurnSrv;
    snprintf(descr[0], sizeof(descr[0]), "TURN ");
    snprintf(descr[1], sizeof(descr[1]), " (turn-remote-peer-address: %s:%d)", 
              FORMAT_NETADDR(pnetsock->turn.saPeerRelay, tmp, sizeof(tmp)), htons(INET_PORT(pnetsock->turn.saPeerRelay)));

  }

  if((rc = snprintf(buf, sz, "Streaming to %s%s:%d%s %s %s",
                descr[0], FORMAT_NETADDR(*psadst, tmp, sizeof(tmp)), ntohs(PINET_PORT(psadst)), 
                descr[1], RTP_TRANS_DESCR2(pStreamOut->pXmitAction, pDest), pStreamOut->descr)) > 0) {
    idx += rc;
  }

  if(pStreamOut->pXmitAction->do_output_rtphdr && ntohs(INET_PORT(pDest->saDstsRtcp)) > 0 &&
     RTCP_PORT_FROM_RTP(ntohs(INET_PORT(pDest->saDsts))) != ntohs(INET_PORT(pDest->saDstsRtcp))) {

    snprintf(&buf[idx], sz - idx, ", rtcp-port:%d", ntohs(INET_PORT(pDest->saDstsRtcp)));

  }

  return buf;
}

#define DEFAULT_CLOCKHZ(idx, d1, _sdp, d2) \
         pCfg->cfgrtp.clockHzs[(idx)] > 0 ? pCfg->cfgrtp.clockHzs[(idx)] : \
          (  (d1) > 0 ? (d1) : ( (_sdp).common.available ? (_sdp).common.clockHz : (d2) )  );
           

static int pktz_do_mpeg4v(CODEC_PAIR_T *pCodecs, STREAMER_CFG_T *pCfg,
                          STREAM_RTP_MULTI_T rtpMultisRtp[IXCODE_VIDEO_OUT_MAX][2],
                          STREAM_XMIT_NODE_T streamsOutRtp[IXCODE_VIDEO_OUT_MAX][2],
                          float frtcp_sr_intervalsec[2],
                          int fir_recv_via_rtcp[2],
                          unsigned int *pidxProg,
                          int *pktz_do,                          
                          XC_CODEC_TYPE_T codecType) {

  int rc = 0;
  unsigned int outidx;
  unsigned int clockHz = 0;
  STREAMER_DEST_CFG_T *pDest = NULL;
  //FRAME_RATE_T fps;
  FRAME_RATE_T *pFps = NULL;
  char str[128];

  clockHz = DEFAULT_CLOCKHZ(STREAM_IDX_VID, pCodecs->pMpg4v ? pCodecs->pMpg4v->param.clockHz : 0,
                             pCfg->sdpsx[1][0].vid, MPEG4_DEFAULT_CLOCKHZ);

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    //if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
    if(outidx > 0 &&
       (!pCfg->sdpActions[outidx].pPathOutFile && !pCfg->xcode.vid.out[outidx].active)) {
      continue;
    }

    if(stream_init_params(pCfg, &rtpMultisRtp[outidx][*pidxProg], codecType, clockHz,
         pCodecs->pMpg4v ? pCodecs->pMpg4v->param.frameDeltaHz : 0,
         frtcp_sr_intervalsec[1], fir_recv_via_rtcp[1], *pidxProg) < 0) {
      return -1;
    }

    streamsOutRtp[outidx][*pidxProg].pRtpMulti = &rtpMultisRtp[outidx][*pidxProg];
    snprintf(streamsOutRtp[outidx][*pidxProg].descr, sizeof(streamsOutRtp[outidx][*pidxProg].descr),
               STREAM_RTP_DESCRIPTION_MPG4V"%s",
               output_descr(str, sizeof(str), &rtpMultisRtp[outidx][*pidxProg], &streamsOutRtp[outidx][*pidxProg]));

    pDest = pCfg->pdestsCfg[outidx].dstHost[0] ?  &pCfg->pdestsCfg[outidx] : &pCfg->pdestsCfg[0];

    //if(pCodecs->pMpg4v && pCodecs->pMpg4v->param.clockHz > 0 && pCodecs->pMpg4v->param.frameDeltaHz > 0) {
    //  fps.clockHz = pCodecs->pMpg4v->param.clockHz;
    //  fps.frameDeltaHz = pCodecs->pMpg4v->param.frameDeltaHz;
    //  pFps = &fps;
    //}

    sdputil_init(&pCfg->sdpsx[1][outidx],
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.pt,
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.clockRateHz,
                 codecType,
                 pDest->dstHost, pDest->ports[0], pDest->portsRtcp[0], 
                 pDest->srtps[*pidxProg].kt.k.lenKey > 0 ? &pDest->srtps[*pidxProg] : NULL,
                 pDest->dtlsCfg.active ? &pDest->dtlsCfg : NULL,
                 pDest->stunCfgs[*pidxProg].cfg.bindingRequest ? &pDest->stunCfgs[*pidxProg].cfg : NULL,
                 NULL, pFps, &pCfg->fbReq);

  } // end of for(outidx...

  (*pktz_do) |= PKTZ_DO_MPEG4V;
  (*pidxProg)++;

  return rc;
}

static int pktz_do_h263(CODEC_PAIR_T *pCodecs, STREAMER_CFG_T *pCfg,
                          STREAM_RTP_MULTI_T rtpMultisRtp[IXCODE_VIDEO_OUT_MAX][2],
                          STREAM_XMIT_NODE_T streamsOutRtp[IXCODE_VIDEO_OUT_MAX][2],
                          float frtcp_sr_intervalsec[2],
                          int fir_recv_via_rtcp[2],
                          unsigned int *pidxProg,
                          int *pktz_do,                          
                          XC_CODEC_TYPE_T codecType) {

  int rc = 0;
  unsigned int outidx;
  unsigned int clockHz = 0;
  STREAMER_DEST_CFG_T *pDest = NULL;
  //FRAME_RATE_T fps;
  FRAME_RATE_T *pFps = NULL;
  const char *descr = NULL;
  char str[128];

  clockHz = DEFAULT_CLOCKHZ(STREAM_IDX_VID, pCodecs->pMpg4v ? pCodecs->pMpg4v->param.clockHz : 0,
                             pCfg->sdpsx[1][0].vid, MPEG4_DEFAULT_CLOCKHZ);

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    //if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
    if(outidx > 0 &&
       (!pCfg->sdpActions[outidx].pPathOutFile && !pCfg->xcode.vid.out[outidx].active)) {
      continue;
    }

    if(stream_init_params(pCfg, &rtpMultisRtp[outidx][*pidxProg], codecType, clockHz,
         pCodecs->pMpg4v ? pCodecs->pMpg4v->param.frameDeltaHz : 0,
         frtcp_sr_intervalsec[1], fir_recv_via_rtcp[1], *pidxProg) < 0) {
      return -1;
    }

    if(codecType == XC_CODEC_TYPE_H263) {
      descr = STREAM_RTP_DESCRIPTION_H263;
    } else if(codecType == XC_CODEC_TYPE_H263_PLUS) {
      descr = STREAM_RTP_DESCRIPTION_H263_PLUS;
    } else {
      descr = "";
    }

    streamsOutRtp[outidx][*pidxProg].pRtpMulti = &rtpMultisRtp[outidx][*pidxProg];
    snprintf(streamsOutRtp[outidx][*pidxProg].descr, sizeof(streamsOutRtp[outidx][*pidxProg].descr),
               "%s%s",
               descr,
               output_descr(str, sizeof(str), &rtpMultisRtp[outidx][*pidxProg], &streamsOutRtp[outidx][*pidxProg]));

    pDest = pCfg->pdestsCfg[outidx].dstHost[0] ?  &pCfg->pdestsCfg[outidx] : &pCfg->pdestsCfg[0];

    //if(pCodecs->pMpg4v && pCodecs->pMpg4v->param.clockHz > 0 && pCodecs->pMpg4v->param.frameDeltaHz > 0) {
    //  fps.clockHz = pCodecs->pMpg4v->param.clockHz;
    //  fps.frameDeltaHz = pCodecs->pMpg4v->param.frameDeltaHz;
    //  pFps = &fps;
    //}

    sdputil_init(&pCfg->sdpsx[1][outidx],
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.pt,
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.clockRateHz,
                 codecType,
                 pDest->dstHost, pDest->ports[0], pDest->portsRtcp[0], 
                 pDest->srtps[*pidxProg].kt.k.lenKey > 0 ? &pDest->srtps[*pidxProg] : NULL,
                 pDest->dtlsCfg.active ? &pDest->dtlsCfg : NULL,
                 pDest->stunCfgs[*pidxProg].cfg.bindingRequest ? &pDest->stunCfgs[*pidxProg].cfg : NULL,
                 NULL, pFps, &pCfg->fbReq);

  } // end of for(outidx...

  (*pktz_do) |= PKTZ_DO_H263;
  (*pidxProg)++;

  return rc;
}

static int pktz_do_h264(CODEC_PAIR_T *pCodecs, STREAMER_CFG_T *pCfg,
                        STREAM_RTP_MULTI_T rtpMultisRtp[IXCODE_VIDEO_OUT_MAX][2],
                        STREAM_XMIT_NODE_T streamsOutRtp[IXCODE_VIDEO_OUT_MAX][2],
                        float frtcp_sr_intervalsec[2],
                        int fir_recv_via_rtcp[2],
                        unsigned int *pidxProg,
                        int *pktz_do) {
  int rc = 0;
  unsigned int outidx;
  unsigned int clockHz = 0;
  STREAMER_DEST_CFG_T *pDest = NULL;
  char str[128];
  SDP_CODEC_PARAM_T codecParam;

  codecParam.flags = SDP_CODEC_PARAM_FLAGS_PKTZMODE;
  codecParam.u.pktzMode = PKTZ_H264_MODE_NOTSET;

  if(pCfg->cfgrtp.rtpPktzMode == 0) {
    codecParam.u.pktzMode = PKTZ_H264_MODE_0;
    LOG(X_DEBUG("Using H.264 NAL packetization mode 0")); 
  } else if(pCfg->cfgrtp.rtpPktzMode == 1 || pCfg->cfgrtp.rtpPktzMode == -1) { 
    codecParam.u.pktzMode = PKTZ_H264_MODE_1;
  } else if(pCfg->cfgrtp.rtpPktzMode == 2) {
    codecParam.u.pktzMode = PKTZ_H264_MODE_2;
  } else {
    LOG(X_WARNING("Defaulting to H.264 NAL packetization mode 1")); 
    codecParam.u.pktzMode = PKTZ_H264_MODE_1;
  }

  clockHz = DEFAULT_CLOCKHZ(STREAM_IDX_VID, pCodecs->pH264 ? pCodecs->pH264->clockHz : 0,
                            pCfg->sdpsx[1][0].vid, H264_DEFAULT_CLOCKHZ);

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    //if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
    if(outidx > 0 &&
       (!pCfg->sdpActions[outidx].pPathOutFile && !pCfg->xcode.vid.out[outidx].active)) {
      continue;
    }

    if(stream_init_params(pCfg, &rtpMultisRtp[outidx][*pidxProg],
                          XC_CODEC_TYPE_H264, clockHz,
                          pCodecs->pH264 ? pCodecs->pH264->frameDeltaHz : 0,
                          frtcp_sr_intervalsec[1], fir_recv_via_rtcp[1], *pidxProg) < 0) {
      return -1;
    }

    pDest = pCfg->pdestsCfg[outidx].dstHost[0] ?  &pCfg->pdestsCfg[outidx] : &pCfg->pdestsCfg[0];

    streamsOutRtp[outidx][*pidxProg].pRtpMulti = &rtpMultisRtp[outidx][*pidxProg];
    snprintf(streamsOutRtp[outidx][*pidxProg].descr, sizeof(streamsOutRtp[outidx][*pidxProg].descr),
           STREAM_RTP_DESCRIPTION_H264"%s",
           output_descr(str, sizeof(str), &rtpMultisRtp[outidx][*pidxProg], &streamsOutRtp[outidx][*pidxProg]));

    sdputil_init(&pCfg->sdpsx[1][outidx],
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.pt,
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.clockRateHz,
                 XC_CODEC_TYPE_H264, pDest->dstHost, pDest->ports[0], pDest->portsRtcp[0], 
                 pDest->srtps[*pidxProg].kt.k.lenKey > 0 ? &pDest->srtps[*pidxProg] : NULL,
                 pDest->dtlsCfg.active ? &pDest->dtlsCfg : NULL,
                 pDest->stunCfgs[*pidxProg].cfg.bindingRequest ? &pDest->stunCfgs[*pidxProg].cfg : NULL,
                 &codecParam, NULL, &pCfg->fbReq);

  } // end of for(outidx...
  //fprintf(stderr, "H264 SDP %s:%d, %s:%d\n", pCfg->sdpsx[1][0].c.iphost, pCfg->sdpsx[1][0].vid.common.port, pCfg->sdpsx[1][1].c.iphost, pCfg->sdpsx[1][1].vid.common.port);
 //fprintf(stderr, " clock:%d %d\n", clockHz,  streamsOutRtp[0][*pidxProg].pRtpMulti->init.clockRateHz);

  (*pktz_do) |= PKTZ_DO_H264;
  (*pidxProg)++;

  return rc;
}

static int pktz_do_vp8(CODEC_PAIR_T *pCodecs, STREAMER_CFG_T *pCfg,
                        STREAM_RTP_MULTI_T rtpMultisRtp[IXCODE_VIDEO_OUT_MAX][2],
                        STREAM_XMIT_NODE_T streamsOutRtp[IXCODE_VIDEO_OUT_MAX][2],
                        float frtcp_sr_intervalsec[2],
                        int fir_recv_via_rtcp[2],
                        unsigned int *pidxProg,
                        int *pktz_do) {
  int rc = 0;
  unsigned int outidx;
  unsigned int clockHz = 0;
  STREAMER_DEST_CFG_T *pDest = NULL;
  char str[128];

  clockHz = DEFAULT_CLOCKHZ(STREAM_IDX_VID, pCodecs->pVp8 ? pCodecs->pVp8->clockHz : 0,
                            pCfg->sdpsx[1][0].vid, VP8_DEFAULT_CLOCKHZ);

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    //if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
    if(outidx > 0 &&
       (!pCfg->sdpActions[outidx].pPathOutFile && !pCfg->xcode.vid.out[outidx].active)) {
      continue;
    }

    if(stream_init_params(pCfg, &rtpMultisRtp[outidx][*pidxProg],
                          XC_CODEC_TYPE_VP8, clockHz,
                          pCodecs->pVp8 ? pCodecs->pVp8->frameDeltaHz : 0,
                          frtcp_sr_intervalsec[1], fir_recv_via_rtcp[1], *pidxProg) < 0) {
      return -1;
    }

    streamsOutRtp[outidx][*pidxProg].pRtpMulti = &rtpMultisRtp[outidx][*pidxProg];
    snprintf(streamsOutRtp[outidx][*pidxProg].descr, sizeof(streamsOutRtp[outidx][*pidxProg].descr),
           STREAM_RTP_DESCRIPTION_VP8"%s",
           output_descr(str, sizeof(str), &rtpMultisRtp[outidx][*pidxProg], &streamsOutRtp[outidx][*pidxProg]));

    pDest = pCfg->pdestsCfg[outidx].dstHost[0] ?  &pCfg->pdestsCfg[outidx] : &pCfg->pdestsCfg[0];

    sdputil_init(&pCfg->sdpsx[1][outidx],
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.pt,
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.clockRateHz,
                 XC_CODEC_TYPE_VP8, pDest->dstHost, pDest->ports[0], pDest->portsRtcp[0], 
                 pDest->srtps[*pidxProg].kt.k.lenKey > 0 ? &pDest->srtps[*pidxProg] : NULL,
                 pDest->dtlsCfg.active ? &pDest->dtlsCfg : NULL,
                 pDest->stunCfgs[*pidxProg].cfg.bindingRequest ? &pDest->stunCfgs[*pidxProg].cfg : NULL,
                 NULL, NULL, &pCfg->fbReq);

  } // end of for(outidx...

  (*pktz_do) |= PKTZ_DO_VP8;
  (*pidxProg)++;

  return rc;
}

static int pktz_do_aac(CODEC_PAIR_T *pCodecs, STREAMER_CFG_T *pCfg,
                       STREAM_RTP_MULTI_T rtpMultisRtp[IXCODE_VIDEO_OUT_MAX][2],
                       STREAM_XMIT_NODE_T streamsOutRtp[IXCODE_VIDEO_OUT_MAX][2],
                       float frtcp_sr_intervalsec[2],
                       int fir_recv_via_rtcp[2],
                       unsigned int *pidxProg,
                       int *pktz_do) {
  int rc = 0;
  unsigned int outidx;
  unsigned int clockHz = 0;
  STREAMER_DEST_CFG_T *pDest = NULL;
  char str[128];

  clockHz = DEFAULT_CLOCKHZ(STREAM_IDX_AUD, pCodecs->pAac ? pCodecs->pAac->clockHz : 0,
                                 pCfg->sdpsx[1][0].aud, 0);

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    //if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
    if(outidx > 0 &&
       (!pCfg->sdpActions[outidx].pPathOutFile && !pCfg->xcode.vid.out[outidx].active)) {
      continue;
    }

    if(stream_init_params(pCfg, &rtpMultisRtp[outidx][*pidxProg],
                          XC_CODEC_TYPE_AAC, clockHz,
         pCodecs->pAac ? pCodecs->pAac->decoderSP.frameDeltaHz : 0,
         frtcp_sr_intervalsec[1], 0, *pidxProg) < 0) {
      return -1;
    }

    streamsOutRtp[outidx][*pidxProg].pRtpMulti = &rtpMultisRtp[outidx][*pidxProg];
    snprintf(streamsOutRtp[outidx][*pidxProg].descr, sizeof(streamsOutRtp[outidx][*pidxProg].descr),
           STREAM_RTP_DESCRIPTION_AAC"%s",
           output_descr(str, sizeof(str), &rtpMultisRtp[outidx][*pidxProg], &streamsOutRtp[outidx][*pidxProg]));

    pDest = pCfg->pdestsCfg[outidx].dstHost[0] ?  &pCfg->pdestsCfg[outidx] : &pCfg->pdestsCfg[0];

    sdputil_init(&pCfg->sdpsx[1][outidx],
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.pt,
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.clockRateHz,
                 XC_CODEC_TYPE_AAC,
                 pDest->dstHost, pDest->ports[1], pDest->portsRtcp[1], 
                 pDest->srtps[*pidxProg].kt.k.lenKey > 0 ? &pDest->srtps[*pidxProg] : NULL,
                 pDest->dtlsCfg.active ? &pDest->dtlsCfg : NULL,
                 pDest->stunCfgs[*pidxProg].cfg.bindingRequest ? &pDest->stunCfgs[*pidxProg].cfg : NULL,
                 NULL, NULL, &pCfg->fbReq);

    //strncpy(pCfg->sdps[1].aud.u.aac.mode, "AAC-hbr", sizeof(pCfg->sdps[1].aud.u.aac.mode));
    //pCfg->sdps[1].aud.u.aac.sizelength = 13;
    //pCfg->sdps[1].aud.u.aac.indexlength = 3;
    //pCfg->sdps[1].aud.u.aac.indexdeltalength = 3;

  } // end of for(outidx...
  //fprintf(stderr, "AAC clock:%d %d\n", clockHz,  streamsOutRtp[0][*pidxProg].pRtpMulti->init.clockRateHz);
  (*pktz_do) |= PKTZ_DO_AAC;
  (*pidxProg)++;

  return rc;
}

static int pktz_do_amr(CODEC_PAIR_T *pCodecs, STREAMER_CFG_T *pCfg,
                       STREAM_RTP_MULTI_T rtpMultisRtp[IXCODE_VIDEO_OUT_MAX][2],
                       STREAM_XMIT_NODE_T streamsOutRtp[IXCODE_VIDEO_OUT_MAX][2],
                       float frtcp_sr_intervalsec[2],
                       int fir_recv_via_rtcp[2],
                       unsigned int *pidxProg, 
                       int *pktz_do) {
  int rc = 0;
  unsigned int outidx;
  unsigned int clockHz = 0;
  STREAMER_DEST_CFG_T *pDest = NULL;
  char str[128];

  clockHz = DEFAULT_CLOCKHZ(STREAM_IDX_AUD, pCodecs->pAmr ? pCodecs->pAmr->clockHz : 0,
                             pCfg->sdpsx[1][0].aud, AMRNB_CLOCKHZ);

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    //if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
    if(outidx > 0 &&
       (!pCfg->sdpActions[outidx].pPathOutFile && !pCfg->xcode.vid.out[outidx].active)) {
      continue;
    }

    if(stream_init_params(pCfg, &rtpMultisRtp[outidx][*pidxProg],
                          XC_CODEC_TYPE_AMRNB, clockHz,
                          AMRNB_SAMPLE_DURATION_HZ, frtcp_sr_intervalsec[1], 0, *pidxProg) < 0) {
      return -1;
    }

    streamsOutRtp[outidx][*pidxProg].pRtpMulti = &rtpMultisRtp[outidx][*pidxProg];
    snprintf(streamsOutRtp[outidx][*pidxProg].descr, sizeof(streamsOutRtp[outidx][*pidxProg].descr),
             STREAM_RTP_DESCRIPTION_AMR"%s",
             output_descr(str, sizeof(str), &rtpMultisRtp[outidx][*pidxProg], &streamsOutRtp[outidx][*pidxProg]));

    pDest = pCfg->pdestsCfg[outidx].dstHost[0] ?  &pCfg->pdestsCfg[outidx] : &pCfg->pdestsCfg[0];

    sdputil_init(&pCfg->sdpsx[1][outidx],
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.pt,
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.clockRateHz,
                 XC_CODEC_TYPE_AMRNB,
                 pDest->dstHost, pDest->ports[1], pDest->portsRtcp[1], 
                 pDest->srtps[*pidxProg].kt.k.lenKey > 0 ? &pDest->srtps[*pidxProg] : NULL,
                 pDest->dtlsCfg.active ? &pDest->dtlsCfg : NULL,
                 pDest->stunCfgs[*pidxProg].cfg.bindingRequest ? &pDest->stunCfgs[*pidxProg].cfg : NULL,
                 NULL, NULL, &pCfg->fbReq);

  } // end of for(outidx...

  (*pktz_do) |= PKTZ_DO_AMR;
  (*pidxProg)++;

  return rc;
}

static int pktz_do_silk(CODEC_PAIR_T *pCodecs, STREAMER_CFG_T *pCfg,
                       STREAM_RTP_MULTI_T rtpMultisRtp[IXCODE_VIDEO_OUT_MAX][2],
                       STREAM_XMIT_NODE_T streamsOutRtp[IXCODE_VIDEO_OUT_MAX][2],
                       float frtcp_sr_intervalsec[2],
                       int fir_recv_via_rtcp[2],
                       unsigned int *pidxProg,
                       int *pktz_do) {
  int rc = 0;
  unsigned int outidx;
  unsigned int clockHz = 0;
  STREAMER_DEST_CFG_T *pDest = NULL;
  SDP_CODEC_PARAM_T codecParam;
  SDP_CODEC_PARAM_T *pCodecParam = NULL;
  char str[128];

  if(pCfg->xcode.aud.common.cfgDo_xcode && pCfg->xcode.aud.cfgChannelsOut > 0) {
    pCodecParam = &codecParam;
    pCodecParam->flags = SDP_CODEC_PARAM_FLAGS_CHANNELS;
    pCodecParam->u.channels = pCfg->xcode.aud.cfgChannelsOut;
  }

  clockHz = DEFAULT_CLOCKHZ(STREAM_IDX_AUD, 0,
                             pCfg->sdpsx[1][0].aud, pCfg->xcode.aud.cfgSampleRateOut);

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    //if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
    if(outidx > 0 &&
       (!pCfg->sdpActions[outidx].pPathOutFile && !pCfg->xcode.vid.out[outidx].active)) {
      continue;
    }

    if(stream_init_params(pCfg, &rtpMultisRtp[outidx][*pidxProg],
                          XC_CODEC_TYPE_SILK, clockHz,
                          clockHz/ 50, frtcp_sr_intervalsec[1], 0, *pidxProg) < 0) {
      return -1;
    }

    streamsOutRtp[outidx][*pidxProg].pRtpMulti = &rtpMultisRtp[outidx][*pidxProg];
    snprintf(streamsOutRtp[outidx][*pidxProg].descr, sizeof(streamsOutRtp[outidx][*pidxProg].descr),
             STREAM_RTP_DESCRIPTION_SILK"%s",
             output_descr(str, sizeof(str), &rtpMultisRtp[outidx][*pidxProg], &streamsOutRtp[outidx][*pidxProg]));

    pDest = pCfg->pdestsCfg[outidx].dstHost[0] ?  &pCfg->pdestsCfg[outidx] : &pCfg->pdestsCfg[0];

    sdputil_init(&pCfg->sdpsx[1][outidx],
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.pt,
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.clockRateHz,
                 XC_CODEC_TYPE_SILK,
                 pDest->dstHost, pDest->ports[1], pDest->portsRtcp[1], 
                 pDest->srtps[*pidxProg].kt.k.lenKey > 0 ? &pDest->srtps[*pidxProg] : NULL,
                 pDest->dtlsCfg.active ? &pDest->dtlsCfg : NULL,
                 pDest->stunCfgs[*pidxProg].cfg.bindingRequest ? &pDest->stunCfgs[*pidxProg].cfg : NULL,
                 NULL, NULL, &pCfg->fbReq);

  } // end of for(outidx...

  (*pktz_do) |= PKTZ_DO_SILK;
  (*pidxProg)++;

  return rc;
}

static int pktz_do_opus(CODEC_PAIR_T *pCodecs, STREAMER_CFG_T *pCfg,
                       STREAM_RTP_MULTI_T rtpMultisRtp[IXCODE_VIDEO_OUT_MAX][2],
                       STREAM_XMIT_NODE_T streamsOutRtp[IXCODE_VIDEO_OUT_MAX][2],
                       float frtcp_sr_intervalsec[2],
                       int fir_recv_via_rtcp[2],
                       unsigned int *pidxProg,
                       int *pktz_do) {
  int rc = 0;
  unsigned int outidx;
  unsigned int clockHz = 0;
  STREAMER_DEST_CFG_T *pDest = NULL;
  SDP_CODEC_PARAM_T codecParam;
  SDP_CODEC_PARAM_T *pCodecParam = NULL;
  char str[128];

  if(pCfg->xcode.aud.common.cfgDo_xcode && pCfg->xcode.aud.cfgChannelsOut > 0) {
    pCodecParam = &codecParam;
    pCodecParam->flags = SDP_CODEC_PARAM_FLAGS_CHANNELS;
    pCodecParam->u.channels = pCfg->xcode.aud.cfgChannelsOut;
  }

  clockHz = DEFAULT_CLOCKHZ(STREAM_IDX_AUD, 0,
                             pCfg->sdpsx[1][0].aud, pCfg->xcode.aud.cfgSampleRateOut);

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(outidx > 0 &&
       (!pCfg->sdpActions[outidx].pPathOutFile && !pCfg->xcode.vid.out[outidx].active)) {
      continue;
    }

    if(stream_init_params(pCfg, &rtpMultisRtp[outidx][*pidxProg],
                          XC_CODEC_TYPE_OPUS, clockHz,
                          clockHz/ 50, frtcp_sr_intervalsec[1], 0, *pidxProg) < 0) {
      return -1;
    }

    streamsOutRtp[outidx][*pidxProg].pRtpMulti = &rtpMultisRtp[outidx][*pidxProg];
    snprintf(streamsOutRtp[outidx][*pidxProg].descr, sizeof(streamsOutRtp[outidx][*pidxProg].descr),
             STREAM_RTP_DESCRIPTION_OPUS"%s",
             output_descr(str, sizeof(str), &rtpMultisRtp[outidx][*pidxProg], &streamsOutRtp[outidx][*pidxProg]));

    pDest = pCfg->pdestsCfg[outidx].dstHost[0] ?  &pCfg->pdestsCfg[outidx] : &pCfg->pdestsCfg[0];

    sdputil_init(&pCfg->sdpsx[1][outidx],
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.pt,
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.clockRateHz,
                 XC_CODEC_TYPE_OPUS,
                 pDest->dstHost, pDest->ports[1], pDest->portsRtcp[1], 
                 pDest->srtps[*pidxProg].kt.k.lenKey > 0 ? &pDest->srtps[*pidxProg] : NULL,
                 pDest->dtlsCfg.active ? &pDest->dtlsCfg : NULL,
                 pDest->stunCfgs[*pidxProg].cfg.bindingRequest ? &pDest->stunCfgs[*pidxProg].cfg : NULL,
                 pCodecParam, NULL, &pCfg->fbReq);

  } // end of for(outidx...

  (*pktz_do) |= PKTZ_DO_OPUS;
  (*pidxProg)++;

  return rc;
}

static int pktz_do_pcm(CODEC_PAIR_T *pCodecs, STREAMER_CFG_T *pCfg,
                       STREAM_RTP_MULTI_T rtpMultisRtp[IXCODE_VIDEO_OUT_MAX][2],
                       STREAM_XMIT_NODE_T streamsOutRtp[IXCODE_VIDEO_OUT_MAX][2],
                       float frtcp_sr_intervalsec[2],
                       int fir_recv_via_rtcp[2],
                       unsigned int *pidxProg,
                       int *pktz_do) {
  int rc = 0;
  unsigned int outidx;
  STREAMER_DEST_CFG_T *pDest = NULL;
  XC_CODEC_TYPE_T codecType;
  char str[128];

  codecType = XC_CODEC_TYPE_G711_MULAW;

  if((pCfg->xcode.aud.common.cfgDo_xcode &&
      pCfg->xcode.aud.common.cfgFileTypeOut == XC_CODEC_TYPE_G711_ALAW) ||
      pCfg->sdpsx[1][0].aud.common.codecType == XC_CODEC_TYPE_G711_ALAW) {
    codecType = XC_CODEC_TYPE_G711_ALAW;
    (*pktz_do) |= PKTZ_DO_G711_ALAW;
  } else {
    (*pktz_do) |= PKTZ_DO_G711_MULAW;
  }

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    //if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
    if(outidx > 0 &&
       (!pCfg->sdpActions[outidx].pPathOutFile && !pCfg->xcode.vid.out[outidx].active)) {
      continue;
    }

    if(stream_init_params(pCfg, &rtpMultisRtp[outidx][*pidxProg],
                          codecType, 8000, 160,
                          frtcp_sr_intervalsec[1], 0, *pidxProg) < 0) {
      return -1;
    }

    streamsOutRtp[outidx][*pidxProg].pRtpMulti = &rtpMultisRtp[outidx][*pidxProg];
    snprintf(streamsOutRtp[outidx][*pidxProg].descr, sizeof(streamsOutRtp[outidx][*pidxProg].descr), "%s%s",
              (codecType == XC_CODEC_TYPE_G711_ALAW ?
                 STREAM_RTP_DESCRIPTION_G711_ALAW :
                 STREAM_RTP_DESCRIPTION_G711_MULAW),
              output_descr(str, sizeof(str), &rtpMultisRtp[outidx][*pidxProg], &streamsOutRtp[outidx][*pidxProg]));

    pDest = pCfg->pdestsCfg[outidx].dstHost[0] ?  &pCfg->pdestsCfg[outidx] : &pCfg->pdestsCfg[0];

    sdputil_init(&pCfg->sdpsx[1][outidx],
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.pt,
                 streamsOutRtp[0][*pidxProg].pRtpMulti->init.clockRateHz, codecType,
                 pDest->dstHost, pDest->ports[1], pDest->portsRtcp[1], 
                 pDest->srtps[*pidxProg].kt.k.lenKey > 0 ? &pDest->srtps[*pidxProg] : NULL,
                 pDest->dtlsCfg.active ? &pDest->dtlsCfg : NULL,
                 pDest->stunCfgs[*pidxProg].cfg.bindingRequest ? &pDest->stunCfgs[*pidxProg].cfg : NULL,
                 NULL, NULL, &pCfg->fbReq);

  } // end of for(outidx...

  (*pidxProg)++;

  return rc;
}

static void link_rtp_dest_list(STREAM_RTP_MULTI_T rtpMultisRtp[IXCODE_VIDEO_OUT_MAX][2]) {
  unsigned int outidx;

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    //
    // Create a linked list of all the RTP output structs
    // This list will be used by the RTCP RR receiver to find the appropriate
    // STREAM_RTP_MULTI_T context since an RTCP msg may be received on the socket
    // listener of the wrong struct
    //
    if(outidx > 0) {
      rtpMultisRtp[outidx - 1][1].pnext = &rtpMultisRtp[outidx][0];
    }
    rtpMultisRtp[outidx][0].pnext = &rtpMultisRtp[outidx][1];
    rtpMultisRtp[outidx][1].pnext = NULL;
    rtpMultisRtp[outidx][0].pheadall = rtpMultisRtp[outidx][1].pheadall = &rtpMultisRtp[0][0];

  }
}

static int check_duplicate_rtcp(STREAM_RTP_MULTI_T rtpMultisRtp[IXCODE_VIDEO_OUT_MAX][2]) {
  unsigned int outidx, outidx2;
  unsigned int idx, idx2;
  unsigned int idxDest, idxDest2;
  int cnt = 0;

  //
  // This works ok in practice, because size N is quite limited
  //

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
    for(idx = 0; idx < 2; idx++) {
      for(idxDest = 0; idxDest < rtpMultisRtp[outidx][idx].maxDests; idxDest++) {

        if(!rtpMultisRtp[outidx][idx].pdests[idxDest].isactive ||
          INET_PORT(rtpMultisRtp[outidx][idx].pdests[idxDest].saDstsRtcp) == 0) {
          continue;
        }

        for(outidx2 = 0; outidx2 < IXCODE_VIDEO_OUT_MAX; outidx2++) {
          for(idx2 = 0; idx2 < 2; idx2++) {
            for(idxDest2 = 0; idxDest2 < rtpMultisRtp[outidx2][idx2].maxDests; idxDest2++) {

              if(!rtpMultisRtp[outidx][idx].pdests[idxDest].isactive ||
                INET_PORT(rtpMultisRtp[outidx][idx].pdests[idxDest].saDsts) == 0 ||
                INET_PORT(rtpMultisRtp[outidx][idx].pdests[idxDest].saDstsRtcp) == 0 ||
                (outidx == outidx2 && idx == idx2 && idxDest == idxDest2)) {
                continue;
              } else if(INET_PORT(rtpMultisRtp[outidx][idx].pdests[idxDest].saDstsRtcp) == 
                        INET_PORT(rtpMultisRtp[outidx2][idx2].pdests[idxDest2].saDstsRtcp) ||
                        INET_PORT(rtpMultisRtp[outidx][idx].pdests[idxDest].saDsts) == 
                        INET_PORT(rtpMultisRtp[outidx2][idx2].pdests[idxDest2].saDsts)) {
                //fprintf(stderr, "DUP!\n");
                return 1;
              }
              cnt++;
            }
          }
        }

        //fprintf(stderr, "OUTIDX[%d][%d].idxDest[%d/%d]:active:%d, port:%d, cnt:%d\n", outidx, idx, idxDest,rtpMultisRtp[outidx][idx].maxDests, rtpMultisRtp[outidx][idx].pdests[idxDest].isactive, htons(INET_PORT(rtpMultisRtp[outidx][idx].pdests[idxDest].saDstsRtcp)), cnt);
      }
    }
  }

  return 0;
}
/*
static int check_audiovideo_mux(STREAM_RTP_MULTI_T rtpMultisRtp[IXCODE_VIDEO_OUT_MAX][2]) {
  unsigned int outidx, outidx2;
  unsigned int idx, idx2;
  unsigned int idxDest, idxDest2;
  int cnt = 0;

  //
  // This works ok in practice, because size N is quite limited
  //

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
    for(idx = 0; idx < 2; idx++) {
      for(idxDest = 0; idxDest < rtpMultisRtp[outidx][idx].maxDests; idxDest++) {

        if(!rtpMultisRtp[outidx][idx].pdests[idxDest].isactive ||
          rtpMultisRtp[outidx][idx].pdests[idxDest].saDstsRtcp.sin_port == 0) {
          continue;
        }

        for(outidx2 = 0; outidx2 < IXCODE_VIDEO_OUT_MAX; outidx2++) {
          for(idx2 = 0; idx2 < 2; idx2++) {
            for(idxDest2 = 0; idxDest2 < rtpMultisRtp[outidx2][idx2].maxDests; idxDest2++) {

              if(!rtpMultisRtp[outidx][idx].pdests[idxDest].isactive ||
                rtpMultisRtp[outidx][idx].pdests[idxDest].saDsts.sin_port == 0 ||
                (outidx == outidx2 && idx == idx2 && idxDest == idxDest2)) {
                continue;
              } else if(rtpMultisRtp[outidx][idx].pdests[idxDest].saDsts.sin_port ==
                        rtpMultisRtp[outidx2][idx2].pdests[idxDest2].saDsts.sin_port ||
                        rtpMultisRtp[outidx][idx].pdests[idxDest].saDsts.sin_addr.s_addr ==
                        rtpMultisRtp[outidx2][idx2].pdests[idxDest2].saDsts.sin_addr.s_addr ) {
           
                if(NETIOSOCK_FD(rtpMultisRtp[outidx2][idx2].pdests[idxDest2].xmit.netsock) != INVALID_SOCKET &&
                   NETIOSOCK_FD(rtpMultisRtp[outidx][idx].pdests[idxDest].xmit.netsock) != INVALID_SOCKET &&
                   !rtpMultisRtp[outidx2][idx2].pdests[idxDest2].xmit.netsock.pPeer &&
                   rtpMultisRtp[outidx][idx].pdests[idxDest].xmit.netsock.pPeer !=
                   &rtpMultisRtp[outidx2][idx2].pdests[idxDest2].xmit.netsock) {

                  LOG(X_DEBUG("Setting output %s:%d netsock fd:%d = peer-fd:%d"), 
                    inet_ntoa(rtpMultisRtp[outidx][idx].pdests[idxDest].saDsts.sin_addr),
                    htons(rtpMultisRtp[outidx][idx].pdests[idxDest].saDsts.sin_port),
                    NETIOSOCK_FD(rtpMultisRtp[outidx2][idx2].pdests[idxDest2].xmit.netsock),
                    NETIOSOCK_FD(rtpMultisRtp[outidx][idx].pdests[idxDest].xmit.netsock));

                  rtpMultisRtp[outidx2][idx2].pdests[idxDest2].xmit.netsock.pPeer = 
                           &rtpMultisRtp[outidx][idx].pdests[idxDest].xmit.netsock;
                }

              }
              cnt++;
            }
          }
        }

        //fprintf(stderr, "OUTIDX[%d][%d].idxDest[%d/%d]:active:%d, port:%d, cnt:%d\n", outidx, idx, idxDest,rtpMultisRtp[outidx][idx].maxDests, rtpMultisRtp[outidx][idx].pdests[idxDest].isactive, htons(rtpMultisRtp[outidx][idx].pdests[idxDest].saDstsRtcp.sin_port), cnt);
      }
    }
  }

  return 0;
}
*/

static void show_rtcpfb_config(const STREAMER_CFG_T *pStreamerCfg, int firRecv, int firXmit, 
                               int nackRecv, int nackXmit, int apprembXmit) {

  if(firXmit) {
    LOG(X_DEBUG("FIR-Encoder:%d, FIR-RTCPInputHandler:%d, FIR-EncoderFromRemoteConnect:%d, "
               "FIR-EncoderFromRemoteMessage:%d"), 
      pStreamerCfg->fbReq.firCfg.fir_encoder,
      pStreamerCfg->fbReq.firCfg.fir_recv_via_rtcp,
      pStreamerCfg->fbReq.firCfg.fir_recv_from_connect,
      pStreamerCfg->fbReq.firCfg.fir_recv_from_remote);
  }

  if(firRecv) {
    LOG(X_DEBUG("FIR-SendFromRemoteConnect:%d, "
               "FIR-SendFromRemoteMessage:%d, FIR-SendFromDecoder:%d, FIR-SendFromCapture:%d"),
      pStreamerCfg->fbReq.firCfg.fir_send_from_local,
      pStreamerCfg->fbReq.firCfg.fir_send_from_remote,
      pStreamerCfg->fbReq.firCfg.fir_send_from_decoder,
      pStreamerCfg->fbReq.firCfg.fir_send_from_capture);
  }

  if(nackXmit) {
    LOG(X_DEBUG("NACK-RtpRetransmit:%d"), pStreamerCfg->fbReq.nackRtpRetransmit);
  }

  if(nackRecv) {
    LOG(X_DEBUG("NACK-RtcpRequestRetransmit:%d"), BOOL_ISENABLED(pStreamerCfg->fbReq.nackRtcpSend));
  }

  if(apprembXmit) {
    LOG(X_DEBUG("REMB-REMBRtcpNotify:%d"), BOOL_ISENABLED(pStreamerCfg->fbReq.apprembRtcpSend));
  }

}

static int stream_av_onexit(STREAMER_CFG_T *pCfg, CODEC_PAIR_T *pCodecs, int rc) {
  unsigned int outidx;

  if(rc < 0) {
    stream_setstate(pCfg, STREAMER_STATE_ERROR, 1);
  }

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(pCfg->rtpMultisMp2ts[outidx].isinit) {
      stream_rtp_close(&pCfg->rtpMultisMp2ts[0]);
    }
    if(&pCfg->rtpMultisRtp[outidx][0].isinit) {
      stream_rtp_close(&pCfg->rtpMultisRtp[0][0]);
    }
    if(&pCfg->rtpMultisRtp[outidx][1].isinit) {
      stream_rtp_close(&pCfg->rtpMultisRtp[0][1]);
    }
  }

  codecs_close(pCodecs);

  outfmt_close(&pCfg->action.liveFmts);

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(pCfg->rtpMultisMp2ts[outidx].pdests) {
      avc_free((void **) &pCfg->rtpMultisMp2ts[outidx].pdests);
    }
    if(pCfg->rtpMultisRtp[outidx][0].pdests) {
      avc_free((void **) &pCfg->rtpMultisRtp[outidx][0].pdests);
    }
    if(pCfg->rtpMultisRtp[outidx][1].pdests) {
      avc_free((void **) &pCfg->rtpMultisRtp[outidx][1].pdests);
    }

  }

  return rc;
}

static int stream_av(CODEC_PAIR_T *pCodecs, STREAMER_CFG_T *pCfg) {

  int rc = 0;
  int rcJmp;
  int jmpTryCnt;
  unsigned int idx;
  unsigned int idxDest;
  unsigned int idxProg = 0;
  STREAM_XMIT_NODE_T streamIn;

  STREAM_XMIT_NODE_T streamsOutMp2ts[IXCODE_VIDEO_OUT_MAX];
  STREAM_XMIT_NODE_T streamsOutRtp[IXCODE_VIDEO_OUT_MAX][2]; // 0 - video, 1 - audio

  STREAM_XMIT_BW_DESCR_T  xmitBwDescrs[IXCODE_VIDEO_OUT_MAX][2];
  float frtcp_sr_intervalsec[2];
  int fir_recv_via_rtcp[2];
  STREAM_AV_T *pAv = pCodecs->pStreamAv;
  STREAM_AV_T *pStreamAv = NULL;
  PKTZ_CBSET_T pktzV[MAX_PACKETIZERS * IXCODE_VIDEO_OUT_MAX];
  PKTZ_CBSET_T pktzA[MAX_PACKETIZERS * IXCODE_VIDEO_OUT_MAX];
  PKTZ_INIT_PARAMS_MP2TS_T pktzsInitMp2ts[IXCODE_VIDEO_OUT_MAX];
  PKTZ_INIT_PARAMS_FRBUF_T pktzInitFrbufV;
  PKTZ_INIT_PARAMS_FRBUF_T pktzInitFrbufA;
  PKTZ_INIT_PARAMS_H264_T pktzsInitH264[IXCODE_VIDEO_OUT_MAX];
  PKTZ_INIT_PARAMS_MPG4V_T pktzsInitMpg4v[IXCODE_VIDEO_OUT_MAX];
  PKTZ_INIT_PARAMS_H263_T pktzsInitH263[IXCODE_VIDEO_OUT_MAX];
  PKTZ_INIT_PARAMS_VP8_T pktzsInitVp8[IXCODE_VIDEO_OUT_MAX];
  PKTZ_INIT_PARAMS_AAC_T pktzsInitAac[IXCODE_VIDEO_OUT_MAX];
  PKTZ_INIT_PARAMS_AMR_T pktzsInitAmr[IXCODE_VIDEO_OUT_MAX];
  PKTZ_INIT_PARAMS_SILK_T pktzsInitSilk[IXCODE_VIDEO_OUT_MAX];
  PKTZ_INIT_PARAMS_OPUS_T pktzsInitOpus[IXCODE_VIDEO_OUT_MAX];
  PKTZ_INIT_PARAMS_PCM_T pktzsInitPcm[IXCODE_VIDEO_OUT_MAX];
  STREAMER_DEST_CFG_T *pDest;
  int pktz_do = 0;
  AV_PROG_T *pProg;
  int lastPatContIdx[IXCODE_VIDEO_OUT_MAX];
  int lastPmtContIdx[IXCODE_VIDEO_OUT_MAX];
  double duration;
  double durationLongest = 0;
  float fJumpOffsetSecActual = 0;
  unsigned int numPacketizers = 0;
  unsigned int numPacketizersV = 0;
  unsigned int numPacketizersA = 0;
  int stream_tofile = 0;
  int stream_tofrbuf = 0;
  int assume_vidfmt = 0;
  int assume_audfmt = 0;
  //int is_pip = pCfg->pip.pCfgOverlay ? 1 : 0;
  int is_pip = pCfg->xcode.vid.pip.active ? 1 : 0;
  unsigned int outidx;
  char str[256];

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
    lastPatContIdx[outidx] = MP2TS_HDR_TS_CONTINUITY;
    lastPmtContIdx[outidx] = MP2TS_HDR_TS_CONTINUITY;
  }

  memset(&pktzInitFrbufV, 0, sizeof(pktzInitFrbufV));
  memset(&pktzInitFrbufA, 0, sizeof(pktzInitFrbufA));

  //
  // streamAv.numProg is ambiguous between input progs, and desired output progs
  // Ensure numProg remains 2, if vid and aud inputs are present,regardless of
  // what is requested at the output. So that vid output is always prog idx 0.
  //
  if(pCfg->novid && pCfg->xcode.vid.common.cfgDo_xcode) {
    pCfg->xcode.vid.common.cfgDo_xcode = 0;
  }
  if(pCfg->noaud) {
    if(pCodecs->vs.streamAv.numProg > 1) {
      pCodecs->vs.streamAv.numProg--;
    }
    if(pCfg->xcode.aud.common.cfgDo_xcode) {
      pCfg->xcode.aud.common.cfgDo_xcode = 0;
    }
  }

  //
  // Check if output is to local framebuffer(s)
  //
  if(!pCfg->xcode.vid.pip.active && pCfg->xcode.vid.common.cfgDo_xcode &&
      IS_XC_CODEC_TYPE_VID_RAW(pCfg->xcode.vid.common.cfgFileTypeOut) &&
      (!pCfg->pip.pMixerCfg || !pCfg->pip.pMixerCfg->conferenceInputDriver)) {

    pktzInitFrbufV.va.codecType = pCfg->xcode.vid.common.cfgFileTypeOut;
    pktzInitFrbufA.va.isvideo = 1;
    pktzInitFrbufV.va.u.v.width = pCfg->xcode.vid.out[0].cfgOutH;
    pktzInitFrbufV.va.u.v.height = pCfg->xcode.vid.out[0].cfgOutV;
    pktzInitFrbufV.ppPktzFrBufOut = &pCfg->pRawOutCfg->pVidFrBuf;
    stream_tofrbuf = 1;
  }

  if(!pCfg->xcode.vid.pip.active && pCfg->xcode.aud.common.cfgDo_xcode &&
      IS_XC_CODEC_TYPE_AUD_RAW(pCfg->xcode.aud.common.cfgFileTypeOut) &&
      (!pCfg->pip.pMixerCfg || !pCfg->pip.pMixerCfg->conferenceInputDriver)) {
    pktzInitFrbufA.ppPktzFrBufOut = &pCfg->pRawOutCfg->pAudFrBuf;
    pktzInitFrbufA.va.codecType = pCfg->xcode.aud.common.cfgFileTypeOut;
    pktzInitFrbufA.va.isvideo = 0;
    pktzInitFrbufA.va.clockRateHz = pCfg->xcode.aud.cfgSampleRateOut;
    pktzInitFrbufA.va.u.a.channels = pCfg->xcode.aud.cfgChannelsOut;
    pktzInitFrbufA.va.u.a.samplesbufdurationms = pCfg->audsamplesbufdurationms;
    stream_tofrbuf = 1;
  }

  //
  // Check if output is a local file
  //
  if(!is_pip && pCfg->pdestsCfg[0].dstHost[0] != '\0' && pCfg->pdestsCfg[0].ports[0] == 0) {
#if !defined(ENABLE_RECORDING)
    LOG(X_ERROR(VSX_RECORDING_DISABLED_BANNER));
    LOG(X_ERROR(VSX_FEATURE_DISABLED_BANNER));
    return stream_av_onexit(pCfg, pCodecs, -1);
#else // (ENABLE_RECORDING)
    LOG(X_INFO("Will write output to file '%s'"), pCfg->pdestsCfg[0].dstHost);
    stream_tofile = 1;
#endif // (ENABLE_RECORDING)
  }

  if(STREAMER_DO_OUTPUT(pCfg->action) && pCfg->xcode.vid.common.cfgDo_xcode &&
     IS_XC_CODEC_TYPE_AUD_RAW(pCfg->xcode.vid.common.cfgFileTypeOut)) {
    LOG(X_ERROR("Server output requires a supported encoder output"));
    return stream_av_onexit(pCfg, pCodecs, -1);
  }

  //
  // Return if no adequate stream output is set
  //
  if((pCfg->action.do_output && (pCfg->numDests < 1 && 
      !pCfg->action.do_rtspannounce && !pCfg->action.do_rtmppublish)) || 
     pCfg->numDests > *pCfg->pmaxRtp ||
     (pCfg->xcode.vid.common.cfgFileTypeIn != XC_CODEC_TYPE_VID_CONFERENCE &&
      pCfg->xcode.aud.common.cfgFileTypeIn != XC_CODEC_TYPE_AUD_CONFERENCE &&
       !STREAMER_DO_OUTPUT(pCfg->action) && !pCfg->xcode.vid.pip.active && !stream_tofrbuf) ||
     (!pAv && (!pCodecs->pAac && !pCodecs->pAmr && !pCodecs->pSilk && !pCodecs->pOpus && !pCodecs->pVorbis && 
               !pCodecs->pConferenceA) && 
      (!pCodecs->pH264 && !pCodecs->pMpg4v && !pCodecs->pVp8 && !pCodecs->pImage && 
       !pCodecs->pConferenceV)) ||
     (pAv && pAv->numProg == 0)) {

    LOG(X_ERROR("No stream output set"));
    return stream_av_onexit(pCfg, pCodecs, -1);

  } else if(stream_tofile && !(pCfg->proto & STREAM_PROTO_MP2TS)) {

    LOG(X_ERROR("Ouput protocol not supported for direct file output"));
    return stream_av_onexit(pCfg, pCodecs, -1);
  }

  memset(&streamIn, 0, sizeof(streamIn));
  memset(streamsOutMp2ts, 0, sizeof(streamsOutMp2ts));
  memset(streamsOutRtp, 0, sizeof(streamsOutRtp));

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    //fprintf(stderr, "OUTIDX:%d\n", outidx);

    if(rtpmultiset_init(&pCfg->rtpMultisMp2ts[outidx], 1, 0, *pCfg->pmaxRtp) < 0 || 
       rtpmultiset_init(&pCfg->rtpMultisRtp[outidx][0], 2, pCfg->status.favoffsetrtcp, *pCfg->pmaxRtp) < 0) {
      return stream_av_onexit(pCfg, pCodecs, -1);
    }
    pCfg->rtpMultisMp2ts[outidx].outidx = outidx;
    pCfg->rtpMultisMp2ts[outidx].pStreamerCfg = pCfg;
    pCfg->rtpMultisMp2ts[outidx].outidx = outidx;
    pCfg->rtpMultisRtp[outidx][0].outidx = outidx;
    pCfg->rtpMultisRtp[outidx][0].pStreamerCfg = pCfg;
    pCfg->rtpMultisRtp[outidx][1].outidx = outidx;
    pCfg->rtpMultisRtp[outidx][1].pStreamerCfg = pCfg;
    pCfg->rtpMultisRtp[outidx][1].avidx = 1;

    pCfg->pRtpMultisRtp[outidx] = pCfg->rtpMultisRtp[outidx];

  }

  //
  // Form a linked list of all pCfg->rtpMultisRtp starting at pheadall
  //
  link_rtp_dest_list(pCfg->rtpMultisRtp);

  //
  // Use the same local server port for each RTP channel
  // this may be required for some firewalls (RTSP mobile clients)
  //
  if(pCfg->pRtspSessions && pCfg->pRtspSessions->staticLocalPort && 
    (pCfg->action.do_rtsplive || pCfg->action.do_rtspannounce)) {
    LOG(X_DEBUG("Using common static local RTP port %d"), pCfg->pRtspSessions->staticLocalPort);
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      pCfg->rtpMultisRtp[outidx][0].overlappingPorts = pCfg->rtpMultisRtp[outidx][1].overlappingPorts = 1;
    }
  }

  memset(pktzV, 0, sizeof(pktzV));
  memset(pktzA, 0, sizeof(pktzA));
  memset(&pktzsInitMp2ts, 0, sizeof(pktzsInitMp2ts));
  memset(&pktzsInitH264, 0, sizeof(pktzsInitH264));
  memset(&pktzsInitMpg4v, 0, sizeof(pktzsInitMpg4v));
  memset(&pktzsInitH263, 0, sizeof(pktzsInitH263));
  memset(&pktzsInitAac, 0, sizeof(pktzsInitAac));
  memset(&pktzsInitAmr, 0, sizeof(pktzsInitAmr));
  memset(&pktzsInitPcm, 0, sizeof(pktzsInitPcm));
  frtcp_sr_intervalsec[0] = frtcp_sr_intervalsec[1] = pCfg->frtcp_sr_intervalsec;
  fir_recv_via_rtcp[0] = fir_recv_via_rtcp[1] = pCfg->fbReq.firCfg.fir_recv_via_rtcp;

  pCodecs->vs.streamAv.msDelayTx = pCfg->status.msDelayTx;
  if(pCodecs->vs.streamAv.msDelayTx > 0) {
    LOG(X_DEBUG("Setting streaming delay %u ms"), pCodecs->vs.streamAv.msDelayTx);
  }

  //
  // Ensure that if video port is present, audio port is not 0 
  // 
  for(idx = 0; idx < *pCfg->pmaxRtp; idx++) {
    if(pCfg->pdestsCfg[idx].numPorts == 1) {
      pCfg->pdestsCfg[idx].ports[1] = pCfg->pdestsCfg[idx].ports[0];
      pCfg->pdestsCfg[idx].numPorts = 2;
    }
  }

  //
  // Check if restreaming a live stream
  //
  if(pAv) {

    if((pCodecs->pAac || pCodecs->pAmr || pCodecs->pSilk || pCodecs->pOpus || pCodecs->pVorbis || 
        pCodecs->pConferenceA) || 
       (pCodecs->pH264 || pCodecs->pMpg4v || pCodecs->pVp8 || pCodecs->pImage || pCodecs->pConferenceV)) {
      pCodecs->vs.streamAv.numProg = 0;
      pAv = NULL;
    }

    //durationLongest = pCodecs->vs.streamAv.lastHz /90000.0f;
    durationLongest = 0;
    //pCodecs->vs.streamAv.lastHz = 0;
  }

  pStreamAv = &pCodecs->vs.streamAv;

  //
  // Initialize output stream formatters
  //
  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    streamsOutMp2ts[outidx].pXmitAction = &pCfg->action;
    streamsOutMp2ts[outidx].pXmitCbs = &pCfg->cbs[outidx];
    streamsOutMp2ts[outidx].pXmitDestRc = pCfg->pxmitDestRc;
    streamsOutMp2ts[outidx].pLiveQ = &pCfg->liveQs[outidx];
    streamsOutMp2ts[outidx].pLiveQ2 = NULL;
    streamsOutMp2ts[outidx].verbosity = pCfg->verbosity;
    streamsOutMp2ts[outidx].poverwritefile = &pCfg->overwritefile;
    streamsOutMp2ts[outidx].pfrtcp_sr_intervalsec = &frtcp_sr_intervalsec[0];

    for(idx = 0; idx < 2; idx++) {
      memcpy(&streamsOutRtp[outidx][idx], &streamsOutMp2ts[outidx], sizeof(streamsOutRtp[outidx][idx]));
      streamsOutRtp[outidx][idx].idxProg = idx;

      //
      // Turn off any post-stream actions which should only be relevant for m2t
      //
      streamsOutRtp[outidx][idx].disableActions = 1;
      streamsOutRtp[outidx][idx].pLiveQ = NULL;
      streamsOutRtp[outidx][idx].pLiveQ2 = &pCfg->liveQ2s[outidx];
    }  

    streamsOutRtp[outidx][0].pfrtcp_sr_intervalsec = &frtcp_sr_intervalsec[1];
    streamsOutRtp[outidx][1].pfrtcp_sr_intervalsec = &frtcp_sr_intervalsec[1];

  }

  if(!pCfg->action.do_output_rtphdr) {
    frtcp_sr_intervalsec[0] = 0;
    fir_recv_via_rtcp[0] = 0;
  }

//pCfg->xcode.vid.common.cfgFileTypeOut = XC_CODEC_TYPE_MPEG4V; pCfg->xcode.aud.common.cfgFileTypeOut = XC_CODEC_TYPE_AMRNB;

  //
  // Need a better way to do this,
  // but the aac input decoder may only have ADTS headers available, which do not include
  // aac esds extensions
  //
  if(pCodecs->pAac) {
    //fprintf(stderr, "AAC FREQ:%dHz, haveFreqIdx:%d\n", esds_getFrequencyVal(ESDS_FREQ_IDX(pCodecs->pAac->decoderSP)), pCodecs->pAac->decoderSP.haveExtFreqIdx);
    memcpy(&XCODE_AUD_UDATA_PTR(&pCfg->xcode)->u.aac.descr.sp, &pCodecs->pAac->decoderSP, sizeof(ESDS_DECODER_CFG_T));
    XCODE_AUD_UDATA_PTR(&pCfg->xcode)->u.aac.descr.sp.init = 1;
  }

  //
  // Setup output packetizers
  // 
  if(!stream_tofile && 
    (((pCfg->proto & STREAM_PROTO_RTP) && pCfg->numDests > 0) || 
     pCfg->action.do_rtsplive || pCfg->action.do_rtspannounce)) {

    if((pCfg->action.do_rtsplive || pCfg->action.do_rtspannounce) && !pCfg->action.do_output_rtphdr) {

      if((pCfg->proto & STREAM_PROTO_RTP) && pCfg->pdestsCfg[0].dstHost[0] != '\0') {
        LOG(X_ERROR("Direct UDP (non RTP) packetization not supported with RTSP output"));
        return stream_av_onexit(pCfg, pCodecs, -1);
      }
      pCfg->action.do_output_rtphdr = 1;
    }

    //
    // Turn off rtcp if not using rtp based packetization
    //
    if(!pCfg->action.do_output_rtphdr) {
      frtcp_sr_intervalsec[1] = 0;
      fir_recv_via_rtcp[1] = 0;
    }

    idxProg = 0;
    if(!pCfg->novid) {

      if((pCfg->xcode.vid.common.cfgDo_xcode && 
          pCfg->xcode.vid.common.cfgFileTypeOut == XC_CODEC_TYPE_H264) ||
         (!pCfg->xcode.vid.common.cfgDo_xcode && 
          ((pCfg->sdpsx[1][0].vid.common.available &&
           pCfg->sdpsx[1][0].vid.common.codecType == XC_CODEC_TYPE_H264) ||
           pCodecs->pH264 || 
           (!pCfg->sdpsx[1][0].vid.common.available && 
            pAv && ((pCodecs->isInM2ts || pCodecs->vidType == XC_CODEC_TYPE_H264) 
            && (assume_vidfmt = 1))
          )))) {

        if(assume_vidfmt) {
          LOG(X_WARNING("Assuming input video contains elementary H.264 stream"));
        }

        if((rc = pktz_do_h264(pCodecs, pCfg, pCfg->rtpMultisRtp, streamsOutRtp,
                              frtcp_sr_intervalsec, fir_recv_via_rtcp, &idxProg, &pktz_do)) < 0) {
          return stream_av_onexit(pCfg, pCodecs, -1);
        } 

      } else if((pCfg->xcode.vid.common.cfgDo_xcode && 
                 pCfg->xcode.vid.common.cfgFileTypeOut == XC_CODEC_TYPE_MPEG4V) ||
        (!pCfg->xcode.vid.common.cfgDo_xcode && 
         ((pCfg->sdpsx[1][0].vid.common.available && 
           pCfg->sdpsx[1][0].vid.common.codecType == XC_CODEC_TYPE_MPEG4V) ||
          pCodecs->pMpg4v ))) {

        if((rc = pktz_do_mpeg4v(pCodecs, pCfg, pCfg->rtpMultisRtp, streamsOutRtp,
                       frtcp_sr_intervalsec, fir_recv_via_rtcp, &idxProg, &pktz_do, XC_CODEC_TYPE_MPEG4V)) < 0) {
          //stream_setstate(pCfg, STREAMER_STATE_ERROR, 1);
          return stream_av_onexit(pCfg, pCodecs, -1);
        } 

      } else if((pCfg->xcode.vid.common.cfgDo_xcode && 
                 (pCfg->xcode.vid.common.cfgFileTypeOut == XC_CODEC_TYPE_H263_PLUS ||
                 pCfg->xcode.vid.common.cfgFileTypeOut == XC_CODEC_TYPE_H263)) ||
        (!pCfg->xcode.vid.common.cfgDo_xcode && 
         ((pCfg->sdpsx[1][0].vid.common.available && 
           (pCfg->sdpsx[1][0].vid.common.codecType == XC_CODEC_TYPE_H263_PLUS ||
            pCfg->sdpsx[1][0].vid.common.codecType == XC_CODEC_TYPE_H263)) ||
          0))) {

        if((rc = pktz_do_h263(pCodecs, pCfg, pCfg->rtpMultisRtp, streamsOutRtp,
                       frtcp_sr_intervalsec, fir_recv_via_rtcp, &idxProg, &pktz_do, 
                 ((pCfg->xcode.vid.common.cfgDo_xcode &&
                   pCfg->xcode.vid.common.cfgFileTypeOut == XC_CODEC_TYPE_H263_PLUS) ||
                   pCfg->sdpsx[1][0].vid.common.codecType == XC_CODEC_TYPE_H263_PLUS) 
                       ? XC_CODEC_TYPE_H263_PLUS : XC_CODEC_TYPE_H263)) < 0) {
          return stream_av_onexit(pCfg, pCodecs, -1);
        } 

      } else if((pCfg->xcode.vid.common.cfgDo_xcode &&
                 pCfg->xcode.vid.common.cfgFileTypeOut == XC_CODEC_TYPE_VP8) ||
        (!pCfg->xcode.vid.common.cfgDo_xcode &&
         ((pCfg->sdpsx[1][0].vid.common.available &&
           pCfg->sdpsx[1][0].vid.common.codecType == XC_CODEC_TYPE_VP8) ||
          pCodecs->pVp8 ))) {

        if((rc = pktz_do_vp8(pCodecs, pCfg, pCfg->rtpMultisRtp, streamsOutRtp,
                       frtcp_sr_intervalsec, fir_recv_via_rtcp, &idxProg, &pktz_do)) < 0) {
          return stream_av_onexit(pCfg, pCodecs, -1);
        }
      }

    } // (!pCfg->novid...

    if(!pCfg->noaud) {

    if((pCfg->xcode.aud.common.cfgDo_xcode && 
        pCfg->xcode.aud.common.cfgFileTypeOut == XC_CODEC_TYPE_AAC) ||
       (!pCfg->xcode.aud.common.cfgDo_xcode && 
        ((pCfg->sdpsx[1][0].aud.common.available &&
         pCfg->sdpsx[1][0].aud.common.codecType == XC_CODEC_TYPE_AAC) ||
         pCodecs->pAac || 
         (!pCfg->sdpsx[1][0].aud.common.available &&
          pAv && ((pCodecs->isInM2ts || pCodecs->audType == XC_CODEC_TYPE_AAC)
          && (assume_audfmt = 1))
         )))) {

      if(assume_audfmt) {
        LOG(X_WARNING("Assuming input audio contains elementary AAC stream"));
      }

      if((rc = pktz_do_aac(pCodecs, pCfg, pCfg->rtpMultisRtp, streamsOutRtp,
                           frtcp_sr_intervalsec, fir_recv_via_rtcp, &idxProg, &pktz_do)) < 0) {
        return stream_av_onexit(pCfg, pCodecs, -1);
      } 

    } else if((pCfg->xcode.aud.common.cfgDo_xcode && 
              pCfg->xcode.aud.common.cfgFileTypeOut == XC_CODEC_TYPE_AMRNB) ||
      (!pCfg->xcode.aud.common.cfgDo_xcode &&
       ((pCfg->sdpsx[1][0].aud.common.available &&
         pCfg->sdpsx[1][0].aud.common.codecType == XC_CODEC_TYPE_AMRNB) ||
        pCodecs->pAmr))) {

      if((rc = pktz_do_amr(pCodecs, pCfg, pCfg->rtpMultisRtp, streamsOutRtp,
                           frtcp_sr_intervalsec, fir_recv_via_rtcp, &idxProg, &pktz_do)) < 0) {
        return stream_av_onexit(pCfg, pCodecs, -1);
      } 

    } else if((pCfg->xcode.aud.common.cfgDo_xcode && 
              pCfg->xcode.aud.common.cfgFileTypeOut == XC_CODEC_TYPE_SILK) ||
      (!pCfg->xcode.aud.common.cfgDo_xcode &&
       ((pCfg->sdpsx[1][0].aud.common.available &&
         pCfg->sdpsx[1][0].aud.common.codecType == XC_CODEC_TYPE_SILK) ||
        pCodecs->pSilk))) {

      if((rc = pktz_do_silk(pCodecs, pCfg, pCfg->rtpMultisRtp, streamsOutRtp,
                           frtcp_sr_intervalsec, fir_recv_via_rtcp, &idxProg, &pktz_do)) < 0) {
        return stream_av_onexit(pCfg, pCodecs, -1);
      } 

    } else if((pCfg->xcode.aud.common.cfgDo_xcode && 
              pCfg->xcode.aud.common.cfgFileTypeOut == XC_CODEC_TYPE_OPUS) ||
      (!pCfg->xcode.aud.common.cfgDo_xcode &&
       ((pCfg->sdpsx[1][0].aud.common.available &&
         pCfg->sdpsx[1][0].aud.common.codecType == XC_CODEC_TYPE_OPUS) ||
        pCodecs->pOpus))) {

      if((rc = pktz_do_opus(pCodecs, pCfg, pCfg->rtpMultisRtp, streamsOutRtp,
                           frtcp_sr_intervalsec, fir_recv_via_rtcp, &idxProg, &pktz_do)) < 0) {
        return stream_av_onexit(pCfg, pCodecs, -1);
      } 

    } else if((pCfg->xcode.aud.common.cfgDo_xcode && 
              (pCfg->xcode.aud.common.cfgFileTypeOut == XC_CODEC_TYPE_G711_MULAW ||
               pCfg->xcode.aud.common.cfgFileTypeOut == XC_CODEC_TYPE_G711_ALAW)) ||
      (!pCfg->xcode.aud.common.cfgDo_xcode &&
       ((pCfg->sdpsx[1][0].aud.common.available &&
         (pCfg->sdpsx[1][0].aud.common.codecType == XC_CODEC_TYPE_G711_MULAW ||
          pCfg->sdpsx[1][0].aud.common.codecType == XC_CODEC_TYPE_G711_ALAW))))) {


      if((rc = pktz_do_pcm(pCodecs, pCfg, pCfg->rtpMultisRtp, streamsOutRtp,
                           frtcp_sr_intervalsec, fir_recv_via_rtcp, &idxProg, &pktz_do)) < 0) {
        return stream_av_onexit(pCfg, pCodecs, -1);
      } 

    } 

    } else {
      //idxProg++;
    }

    if(!(pktz_do & PKTZ_MASK_VID) && !(pktz_do & PKTZ_MASK_AUD)) {
      if(!pCfg->xcode.aud.common.cfgDo_xcode || !pCfg->xcode.aud.common.cfgDo_xcode) {
        LOG(X_ERROR("Unable to set RTP %s output packetizer based on unknown input "
                    "codecs since output is not transcoded"),
                (!(pktz_do & PKTZ_MASK_VID) ? "video" : "audio"));
      } else {
        LOG(X_ERROR("No RTP packetizers set."));
      }
      return stream_av_onexit(pCfg, pCodecs, -1);
    }
  }

  if(((pCfg->proto & STREAM_PROTO_MP2TS) && pCfg->numDests > 0) || stream_tofile ||
     pCfg->action.do_tslive || pCfg->action.do_httplive) {

    pktz_do |= PKTZ_DO_MP2TS;

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
        continue;
      }

      if(stream_init_params(pCfg, &pCfg->rtpMultisMp2ts[outidx], 
                            MEDIA_FILE_TYPE_MP2TS, 90000, 0,
                            frtcp_sr_intervalsec[0], fir_recv_via_rtcp[0], idxProg) < 0) {
        return stream_av_onexit(pCfg, pCodecs, -1);
      }

      streamsOutMp2ts[outidx].pRtpMulti = &pCfg->rtpMultisMp2ts[outidx];

      //
      // Check if we are packetizing m2t only for the sake of avclive / httplive
      //
      if(!(pCfg->proto & STREAM_PROTO_MP2TS)) {
        pCfg->rtpMultisMp2ts[outidx].numDests = 0;
      } else {
        snprintf(streamsOutMp2ts[outidx].descr, sizeof(streamsOutMp2ts[outidx].descr), 
               "%s "STREAM_RTP_DESCRIPTION_MP2TS"%s",
        streamsOutMp2ts[outidx].pXmitAction->do_output_rtphdr ? "RTP" : "UDP",
        output_descr(str, sizeof(str), &pCfg->rtpMultisMp2ts[outidx], &streamsOutMp2ts[outidx]));
      }

    } // end of for(outidx...

  }

  if(stream_tofrbuf) {
    if(IS_XC_CODEC_TYPE_VID_RAW(pCfg->xcode.vid.common.cfgFileTypeOut)) {
      pktz_do |= PKTZ_DO_FRBUFV;
    }
    if(IS_XC_CODEC_TYPE_AUD_RAW(pCfg->xcode.aud.common.cfgFileTypeOut)) {
      pktz_do |= PKTZ_DO_FRBUFA;
    }

  }

  if(!pCfg->novid) {
  //
  // Check and setup any stream_net based inputs
  //
  if(pCodecs->pH264) {

    if((pktz_do & PKTZ_DO_H264)) {
      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
        memcpy(pCfg->sdpsx[1][outidx].vid.u.h264.profile_level_id, &pCodecs->pH264->spspps.sps_buf[1], 3);
        memcpy(&pCfg->sdpsx[1][outidx].vid.u.h264.spspps, &pCodecs->pH264->spspps, sizeof(SPSPPS_RAW_T));
      }
    }

    pCodecs->pNetH264 = &pCodecs->vn.netH264;
    pCodecs->pNetH264->pH264 = pCodecs->pH264;
    pCodecs->pNetH264->includeAnnexBHdr = 1;
    pCodecs->pNetH264->includeSeqHdrs = pCfg->cfgstream.includeSeqHdrs;

    pProg = &pStreamAv->progs[pStreamAv->numProg];
    pProg->pCbData = pCodecs->pNetH264;
    if(pProg->streamType == 0 || 
       pProg->streamType == MP2PES_STREAMTYPE_NOTSET) { 
      // Only change the output program if it has not been 
      //previously set for transcoding 
      pProg->streamType = MP2PES_STREAM_VIDEO_H264;
    }
    pProg->streamId = MP2PES_STREAMID_VIDEO_START;
    pProg->haveDts = 1;
    pProg->pXcodeData->inStreamType = MP2PES_STREAM_VIDEO_H264;
    pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn = MEDIA_FILE_TYPE_H264b;
    stream_net_h264_init(&pProg->stream);

    if((duration = (double)pCodecs->pH264->lastFrameId /
       ((double)pCodecs->pH264->clockHz / pCodecs->pH264->frameDeltaHz)) > durationLongest) {
      durationLongest = duration;
    }

    if(pCfg->status.useStatus) {
      pCfg->status.vidProps[0].mediaType = XC_CODEC_TYPE_H264;
      pCfg->status.vidProps[0].resH = pCodecs->pH264->frameWidthPx;
      pCfg->status.vidProps[0].resV = pCodecs->pH264->frameHeightPx;
      if(pCodecs->pH264->frameDeltaHz) {
        pCfg->status.vidProps[0].fps = (float)pCodecs->pH264->clockHz / pCodecs->pH264->frameDeltaHz;
      }
    }

    pStreamAv->numProg++;

  } else if(pCodecs->pMpg4v) {

    if((pktz_do & PKTZ_DO_MPEG4V)) {
      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
        memcpy(&pCfg->sdpsx[1][outidx].vid.u.mpg4v.seqHdrs, &pCodecs->pMpg4v->seqHdrs, sizeof(MPG4V_SEQ_HDRS_T));
      }
    }

    pCodecs->pNetMpg4v = &pCodecs->vn.netMpg4v;
    pCodecs->pNetMpg4v->pMpg4v = pCodecs->pMpg4v;
    pCodecs->pNetMpg4v->includeSeqHdrs = pCfg->cfgstream.includeSeqHdrs;

    pProg = &pStreamAv->progs[pStreamAv->numProg];
    pProg->pCbData = pCodecs->pNetMpg4v;
    if(pProg->streamType == 0 || 
       pProg->streamType == MP2PES_STREAMTYPE_NOTSET) { 
      pProg->streamType = MP2PES_STREAM_VIDEO_MP4V;
    }
    pProg->streamId = MP2PES_STREAMID_VIDEO_START;
    pProg->haveDts = 1;
    pProg->pXcodeData->inStreamType = MP2PES_STREAM_VIDEO_MP4V;
    pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn = MEDIA_FILE_TYPE_MP4V;
    stream_net_mpg4v_init(&pProg->stream);
    if((duration = (double)pCodecs->pMpg4v->lastFrameId /
       ((double)pCodecs->pMpg4v->param.clockHz / 
         pCodecs->pMpg4v->param.frameDeltaHz)) > durationLongest) {
      durationLongest = duration;
    }

    if(pCfg->status.useStatus) {
      pCfg->status.vidProps[0].mediaType = XC_CODEC_TYPE_MPEG4V;
      pCfg->status.vidProps[0].resH = pCodecs->pMpg4v->param.frameWidthPx;
      pCfg->status.vidProps[0].resV = pCodecs->pMpg4v->param.frameHeightPx;
      if(pCodecs->pMpg4v->param.frameDeltaHz) {
        pCfg->status.vidProps[0].fps = (float)pCodecs->pMpg4v->param.clockHz / 
                                     pCodecs->pMpg4v->param.frameDeltaHz;
      }

    }
    pStreamAv->numProg++;

  } else if(pCodecs->pVp8) {

    if((pktz_do & PKTZ_DO_VP8)) {
      //for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      //  memcpy(&pCfg->sdpsx[1][outidx].vid.u.mpg4v.seqHdrs, &pCodecs->pMpg4v->seqHdrs, sizeof(MPG4V_SEQ_HDRS_T));
      //}
    }

    pCodecs->pNetVp8 = &pCodecs->vn.netVp8;
    pCodecs->pNetVp8->pVp8 = pCodecs->pVp8;

    pProg = &pStreamAv->progs[pStreamAv->numProg];
    pProg->pCbData = pCodecs->pNetVp8;
    if(pProg->streamType == 0 || 
       pProg->streamType == MP2PES_STREAMTYPE_NOTSET) { 
        pProg->streamType = MP2PES_STREAM_PRIV_START;
    }
    pProg->streamId = MP2PES_STREAMID_VIDEO_START;
    pProg->haveDts = 1;
    pProg->pXcodeData->inStreamType = MP2PES_STREAM_PRIV_START;
    pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn = XC_CODEC_TYPE_VP8;
    stream_net_vp8_init(&pProg->stream);
    //if((duration = (double)pCodecs->pMpg4v->lastFrameId /
    //   ((double)pCodecs->pMpg4v->param.clockHz / 
    //     pCodecs->pMpg4v->param.frameDeltaHz)) > durationLongest) {
    //  durationLongest = duration;
    //}

    //
    // VP8 does not have codec priv date, widthxheight is available from the first 
    // keyframe in the bitstream
    //
    if(pCfg->status.useStatus) {
      pCfg->status.vidProps[0].mediaType = XC_CODEC_TYPE_VP8;
      pCfg->status.vidProps[0].resH = pCodecs->pVp8->frameWidthPx;
      pCfg->status.vidProps[0].resV = pCodecs->pVp8->frameHeightPx;
      if(pCodecs->pVp8->frameDeltaHz) {
        pCfg->status.vidProps[0].fps = (float)pCodecs->pVp8->clockHz / 
                                     pCodecs->pVp8->frameDeltaHz;
      }

    }
    pStreamAv->numProg++;

  } else if(pCodecs->pImage) {

    if(!pCfg->xcode.vid.common.cfgDo_xcode) {
      LOG(X_ERROR("Image streaming requires xcode options"));
      return stream_av_onexit(pCfg, pCodecs, -1);
//    } else if(!pCfg->xcode.vid.pip.active) {
//      LOG(X_ERROR("Image output only enabled for PIP"));
//      return stream_av_onexit(pCfg, pCodecs, -1);
    }

    if(!pCfg->xcode.vid.pip.active && pCfg->xcode.vid.cfgOutClockHz > 0 && pCfg->xcode.vid.cfgOutFrameDeltaHz > 0) {
      pCodecs->vn.netImage.clockHz = pCfg->xcode.vid.cfgOutClockHz;
      pCodecs->vn.netImage.frameDeltaHz = pCfg->xcode.vid.cfgOutFrameDeltaHz;
    } else if(!pCfg->xcode.vid.pip.active) {
      LOG(X_ERROR("Image output for main overlay requires xcode frame rate to be set"));
      return stream_av_onexit(pCfg, pCodecs, -1);
    }

    pCodecs->pNetImage = &pCodecs->vn.netImage;
    pCodecs->pNetImage->pImage = pCodecs->pImage;

    pProg = &pStreamAv->progs[pStreamAv->numProg];
    pProg->pCbData = pCodecs->pNetImage;
    pProg->streamType = MP2PES_STREAM_PRIV_START;
    pProg->pXcodeData->inStreamType = MP2PES_STREAM_PRIV_START;
    pProg->streamId = MP2PES_STREAMID_PRIV_START;
    pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn = pCodecs->pImage->mediaType;
    pProg->pXcodeData->piXcode->vid.inH = pCodecs->pImage->width; 
    pProg->pXcodeData->piXcode->vid.inV = pCodecs->pImage->height; 
    stream_net_image_init(&pProg->stream);

    if(pProg->pXcodeData->piXcode->vid.cfgOutClockHz == 0 || 
       pProg->pXcodeData->piXcode->vid.cfgOutFrameDeltaHz == 0) {
      pProg->pXcodeData->piXcode->vid.cfgOutClockHz = 1;
      pProg->pXcodeData->piXcode->vid.cfgOutFrameDeltaHz = 1;
    }

//fprintf(stderr, "IMAGE XCODE CODEC:%d->%d\n", pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn,pProg->pXcodeData->piXcode->vid.common.cfgFileTypeOut);


    pStreamAv->numProg++;

  } else if(pCodecs->pConferenceV) {

    if(pCfg->xcode.vid.cfgOutClockHz == 0 ||
       pCfg->xcode.vid.cfgOutFrameDeltaHz == 0) {
      LOG(X_ERROR("No configured output frame rate set"));
      return stream_av_onexit(pCfg, pCodecs, -1);
    }

    pCodecs->vn.netConferenceV.clockHz = pCfg->xcode.vid.cfgOutClockHz;
    pCodecs->vn.netConferenceV.frameDeltaHz = pCfg->xcode.vid.cfgOutFrameDeltaHz;
    pCodecs->vn.netConferenceV.pCfgOverlay = pCfg;
    pCodecs->vn.netConferenceV.pConferenceA = pCodecs->pNetConferenceA;
    pCodecs->pNetConferenceV = &pCodecs->vn.netConferenceV;
    pCodecs->pNetConferenceV->id = STREAM_VCONFERENCE_ID;
    pProg = &pStreamAv->progs[pStreamAv->numProg];
    pProg->pCbData = pCodecs->pNetConferenceV;
    pProg->streamId = MP2PES_STREAMID_VIDEO_START;
    pProg->haveDts = 1;

    pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn = XC_CODEC_TYPE_VID_CONFERENCE;
    stream_net_vconference_init(&pProg->stream);
    pCodecs->vs.streamAv.useInputTiming = 1;

    pStreamAv->numProg++;

  }
  }

  if(!pCfg->noaud) {
  if(pCodecs->pAac) {

    if((pktz_do & PKTZ_DO_AAC)) {
      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
        pCfg->sdpsx[1][outidx].aud.u.aac.profile_level_id = 1;
        pCfg->sdpsx[1][outidx].aud.channels = pCodecs->pAac->decoderSP.channelIdx;
        memcpy(&pCfg->sdpsx[1][outidx].aud.u.aac.decoderCfg, &pCodecs->pAac->decoderSP, 
             sizeof(pCfg->sdpsx[1][outidx].aud.u.aac.decoderCfg));
        esds_encodeDecoderSp(pCfg->sdpsx[1][outidx].aud.u.aac.config, 
                             sizeof(pCfg->sdpsx[1][0].aud.u.aac.config), &pCodecs->pAac->decoderSP);
      }
    }

    pCodecs->pNetAac = &pCodecs->an.netAac;
    memcpy(&pCodecs->pNetAac->aac, pCodecs->pAac, sizeof(AAC_DESCR_T));

    pProg = &pStreamAv->progs[pStreamAv->numProg];
    pProg->pCbData = pCodecs->pNetAac;
    if(pProg->streamType == 0 ||
       pProg->streamType == MP2PES_STREAMTYPE_NOTSET) { 
      pProg->streamType = MP2PES_STREAM_AUDIO_AACADTS;
    }
    pProg->streamId = MP2PES_STREAMID_AUDIO_START;
    pProg->pXcodeData->inStreamType = MP2PES_STREAM_AUDIO_AACADTS;
    pProg->pXcodeData->piXcode->aud.common.cfgFileTypeIn = XC_CODEC_TYPE_AAC;
    stream_net_aac_init(&pProg->stream);
    if((duration = (double)pCodecs->pAac->lastFrameId / 
      ((double)pCodecs->pAac->clockHz/pCodecs->pAac->decoderSP.frameDeltaHz)) > durationLongest) {
      durationLongest = duration;
    }

    pStreamAv->numProg++;

  } else if(pCodecs->pAmr) {

    if((pktz_do & PKTZ_DO_AMR)) {

    }

    pCodecs->pNetAmr = &pCodecs->an.netAmr;
    pCodecs->pNetAmr->pAmr  = pCodecs->pAmr;

    pProg = &pStreamAv->progs[pStreamAv->numProg];
    pProg->pCbData = pCodecs->pNetAmr;

    //
    //TODO: configure amr mp2ts program stream type id
    //
    if(pProg->streamType == 0 ||
       pProg->streamType == MP2PES_STREAMTYPE_NOTSET) { 
      pProg->streamType = MP2PES_STREAM_PRIV_START;
    }
    pProg->streamId = MP2PES_STREAMID_AUDIO_START;
    pProg->pXcodeData->inStreamType = MP2PES_STREAM_PRIV_START;
    pProg->pXcodeData->piXcode->aud.common.cfgFileTypeIn = XC_CODEC_TYPE_AMRNB;
    stream_net_amr_init(&pProg->stream);
    if((duration = (double)pCodecs->pAmr->totFrames / 
        ((double)pCodecs->pAmr->clockHz/pCodecs->pAmr->frameDurationHz)) > durationLongest) {
      durationLongest = duration;
    }
    pStreamAv->numProg++;

  } else if(pCodecs->pVorbis) {

    if((pktz_do & PKTZ_DO_VORBIS)) {

    }

    pCodecs->pNetVorbis = &pCodecs->an.netVorbis;
    pCodecs->pNetVorbis->pVorbis = pCodecs->pVorbis;

    pProg = &pStreamAv->progs[pStreamAv->numProg];
    pProg->pCbData = pCodecs->pNetVorbis;
    if(pProg->streamType == 0 ||
       pProg->streamType == MP2PES_STREAMTYPE_NOTSET) {
      pProg->streamType = MP2PES_STREAM_PRIV_START;
    }
    pProg->streamId = MP2PES_STREAMID_AUDIO_START;
    pProg->pXcodeData->inStreamType = MP2PES_STREAM_PRIV_START;
    pProg->pXcodeData->piXcode->aud.common.cfgFileTypeIn = XC_CODEC_TYPE_VORBIS;
    stream_net_vorbis_init(&pProg->stream);
    //if((duration = (double)pCodecs->pMpg4v->lastFrameId /
    //   ((double)pCodecs->pMpg4v->param.clockHz /
    //     pCodecs->pMpg4v->param.frameDeltaHz)) > durationLongest) {
    //  durationLongest = duration;
    //}
    if(pCfg->xcode.aud.common.cfgDo_xcode) {
      XCODE_AUD_UDATA_PTR(&pCfg->xcode)->u.vorbis.phdr = pCodecs->pVorbis->codecPrivData;
      XCODE_AUD_UDATA_PTR(&pCfg->xcode)->u.vorbis.hdrLen = pCodecs->pVorbis->codecPrivLen;
    } else {
      //
      // This is bad because ixcode_close_aud will try to dealloc this ebml blob owned data if it is called
      //
      pCfg->xcode.aud.phdr = pCodecs->pVorbis->codecPrivData; 
      pCfg->xcode.aud.hdrLen = pCodecs->pVorbis->codecPrivLen; 
    }

    if(pCfg->status.useStatus) {
      pCfg->status.vidProps[0].mediaType = XC_CODEC_TYPE_VORBIS;
    }
    pStreamAv->numProg++;


  } else if(pCodecs->pConferenceA) {

    if(pCfg->xcode.aud.cfgSampleRateOut == 0) {
      LOG(X_ERROR("No configured output sample rate set"));
      return stream_av_onexit(pCfg, pCodecs, -1);
    }

    pCodecs->an.netConferenceA.clockHz = pCfg->xcode.aud.cfgSampleRateOut;
    pCodecs->an.netConferenceA.frameDeltaHz = pCodecs->an.netConferenceA.clockHz / (1000 / STREAM_ACONFERENCE_PTIME); 
    pCodecs->an.netConferenceA.pConferenceV = pCodecs->pNetConferenceV;

    pCodecs->pNetConferenceA = &pCodecs->an.netConferenceA;
    pCodecs->pNetConferenceA->id = STREAM_ACONFERENCE_ID;
    pProg = &pStreamAv->progs[pStreamAv->numProg];
    pProg->pCbData = pCodecs->pNetConferenceA;
    pProg->streamId = MP2PES_STREAMID_AUDIO_START;
    pProg->haveDts = 0;

    pCfg->xcode.aud.cfgSampleRateIn = pCfg->xcode.aud.cfgSampleRateOut;
    if((pCfg->xcode.aud.cfgChannelsIn = pCfg->xcode.aud.cfgChannelsOut) == 0) {   
      pCfg->xcode.aud.cfgChannelsIn = 1;
    }
    pProg->pXcodeData->piXcode->aud.common.cfgFileTypeIn = XC_CODEC_TYPE_AUD_CONFERENCE;
    stream_net_aconference_init(&pProg->stream);
    pCodecs->vs.streamAv.useInputTiming = 1;

    pStreamAv->numProg++;

  }
  }

  if(pCfg->status.useStatus) {
    if(pCfg->xcode.vid.common.cfgDo_xcode) {
      pCfg->status.vidProps[0].mediaType = pCfg->xcode.vid.common.cfgFileTypeOut;
    }
    for(outidx = 1; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      memcpy(&pCfg->status.vidProps[outidx], &pCfg->status.vidProps[0], sizeof(pCfg->status.vidProps[outidx]));
    }
    pStreamAv->pvidProp = &pCfg->status.vidProps[0];
  }

  // impose max pkt/sec throttling
  //pStreamAv->tmMinPktInterval = 500;

  if(pCfg->u.cfgmp2ts.useMp2tsContinuity) {
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      lastPatContIdx[outidx] = pCfg->u.cfgmp2ts.lastPatContIdx;
      lastPmtContIdx[outidx] = pCfg->u.cfgmp2ts.lastPmtContIdx;
    }
  }

  if((pktz_do & PKTZ_DO_MP2TS)) {

    // Enable the user specified reporting callback
    if(pCfg->pReportCfg) {
      pCfg->status.xmitBwDescr.pReportCfg = pCfg->pReportCfg;
    }

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
        continue;
      }

      pCodecs->pPktzsMp2ts[outidx] = &pCodecs->pktzsMp2ts[outidx];
      pktzsInitMp2ts[outidx].common.pXmitNode = &streamsOutMp2ts[outidx];
      pktzsInitMp2ts[outidx].common.pRtpMulti = &pCfg->rtpMultisMp2ts[outidx];
      pktzsInitMp2ts[outidx].common.xmitflushpkts = pCfg->u.cfgmp2ts.xmitflushpkts;
      if(outidx == 0 && pktzsInitMp2ts[outidx].common.xmitflushpkts > 0) {
        LOG(X_DEBUG("MPEG-2 TS minimum packet size set to %d"), 
          pktzsInitMp2ts[outidx].common.xmitflushpkts);
      }
      pktzsInitMp2ts[outidx].common.cbXmitPkt = stream_tofile ? streamxmit_writepkt : streamxmit_sendpkt;
      pktzsInitMp2ts[outidx].usePatDeltaHz = 1;
      pktzsInitMp2ts[outidx].plastPatContIdx = &lastPatContIdx[outidx];
      pktzsInitMp2ts[outidx].plastPmtContIdx = &lastPmtContIdx[outidx];
      pktzsInitMp2ts[outidx].firstProgId = pCfg->u.cfgmp2ts.firstProgId;
      pktzsInitMp2ts[outidx].maxPcrIntervalMs = pCfg->u.cfgmp2ts.maxPcrIntervalMs;
      pktzsInitMp2ts[outidx].maxPatIntervalMs = pCfg->u.cfgmp2ts.maxPatIntervalMs;
      if(outidx == 0 && (pCfg->u.cfgmp2ts.maxPcrIntervalMs != 0 || pCfg->u.cfgmp2ts.maxPatIntervalMs != 0)) {
        LOG(X_DEBUG("MPEG-2 TS PCR interval %u ms, PAT interval %u ms"),
          pCfg->u.cfgmp2ts.maxPcrIntervalMs, pCfg->u.cfgmp2ts.maxPatIntervalMs);
      }

      //
      // Enable AUD NAL insertion for each xcoded video frame
      //
      if(pCfg->action.do_httplive) {
        pktzsInitMp2ts[outidx].includeVidH264AUD = 1;
      }
  
      pktzV[numPacketizersV].isactive = 1;
      pktzV[numPacketizersV].pktz_do_mask = PKTZ_DO_MP2TS;
      pktzV[numPacketizersV].outidx = outidx;
      pktzV[numPacketizersV].pRtpMulti = pktzsInitMp2ts[outidx].common.pRtpMulti;
      pktzV[numPacketizersV].pCbData = pCodecs->pPktzsMp2ts[outidx];
      pktzV[numPacketizersV].pCbInitData = &pktzsInitMp2ts[outidx];
      pktzV[numPacketizersV].cbInit = stream_pktz_mp2ts_init;
      pktzV[numPacketizersV].cbReset = stream_pktz_mp2ts_reset;
      pktzV[numPacketizersV].cbClose = stream_pktz_mp2ts_close;
      pktzV[numPacketizersV].cbNewFrame = stream_pktz_mp2ts_addframe;
      memcpy(&pktzA[numPacketizersA], &pktzV[numPacketizersV], 
         sizeof(pktzA[numPacketizersA]));

      numPacketizersV++;
      numPacketizersA++;

    } // end of for(outidx... 

  }

  if(!stream_tofile && (pktz_do & (PKTZ_MASK_VID | PKTZ_MASK_AUD))) {

    // Enable the user specified reporting callback
    if(pCfg->pReportCfg) {
      xmitBwDescrs[0][0].pReportCfg = pCfg->pReportCfg;
      xmitBwDescrs[0][1].pReportCfg = pCfg->pReportCfg;
    }

    if((pktz_do & PKTZ_DO_H264)) {

      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
          continue;
        }

        pCodecs->pPktzsH264[outidx] = &pCodecs->vp.pktzsH264[outidx];
        //pktzsInitH264[outidx].common.outidx = outidx;
        pktzsInitH264[outidx].common.pXmitNode = &streamsOutRtp[outidx][0];
        pktzsInitH264[outidx].common.pRtpMulti = &pCfg->rtpMultisRtp[outidx][0];
        pktzsInitH264[outidx].common.xmitflushpkts = 0;
        pktzsInitH264[outidx].common.cbXmitPkt = stream_tofile ? streamxmit_writepkt : streamxmit_sendpkt;
        pktzsInitH264[outidx].pSdp = &pCfg->sdpsx[1][outidx].vid;

        pktzV[numPacketizersV].isactive = 1;
        pktzV[numPacketizersV].pktz_do_mask = PKTZ_DO_H264;
        pktzV[numPacketizersV].outidx = outidx;
        pktzV[numPacketizersV].pRtpMulti = pktzsInitH264[outidx].common.pRtpMulti;
        pktzV[numPacketizersV].pCbData = pCodecs->pPktzsH264[outidx];
        pktzV[numPacketizersV].pCbInitData = &pktzsInitH264[outidx];
        pktzV[numPacketizersV].cbInit = stream_pktz_h264_init;
        pktzV[numPacketizersV].cbReset = stream_pktz_h264_reset;
        pktzV[numPacketizersV].cbClose = stream_pktz_h264_close;
        pktzV[numPacketizersV].cbNewFrame = stream_pktz_h264_addframe;
        numPacketizersV++;
      }

    } else if((pktz_do & PKTZ_DO_MPEG4V)) {

      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
          continue;
        }

        pCodecs->pPktzsMpg4v[outidx] = &pCodecs->vp.pktzsMpg4v[outidx];
        //pktzsInitMpg4v[outidx].common.outidx = outidx;
        pktzsInitMpg4v[outidx].common.pXmitNode = &streamsOutRtp[outidx][0];
        pktzsInitMpg4v[outidx].common.pRtpMulti = &pCfg->rtpMultisRtp[outidx][0];
        pktzsInitMpg4v[outidx].common.xmitflushpkts = 0;
        pktzsInitMpg4v[outidx].common.cbXmitPkt = stream_tofile ? streamxmit_writepkt : streamxmit_sendpkt;
        pktzsInitMpg4v[outidx].pSdp = &pCfg->sdpsx[1][outidx].vid;

        pktzV[numPacketizersV].isactive = 1;
        pktzV[numPacketizersV].pktz_do_mask = PKTZ_DO_MPEG4V;
        pktzV[numPacketizersV].outidx = outidx;
        pktzV[numPacketizersV].pRtpMulti = pktzsInitMpg4v[outidx].common.pRtpMulti;
        pktzV[numPacketizersV].pCbData = pCodecs->pPktzsMpg4v[outidx];
        pktzV[numPacketizersV].pCbInitData = &pktzsInitMpg4v[outidx];
        pktzV[numPacketizersV].cbInit = stream_pktz_mpg4v_init;
        pktzV[numPacketizersV].cbReset = stream_pktz_mpg4v_reset;
        pktzV[numPacketizersV].cbClose = stream_pktz_mpg4v_close;
        pktzV[numPacketizersV].cbNewFrame = stream_pktz_mpg4v_addframe;
        numPacketizersV++;
      }

    } else if((pktz_do & PKTZ_DO_H263)) {

      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
          continue;
        }

        pCodecs->pPktzsH263[outidx] = &pCodecs->vp.pktzsH263[outidx];
        //pktzsInitH263[outidx].common.outidx = outidx;
        pktzsInitH263[outidx].common.pXmitNode = &streamsOutRtp[outidx][0];
        pktzsInitH263[outidx].common.pRtpMulti = &pCfg->rtpMultisRtp[outidx][0];
        pktzsInitH263[outidx].common.xmitflushpkts = 0;
        pktzsInitH263[outidx].common.cbXmitPkt = stream_tofile ? streamxmit_writepkt : streamxmit_sendpkt;
        pktzsInitH263[outidx].pSdp = &pCfg->sdpsx[1][outidx].vid;
        pktzsInitH263[outidx].codecType = pCfg->xcode.vid.common.cfgFileTypeOut;

        pktzV[numPacketizersV].isactive = 1;
        pktzV[numPacketizersV].pktz_do_mask = PKTZ_DO_H263;
        pktzV[numPacketizersV].outidx = outidx;
        pktzV[numPacketizersV].pRtpMulti = pktzsInitH263[outidx].common.pRtpMulti;
        pktzV[numPacketizersV].pCbData = pCodecs->pPktzsH263[outidx];
        pktzV[numPacketizersV].pCbInitData = &pktzsInitH263[outidx];
        pktzV[numPacketizersV].cbInit = stream_pktz_h263_init;
        pktzV[numPacketizersV].cbReset = stream_pktz_h263_reset;
        pktzV[numPacketizersV].cbClose = stream_pktz_h263_close;
        pktzV[numPacketizersV].cbNewFrame = stream_pktz_h263_addframe;
        numPacketizersV++;
      }

    } else if((pktz_do & PKTZ_DO_VP8)) {

      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
          continue;
        }

        pCodecs->pPktzsVp8[outidx] = &pCodecs->vp.pktzsVp8[outidx];
        //pktzsInitVp8[outidx].common.outidx = outidx;
        pktzsInitVp8[outidx].common.pXmitNode = &streamsOutRtp[outidx][0];
        pktzsInitVp8[outidx].common.pRtpMulti = &pCfg->rtpMultisRtp[outidx][0];
        pktzsInitVp8[outidx].common.xmitflushpkts = 0;
        pktzsInitVp8[outidx].common.cbXmitPkt = stream_tofile ? streamxmit_writepkt : streamxmit_sendpkt;
        pktzsInitVp8[outidx].pSdp = &pCfg->sdpsx[1][outidx].vid;

        pktzV[numPacketizersV].isactive = 1;
        pktzV[numPacketizersV].pktz_do_mask = PKTZ_DO_VP8;
        pktzV[numPacketizersV].outidx = outidx;
        pktzV[numPacketizersV].pRtpMulti = pktzsInitVp8[outidx].common.pRtpMulti;
        pktzV[numPacketizersV].pCbData = pCodecs->pPktzsVp8[outidx];
        pktzV[numPacketizersV].pCbInitData = &pktzsInitVp8[outidx];
        pktzV[numPacketizersV].cbInit = stream_pktz_vp8_init;
        pktzV[numPacketizersV].cbReset = stream_pktz_vp8_reset;
        pktzV[numPacketizersV].cbClose = stream_pktz_vp8_close;
        pktzV[numPacketizersV].cbNewFrame = stream_pktz_vp8_addframe;
        numPacketizersV++;
      }

    }

    if((pktz_do & PKTZ_DO_AAC)) {

      idxProg = (pStreamAv->numProg > 1 ? 1 : 0);

      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
          continue;
        }

        pCodecs->pPktzsAac[outidx] = &pCodecs->ap.pktzsAac[outidx];
        //pktzsInitAac[outidx].common.outidx = outidx;
        pktzsInitAac[outidx].common.pXmitNode = &streamsOutRtp[outidx][idxProg];
        pktzsInitAac[outidx].common.pRtpMulti = &pCfg->rtpMultisRtp[outidx][idxProg];
        pCfg->rtpMultisRtp[outidx][idxProg].isaud = 1;
        pktzsInitAac[outidx].common.xmitflushpkts = 0;
        pktzsInitAac[outidx].common.cbXmitPkt = stream_tofile ? streamxmit_writepkt : streamxmit_sendpkt;
        //if(pCfg->status.favoffsetrtp > 0) {
        //  pktzInitAac.common.favoffsetrtp = pCfg->status.favoffsetrtp;
        //}
        pktzsInitAac[outidx].pSdp = &pCfg->sdpsx[1][outidx].aud;

        pktzA[numPacketizersA].isactive = 1;
        pktzA[numPacketizersA].pktz_do_mask = PKTZ_DO_AAC;
        pktzA[numPacketizersA].outidx = outidx;
        pktzA[numPacketizersA].pRtpMulti = pktzsInitAac[outidx].common.pRtpMulti;
        pktzA[numPacketizersA].pCbData = pCodecs->pPktzsAac[outidx];
        pktzA[numPacketizersA].pCbInitData = &pktzsInitAac[outidx];
        pktzA[numPacketizersA].cbInit = stream_pktz_aac_init;
        pktzA[numPacketizersA].cbReset = stream_pktz_aac_reset;
        pktzA[numPacketizersA].cbClose = stream_pktz_aac_close;
        pktzA[numPacketizersA].cbNewFrame = stream_pktz_aac_addframe;
        numPacketizersA++;
      }

    } else if((pktz_do & PKTZ_DO_AMR)) {

      idxProg = (pStreamAv->numProg > 1 ? 1 : 0);

      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
          continue;
        }

        pCodecs->pPktzsAmr[outidx] = &pCodecs->ap.pktzsAmr[outidx];
        //pktzsInitAmr[outidx].common.outidx = outidx;
        pktzsInitAmr[outidx].common.pXmitNode = &streamsOutRtp[outidx][idxProg];
        pktzsInitAmr[outidx].common.pRtpMulti = &pCfg->rtpMultisRtp[outidx][idxProg];
        pCfg->rtpMultisRtp[outidx][idxProg].isaud = 1;
        pktzsInitAmr[outidx].common.xmitflushpkts = 0;
        pktzsInitAmr[outidx].common.cbXmitPkt = stream_tofile ? streamxmit_writepkt : streamxmit_sendpkt;
        //if(pCfg->status.favoffsetrtp > 0) {
        //  pktzInitAmr.common.favoffsetrtp = pCfg->status.favoffsetrtp;
        //}
        pktzsInitAmr[outidx].pSdp = &pCfg->sdpsx[1][outidx].aud;
        if(pCfg->cfgrtp.audioAggregatePktsMs > 0) {
          pktzsInitAmr[outidx].compoundSampleMs = pCfg->cfgrtp.audioAggregatePktsMs;
        } else {
          pktzsInitAmr[outidx].compoundSampleMs = AMR_COMPOUND_SAMPLE_MS;
        }

        pktzA[numPacketizersA].isactive = 1;
        pktzA[numPacketizersA].pktz_do_mask = PKTZ_DO_AMR;
        pktzA[numPacketizersA].outidx = outidx;
        pktzA[numPacketizersA].pRtpMulti = pktzsInitAmr[outidx].common.pRtpMulti;
        pktzA[numPacketizersA].pCbData = pCodecs->pPktzsAmr[outidx];
        pktzA[numPacketizersA].pCbInitData = &pktzsInitAmr[outidx];
        pktzA[numPacketizersA].cbInit = stream_pktz_amr_init;
        pktzA[numPacketizersA].cbReset = stream_pktz_amr_reset;
        pktzA[numPacketizersA].cbClose = stream_pktz_amr_close;
        pktzA[numPacketizersA].cbNewFrame = stream_pktz_amr_addframe;
        numPacketizersA++;
      }

    } else if((pktz_do & PKTZ_DO_SILK)) {

      idxProg = (pStreamAv->numProg > 1 ? 1 : 0);

      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
          continue;
        }

        pCodecs->pPktzsSilk[outidx] = &pCodecs->ap.pktzsSilk[outidx];
        pktzsInitSilk[outidx].common.pXmitNode = &streamsOutRtp[outidx][idxProg];
        pktzsInitSilk[outidx].common.pRtpMulti = &pCfg->rtpMultisRtp[outidx][idxProg];
        pCfg->rtpMultisRtp[outidx][idxProg].isaud = 1;
        //pktzsInitSilk[outidx].common.clockRateHz = 44100;
        //pktzsInitSilk[outidx].common.xmitflushpkts = 0;
        pktzsInitSilk[outidx].common.cbXmitPkt = stream_tofile ? streamxmit_writepkt : streamxmit_sendpkt;
        //if(pCfg->status.favoffsetrtp > 0) {
        //  pktzInitSilk.common.favoffsetrtp = pCfg->status.favoffsetrtp;
        //}
        pktzsInitSilk[outidx].pSdp = &pCfg->sdpsx[1][outidx].aud;
        //pktzsInitSilk[outidx].compoundSampleMs = 200;

        pktzA[numPacketizersA].isactive = 1;
        pktzA[numPacketizersA].pktz_do_mask = PKTZ_DO_SILK;
        pktzA[numPacketizersA].outidx = outidx;
        pktzA[numPacketizersA].pRtpMulti = pktzsInitSilk[outidx].common.pRtpMulti;
        pktzA[numPacketizersA].pCbData = pCodecs->pPktzsSilk[outidx];
        pktzA[numPacketizersA].pCbInitData = &pktzsInitSilk[outidx];
        pktzA[numPacketizersA].cbInit = stream_pktz_silk_init;
        pktzA[numPacketizersA].cbReset = stream_pktz_silk_reset;
        pktzA[numPacketizersA].cbClose = stream_pktz_silk_close;
        pktzA[numPacketizersA].cbNewFrame = stream_pktz_silk_addframe;
        numPacketizersA++;
      }

    } else if((pktz_do & PKTZ_DO_OPUS)) {

      idxProg = (pStreamAv->numProg > 1 ? 1 : 0);

      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
          continue;
        }

        pCodecs->pPktzsOpus[outidx] = &pCodecs->ap.pktzsOpus[outidx];
        pktzsInitOpus[outidx].common.pXmitNode = &streamsOutRtp[outidx][idxProg];
        pktzsInitOpus[outidx].common.pRtpMulti = &pCfg->rtpMultisRtp[outidx][idxProg];
        pCfg->rtpMultisRtp[outidx][idxProg].isaud = 1;
        //pktzsInitOpus[outidx].common.clockRateHz = 44100;
        //pktzsInitOpus[outidx].common.xmitflushpkts = 0;
        pktzsInitOpus[outidx].common.cbXmitPkt = stream_tofile ? streamxmit_writepkt : streamxmit_sendpkt;
        //if(pCfg->status.favoffsetrtp > 0) {
        //  pktzInitOpus.common.favoffsetrtp = pCfg->status.favoffsetrtp;
        //}
        pktzsInitOpus[outidx].pSdp = &pCfg->sdpsx[1][outidx].aud;
        //pktzsInitOpus[outidx].compoundSampleMs = 200;

        pktzA[numPacketizersA].isactive = 1;
        pktzA[numPacketizersA].pktz_do_mask = PKTZ_DO_OPUS;
        pktzA[numPacketizersA].outidx = outidx;
        pktzA[numPacketizersA].pRtpMulti = pktzsInitOpus[outidx].common.pRtpMulti;
        pktzA[numPacketizersA].pCbData = pCodecs->pPktzsOpus[outidx];
        pktzA[numPacketizersA].pCbInitData = &pktzsInitOpus[outidx];
        pktzA[numPacketizersA].cbInit = stream_pktz_opus_init;
        pktzA[numPacketizersA].cbReset = stream_pktz_opus_reset;
        pktzA[numPacketizersA].cbClose = stream_pktz_opus_close;
        pktzA[numPacketizersA].cbNewFrame = stream_pktz_opus_addframe;
        numPacketizersA++;
      }

    } else if((pktz_do & PKTZ_DO_G711)) {

      idxProg = (pStreamAv->numProg > 1 ? 1 : 0);

      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        if(outidx > 0 && !pCfg->xcode.vid.out[outidx].active) {
          continue;
        }

        pCodecs->pPktzsPcm[outidx] = &pCodecs->ap.pktzsPcm[outidx];
        //pktzsInitPcm[outidx].common.outidx = outidx;
        pktzsInitPcm[outidx].common.pXmitNode = &streamsOutRtp[outidx][idxProg];
        pktzsInitPcm[outidx].common.pRtpMulti = &pCfg->rtpMultisRtp[outidx][idxProg];
        pCfg->rtpMultisRtp[outidx][idxProg].isaud = 1;
        pktzsInitPcm[outidx].common.cbXmitPkt = stream_tofile ? streamxmit_writepkt : streamxmit_sendpkt;
        pktzsInitPcm[outidx].pSdp = &pCfg->sdpsx[1][outidx].aud;
        pktzsInitPcm[outidx].compoundSampleMs = 20;

        pktzA[numPacketizersA].isactive = 1;
        pktzA[numPacketizersA].pktz_do_mask = (pktz_do & PKTZ_DO_G711);
        pktzA[numPacketizersA].outidx = outidx;
        pktzA[numPacketizersA].pRtpMulti = pktzsInitPcm[outidx].common.pRtpMulti;
        pktzA[numPacketizersA].pCbData = pCodecs->pPktzsPcm[outidx];
        pktzA[numPacketizersA].pCbInitData = &pktzsInitPcm[outidx];
        pktzA[numPacketizersA].cbInit = stream_pktz_pcm_init;
        pktzA[numPacketizersA].cbReset = stream_pktz_pcm_reset;
        pktzA[numPacketizersA].cbClose = stream_pktz_pcm_close;
        pktzA[numPacketizersA].cbNewFrame = stream_pktz_pcm_addframe;
        numPacketizersA++;
      }

    } // end of else if((pktz_do & PKTZ_DO_G711)) {

  }

  if(stream_tofrbuf) {

    if((pktz_do & PKTZ_DO_FRBUFV)) {
      pCodecs->pPktzFrbufV = &pCodecs->pktzFrbufV;

      pktzV[numPacketizersV].isactive = 1;
      pktzV[numPacketizersV].pktz_do_mask = PKTZ_DO_FRBUFV;
      pktzV[numPacketizersV].pCbData = pCodecs->pPktzFrbufV;
      pktzV[numPacketizersV].pCbInitData = &pktzInitFrbufV;
      pktzV[numPacketizersV].cbInit = stream_pktz_frbuf_init;
      pktzV[numPacketizersV].cbReset = stream_pktz_frbuf_reset;
      pktzV[numPacketizersV].cbClose = stream_pktz_frbuf_close;
      pktzV[numPacketizersV].cbNewFrame = stream_pktz_frbuf_addframe;
      numPacketizersV++;
    }
    if((pktz_do & PKTZ_DO_FRBUFA)) {
      idxProg = (pStreamAv->numProg > 1 ? 1 : 0);

      pCodecs->pPktzFrbufA = &pCodecs->pktzFrbufA;
      pktzA[numPacketizersA].isactive = 1;
      pktzA[numPacketizersA].pktz_do_mask = PKTZ_DO_FRBUFA;
      pktzA[numPacketizersA].pCbData = pCodecs->pPktzFrbufA;
      pktzA[numPacketizersA].pCbInitData = &pktzInitFrbufA;
      pktzA[numPacketizersA].cbInit = stream_pktz_frbuf_init;
      pktzA[numPacketizersA].cbReset = stream_pktz_frbuf_reset;
      pktzA[numPacketizersA].cbClose = stream_pktz_frbuf_close;
      pktzA[numPacketizersA].cbNewFrame = stream_pktz_frbuf_addframe;
      numPacketizersA++;
    }

  }

  numPacketizers = MAX(numPacketizersA, numPacketizersV);

  if(pktz_do != 0) {
    LOG(X_DEBUG("Creating output (%d prog) packetizers: %s%s%s%s%s%s%s%s%s%s"), 
          //pStreamAv->numProg,
          numPacketizers,
          ((pktz_do & PKTZ_DO_MP2TS) ? STREAM_RTP_DESCRIPTION_MP2TS" " : ""), 
          //((pktz_do & PKTZ_DO_FLV) ? STREAM_RTP_DESCRIPTION_FLV" " : ""), 
          ((pktz_do & PKTZ_DO_H264) ? STREAM_RTP_DESCRIPTION_H264" " : ""), 
          ((pktz_do & PKTZ_DO_MPEG4V) ? STREAM_RTP_DESCRIPTION_MPG4V" " : ""), 
          ((pktz_do & PKTZ_DO_H263) ? STREAM_RTP_DESCRIPTION_H263" " : ""), 
          ((pktz_do & PKTZ_DO_AAC) ? STREAM_RTP_DESCRIPTION_AAC" " : ""), 
          ((pktz_do & PKTZ_DO_AMR) ? STREAM_RTP_DESCRIPTION_AMR" " : ""),
          ((pktz_do & PKTZ_DO_SILK) ? STREAM_RTP_DESCRIPTION_SILK" " : ""),
          ((pktz_do & PKTZ_DO_OPUS) ? STREAM_RTP_DESCRIPTION_OPUS" " : ""),
          ((pktz_do & PKTZ_DO_G711_ALAW) ? STREAM_RTP_DESCRIPTION_G711_ALAW " " : ""),
          ((pktz_do & PKTZ_DO_G711_MULAW) ? STREAM_RTP_DESCRIPTION_G711_MULAW " " : ""));
  }

  if(stream_tofrbuf) {
    LOG(X_DEBUG("Output to local framebuffer interface %s%s"),
          ((pktz_do & PKTZ_DO_FRBUFV) ? "vid " : ""),
          ((pktz_do & PKTZ_DO_FRBUFA) ? "aud " : ""));
  }

  if(!pCfg->novid) {
    show_rtcpfb_config(pCfg, 
                       pCfg->sharedCtxt.active, 
                       (pCfg->action.do_output && pCfg->action.do_output_rtphdr ? 1 : 0),
                       (pCfg->sharedCtxt.active && BOOL_ISENABLED(pCfg->fbReq.nackRtcpSend) ? 1 : 0),
                        pCfg->fbReq.nackRtpRetransmit,
                       (pCfg->sharedCtxt.active && BOOL_ISENABLED(pCfg->fbReq.apprembRtcpSend) ? 1 : 0));
  }

  //
  // Associate any liveFmt frame receivers
  //
  if(pCfg->action.do_rtmplive || pCfg->action.do_rtmppublish || pCfg->action.do_flvlive ||
     pCfg->action.do_flvliverecord || pCfg->action.do_moofliverecord || 
     pCfg->action.do_mkvlive || pCfg->action.do_mkvliverecord) {

    pCodecs->vs.streamAv.progs[0].pLiveFmts = &pCfg->action.liveFmts;
    pCodecs->vs.streamAv.progs[1].pLiveFmts = &pCfg->action.liveFmts;

    //
    // Init the stream output async frame callback mechanism.
    // The size of the a/v ordering queue is quite arbitrary
    //
    if(outfmt_init(&pCfg->action.liveFmts, 
                   (pCodecs->pConferenceV && pCodecs->pConferenceA &&
                    pCfg->xcode.vid.common.cfgDo_xcode && !pCfg->novid && !pCfg->noaud ? 200 : 0)) < 0) {
                   //   200) < 0) {
      return stream_av_onexit(pCfg, pCodecs, -1);
    }

  }

  if(stream_av_init(pStreamAv, pktzV, pktzA, numPacketizers) < 0) {
    stream_setstate(pCfg, STREAMER_STATE_ERROR, 1);
    return stream_av_onexit(pCfg, pCodecs, -1);
  }

  streamIn.prunning = &pCfg->running;
  streamIn.verbosity = pCfg->verbosity;
  streamIn.pLic = &pCfg->lic;
  streamIn.pRtpMulti = pCfg->rtpMultisRtp[0][0].pheadall; // TODO: what about mp2t packetizers (for dtls handhsake initiation from streamer2)
  streamIn.pdtlsTimeouts =  &pCfg->cfgrtp.dtlsTimeouts;

  init_readframes_av(&streamIn, pStreamAv);

  //
  // Write to local output file
  //
  if(stream_tofile) {
    //pthread_mutex_lock(&pCfg->mtxStrmr);
    stream_setstate(pCfg, STREAMER_STATE_RUNNING, 0);
    //pCfg->jump.type = JUMP_OFFSET_TYPE_NONE;
    //pCfg->pauseRequested = 0;
    //pthread_mutex_unlock(&pCfg->mtxStrmr);

    rc = streamer_run_tofile(&streamIn, &streamsOutMp2ts[0], pCfg->pdestsCfg[0].dstHost);
    stream_setstate(pCfg, STREAMER_STATE_FINISHED, 1);
    return stream_av_onexit(pCfg, pCodecs, 0);
  }

  if((pCfg->proto & STREAM_PROTO_MP2TS)) {

    if(streamer_add_recipients(pCfg, pCfg->rtpMultisMp2ts, 1) < 0) {
      return stream_av_onexit(pCfg, pCodecs, -1);
    }

    if(pktz_do == PKTZ_DO_MP2TS) {
      // Avoid resetting sdp if any RTP packetizers are set
      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        pDest = pCfg->pdestsCfg[outidx].dstHost[0] ?  &pCfg->pdestsCfg[outidx] : &pCfg->pdestsCfg[0];

        if(!pCfg->action.do_output_rtphdr) {
           pCfg->sdpsx[0][outidx].vid.common.transType = SDP_TRANS_TYPE_RAW_UDP;
        }

        sdputil_init(&pCfg->sdpsx[0][outidx], 
                    streamsOutMp2ts[0].pRtpMulti->init.pt, 
                    streamsOutMp2ts[0].pRtpMulti->init.clockRateHz, 
                    MEDIA_FILE_TYPE_MP2TS,
                    pDest->dstHost, pDest->ports[0], pDest->portsRtcp[0], 
                    pDest->srtps[0].kt.k.lenKey > 0 ? &pDest->srtps[0] : NULL,
                    pDest->dtlsCfg.active ? &pDest->dtlsCfg : NULL,
                    pDest->stunCfgs[0].cfg.bindingRequest ? &pDest->stunCfgs[0].cfg : NULL,
                    NULL, NULL, &pCfg->fbReq);
      }
    }

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      for(idxDest = 0; idxDest < pCfg->numDests; idxDest++) {
        if(streamsOutMp2ts[outidx].pRtpMulti &&
           streamsOutMp2ts[outidx].pRtpMulti->pdests[idxDest].isactive) {

          LOG(X_INFO("%s"), output_descr_stream(str, sizeof(str), 
                            &streamsOutMp2ts[outidx].pRtpMulti->pdests[idxDest], 
                            &streamsOutMp2ts[outidx]));

        }
      }
    }

  }

  if((pCfg->proto & STREAM_PROTO_RTP)) {

    if(streamer_add_recipients(pCfg, (STREAM_RTP_MULTI_T *) pCfg->rtpMultisRtp, 
       (pCfg->novid || pCfg->noaud ? 1 : 2)) < 0) {
      return stream_av_onexit(pCfg, pCodecs, -1);
    }

    //
    // After all RTP streams have been added, set the post-dtls capture-side handshake callback invocation
    // to enable calling the streamer-side handshake callback.  This is done here since if we're using a single capture port
    // for audio/video mux, we want to invoke the stream-side callback for each stream to assign srtp keys.
    //
    pCfg->sharedCtxt.av[0].dtlsKeyUpdateStorages[0].readyForCb = pCfg->sharedCtxt.av[0].dtlsKeyUpdateStorages[1].readyForCb = 1;
    pCfg->sharedCtxt.av[1].dtlsKeyUpdateStorages[0].readyForCb = pCfg->sharedCtxt.av[1].dtlsKeyUpdateStorages[1].readyForCb = 1;

    //check_audiovideo_mux(pCfg->rtpMultisRtp);


    //
    // If there is any duplication among RTP / RTCP ports we should use
    // the linked list of all possible ports as the search for the RTP
    // stream corresponding to a received RTCP message within stream_rtcp.c 
    // RTCP receiver
    //
    if(!pCfg->rtpMultisRtp[0][0].pheadall && check_duplicate_rtcp(pCfg->rtpMultisRtp)) {
      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
        pCfg->rtpMultisRtp[outidx][0].overlappingPorts = pCfg->rtpMultisRtp[outidx][1].overlappingPorts = 1;
      }
    }

 //fprintf(stderr, "NUMD:%d\n", pCfg->numDests); 
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      for(idxDest = 0; idxDest < pCfg->numDests; idxDest++) {
        for(idx = 0; idx < pStreamAv->numProg; idx++) { 
          if(streamsOutRtp[outidx][idx].pRtpMulti &&
            streamsOutRtp[outidx][idx].pRtpMulti->pdests[idxDest].isactive) {

/*
            str[0] = '\0';
            if(streamsOutRtp[outidx][idx].pXmitAction->do_output_rtphdr && 
               ntohs(streamsOutRtp[outidx][idx].pRtpMulti->pdests[idxDest].saDstsRtcp.sin_port) > 0 &&
               RTCP_PORT_FROM_RTP(ntohs(streamsOutRtp[outidx][idx].pRtpMulti->pdests[idxDest].saDsts.sin_port))
               != ntohs(streamsOutRtp[outidx][idx].pRtpMulti->pdests[idxDest].saDstsRtcp.sin_port)) {
              snprintf(str, sizeof(str), ", rtcp-port:%d", 
                 ntohs(streamsOutRtp[outidx][idx].pRtpMulti->pdests[idxDest].saDstsRtcp.sin_port));
            }
*/

            LOG(X_INFO("%s"), output_descr_stream(str, sizeof(str), 
                                &streamsOutRtp[outidx][idx].pRtpMulti->pdests[idxDest], 
                                &streamsOutRtp[outidx][idx]));

          }
        }
      } 
    }
  }

  //if(pCfg->rtpMultisRtp[outidx][0].overlappingPorts) {
  //  LOG(X_DEBUG("Using overlapping RTP port based lookup"));
  //}

  //fprintf(stderr, "DEST PORT:%d, %d,%d  %d,%d\n", ntohs(pCfg->rtpMultisMp2ts[0].pdests[0].saDsts.sin_port), ntohs(pCfg->rtpMultisRtp[0][0].pdests[0].saDsts.sin_port), ntohs(pCfg->rtpMultisRtp[0][1].pdests[0].saDsts.sin_port), ntohs(pCfg->rtpMultisRtp[1][0].pdests[0].saDsts.sin_port), ntohs(pCfg->rtpMultisRtp[1][1].pdests[0].saDsts.sin_port));
  //fprintf(stderr, "0x%x 0x%x 0x%x 0x%x \n",  &pCfg->rtpMultisRtp[0][0], &pCfg->rtpMultisRtp[0][1], &pCfg->rtpMultisRtp[1][0], &pCfg->rtpMultisRtp[1][1]);


  if((pktz_do & PKTZ_MASK_VID) || (pktz_do & PKTZ_MASK_AUD)) {

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if(outidx > 0 && 
         (!pCfg->sdpActions[outidx].pPathOutFile && !pCfg->xcode.vid.out[outidx].active)) {
        continue;
      }


      //
      // Set SDP callback function after output of first frame(s)
      // 
      pCfg->sdpActions[outidx].pCfg = pCfg;
      pCfg->sdpActions[outidx].outidx = outidx;
      pCfg->sdpActions[outidx].cbSdpUpdated = cbSdpUpdated;
      pStreamAv->pSdpActions[outidx] = &pCfg->sdpActions[outidx];

    }

    //
    // If doing transcoding, update SDP contents
    //
    if((pktz_do & PKTZ_MASK_VID)) {

      //fprintf(stderr, "TEST GETVIDHEADER\n");
      //xcode_getvidheader(pStreamAv->progs[0].pXcodeData);

      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        if((assume_vidfmt || pCfg->xcode.vid.common.cfgDo_xcode || 
          (!pCodecs->pH264 && !pCodecs->pMpg4v && !pCfg->sdpsx[1][0].vid.common.available))) {
          //fprintf(stderr, "SET UPDATE VID [outidx:%d] to 1 assumev:%d, assumeaud:%d\n", outidx, assume_vidfmt, assume_audfmt);
          pCfg->sdpActions[outidx].updateVid = 1;
        }

      }
    }

    if((pktz_do & PKTZ_MASK_AUD)) { 

      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        if((assume_audfmt || pCfg->xcode.aud.common.cfgDo_xcode ||
          (!pCodecs->pAac && !pCodecs->pAmr && !pCfg->sdpsx[1][0].aud.common.available))) {

          pCfg->sdpActions[outidx].updateAud = 1;
        }

      }
    }

    //
    // If the SDP updater will not be invoked from stream_av (per xcode output based SDP)
    // write an SDP file right here
    //
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      //fprintf(stderr, "SDP UPDATER from streamerCfg idx[%d] cb:0x%x, path:'%s', aud:%d, vid:%d\n", outidx, pCfg->sdpActions[outidx].cbSdpUpdated, pCfg->sdpActions[outidx].pPathOutFile, pCfg->sdpActions[outidx].updateAud,  pCfg->sdpActions[outidx].updateVid);

      if(pCfg->sdpActions[outidx].cbSdpUpdated && pCfg->sdpActions[outidx].pPathOutFile &&
         !pCfg->sdpActions[outidx].updateAud && !pCfg->sdpActions[outidx].updateVid) {

         pCfg->sdpActions[outidx].cbSdpUpdated(&pCfg->sdpActions[outidx]);
      }
    }

  } 

  //fprintf(stderr, "ASSUME_VIDFMT:%d ASSUME_AUDFMT:%d h264:0x%x, mpg4v:0x%x, aac:0x%x, amr:0x%x updateV:%d, updateA:%d\nSDP avail:%d, codec:%d, mpg4v{profile_level:%d hdrsLen:%d}\n", assume_vidfmt, assume_audfmt, pCodecs->pH264, pCodecs->pMpg4v, pCodecs->pAac, pCodecs->pAmr, pCfg->sdpActions[0].updateVid, pCfg->sdpActions[0].updateAud, pCfg->sdpsx[1][0].vid.common.available, pCfg->sdpsx[1][0].vid.common.codecType, pCfg->sdpsx[1][0].vid.u.mpg4v.profile_level_id, pCfg->sdpsx[1][0].vid.u.mpg4v.seqHdrs.hdrsLen);

  if(pktz_do & PKTZ_DO_MP2TS) {

    if(pCfg->verbosity > VSX_VERBOSITY_NORMAL) {
      sdp_write(&pCfg->sdpsx[0][0], NULL, S_DEBUG);
    }

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      if(pktz_do == PKTZ_DO_MP2TS && pCfg->sdpActions[outidx].pPathOutFile) {
        rc = sdp_write_path(&pCfg->sdpsx[0][outidx], pCfg->sdpActions[outidx].pPathOutFile);
      }
    }

  }

  if(pCfg->status.useStatus) {
    pCfg->status.durationMs = durationLongest * 1000.0f;
    pCfg->status.locationMs = 0.0f;
    pStreamAv->plocationMs = &pCfg->status.locationMs;
    if((pktz_do & PKTZ_DO_MP2TS)) {
      streamsOutMp2ts[0].pBwDescr = &pCfg->status.xmitBwDescr;
    }
  }
  pCodecs->vs.streamAv.streamflags = pCfg->streamflags;

  if((pCodecs->vs.streamAv.avReaderType = pCfg->avReaderType) != VSX_STREAM_READER_TYPE_DEFAULT) {
    LOG(X_DEBUG("A/V reader type is set to: %d"), pCodecs->vs.streamAv.avReaderType);
  }

  //
  // User reporting callback is enabled 
  //
  if(pCfg->pReportCfg) {
    memset(&pCfg->pReportCfg->reportCbData, 0, sizeof(pCfg->pReportCfg->reportCbData));
    streamsOutMp2ts[0].pBwDescr = &pCfg->status.xmitBwDescr;
    streamsOutRtp[0][0].pBwDescr = &xmitBwDescrs[0][0];
    streamsOutRtp[0][1].pBwDescr = &xmitBwDescrs[0][1];
  }

  if(pCfg->action.do_rtsplive || pCfg->action.do_rtspannounce) {

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      pCfg->pRtspSessions->pRtpMultis[outidx] = pCfg->rtpMultisRtp[outidx];
    }
    if(!pCfg->action.do_output) {
      pCfg->action.do_output = 1;
    }

  }
 
//LOG(X_DEBUG("TO_FILE:%d, PROTO:0x%x, NUMD:%d, pktz_do: 0x%x DO_RTSPLIVE:%d, DO_RTSPANNOUNCE:%d, dstHost:'%s' :%d, outTransType: 0x%x"), stream_tofile,pCfg->proto, pCfg->numDests, pktz_do, pCfg->action.do_rtsplive, pCfg->action.do_rtspannounce, pCfg->pdestsCfg->dstHost, pCfg->pdestsCfg->ports[0], pCfg->pdestsCfg[0].outTransType);

  //
  // Wait until succesfully ANNOUNCE setup to a remote RTSP server
  //
  if(pCfg->action.do_rtspannounce) {
    if((rc = stream_rtsp_announce_start(pCfg, 1)) < 0) {
      LOG(X_ERROR("Failed to do RTSP ANNOUNCE and SETUP"));
      return stream_av_onexit(pCfg, pCodecs, -1);
    }
  }

  //
  // Wait until succesfully PUBLISH / CONNECT to a remote RTMP server
  //
  if(pCfg->action.do_rtmppublish) {
    if((rc = stream_rtmp_publish_start(pCfg, 1)) < 0) {
      LOG(X_ERROR("Failed to do RTMP PUBLISH"));
      return stream_av_onexit(pCfg, pCodecs, -1);
    }
  }

  do {

    pthread_mutex_lock(&pCfg->mtxStrmr);
    stream_setstate(pCfg, STREAMER_STATE_RUNNING, 0);
    pCfg->jump.type = JUMP_OFFSET_TYPE_NONE;
    pCfg->pauseRequested = 0;
    pthread_mutex_unlock(&pCfg->mtxStrmr);

    //rc = streamer_run_tonet(&streamIn, durationLongest);
    rc = streamer_run_tonet(&streamIn, pCfg->fDurationMax);

    LOG(X_DEBUGV("streamer_run_tonet done with code: %d, pause:%d, jump:%d, loop:%d"), 
     rc, pCfg->pauseRequested, pCfg->jump.type, pCfg->loop);

    if(pCfg->pauseRequested) {
      //TODO: store the byte jump offset not the sec offset here

      pCfg->jump.u.jumpOffsetSec = (float) (*(pStreamAv->plocationMs) / 1000.0);
      while(pCfg->pauseRequested && !pCfg->quitRequested) {
        usleep(50000);
      }
      pCfg->pauseRequested = 0;
      if(!pCfg->quitRequested) {
        pCfg->jump.type = JUMP_OFFSET_TYPE_SEC; 
      }
    }

    if((pCfg->loop && pCfg->running == STREAMER_STATE_RUNNING) || 
        pCfg->jump.type != JUMP_OFFSET_TYPE_NONE) {

      jmpTryCnt = 0;

      if(pCfg->loop && pCfg->jump.type == JUMP_OFFSET_TYPE_NONE) {
        pCfg->jump.u.jumpOffsetSec = 0;

        //pStreamAv->haveStartHz = 0;
        //pStreamAv->startHzIn = 0;
        //pStreamAv->lastHzIn = 0;
        pStreamAv->tmStartTx = timer_GetTime();
        pStreamAv->progs[0].frameId = pStreamAv->progs[1].frameId = 0; 
        pStreamAv->progs[0].haveStartHzIn = pStreamAv->progs[1].haveStartHzIn = 0; 
        pStreamAv->progs[0].lastHzIn = pStreamAv->progs[1].lastHzIn = 
        pStreamAv->progs[0].lastHzOut = pStreamAv->progs[1].lastHzOut = 0;
        pStreamAv->progs[0].noMoreData = pStreamAv->progs[1].noMoreData = 0;

        if(pCfg->xcode.vid.common.cfgDo_xcode) {
          pCfg->xcode.vid.common.outpts90Khz = 0;
          //pCfg->xcode.vid.common.inpts90Khz = 0;
        }
        if(pCfg->xcode.aud.common.cfgDo_xcode) {
          pCfg->xcode.aud.common.outpts90Khz = 0;
          //pCfg->xcode.aud.common.inpts90Khz = 0;
          //pCfg->xcode.aud.common.decodeInIdx = 0;
          //pCfg->xcode.aud.common.decodeOutIdx = 0;
          //pCfg->xcode.aud.common.encodeInIdx = 0;
          //pCfg->xcode.aud.common.encodeOutIdx = 0;
        }


      }


      do {
 
        fJumpOffsetSecActual = pCfg->jump.u.jumpOffsetSec;
        rcJmp = 0;
        LOG(X_DEBUG("Repositioning stream offset to %.2f"), pCfg->jump.u.jumpOffsetSec);

        if(pAv) {
          jmpTryCnt = 1;
          if(pCfg->jump.type == JUMP_OFFSET_TYPE_SEC && pCfg->jump.u.jumpOffsetSec >= 0 &&
             (fJumpOffsetSecActual = stream_net_pes_jump(pStreamAv->progs[0].pCbData,
                                                 pCfg->jump.u.jumpOffsetSec, 1)) < 0) {
            rcJmp= -1;
          } else if(pCfg->jump.type == JUMP_OFFSET_TYPE_BYTE ||
             (pCfg->loop && pCfg->jump.type == JUMP_OFFSET_TYPE_NONE && pCodecs->isInM2ts)) {
            mp2ts_jump_file_offset(pCfg->status.pFileStream, pCfg->jump.u.jumpPercent,
                                   MP2TS_LEN *
            (pCfg->xcode.vid.common.cfgDo_xcode ? TS_OFFSET_PKTS_XCODE : TS_OFFSET_PKTS));

            fJumpOffsetSecActual = 0;
          }

        }

        if(rcJmp == 0) {

          if(pCodecs->pH264) {

            //TODO: this will clear out the sps/pps 
            memset(pCodecs->pNetH264, 0, sizeof(pCodecs->vn.netH264));
            pCodecs->pNetH264->pH264 = pCodecs->pH264;
            pCodecs->pNetH264->includeAnnexBHdr = 1;
            pCodecs->pNetH264->includeSeqHdrs = pCfg->cfgstream.includeSeqHdrs;

            if(pCfg->jump.type == JUMP_OFFSET_TYPE_SEC && pCfg->jump.u.jumpOffsetSec > 0 &&
               (fJumpOffsetSecActual = stream_net_h264_jump(pCodecs->pNetH264, 
                                                            pCfg->jump.u.jumpOffsetSec, 1)) < 0) {
              rcJmp = -1;
            }

          } else if(pCodecs->pMpg4v) {

            memset(pCodecs->pNetMpg4v, 0, sizeof(pCodecs->vn.netMpg4v));
            pCodecs->pNetMpg4v = &pCodecs->vn.netMpg4v;
            pCodecs->pNetMpg4v->pMpg4v = pCodecs->pMpg4v;
            pCodecs->pNetMpg4v->includeSeqHdrs = pCfg->cfgstream.includeSeqHdrs;

            if(pCfg->jump.type == JUMP_OFFSET_TYPE_SEC && pCfg->jump.u.jumpOffsetSec > 0 &&
               (fJumpOffsetSecActual = stream_net_mpg4v_jump(pCodecs->pNetMpg4v, 
                                                             pCfg->jump.u.jumpOffsetSec, 1)) < 0) {
              rcJmp = -1;
            }

          }

        }

        if(rcJmp == 0) {

          if(pCodecs->pAac) {

            memset(pCodecs->pNetAac, 0, sizeof(pCodecs->an.netAac));
            memcpy(&pCodecs->pNetAac->aac, pCodecs->pAac, sizeof(AAC_DESCR_T));
        
            if(pCfg->jump.type == JUMP_OFFSET_TYPE_SEC && fJumpOffsetSecActual > 0 &&
               stream_net_aac_jump(pCodecs->pNetAac, fJumpOffsetSecActual) < 0) {
              rcJmp = -1; 
              pCodecs->pNetAac->idxSample = 0;
            }

          } else if(pCodecs->pAmr) {

            memset(pCodecs->pNetAmr, 0, sizeof(pCodecs->an.netAmr));
            pCodecs->pNetAmr = &pCodecs->an.netAmr;
            pCodecs->pNetAmr->pAmr  = pCodecs->pAmr;

            if(pCfg->jump.type == JUMP_OFFSET_TYPE_SEC && fJumpOffsetSecActual > 0 &&
               stream_net_amr_jump(pCodecs->pNetAmr, fJumpOffsetSecActual) < 0) {
              rcJmp = -1; 
              pCodecs->pNetAmr->idxSample = 0;
            }

          }

        }

        if(rcJmp != 0) {
          pCfg->jump.u.jumpOffsetSec = 0;
        }

      } while(rcJmp != 0 && jmpTryCnt++ < 1);

      //fprintf(stderr, "CALLING STREAM_AV_RESET FROM STREAMER_RTP\n");
      if(stream_av_reset(pStreamAv, 1) < 0) {
        rc = -1;
        break;
      }

      if(pCfg->jump.type != JUMP_OFFSET_TYPE_NONE) {
     
        for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
          if(pCfg->rtpMultisMp2ts[outidx].pRtp) {
            pCfg->rtpMultisMp2ts[outidx].pRtp->sequence_num = 
                              htons(ntohs(pCfg->rtpMultisMp2ts[outidx].pRtp->sequence_num) - 1);
          }
        }
        pStreamAv->locationMsStart = fJumpOffsetSecActual * 1000.0;

      } else {
        pStreamAv->locationMsStart = 0;
      }

    }

  } while(g_proc_exit == 0 && !pCfg->quitRequested &&
          ((pCfg->loop && pCfg->running == STREAMER_STATE_RUNNING) || 
           pCfg->jump.type != JUMP_OFFSET_TYPE_NONE));

  if(pCfg->u.cfgmp2ts.useMp2tsContinuity) {
    pCfg->u.cfgmp2ts.lastPatContIdx = lastPatContIdx[0];
    pCfg->u.cfgmp2ts.lastPmtContIdx = lastPmtContIdx[0];
  }

  stream_av_close(pStreamAv);

  pthread_mutex_lock(&pCfg->mtxStrmr);
  //pCfg->running = STREAMER_STATE_FINISHED;
  if(pCfg->rtpMultisMp2ts[0].pRtp) {
    pCfg->cfgrtp.sequences_at_end[0] = htons(pCfg->rtpMultisMp2ts[0].pRtp->sequence_num);
  }
  if(pCfg->action.do_rtsplive || pCfg->action.do_rtspannounce) {
    pthread_mutex_lock(&pCfg->pRtspSessions->mtx);
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      pCfg->pRtspSessions->pRtpMultis[outidx] = NULL;
    }
    pthread_mutex_unlock(&pCfg->pRtspSessions->mtx);
  }
  pthread_mutex_unlock(&pCfg->mtxStrmr);

  if(pCfg->action.do_rtspannounce) {
    stream_rtsp_close(pCfg);
  }
  if(pCfg->action.do_rtmppublish) {
    stream_rtmp_close(pCfg);
  }

  stream_av_onexit(pCfg, pCodecs, 0);

  LOG(X_DEBUGV("streaming ended rc:%d"), rc);
  return rc;
}

int stream_h264b(FILE_STREAM_T *pFile,
                 STREAMER_CFG_T *pCfg,
                 double fps) {

  CODEC_PAIR_T codecs;
  int rc = 0;

  if(!pFile || !pCfg) {
    return -1;
  }

  memset(&codecs, 0, sizeof(codecs));
  codecs.pH264 = &codecs.v.h264;

  if(h264_createNalListFromAnnexB(codecs.pH264, pFile, pCfg->fStart, fps) != 0) {
    return -1;
  }

  if(h264_updateFps(codecs.pH264, fps) < 0) {
    codecs_close(&codecs);
    return -1;
  }
  
  if(codecs.pH264->pNalsLastSlice == NULL) {
    LOG(X_WARNING("No NALS to stream"));
    codecs_close(&codecs);
    return -1;
  }

  switch(pCfg->proto) {
    case STREAM_PROTO_RTP:
    case STREAM_PROTO_MP2TS:
      rc = stream_av_xcode(&codecs, pCfg,
                           &codecs.vs.streamAv.progs[MP2PES_STREAM_IDX_VID].pXcodeData,
                           NULL,
                           NULL, NULL,
                           XC_CODEC_TYPE_H264, XC_CODEC_TYPE_UNKNOWN);

      break;
    default:
      LOG(X_ERROR("Unsupported protocol type: %u"), pCfg->proto);
      rc = -1;
      break;
  }

  codecs_close(&codecs);

  return rc;
}

int stream_aacAdts(FILE_STREAM_T *pFile,
                   STREAMER_CFG_T *pCfg,
                   unsigned int frameDeltaHz) {

  CODEC_PAIR_T codecs;
  int rc = 0;

  memset(&codecs, 0, sizeof(codecs));

  if(!pFile || !pCfg) {
    return -1;
  }

  if((codecs.pAac = aac_initFromAdts(pFile, frameDeltaHz)) == NULL) {
    return -1;
  }

  if(codecs.pAac->clockHz == 0 || codecs.pAac->decoderSP.frameDeltaHz == 0) {
    LOG(X_ERROR("Unable to extract timing information from file."));
    codecs_close(&codecs);
    return -1;
  }

  switch(pCfg->proto) {
    case STREAM_PROTO_RTP:
    case STREAM_PROTO_MP2TS:
      codecs.pAac->includeAdtsHdr = 1;
      rc = stream_av_xcode(&codecs, pCfg,
                           NULL, 
                           &codecs.vs.streamAv.progs[MP2PES_STREAM_IDX_AUD].pXcodeData,
                           NULL, NULL,
                           XC_CODEC_TYPE_UNKNOWN, XC_CODEC_TYPE_AAC);

      break;
    default:
      LOG(X_ERROR("Unsupported protocol type: %u"), pCfg->proto);
      rc = -1;
      break;
  }

  codecs_close(&codecs);

  return rc;
}

int stream_image(const char *path, STREAMER_CFG_T *pCfg, enum MEDIA_FILE_TYPE mediaType) {
  int rc = 0;
  CODEC_PAIR_T codecs;

  memset(&codecs, 0, sizeof(codecs));

  //
  // If there is no mixer source specified, turn off any audio processing since we're streaming 
  // an image
  //
  if(!pCfg->xcode.vid.pip.active && !pCfg->pip.pMixerCfg->active && !pCfg->noaud) {
    pCfg->noaud = 1;
    pCfg->xcode.aud.common.cfgDo_xcode = 0;
  } 

  if(image_open(path, &codecs.v.image, mediaType) < 0) {
    return -1;
  }

  //
  // If this image is a pip, disable the close on exit flag, since it is one frame
  // of infinite duration
  //
  if(pCfg->xcode.vid.pip.active &&
     (pCfg->xcode.vid.pip.flags & PIP_FLAGS_CLOSE_ON_END)) {
    pCfg->xcode.vid.pip.flags &= ~PIP_FLAGS_CLOSE_ON_END;
  }

  codecs.pImage = &codecs.v.image;

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
  if(!pCfg->xcode.vid.pip.active && pCfg->pip.pMixerCfg->active && !pCfg->noaud && STREAMER_DO_OUTPUT(pCfg->action)) {
    codecs.pConferenceA = &codecs.a.conferenceA;
    LOG(X_DEBUGV("Creating audio component for still video image"));
  }
#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  switch(pCfg->proto) {
    case STREAM_PROTO_RTP:
    case STREAM_PROTO_MP2TS:

      rc = stream_av_xcode(&codecs, pCfg, 
                           &codecs.vs.streamAv.progs[MP2PES_STREAM_IDX_VID].pXcodeData, 
                            &codecs.vs.streamAv.progs[MP2PES_STREAM_IDX_AUD].pXcodeData,
                            NULL, NULL, 
                            XC_CODEC_TYPE_UNKNOWN, 
                            codecs.pConferenceA ? XC_CODEC_TYPE_AUD_CONFERENCE : XC_CODEC_TYPE_UNKNOWN);
      break;
    default:
      LOG(X_ERROR("Unsupported protocol type: %u"), pCfg->proto);
      rc = -1;
      break;
  }

  codecs_close(&codecs);

  return rc;
}

int stream_mp4_path(const char *path, STREAMER_CFG_T *pCfg) {
  int rc = 0;
  MP4_CONTAINER_T *pMp4;

  if((pMp4 = mp4_open(path, 1)) == NULL) {
    LOG(X_ERROR("Failed to read %s"), path);
    return -1;
  }

  rc = stream_mp4(pMp4, pCfg);

  mp4_close(pMp4);

  return rc;
}

int stream_mp4(MP4_CONTAINER_T *pMp4, STREAMER_CFG_T *pCfg) {
  CODEC_PAIR_T codecs;
  MP4_TRAK_AVCC_T mp4BoxSetAvcC;
  MP4_TRAK_MP4V_T mp4BoxSetMp4v;
  MP4_TRAK_MP4A_T mp4BoxSetMp4a;
  XC_CODEC_TYPE_T vidCodecType = XC_CODEC_TYPE_UNKNOWN;
  XC_CODEC_TYPE_T audCodecType = XC_CODEC_TYPE_UNKNOWN;
  int rc = 0;

  memset(&codecs, 0, sizeof(codecs));

  if(!pCfg->novid) {
  if(mp4_loadTrack(pMp4->proot, &mp4BoxSetAvcC.tk, *((uint32_t *) "avc1"), 1)) {

    LOG(X_DEBUG("Found track avc1"));
    vidCodecType = XC_CODEC_TYPE_H264;
    codecs.pH264 = &codecs.v.h264;

    //
    // Use  h264_createFromMp4
    // to avoid pre-fetching the entire NAL list
    //
    if(h264_createNalListFromMp4Direct(codecs.pH264, pMp4, pCfg->fStart) != 0) {
    //if(h264_createNalListFromMp4(codecs.pH264, pMp4, NULL, pCfg->fStart) != 0) {
      return -1;
    }

    if(codecs.pH264->clockHz == 0 || codecs.pH264->frameDeltaHz == 0) {

      if((codecs.pH264->frameDeltaHz = mp4_getSttsSampleDelta(&mp4BoxSetAvcC.tk.pStts->list)) == 0) {
        LOG(X_ERROR("Unable to extract timing information from avcC track"));
        codecs_close(&codecs);
        return -1;
      }
      codecs.pH264->clockHz = mp4BoxSetAvcC.tk.pMdhd->timescale;
      LOG(X_DEBUG("Using mp4 fps %d/%d"), codecs.pH264->clockHz, 
                                          codecs.pH264->frameDeltaHz);
    }

  } else if(mp4_loadTrack(pMp4->proot, &mp4BoxSetMp4v.tk, *((uint32_t *) "mp4v"), 1)) {

    LOG(X_DEBUG("Found track mp4v"));
    vidCodecType = XC_CODEC_TYPE_MPEG4V;
    codecs.pMpg4v = &codecs.v.mpg4v;

    if(mpg4v_createSampleListFromMp4(codecs.pMpg4v, pMp4, NULL, pCfg->fStart) != 0) {
      codecs_close(&codecs);
      return -1;
    }
    if(codecs.pMpg4v->param.clockHz == 0 || codecs.pMpg4v->param.frameDeltaHz == 0) {

      if((codecs.pMpg4v->param.frameDeltaHz = 
                 mp4_getSttsSampleDelta(&mp4BoxSetMp4v.tk.pStts->list)) == 0) {
        LOG(X_ERROR("Unable to extract timing information from mp4v track"));
        codecs_close(&codecs);
        return -1;
      }
      codecs.pMpg4v->param.clockHz = mp4BoxSetMp4v.tk.pMdhd->timescale;
      LOG(X_DEBUG("Using mp4 fps %d/%d"), codecs.pMpg4v->param.clockHz, 
                                          codecs.pMpg4v->param.frameDeltaHz);
    }

  } else {
    LOG(X_WARNING("No supported video track not found in %s"), pMp4->pStream->cbGetName(pMp4->pStream));
    pCfg->novid = 1;
  }
  }

  if(!pCfg->noaud) {
  if(mp4_loadTrack(pMp4->proot, &mp4BoxSetMp4a.tk, *((uint32_t *) "mp4a"), 1)) {

    LOG(X_DEBUG("Found track mp4a"));
    audCodecType = XC_CODEC_TYPE_AAC;

    if(mp4_initMp4aTrack(pMp4, &mp4BoxSetMp4a) < 0) {
      codecs_close(&codecs);
      return -1;
    }

    //if((codecs.pAac = aac_getSamplesFromMp4(pMp4, NULL, pCfg->fStart)) == NULL) {
    if((codecs.pAac = aac_getSamplesFromMp4Direct(pMp4, pCfg->fStart)) == NULL) {
      codecs_close(&codecs);
      return -1;
    }

    if(codecs.pAac->clockHz == 0 || codecs.pAac->decoderSP.frameDeltaHz == 0) {
      LOG(X_ERROR("Unable to extract timing information from mp4a track"));
      codecs_close(&codecs);
      return -1;
    }

  } else if(mp4_loadTrack(pMp4->proot, &mp4BoxSetMp4a.tk, *((uint32_t *) "samr"), 1)) {

    LOG(X_DEBUG("Found track samr"));
    codecs.pAmr = &codecs.a.amr;
    audCodecType = XC_CODEC_TYPE_AMRNB;

    if(amr_createSampleListFromMp4Direct(codecs.pAmr, pMp4, pCfg->fStart) != 0) {
      codecs_close(&codecs);
      return -1;
    }

  } else {
    LOG(X_WARNING("mp4a track not found in %s"), pMp4->pStream->cbGetName(pMp4->pStream));
    pCfg->noaud = 1;
  }
  }

  switch(pCfg->proto) {
    case STREAM_PROTO_RTP:
    case STREAM_PROTO_MP2TS:
      if(codecs.pAac) {
        codecs.pAac->includeAdtsHdr = 1;
      }
      rc = stream_av_xcode(&codecs, pCfg, 
                           &codecs.vs.streamAv.progs[MP2PES_STREAM_IDX_VID].pXcodeData, 
                           &codecs.vs.streamAv.progs[MP2PES_STREAM_IDX_AUD].pXcodeData,
                           NULL, NULL, 
                           vidCodecType, audCodecType);
      break;
    default:
      LOG(X_ERROR("Unsupported protocol type: %u"), pCfg->proto);
      rc = -1;
      break;
  }

  codecs_close(&codecs);

  return rc;
}

int stream_mkv(const char *path,
               STREAMER_CFG_T *pCfg, double fps) {

  MKV_CONTAINER_T *pMkv;
  CODEC_PAIR_T codecs;
  double fpsmkv;
  XC_CODEC_TYPE_T vidCodecType = XC_CODEC_TYPE_UNKNOWN;
  XC_CODEC_TYPE_T audCodecType = XC_CODEC_TYPE_UNKNOWN;
  int rc = 0;

  memset(&codecs, 0, sizeof(codecs));

  if((pMkv = mkv_open(path)) == NULL) {
    LOG(X_ERROR("Failed to read %s"), path);
    return -1;
  }

  if(pMkv->vid.pTrack) {

    vidCodecType = pMkv->vid.pTrack->codecType;

    if((fpsmkv = mkv_getfps(pMkv)) <= 0) {
      LOG(X_ERROR("Unable to extract video timing information from mkv"));
      mkv_close(pMkv);
      return -1;
    }

    switch(vidCodecType) {

      case XC_CODEC_TYPE_H264:

        codecs.pH264 = &codecs.v.h264;
        vid_convert_fps(fpsmkv, &codecs.pH264->clockHz, &codecs.pH264->frameDeltaHz);
        //fprintf(stderr, "FPS:%.3f %d/%d\n", fpsmkv, codecs.pH264->clockHz, codecs.pH264->frameDeltaHz);

        if(h264_createNalListFromMkv(codecs.pH264, pMkv) < 0) {
          mkv_close(pMkv);
          return -1;
        }
        break;

      case XC_CODEC_TYPE_VP8:

        codecs.pVp8 = &codecs.v.vp8;
        if(vp8_loadFramesFromMkv(codecs.pVp8, pMkv) < 0) {
          mkv_close(pMkv);
          return -1;
        }

        break;

      default:
        LOG(X_ERROR("%s contains unsupported video codec %s"), pMkv->pStream->filename, pMkv->vid.pTrack->codecId);
        break;
    }

  }

  if(pMkv->aud.pTrack) {

    audCodecType = pMkv->aud.pTrack->codecType;

    switch(audCodecType) {

      case XC_CODEC_TYPE_AAC:

        if((codecs.pAac = aac_getSamplesFromMkv(pMkv)) == NULL) {
          codecs_close(&codecs);
          mkv_close(pMkv);
          return -1;
        }
        break;

      case XC_CODEC_TYPE_VORBIS:

        codecs.pVorbis = &codecs.a.vorbis;
        if(vorbis_loadFramesFromMkv(codecs.pVorbis, pMkv) < 0) {
          mkv_close(pMkv);
          return -1;
        }

        break;

      default:
        LOG(X_ERROR("%s contains unsupported audio codec %s"), pMkv->pStream->filename, pMkv->aud.pTrack->codecId);
        break;
    }

  }

  switch(pCfg->proto) {
    case STREAM_PROTO_RTP:
    case STREAM_PROTO_MP2TS:

      if(codecs.pAac) {
        codecs.pAac->includeAdtsHdr = 1;
      }

      rc = stream_av_xcode(&codecs, pCfg,
                           &codecs.vs.streamAv.progs[MP2PES_STREAM_IDX_VID].pXcodeData,
                           &codecs.vs.streamAv.progs[MP2PES_STREAM_IDX_AUD].pXcodeData,
                           NULL, NULL,
                           vidCodecType, audCodecType);
      break;
    default:
      LOG(X_ERROR("Unsupported protocol type: %u"), pCfg->proto);
      rc = -1;
      break;
  }

  codecs_close(&codecs);
  mkv_close(pMkv);

  return rc;
}

int stream_flv(const char *path,
               STREAMER_CFG_T *pCfg, double fps) {

  FLV_CONTAINER_T *pFlv;
  CODEC_PAIR_T codecs;
  XC_CODEC_TYPE_T vidCodecType = XC_CODEC_TYPE_UNKNOWN;
  XC_CODEC_TYPE_T audCodecType = XC_CODEC_TYPE_UNKNOWN;
  int rc = 0;

  memset(&codecs, 0, sizeof(codecs));

  if((pFlv = flv_open(path)) == NULL) {
    LOG(X_ERROR("Failed to read %s"), path);
    return -1;
  }

  if(FLV_HAVE_VIDEO(pFlv) && pFlv->vidCfg.pAvcC) {

    codecs.pH264 = &codecs.v.h264;
    vidCodecType = XC_CODEC_TYPE_H264;

    if(h264_createNalListFromFlv(codecs.pH264, pFlv, fps) != 0) {
      flv_close(pFlv);
      return -1;
    }

    if(codecs.pH264->clockHz == 0 || codecs.pH264->frameDeltaHz == 0) {
      if(pFlv->sampledeltaHz == 0 || pFlv->timescaleHz == 0) {
        LOG(X_ERROR("Unable to extract video timing information from flv"));
        codecs_close(&codecs);
        flv_close(pFlv);
        return -1;
      }
      codecs.pH264->clockHz = pFlv->timescaleHz;
      codecs.pH264->frameDeltaHz = pFlv->sampledeltaHz;
    }

  }

  if(FLV_HAVE_AUDIO(pFlv) && pFlv->audCfg.aacCfg.objTypeIdx != 0) {

    audCodecType = XC_CODEC_TYPE_AAC;

    if((codecs.pAac = aac_getSamplesFromFlv(pFlv)) == NULL) {
      codecs_close(&codecs);
      flv_close(pFlv);
      return -1;
    }

    if(codecs.pAac->clockHz == 0 || codecs.pAac->decoderSP.frameDeltaHz == 0) {
      LOG(X_ERROR("Unable to audio extract timing information from flv"));
      codecs_close(&codecs);
      flv_close(pFlv);
      return -1;
    }
  }

  switch(pCfg->proto) {
    case STREAM_PROTO_RTP:
    case STREAM_PROTO_MP2TS:
      if(codecs.pAac) {
        codecs.pAac->includeAdtsHdr = 1;
      }

      rc = stream_av_xcode(&codecs, pCfg,
                           &codecs.vs.streamAv.progs[MP2PES_STREAM_IDX_VID].pXcodeData,
                           &codecs.vs.streamAv.progs[MP2PES_STREAM_IDX_AUD].pXcodeData,
                           NULL, NULL,
                           vidCodecType, audCodecType);
      break;
    default:
      LOG(X_ERROR("Unsupported protocol type: %u"), pCfg->proto);
      rc = -1;
      break;
  }

  codecs_close(&codecs);
  flv_close(pFlv);

  return rc;
}

int stream_conference(STREAMER_CFG_T *pCfg) {
  int rc = 0;
  CODEC_PAIR_T codecs;

  memset(&codecs, 0, sizeof(codecs));

  if(pCfg->xcode.vid.pip.active) {
    return -1;
  }

  codecs.pConferenceV = &codecs.v.conferenceV;

  //if(!pCfg->xcode.vid.common.cfgDo_xcode && pCfg->pip.pMixerCfg->conferenceInputDriver) {

    //fprintf(stderr, "conference do_xcode:%d, filetypeout:%d\n", pCfg->xcode.vid.common.cfgDo_xcode, pCfg->xcode.vid.common.cfgFileTypeOut);
    //pCfg->xcode.vid.common.cfgDo_xcode = 1;
    //pCfg->xcode.vid.common.cfgFileTypeOut = XC_CODEC_TYPE_RAWV_YUVA420P;
  //}


#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
  if(!pCfg->noaud && STREAMER_DO_OUTPUT(pCfg->action)) {
    codecs.pConferenceA = &codecs.a.conferenceA;
  }

  if(STREAMER_DO_OUTPUT(pCfg->action) &&
     pCfg->xcode.vid.common.cfgDo_xcode &&
     pCfg->xcode.aud.common.cfgDo_xcode) {

    if(IS_XC_CODEC_TYPE_VID_RAW(pCfg->xcode.vid.common.cfgFileTypeOut) &&
       !IS_XC_CODEC_TYPE_AUD_RAW(pCfg->xcode.aud.common.cfgFileTypeOut)) {
      LOG(X_ERROR("Video encoder codec must be specified for output with audio encoding"));
      return -1;
    } else if(!IS_XC_CODEC_TYPE_VID_RAW(pCfg->xcode.vid.common.cfgFileTypeOut) &&
              IS_XC_CODEC_TYPE_AUD_RAW(pCfg->xcode.aud.common.cfgFileTypeOut)) {
      LOG(X_ERROR("Audio encoder codec must be specified for output with video encoding"));
      return -1;
    }
  }
#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  switch(pCfg->proto) {
    case STREAM_PROTO_RTP:
    case STREAM_PROTO_MP2TS:

      rc = stream_av_xcode(&codecs, pCfg, 
                           &codecs.vs.streamAv.progs[MP2PES_STREAM_IDX_VID].pXcodeData, 
                           &codecs.vs.streamAv.progs[MP2PES_STREAM_IDX_AUD].pXcodeData,
                           NULL, NULL, 
                           codecs.pConferenceV ? XC_CODEC_TYPE_VID_CONFERENCE : XC_CODEC_TYPE_UNKNOWN, 
                           codecs.pConferenceA ? XC_CODEC_TYPE_AUD_CONFERENCE : XC_CODEC_TYPE_UNKNOWN);
      break;
    default:
      LOG(X_ERROR("Unsupported protocol type: %u"), pCfg->proto);
      rc = -1;
      break;
  }

  codecs_close(&codecs);

  return rc;
}

static MP2TS_CONTAINER_T *mp2ts_open_checklive(const char *path,
                                STREAMER_CFG_T *pCfg) {

  int liveFile = 0;
  MP2TS_CONTAINER_T *pMp2ts = NULL;

  if((liveFile = fileops_isBeingWritten(path))) {

    if(!(pMp2ts = mp2ts_open(path, &pCfg->quitRequested, 1, 0))) {
      return NULL;
    }

    LOG(X_DEBUG("TS file %s is opened as a live file"), path);

  } else {

    if(!(pMp2ts = mp2ts_openfrommeta(path)) &&
       !(pMp2ts = mp2ts_open(path, &pCfg->quitRequested, 0, 1))) {
      return NULL;
    }

  }

  pMp2ts->liveFile = liveFile;

  return pMp2ts;
}

int stream_mp2ts_pes_file(const char *path,
                    STREAMER_CFG_T *pCfg,
                    unsigned int mp2tspktlen) {

  MP2TS_CONTAINER_T *pMp2ts = NULL;
  STREAM_DATASRC_T pesDataSrc;
  STREAM_PES_T pes;
  int rc = 0;
  unsigned int idx;
  STREAM_AV_T streamAv;
  MP2TS_PID_DESCR_T *pPidDescr;
  STREAM_PES_FILE_CBDATA_T pesCbData;
  int haveAud = 0;
  int haveVid = 0;

  if(!(pMp2ts = mp2ts_open_checklive(path, pCfg))) {
    return -1;
  }

  memset(&pes, 0, sizeof(pes));

  memset(&pesCbData, 0, sizeof(pesCbData));
  pesCbData.pFile = pMp2ts->pStream;
  pesCbData.liveFile = pMp2ts->liveFile;

  memset(&pesDataSrc, 0, sizeof(pesDataSrc));
  pesDataSrc.pContainer = pMp2ts;
  pesDataSrc.pCbData = &pesCbData;
  pesDataSrc.cbGetData = stream_mp2ts_pes_cbgetfiledata;
  pesDataSrc.cbHaveData = stream_mp2ts_pes_cbhavefiledata;
  pesDataSrc.cbResetData = stream_mp2ts_pes_cbresetfiledata;

  if(pCfg->status.useStatus) {
    pCfg->status.pFileStream = pMp2ts->pStream;
  }

  if(pMp2ts->liveFile) {
    //
    // jump to end of live file 
    //
    mp2ts_jump_file_offset(pMp2ts->pStream, 100, MP2TS_LEN *
            (pCfg->xcode.vid.common.cfgDo_xcode ? TS_OFFSET_PKTS_XCODE : TS_OFFSET_PKTS));

  } else {


    if(!pMp2ts->pmtChanges) {
      LOG(X_ERROR("mpeg2-ts pmt contents not found"));
      mp2ts_close(pMp2ts);
      return -1;
    }

    memset(&streamAv, 0, sizeof(streamAv));
    //streamAv.lastHzin = pMp2ts->durationHz;

    for(idx = 0;
      idx < sizeof(pMp2ts->pmtChanges->pidDescr) / sizeof(pMp2ts->pmtChanges->pidDescr[0]);
      idx++ ) {

      pPidDescr = &pMp2ts->pmtChanges->pidDescr[idx];

      if(pPidDescr->pid.id != 0) {

        if(!haveVid && codectype_isVid(mp2_getCodecType(pPidDescr->pid.streamtype))) {

          haveVid = 1;

          if(pCfg->status.useStatus) {
            memcpy(&pCfg->status.vidProps[0], &pPidDescr->u.vidProp, sizeof(pCfg->status.vidProps[0]));
          }

          streamAv.progs[MP2PES_STREAM_IDX_VID].streamType = 
                                      (uint8_t) pPidDescr->pid.streamtype; 
          if(!mp2_check_pes_streamid(pPidDescr->streamId)) {
            streamAv.progs[MP2PES_STREAM_IDX_VID].streamId = MP2PES_STREAMID_VIDEO_START; 
            LOG(X_WARNING("Video pid: 0x%x stype: 0x%x appears to have an invalid stream id: 0x%x"
                          " Replacing with stream id: 0x%x"),
                pPidDescr->pid.id, pPidDescr->pid.streamtype, pPidDescr->streamId,
                streamAv.progs[MP2PES_STREAM_IDX_VID].streamId);
          } else {
            streamAv.progs[MP2PES_STREAM_IDX_VID].streamId = pPidDescr->streamId; 
          }

        } else if(!haveAud && codectype_isAud(mp2_getCodecType(pPidDescr->pid.streamtype))) {
 
          haveAud = 1;

          streamAv.progs[MP2PES_STREAM_IDX_AUD].streamType = 
                                   (uint8_t) pPidDescr->pid.streamtype; 

          if(!mp2_check_pes_streamid(pPidDescr->streamId)) {
            streamAv.progs[MP2PES_STREAM_IDX_AUD].streamId = MP2PES_STREAMID_AUDIO_START;
            LOG(X_WARNING("Audio pid: 0x%x stype: 0x%x appears to have an invalid stream id: 0x%x"
                        " Replacing with stream id: 0x%x"),
                pPidDescr->pid.id, pPidDescr->pid.streamtype, pPidDescr->streamId,
                streamAv.progs[MP2PES_STREAM_IDX_AUD].streamId);
          } else {
            streamAv.progs[MP2PES_STREAM_IDX_AUD].streamId = pPidDescr->streamId; 
          }

        }
        if(haveAud && haveVid) {
          break;
        }

      }

    } // end of for

    if(!(haveVid && haveAud)) {
      LOG(X_ERROR("stream_mp2ts_pes not implemented for non-av stream"));
      return -1;  
    }

    if(pCfg->fStart > 0 &&
       (mp2ts_jump_file(pMp2ts, pCfg->fStart, &streamAv.locationMsStart)) == NULL) {
      return -1;
    }

  }

  if(haveVid) {

    if(pCfg->novid) {
      pes.vid.inactive = 1;
    } else {
      if(!(pes.vid.pQueue = stream_createFrameQ(STREAM_FRAMEQ_TYPE_VID_NETFRAMES,
                                              pCfg->frvidqslots, 0))) {
        return -1;
      }
      pes.vid.pQueue->cfg.uselock = 0;
      pes.vid.pQueue->haveRdr = 1;
    }
  }
  if(haveAud) {

    if(pCfg->noaud) {
      pes.aud.inactive = 1;
    } else {
      if(!(pes.aud.pQueue = stream_createFrameQ(STREAM_FRAMEQ_TYPE_AUD_NETFRAMES,
                                                pCfg->fraudqslots, 0))) {
        if(pes.vid.pQueue) {
          pktqueue_destroy(pes.vid.pQueue);
        }
        return -1;
      }
      pes.aud.pQueue->cfg.uselock = 0;
      pes.aud.pQueue->haveRdr = 1;
    }
  }

  rc = stream_mp2ts_pes(&pesDataSrc, 
                        &pesDataSrc,
                        pCfg, 
                        pMp2ts->liveFile ? NULL : &streamAv, 
                        &pes,
                        NULL,
                        mp2tspktlen);

  mp2ts_close(pMp2ts);

  if(pes.vid.pQueue) {
    pktqueue_destroy(pes.vid.pQueue);
    pes.vid.pQueue = NULL;
  }
  if(pes.aud.pQueue) {
    pktqueue_destroy(pes.aud.pQueue);
    pes.aud.pQueue = NULL;
  }

  return rc;
}

int stream_mp2ts_pes(const STREAM_DATASRC_T *pDataSrcVid,
                     const STREAM_DATASRC_T *pDataSrcAud,
                     STREAMER_CFG_T *pCfg,
                     STREAM_AV_T *pStreamAv,
                     STREAM_PES_T *pPes,
                     STREAM_NET_AV_T *pAv,
                     unsigned int mp2tspktlen) {

  CODEC_PAIR_T codecs;
  STREAM_PES_DATA_T *pPesAud = NULL;
  STREAM_PES_DATA_T *pPesVid = NULL;
  unsigned int idx = 0;
  unsigned int outidx;
  int rc = 0;
  int haveAud = 0;
  int haveVid = 0;
  //int vidQsz = 0;
  //int audQsz = 0;
  int **ppHaveSeqStartVid = NULL;
  int **ppHaveSeqStartAud = NULL;
  uint16_t vidStreamType = MP2PES_STREAMTYPE_NOTSET;
  uint16_t audStreamType = MP2PES_STREAMTYPE_NOTSET;
  XC_CODEC_TYPE_T vidCodecType = XC_CODEC_TYPE_UNKNOWN;
  XC_CODEC_TYPE_T audCodecType = XC_CODEC_TYPE_UNKNOWN;

  if(!pCfg || (!pDataSrcVid && !pDataSrcAud) || (!pPes && !pAv)) {
    return -1;
  }

  memset(&codecs, 0, sizeof(codecs));
  codecs.pStreamAv = &codecs.vs.streamAv;
  if(pStreamAv) {
    memcpy(codecs.pStreamAv, pStreamAv, sizeof(codecs.vs.streamAv));
  }

  if(pPes) {
    if(!pDataSrcVid) {
      return -1;
    }
    pPes->tsPktLen = mp2tspktlen;
    pPes->pContainer = pDataSrcVid->pContainer;
    pPes->pXmitAction = &pCfg->action;
    codecs.isInM2ts = 1;
  } else if(pAv) {
    pAv->pXmitAction = &pCfg->action;
    codecs.vidType = pAv->vidCodecType;
    codecs.audType = pAv->audCodecType;
  }

  //
  // Set up video Packetized Elementary Stream
  //
  if(pDataSrcVid) {
    haveVid = 1;
    if(pPes) {
      pPesVid = &pPes->vid;
      pPesVid->pPes = pPes;
      pPes->pPcrData = &pPes->vid;
      stream_net_pes_init(&codecs.pStreamAv->progs[idx].stream);
      //vidQsz = FRAMESZ_VIDQUEUE_NUMFRAMES_DFLT;
      pPes->pDataSrc = pDataSrcVid;
    } else if(pAv) {
      pPesVid = &pAv->vid;
      pPesVid->pPes = pAv;
      vidStreamType = mp2_getStreamType(pAv->vidCodecType);
      vidCodecType = pAv->vidCodecType;
      stream_net_av_init(&codecs.pStreamAv->progs[idx].stream);
    }

    pPesVid->pDataSrc = pDataSrcVid;
    pPesVid->streamflags = pCfg->streamflags;
    //pPesVid->avReaderType = pCfg->avReaderType;
    codecs.pStreamAv->progs[0].pCbData = pPesVid;
    if(pDataSrcAud) {
      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
        codecs.pStreamAv->progs[0].pavs[outidx] = &pCfg->status.avs[outidx];
      }
    }
    //for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      //if(pCfg->status.avs[outidx].offset0 < 0) {
    //    codecs.pStreamAv->progs[0].pavs[outidx] = &pCfg->status.avs[outidx];
      //}
    //}

    if(pStreamAv == NULL) {
      codecs.pStreamAv->progs[idx].streamId = MP2PES_STREAMID_VIDEO_START;
      codecs.pStreamAv->progs[idx].streamType = vidStreamType;
      //if(pAv) {
      //  codecs.pStreamAv->vidDescr.mediaType = pAv->vidCodecType;
      //}
    }

    codecs.pStreamAv->progs[idx].haveDts = 1;
    codecs.pStreamAv->progs[idx].pesLenZero = 1;

    // Only enable the output stream type to vary if streaming live capture
    // and not doing transcoding
    if(pCfg->xcode.vid.common.cfgDo_xcode == 0 &&
       codecs.pStreamAv->progs[idx].streamType == MP2PES_STREAMTYPE_NOTSET) {
      pPesVid->pOutStreamType = &codecs.pStreamAv->progs[idx].streamType;
    }

    if(pCfg->novid) {
      codecs.pStreamAv->progs[idx].inactive = 1;
    }
    idx++;
  }

  //
  // Set up audio Packetized Elementary Stream
  //
  if(pDataSrcAud) {
    haveAud = 1;
    if(pPes) {
      pPesAud = &pPes->aud;
      pPesAud->pPes = pPes;
      stream_net_pes_init(&codecs.pStreamAv->progs[idx].stream);

      if(!pPes->pDataSrc) {
        pPes->pDataSrc = pDataSrcAud;
      }
      // Ensure audio data source is null, as all m2t packets will be pulled
      // from video pid
      //pDataSrcAud = NULL;
      //audQsz = FRAMESZ_AUDQUEUE_NUMFRAMES_DFLT;
    } else {
      pPesAud = &pAv->aud;
      pPesAud->pPes = pAv;
      pPesAud->pDataSrc = pDataSrcAud;
      audStreamType = mp2_getStreamType(pAv->audCodecType);
      audCodecType = pAv->audCodecType;
      stream_net_av_init(&codecs.pStreamAv->progs[idx].stream);

      //fprintf(stderr, "aud st: 0x%x ct:0x%x\n", audStreamType, audCodecType);
    }
    pPesAud->pDataSrc = pDataSrcAud;
    pPesAud->streamflags = pCfg->streamflags;
    //pPesAud->avReaderType = pCfg->avReaderType;
    codecs.pStreamAv->progs[idx].pCbData = pPesAud;
    if(pDataSrcVid) {
      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
        codecs.pStreamAv->progs[idx].pavs[outidx] = &pCfg->status.avs[outidx]; 
      }
    }
    //for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
    //  if(pCfg->status.avs[outidx].offset0 >= 0) {
    //    codecs.pStreamAv->progs[idx].pavs[outidx] = &pCfg->status.avs[outidx]; 
    //  }
    //}

    if(pStreamAv == NULL) {
      codecs.pStreamAv->progs[idx].streamId = MP2PES_STREAMID_PRIV_START;
      codecs.pStreamAv->progs[idx].streamType = audStreamType;
      //if(pAv) {
      //  codecs.pStreamAv->progs[idx].codecType = pAv->audCodecType;
      //}
    }

    // Only enable the output stream type to vary if streaming live capture 
    // and not doing transcoding
    if(pCfg->xcode.aud.common.cfgDo_xcode == 0 &&
       codecs.pStreamAv->progs[idx].streamType == MP2PES_STREAMTYPE_NOTSET) {
       pPesAud->pOutStreamType = &codecs.pStreamAv->progs[idx].streamType;
    }

    if(pCfg->noaud) {
      codecs.pStreamAv->progs[idx].inactive = 1;
    }
    idx++;
  }

  codecs.pStreamAv->numProg = idx;

  if(pDataSrcVid && pCfg->xcode.vid.common.cfgDo_xcode) {

    // Increase the audio frames queue to account for video encoder output lag
    //audQsz += 120;

    //
    // sequence start is set to 0 - and will be set when codec specific
    // sequence start headers are received in the input stream
    //
    ppHaveSeqStartVid = &pPesVid->pHaveSeqStart;
    if(pDataSrcAud) {
      ppHaveSeqStartAud = &pPesAud->pHaveSeqStart;
    }

  }

  //
  // av adjustment presets for input mpeg2-ts stream only 
  //
  if(haveVid && haveAud && pCfg->status.faoffset_m2tpktz != 0) {  
    codecs.pStreamAv->progs[1].avoffset_pktz = 90000 * pCfg->status.faoffset_m2tpktz;
  }

  if(pPes && haveVid && haveAud) {

    //if(pCfg->xcode.vid.common.cfgDo_xcode == 0) {

      // avSyncOffsetHz is applied to any pts/dts written on the output stream
      if(pCfg->status.faoffset_m2tpktz == 0) {
        codecs.pStreamAv->progs[1].avoffset_pktz = -45000;
      }
 
    //}

    if(!pCfg->xcode.vid.common.cfgDo_xcode && !pCfg->xcode.aud.common.cfgDo_xcode &&
       pCfg->status.haveavoffsets0[0] == 0) {
      //TODO: seems like this offset needs to be zero when playing locally
      // W/ PES avc -> PES avc (->vlc), it should be set to ~ 45000
      pCfg->status.avs[0].offset0 = pCfg->status.avs[0].offsetCur = 45000;
    }
  }

  if(codecs.pStreamAv->progs[1].avoffset_pktz != 0) {
    LOG(X_DEBUG("Applying A/V mp2ts packetization offset %.3fs"),
                PTSF(codecs.pStreamAv->progs[1].avoffset_pktz));
  }

  //
  // av.avoffset0 can be set here or via prior configuration as the desired base
  // offset.
  // av.offsetCur is adjusted automatically by xcode.c to accomodate for
  // video / audio encoder lag
  //
  //if(pCfg->status.av.offsetCur != 0) {
  //  LOG(X_DEBUG("Applying A/V input offset %.3f"), PTSF(pCfg->status.av.offsetCur));
  //}

  if(rc == 0) {
    rc = stream_av_xcode(&codecs, pCfg, 
                         (pPesVid ? &pPesVid->pXcodeData : NULL),
                         (pPesAud ? &pPesAud->pXcodeData : NULL),
                         ppHaveSeqStartVid, ppHaveSeqStartAud,
                         vidCodecType, audCodecType);
  }

/*
  if(pPes) {
    //TODO: pPes should be deprecated... queue should be created in capture_net 
    if(pPesVid) {
      pktqueue_destroy(pPesVid->pQueue);
    }
    if(pPesAud) {
      pktqueue_destroy(pPesAud->pQueue);
    }
  }
*/

  return rc;
}

int stream_mp2ts_raw_file(const char *path,
                          STREAMER_CFG_T *pCfg,
                          float tmMinPktIntervalMs,
                          unsigned int mp2tspktlen) {
  // 
  // This has been deprecated infavor of
  // stream_mp2ts_pes which extracts each elementary stream
  // from the transport stream
  //
  int rc = 0;
  STREAM_DATASRC_T replayCfg;
  MP2TS_CONTAINER_T *pMp2ts = NULL;

  if(!(pMp2ts = mp2ts_open_checklive(path, pCfg))) {
    return -1;
  }

  memset(&replayCfg, 0, sizeof(replayCfg));
  replayCfg.pContainer = pMp2ts;
  replayCfg.pCbData = pMp2ts->pStream;
  replayCfg.cbGetData = stream_rtp_mp2tsraw_cbgetfiledata;
  replayCfg.cbHaveData = stream_rtp_mp2tsraw_cbhavefiledata;
  replayCfg.cbResetData = stream_rtp_mp2tsraw_cbresetfiledata;

  rc = stream_mp2ts_raw(&replayCfg, pCfg, tmMinPktIntervalMs, mp2tspktlen);

  mp2ts_close(pMp2ts);

  return rc;
}

int stream_mp2ts_raw(const STREAM_DATASRC_T *pDataSrc,
                     STREAMER_CFG_T *pCfg,
                     float tmMinPktIntervalMs,
                     unsigned int mp2tspktlen) {

  int rc = 0;
  STREAM_XMIT_NODE_T stream;
  STREAM_RTP_MP2TS_REPLAY_T streamMp2ts;
  //unsigned int outidx;
  unsigned int idxDest;
  char tmp[128];

  if(!pCfg || pCfg->numDests >= *pCfg->pmaxRtp ||
     (pCfg->action.do_output && pCfg->numDests < 1) ||
     (!pCfg->action.do_output && !pCfg->action.do_rtmplive && 
      !pCfg->action.do_rtsplive && !pCfg->action.do_rtspannounce &&
      !pCfg->action.do_flvlive && !pCfg->action.do_flvliverecord && !pCfg->action.do_moofliverecord &&
       !pCfg->action.do_mkvlive && !pCfg->action.do_mkvliverecord && !pCfg->action.do_tslive && !pCfg->action.do_httplive)) {
    LOG(X_ERROR("No stream output set"));
    return -1;
  }

  //numDests = validateDests(pCfg);
  if(pCfg->pdestsCfg[0].dstHost[0] != '\0' && pCfg->pdestsCfg[0].ports[0] == 0) {
  //if(pCfg->pdestsCfg[0].ports[0] <= 0) {
    LOG(X_ERROR("Writing to local output file not allowed for replay."));
    return -1;
  } else if(pCfg->proto != STREAM_PROTO_MP2TS) {
    LOG(X_ERROR("Stream protocol other than m2t not supported for replay of m2t."));
    return -1;
  } 

  memset(&streamMp2ts, 0, sizeof(streamMp2ts));
  memset(&stream, 0, sizeof(stream));
  streamMp2ts.replayUseLocalTiming = pCfg->u.cfgmp2ts.replayUseLocalTiming;
  stream.pfrtcp_sr_intervalsec = &pCfg->frtcp_sr_intervalsec;
  if(!pCfg->action.do_output_rtphdr) {
    pCfg->frtcp_sr_intervalsec = 0;
  }

  if(rtpmultiset_init(&streamMp2ts.rtpMulti, 1, 0, *pCfg->pmaxRtp) < 0) {
    return -1;
  }
  streamMp2ts.rtpMulti.pStreamerCfg = pCfg;;

  if(stream_init_params(pCfg, &streamMp2ts.rtpMulti,
                        MEDIA_FILE_TYPE_MP2TS, 90000, 0, pCfg->frtcp_sr_intervalsec, 0, 0) < 0) {
    return -1;
  }

  stream.prtp_sequences_at_end[0] = &pCfg->cfgrtp.sequences_at_end[0]; 
  stream.pXmitAction = &pCfg->action;
  stream.pXmitCbs = &pCfg->cbs[0];
  if(pCfg->pSleepQs[0]) {
    stream.pSleepQ = pCfg->pSleepQs[0];
  }

  init_xmit_mp2tsraw(&stream, &streamMp2ts);

  // impose max pkt/sec throttling
  //streamMp2ts.tmMinPktInterval = 2000;
  streamMp2ts.tmMinPktInterval = (TIME_VAL) ((float)tmMinPktIntervalMs * 1000);
  streamMp2ts.mp2tslen = mp2tspktlen;

  memcpy(&streamMp2ts.cfg, pDataSrc, sizeof(STREAM_DATASRC_T));
  if(stream_rtp_mp2tsraw_init(&streamMp2ts) < 0) {
    stream_setstate(pCfg, STREAMER_STATE_ERROR, 1);
    return -1;
  }

  if(pCfg->status.useStatus) {
    stream.pBwDescr = &pCfg->status.xmitBwDescr;
    streamMp2ts.pVidProp = &pCfg->status.vidProps[0];

    if(pDataSrc->pContainer) {
      if(!pDataSrc->pContainer->liveFile) {
        pCfg->status.durationMs = (double) pDataSrc->pContainer->durationHz/90.0f;
        pCfg->status.locationMs = 0.0f;
        streamMp2ts.plocationMs = &pCfg->status.locationMs;
        streamMp2ts.locationMsStart = 0;
      }
      pCfg->status.pFileStream = pDataSrc->pContainer->pStream;
    }

  }

  if(pDataSrc->pContainer) {

    if(pDataSrc->pContainer->liveFile) {

      //
      // jump to end of live file 
      //
      mp2ts_jump_file_offset(streamMp2ts.cfg.pContainer->pStream, 100, 
                             MP2TS_LEN * TS_OFFSET_PKTS);

    } else if((streamMp2ts.pCurStreamChange = mp2ts_jump_file(streamMp2ts.cfg.pContainer, 
                                             pCfg->fStart,
                                             &streamMp2ts.locationMsStart)) == NULL) {
      stream_setstate(pCfg, STREAMER_STATE_ERROR, 1);
      return -1;
    }
  }

  stream.verbosity = pCfg->verbosity;
  stream.pLic = &pCfg->lic;
  stream.pRtpMulti = &streamMp2ts.rtpMulti;

  if(streamer_add_recipients(pCfg, stream.pRtpMulti, 1) < 0) {
    stream_setstate(pCfg, STREAMER_STATE_ERROR, 1);
    return -1;
  }

  for(idxDest = 0; idxDest < pCfg->numDests; idxDest++) {
     LOG(X_INFO("Streaming to: %s:%d %s"),
        FORMAT_NETADDR(stream.pRtpMulti->pdests[idxDest].saDsts, tmp, sizeof(tmp)),
        ntohs(INET_PORT(stream.pRtpMulti->pdests[idxDest].saDsts)), stream.descr);
  }

  if(pCfg->action.do_tslive || pCfg->action.do_httplive) {
    LOG(X_DEBUG("Stream available via %s%s"),
        pCfg->action.do_tslive ? "live " : "",
        pCfg->action.do_httplive ? "httplive" : "");
  }

  stream.prunning = &pCfg->running;
  stream.pXmitDestRc = pCfg->pxmitDestRc;
  //for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
    stream.pLiveQ = &pCfg->liveQs[0];
  //}
  stream.pLiveQ2 = NULL;

  do {

    pthread_mutex_lock(&pCfg->mtxStrmr);
    stream_setstate(pCfg, STREAMER_STATE_RUNNING, 0);
    pCfg->jump.type = JUMP_OFFSET_TYPE_NONE;
    pCfg->pauseRequested = 0;
    pthread_mutex_unlock(&pCfg->mtxStrmr);

    //TODO: deprecate in-favor of streamer2
    stream_udp(&stream, 0);

    if(pCfg->pauseRequested) {

      if(pDataSrc->pContainer) {
        pCfg->jump.u.jumpOffsetSec = (float) (*(streamMp2ts.plocationMs) / 1000.0);
      } else {
        pCfg->jump.u.jumpOffsetSec = 0;
      }
      while(pCfg->pauseRequested && !pCfg->quitRequested) {
        usleep(50000);
      }
      pCfg->pauseRequested = 0;
      if(!pCfg->quitRequested) {
        pCfg->jump.type = JUMP_OFFSET_TYPE_SEC;
      }

      streamMp2ts.rtpMulti.pRtp->sequence_num = 
          htons(ntohs(streamMp2ts.rtpMulti.pRtp->sequence_num)-1);

    }

    if((pCfg->loop && pCfg->running == STREAMER_STATE_RUNNING) || 
       pCfg->jump.type != JUMP_OFFSET_TYPE_NONE) {

      if(pCfg->loop && pCfg->jump.type == JUMP_OFFSET_TYPE_NONE) {
        streamMp2ts.locationMsStart = 0;
        pCfg->jump.u.jumpOffsetSec = 0;
      }

      if(pDataSrc->pContainer && (pCfg->loop || pCfg->jump.type != JUMP_OFFSET_TYPE_NONE)) {

        //streamMp2ts.rtpMulti.m_pRtp->sequence_num = 
        //   htons(ntohs(streamMp2ts.rtpMulti.m_pRtp->sequence_num)-1);
        if(pCfg->jump.type == JUMP_OFFSET_TYPE_SEC) {
          if((streamMp2ts.pCurStreamChange = mp2ts_jump_file(streamMp2ts.cfg.pContainer,
                  pCfg->jump.u.jumpOffsetSec, &streamMp2ts.locationMsStart)) == NULL) {
            rc = -1;
            break;
          }

        } else if(pCfg->jump.type == JUMP_OFFSET_TYPE_BYTE ||
                  (pCfg->loop && pCfg->jump.type == JUMP_OFFSET_TYPE_NONE && pCfg->jump.u.jumpPercent < 99)) {
          mp2ts_jump_file_offset(streamMp2ts.cfg.pContainer->pStream, pCfg->jump.u.jumpPercent, 
                                 TS_OFFSET_PKTS * MP2TS_LEN);
        }

      } else if(streamMp2ts.cfg.cbResetData(streamMp2ts.cfg.pCbData) < 0) {
        rc = -1;
        break;
      }

      if(stream_rtp_mp2tsraw_reset(&streamMp2ts) < 0) {
        rc = -1;
        break;
      }
    }

  } while(!g_proc_exit && !pCfg->quitRequested && 
         ((pCfg->loop && pCfg->running == STREAMER_STATE_RUNNING) || pCfg->jump.type != JUMP_OFFSET_TYPE_NONE));


  stream_rtp_mp2tsraw_close(&streamMp2ts);
  stream_rtp_close(stream.pRtpMulti);

  pthread_mutex_lock(&pCfg->mtxStrmr);
  stream_setstate(pCfg, STREAMER_STATE_FINISHED, 0);
  pCfg->cfgrtp.sequences_at_end[0] = htons(streamMp2ts.rtpMulti.pRtp->sequence_num);
  pthread_mutex_unlock(&pCfg->mtxStrmr);

  return rc;
}

static int stream_av_xcode(CODEC_PAIR_T *pCodecs,
                           STREAMER_CFG_T *pCfg,
                           STREAM_XCODE_DATA_T **ppXcodeDataVid,
                           STREAM_XCODE_DATA_T **ppXcodeDataAud,
                           int **ppHaveSeqStartVid,
                           int **ppHaveSeqStartAud,
                           XC_CODEC_TYPE_T vidCodecType,
                           XC_CODEC_TYPE_T audCodecType) {
  int rc = 0;
  STREAM_XCODE_CTXT_T *pXcodeCtxt = NULL;
  PKTQUEUE_CONFIG_T pipVFrameCfg;
  unsigned int outidx;
  unsigned int vidOutBufSz = 0;
  unsigned int fpsInNHint = 0;
  unsigned int fpsInDHint = 0;
  unsigned int progIdxAudio = (ppXcodeDataVid ? 1 : 0);

  if(((!pCodecs->pH264 && !pCodecs->pMpg4v && !pCodecs->pVp8 && !pCodecs->pConferenceV) && 
      (!pCodecs->pAac && !pCodecs->pAmr)) && 
     !pCodecs->pStreamAv && !pCodecs->pImage) {
    LOG(X_ERROR("No supported content to stream"));
    return -1;
  }

  if(pCodecs->pH264) {
    fpsInNHint = pCodecs->pH264->clockHz;
    fpsInDHint = pCodecs->pH264->frameDeltaHz;
  } else if(pCodecs->pMpg4v) {
    fpsInNHint = pCodecs->pMpg4v->param.clockHz;
    fpsInDHint = pCodecs->pMpg4v->param.frameDeltaHz;
  }

  if(!(pXcodeCtxt = pCfg->pXcodeCtxt)) {
    return -1;
  }

  if(ppXcodeDataVid) {
    *ppXcodeDataVid = &pXcodeCtxt->vidData;
    (*ppXcodeDataVid)->piXcode = &pCfg->xcode;
    if(pCfg->status.useStatus) {
      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
        (*ppXcodeDataVid)->pVidProps[outidx] = &pCfg->status.vidProps[outidx];
      }
    }
    (*ppXcodeDataVid)->inStreamType = mp2_getStreamType(vidCodecType);
    if((*ppXcodeDataVid)->inStreamType != MP2PES_STREAMTYPE_NOTSET) {
      (*ppXcodeDataVid)->piXcode->vid.common.cfgFileTypeIn = vidCodecType;
    }

    if(pCfg->xcode.vid.common.cfgFileTypeOut == XC_CODEC_TYPE_UNKNOWN) {
      pCfg->xcode.vid.common.cfgFileTypeOut = pCfg->xcode.vid.common.cfgFileTypeIn;
    }

    //fprintf(stderr, "cfgFileTypeIn:%d cfgFileTypeOut:%d, vidCodecType:%d, inStreamType:%d\n", pCfg->xcode.vid.common.cfgFileTypeIn, pCfg->xcode.vid.common.cfgFileTypeOut, vidCodecType, (*ppXcodeDataVid)->inStreamType);

    if(ppXcodeDataAud) {
      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
        (*ppXcodeDataVid)->pavs[outidx] = &pCfg->status.avs[outidx];
        pCodecs->vs.streamAv.progs[0].pavs[outidx] = (*ppXcodeDataVid)->pavs[outidx];
      }
    }
    (*ppXcodeDataVid)->pignoreencdelay_av = &pCfg->status.ignoreencdelay_av;
    pCodecs->vs.streamAv.progs[0].pXcodeData = &pXcodeCtxt->vidData;
  }

  if(ppXcodeDataAud) {
    *ppXcodeDataAud = &pXcodeCtxt->audData;
    (*ppXcodeDataAud)->piXcode = &pCfg->xcode;
    (*ppXcodeDataAud)->inStreamType = mp2_getStreamType(audCodecType);
    if((*ppXcodeDataAud)->inStreamType != MP2PES_STREAMTYPE_NOTSET) {
      (*ppXcodeDataAud)->piXcode->aud.common.cfgFileTypeIn = audCodecType;

      //
      // This is a real bad way to get the SILK decoder input clock for xcode_aud::xcode_aud_silk
      //
      if(pCfg->xcode.aud.cfgSampleRateIn == 0) {
        if(audCodecType == XC_CODEC_TYPE_SILK) {
          pCfg->xcode.aud.cfgSampleRateIn = DEFAULT_CLOCKHZ(STREAM_IDX_AUD, 0, pCfg->sdpsx[1][0].aud, 0); 
        } else if( audCodecType == XC_CODEC_TYPE_OPUS) {
          // Opus is always advertised to be at 48KHz, 
          pCfg->xcode.aud.cfgSampleRateIn = STREAM_RTP_OPUS_CLOCKHZ;
        }
      }
    }

    if(ppXcodeDataVid) {
      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
        (*ppXcodeDataAud)->pavs[outidx] = &pCfg->status.avs[outidx];
        pCodecs->vs.streamAv.progs[(ppXcodeDataVid ? 1 : 0)].pavs[outidx] = (*ppXcodeDataAud)->pavs[outidx];
      }
    }
    (*ppXcodeDataAud)->pignoreencdelay_av = &pCfg->status.ignoreencdelay_av;
    pCodecs->vs.streamAv.progs[(ppXcodeDataVid ? 1 : 0)].pXcodeData = &pXcodeCtxt->audData;
  }

  //
  // If the (delayed) load xcode config file is sent, do delayed initialization based on
  // input stream properties, such as dimensions
  //
  if(pCfg->xcodecfgfile) {
    pXcodeCtxt->vidUData.flags |= STREAM_XCODE_FLAGS_LOAD_XCFG;
    pXcodeCtxt->vidUData.xcodecfgfile = pCfg->xcodecfgfile; 
  }

  //
  // For raw output, the output tmpFrameBuf will store raw frames
  // instead of reusing the input capture buffer
  // (the same buffer is used to store upsample content)
  //
  if(!pCfg->xcode.vid.pip.active) {
    if((IS_XC_CODEC_TYPE_VID_RAW(pCfg->xcode.vid.common.cfgFileTypeOut) &&
       (vidOutBufSz = pCfg->xcode.vid.out[0].cfgOutH * pCfg->xcode.vid.out[0].cfgOutV * 
          codecType_getBitsPerPx(pCfg->xcode.vid.common.cfgFileTypeOut) / 8) == 0) ||
       (pCfg->xcode.vid.common.cfgFileTypeIn == XC_CODEC_TYPE_VID_CONFERENCE &&
       (vidOutBufSz = pCfg->xcode.vid.out[0].cfgOutH * pCfg->xcode.vid.out[0].cfgOutV * 
          codecType_getBitsPerPx(pCfg->xcode.vid.common.cfgFileTypeIn) / 8) == 0)) {
      LOG(X_ERROR("Raw output dimensions not set"));
      return -1;
    }
  }

  //fprintf(stderr, "PIP:%d maxPktLen:%d\n", pCfg->xcode.vid.pip.active, pCfg->pStorageBuf->streamerCfg.action.outfmtQCfg.cfgRtmp.maxPktLen);

  //
  // If this is an active PIP thread, create the queue used to read video frames to be sent
  // to each PIP (conference endpoint), which may have been encoded by the main conference overlay
  // encoder
  //
  memset(&pipVFrameCfg, 0, sizeof(pipVFrameCfg));
  if(pCfg->xcode.vid.pip.active && !pCfg->xcode.vid.pip.doEncode && 
    !IS_XC_CODEC_TYPE_VID_RAW(pCfg->xcode.vid.common.cfgFileTypeOut)) {
    pipVFrameCfg.maxPkts = 8;
    pipVFrameCfg.maxPktLen = pCfg->pStorageBuf->streamerCfg.action.outfmtQCfg.cfgRtmp.maxPktLen;
    pipVFrameCfg.growMaxPktLen = pCfg->pStorageBuf->streamerCfg.action.outfmtQCfg.cfgRtmp.growMaxPktLen;
    LOG(X_DEBUG("PIP reading shared encoder video frames"));
  } 

  if(xcodectxt_allocbuf(pXcodeCtxt, vidOutBufSz, &pipVFrameCfg)){
    return -1;
  }

  //fprintf(stderr, "CALLED XCODECTXT_ALLOCBUF tid:0x%x, 0x%x, suggested:%d tmpFrame:%d, UData: 0x%x pip frq(%d,%d)\n", pthread_self(), pXcodeCtxt, vidOutBufSz, pXcodeCtxt->vidUData.tmpFrame.allocSz, &pXcodeCtxt->vidUData, pXcodeCtxt->vidUData.pPipFrQ ? pXcodeCtxt->vidUData.pPipFrQ->cfg.maxPkts : 0, pXcodeCtxt->vidUData.pPipFrQ ? pXcodeCtxt->vidUData.pPipFrQ->cfg.growMaxPktLen : 0);

  pCodecs->vs.streamAv.progs[0].frameData.mediaType = vidCodecType;
  pCodecs->vs.streamAv.progs[progIdxAudio].frameData.mediaType = audCodecType;

  if(pCfg->xcode.vid.common.cfgDo_xcode || pCfg->xcode.aud.common.cfgDo_xcode) {

    if(xcodectxt_init(pXcodeCtxt, pCfg->xcode.vid.common.cfgDo_xcode ?
         ((float)pCfg->xcode.vid.cfgOutFrameDeltaHz / pCfg->xcode.vid.cfgOutClockHz) : 0,
         pCfg->xcode.aud.common.cfgDo_xcode, pCfg->xcode.vid.common.cfgAllowUpsample,
         fpsInNHint, fpsInDHint) < 0) {
      return -1;
    }

    if(pCfg->xcode.vid.common.cfgDo_xcode) {

      if(ppHaveSeqStartVid) {
        (*ppHaveSeqStartVid) = &pXcodeCtxt->vidUData.haveSeqStart;
      }

      if((pCodecs->vs.streamAv.progs[0].streamType = 
                     mp2_getStreamType(pCfg->xcode.vid.common.cfgFileTypeOut)) ==
                     MP2PES_STREAMTYPE_NOTSET) {
        LOG(X_ERROR("Unsupported output video codec %d"), 
                    pCfg->xcode.vid.common.cfgFileTypeOut);
        rc = -1;
      }
      pCodecs->vs.streamAv.progs[0].frameData.mediaType = pCfg->xcode.vid.common.cfgFileTypeOut;

#if defined(VSX_HAVE_LICENSE) && defined(VSX_HAVE_LICENSE_WATERMARK)
      if(!(pCfg->lic.capabilities & LIC_CAP_NO_WATERMARK)) {
        pCfg->xcode.vid.usewatermark = 1;
      }
#endif // VSX_HAVE_LICENSE

      //TODO: enable pat/pmt caching for apple live http
      // thus pat/pmt will be pre-pended to any keyframe based segmenting
      //streamMp2ts.cacheLastPat = streamMp2ts.cacheLastPmt = 1;

    }

    if(pCfg->xcode.aud.common.cfgDo_xcode) {

      if(ppHaveSeqStartAud) {
        (*ppHaveSeqStartAud) = &pXcodeCtxt->audUData.haveSeqStart;
      }

      if((pCodecs->vs.streamAv.progs[progIdxAudio].streamType = 
                     mp2_getStreamType(pCfg->xcode.aud.common.cfgFileTypeOut)) ==
                     MP2PES_STREAMTYPE_NOTSET) {
        LOG(X_ERROR("Unsupported output audio codec %d"), pCfg->xcode.aud.common.cfgFileTypeOut);
        rc = -1;
      }
      pCodecs->vs.streamAv.progs[progIdxAudio].frameData.mediaType = pCfg->xcode.aud.common.cfgFileTypeOut;
    }

  }

#ifdef XCODE_IPC
  if(rc == 0 && (pCfg->xcode.vid.common.cfgDo_xcode || pCfg->xcode.aud.common.cfgDo_xcode)) {
    if(!(pCfg->xcode.vid.common.pIpc = xcode_ipc_open(XCODE_IPC_MEM_SZ, XCODE_IPC_SHMKEY))) {
      rc = -1;
    } else {
      pCfg->xcode.aud.common.pIpc = pCfg->xcode.vid.common.pIpc;
    }
  }
#endif // XCODE_IPC

  if(rc == 0) {

    //
    // If the configuration is set to '2', it is assumed each avs[] has its own explicit a/v 
    // offset.
    // If the configuration is set to '1', the user has specified an global a/v offset 
    // applicable to all output encodings, which should be disabled for the passthru [0] index
    //
    if(pCfg->status.haveavoffsets0[0] == 1 && pCfg->xcode.vid.out[0].passthru) {
      pCfg->status.avs[0].offset0 = pCfg->status.avs[0].offsetCur = 0;
      pCfg->status.haveavoffsets0[0] = 0;
    }

    //
    // av.avoffset0 can be set here or via prior configuration as the desired base
    // offset.
    // av.offsetCur is adjusted automatically by xcode.c to accomodate for
    // video / audio encoder lag
    //
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if(pCfg->status.avs[outidx].offsetCur != 0 && 
         (outidx == 0 ||  pCfg->xcode.vid.out[outidx].active)) {

        LOG(X_DEBUG("Applying A/V input offset[%d] %s%.3f sec."), outidx, 
            pCfg->status.avs[outidx].offsetCur > 0 ? "+" : "", 
            PTSF(pCfg->status.avs[outidx].offsetCur));

      }
    }

    //
    // sharedCtxt is used for rtcp exchange between capture & streamer threads
    //

    //pCfg->sharedCtxt.active = 1;

    rc = stream_av(pCodecs, pCfg);
  }
  
  //
  // Since this is a PIP streamer which is already done processing input
  // sleep efficiantly until the main streamer is done to prevent closing
  // pip xcode context
  //

  if(pCfg->xcode.vid.pip.active) {
    if(!(pCfg->xcode.vid.pip.flags & PIP_FLAGS_CLOSE_ON_END)) {
      //LOG(X_DEBUG("PIP_STOP... streamer_rtp wait on conditional. running:%d"), pCfg->running);
      vsxlib_cond_wait(&pCfg->pip.cond, &pCfg->pip.mtxCond);
      //LOG(X_DEBUG("PIP_STOP... streamer_rtp done wait on conditional. running:%d"), pCfg->running);
    }
    if(pCfg->pip.pCfgOverlay) {
      //LOG(X_DEBUG("PIP_STOP... streamer_rtp deleting conditional???? why here..."));
      //pthread_cond_destroy(&pCfg->pip.cond);
      //pthread_mutex_destroy(&pCfg->pip.mtxCond);
    }
  }
  //
  // sharedCtxt is used for rtcp exchange between capture & streamer threads
  //
  pthread_mutex_lock(&pCfg->sharedCtxt.mtxRtcpHdlr);
  pCfg->sharedCtxt.active = 0;
  pthread_mutex_unlock(&pCfg->sharedCtxt.mtxRtcpHdlr);
  //LOG(X_DEBUG("PIP_STOP... streamer_rtp exiting...%d"), pCfg->running);

  //pCfg->running = STREAMER_STATE_FINISHED;

  //
  // xcode_init_[vid|aud] are called automatically from
  // xcode_[vid|aud], but the close function must be explicitly
  // called on xcoder de-init
  //
  if(pCfg->xcode.vid.common.cfgDo_xcode) {
    xcode_close_vid(&pCfg->xcode.vid);
    pCfg->xcode.vid.common.is_init = 0;
  }
  if(pCfg->xcode.aud.common.cfgDo_xcode) {
    xcode_close_aud(&pCfg->xcode.aud);
    pCfg->xcode.aud.common.is_init = 0;
  }

  xcodectxt_close(pXcodeCtxt);

#ifdef XCODE_IPC
  if(pCfg->xcode.vid.common.pIpc) {
    xcode_ipc_close(pCfg->xcode.vid.common.pIpc);
  }
#endif // XCODE_IPC

  stream_setstate(pCfg, STREAMER_STATE_FINISHED, 1);

  return rc;
}

PKTQUEUE_T *stream_createFrameQ(enum STREAM_FRAMEQ_TYPE type,
                                unsigned int numSlots,
                                unsigned int nonDefaultSlotSz) {

  PKTQUEUE_T *pQ = NULL;
  unsigned int maxSlots = 0;
  unsigned int szSlotMax = 0;
  unsigned int szSlot = nonDefaultSlotSz;
  unsigned int prebufOffset = 0;
  unsigned int growMultiplier = 0;
  int id = 0;

  if(STREAM_FRAMEQ_TYPE_VID(type)) {

    if(szSlot == 0) {
      szSlot = FRAMESZ_VIDQUEUE_SZFRAME_DFLT; 
    }
    prebufOffset = FRAMESZ_VIDQUEUE_PREBUF_BYTES;
    id = STREAMER_QID_VID_FRAMES;

  } else {

    if(szSlot == 0) { 
      szSlot = FRAMESZ_AUDQUEUE_SZFRAME_DFLT; 
    }
    id = STREAMER_QID_AUD_FRAMES;

  }

  if(STREAM_FRAMEQ_TYPE_FRAMEBUF(type)) {

    if(!(pQ = framebuf_create(szSlot, prebufOffset))) {
      LOG(X_ERROR("Failed to allocate frame buffer for slot size %d"), szSlot);
      return NULL;
    }

  } else if(STREAM_FRAMEQ_TYPE_RINGBUF(type)) {

    if(!(pQ = ringbuf_create(szSlot))) {
      LOG(X_ERROR("Failed to allocate ring buffer for slot size %d"), szSlot);
      return NULL;
    }

  } else {
    growMultiplier = FRAMESZ_GROW_MULTIPLIER;
    szSlotMax = szSlot * growMultiplier;
    maxSlots = numSlots;
    if(!(pQ = pktqueue_create(numSlots, szSlot, maxSlots, szSlotMax, prebufOffset, 0, 1))) {
      LOG(X_ERROR("Failed to allocate frame queue %d x %d"), numSlots, szSlot);
      return NULL;
    }
  }


  pQ->cfg.id = id;
  pQ->cfg.overwriteType = PKTQ_OVERWRITE_FIND_KEYFRAME;
  //pQ->cfg.growSzOnlyAsNeeded = 20;
  LOG(X_DEBUG("Allocated %s frame %s(id:%d) %d x %d (%d x %d)"),
      (STREAM_FRAMEQ_TYPE_VID(type) ? "video" : "audio"), 
      ((STREAM_FRAMEQ_TYPE_FRAMEBUF(type) || 
        STREAM_FRAMEQ_TYPE_RINGBUF(type)) ? "buffer" : "queue"),
      pQ->cfg.id, numSlots, szSlot, (szSlotMax > 0 ? maxSlots : 0), szSlotMax);

  return pQ;
}

int streamer_stop(STREAMER_CFG_T *pCfg, int quit, int pause, int delay, int lock) {

  unsigned int idxQ;

  if(!pCfg) {
    return -1;
  }

  //
  // The following code assumes we are called only once from the capture thread
  //
  if(delay && !g_proc_exit) {
    if(pCfg->status.msDelayTx > 0) {
      LOG(X_DEBUG("Transmitter waiting for %d ms to accomodate output delay"), pCfg->status.msDelayTx);
      usleep((pCfg->status.msDelayTx + 500)* 1000);
    //} else if(pCfg->xcode.vid.common.cfgDo_xcode) {
    //  LOG(X_DEBUG("SSSSS"));
    //  usleep(2000 * 1000);
    }
  }

  if(lock) {
    pthread_mutex_lock(&pCfg->mtxStrmr);
  }

  pCfg->jump.type = JUMP_OFFSET_TYPE_NONE;
  pCfg->pauseRequested = pause;
  pCfg->quitRequested = quit;
  stream_setstate(pCfg, STREAMER_STATE_INTERRUPT, !lock);

  for(idxQ = 0; idxQ < 2; idxQ++ ) {
    if(pCfg->pSleepQs[idxQ]) {
      pCfg->pSleepQs[idxQ]->quitRequested = 1;
      pktqueue_wakeup(pCfg->pSleepQs[idxQ]);
    }
  }

  if(lock) {
    pthread_mutex_unlock(&pCfg->mtxStrmr);
  }

  return 0;
}


#endif // VSX_HAVE_STREAMER
