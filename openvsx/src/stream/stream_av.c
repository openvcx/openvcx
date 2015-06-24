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
#include "pktcapture.h"
#include "pktgen.h"

#if defined(VSX_HAVE_STREAMER) 

//
// This code here has emerged to be quite a nightmare and really needs to be rewritten cleanly
//

//#define DEBUG_MIXER_TIMING 1

#define SET_DTS(pts, dts) (((pts) > (dts))  ? ((pts) - (dts)) : -1 * ((dts) - (pts)))

static int loadFrameData(AV_PROG_T *pProg, unsigned char *pData, unsigned int szData) {

  int szCopy = 0;
  unsigned int idxData = 0;

  while(idxData < szData) {

    if(pProg->frameWrIdx == 0 ||
       (szCopy = pProg->stream.cbSzRemainingInSlice(pProg->pCbData)) == 0) {

      if(pProg->stream.cbGetNextFrameSlice(pProg->pCbData) == NULL) {
        LOG(X_ERROR("Unable to get next frame slice"));
        return -1;
      }

      szCopy = pProg->stream.cbSzRemainingInSlice(pProg->pCbData);
    }

    if(szCopy > (int)(szData - idxData)) {
      szCopy = szData - idxData;
    }

    if(pProg->stream.cbCopyData(pProg->pCbData, &pData[idxData], szCopy) != szCopy) {
      return -1;
    }

    //fprintf(stderr, "sz:%d\n", szData); avc_dumpHex(stderr, &pData[idxData], 16, 0);

    pProg->frameWrIdx += szCopy;
    idxData += szCopy;
  }

  //fprintf(stderr, "loaded fr data prog:0x%x len:%d\n", pProg->progId, szData);

  return szData;
}

static int update_packetizers(AV_PROG_T *pProgArg) {
  STREAM_AV_T *pAv = pProgArg->pAv;
  AV_PROG_T *pProg;
  unsigned int idxprog;
  unsigned int pktzidx;
  unsigned int outidx;
  int active;

  //
  // Turn off any active packetizers for encoder outidx that is currently not active
  // such as after performing delayed xcoder config initialization
  //

  for(idxprog = 0; idxprog < AV_MAX_PROGS; idxprog++) {

    if(!(pProg = (AV_PROG_T *) &pAv->progs[idxprog])) {
      continue;
    }

    for(pktzidx = 0; pktzidx < pProg->numpacketizers; pktzidx++) {

      outidx = pProg->pktz[pktzidx].outidx;

//fprintf(stderr, "PACKETIZER pktz[%d] do_mask:0x%x outidx:%d pktz isactive:%d xcode active:%d\n", pktzidx, pProg->pktz[pktzidx].pktz_do_mask, outidx, pProg->pktz[pktzidx].isactive, pProg->pXcodeData->piXcode->vid.out[outidx].active);
      if(pProg->pXcodeData && pProg->pXcodeData->piXcode && pProg->pktz[pktzidx].isactive) {

        if(pProg->pXcodeData->piXcode->vid.common.cfgDo_xcode) {
          active = pProg->pXcodeData->piXcode->vid.out[outidx].active;
        } else {
          active = 1;
        }

        if(!active) {
          //fprintf(stderr, "TURNING OFF PACKETIZER progidx:%d pktz[%d] do_mask:0x%x outidx:%d pktz isactive:%d xcode active:%d\n", idxprog, pktzidx, pProg->pktz[pktzidx].pktz_do_mask, outidx, pProg->pktz[pktzidx].isactive, pProg->pXcodeData->piXcode->vid.out[outidx].active);

          pProg->pktz[pktzidx].isactive = 0;
        }
      }
    }
  }

  return 0;
}

static int upsampleFrame(STREAM_XCODE_DATA_T *pXcodeData) {

  PKTQUEUE_PKT_T *pPkt = &XCODE_VID_UDATA_PTR(pXcodeData->piXcode)->tmpFrame;

  VSX_DEBUG_STREAMAV( LOG(X_DEBUG("stream_av upsample inpts:%.3f outpts:%.3f"), 
                PTSF(pXcodeData->piXcode->vid.common.inpts90Khz),  
                PTSF(pXcodeData->piXcode->vid.common.outpts90Khz)));

  pPkt->idx = 1;
  // set the frame length to 0
  pXcodeData->curFrame.pkt.len = 0;
  pXcodeData->curFrame.pkt.pData = pPkt->pData;
  pXcodeData->curFrame.pkt.pBuf = pPkt->pBuf;

  memset(&pXcodeData->curFrame.pkt.xtra, 0, sizeof(pXcodeData->curFrame.pkt.xtra));
  pXcodeData->curFrame.pData = pXcodeData->curFrame.pkt.pData;
  pXcodeData->curFrame.lenData = pXcodeData->curFrame.pkt.len;
  pXcodeData->curFrame.idxReadInFrame = 0;
  pXcodeData->curFrame.idxReadFrame = 0;
  pXcodeData->curFrame.numUpsampled = 1;

  return 0;
}

static int update_sdp_vid(STREAM_SDP_ACTION_T *pSdpAction, 
                          const AV_PROG_T *pProg,
                          SDP_DESCR_T *pSdp,
                          const OUTFMT_VID_HDRS_UNION_T *pHdrs) {
  int rc = 0;
  //SDP_DESCR_T *pSdp;
  XC_CODEC_TYPE_T codecType;
  const char *descr = NULL;
  const SPSPPS_RAW_T *pspspps;
  int isxcoded = pProg->pXcodeData->piXcode->vid.common.cfgDo_xcode;

  if(isxcoded) {
    codecType = pProg->pXcodeData->piXcode->vid.common.cfgFileTypeOut;
  } else {
    codecType = mp2_getCodecType(pProg->streamType);
  }

  //TODO: if streaming from non transcoded m2t capture , and rtp / rtsp output is present
  // it was assumed that m2t es was h264/aac - need to validated stream types here and 
  // possibly return error

  if(codecType == XC_CODEC_TYPE_H264) { 
    pspspps = &pHdrs->h264;

    if(pspspps && pspspps->sps_len > 0 && pspspps->pps_len > 0) {
      //pSdp = &((STREAMER_CFG_T *) pSdpAction->pCfg)->sdps[1];

      pSdp->vid.common.codecType = XC_CODEC_TYPE_H264;
      pSdp->vid.common.available = 1;
      strncpy(pSdp->vid.common.encodingName, SDP_RTPMAP_ENCODINGNAME_H264, sizeof(pSdp->vid.common.encodingName));
      memcpy(pSdp->vid.u.h264.profile_level_id, &pspspps->sps[1], 3);
      memcpy(&pSdp->vid.u.h264.spspps, pspspps, sizeof(SPSPPS_RAW_T));
    //pSdpAction->vidUpdated = 1;
      rc = 1;
    }

  } else if(codecType == XC_CODEC_TYPE_MPEG4V) {

    if(pHdrs->mpg4v.hdrsLen > 0) {

      //pSdp = &((STREAMER_CFG_T *) pSdpAction->pCfg)->sdps[1];
      pSdp->vid.common.codecType = XC_CODEC_TYPE_MPEG4V;
      pSdp->vid.common.available = 1;
      strncpy(pSdp->vid.common.encodingName, SDP_RTPMAP_ENCODINGNAME_MPEG4V, sizeof(pSdp->vid.common.encodingName));
      //pSdp->vid.u.mpg4v.profile_level_id = pHdrs->u.mpg4v.
      //avc_dumpHex(stderr, pSdp->vid.u.mpg4v.seqHdrs.hdrsBuf, pSdp->vid.u.mpg4v.seqHdrs.hdrsLen, 1);
      //avc_dumpHex(stderr, pHdrs->mpg4v.hdrsBuf, pHdrs->mpg4v.hdrsLen, 1);
      memcpy(&pSdp->vid.u.mpg4v.seqHdrs, &pHdrs->mpg4v, sizeof(pSdp->vid.u.mpg4v.seqHdrs));

      rc = 1;
    }

  } else if(codecType == XC_CODEC_TYPE_H263 ||
            codecType == XC_CODEC_TYPE_H263_PLUS) { 

      if(codecType == XC_CODEC_TYPE_H263_PLUS) {
        descr = SDP_RTPMAP_ENCODINGNAME_H263_PLUS;
      } else {
        descr = SDP_RTPMAP_ENCODINGNAME_H263;
      }

      //pSdp = &((STREAMER_CFG_T *) pSdpAction->pCfg)->sdps[1];
      pSdp->vid.common.codecType = codecType;
      pSdp->vid.common.available = 1;
      strncpy(pSdp->vid.common.encodingName, descr, sizeof(pSdp->vid.common.encodingName));
      //TODO: Not done here...
      //memcpy(&pSdp->vid.u.mpg4v.seqHdrs, &pHdrs->mpg4v, sizeof(pSdp->vid.u.mpg4v.seqHdrs));

      rc = 1;

  } else if(codecType == XC_CODEC_TYPE_VP8) { 
    pSdp->vid.common.codecType = XC_CODEC_TYPE_VP8;
    pSdp->vid.common.available = 1;
    strncpy(pSdp->vid.common.encodingName, SDP_RTPMAP_ENCODINGNAME_VP8, sizeof(pSdp->vid.common.encodingName));
    rc = 1;

  } else {
    rc = -1;
  }

  return rc;
}

static int update_sdp_aud(STREAM_SDP_ACTION_T *pSdpAction, 
                          const AV_PROG_T *pProg,
                          SDP_DESCR_T *pSdp,
                          const unsigned char *pAdts, unsigned int lenAdts) {
  int rc = 0;
  //SDP_DESCR_T *pSdp;
  STREAM_XCODE_AUD_UDATA_T *pUData = NULL;
  AAC_ADTS_FRAME_T aacFrame;
  XC_CODEC_TYPE_T codecType;
  const IXCODE_AUDIO_CTXT_T *pXcodeAud = &pProg->pXcodeData->piXcode->aud;
  int isxcoded = pProg->pXcodeData->piXcode->aud.common.cfgDo_xcode;

  if(isxcoded) {
    codecType = pProg->pXcodeData->piXcode->aud.common.cfgFileTypeOut;
  } else {
    codecType = mp2_getCodecType(pProg->streamType);
  }

  if(codecType == XC_CODEC_TYPE_AMRNB) {
    pSdp->aud.common.clockHz = AMRNB_CLOCKHZ;
    return 1;
  } else if(codecType == XC_CODEC_TYPE_SILK || codecType == XC_CODEC_TYPE_OPUS) {
    if(pProg->pXcodeData->piXcode->aud.common.cfgDo_xcode) {
      if(pProg->pXcodeData->piXcode->aud.cfgSampleRateOut > 0) {
        pSdp->aud.common.clockHz = pProg->pXcodeData->piXcode->aud.cfgSampleRateOut;
      }
      if(pProg->pXcodeData->piXcode->aud.cfgChannelsOut > 0) {
        pSdp->aud.channels = pProg->pXcodeData->piXcode->aud.cfgChannelsOut;
      }
    }
    return 1;
  } else if(codecType == XC_CODEC_TYPE_G711_MULAW || codecType == XC_CODEC_TYPE_G711_ALAW) {
    pSdp->aud.common.clockHz = 8000;
    return 1;
  } else if(codecType != XC_CODEC_TYPE_AAC) {
    LOG(X_ERROR("Audio codec type %s (streamtype: 0x%x) not supported for SDP creation"), 
    codecType_getCodecDescrStr(mp2_getCodecType(pProg->streamType)), pProg->streamType);
    return -1;
  }

  pUData = (STREAM_XCODE_AUD_UDATA_T *) pXcodeAud->pUserData;

  if(pUData && pAdts && lenAdts >= 7) {
    //pSdp = &((STREAMER_CFG_T *) pSdpAction->pCfg)->sdps[1];

    memset(&aacFrame, 0, sizeof(aacFrame));
    if(aac_decodeAdtsHdr(pAdts, lenAdts, &aacFrame) < 0) {
      return - 1;
    }
    memcpy(&pSdp->aud.u.aac.decoderCfg, &aacFrame.sp, sizeof(pSdp->aud.u.aac.decoderCfg));
    pSdp->aud.common.codecType = XC_CODEC_TYPE_AAC;
    pSdp->aud.common.available = 1;
    strncpy(pSdp->aud.common.encodingName, SDP_RTPMAP_ENCODINGNAME_AAC,
        sizeof(pSdp->aud.common.encodingName));

    pSdp->aud.channels = pSdp->aud.u.aac.decoderCfg.channelIdx;
    pSdp->aud.common.clockHz = esds_getFrequencyVal(ESDS_FREQ_IDX(pSdp->aud.u.aac.decoderCfg));
    pSdp->aud.u.aac.profile_level_id = 1;
    esds_encodeDecoderSp(pSdp->aud.u.aac.config, sizeof(pSdp->aud.u.aac.config),
                                 &pSdp->aud.u.aac.decoderCfg);

    //pSdpAction->audUpdated = 1;
    rc = 1;
  }

  return rc;
}

static enum STREAM_NET_ADVFR_RC update_sdp(AV_PROG_T *pProg, int isvid, enum STREAM_NET_ADVFR_RC rc) {
  unsigned int outidx;
  int i;
  int do_sdp_update = 0;
  STREAM_SDP_ACTION_T **pSdpActions = pProg->pAv->pSdpActions;

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    //TODO: cbSdpUdate is not called for 2nd output stream IF output is transcoded (hence cb is needed and not done from prior streamer_rtp) and we're not using multiple xcode output.  We still want to call the cb to create a 2nd SDP file to represent the output ports, etc...

    if(outidx > 0 && !pProg->pXcodeData->piXcode->vid.out[outidx].active) {
      continue;
      //fprintf(stderr, "AV SDP CHECK outidx[%d] cont\n", outidx);
    } else if(!pSdpActions[outidx] || pSdpActions[outidx]->calledCb ||
     ((!pSdpActions[outidx]->updateVid || pSdpActions[outidx]->vidUpdated) && 
      (!pSdpActions[outidx]->updateAud || pSdpActions[outidx]->audUpdated))) {
      //fprintf(stderr, "AV SDP CHECK outidx[%d] cont2\n", outidx);
      continue;
    }


    //fprintf(stderr, "AV SDP CHECK outidx[%d] 0x%x, calledCb:%d updV:%d,%d updA:%d,%d\n", outidx, pSdpActions[outidx] ,pSdpActions[outidx]->calledCb, pSdpActions[outidx]->updateVid, pSdpActions[outidx]->vidUpdated, pSdpActions[outidx]->updateAud, pSdpActions[outidx]->audUpdated);

    if(isvid && pSdpActions[outidx]->updateVid && !pSdpActions[outidx]->vidUpdated &&
       rc == STREAM_NET_ADVFR_RC_OK && OUTFMT_LEN_IDX(&pProg->frameData, outidx) > 0) {

       //fprintf(stderr, "TRY VID SPS in codec:%d, out codec:%d\n", pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn, pProg->pXcodeData->piXcode->vid.common.cfgFileTypeOut); avc_dumpHex(stderr, OUTFMT_DATA_IDX(&pProg->frameData, outidx), MIN(16, OUTFMT_LEN_IDX(&pProg->frameData, outidx)), 1);

      do_sdp_update = 0;

      if((pProg->pXcodeData->piXcode->vid.common.cfgFileTypeOut == XC_CODEC_TYPE_H264) ||
         (!pProg->pXcodeData->piXcode->vid.common.cfgDo_xcode &&
           (pProg->pXcodeData->inStreamType == MP2PES_STREAM_VIDEO_H264 ||
           pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn == XC_CODEC_TYPE_H264))) {

        if(xcode_h264_find_spspps( &XCODE_VID_UDATA_PTR(pProg->pXcodeData->piXcode)->out[outidx].uout.xh264,
                 OUTFMT_DATA_IDX(&pProg->frameData, outidx), OUTFMT_LEN_IDX(&pProg->frameData, outidx), 
                 &OUTFMT_VSEQHDR_IDX(&pProg->frameData, outidx).h264) > 0) {

          do_sdp_update = 1;
          LOG(X_DEBUG("Updating H.264 SDP (outidx:%d) from video frame len:%d (sps:%d pps:%d)"), 
            outidx, pProg->pXcodeData->curFrame.lenData,
            OUTFMT_VSEQHDR_IDX(&pProg->frameData, outidx).h264.sps_len,
            OUTFMT_VSEQHDR_IDX(&pProg->frameData, outidx).h264.pps_len);
        }

      } else if(pProg->pXcodeData->piXcode->vid.common.cfgFileTypeOut == XC_CODEC_TYPE_MPEG4V ||
                (!pProg->pXcodeData->piXcode->vid.common.cfgDo_xcode &&
                 (pProg->pXcodeData->inStreamType == MP2PES_STREAM_VIDEO_MP4V ||
                  pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn == XC_CODEC_TYPE_MPEG4V))) {

        if(xcode_mpg4v_find_seqhdrs(OUTFMT_DATA_IDX(&pProg->frameData, outidx), 
                                    OUTFMT_LEN_IDX(&pProg->frameData, outidx),
                                    &OUTFMT_VSEQHDR_IDX(&pProg->frameData, outidx).mpg4v) > 0) {
                                
          do_sdp_update = 1;
          LOG(X_DEBUG("Updating MPEG-4 SDP (outidx:%d) from video frame len:%d (headers length:%d)"), 
            outidx, pProg->pXcodeData->curFrame.lenData,
            OUTFMT_VSEQHDR_IDX(&pProg->frameData, outidx).mpg4v.hdrsLen);
        }

      } else if(pProg->pXcodeData->piXcode->vid.common.cfgFileTypeOut == XC_CODEC_TYPE_H263 ||
                pProg->pXcodeData->piXcode->vid.common.cfgFileTypeOut == XC_CODEC_TYPE_H263_PLUS) {

          do_sdp_update = 1;
          LOG(X_DEBUG("Updating H263 SDP (outidx:%d) from video"), outidx);

      } else if((pProg->pXcodeData->piXcode->vid.common.cfgFileTypeOut == XC_CODEC_TYPE_VP8) ||
                 0) {
                //(!pProg->pXcodeData->piXcode->vid.common.cfgDo_xcode &&
                // (pProg->pXcodeData->inStreamType == MP2PES_STREAM_VIDEO_MP4V ||
                //  pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn == XC_CODEC_TYPE_MPEG4V))) {
          do_sdp_update = 1;

      } else {
        LOG(X_ERROR("SDP update for codec %s not supported."), 
          codecType_getCodecDescrStr(pProg->pXcodeData->piXcode->vid.common.cfgFileTypeOut));
        return STREAM_NET_ADVFR_RC_ERROR;
      }

      if(do_sdp_update) {

        //fprintf(stderr, "H264 stream_av SDP %s:%d\n", ((STREAMER_CFG_T *) pSdpActions[outidx]->pCfg)->sdpsx[1][outidx].c.iphost, ((STREAMER_CFG_T *) pSdpActions[outidx]->pCfg)->sdpsx[1][outidx].vid.common.port);

        if(update_sdp_vid(pSdpActions[outidx], pProg, 
                          &((STREAMER_CFG_T *) pSdpActions[outidx]->pCfg)->sdpsx[1][outidx],
                          &OUTFMT_VSEQHDR_IDX(&pProg->frameData, outidx)) > 0) {
          pSdpActions[outidx]->vidUpdated = 1;
        }
      }
 
    } else if(!isvid && pSdpActions[outidx]->updateAud && !pSdpActions[outidx]->audUpdated &&
              rc == STREAM_NET_ADVFR_RC_OK && OUTFMT_LEN_IDX(&pProg->frameData, outidx) > 0) {

      LOG(X_DEBUG("Updating SDP from audio frame len:%d"), pProg->pXcodeData->curFrame.lenData);

      if((i = update_sdp_aud(pSdpActions[outidx], pProg, 
                             &((STREAMER_CFG_T *) pSdpActions[outidx]->pCfg)->sdpsx[1][outidx],
                             OUTFMT_DATA_IDX(&pProg->frameData, outidx), 
                             OUTFMT_LEN_IDX(&pProg->frameData, outidx))) > 0) {
        pSdpActions[outidx]->audUpdated = 1;
      } else if(i < 0) {
        rc = STREAM_NET_ADVFR_RC_ERROR;
        break;
      }
    }

    if((!pSdpActions[outidx]->updateVid || 
        (pSdpActions[outidx]->updateVid && pSdpActions[outidx]->vidUpdated)) &&
       (!pSdpActions[outidx]->updateAud || 
        (pSdpActions[outidx]->updateAud && pSdpActions[outidx]->audUpdated))) {
      //fprintf(stderr, "CALLING SDP UPDATE CB outidx[%d] updV:%d,%d updA:%d,%d\n", outidx, pSdpActions[outidx]->updateVid, pSdpActions[outidx]->vidUpdated, pSdpActions[outidx]->updateAud, pSdpActions[outidx]->audUpdated);
      pSdpActions[outidx]->cbSdpUpdated(pSdpActions[outidx]);
    }

  } // end of for(outidx...

  return rc;
}

//static uint64_t lastpts0,lastpts1;

#if 0
static uint64_t s_pts_prev;
static unsigned int s_framecnt;
static time_t s_tm0;
static float s_fps;

uint64_t testfakefps(uint64_t ptsarg) {
  const float fpsmin = 5;
  const float fpsmax = 25;
  const float fpsstepmax = 5;
  uint64_t pts;
  time_t tm;

  if(s_framecnt == 0) {
    s_tm0 = time(NULL);
    s_fps = 25;
    s_pts_prev = ptsarg;
  }

  if((tm = time(NULL)) - s_tm0 >= 2) {
    if((s_fps -= fpsstepmax) < fpsmin) {
      s_fps = fpsmin;
    }
    
    s_tm0 = tm + ceil(2.0f);
    s_pts_prev += 90000.0f * 2.0f;
    //fprintf(stderr, "FPS:%.3f, pts:%.3f, pts_prev:%.3f frame:%d\n", s_fps, PTSF(pts), PTSF(s_pts_prev), s_framecnt);
  }

  pts = s_pts_prev + ((double)90000.0f / s_fps );
  s_framecnt++;


  fprintf(stderr, "FPS:%.3f, pts:%.3f, pts_prev:%.3f frame:%d\n", s_fps, PTSF(pts), PTSF(s_pts_prev), s_framecnt);
  s_pts_prev = pts;

  return pts;
}
#endif // 0

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
void dump_cap_queues(AV_PROG_T *pProg) {
  STREAM_AV_DATA_T *pData = (STREAM_AV_DATA_T *) pProg->pCbData;
  STREAM_H264_T *pH264 = (STREAM_H264_T *) pProg->pCbData;
  //STREAM_AAC_T *pAac = (STREAM_AAC_T *) pProg->pCbData;
  STREAM_VCONFERENCE_T *pVC = (STREAM_VCONFERENCE_T *) pProg->pCbData;
  STREAM_ACONFERENCE_T *pAC = (STREAM_ACONFERENCE_T *) pProg->pCbData;

  if(pProg->stream.type != STREAM_NET_TYPE_LIVE) {
    return;
  }

  if(pH264->includeAnnexBHdr == 1 && pH264->includeSeqHdrs == 1) 
    return;
  else if(!pData->pDataSrc) {
    return;
  } else if(pVC->id == STREAM_VCONFERENCE_ID || pAC->id == STREAM_ACONFERENCE_ID) {
    return;
  }

  PKTQUEUE_T *pQ = (PKTQUEUE_T *) pData->pDataSrc->pCbData;
  if(pQ->cfg.id != 10 && pQ->cfg.id != 11) {
    return;
  }

  STREAM_PES_T *pPes = (STREAM_PES_T *) pData->pPes;
  PKTQUEUE_PKT_T *pPkt, *pPkt2;
  PKTQUEUE_T *pQC;

  if(pData == &pPes->vid)
    pQC = pPes->aud.pDataSrc ? pPes->aud.pDataSrc->pCbData : NULL;
  else
    pQC = pPes->vid.pDataSrc ? pPes->vid.pDataSrc->pCbData : NULL;

  pPkt=&pQ->pkts[pQ->idxRd];
  pPkt2=&pQ->pkts[pQ->idxWr == 0 ? pQ->cfg.growMaxPkts -1 : pQ->idxWr -1];
  LOG(X_DEBUG("qid:%d, rd:%d, wr:%d/%d(%d), haveRdr:%d, haveData:%d, %.3f - %.3f"), pQ->cfg.id, pQ->idxRd, pQ->idxWr, pQ->cfg.maxPkts, pQ->cfg.growMaxPkts, pQ->haveRdr, pQ->haveData, PTSF(pPkt->xtra.tm.pts), PTSF(pPkt2->xtra.tm.pts));

  if(pQC) {
    pPkt=&pQC->pkts[pQC->idxRd];
    pPkt2=&pQC->pkts[pQC->idxWr == 0 ? pQC->cfg.growMaxPkts -1 : pQC->idxWr -1];
    LOG(X_DEBUG("qid:%d, rd:%d, wr:%d/%d(%d), haveRdr:%d, haveData:%d, %.3f - %.3f"), pQC->cfg.id, pQC->idxRd, pQC->idxWr, pQC->cfg.maxPkts, pQC->cfg.growMaxPkts, pQC->haveRdr, pQC->haveData,  PTSF(pPkt->xtra.tm.pts), PTSF(pPkt2->xtra.tm.pts));
  }

}
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

static int stream_av_havepipframe(AV_PROG_T *pProg) {
  STREAM_XCODE_VID_UDATA_T *pUDataV;

  if(pProg->pXcodeData && 
     (pUDataV = ((STREAM_XCODE_VID_UDATA_T *) pProg->pXcodeData->piXcode->vid.pUserData)) &&
     pUDataV->type == XC_CODEC_TYPE_VID_GENERIC && 
     pUDataV->pPipFrQ && 
     pProg->lastHzIn > 0 &&
     pktqueue_havepkt(pUDataV->pPipFrQ)) {
    return 1;
  } 
  return 0;
}


static enum STREAM_NET_ADVFR_RC advanceFrame(AV_PROG_T *pProg,
                                             STREAM_NET_ADVFR_DATA_T *pAdvfrData) {

  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_NOTAVAIL;
  PKTQUEUE_PKT_T *pPkt = NULL;
  unsigned int outidx;
  int upsamplingFrame = 0;
  int isvid = 1;
  int do_xcode = 0;
  int64_t tm;
  int64_t upsampleFrDelta;
  int64_t dts = 0;

  //fprintf(stderr, "advfr start\n");

  // pProg->streamType may not be set for live mpeg2-ts on first iteration
  //isvid = codectype_isVid(mp2_getCodecType(pProg->streamType));

  if(pProg->pXcodeData->piXcode->vid.common.cfgDo_xcode || 
     pProg->pXcodeData->piXcode->aud.common.cfgDo_xcode) {
    do_xcode = 1;
  }

//static int s_invfrpipupsample, s_invfrpip,s_invfr; if(do_xcode && codectype_isVid(pProg->frameData.mediaType)) { if(pProg->pipUpsampleFr && pProg->pXcodeData->piXcode->vid.pip.active==1)s_invfrpipupsample++;  if(!pProg->pipUpsampleFr && pProg->pXcodeData->piXcode->vid.pip.active==1)s_invfrpip++; if(pProg->pXcodeData->piXcode->vid.pip.active==0)s_invfr++; fprintf(stderr, "av frames in pip_upsample:%d pip_w_input:%d, non-pip-input:%d\n", s_invfrpipupsample, s_invfrpip, s_invfr); }

  //
  // Upsample by duplicating previous decoded frame
  //
  if(do_xcode && codectype_isVid(pProg->frameData.mediaType)) {

    //TODO: should check if we have a stream_av_havepipframe if there is no input frame available (cap q size == 0)

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
    if(pProg->pXcodeData && pProg->pXcodeData->piXcode->vid.pip.active) {
      int pkt_rc = 0;
      STREAM_XCODE_VID_UDATA_T *pUDataV = (STREAM_XCODE_VID_UDATA_T *) pProg->pXcodeData->piXcode->vid.pUserData;
      if(pUDataV->pPipFrQ) pkt_rc = pktqueue_havepkt(pUDataV->pPipFrQ);
      LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x check if vid share frame avail.  type:%d, lastHzIn:%llu, havepkt_rc:%d, cbHaveDataNow:%d"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), (pUDataV->type == XC_CODEC_TYPE_VID_GENERIC), pProg->lastHzIn, pkt_rc, pProg->stream.cbHaveDataNow ? pProg->stream.cbHaveDataNow(pAdvfrData->pArgIn) : -2);
    }
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)

    //
    // If this is a PIP thread w/ live (queued) capture, and there is no input frame available,
    // but there is a PIP output frame queued for us by the main overlay encoder, then proceed to
    // call xcode_vid with the pipUpsampleFr flag set to get the output frame
    //
    if(pProg->stream.cbHaveDataNow && 
       pProg->stream.cbHaveDataNow(pAdvfrData->pArgIn) == 0 &&
       stream_av_havepipframe(pProg)) {
      pProg->pipUpsampleFr = 1;
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
        LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x cbHaveDataNow == 0 upsamplingFrame=1"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self());
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)

    }

    if(pProg->pipUpsampleFr) {

      upsampleFrame(pProg->pXcodeData);
      upsamplingFrame = 1;
      pProg->pipUpsampleFr = 0;

    } else if(pProg->pXcodeData->piXcode->vid.common.cfgAllowUpsample &&
       pProg->pXcodeData->piXcode->vid.common.cfgCFROutput && 
       pProg->pXcodeData->piXcode->vid.common.encodeOutIdx > 0) {

      if(pProg->pXcodeData->piXcode->vid.common.cfgCFROutput) {
        upsampleFrDelta = (int64_t) 
        ((90000 + 30000) * pProg->pXcodeData->piXcode->vid.resOutFrameDeltaHz / 
                           pProg->pXcodeData->piXcode->vid.resOutClockHz);
      } else {
        upsampleFrDelta = 3000;
      }

      if(pProg->pXcodeData->piXcode->vid.common.inpts90Khz >
         pProg->pXcodeData->piXcode->vid.common.outpts90Khz + upsampleFrDelta &&
         upsampleFrame(pProg->pXcodeData) == 0) {
        upsamplingFrame = 1;
      }

     //fprintf(stderr, "stream_av upsample compared in: %.3f, out:%.3f, delta:%.3f, upsample:%d\n", PTSF(pProg->pXcodeData->piXcode->vid.common.inpts90Khz), PTSF(pProg->pXcodeData->piXcode->vid.common.outpts90Khz), PTSF(upsampleFrDelta), upsamplingFrame); 

    }

  //} else if(do_xcode && codectype_isAud(mp2_getCodecType(pProg->streamType))) {
  } else if(do_xcode && codectype_isAud(pProg->frameData.mediaType)) {

    if(pProg->pXcodeData->curFrame.idxCompoundFrame < 
       pProg->pXcodeData->curFrame.numCompoundFrames) {
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
      LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x setting upsamplingFrame for audio compoundFrames %d/%d"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), pProg->pXcodeData->curFrame.idxCompoundFrame, pProg->pXcodeData->curFrame.numCompoundFrames);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
      upsamplingFrame = 1;
      //isvid = 0;
    }
    isvid = 0;

  }

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(outidx > 0 && !pProg->pXcodeData->piXcode->vid.out[outidx].active) {
      continue;
    }

    OUTFMT_VSEQHDR_IDX(&pProg->frameData, outidx).h264.sps_len = 0;
    OUTFMT_VSEQHDR_IDX(&pProg->frameData, outidx).h264.pps_len = 0;
  }

  //TODO: this should work for mpg4v and h264
  //pProg->frameData.vseqhdr.h264.sps_len = pProg->frameData.vseqhdr.h264.pps_len = 0;
  //pAdvfrData->hdr.pspspps = &pProg->frameData.vseqhdr.h264;

  if(!upsamplingFrame) {

    //pProg->frameData.dts=0;
    *pAdvfrData->pDts = 0;
    //fprintf(stderr, "CALLING CBADVANCEFRAME tid:0x%x, isvid:%d, do_xcode:%d, progId:%d, streamType:%d, codec:%d\n", pthread_self(), isvid, do_xcode, pProg->progId, pProg->streamType, pProg->frameData.mediaType);

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
      LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x calling cbAdvanceFrame for prog %d"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), pProg == &pProg->pAv->progs[0] ? 0 : 1);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)

    rc = pProg->lastRc = pProg->stream.cbAdvanceFrame(pAdvfrData);

    pProg->tmLastFrCheck = timer_GetTime();
    if(pProg->dtx && rc == STREAM_NET_ADVFR_RC_OK) {
      pProg->dtx = 0;
    }

    if(!pProg->pAv->haveTmStartTx) {
      TIME_VAL tm = timer_GetTime();
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
        LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x changing tmStartTx from %llu us -> %llu us"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), ((STREAM_AV_T *) pProg->pAv)->tmStartTx, tm);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
      pProg->pAv->tmStartTx = tm;
      pProg->pAv->haveTmStartTx = 1;
    }

    if(rc != STREAM_NET_ADVFR_RC_OK) { 
      VSX_DEBUG_STREAMAV( LOG(X_DEBUGV("cbAdvanceFrame rc:%d id:0x%x stype:0x%x"), rc, pProg->streamId, pProg->streamType));

      if(do_xcode && rc == STREAM_NET_ADVFR_RC_NOTAVAIL && isvid && stream_av_havepipframe(pProg)) {
        upsampleFrame(pProg->pXcodeData);
        upsamplingFrame = 1;

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
        LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x advanceFrame got shared pip frame, frameId:%llu, upsamplingFrame=1"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), pProg->frameId);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)

      } else {

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
        LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x advanceFrame returning rc:%d"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), rc);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)

        return rc;
      }

    } else if(*pAdvfrData->plen <= 0) {
      VSX_DEBUG_STREAMAV( LOG(X_DEBUG("cbAdvanceFrame rc:%d sid:0x%x stype:0x%x"), 
                     rc, pProg->streamId, pProg->streamType) );
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
    LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x advanceFrame returning *plen=0"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self());
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
      return rc;
    }

    if(!pProg->haveStartHzIn) {
      pProg->startHzIn = *pAdvfrData->pPts;
      pProg->haveStartHzIn = 1;
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
      LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x set prog->startHzIn=%.3f"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), PTSF(pProg->startHzIn));
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
    }


  } // end of if(!upsamplingFrame ...

  if(!upsamplingFrame) {

    isvid = pAdvfrData->isvid;

    //
    // This is the PTS of the new frame w/o any xcode layer alterations
    //
    pProg->lastHzIn = *pAdvfrData->pPts;    

    // Since this is a video timestamp, delay it only if a/v < 0 
    if(pProg->pavs[0] && pProg->pavs[0]->offsetCur < 0) {

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
      // not sure why here??
      LOG(X_DEBUG("%llu.%llu stream_av tid:0%x, applying lastHzIn[0] video offset %.3f"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), PTSF(abs(pProg->pavs[0]->offsetCur)));
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

      pProg->lastHzIn += abs(pProg->pavs[0]->offsetCur); 
    }
/*
      if(pProg->pavs[0]->offsetCur > 0) {
        pProg->lastHzIn += pProg->pavs[0]->offsetCur;
      } else if(pProg->pavs[0]->offsetCur < 0) {
        if(pProg->pavs[0]->offsetCur * -1 > (int64_t) pProg->lastHzIn) {
          pProg->lastHzIn = 0;
        } else {
          pProg->lastHzIn += pProg->pavs[0]->offsetCur;
        }
      }
    }
*/

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
    LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x advanceFrame after cbAdvanceFrame pPts:%.3f, isvid:%d, len:%d, pip.active:%d"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), PTSF(*pAdvfrData->pPts), isvid, *pAdvfrData->plen, pProg->pXcodeData->piXcode->vid.pip.active);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)

//if(isvid) *pAdvfrData->pPts = testfakefps(*pAdvfrData->pPts);
    //fprintf(stderr, "GOT cbAdvanceFrame vid:%d, pts:%f\n", isvid, PTSF(*pAdvfrData->pPts));

    if(pProg->haveDts && pProg->pXcodeData->curFrame.pkt.xtra.tm.dts != 0) {
      dts = pProg->stream.cbFrameDecodeHzOffset(pProg->pCbData);
    }

    //if(1||isvid) fprintf(stderr, "%s PTS{ fr:%.3f, curF:%.3f },  DTS{ fr:%.3f, curF:%.3f, dlta:%.3f} haveDts:%d, len:%d\n", isvid?"vid":"aud",PTSF(*pAdvfrData->pPts), PTSF(pProg->pXcodeData->curFrame.frameData.xtra.tm.pts),PTSF(*pAdvfrData->pDts), PTSF(pProg->pXcodeData->curFrame.frameData.xtra.tm.dts), PTSF(dts), pProg->haveDts, *pAdvfrData->plen);

    //
    // For capture such as RTMP, the input codec type is not known until capture 
    // is configured and the first audio packet actually receivedi
    //
    if(isvid && pAdvfrData->codecType != XC_CODEC_TYPE_UNKNOWN &&
       pAdvfrData->codecType != pProg->pXcodeData->piXcode->aud.common.cfgFileTypeIn) {

      pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn = pAdvfrData->codecType;
      if(!pProg->pXcodeData->piXcode->aud.common.cfgDo_xcode) {
        pProg->streamType = mp2_getStreamType(pAdvfrData->codecType);
        LOG(X_DEBUG("Changing input vid "MP2PES_STREAMTYPE_FMT_STR" media:0x%x"), 
             MP2PES_STREAMTYPE_FMT_ARGS(pProg->streamType), 
             pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn);
      }

    } else if(!isvid && pAdvfrData->codecType != XC_CODEC_TYPE_UNKNOWN &&
       pAdvfrData->codecType != pProg->pXcodeData->piXcode->aud.common.cfgFileTypeIn) {

      pProg->pXcodeData->piXcode->aud.common.cfgFileTypeIn = pAdvfrData->codecType;
      if(!pProg->pXcodeData->piXcode->aud.common.cfgDo_xcode) {
        pProg->streamType = mp2_getStreamType(pAdvfrData->codecType);
        LOG(X_DEBUG("Changing input aud "MP2PES_STREAMTYPE_FMT_STR" media:0x%x"), 
             MP2PES_STREAMTYPE_FMT_ARGS(pProg->streamType), 
             pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn);
      }
    }

  //if(isvid){ fprintf(stderr, "VID codec:%d instype:0x%x stype:0x%x cfgFileTypeIn:0x%x, frcodectype:0x%x\n", pProg->frameData.mediaType, pProg->pXcodeData->inStreamType, pProg->streamType, pProg->pXcodeData->piXcode->aud.common.cfgFileTypeIn, pAdvfrData->codecType); }

    VSX_DEBUG_STREAMAV( LOG(X_DEBUGV("cbAdvanceFrame got rc:%d %s len:%d streamtype:0x%x pts:%.3f dts:%.3f lastHz v:%.3f a:%.3f)"), 
       rc, isvid?"vid":"aud", OUTFMT_LEN(&pProg->frameData), pProg->streamType, 
       PTSF(*pAdvfrData->pPts),PTSF(*pAdvfrData->pDts), 
       PTSF(pProg->pAv->progs[0].lastHzOut), 
       PTSF(pProg->pAv->progs[1].lastHzOut)));

    if(isvid) {

      if(pProg->pXcodeData->curFrame.pSwappedSlot) {
        //fprintf(stderr, "vid swapped v\n");
        pPkt = pProg->pXcodeData->curFrame.pSwappedSlot;
      } else {
        //fprintf(stderr, "vid tmpFrame v\n");
        //TODO: this should be deprecated since we're now using pktqueue_swapreadslots
        pPkt = &XCODE_VID_UDATA_PTR(pProg->pXcodeData->piXcode)->tmpFrame;
      }

      //fprintf(stderr, "inStreamType:0x%x inmediatype:0x%x\n", pProg->pXcodeData->inStreamType, pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn); 

      if(pProg->pXcodeData->inStreamType == MP2PES_STREAMTYPE_NOTSET) {
        pProg->pXcodeData->inStreamType = pProg->streamType;
        pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn = 
                 mp2_getCodecType(pProg->streamType);
        LOG(X_DEBUG("auto set input vid "MP2PES_STREAMTYPE_FMT_STR" media:0x%x"), 
             MP2PES_STREAMTYPE_FMT_ARGS(pProg->streamType), 
             pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn);
      }

    } else {

      if(pProg->pXcodeData->curFrame.pSwappedSlot) {
        pPkt = pProg->pXcodeData->curFrame.pSwappedSlot;
        //fprintf(stderr, "aud swapped allocSz:%d\n", pPkt->allocSz);
      } else {
        //TODO: this should be deprecated since we're now using pktqueue_swapreadslots
        pPkt = &XCODE_AUD_UDATA_PTR(pProg->pXcodeData->piXcode)->tmpFrame;
        //fprintf(stderr, "aud tmpFrame allocSz:%d\n", pPkt->allocSz);
      }

      if(pProg->pXcodeData &&
         pProg->pXcodeData->inStreamType == MP2PES_STREAMTYPE_NOTSET) {
        pProg->pXcodeData->inStreamType = pProg->streamType;
        pProg->pXcodeData->piXcode->aud.common.cfgFileTypeIn = 
               mp2_getCodecType(pProg->streamType);
        LOG(X_DEBUG("auto set input aud "MP2PES_STREAMTYPE_FMT_STR" media:0x%x"), 
             MP2PES_STREAMTYPE_FMT_ARGS(pProg->streamType),
             pProg->pXcodeData->piXcode->aud.common.cfgFileTypeIn);
      }

    } // end of else ... if(vid

    if(pProg->lastStreamType != pProg->streamType) {
      LOG(X_DEBUG("Set prog: 0x%x output "MP2PES_STREAMTYPE_FMT_STR), pProg->progId, 
            MP2PES_STREAMTYPE_FMT_ARGS(pProg->streamType));
      pProg->lastStreamType = pProg->streamType;
    }

    if((pPkt->len = OUTFMT_LEN_IDX(&pProg->frameData, 0)) > pPkt->allocSz) {
      LOG(X_WARNING("Truncating streamtype: 0x%x xcode input frame length %u -> %u"),
          pProg->streamType, OUTFMT_LEN(&pProg->frameData), pPkt->allocSz);
      pPkt->len = pPkt->allocSz;
    }  
    
    //fprintf(stderr, "LOADFRAMEDATA len:%d/%d outidx:%d\n", pPkt->len, pPkt->allocSz, pProg->frameData.xout.outidx);

    //
    // Copy the entire frame data contents into tmpFrame
    // Note, this is only still required for legacy stream_net_pes
    // and stream_net[proto] non stream_net_av streams 
    if(!pProg->pXcodeData->curFrame.pSwappedSlot &&
       loadFrameData(pProg, pPkt->pData, pPkt->len) != pPkt->len) {
      return STREAM_NET_ADVFR_RC_ERROR;
    }

    pProg->pXcodeData->curFrame.idxReadInFrame = 0;
    pProg->frameWrIdx = 0;

    if(do_xcode) {

      if(isvid && pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn == XC_CODEC_TYPE_VID_CONFERENCE) {
        pPkt->len = pPkt->allocSz;
      }

      memcpy(&pProg->pXcodeData->curFrame.pkt, pPkt, sizeof(PKTQUEUE_PKT_T));
      OUTFMT_DATA(&pProg->frameData) = pProg->pXcodeData->curFrame.pData = 
                               pProg->pXcodeData->curFrame.pkt.pData;
      pProg->pXcodeData->curFrame.lenData = pProg->pXcodeData->curFrame.pkt.len;

      //if(isvid) fprintf(stderr, "CURFRAME PTS CHANGED from %.3f to %.3f\n", pProg->pXcodeData->curFrame.pkt.xtra.tm.pts, *pAdvfrData->pPts);

      if((isvid && pProg->pXcodeData->piXcode->vid.common.cfgDo_xcode) ||
         (!isvid && pProg->pXcodeData->piXcode->aud.common.cfgDo_xcode)) {
        pProg->pXcodeData->curFrame.pkt.xtra.tm.pts = *pAdvfrData->pPts; 
//fprintf(stderr, "stream_av SET PTS xtra.tm.pts:%.3f, prog:%d isvid:%d\n", PTSF(pProg->pXcodeData->curFrame.pkt.xtra.tm.pts), ((STREAM_AV_T *)pProg->pAv)->numProg, isvid);
        pProg->pXcodeData->curFrame.pkt.xtra.tm.dts = 0;
      }

      pPkt->idx = 0;
//fprintf(stderr, "INPUT LEN:%d\n", pProg->pXcodeData->curFrame.lenData);avc_dumpHex(stderr, pProg->frameData.pData, 16, 1);

    } else {

      // TODO: for certain input feeds such as httplive (iPhone)
      // need to get rid of AUD NALs
      // TODO: need to tage keyframes - even after skipping SEI
      OUTFMT_DATA(&pProg->frameData) = pProg->pXcodeData->curFrame.pData = 
        pProg->pXcodeData->curFrame.xout.outbuf.poffsets[0] = pPkt->pData;
      pProg->pXcodeData->curFrame.lenData = pProg->pXcodeData->curFrame.xout.outbuf.lens[0] = pPkt->len;


    } // end of do_xcode

  } // end of if(!upsamplingFrame)

  if(isvid && pProg->pXcodeData->piXcode->vid.common.cfgDo_xcode) {

    if(pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn == MEDIA_FILE_TYPE_UNKNOWN) {
      pProg->pXcodeData->piXcode->vid.common.cfgFileTypeIn = 
           mp2_getCodecType(pProg->streamType);
    }

    //TODO: this should be codec specific
    if(pProg->pXcodeData->piXcode->vid.common.cfgFileTypeOut == XC_CODEC_TYPE_H264) {

      //pProg->frameData.vseqhdr.h264.sps_len = pProg->frameData.vseqhdr.h264.pps_len = 0;

      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        if(outidx > 0 && !pProg->pXcodeData->piXcode->vid.out[outidx].active) {
          continue;
        }

        OUTFMT_VSEQHDR_IDX(&pProg->frameData, outidx).h264.sps_len = 0;
        OUTFMT_VSEQHDR_IDX(&pProg->frameData, outidx).h264.pps_len = 0;
        XCODE_VID_UDATA_PTR(pProg->pXcodeData->piXcode)->out[outidx].uout.xh264.pSpspps =
          &OUTFMT_VSEQHDR_IDX(&pProg->frameData, outidx).h264;

      }

      //((PESXCODE_VID_UDATA_T *)pProg->pXcodeData->piXcode->vid.pUserData)->out[0].uout.xh264.pSpspps = &pProg->frameData.vseqhdr.h264;

    } else if(pProg->pXcodeData->piXcode->vid.common.cfgFileTypeOut == XC_CODEC_TYPE_MPEG4V) {
    //  ((PESXCODE_VID_UDATA_T *)pProg->pXcodeData->piXcode->vid.pUserData)->uout.xmpg4v.pSpspps = &pProg->frameData.vseqhdr.mpg4v;
//fprintf(stderr, "MPEG-4.... set len:%d\n", ((PESXCODE_VID_UDATA_T *)pProg->pXcodeData->piXcode->vid.pUserData)->uout.xmpg4v.mpg4v.seqHdrs.hdrsLen);

    }

//tvx0=timer_GetTime();
   //if(pProg->pXcodeData->piXcode->vid.pip.active) LOG(X_DEBUG("calling xcode_vid w/ input len:%d, inpts:%.3f, outputs:%.3f %s"), pProg->pXcodeData->curFrame.lenData, ((double)pProg->pXcodeData->piXcode->vid.common.inpts90Khz/90000), ((double)pProg->pXcodeData->piXcode->vid.common.outpts90Khz/90000), (0|| (pProg->pXcodeData->piXcode->vid.common.inpts90Khz + 150 >= pProg->pXcodeData->piXcode->vid.common.outpts90Khz)) ? "YES" : "NO");

    if((rc = xcode_vid(pProg->pXcodeData)) <= STREAM_NET_ADVFR_RC_ERROR) {
      rc = STREAM_NET_ADVFR_RC_ERROR_CRIT;
    }

    if(pProg->pXcodeData->piXcode->vid.common.encodeOutIdx <= 1) {
      update_packetizers(pProg);
    }

//fprintf(stderr, "xcoded vid in %.3f ms\n",  (double)(timer_GetTime()- tvx0)/TIME_VAL_MS);


    *pAdvfrData->pkeyframeIn = pProg->pXcodeData->curFrame.xout.keyframes[0];
    *pAdvfrData->plen = pProg->pXcodeData->curFrame.xout.outbuf.lens[0]; 

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
    LOG(X_DEBUG("%llu.%llu stream_av after xcode_vid tid:0x%x pip.active:%d, havePip:%d rc:%d len:%d, in-seqhdr:%d"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), pProg->pXcodeData->piXcode->vid.pip.active, pProg->pXcodeData->piXcode->vid.overlay.havePip, rc, pProg->pXcodeData->curFrame.xout.outbuf.lens[0], ((STREAM_XCODE_VID_UDATA_T *) pProg->pXcodeData->piXcode->vid.pUserData)->haveSeqStart);
    //avc_dumpHex(stderr, OUTFMT_DATA(&pProg->pXcodeData->curFrame), MIN(16, OUTFMT_LEN(&pProg->pXcodeData->curFrame)), 0);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if((outidx > 0 && !pProg->pXcodeData->piXcode->vid.out[outidx].active) ||
         pProg->pXcodeData->piXcode->vid.out[outidx].passthru) {
        continue;
      }

      if(pProg->pXcodeData->curFrame.xout.tms[outidx].dts != 0) {
        tm = pProg->pXcodeData->curFrame.xout.tms[outidx].dts;
      } else {
        tm = pProg->pXcodeData->curFrame.xout.tms[outidx].pts;
        pProg->frameData.xout.tms[outidx].dts = 0;
      }

      //fprintf(stderr, "THE PTS:%.3f, pip.active:%d\n", PTSF(pProg->pXcodeData->curFrame.xout.tms[outidx].pts), pProg->pXcodeData->piXcode->vid.pip.active);

      pProg->frameData.xout.tms[outidx].pts = xcode_getFrameTm(pProg->pXcodeData, 1, tm);
      if(pProg->pXcodeData->curFrame.xout.tms[outidx].dts != 0) {
        pProg->frameData.xout.tms[outidx].dts = 
                               SET_DTS(pProg->pXcodeData->curFrame.xout.tms[outidx].pts,
                                       pProg->pXcodeData->curFrame.xout.tms[outidx].dts);
      }

      //fprintf(stderr, "XCODE_VID rc:%d outidx[%d] len:%d\n", rc, outidx, OUTFMT_LEN_IDX(&pProg->frameData, outidx));
    }
    *pAdvfrData->pPts = pProg->frameData.xout.tms[0].pts; 
    *pAdvfrData->pDts = pProg->frameData.xout.tms[0].dts; 

    memcpy(pProg->frameData.xout.outbuf.poffsets, pProg->pXcodeData->curFrame.xout.outbuf.poffsets, 
           sizeof(pProg->frameData.xout.outbuf.poffsets));

    memcpy(pProg->frameData.xout.outbuf.lens, pProg->pXcodeData->curFrame.xout.outbuf.lens, 
           sizeof(pProg->frameData.xout.outbuf.lens));

    memcpy(pProg->frameData.xout.keyframes, pProg->pXcodeData->curFrame.xout.keyframes, 
           sizeof(pProg->frameData.xout.keyframes));


    //pProg->frameData.xout.outidx = 1;
    //fprintf(stderr, "VID OUT len:%d, pts:%.3f, dts:%.3f [0]:%.3f,%.3f  [1]:%.3f,%.3f\n", pProg->frameData.xout.outbuf.lens[0], PTSF(pProg->pXcodeData->curFrame.pkt.xtra.tm.pts), PTSF(pProg->pXcodeData->curFrame.pkt.xtra.tm.dts), PTSF(pProg->pXcodeData->curFrame.xout.tms[0].pts), PTSF(pProg->pXcodeData->curFrame.xout.tms[0].dts), PTSF(pProg->pXcodeData->curFrame.xout.tms[1].pts), PTSF(pProg->pXcodeData->curFrame.xout.tms[1].dts)); 
    //fprintf(stderr, "MULTI-OUT len[0]:%d, len[1]:%d, %s, %s\n", pProg->frameData.xout.outbuf.lens[0], pProg->frameData.xout.outbuf.lens[1], pProg->frameData.xout.keyframes[0] ? "KEYFRAME[0]" : "           ", pProg->frameData.xout.keyframes[1] ? "KEYFRAME[1]" : "           "); avc_dumpHex(stderr,pProg->frameData.xout.outbuf.poffsets[0], MIN(16, pProg->frameData.xout.outbuf.lens[0]), 1); avc_dumpHex(stderr,pProg->frameData.xout.outbuf.poffsets[1], MIN(16, pProg->frameData.xout.outbuf.lens[1]), 1);

    //fprintf(stderr, "0x%x xcoded vid keyframe:%d len:%d pts:%.3f (%.3f), dts:%.3f (%.3f) (upsampled:%d)\n", pthread_self(), *pAdvfrData->pkeyframe, pProg->pXcodeData->curFrame.lenData, PTSF(*pAdvfrData->pPts), PTSF(pProg->pXcodeData->curFrame.pkt.xtra.tm.pts),PTSF(*pAdvfrData->pDts), PTSF(pProg->pXcodeData->curFrame.pkt.xtra.tm.dts), upsamplingFrame);
    //if(pProg->pXcodeData->curFrame.lenData>0)avc_dumpHex(stderr, pProg->pXcodeData->curFrame.pData, pProg->pXcodeData->curFrame.lenData, 1);

  } else if(!isvid && pProg->pXcodeData->piXcode->aud.common.cfgDo_xcode) {

    if(pProg->pXcodeData->piXcode->aud.common.cfgFileTypeIn == MEDIA_FILE_TYPE_UNKNOWN) {
     pProg->pXcodeData->piXcode->aud.common.cfgFileTypeIn = 
           mp2_getCodecType(pProg->streamType);
    }

    pProg->pXcodeData->curFrame.pData = pProg->pXcodeData->curFrame.pkt.pBuf;

//fprintf(stderr, "xcode_aud BEFORE pts:%llu %.3f\n",  pProg->pXcodeData->curFrame.pkt.xtra.tm.pts, PTSF(pProg->pXcodeData->curFrame.pkt.xtra.tm.pts));

//tvx0=timer_GetTime();
    if((rc = xcode_aud(pProg->pXcodeData)) <= STREAM_NET_ADVFR_RC_ERROR) {
      rc = STREAM_NET_ADVFR_RC_ERROR_CRIT;
    }

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
    //if(rc != STREAM_NET_ADVFR_RC_NOTENCODED && pProg->pXcodeData->piXcode->vid.pip.active) {
    //  pProg->pXcodeData->curFrame.lenData = 0;
    //  rc = STREAM_NET_ADVFR_RC_NOTENCODED;
    //}
#endif // XCODE_HAVE_PIP_AUDIO

    //fprintf(stderr, "AUD IDX:%d\n", pProg->pXcodeData->piXcode->aud.common.encodeOutIdx);
    if(pProg->pXcodeData->piXcode->aud.common.encodeOutIdx <= 1) {
      update_packetizers(pProg);
    }
//fprintf(stderr, "xcoded aud in %.3f ms\n",  (double)(timer_GetTime()- tvx0)/TIME_VAL_MS);

    *pAdvfrData->pPts = xcode_getFrameTm(pProg->pXcodeData, 1, 0);
    *pAdvfrData->plen = pProg->pXcodeData->curFrame.lenData; 

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
    //if(pProg->pXcodeData->piXcode->aud.pMixer)  conference_dump (stderr, pProg->pXcodeData->piXcode->aud.pMixer);
    LOG(X_DEBUG("%llu.%llu stream_av after xcode_aud tid:0x%x pip.active:%d, havePip:%d rc:%d len:%d, pts:%.3f, coumpound:%d/%d"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), pProg->pXcodeData->piXcode->vid.pip.active, pProg->pXcodeData->piXcode->vid.overlay.havePip, rc, pProg->pXcodeData->curFrame.lenData, PTSF(*pAdvfrData->pPts), pProg->pXcodeData->curFrame.idxCompoundFrame, pProg->pXcodeData->curFrame.numCompoundFrames);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)


    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if(outidx > 0 && !pProg->pXcodeData->piXcode->vid.out[outidx].active) {
        continue;
      }

      pProg->frameData.xout.outbuf.poffsets[outidx] = pProg->pXcodeData->curFrame.pData;
      pProg->frameData.xout.outbuf.lens[outidx] = *pAdvfrData->plen;
      pProg->frameData.xout.tms[outidx].pts = *pAdvfrData->pPts;
      pProg->frameData.xout.tms[outidx].dts = 0;
    }

    //fprintf(stderr, "xcode_aud AFTER pts:%llu %.3f (%llu %.3f) len:%d (0x%x)\n",  pProg->pXcodeData->curFrame.pkt.xtra.tm.pts, PTSF(pProg->pXcodeData->curFrame.pkt.xtra.tm.pts), *pAdvfrData->pPts, PTSF(*pAdvfrData->pPts), *pAdvfrData->plen, pProg->pXcodeData->curFrame.pData);
    //avc_dumpHex(stderr, pProg->pXcodeData->curFrame.pData, 16, 1);

    //fprintf(stderr, "xcoded aud len:%d pts:%.3f (%.3f)\n", pProg->pXcodeData->curFrame.lenData, (double)*pAdvfrData->pPts/90000.0f, (double)pProg->pXcodeData->curFrame.frameData.xtra.tm.pts/90000.0f);

  } // end of if(isvid && is xcode...

  //fprintf(stderr, "advfr done rc:%d\n", rc);

  //TODO: check if same->same xcode
  if(isvid && (!pProg->pXcodeData->piXcode->vid.common.cfgDo_xcode ||
               pProg->pXcodeData->piXcode->vid.out[0].passthru)) {

    OUTFMT_KEYFRAME_IDX(&pProg->frameData, 0) = *pAdvfrData->pkeyframeIn;

    if(pProg->haveDts) {
      //TODO: this may not be entirely right...
      *pAdvfrData->pDts = pProg->stream.cbFrameDecodeHzOffset(pProg->pCbData);
      //*pAdvfrData->pDts = dts;
    }

    //fprintf(stderr, "direct frame rc:%d keyframe:%d len:%d(%d) pts:%.3f, dts:%.3f\n", rc, *pAdvfrData->pkeyframe, pProg->pXcodeData->curFrame.lenData, *pAdvfrData->plen, (double)*pAdvfrData->pPts/90000.0f, (double)*pAdvfrData->pDts/90000.0f);

  }
  //fprintf(stderr, "PTS... %.3f, durationTot:%llu\n", PTSF(*pAdvfrData->pPts), pProg->pXcodeData->curFrame.tm.durationTot); 

  //
  // Add durationTot pts adjustment from start of session time, across any resets
  //
  *pAdvfrData->pPts += pProg->pXcodeData->curFrame.tm.durationTot;

  //
  // Update any SDP contents for RTP based delivery
  // if transcoding is enabled
  //
  rc = update_sdp(pProg, isvid, rc);
 

  //TODO: check if same -> same xcode
  if((isvid && (!pProg->pXcodeData->piXcode->vid.common.cfgDo_xcode ||
                pProg->pXcodeData->piXcode->vid.out[0].passthru)) ||
    (!isvid && !pProg->pXcodeData->piXcode->aud.common.cfgDo_xcode)) {
    OUTFMT_PTS_IDX(&pProg->frameData, 0) = *pAdvfrData->pPts;
    OUTFMT_DTS_IDX(&pProg->frameData, 0) = *pAdvfrData->pDts;

  }

//for(outidx=0;outidx<IXCODE_VIDEO_OUT_MAX;outidx++){
//if(pProg->pXcodeData->piXcode->vid.out[outidx].active) fprintf(stderr, "outidx[%d] %s { PTS:%.3f DTS:%.3f } ", outidx, isvid ? "vid" : "aud", PTSF(OUTFMT_PTS_IDX(&pProg->frameData, outidx)), PTSF(OUTFMT_DTS_IDX(&pProg->frameData, outidx)));
//} fprintf(stderr, "\n");

    //LOG(X_DEBUG("AV OUT %s pts:%.3f, dts:%.3f [0]:len:%d,%.3f,%.3f  [1]:len:%d,%.3f,%.3f in-pts:%.3f"), pProg->frameData.isvid ? "vid" : "aud", PTSF(pProg->pXcodeData->curFrame.pkt.xtra.tm.pts), PTSF(pProg->pXcodeData->curFrame.pkt.xtra.tm.dts), pProg->frameData.xout.outbuf.lens[0], PTSF(pProg->frameData.xout.tms[0].pts), PTSF(pProg->frameData.xout.tms[0].dts), pProg->frameData.xout.outbuf.lens[1], PTSF(pProg->frameData.xout.tms[1].pts), PTSF(pProg->frameData.xout.tms[1].dts), PTSF(*pAdvfrData->pPts));  

  //pProg->frameData.pts = *pAdvfrData->pPts;
  //pProg->frameData.dts = *pAdvfrData->pDts;
  pProg->frameData.streamType = pProg->streamType;
  if(pProg->frameData.mediaType == XC_CODEC_TYPE_UNKNOWN) {
    pProg->frameData.mediaType = mp2_getCodecType(pProg->streamType);
  }
//fprintf(stderr, "MEDIA TYPE:%d\n", pProg->frameData.mediaType);
  pProg->frameData.isvid = isvid ? 1 : 0;
  pProg->frameData.isaud = isvid ? 0 : 1;

//static int s_outvfr, s_outvfrpip; if(isvid && OUTFMT_LEN(&pProg->frameData) > 0 && pProg->pXcodeData->piXcode->vid.pip.active==0)s_outvfr++;  if(isvid && OUTFMT_LEN(&pProg->frameData) > 0 && pProg->pXcodeData->piXcode->vid.pip.active==1)s_outvfrpip++; fprintf(stderr, "av frames out:%d pip:%d\n", s_outvfr, s_outvfrpip);

  VSX_DEBUG_STREAMAV(
  if(OUTFMT_LEN(&pProg->frameData) > 0) {

    LOG(X_DEBUG("new-frame[%lld] %s done len:%d stype:0x%x pts:%.3f(+%.1f)+%.3f=%.3f(dlta:%.3f) "
           "dts:%.3f lastHz[v:%.3f(%d) a:%.3f(%d)], frameTmSt:%.3f, key:%d, rc:%d"), 
     pProg->frameId, (isvid ? "vid" : "aud"), OUTFMT_LEN(&pProg->frameData), pProg->streamType, 
     PTSF(*pAdvfrData->pPts), 
     PTSF(pProg->pXcodeData->curFrame.tm.durationTot),
     PTSF(pProg->pavs[0] ? pProg->pavs[0]->offsetCur : 0), 
     PTSF(*pAdvfrData->pPts + (pProg->pavs[0] ? pProg->pavs[0]->offsetCur : 0)), 
     PTSF(*pAdvfrData->pPts - (isvid ? pProg->pAv->dbglastptsvid : 
                                       pProg->pAv->dbglastptsaud)), 
     PTSF(*pAdvfrData->pDts), 
     PTSF(pProg->pAv->progs[0].lastHzOut), pProg->pAv->progs[0].haveStartHzIn, 
     PTSF(pProg->pAv->progs[1].lastHzOut), pProg->pAv->progs[1].haveStartHzIn, 
     PTSF(pProg->pXcodeData->frameTmStartOffset), pAdvfrData->pkeyframeIn ? *pAdvfrData->pkeyframeIn : 0, rc);

     if(isvid && OUTFMT_LEN(&pProg->frameData) > 0) {
       pProg->pAv->dbglastptsvid = *pAdvfrData->pPts;
     } else  {
       pProg->pAv->dbglastptsaud = *pAdvfrData->pPts;
     }
  }
  )

  return rc;
}

int stream_av_init(STREAM_AV_T *pAv, 
                   const PKTZ_CBSET_T *pPktz0,
                   const PKTZ_CBSET_T *pPktz1,
                   unsigned int numpacketizers) {
  int rc = 0;
  unsigned int idxPktz, idx, outidx;
  PKTZ_INIT_PARAMS_T *pPktzInit;
  const PKTZ_CBSET_T *pPktz;

  if(!pAv || numpacketizers > (MAX_PACKETIZERS * IXCODE_VIDEO_OUT_MAX)) {
    return -1;
  }

  pAv->prepareCalled = 0;

  for(idxPktz = 0; idxPktz < numpacketizers; idxPktz++) {

    for(idx = 0; idx < pAv->numProg; idx++) {

      if(pAv->progs[idx].inactive) {
        continue; 
      }

      pPktz = &pPktz0[idxPktz];
      if(idx > 0 || !pPktz->pCbInitData) {
        pPktz = &pPktz1[idxPktz];
      }

      memcpy(&pAv->progs[idx].pktz[idxPktz], pPktz, sizeof(pAv->progs[idx].pktz[idxPktz]));
      pPktzInit = (PKTZ_INIT_PARAMS_T *) pAv->progs[idx].pktz[idxPktz].pCbInitData;
      if(!pPktzInit) {
        LOG(X_ERROR("output program[%d/%d] packetizer[%d/%d] not configured (%s %s)"), 
           idx+1, pAv->numProg, idxPktz+1, numpacketizers, 
           pPktz0[idxPktz].pCbInitData ? "vid" : "novid", 
           pPktz1[idxPktz].pCbInitData ? "aud" : "noaud");
        return -1;
      }

      if(idx == 0 && pAv->pvidProp) {
        pPktzInit->pvidProp = pAv->pvidProp;
      } else {
        pPktzInit->pvidProp = NULL;
      }

      pPktzInit->pavoffset_pktz = &pAv->progs[idx].avoffset_pktz;
      pPktzInit->pStreamId = &pAv->progs[idx].streamId;
      pPktzInit->pStreamType = &pAv->progs[idx].streamType;
      pPktzInit->pPesLenZero = &pAv->progs[idx].pesLenZero;
      pPktzInit->pHaveDts = &pAv->progs[idx].haveDts;
      pPktzInit->pFrameData = &pAv->progs[idx].frameData;
      pPktzInit->pinactive = &pAv->progs[idx].inactive;

      if(pAv->progs[idx].pktz[idxPktz].cbInit(pAv->progs[idx].pktz[idxPktz].pCbData, 
                             pAv->progs[idx].pktz[idxPktz].pCbInitData, idx) < 0) {
        LOG(X_ERROR("Failed to init packetizer[%d] prog[%d]"), idxPktz, idx);
        return -1;
      }
      pAv->progs[idx].numpacketizers = numpacketizers;
    }

  }

  for(idx = 0; idx < pAv->numProg; idx++) {
    pAv->progs[idx].pAv =  pAv;
    pAv->progs[idx].lastStreamType = pAv->progs[idx].streamType;
    pAv->progs[idx].progId = idx; 
    pAv->progs[idx].lastHzIn = pAv->progs[idx].lastHzOut = 0;
    //pAv->progs[idx].plastHz = &pAv->progs[idx].lastHzOut;
    pAv->progs[idx].startHzIn = 0;
    pAv->progs[idx].haveStartHzIn = 0;
    pAv->progs[idx].startHzOut = 0;
    pAv->progs[idx].haveStartHzOut = 0;
    pAv->progs[idx].frameId = 0;
    pAv->progs[idx].frameWrIdx = 0;
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      OUTFMT_LEN_IDX(&pAv->progs[idx].frameData, outidx) = 0;
    }
  }

  //pAv->lastHz = 0;
  //pAv->startHzIn = 0;
  //pAv->haveStartHz = 0;

  if(rc < 0) {
     stream_av_close(pAv); 
  }

  return rc;
}

int stream_av_reset(STREAM_AV_T *pAv, int fullReset) {
  unsigned int idx, idxPktz, outidx;
  STREAM_NET_ADVFR_DATA_T advfrData;
  int rc = 0;

  if(!pAv) {
    return -1;
  }

  LOG(X_DEBUG("stream_av_reset full:%d start: [0]:%d, %.3f, [1]:%d, %.3f"), fullReset,
     pAv->progs[0].haveStartHzIn, PTSF(pAv->progs[0].lastHzOut),
     pAv->progs[1].haveStartHzIn, PTSF(pAv->progs[1].lastHzOut));

  for(idx = 0; idx < pAv->numProg; idx++) {

    pAv->progs[idx].haveStartHzIn = 0;
    pAv->progs[idx].startHzIn = 0;
    pAv->progs[idx].haveStartHzOut = 0;
    pAv->progs[idx].startHzOut = 0;
    pAv->progs[idx].lastHzIn = pAv->progs[idx].lastHzOut = 0;
    pAv->progs[idx].noMoreData = 0;
    pAv->progs[idx].frameId = 0;
    pAv->progs[idx].frameWrIdx = 0;
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      OUTFMT_LEN_IDX(&pAv->progs[idx].frameData, outidx) = 0;
    }

  }

  //pAv->haveStartHz = 0;
  //pAv->lastHz = 0;

  for(idx = 0; idx < pAv->numProg; idx++) {

    memset(&advfrData, 0, sizeof(advfrData));
    advfrData.pArgIn = pAv->progs[idx].pCbData;
    advfrData.pPts = (uint64_t *) & OUTFMT_PTS_IDX(&pAv->progs[idx].frameData, 0);
    advfrData.pDts = (int64_t *) & OUTFMT_DTS_IDX(&pAv->progs[idx].frameData, 0);
    advfrData.plen = (unsigned int *) & OUTFMT_LEN( &pAv->progs[idx].frameData) ;
    advfrData.pkeyframeIn = & OUTFMT_KEYFRAME( &pAv->progs[idx].frameData);

    if(advanceFrame(&pAv->progs[idx], &advfrData) <= STREAM_NET_ADVFR_RC_ERROR) { 
      LOG(X_ERROR("stream_av_reset failed for program[%d]/%d"), idx, pAv->numProg);
      return -1;
    }

    pAv->progs[idx].lastHzOffsetJmp = *advfrData.pPts;
    //fprintf(stderr, "stream_av_reset prog[%d] lastHzOffsetJmp: %.3f\n", idx, PTSF(pAv->progs[idx].lastHzOffsetJmp));
    //pAv->progs[idx].needLastHzOffsetJmp = 1;

    for(idxPktz = 0; idxPktz < pAv->progs[idx].numpacketizers; idxPktz++) {
      if(pAv->progs[idx].pktz[idxPktz].cbReset &&
         pAv->progs[idx].pktz[idxPktz].cbReset(pAv->progs[idx].pktz[idxPktz].pCbData, 
                                               fullReset, idx) < 0) {
        rc = -1;
        break;
      }
    }
    
  }

  if(pAv->plocationMs) {
    pAv->locationMsStart = *pAv->plocationMs;
  }

  pAv->prepareCalled = 0;

  return rc;
}


int stream_av_close(STREAM_AV_T *pAv) {
  //unsigned int idx, idxPktz;
  int rc = 0;

  if(!pAv) {
    return -1;
  }

/*
  for(idx = 0; idx < pAv->numProg; idx++) {
    for(idxPktz = 0; idxPktz < pAv->progs[idx].numpacketizers; idxPktz++) {
      if(pAv->progs[idx].pktz[idxPktz].cbClose && 
         pAv->progs[idx].pktz[idxPktz].cbClose(pAv->progs[idx].pktz[idxPktz].pCbData, idx) < 0) {
        rc = -1;
      }
    }
  }
*/
  return rc;
}

static int check_nextframe_rc(STREAM_AV_T *pAv, AV_PROG_T *pProg, 
                              enum STREAM_NET_ADVFR_RC advanceFrRc) {

  if(advanceFrRc <= STREAM_NET_ADVFR_RC_ERROR ||
     advanceFrRc == STREAM_NET_ADVFR_RC_NOCONTENT) {

    if(advanceFrRc != STREAM_NET_ADVFR_RC_ERROR_CRIT &&
       !pProg->stream.cbHaveMoreData(pProg->pCbData)) {
      pProg->noMoreData = 1;
      LOG(X_DEBUG("stream_av prog[%d] rc:%d no more data"), 
           (pProg == &pAv->progs[0] ? 0 : 1), advanceFrRc);
      return 0;
    } else if(advanceFrRc != STREAM_NET_ADVFR_RC_NOCONTENT) {
      pProg->noMoreData = 1;
      LOG(X_ERROR("Frame creation error for prog: 0x%x type: "
           MP2PES_STREAMTYPE_FMT_STR), pProg->progId, 
           MP2PES_STREAMTYPE_FMT_ARGS(pProg->streamType));
      //return -1;
      return advanceFrRc <= STREAM_NET_ADVFR_RC_ERROR ? advanceFrRc : -1;
    }

  } else if(advanceFrRc == STREAM_NET_ADVFR_RC_NEWPROG ||
            advanceFrRc == STREAM_NET_ADVFR_RC_RESET ||
            advanceFrRc == STREAM_NET_ADVFR_RC_OVERWRITE) {

     
    //fprintf(stderr, "CALLING STREAM_AV_RESET FROM CHECK_NEXT_FR\n");
    stream_av_reset(pAv, 2);
    //fprintf(stderr, "stream_av_preparepkt returning: 0 reset\n");
    return 0;

  } else if(advanceFrRc == STREAM_NET_ADVFR_RC_NOTAVAIL) {

    // No frame available at this time (live capture / transcoding)
    //pAv->lastFrUnaivalable = 1;
    //fprintf(stderr, "unavailable pktz %s[%d] len:%d pts:%.3f dts:%.3f key:%d\n", pProg->frameData.isvid ? "vid" : "aud", pProg->frameId, pProg->frameData.len, (double)pProg->frameData.pts/90000.0f, (double)pProg->frameData.dts/90000.0f, pProg->frameData.keyframe);
    //fprintf(stderr, "stream_av_preparepkt returning: 0 not avail rc:%d\n", advanceFrRc);
    return 0;

  }

  return 1;
}

enum STREAM_NET_ADVFR_RC advanceFrameWrap(AV_PROG_T *pProg) {

  enum STREAM_NET_ADVFR_RC advanceFrRc; 
  STREAM_NET_ADVFR_DATA_T advfrData;

  memset(&advfrData, 0, sizeof(advfrData));
  advfrData.pArgIn = pProg->pCbData;
  advfrData.pPts = & OUTFMT_PTS_IDX(&pProg->frameData, 0);
  advfrData.pDts = & OUTFMT_DTS_IDX(&pProg->frameData, 0);
  advfrData.plen = (unsigned int *) & OUTFMT_LEN_IDX( &pProg->frameData, 0);
  advfrData.pkeyframeIn = & OUTFMT_KEYFRAME_IDX( &pProg->frameData, 0) ;

  advanceFrRc = advanceFrame(pProg, &advfrData);

#if 1
#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
  STREAM_AV_T *pAv = pProg->pAv;
  //
  // This is a bad hack.
  // For mixed audio output, if we read a PIP from a file, we can read audio too fast (not in real-time)
  // and overfill the mixer, because the mixed output runs in real-time. 
  //
  if(advanceFrRc == STREAM_NET_ADVFR_RC_NOTAVAIL && pAv->numProg == 2 && pProg == &pAv->progs[1] &&
     pProg->pXcodeData && pProg->pXcodeData->piXcode->vid.pip.active && 
//     !pProg->pXcodeData->piXcode->vid.pip.doEncode &&
     pProg->pXcodeData->piXcode->aud.pMixerParticipant &&
     pAv->progs[0].haveStartHzIn && !pProg->haveStartHzIn &&
     (!pProg->stream.cbHaveDataNow || pProg->stream.cbHaveDataNow(advfrData.pArgIn) == 0)) {
    //fprintf(stderr, "stream_av tid:0x%x sleep...\n", pthread_self());
    usleep(15000);
  }
#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
#endif

  //fprintf(stderr, "called adv fr on rc:%d prog[%d/%d]0x%x,stype:0x%x, haveStartHzIn:%d, len:%d pts:%.3f\n", advanceFrRc, (pProg == &((STREAM_AV_T *)(pProg->pAv))->progs[0] ? 0 : 1), ((STREAM_AV_T *)(pProg->pAv))->numProg, pProg->progId, pProg->streamType, pProg->haveStartHzIn, OUTFMT_LEN(&pProg->frameData), PTSF(OUTFMT_PTS(&pProg->frameData)));

  //pip_check_idle();

  return advanceFrRc;
}

static int get_next_frame(STREAM_AV_T *pAv, AV_PROG_T **ppProg) {
  int moreData = 0;
  unsigned int idx;
  unsigned int lowestHzIdx = 0;
  enum STREAM_NET_ADVFR_RC advanceFrRc;
  uint64_t *pLastHz0, *pLastHz1;
  int *pHaveStartHz0 = NULL, *pHaveStartHz1 = NULL;
  int is_pip = 0;

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
  if(pAv->progs[0].pXcodeData && pAv->progs[0].pXcodeData->piXcode->vid.pip.active && 
//     !pAv->progs[0].pXcodeData->piXcode->vid.pip.doEncode &&
     pAv->progs[0].pXcodeData->piXcode->aud.pMixerParticipant) {
    is_pip = 1;
  }
#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  for(idx = 0; idx < pAv->numProg; idx++) {

    if(is_pip || pAv->useInputTiming) {
      // net_aconf, net_vconf we need to make sure cbAdvanceFrame is getting called for audio even before the first encoded video frame is returned... this could be a hack for that
      pHaveStartHz0 = &pAv->progs[idx].haveStartHzIn;
      pHaveStartHz1 = &pAv->progs[lowestHzIdx].haveStartHzIn;
      pLastHz0 = &pAv->progs[idx].lastHzIn;
      pLastHz1 = &pAv->progs[lowestHzIdx].lastHzIn;
    } else {
      pHaveStartHz0 = &pAv->progs[idx].haveStartHzOut;
      pLastHz0 = &pAv->progs[idx].lastHzOut;
      pHaveStartHz1 = &pAv->progs[lowestHzIdx].haveStartHzOut;
      pLastHz1 = &pAv->progs[lowestHzIdx].lastHzOut;
    }

    if(!pAv->progs[idx].noMoreData) {
      moreData = 1;
      if(idx != lowestHzIdx &&
         (pAv->progs[lowestHzIdx].noMoreData ||
          *pLastHz0 < *pLastHz1 ||
          (*pHaveStartHz1 && !(*pHaveStartHz0)))) { 
        lowestHzIdx = idx;
      }     
    }
  }

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
        LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x, get_next_frame is_pip:%d lowestHzIdx:[%d]/%d, lastHzOut:%lld,%lld, haveStartHzOut:%d,%d"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), is_pip, lowestHzIdx, pAv->numProg, pAv->progs[0].lastHzOut, pAv->progs[1].lastHzOut, pAv->progs[0].haveStartHzOut, pAv->progs[1].haveStartHzOut);
#endif  // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  if(pAv->avReaderType == VSX_STREAM_READER_TYPE_NOSYNC) {

    if(pAv->numProg > 1 && lowestHzIdx == 0 && pAv->progs[0].lastRc == STREAM_NET_ADVFR_RC_NOTAVAIL &&
       *pHaveStartHz1 && *pHaveStartHz0 &&
       pAv->progs[1].tmLastFrCheck > 0 && pAv->progs[0].tmLastFrCheck > pAv->progs[1].tmLastFrCheck) {

      TIME_VAL tvElapsedProg;
      TIME_VAL tvNow = timer_GetTime();
      TIME_VAL tvElapsed = tvNow - pAv->tmStartTx;
      if(pAv->progs[1].haveStartHzOut) {
        tvElapsedProg = (TIME_VAL) ((pAv->progs[1].lastHzOut - pAv->progs[1].startHzOut) * ((double)100.0 / 9.0f));
      } else {
        tvElapsedProg = 0;
      }
      tvElapsedProg += (pAv->msDelayTx * TIME_VAL_MS);

      if(tvElapsed >= tvElapsedProg && pAv->progs[1].stream.cbHaveDataNow && pAv->progs[1].stream.cbHaveDataNow(pAv->progs[1].pCbData)) {
      //if(pAv->progs[0].dtx || (tvElapsed >= tvElapsedProg && (tvNow - pAv->progs[1].tmLastFrCheck) > VIDEO_DTX_LATE_THRESHOLD_US)) {
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
        LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x, getting RT audio (was:%d), changing lowestHzIdx to 0, [0].ago:%lldus, [1].ago:%lldus, [1].cansend: %lldus >= (%lld-%lld)%lldus (%lldus past due)"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), pAv->progs[0].dtx, tvNow - pAv->progs[0].tmLastFrCheck, tvNow - pAv->progs[1].tmLastFrCheck, tvElapsed, pAv->progs[1].lastHzOut, pAv->progs[1].startHzOut, tvElapsedProg, tvElapsed - tvElapsedProg);
        dump_cap_queues(&pAv->progs[0]);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
        pAv->progs[lowestHzIdx].dtx = 1;
        lowestHzIdx = 1;

      }
    }
    
  } else if(pAv->avReaderType == VSX_STREAM_READER_TYPE_NOTIMESTAMPS) {
    //int lowestHzIdxPrior = lowestHzIdx;
    for(idx = pAv->numProg; idx > 0; idx--) {

      if(pAv->progs[idx-1].stream.cbHaveDataNow && 
         pAv->progs[idx-1].stream.cbHaveDataNow(pAv->progs[idx-1].pCbData)) {
        lowestHzIdx = idx -1; 
        break;
      }
    }
    //fprintf(stderr, "LOWESTHZIDX %d%s\n", lowestHzIdx, lowestHzIdxPrior !=lowestHzIdx ? " changed" : "");
  }


#if 1
  
  //TODO: dtx for live capture should not be less than jitter playout buffer

#define AUDIO_DTX_LATE_THRESHOLD_US     100000
  if(lowestHzIdx == 1 && pAv->progs[1].lastRc == STREAM_NET_ADVFR_RC_NOTAVAIL &&
     *pHaveStartHz0 && *pHaveStartHz1 &&
     pAv->progs[0].tmLastFrCheck > 0 && pAv->progs[1].tmLastFrCheck > pAv->progs[0].tmLastFrCheck) {

    TIME_VAL tvElapsedProg;
    TIME_VAL tvNow = timer_GetTime();
    TIME_VAL tvElapsed = tvNow - pAv->tmStartTx;
    if(pAv->progs[0].haveStartHzOut) {
      tvElapsedProg = (TIME_VAL) ((pAv->progs[0].lastHzOut - pAv->progs[0].startHzOut) * ((double)100.0 / 9.0f));
    } else {
      tvElapsedProg = 0;
    }
    tvElapsedProg += (pAv->msDelayTx * TIME_VAL_MS);

    if(pAv->progs[1].dtx || (tvElapsed >= tvElapsedProg && (tvNow - pAv->progs[0].tmLastFrCheck) > AUDIO_DTX_LATE_THRESHOLD_US)) {
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
      LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x, possible audio dtx(was:%d), changing lowestHzIdx to 0, [0].ago:%lldus, [1].ago:%lldus, [0].cansend: %lldus >= (%lld-%lld)%lldus (%lldus past due)"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), pAv->progs[1].dtx, tvNow - pAv->progs[0].tmLastFrCheck, tvNow - pAv->progs[1].tmLastFrCheck, tvElapsed, pAv->progs[0].lastHzOut, pAv->progs[0].startHzOut, tvElapsedProg, tvElapsed - tvElapsedProg);
      dump_cap_queues(&pAv->progs[0]);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
      pAv->progs[lowestHzIdx].dtx = 1;
      lowestHzIdx = 0;

    }
  }

#define VIDEO_DTX_LATE_THRESHOLD_US     500000
  if(pAv->numProg > 1 && lowestHzIdx == 0 && pAv->progs[0].lastRc == STREAM_NET_ADVFR_RC_NOTAVAIL &&
     *pHaveStartHz1 && *pHaveStartHz0 &&
     pAv->progs[1].tmLastFrCheck > 0 && pAv->progs[0].tmLastFrCheck > pAv->progs[1].tmLastFrCheck) {

    TIME_VAL tvElapsedProg;
    TIME_VAL tvNow = timer_GetTime();
    TIME_VAL tvElapsed = tvNow - pAv->tmStartTx;
    if(pAv->progs[1].haveStartHzOut) {
      tvElapsedProg = (TIME_VAL) ((pAv->progs[1].lastHzOut - pAv->progs[1].startHzOut) * ((double)100.0 / 9.0f));
    } else {
      tvElapsedProg = 0;
    }
    tvElapsedProg += (pAv->msDelayTx * TIME_VAL_MS);

    if(pAv->progs[0].dtx || (tvElapsed >= tvElapsedProg && (tvNow - pAv->progs[1].tmLastFrCheck) > VIDEO_DTX_LATE_THRESHOLD_US)) {
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
      LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x, possible video dtx(was:%d), changing lowestHzIdx to 0, [0].ago:%lldus, [1].ago:%lldus, [1].cansend: %lldus >= (%lld-%lld)%lldus (%lldus past due)"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), pAv->progs[0].dtx, tvNow - pAv->progs[0].tmLastFrCheck, tvNow - pAv->progs[1].tmLastFrCheck, tvElapsed, pAv->progs[1].lastHzOut, pAv->progs[1].startHzOut, tvElapsedProg, tvElapsed - tvElapsedProg);
      dump_cap_queues(&pAv->progs[0]);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
      pAv->progs[lowestHzIdx].dtx = 1;
      lowestHzIdx = 1;

    }
  }

#endif // 1 

#if 0
  int pip_try_vid = 0;
  //
  // For mixed audio output, if we read a PIP from a file, we can read audio too fast (not in real-time)
  // and overfill the mixer, because the mixed output runs in real-time. 
  // The 12000 offset depends on the HZ audio output 
  //
  if(lowestHzIdx == 1 && 
     !pAv->progs[0].noMoreData &&
     pAv->progs[lowestHzIdx].pXcodeData &&
     //pAv->progs[lowestHzIdx].pXcodeData->piXcode->vid.pip.active &&
     pAv->progs[lowestHzIdx].pXcodeData->piXcode->aud.pMixerParticipant &&
     pAv->progs[0].haveStartHz && !pAv->progs[lowestHzIdx].haveStartHz &&
     pAv->progs[0].lastHzIn + 12000 < pAv->progs[lowestHzIdx].lastHzIn) {
    pip_try_vid = 1;
    lowestHzIdx = 0;
  }
#endif // 0
 
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
  if(pAv->progs[0].pXcodeData->piXcode->vid.pip.active) {
    LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x, next advanceFrameWrap lowestHzIdx (from:%s) is [%d]/%d lastHzOut 0:%.3f, 1:%.3f, lastHzIn 0:%.3f, 1:%.3f, noMoreData 0:%d, 1:%d, haveStartHzIn 0:%d, 1:%d, Out 0:%d, 1:%d"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), pLastHz1 == &pAv->progs[0].lastHzOut || pLastHz1 == &pAv->progs[1].lastHzOut ? "OUT" :  "IN", lowestHzIdx, pAv->numProg, PTSF(pAv->progs[0].lastHzOut), PTSF(pAv->progs[1].lastHzOut), PTSF(pAv->progs[0].lastHzIn), PTSF(pAv->progs[1].lastHzIn), pAv->progs[0].noMoreData, pAv->progs[1].noMoreData, pAv->progs[0].haveStartHzIn, pAv->progs[1].haveStartHzIn, pAv->progs[0].haveStartHzOut, pAv->progs[1].haveStartHzOut);
    dump_cap_queues(&pAv->progs[0]);
  }
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

  if(!moreData) {
    LOG(X_DEBUG("All programs have ended"));
    return -2;
  }

 
  *ppProg = &pAv->progs[lowestHzIdx];

  advanceFrRc = advanceFrameWrap(*ppProg);

  //fprintf(stderr, "called adv fr on rc:%d prog[%d/%d]0x%x,stype:0x%x, lowestHzIdx:%d, haveStartHzIn:%d len:%d pts:%.3f\n", advanceFrRc, lowestHzIdx, pAv->numProg, (*ppProg)->progId, (*ppProg)->streamType, lowestHzIdx, (*ppProg)->haveStartHzIn, OUTFMT_LEN(&(*ppProg)->frameData), PTSF(OUTFMT_PTS(&(*ppProg)->frameData)));

  return check_nextframe_rc(pAv, *ppProg, advanceFrRc);
}

static int get_next_frame_preferaudio(STREAM_AV_T *pAv, AV_PROG_T **ppProg) {
  int checknextprog = 0;
  unsigned int lowestHzIdx = 0;
  enum STREAM_NET_ADVFR_RC advanceFrRc = STREAM_NET_ADVFR_RC_NOTAVAIL;

  if(pAv->numProg > 1 && !pAv->progs[1].noMoreData) {
    lowestHzIdx = 1;

    *ppProg = &pAv->progs[lowestHzIdx];

    if((advanceFrRc = advanceFrameWrap(*ppProg)) == STREAM_NET_ADVFR_RC_NOTAVAIL) {
      if(!pAv->progs[0].noMoreData) {
        lowestHzIdx = 0;
        *ppProg = &pAv->progs[lowestHzIdx];
        checknextprog = 1;
      }
    }

  } else if(pAv->progs[0].noMoreData && (pAv->numProg == 1 ||
            (pAv->numProg > 1 && pAv->progs[1].noMoreData))) {
    LOG(X_DEBUG("All programs have ended"));
    return -2;
  }

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
  LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x, next_preferaudio advanceFrameWrap lowestHzIdx is [%d]/%d lastHzOut 0:%.3f, 1:%.3f, lastHzIn 0:%.3f, 1:%.3f, noMoreData 0:%d, 1:%d, haveStartHzIn 0:%d, 1:%d, Out 0:%d, 1:%d"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(),  lowestHzIdx, pAv->numProg, PTSF(pAv->progs[0].lastHzOut), PTSF(pAv->progs[1].lastHzOut), PTSF(pAv->progs[0].lastHzIn), PTSF(pAv->progs[1].lastHzIn), pAv->progs[0].noMoreData, pAv->progs[1].noMoreData, pAv->progs[0].haveStartHzIn, pAv->progs[1].haveStartHzIn, pAv->progs[0].haveStartHzOut, pAv->progs[1].haveStartHzOut);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)

  if(checknextprog) {
    advanceFrRc = advanceFrameWrap(*ppProg);
  }

  return check_nextframe_rc(pAv, *ppProg, advanceFrRc);
}

//int g_fd;
//static struct timeval g_tv,g_tv1;
//static int g_done;

//#define FRAME_THIN_TEST 1

#if defined(FRAME_THIN_TEST)
static int g_gop, g_sent, g_skip;
#endif // FRAME_THIN_TEST


int stream_av_preparepkt(void *pArg) {
  STREAM_AV_T *pAv = (STREAM_AV_T *) pArg;
  AV_PROG_T *pProg = NULL;
  int rc = 0;
  int rcpktz = 0;
  unsigned int idx;

  pAv->prepareCalled = 1;
  VSX_DEBUG_STREAMAV(LOG(X_DEBUGV("stream_av_preparepkt numProg:%d lastHz:[0]:%.3f, [1]:%.3f) " "noMoreData:%d,%d"), 
    pAv->numProg, PTSF(pAv->progs[0].lastHzOut), 
    PTSF(pAv->progs[1].lastHzOut), pAv->progs[0].noMoreData, pAv->progs[1].noMoreData));

  if((pAv->avReaderType == VSX_STREAM_READER_TYPE_PREFERAUDIO)) {
    rc = get_next_frame_preferaudio(pAv, &pProg);
  } else {
    rc = get_next_frame(pAv, &pProg);
  }

  //fprintf(stderr, "get_next_frame returned %d, pProg: 0x%x\n", rc, pProg);

  if(rc <= 0) {
    return rc;
  } else if(!pProg) {
    return -1;
  }

  //
  // Apply audio / video sync input ts offset
  //

  for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
    if(pProg->pavs[idx]) {

      //if(pProg->pXcodeData->piXcode->vid.out[idx].passthru ||
      //   (idx > 0 && !pProg->pXcodeData->piXcode->vid.out[idx].active)) {
      if(!pProg->pXcodeData->piXcode->vid.out[idx].active) {
        continue;
      }

      //fprintf(stderr, "stream_av outidx[%d] prog av offsetCur:%.3f offset0:%.3f\n", idx, PTSF(pProg->pavs[idx]->offsetCur), PTSF(pProg->pavs[idx]->offset0));
//fprintf(stderr, "[0].offsetCur:%.3f, [1].offsetCur:%.3f\n", PTSF(pProg->pavs[idx]->offsetCur), PTSF(pProg->pavs[idx]->offsetCur));
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)
      if((pProg->pavs[idx]->offsetCur > 0 && pProg->frameData.isaud) ||
         (pProg->pavs[idx]->offsetCur < 0 && pProg->frameData.isvid)) {
        LOG(X_DEBUG("%llu.%llu stream_av tid:0%x, applying a/v %.3f offset to prog[%d], outidx[%d] to pts:%.3f"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), PTSF(abs(pProg->pavs[idx]->offsetCur)), pProg == &pAv->progs[0] ? 0 : pProg == &pAv->progs[1] ? 1 : -1, idx, PTSF(OUTFMT_PTS_IDX(&pProg->frameData, idx)));
      }
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 0)

      //
      // Only apply a positive offset to the appropriate program
      // Negative offsetCur value means decrement audio ts (delay video)
      // Positive offsetCur value means increment audio ts (delay audio)
      //
      if(pProg->pavs[idx]->offsetCur > 0 && pProg->frameData.isaud) {
        OUTFMT_PTS_IDX(&pProg->frameData, idx) += pProg->pavs[idx]->offsetCur; 
        //LOG(X_DEBUG("mkvsrv_ Added offset of %.3f to audio frame time: %.3f"), PTSF(pProg->pavs[idx]->offsetCur),  PTSF(OUTFMT_PTS_IDX(&pProg->frameData, idx)));
      } else if(pProg->pavs[idx]->offsetCur < 0 && pProg->frameData.isvid) {
        OUTFMT_PTS_IDX(&pProg->frameData, idx) += abs(pProg->pavs[idx]->offsetCur);
        //LOG(X_DEBUG("mkvsrv_ Added offset of %.3f to video frame time: %.3f"), PTSF(abs(pProg->pavs[idx]->offsetCur)),  PTSF(OUTFMT_PTS_IDX(&pProg->frameData, idx)));
      } else {
        //LOG(X_DEBUG("mkvsrv_ no offset for %s frame time: %.3f"), pProg->frameData.isvid ? "video" : "audio",  PTSF(OUTFMT_PTS_IDX(&pProg->frameData, idx)));
      }

/*
      if(pProg->pavs[idx]->offsetCur > 0) {
        OUTFMT_PTS_IDX(&pProg->frameData, idx) += pProg->pavs[idx]->offsetCur; 
      } else if(pProg->pavs[idx]->offsetCur < 0) {
        if(pProg->pavs[idx]->offsetCur * -1 > (int64_t) OUTFMT_PTS_IDX(&pProg->frameData, idx)) {
          OUTFMT_PTS_IDX(&pProg->frameData, idx) = 0;
        } else {
          OUTFMT_PTS_IDX(&pProg->frameData, idx) += pProg->pavs[idx]->offsetCur;
        }
      }
*/

    }

  } // end of for(idx...

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
  LOG(X_DEBUG("%llu.%llu stream_av tid: 0x%x, changing lastHzout %.3f (%llu) -> %.3f (%llu) (-> lastHzIn:%.3f), pip.active:%d, isvid:%d"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), PTSF(pProg->lastHzOut), pProg->lastHzOut, PTSF(OUTFMT_PTS_IDX(&pProg->frameData, 0)), OUTFMT_PTS_IDX(&pProg->frameData, 0), (!pProg->pXcodeData || !pProg->pXcodeData->piXcode->vid.pip.active || !pProg->pXcodeData->piXcode->aud.pMixerParticipant) ? PTSF(OUTFMT_PTS_IDX(&pProg->frameData, 0)) : PTSF(pProg->lastHzIn), pProg->pXcodeData->piXcode->vid.pip.active, pProg->frameData.isvid);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)

  pProg->lastHzOut = OUTFMT_PTS_IDX(&pProg->frameData, 0);

  //
  // If this is a PIP thread with mixed audio, preserve the lastHzIn obtained from the input pts
  //
/*
  if(!pProg->pXcodeData || !pProg->pXcodeData->piXcode->vid.pip.active 
#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
   || !pProg->pXcodeData->piXcode->aud.pMixerParticipant
#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
   ) {
    //pProg->lastHzIn = pProg->lastHzOut;
    //fprintf(stderr, "tid:0x%x, set isvid:%d lastHzIn to %.3f (%lluHz)\n", pthread_self(), pProg->frameData.isvid, PTSF(pProg->lastHzOut), pProg->lastHzOut);
  } else {
    pProg->plastHz = &pProg->lastHzIn;
    //fprintf(stderr, "tid:0x%x, not set isvid:%d lastHzIn to %.3f (%lluHz)\n", pthread_self(), pProg->frameData.isvid, PTSF(pProg->lastHzOut), pProg->lastHzOut);
  }
*/

  if(!pProg->haveStartHzOut) {
    //pProg->startHz = pProg->lastHzIn;
    pProg->startHzOut = pProg->lastHzOut;
    pProg->haveStartHzOut = 1;
    //fprintf(stderr, "set prog->startHz %.3f\n", PTSF(pProg->startHz));
  }


  //pAv->lastHz = pProg->lastHzOut;
  //if(!pAv->haveStartHzIn || (pProg->lastHzOut > 0 && pProg->lastHzOut < pAv->startHz)) {
  //  pAv->startHz = pAv->lastHz;
  //  pAv->haveStartHzIn = 1;
  //}

  if(OUTFMT_LEN(&pProg->frameData) == 0) {
    // Program disabled or frame not yet available, or this is a pip thread
    //fprintf(stderr, "stream_av_preparepkt tid: 0x%x returning: 0 not avail, len 0, numpacketizers:%d \n", pthread_self(), pProg->numpacketizers);
    return 0;
  }

  pProg->frameId++;

//fprintf(stderr, "ENCODE OUT IDX:%d\n", pProg->pXcodeData->piXcode->vid.common.encodeOutIdx);
 //fprintf(stderr, "STREAM_AV_PREPAREPKT tid: 0x%x, numpacketizers:%d\n", pthread_self(), pProg->numpacketizers);
//update_packetizers(pProg);

  // 
  // Call any frame output packetizers
  //
  for(idx = 0; idx < pProg->numpacketizers; idx++) {

    //fprintf(stderr, "PKTZ IDX[%d]/%d isactive:%d, do_mask:0x%x, numD:%d\n", idx, pProg->numpacketizers, pProg->pktz[idx].isactive, pProg->pktz[idx].pktz_do_mask, pProg->pktz[idx].pRtpMulti->numDests);

    if(pProg->pktz[idx].isactive) {

      if((pProg->pktz[idx].pktz_do_mask & (PKTZ_MASK_VID | PKTZ_MASK_AUD)) &&
         pProg->pktz[idx].pRtpMulti->numDests == 0) {
          //fprintf(stderr, "PKTZ CONT... pktz_do_mask:%0x, numD:%d, pRtpMulti:0x%x\n",  pProg->pktz[idx].pktz_do_mask, pProg->pktz[idx].pRtpMulti->numDests, pProg->pktz[idx].pRtpMulti);
          continue;
      }

      //
      // Set the output frame outidx parameter
      //
      if((pProg->frameData.isvid && pProg->pXcodeData->piXcode->vid.common.cfgDo_xcode) ||
         (!pProg->frameData.isvid && pProg->pXcodeData->piXcode->aud.common.cfgDo_xcode)) {
        pProg->frameData.xout.outidx = pProg->pktz[idx].outidx;
//if(g_tv.tv_sec==0)gettimeofday(&g_tv, NULL); gettimeofday(&g_tv1, NULL); if(g_tv1.tv_sec-g_tv.tv_sec>10) pProg->frameData.xout.outidx = 1; else pProg->frameData.xout.outidx=0;
//if(g_tv.tv_sec==0)gettimeofday(&g_tv, NULL); gettimeofday(&g_tv1, NULL); if(!g_done && g_tv1.tv_sec-g_tv.tv_sec>5) { g_done=1; xcode_vid_test(pProg->pXcodeData);}

      } else {
        pProg->frameData.xout.outidx = 0;
      }

#if defined(FRAME_THIN_TEST)
if(pProg->frameData.isvid) {
g_gop++;
if(OUTFMT_KEYFRAME(&pProg->frameData)) {
  fprintf(stderr, "GOP size:%d %.2f%\n", g_gop, (g_skip+g_sent) > 0 ? ((float)g_sent/(g_skip+g_sent)* 100.0) : 0);
  g_gop = g_sent = g_skip = 0;
}
}
#endif // FRAME_THIN_TEST

    //fprintf(stderr, "tid:0x%x calling pktz[%d/%d]do_mask:0x%x outidx[%d(%d)] %s[%lld] dests:%d len[%d]:%d pts:%.3f dts:%.3f key:%d ", pthread_self(), idx, pProg->numpacketizers, pProg->pktz[idx].pktz_do_mask, pProg->pktz[idx].outidx, pProg->frameData.xout.outidx, pProg->frameData.isvid ? "vid" : "aud", pProg->frameId, pProg->pktz[idx].pRtpMulti->numDests, pProg->frameData.xout.outidx, OUTFMT_LEN(&pProg->frameData), PTSF(OUTFMT_PTS(&pProg->frameData)), PTSF(OUTFMT_DTS(&pProg->frameData)), OUTFMT_KEYFRAME(&pProg->frameData)); avc_dumpHex(stderr, OUTFMT_DATA(&pProg->frameData), MIN(16, OUTFMT_LEN(&pProg->frameData)), 0); 
    //fprintf(stderr, "prog[0].do_mask:0x%x, prog[1].do_mask:0x%x, pProg is %s\n", pAv->progs[0].pktz[idx].pktz_do_mask, pAv->progs[1].pktz[idx].pktz_do_mask, pProg == &pAv->progs[0] ? "0" : "1");

#if defined(FRAME_THIN_TEST)
if(1 || !pProg->frameData.isvid || g_gop < 4) {
#endif // FRAME_THIN_TEST

//static int g_vidfr; if(pProg->frameData.isvid) fprintf(stderr, "VID:%d\n", g_vidfr++);

      if(pProg->pktz[idx].cbNewFrame(pProg->pktz[idx].pCbData, 
                                     (pProg == &pAv->progs[0] ? 0 : 1) ) < 0) {
        LOG(X_ERROR("Unable to process frame for packetizer[%d] prog[%d]"), 
                    idx, (pProg == &pAv->progs[0] ? 0 : 1));
        rcpktz = -1;
      }

#if defined(FRAME_THIN_TEST)
if(pProg->frameData.isvid) g_sent++;
} else if(pProg->frameData.isvid){
g_skip++;
}
#endif // FRAME_THIN_TEST

    } // if(pProg->pktz[idx].isactive ...
  } //for(idx = 0; idx < pProg->numpacketizers...

  //
  // Call the intermediate output handler w/ the newly acquired frame
  // contents, such as RTMP / FLV construction.
  // Note:  RTMP & FLV addFrame creates its own outidx depending on the protocol specific index request
  //
  outfmt_invokeCbs(pProg->pLiveFmts, &pProg->frameData);

  if(rcpktz < 0) {
    rc = -1;
  } else if(rc == 0) {
    rc = 1;
  }

  //fprintf(stderr, "stream_av_preparepkt tid: 0x%x returning: %d\n", pthread_self(), rc);
  return rc;
}

int stream_av_cansend(void *pArg) {
  STREAM_AV_T *pAv = (STREAM_AV_T *) pArg;
  TIME_VAL tvNow, tvElapsed;
  TIME_VAL tvElapsedProg;
  unsigned int idx;
  int haveMoreData = 0;
  int rc = 0;
  int is_pip_file = 0;
  uint64_t *pLastHz, *pStartHz;
  int *pHaveStartHz;

  //fprintf(stderr, "stream_av_cansend tid:0x%x, numProg;%d\n", pthread_self(), pAv->numProg);

  if(pAv->prepareCalled == 0) {
    pAv->prepareCalled = -1;
    pAv->tmStartTx = timer_GetTime();
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
    LOG(X_DEBUG("%llu.%llu stream_av tid:0x%x set (prepareCalled...) tmStartTx from to %llu us"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), pAv->tmStartTx);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
  }

  if(pAv->progs[0].pXcodeData->piXcode->vid.pip.active && 
//     !pAv->progs[0].pXcodeData->piXcode->vid.pip.doEncode &&
     pAv->progs[0].stream.type == STREAM_NET_TYPE_FILE) {
    is_pip_file = 1;
  } 

  tvNow = timer_GetTime();

  tvElapsed = tvNow - pAv->tmStartTx;

  for(idx = 0; idx < pAv->numProg; idx++) {

    tvElapsedProg = 0;

    if(pAv->progs[idx].noMoreData) {
      continue;
    } else {
      haveMoreData = 1;
    }

    if(is_pip_file) {
      pLastHz = &pAv->progs[idx].lastHzIn;
      pStartHz = &pAv->progs[idx].startHzIn;
      pHaveStartHz = &pAv->progs[idx].haveStartHzIn;
/*
      if((pAv->progs[idx].haveStartHzOut && pAv->progs[idx].lastHzOut > pAv->progs[idx].lastHzIn)) {
        pLastHz = &pAv->progs[idx].lastHzOut;
        pStartHz = &pAv->progs[idx].startHzOut;
        pHaveStartHz = &pAv->progs[idx].haveStartHzOut;
      } else {
        pLastHz = &pAv->progs[idx].lastHzIn;
        pStartHz = &pAv->progs[idx].startHzIn;
        pHaveStartHz = &pAv->progs[idx].haveStartHzIn;
      }
*/
    } else {
      pLastHz = &pAv->progs[idx].lastHzOut;
      pStartHz = &pAv->progs[idx].startHzOut;
      pHaveStartHz = &pAv->progs[idx].haveStartHzOut;
    }

    if(*pHaveStartHz) {
      tvElapsedProg = (TIME_VAL) ((*pLastHz - *pStartHz) * ((double)100.0 / 9.0f));
    }

    //
    // Add any program delay
    //
    tvElapsedProg += (pAv->msDelayTx * TIME_VAL_MS);

    //if(pAv->progs[idx].pXcodeData && pAv->progs[idx].pXcodeData->piXcode->vid.pip.active) 
    //fprintf(stderr, "%llu.%llu stream_av_cansend tid: 0x%x cansend??? pip idx:%d tvElapsed:%lluus, tvElapsedProg:%lluus lastHzIn:%llu (%.3f), lastHzOut:%llu (%.3f) startHz:%llu (%.3f)\n", timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), idx, tvElapsed, tvElapsedProg, pAv->progs[idx].lastHzIn, PTSF(pAv->progs[idx].lastHzIn), pAv->progs[idx].lastHzOut, PTSF(pAv->progs[idx].lastHzOut), pAv->progs[idx].startHzOut, PTSF(pAv->progs[idx].startHzOut));

    if(pAv->avReaderType == VSX_STREAM_READER_TYPE_NOTIMESTAMPS && pAv->progs[idx].stream.cbHaveDataNow) {

      if(pAv->prepareCalled == -1 && (rc = stream_av_preparepkt(pArg)) < 0) {
        return rc;
      }

      if(pAv->progs[idx].stream.cbHaveDataNow(pAv->progs[idx].pCbData)) {
        //fprintf(stderr, "%llu.%llu stream_av_cansend tid: 0x%x (pip.active:%d) prog[%d/%d] %lluus >= %lluus lastHzIn:(%.3f %lluHz - %lluHz), lastHzOut:%.3f, %lluHz, dly:%u, late:%llu ms\n", timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), pAv->progs[idx].pXcodeData ? pAv->progs[idx].pXcodeData->piXcode->vid.pip.active : 0, idx, pAv->numProg, tvElapsed, tvElapsedProg, PTSF(pAv->progs[idx].lastHzIn), pAv->progs[idx].lastHzIn, pAv->progs[idx].startHzIn, PTSF(pAv->progs[idx].lastHzOut), pAv->progs[idx].lastHzOut, pAv->msDelayTx, pAv->progs[idx].pXcodeData ? pAv->progs[idx].pXcodeData->tvLateFromRT/TIME_VAL_MS : 0);
        //fprintf(stderr, "prog[0].lastHzIn:%llu - startHzIn:%llu = %.3f, lastHzOut:%llu - startHzOut:%llu = %.3f, (inlate:%lldms, outlate:%lldms), last poll:%llu us ago, rc:%d\n",  pAv->progs[0].lastHzIn, pAv->progs[0].startHzIn, PTSF(pAv->progs[0].lastHzIn - pAv->progs[0].startHzIn), pAv->progs[0].lastHzOut, pAv->progs[0].startHzOut, PTSF(pAv->progs[0].lastHzOut - pAv->progs[0].startHzOut), (tvElapsed - ( (TIME_VAL) ((pAv->progs[0].lastHzIn - pAv->progs[0].startHzIn) * ((double)100.0 / 9.0f))))/1000, (tvElapsed - ( (int64_t) ((pAv->progs[0].lastHzOut - pAv->progs[0].startHzOut) * ((double)100.0 / 9.0f))))/1000, tvNow - pAv->progs[0].tmLastFrCheck, pAv->progs[0].lastRc);
        //fprintf(stderr, "prog[1].lastHzIn:%llu - startHzIn:%llu = %.3f, lastHzOut:%llu - startHzOut:%llu = %.3f, (inlate:%lldms, outlate:%lldms), last poll:%llu us ago, rc:%d\n", pAv->progs[1].lastHzIn, pAv->progs[1].startHzIn, PTSF(pAv->progs[1].lastHzIn - pAv->progs[1].startHzIn), pAv->progs[1].lastHzOut, pAv->progs[1].startHzOut, PTSF(pAv->progs[1].lastHzOut - pAv->progs[1].startHzOut), (tvElapsed - ( (TIME_VAL) ((pAv->progs[1].lastHzIn - pAv->progs[1].startHzIn) * ((double)100.0 / 9.0f))))/1000, (tvElapsed - ( (int64_t) ((pAv->progs[1].lastHzOut - pAv->progs[1].startHzOut) * ((double)100.0 / 9.0f))))/1000, tvNow - pAv->progs[1].tmLastFrCheck, pAv->progs[1].lastRc);
        rc = 1;
        break;
      }


    } else if((!*pHaveStartHz && tvElapsed >= (TIME_VAL_MS * pAv->msDelayTx)) || tvElapsed >= tvElapsedProg) {

      if(pAv->plocationMs) {
        *pAv->plocationMs = (double) (tvElapsed / 1000) + pAv->locationMsStart;
      }
      
      // 
      // This lateness offset also includes the encoder frame lag time
      //
      if(pAv->progs[idx].pXcodeData) {
        pAv->progs[idx].pXcodeData->tvLateFromRT = tvElapsed - tvElapsedProg;
      }

      if(pAv->prepareCalled == -1 && (rc = stream_av_preparepkt(pArg)) < 0) {
        return rc;
      }
   
#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
      LOG(X_DEBUG("%llu.%llu stream_av_cansend tid: 0x%x (pip.active:%d) prog[%d/%d] %lluus >= %lluus lastHzIn:(%.3f %lluHz - %lluHz), lastHzOut:%.3f, %lluHz, dly:%u, late:%llu ms"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pthread_self(), pAv->progs[idx].pXcodeData ? pAv->progs[idx].pXcodeData->piXcode->vid.pip.active : 0, idx, pAv->numProg, tvElapsed, tvElapsedProg, PTSF(pAv->progs[idx].lastHzIn), pAv->progs[idx].lastHzIn, pAv->progs[idx].startHzIn, PTSF(pAv->progs[idx].lastHzOut), pAv->progs[idx].lastHzOut, pAv->msDelayTx, pAv->progs[idx].pXcodeData ? pAv->progs[idx].pXcodeData->tvLateFromRT/TIME_VAL_MS : 0);
      LOG(X_DEBUG("%llu.%llu stream_av prog[0].lastHzIn:%llu - startHzIn:%llu = %.3f, lastHzOut:%llu - startHzOut:%llu = %.3f, (inlate:%lldms, outlate:%lldms), last poll:%llu us ago, rc:%d"),  timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pAv->progs[0].lastHzIn, pAv->progs[0].startHzIn, PTSF(pAv->progs[0].lastHzIn - pAv->progs[0].startHzIn), pAv->progs[0].lastHzOut, pAv->progs[0].startHzOut, PTSF(pAv->progs[0].lastHzOut - pAv->progs[0].startHzOut), (tvElapsed - ( (TIME_VAL) ((pAv->progs[0].lastHzIn - pAv->progs[0].startHzIn) * ((double)100.0 / 9.0f))))/1000, (tvElapsed - ( (int64_t) ((pAv->progs[0].lastHzOut - pAv->progs[0].startHzOut) * ((double)100.0 / 9.0f))))/1000, tvNow - pAv->progs[0].tmLastFrCheck, pAv->progs[0].lastRc);
      LOG(X_DEBUG("%llu.%llu stream_av prog[1].lastHzIn:%llu - startHzIn:%llu = %.3f, lastHzOut:%llu - startHzOut:%llu = %.3f, (inlate:%lldms, outlate:%lldms), last poll:%llu us ago, rc:%d"), timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, pAv->progs[1].lastHzIn, pAv->progs[1].startHzIn, PTSF(pAv->progs[1].lastHzIn - pAv->progs[1].startHzIn), pAv->progs[1].lastHzOut, pAv->progs[1].startHzOut, PTSF(pAv->progs[1].lastHzOut - pAv->progs[1].startHzOut), (tvElapsed - ( (TIME_VAL) ((pAv->progs[1].lastHzIn - pAv->progs[1].startHzIn) * ((double)100.0 / 9.0f))))/1000, (tvElapsed - ( (int64_t) ((pAv->progs[1].lastHzOut - pAv->progs[1].startHzOut) * ((double)100.0 / 9.0f))))/1000, tvNow - pAv->progs[1].tmLastFrCheck, pAv->progs[1].lastRc);

#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
      rc = 1;
      break;

    }

    //
    // If this is a PIP video streaming thread, check if the main overlay has buffered an output frame ready
    // to be processed for us, even if we don't have any input PIP frame ready 
    //

    if(rc == 0 && stream_av_havepipframe(&pAv->progs[idx])) {

      pAv->progs[idx].pipUpsampleFr = 1;
      if(pAv->prepareCalled == -1 && (rc = stream_av_preparepkt(pArg)) < 0) {
        return rc;
      }

#if defined(DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)
        LOG(X_DEBUG("stream_av_cansend-havevidpipfr tid: 0x%x prog[%d/%d] %lluus >= %lluus lastHzIn:(%.3f %lluHz - %lluHz), lastHzOut:%.3f %lluHz, dly:%u, late:%llu ms"), pthread_self(), idx, pAv->numProg, tvElapsed, tvElapsedProg, PTSF(pAv->progs[idx].lastHzIn), pAv->progs[idx].lastHzIn, pAv->progs[idx].startHzIn, PTSF(pAv->progs[idx].lastHzOut), pAv->progs[idx].lastHzOut, pAv->msDelayTx, pAv->progs[idx].pXcodeData ? pAv->progs[idx].pXcodeData->tvLateFromRT/TIME_VAL_MS : 0);
#endif // (DEBUG_MIXER_TIMING) && (DEBUG_MIXER_TIMING > 2)

      rc = 1;
      break;

    }

  }

  //if(pAv->progs[0].pXcodeData->piXcode->vid.pip.active && pAv->progs[1].frameId > 20) g_proc_exit=1;

  if(!haveMoreData) {
    return -2;
  }

  VSX_DEBUG_STREAMAV( LOG(X_DEBUGV("cansend returning %d"), rc));

  return rc;
}


#endif // VSX_HAVE_STREAMER
