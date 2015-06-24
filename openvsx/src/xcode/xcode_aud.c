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


static enum STREAM_NET_ADVFR_RC init_aud(STREAM_XCODE_DATA_T *pData) {

  IXCODE_AUDIO_CTXT_T *pXcode = &pData->piXcode->aud;
  STREAM_XCODE_AUD_UDATA_T *pUData = (STREAM_XCODE_AUD_UDATA_T *)pXcode->pUserData;

  switch(pXcode->common.cfgFileTypeOut) {
    case XC_CODEC_TYPE_G711_MULAW:
    case XC_CODEC_TYPE_G711_ALAW:
    case XC_CODEC_TYPE_AMRNB:
      if(pXcode->cfgChannelsOut != 1) {
        pXcode->cfgChannelsOut = 1;
      }
      break;
    default:
      break;
  }

  if(xcode_init_aud(pXcode) != 0) {
    LOG(X_ERROR("Failed to init audio transcoder"));
    return STREAM_NET_ADVFR_RC_ERROR;
  }

  //fprintf(stderr, "INIT_AUD hdrLen:%d\n", pXcode->hdrLen);

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
  //
  // The mixer initialization is delayed until now because we may not have known the audio
  // output sample rate if it was not specified in the audio xcoder configuration
  //
  if(pUData->pStartMixer) {
    if(pip_create_mixer(pUData->pStartMixer, 1) < 0) {
      return STREAM_NET_ADVFR_RC_ERROR;
    }
    pUData->pStartMixer = 0;
    LOG(X_DEBUG("Created audio mixer"));
  } 
#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  pData->piXcode->aud.common.is_init = 1;

  return STREAM_NET_ADVFR_RC_OK;
}

#define XCODE_AUD_DRIFT_ADJUST_PTS    30000
//#define XCODE_AUD_DRIFT_ADJUST_PTS    90000

/*
static void setCompoundFramesCount(const IXCODE_AUDIO_CTXT_T *pXcode, STREAM_XCODE_DATA_T *pData, 
                                   unsigned int encodeOutIdx) {
  if(pXcode->common.encodeOutIdx > encodeOutIdx) {
    if((pData->curFrame.numCompoundFrames = pXcode->common.encodeOutIdx - encodeOutIdx) >
        COMPOUND_FRAMES_MAX) {
      LOG(X_WARNING("Compound audio frames limited from %d to %d"),
                    pData->curFrame.numCompoundFrames, COMPOUND_FRAMES_MAX);
      pData->curFrame.numCompoundFrames = COMPOUND_FRAMES_MAX;
    }
  }
}
*/

static uint64_t aud_settime(STREAM_XCODE_DATA_T *pData,
                            IXCODE_AUDIO_CTXT_T *pXcode,
                            STREAM_XCODE_AUD_UDATA_T *pUData,
                            unsigned int encodeOutIdx) {
  uint64_t pts;

  if(pXcode->cfgSampleRateOut == 0) {
    if((pXcode->cfgSampleRateOut = pXcode->cfgSampleRateIn) == 0) {
      return pData->curFrame.pkt.xtra.tm.pts;
    }
  }

  pts = ((uint64_t) pUData->outHz * 90000) / pXcode->cfgSampleRateOut;

  //fprintf(stderr, "aud_settime outHz is at %lluHz numFramesOut:%d, pts:%.3f, curFrame.pkt.pts:%.3f, useCompoundFramePts:%d, pXcode->pts:%.3f\n", pUData->outHz, pData->numFramesOut, PTSF(pts), PTSF(pData->curFrame.pkt.xtra.tm.pts), pData->curFrame.useCompoundFramePts, PTSF(pXcode->pts));
  //int64_t delta = pts - pData->curFrame.pkt.xtra.tm.pts; fprintf(stderr, "[%d] %.3f\n", pXcode->common.encodeOutIdx, PTSF(delta));

  //LOG(X_DEBUG("tid:0x%x audio adjust check pts:%.3f vs %.3f (threshold:%.3f)"), pthread_self(), PTSF(pts), PTSF(pData->curFrame.pkt.xtra.tm.pts), PTSF(XCODE_AUD_DRIFT_ADJUST_PTS));
  if(pData->numFramesOut > 1 &&
     (pts > pData->curFrame.pkt.xtra.tm.pts + XCODE_AUD_DRIFT_ADJUST_PTS ||
      pts + XCODE_AUD_DRIFT_ADJUST_PTS < pData->curFrame.pkt.xtra.tm.pts)) {

    LOG(X_WARNING("Adjusting audio output pts from %.3f to %.3f"),
              PTSF(pts), PTSF(pData->curFrame.pkt.xtra.tm.pts));
    pUData->outHz = PTSF(pData->curFrame.pkt.xtra.tm.pts) * pXcode->cfgSampleRateOut;
  }

  // Adjust the frame pts since the samples / frame of the input != output
  if(pData->curFrame.useCompoundFramePts) {
    //TODO: use this as incremental add to base offset in case of reset
    // Currently this is only used by vorbis output, where each output timestamp is arbitrary
    //pUData->outHz = pXcode->pts; // Assumed pts value here is actually in Hz (from xcode layer)
    //pUData->outHz = pData->curFrame.compoundFrames[0].pts;
    //pUData->outHz = pXcode->compoundLens[0].pts; // Assumed value here is actually in Hz (from xcode layer)
    pUData->outHz += (pXcode->compoundLens[0].pts - pData->curFrame.ptsLastCompoundFrame); 
    pData->curFrame.ptsLastCompoundFrame = pXcode->compoundLens[0].pts;
    //fprintf(stderr, "aud_settime outHz set to pts %.3f (%llu)/90000, %lluHz\n", PTSF(pXcode->pts), pXcode->pts, pUData->outHz);
  } else if(pData->numFramesOut++ > 0) {
    pUData->outHz += pXcode->encoderSamplesInFrame;
    //fprintf(stderr, "aud_settime outHz incremented by %dHz to %.3f\n",pXcode->encoderSamplesInFrame, PTSF(((uint64_t) pUData->outHz * 90000) / pXcode->cfgSampleRateOut));
  }

  //fprintf(stderr, "aud pts %.3f -> %.3f numCompoundFr:%d %lluHz (%d/%dHz)\n", PTSF(pData->curFrame.pkt.xtra.tm.pts),PTSF(90000 * pUData->outHz / pXcode->cfgSampleRateOut), pXcode->common.encodeOutIdx - encodeOutIdx, pUData->outHz,pXcode->cfgSampleRateOut, pXcode->encoderSamplesInFrame);

  pts = pData->curFrame.pkt.xtra.tm.pts = 
                        // pData->curFrame.tm.seqstarttm +
                        (90000 * pUData->outHz /  pXcode->cfgSampleRateOut);
                        //(((uint64_t) pUData->outHz * 90000) /  pXcode->cfgSampleRateOut);

  //fprintf(stderr, "aud_settime pts now %.3f (%llu)\n",  PTSF(pts), pts);

  if(pXcode->common.encodeOutIdx > encodeOutIdx) {
    if((pData->curFrame.numCompoundFrames = pXcode->common.encodeOutIdx - encodeOutIdx) > 
        COMPOUND_FRAMES_MAX) {
      LOG(X_WARNING("Compound audio frames limited from %d to %d"), 
                    pData->curFrame.numCompoundFrames, COMPOUND_FRAMES_MAX);
      pData->curFrame.numCompoundFrames = COMPOUND_FRAMES_MAX;
    }
  }

  //fprintf(stderr,"aud_settime returning pts:%.3f, outHz:%.3f, pkt.xtra.tm.pts:%.3f\n", PTSF(pts), PTSF(((uint64_t) pUData->outHz * 90000) / pXcode->cfgSampleRateOut), PTSF(pData->curFrame.pkt.xtra.tm.pts));

  return pts;
}

static int xcode_frame_aud_mask(STREAM_XCODE_DATA_T *pData,
                                unsigned char *bufIn, unsigned int lenIn,
                                unsigned char *bufOut, unsigned int lenOut) {
  int len;
  IXCODE_AUDIO_CTXT_T *pXcode = &pData->piXcode->aud;
  STREAM_XCODE_AUD_UDATA_T *pUData = (STREAM_XCODE_AUD_UDATA_T *)pXcode->pUserData;

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
  int vadPrior = -1;

  if(pXcode->pMixerParticipant && pUData->pPipLayout) {
    vadPrior = pXcode->vadLatest;
  }
#endif //(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
  

  len = xcode_frame_aud(&pData->piXcode->aud, bufIn, lenIn, bufOut, lenOut);

  //fprintf(stderr, "XCODE_FRAME_AUD tid:0x%x pip.active:%d, done with rc:%d in:%d, %.3f\n", pthread_self(), pData->piXcode->vid.pip.active, len, lenIn, PTSF(pData->curFrame.pkt.xtra.tm.pts));

  if(len < 0) {

    pData->consecErr++;

    //
    // Gracefully handle decoder errors for damaged input
    //
    if(len < IXCODE_RC_OK && pData->piXcode->aud.common.decodeInIdx > 2 &&
       pData->consecErr < 3) {
      LOG(X_WARNING("xcode_frame_aud masking error rc:%d (consec:%d)"),
                    len, pData->consecErr);
      len = 0;
    } else {
      LOG(X_ERROR("xcode_frame_aud error rc:%d (consec:%d)"), len, pData->consecErr);
    }

  } else if(pData->consecErr > 0) {
    pData->consecErr = 0;
  }

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
  //
  // Update the PIP layout manager with a new VAD value
  //
  if(pXcode->pMixerParticipant && pUData->pPipLayout && pXcode->vadLatest != vadPrior) {
    pip_layout_update(pUData->pPipLayout, PIP_UPDATE_VAD);
  }
#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  return len;
}


static enum STREAM_NET_ADVFR_RC xcode_aud_ac3(STREAM_XCODE_DATA_T *pData,
                                              IXCODE_AUDIO_CTXT_T *pXcode,
                                              STREAM_XCODE_AUD_UDATA_T *pUData) {
  int sz;
  int len;
  unsigned int idxInBuf = 0;
  unsigned char *pEncode = NULL;
  int endofbuf = 0;
  unsigned int lenFrame;
  unsigned int lenFrameCopied;
  unsigned int encodeOutIdx;
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;

  encodeOutIdx = pXcode->common.encodeOutIdx;
  pData->curFrame.numCompoundFrames = 0;

  do {

    if(pUData->bufIdx == 0) {
      if((sz = ac3_find_starthdr(&pData->curFrame.pData[idxInBuf], 
                                       pData->curFrame.lenData - idxInBuf)) < 0) {
        LOG(X_WARNING("Unable to find ac3 start header [%d]/%d (cur ac fr sz:%d)"),
               idxInBuf, pData->curFrame.lenData, pUData->u.ac3.descr.frameSize);

        pUData->lastFrameErr = 1;
        break;
      }
      idxInBuf += sz;
    }

    if(pUData->u.ac3.descr.frameSize == 0 || pUData->lastFrameErr) {
      if(ac3_parse_hdr(&pData->curFrame.pData[idxInBuf], 
                      pData->curFrame.lenData - idxInBuf,
                      &pUData->u.ac3.descr) < 0) {
        LOG(X_WARNING("Failed to parse ac3 header at pes offset[%d]"), idxInBuf);
        idxInBuf += 2;
        continue;
      }
      if(pUData->lastFrameErr) {
        //fprintf(stdout, "lastFrameErr cleared\n");
        pUData->lastFrameErr = 0;
      }

      pXcode->cfgChannelsIn = pUData->u.ac3.descr.numChannels;
      pXcode->cfgSampleRateIn = pUData->u.ac3.descr.sampleRateHz;

    }

    lenFrame = pUData->u.ac3.descr.frameSize - pUData->bufIdx;
    if(lenFrame > pData->curFrame.lenData - idxInBuf) {
      lenFrame = pData->curFrame.lenData - idxInBuf;
      endofbuf = 1;
    }
    lenFrameCopied = lenFrame;
    if(lenFrameCopied > (pUData->bufLen - pUData->bufIdx)) {
      LOG(X_WARNING("PES audio input frame buffer[%d/%d] length too small %d / %d"),
                     idxInBuf, pData->curFrame.lenData,
                     lenFrame, pUData->u.ac3.descr.frameSize);
      lenFrameCopied = pUData->bufLen - pUData->bufIdx;
    }
    memcpy(&pUData->pBuf[pUData->bufIdx], &pData->curFrame.pData[idxInBuf], lenFrameCopied);
    pUData->bufIdx += lenFrameCopied;
    if(endofbuf) {
      break;
    }
    pEncode = pUData->pBuf;
    idxInBuf += lenFrame;

    if(pEncode) {

      //
      // Initialize the audio transcoder context
      //
      if(!pXcode->common.is_init) {

        if(pXcode->cfgSampleRateOut == 0) {
          pXcode->cfgSampleRateOut = pXcode->cfgSampleRateIn;
        }
        if(init_aud(pData) != STREAM_NET_ADVFR_RC_OK) {
          return STREAM_NET_ADVFR_RC_ERROR;
        }
        pUData->outHz = 0;
      }

      if(pUData->idxEncodedBuf >= pUData->szEncodedBuf) {
        LOG(X_ERROR("xcode aud output[%d]/%d buffer too small."), 
             pUData->idxEncodedBuf, pUData->szEncodedBuf); 
        break;
      } else if(pEncode[0] != 0x0b && pEncode[1] != 0x77) {
        LOG(X_ERROR("Invalid AC3 start code to decode 0x%x 0x%x len:%d"),
                     pEncode[0], pEncode[1], pUData->u.ac3.descr.frameSize);
        pData->curFrame.lenData = 0;
        rc = STREAM_NET_ADVFR_RC_ERROR;
        break;
      } else if((len = xcode_frame_aud_mask(pData, 
                                pEncode, pUData->u.ac3.descr.frameSize, 
                                &pUData->encodedBuf[pUData->idxEncodedBuf], 
                                pUData->szEncodedBuf - pUData->idxEncodedBuf)) < 0) {
        pData->curFrame.lenData = 0;
        rc = STREAM_NET_ADVFR_RC_ERROR;
        break;
      } else if(len > 0) {
        //fprintf(stderr, "transcoded AC3 to idx[%d] len:%d\n", pUData->idxEncodedBuf, len);
        //avc_dumpHex(stderr, &pUData->encodedBuf[pUData->idxEncodedBuf], 16, 1);
        pUData->idxEncodedBuf += len;
      }
      pEncode = NULL;
      pUData->bufIdx = 0;
    }

  } while(idxInBuf < pData->curFrame.lenData);


  if(pUData->idxEncodedBuf == 0) {
    rc = STREAM_NET_ADVFR_RC_NOTAVAIL;      
    pData->curFrame.lenData = 0;
  } else {

    pUData->lastEncodedPts = aud_settime(pData, pXcode, pUData, encodeOutIdx);
    pData->curFrame.pData = pUData->encodedBuf;
    pData->curFrame.lenData = pUData->idxEncodedBuf;
    pUData->idxEncodedBuf = 0;

  }

 //fprintf(stderr, "ac3[%d]->[%d] ac3sz:%d rc:%d got len:%d, %d fr, %llu (%.3f)\n", pXcode->common.decodeOutIdx, pXcode->common.encodeOutIdx, pUData->u.ac3.descr.frameSize, rc, len, pData->curFrame.numCompoundFrames, pData->curFrame.pkt.xtra.tm.pts, (double)pData->curFrame.pkt.xtra.tm.pts/90000.0f);


  return rc;
}

static uint64_t PTS_PREV;
//static int PTS_IDX;
//static int tvcnt;
//static int _tsidx=-1;
//static int _tsdup;

static enum STREAM_NET_ADVFR_RC xcode_aud_mpa2(STREAM_XCODE_DATA_T *pData,
                                              IXCODE_AUDIO_CTXT_T *pXcode,
                                              STREAM_XCODE_AUD_UDATA_T *pUData) {

  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  MP3_DESCR_T mp3;
  uint64_t pts;
  int len;
  unsigned int encodeOutIdx;
  unsigned int szEncodedBuf = pUData->szEncodedBuf;

/*
int tsidx;
if(_tsidx==-1) (int)(_tsidx=pData->curFrame.pkt.xtra.tm.pts/90000);
tsidx=(int)(pData->curFrame.pkt.xtra.tm.pts/90000);
if(pData->curFrame.pkt.xtra.tm.pts==PTS_PREV) _tsdup++;
if(tsidx!=_tsidx) {
  fprintf(stderr, "\n\n\nAUDIO FRAMES IN SEC:%d dup:%d\n", tvcnt, _tsdup);
  _tsidx=tsidx;
  tvcnt=0;
  _tsdup=0;
}
tvcnt++;
*/

//pData->curFrame.pData[2] &= 0xf3;
//pData->curFrame.pData[2] |= 0x07;

  //
  // Initialize the audio transcoder context
  //
  if(!pXcode->common.is_init) {
    if(mp3_parse_hdr(pData->curFrame.pData, pData->curFrame.lenData, &mp3) < 0) {
      LOG(X_ERROR("Failed to parse mpeg audio header"));
      return STREAM_NET_ADVFR_RC_ERROR;
    }

    LOG(X_DEBUG("MPEG-Audio %s/%s, crc:%d, btrtidx:0x%x, %dHz, pad:%d, chmode:%d (%d)"), 
      mp3_version(mp3.version), mp3_layer(mp3.layer), mp3.hascrc, mp3.btrtidx, 
      mp3.sampleRateHz, mp3.haspadding, mp3.chmodeidx, mp3.numChannels);

  //if(!pXcode->common.is_init) {

    pXcode->cfgChannelsIn = mp3.numChannels;
    pXcode->cfgSampleRateIn = mp3.sampleRateHz;
    pXcode->cfgSampleRateIn = 24255;

    if(init_aud(pData) != STREAM_NET_ADVFR_RC_OK) {
      return STREAM_NET_ADVFR_RC_ERROR;
    }
  }


  //if(pData->curFrame.pkt.xtra.tm.pts==PTS_PREV) {
  //  szEncodedBuf = 0;
  //}


  //fprintf(stderr, "MPEG-AUD len:%d ts:%.3f [%d]\n", pData->curFrame.lenData, PTSF(pData->curFrame.pkt.xtra.tm.pts), PTS_IDX++);
  PTS_PREV=pData->curFrame.pkt.xtra.tm.pts;
  //avc_dumpHex(stderr, pData->curFrame.pData, 32, 1);

  encodeOutIdx = pXcode->common.encodeOutIdx;
  pData->curFrame.numCompoundFrames = 0;

  if((len = xcode_frame_aud_mask(pData, 
                                 pData->curFrame.pData,
                                 pData->curFrame.lenData,
                                 pUData->encodedBuf,
                                 szEncodedBuf)) < 0) {

    pData->curFrame.lenData = 0;
    rc = STREAM_NET_ADVFR_RC_ERROR;

  } else if(len == 0) {

    pData->curFrame.lenData = 0;
    rc = STREAM_NET_ADVFR_RC_NOTAVAIL;      

  } else {

    //pts = pData->curFrame.pkt.xtra.tm.pts;

    pts = aud_settime(pData, pXcode, pUData, encodeOutIdx);
    pData->curFrame.lenData = len;
    pData->curFrame.pData = pUData->encodedBuf;

    pData->curFrame.pkt.xtra.tm.pts = pts; 
  }

  //fprintf(stderr, "mp3 xcoded numc:%d len:%d pts:%.3f (in:%.3f) %d smp/Fr szbuf:%d\n", pData->curFrame.numCompoundFrames, len, PTSF(pts), PTSF(PTS_PREV), pXcode->encoderSamplesInFrame, szEncodedBuf);

  return rc;
}

static enum STREAM_NET_ADVFR_RC xcode_aud_vorbis(STREAM_XCODE_DATA_T *pData,
                                                IXCODE_AUDIO_CTXT_T *pXcode,
                                                STREAM_XCODE_AUD_UDATA_T *pUData) {

  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  VORBIS_DESCR_T vorbis;
  uint64_t pts;
  int len;
  unsigned int encodeOutIdx;
  unsigned int szEncodedBuf = pUData->szEncodedBuf;

  //
  // Initialize the audio transcoder context
  //
  if(!pXcode->common.is_init) {

    if(vorbis_parse_hdr(pUData->u.vorbis.phdr, pUData->u.vorbis.hdrLen, &vorbis) < 0) {
      LOG(X_ERROR("Failed to parse vorbis audio header length %d"), pUData->u.vorbis.hdrLen);
      return STREAM_NET_ADVFR_RC_ERROR;
    }

    pXcode->cfgChannelsIn = vorbis.channels;
    pXcode->cfgSampleRateIn = vorbis.clockHz;
    pXcode->pextradata = pUData->u.vorbis.phdr;
    pXcode->extradatasize = pUData->u.vorbis.hdrLen;

    if(pXcode->cfgSampleRateOut == 0) {
      pXcode->cfgSampleRateOut = pXcode->cfgSampleRateIn;
    }
    if(pXcode->cfgChannelsOut == 0) {
      pXcode->cfgChannelsOut = pXcode->cfgChannelsIn;
    }
 
    //LOG(X_DEBUG("Vorbis %dHz, %d channels, frame size %d"), vorbis.clockHz, vorbis.channels, vorbis.framesize);

    if(init_aud(pData) != STREAM_NET_ADVFR_RC_OK) {
      return STREAM_NET_ADVFR_RC_ERROR;
    }
  }

  encodeOutIdx = pXcode->common.encodeOutIdx;
  pData->curFrame.numCompoundFrames = 0;

  if((len = xcode_frame_aud_mask(pData, 
                                 pData->curFrame.pData,
                                 pData->curFrame.lenData,
                                 pUData->encodedBuf,
                                 szEncodedBuf)) < 0) {

    pData->curFrame.lenData = 0;
    rc = STREAM_NET_ADVFR_RC_ERROR;

  } else if(len == 0) {

    pData->curFrame.lenData = 0;
    rc = STREAM_NET_ADVFR_RC_NOTAVAIL;      

  } else {

    //pts = pData->curFrame.pkt.xtra.tm.pts;

    pts = aud_settime(pData, pXcode, pUData, encodeOutIdx);
    pData->curFrame.lenData = len;
    pData->curFrame.pData = pUData->encodedBuf;

    pData->curFrame.pkt.xtra.tm.pts = pts; 
  }

  //fprintf(stderr, "vorbis xcoded numc:%d len:%d pts:%.3f (in:%.3f) %d smp/Fr szbuf:%d\n", pData->curFrame.numCompoundFrames, len, PTSF(pts), PTSF(PTS_PREV), pXcode->encoderSamplesInFrame, szEncodedBuf);

  return rc;
}

static enum STREAM_NET_ADVFR_RC xcode_aud_silk(STREAM_XCODE_DATA_T *pData,
                                               IXCODE_AUDIO_CTXT_T *pXcode,
                                               STREAM_XCODE_AUD_UDATA_T *pUData) {


  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  //VORBIS_DESCR_T vorbis;
  //uint64_t pts;
  int len;
  unsigned int encodeOutIdx;
  unsigned int szEncodedBuf = pUData->szEncodedBuf;

  //
  // Initialize the audio transcoder context
  //
  if(!pXcode->common.is_init) {

    //if(vorbis_parse_hdr(pUData->u.vorbis.phdr, pUData->u.vorbis.hdrLen, &vorbis) < 0) {
    //  LOG(X_ERROR("Failed to parse vorbis audio header length %d"), pUData->u.vorbis.hdrLen);
    //  return STREAM_NET_ADVFR_RC_ERROR;
    //}

    //pXcode->cfgDecoderSamplesInFrame = pUData->u.aac.descr.sp.frameDeltaHz;

    //
    // Set decoder input channels
    //
    if(pXcode->cfgChannelsIn == 0) {
      pXcode->cfgChannelsIn = 1;
    }
//LOG(X_DEBUG("OPUS. cfgSampleRateIn:%d"), pXcode->cfgSampleRateIn); pXcode->cfgSampleRateIn = 48000;
    if(pXcode->cfgSampleRateIn == 0) {
      pXcode->cfgSampleRateIn = 16000; 
    }
    if(pXcode->cfgSampleRateOut == 0) {
      pXcode->cfgSampleRateOut = pXcode->cfgSampleRateIn;
    }
    if(pXcode->cfgChannelsOut == 0) {
      pXcode->cfgChannelsOut = pXcode->cfgChannelsIn;
    }
 
    //LOG(X_DEBUG("SILK %dHz, %d channels, frame size %d"), vorbis.clockHz, vorbis.channels, vorbis.framesize);

    if(init_aud(pData) != STREAM_NET_ADVFR_RC_OK) {
      return STREAM_NET_ADVFR_RC_ERROR;
    }
    pUData->outHz = 0;
  }

  encodeOutIdx = pXcode->common.encodeOutIdx;
  pData->curFrame.numCompoundFrames = 0;

  //fprintf(stderr, "XCODE AUD len:%d data:0x%x\n", pData->curFrame.lenData, pData->curFrame.pData[0]);
  //
  // Check special flag indicating the frame was actually lost and artificially reconstructed
  //
  if((pData->curFrame.lenData == SILK_LOST_FRAME_LEN && pData->curFrame.pData[0] == SILK_LOST_FRAME_DATA) ||
     (pData->curFrame.lenData == OPUS_LOST_FRAME_LEN && pData->curFrame.pData[0] == OPUS_LOST_FRAME_DATA)) {
    pData->curFrame.lenData = 0;
  }

  if((len = xcode_frame_aud_mask(pData, 
                                 pData->curFrame.pData,
                                 pData->curFrame.lenData,
                                 pUData->encodedBuf,
                                 szEncodedBuf)) < 0) {

    pData->curFrame.lenData = 0;
    rc = STREAM_NET_ADVFR_RC_ERROR;

  } else if(len == 0) {

    pData->curFrame.lenData = 0;
    rc = STREAM_NET_ADVFR_RC_NOTAVAIL;      

  } else {

    aud_settime(pData, pXcode, pUData, encodeOutIdx);
    pData->curFrame.lenData = len;
    pData->curFrame.pData = pUData->encodedBuf;

/*
    //pts = pData->curFrame.pkt.xtra.tm.pts;
    pts = aud_settime(pData, pXcode, pUData, encodeOutIdx);
    pData->curFrame.lenData = len;
    pData->curFrame.pData = pUData->encodedBuf;
    pData->curFrame.pkt.xtra.tm.pts = pts; 
*/
  }

  //fprintf(stderr, "silk xcoded numc:%d len:%d (in:%.3f) %.3f, %d smp/Fr szbuf:%d\n", pData->curFrame.numCompoundFrames, len, PTSF(PTS_PREV), PTSF(pData->curFrame.pkt.xtra.tm.pts), pXcode->encoderSamplesInFrame, szEncodedBuf);

  return rc;
}

static enum STREAM_NET_ADVFR_RC xcode_aud_aac(STREAM_XCODE_DATA_T *pData,
                                              IXCODE_AUDIO_CTXT_T *pXcode,
                                              STREAM_XCODE_AUD_UDATA_T *pUData) {

  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  int len;
  unsigned int encodeOutIdx;

  //
  // The aac esds header may have already been init
  // since any header extension may not be included in ADTS headers
  //
  if(!pXcode->common.is_init) {

    //
    // Decode the ADTS header to get the channels and frequency
    // 
    if(!pUData->u.aac.descr.sp.init) {
      if(aac_decodeAdtsHdr(pData->curFrame.pData, pData->curFrame.lenData, 
                         &pUData->u.aac.descr) != 0) {
        //pUData->u.aac.descr.lenhdr = 0;
        return STREAM_NET_ADVFR_RC_ERROR;
      }
    }
    pXcode->cfgDecoderSamplesInFrame = pUData->u.aac.descr.sp.frameDeltaHz;
    pXcode->cfgChannelsIn = pUData->u.aac.descr.sp.channelIdx;
    pXcode->cfgSampleRateIn = esds_getFrequencyVal(ESDS_FREQ_IDX(pUData->u.aac.descr.sp));
//fprintf(stderr, "SET CFG SAMPLE RATE IN:%d, lenData:%d\n", pXcode->cfgSampleRateIn, pData->curFrame.lenData);
    if(pXcode->cfgSampleRateOut == 0) {
      pXcode->cfgSampleRateOut = pXcode->cfgSampleRateIn;
    }
    if(pXcode->cfgChannelsOut == 0) {
      pXcode->cfgChannelsOut = pXcode->cfgChannelsIn;
    }
  }
  //fprintf(stderr, "CHANNELS:%d->%d %dHz->%dHz force:%d\n", pXcode->cfgChannelsIn, pXcode->cfgChannelsOut, pXcode->cfgSampleRateIn, pXcode->cfgSampleRateOut, pXcode->cfgForceOut); 
  //
  // Avoid going through the transcoder if output format is the same
  //
  if(pXcode->common.cfgFileTypeIn == pXcode->common.cfgFileTypeOut &&
    !pXcode->cfgForceOut && 
     pXcode->cfgChannelsIn == pXcode->cfgChannelsOut &&
     pXcode->cfgSampleRateIn == pXcode->cfgSampleRateOut) {
    if(!pXcode->common.is_init) {
      LOG(X_WARNING("Skipping audio transcoding because input and output is same: %s channels:%d %dHz"), 
          codecType_getCodecDescrStr(pXcode->common.cfgFileTypeIn), pXcode->cfgChannelsIn, 
          pXcode->cfgSampleRateIn);
    }
    pXcode->common.is_init = 1;
    return STREAM_NET_ADVFR_RC_OK;
  }

  //
  // Initialize the audio transcoder context
  //
  if(!pXcode->common.is_init && init_aud(pData) != STREAM_NET_ADVFR_RC_OK) {
    return STREAM_NET_ADVFR_RC_ERROR;
  }

  encodeOutIdx = pXcode->common.encodeOutIdx;
  pData->curFrame.numCompoundFrames = 0;

  //fprintf(stderr, "AUD TMPFRAME.ALLOCSZ:%d, LEN:%d, pBuf:0x%x ... curFrame.pData:0x%x, curFrame.lenData:%d\n", pUData->tmpFrame.allocSz, pUData->tmpFrame.len, pUData->tmpFrame.pBuf, pData->curFrame.pData, pData->curFrame.lenData);

  if((len = xcode_frame_aud_mask(pData, 
                                 pData->curFrame.pData,
                                 pData->curFrame.lenData,
                                 pUData->encodedBuf,
                                 pUData->szEncodedBuf)) < 0) {

    pData->curFrame.lenData = 0;
    rc = STREAM_NET_ADVFR_RC_ERROR;

  } else if(len == 0) {

    pData->curFrame.lenData = 0;
    rc = STREAM_NET_ADVFR_RC_NOTAVAIL;      

  } else {

    aud_settime(pData, pXcode, pUData, encodeOutIdx);
    pData->curFrame.lenData = len;
    pData->curFrame.pData = pUData->encodedBuf;

  }

  //fprintf(stderr, "aac[%d] rc:%d got len:%d, %d fr, %.3f\n", pXcode->common.encodeOutIdx, rc, len, pData->curFrame.numCompoundFrames, (double)pData->curFrame.pkt.xtra.tm.pts/90000.0f);

  return rc;
}

static enum STREAM_NET_ADVFR_RC xcode_aud_amr(STREAM_XCODE_DATA_T *pData,
                                              IXCODE_AUDIO_CTXT_T *pXcode,
                                              STREAM_XCODE_AUD_UDATA_T *pUData) {

  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  int len;
  unsigned int encodeOutIdx;

  if(!pXcode->common.is_init) {
    pXcode->cfgChannelsIn = 1;
    pXcode->cfgSampleRateIn = AMRNB_CLOCKHZ;
    pXcode->cfgDecoderSamplesInFrame = AMRNB_SAMPLE_DURATION_HZ;

    if(pXcode->cfgSampleRateOut == 0) {
      pXcode->cfgSampleRateOut = pXcode->cfgSampleRateIn;
    }
    if(pXcode->cfgChannelsOut == 0) {
      pXcode->cfgChannelsOut = pXcode->cfgChannelsIn;
    }

    //
    // Initialize the audio transcoder context
    //
    if(init_aud(pData) != STREAM_NET_ADVFR_RC_OK) {
      return STREAM_NET_ADVFR_RC_ERROR;
    }
    pUData->outHz = 0;
  }

  encodeOutIdx = pXcode->common.encodeOutIdx;
  pData->curFrame.numCompoundFrames = 0;

  if((len = xcode_frame_aud_mask(pData,
                                 pData->curFrame.pData,
                                 pData->curFrame.lenData,
                                 pUData->encodedBuf,
                                 pUData->szEncodedBuf)) < 0) {

    pData->curFrame.lenData = 0;
    rc = STREAM_NET_ADVFR_RC_ERROR;

  } else if(len == 0) {

    pData->curFrame.lenData = 0;
    rc = STREAM_NET_ADVFR_RC_NOTAVAIL;

  } else {

    aud_settime(pData, pXcode, pUData, encodeOutIdx);
    pData->curFrame.lenData = len;
    pData->curFrame.pData = pUData->encodedBuf;

  }

 //fprintf(stderr, "amr[%d] rc:%d got len:%d, %d fr, %.3f\n", pXcode->common.encodeOutIdx, rc, len, pData->curFrame.numCompoundFrames, (double)pData->curFrame.pkt.xtra.tm.pts/90000.0f);

  return rc;
}

static enum STREAM_NET_ADVFR_RC xcode_aud_raw(STREAM_XCODE_DATA_T *pData,
                                              IXCODE_AUDIO_CTXT_T *pXcode,
                                              STREAM_XCODE_AUD_UDATA_T *pUData) {

  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  int len;
  unsigned int encodeOutIdx;

  if(!pXcode->common.is_init) {

    //pXcode->cfgDecoderSamplesInFrame = 1;

    if(pXcode->cfgSampleRateIn == 0) {
      LOG(X_ERROR("Raw audio input sampling rate not given"));
      return STREAM_NET_ADVFR_RC_ERROR;
    } else if(pXcode->cfgChannelsIn == 0) {
      LOG(X_ERROR("Raw audio input channels not given"));
      return STREAM_NET_ADVFR_RC_ERROR;
    }

    if(pXcode->cfgChannelsOut == 0) {
      pXcode->cfgChannelsOut = pXcode->cfgChannelsIn;
    }
    if(pXcode->cfgSampleRateOut == 0) {
      pXcode->cfgSampleRateOut = pXcode->cfgSampleRateIn;
    }

    //
    // Initialize the audio transcoder context
    //
    if(init_aud(pData) != STREAM_NET_ADVFR_RC_OK) {
      return STREAM_NET_ADVFR_RC_ERROR;
    }
    pUData->outHz = 0;
  }

  encodeOutIdx = pXcode->common.encodeOutIdx;
  pData->curFrame.numCompoundFrames = 0;

  if((len = xcode_frame_aud_mask(pData,
                                 pData->curFrame.pData,
                                 pData->curFrame.lenData,
                                 pUData->encodedBuf,
                                 pUData->szEncodedBuf)) < 0) {

    pData->curFrame.lenData = 0;
    rc = STREAM_NET_ADVFR_RC_ERROR;

  } else if(len == 0) {

    pData->curFrame.lenData = 0;
    rc = STREAM_NET_ADVFR_RC_NOTAVAIL;

  } else {

    aud_settime(pData, pXcode, pUData, encodeOutIdx);
    pData->curFrame.lenData = len;
    pData->curFrame.pData = pUData->encodedBuf;

  }

  //fprintf(stderr, "raw aud[%d] rc:%d got len:%d, %d fr, %.3f\n", pXcode->common.encodeOutIdx, rc, len, pData->curFrame.numCompoundFrames, PTSF(pData->curFrame.pkt.xtra.tm.pts));

  return rc;
}

static int find_aac_frame_offsets(STREAM_PES_FRAME_T *pFr) {
  int rc = 0;
  unsigned int idx;
  AAC_ADTS_FRAME_T adts;
  unsigned int offset = 0;
  unsigned char *pData = pFr->pData;
  unsigned int len = pFr->lenData;
  unsigned int lenprev = len;

  for(idx = 0; idx < pFr->numCompoundFrames; idx++) {

    if(idx >= COMPOUND_FRAMES_MAX) {
      LOG(X_WARNING("AAC compound frames %d exceeds max %d"),
                    pFr->numCompoundFrames, COMPOUND_FRAMES_MAX);
      rc = -1;
      break;
    }
    if(idx > 0) {
      pFr->compoundFrames[idx - 1].len = lenprev;
    }
    pFr->compoundFrames[idx].byteOffset = offset;

    if((rc = aac_decodeAdtsHdr(&pData[offset], len - offset, &adts)) < 0) {
      pFr->numCompoundFrames = idx;
      break;
    } 

    offset += adts.lentot;
    lenprev = adts.lentot;

    if(offset > len) {
      LOG(X_WARNING("Encoded AAC ADTS compound frame[%d/%d] offset:%d/tot:%d seems invalid"), 
           idx, pFr->numCompoundFrames, offset, len);
      pFr->numCompoundFrames = idx;
      rc = -1;
      break;
    }
  }

  if(rc == 0 && idx > 0) {
    pFr->compoundFrames[idx - 1].len = lenprev;
  }
  if(pFr->numCompoundFrames > 1) {
    pFr->lenData = pFr->compoundFrames[0].len;
  }

  //for(idx = 0; idx< pFr->numCompoundFrames; idx++) fprintf(stderr, "aac[%d/%d] at[%d] len:%d \n", idx, pFr->numCompoundFrames, pFr->compoundFrames[idx].byteOffset, pFr->compoundFrames[idx].len);

  return rc;
}

static int find_generic_frame_offsets(const IXCODE_AUDIO_CTXT_T *pXcode, STREAM_PES_FRAME_T *pFr) {
  int rc = 0;
  unsigned int idx;
  unsigned int offset = 0;

  for(idx = 0; idx < pFr->numCompoundFrames; idx++) {
    if(pXcode->compoundLens[idx].length > 0) {
      pFr->compoundFrames[idx].byteOffset = offset;
      pFr->compoundFrames[idx].len = pXcode->compoundLens[idx].length;
      pFr->compoundFrames[idx].pts = pXcode->compoundLens[idx].pts;

      offset += pXcode->compoundLens[idx].length;

    } else {
      break;
    }
  }

  if(pFr->numCompoundFrames > 1) {
    pFr->lenData = pFr->compoundFrames[0].len;
  }

  return rc;
}

static int find_amr_frame_offsets(STREAM_PES_FRAME_T *pFr) {
  int rc = 0;
  unsigned int idx;
  unsigned int offset = 0;
  unsigned int len;

  len = AMRNB_GETFRAMEBODYSIZE(pFr->pData[0]) + 1;

  for(idx = 0; idx < pFr->numCompoundFrames; idx++) {

    if(idx >= COMPOUND_FRAMES_MAX) {
      LOG(X_WARNING("AMR compound frames %d exceeds max %d"),
                    pFr->numCompoundFrames, COMPOUND_FRAMES_MAX);
      rc = -1;
      break;
    }
    pFr->compoundFrames[idx].len = len;
    pFr->compoundFrames[idx].byteOffset = offset;
    offset += len;

    if(offset > pFr->lenData) {
      LOG(X_WARNING("Encoded AMR compound frame[%d/%d] offset:%d/tot:%d seems invalid"), 
           idx, pFr->numCompoundFrames, offset, pFr->lenData);
      pFr->numCompoundFrames = idx;
      rc = -1;
      break;
    }
  }

  if(pFr->numCompoundFrames > 1) {
    pFr->lenData = pFr->compoundFrames[0].len;
  }

  return rc;
}

static unsigned int delmetotEncBytes;

enum STREAM_NET_ADVFR_RC xcode_aud(STREAM_XCODE_DATA_T *pData) {

  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  IXCODE_AUDIO_CTXT_T *pXcode = &pData->piXcode->aud; 
  //IXCODE_VIDEO_CTXT_T *pXcodeV = &pData->piXcode->vid; 
  STREAM_XCODE_AUD_UDATA_T *pUData = (STREAM_XCODE_AUD_UDATA_T *)pXcode->pUserData;
  //unsigned int idx;

  VSX_DEBUGLOG("xcode_aud: decode[%d,%d] encode[%d,%d] xcoder: %3f %.3f "
  //fprintf(stderr, "xcode_aud: decode[%d,%d] encode[%d,%d] xcoder: %3f %.3f "
                "(pts:%.3f,dts:%.3f,%.3f, compfr[%d/%d], len:%d)\n", 
    pXcode->common.decodeInIdx, pXcode->common.decodeOutIdx, 
    pXcode->common.encodeInIdx, pXcode->common.encodeOutIdx, 
    PTSF(pXcode->common.inpts90Khz), PTSF(pXcode->common.outpts90Khz), 
    PTSF(pData->curFrame.pkt.xtra.tm.pts), PTSF(pData->curFrame.pkt.xtra.tm.dts),
    PTSF((int64_t)pData->curFrame.pkt.xtra.tm.pts-pData->curFrame.dbgprevpts),
    pData->curFrame.idxCompoundFrame, pData->curFrame.numCompoundFrames, pData->curFrame.lenData); 
    pData->curFrame.dbgprevpts = pData->curFrame.pkt.xtra.tm.pts;
    //avc_dumpHex(stderr, pData->curFrame.pData, pData->curFrame.lenData, 1);

    //fprintf(stderr, "xcode_aud tid:0x%x, vid.pip.active:%d, overlay.havePip:%d\n", pthread_self(), pData->piXcode->vid.pip.active, pData->piXcode->vid.overlay.havePip);

  //
  // If video xcode is enabled, audio will have haveSeqStart set by video component
  // 
  if(!pXcode->common.cfgDo_xcode || (pData->pComplement->piXcode && 
       pData->pComplement->piXcode->vid.common.cfgDo_xcode && 
       !pData->piXcode->vid.pip.active &&
     (!pData->piXcode->vid.common.is_init || !pUData->haveSeqStart))) {

    //
    // If this is a PIP audio thread, we don't want to wait until video input sequence start
    // because video output (from main overlay shared encoded frames) may start prior 
    //

    pData->curFrame.lenData = 0;
    return STREAM_NET_ADVFR_RC_NOTAVAIL;


  } else if(pData->piXcode->vid.pip.active) {

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

    if(!pUData->pStartMixer && !pXcode->pMixerParticipant) {
      //fprintf(stderr, "AUDIO NOT ENC OUT\n");
      pData->curFrame.lenData = 0;
      return STREAM_NET_ADVFR_RC_NOTENCODED;
    }

#else // XCODE_HAVE_PIP_AUDIO

    pData->curFrame.lenData = 0;
    return STREAM_NET_ADVFR_RC_NOTENCODED;

#endif // XCODE_HAVE_PIP_AUDIO

  }

  //fprintf(stderr,"OUTHZ:%lluHz %.3f, pkt.xtra.tm.pts:%.3f, \n", pUData->outHz, PTSF(((uint64_t) pUData->outHz * 90000) / pXcode->cfgSampleRateOut), PTSF(pData->curFrame.pkt.xtra.tm.pts));

  pXcode->pts = pData->curFrame.pkt.xtra.tm.pts;
  //fprintf(stderr, "xcode_aud xcode: 0x%x, pts:%llu\n", pXcode, pXcode->pts);

  //fprintf(stderr, "xcode_aud cfgFileTypeIn:0x%x compound fr[%d/%d] len:%d\n", pXcode->common.cfgFileTypeIn,pData->curFrame.idxCompoundFrame, pData->curFrame.numCompoundFrames, pData->curFrame.lenData);


  //
  // Check if the prior encoder output contained multiple frames
  // and pull the next frame from the pre-computed offset
  //
  if(pData->curFrame.idxCompoundFrame < pData->curFrame.numCompoundFrames) {

    pData->curFrame.pData = &pUData->encodedBuf[
      pData->curFrame.compoundFrames[pData->curFrame.idxCompoundFrame].byteOffset];
    pData->curFrame.lenData = 
      pData->curFrame.compoundFrames[pData->curFrame.idxCompoundFrame].len;

    if(pData->curFrame.useCompoundFramePts) {
      //pUData->outHz = pData->curFrame.compoundFrames[pData->curFrame.idxCompoundFrame].pts;
      pUData->outHz += (pData->curFrame.compoundFrames[pData->curFrame.idxCompoundFrame].pts - 
                        pData->curFrame.ptsLastCompoundFrame); 
      pData->curFrame.ptsLastCompoundFrame = pData->curFrame.compoundFrames[pData->curFrame.idxCompoundFrame].pts;
    } else {
      pUData->outHz += pXcode->encoderSamplesInFrame;
      //fprintf(stderr, "outHz now %llu after incr by:%d\n", pUData->outHz, pXcode->encoderSamplesInFrame);
    }
    pData->curFrame.pkt.xtra.tm.pts = 
                        // pData->curFrame.tm.seqstarttm + 
                         (90000 * pUData->outHz / pXcode->cfgSampleRateOut);

    //LOG(X_DEBUG("xcode_aud (from prev) %d/ %d pts:%.3f, len:%d, (0x%x)"), pData->curFrame.idxCompoundFrame, pData->curFrame.numCompoundFrames, PTSF(pData->curFrame.pkt.xtra.tm.pts), pData->curFrame.lenData, pData->curFrame.pData);
  //LOG(X_DEBUG("xcode_aud[%d] (from prev) tid:0x%x done dec:[%d] xcoder:%3f %.3f (pts:%.3f) len:%d rc:%d numcompound:[%d]/%d bytes:%u, tm:%lu, pip.active:%d, havePip:%d, codec:%d"), pXcode->common.encodeOutIdx, pthread_self(), pXcode->common.decodeOutIdx, PTSF(pXcode->common.inpts90Khz), PTSF(pXcode->common.outpts90Khz), PTSF(pData->curFrame.pkt.xtra.tm.pts), pData->curFrame.lenData, rc, pData->curFrame.idxCompoundFrame, pData->curFrame.numCompoundFrames, delmetotEncBytes, (long)(timer_GetTime()/TIME_VAL_MS), pData->piXcode->vid.pip.active, pData->piXcode->vid.overlay.havePip, pXcode->common.cfgFileTypeOut);

    pData->curFrame.idxCompoundFrame++;
    pData->numFramesOut++;

    if(pData->curFrame.pkt.xtra.tm.pts < pData->curFrame.tm.seqstarttm && !pData->piXcode->vid.pip.active) {
      LOG(X_DEBUG("Dropping compound audio fr before seqstart pts:%.3f seqstart:%.3f"), 
            PTSF(pData->curFrame.pkt.xtra.tm.pts), PTSF(pData->curFrame.tm.seqstarttm));
      pData->curFrame.lenData = 0;
      rc = STREAM_NET_ADVFR_RC_NOTAVAIL;
    }
   //fprintf(stderr, "RETURNING %d\n", rc);
    return rc;
  }

  //AAC_ADTS_FRAME_T aacF; aac_decodeAdtsHdr(pData->curFrame.pData, pData->curFrame.lenData, &aacF); LOG(X_DEBUG("aac frame len:%d, input len:%d"), aacF.lentot, pData->curFrame.lenData);

  switch(pXcode->common.cfgFileTypeIn) {
    case XC_CODEC_TYPE_AC3:
      rc = xcode_aud_ac3(pData, pXcode, pUData);
      break;
    case XC_CODEC_TYPE_MPEGA_L2:
    case XC_CODEC_TYPE_MPEGA_L3:
      rc = xcode_aud_mpa2(pData, pXcode, pUData);
      break;
    case XC_CODEC_TYPE_AAC:

      rc = xcode_aud_aac(pData, pXcode, pUData);
      break;
    case XC_CODEC_TYPE_AMRNB:
      rc = xcode_aud_amr(pData, pXcode, pUData);
      break;
    case XC_CODEC_TYPE_VORBIS:
      rc = xcode_aud_vorbis(pData, pXcode, pUData);
      break;
    case XC_CODEC_TYPE_SILK:
    case XC_CODEC_TYPE_OPUS:
      rc = xcode_aud_silk(pData, pXcode, pUData);
      break;
    case XC_CODEC_TYPE_G711_MULAW:
    case XC_CODEC_TYPE_G711_ALAW:

      if(pXcode->cfgSampleRateIn == 0) {
        pXcode->cfgChannelsIn = 1;
        pXcode->cfgSampleRateIn = 8000;
      }

    case XC_CODEC_TYPE_RAWA_PCM16LE:
    case XC_CODEC_TYPE_RAWA_PCMULAW:
    case XC_CODEC_TYPE_RAWA_PCMALAW:
    case XC_CODEC_TYPE_AUD_CONFERENCE:
      rc = xcode_aud_raw(pData, pXcode, pUData);
      break;
    default:
      LOG(X_ERROR("Unsupported input audio codec to transcode: %s (%d)"), 
           codecType_getCodecDescrStr(pXcode->common.cfgFileTypeIn), pXcode->common.cfgFileTypeIn);
      return STREAM_NET_ADVFR_RC_ERROR;
  }

  //len = AMRNB_GETFRAMEBODYSIZE(pData->curFrame.pData[0]) + 1;
  //fprintf(stderr, "xcoded audio done len:%d rc:%d numComp:%d (%dHz)\n", pData->curFrame.lenData, rc, pData->curFrame.numCompoundFrames, pXcode->encoderSamplesInFrame);

  switch(pXcode->common.cfgFileTypeOut) {
    case XC_CODEC_TYPE_VORBIS:
      //pData->curFrame.idxCompoundFrame = pData->curFrame.numCompoundFrames = 0;
      //pUData->outHz = pXcode->pts;
      pData->curFrame.useCompoundFramePts = 1;
      //for(idx = 0; idx < MIN(sizeof(pXcode->compoundLens) / sizeof(pXcode->compoundLens[0]), COMPOUND_FRAMES_MAX); idx++) {
      //  if(pXcode->compoundLens[idx].length > 0) {
//fprintf(stderr, "COMPOUND[%d] len:%d, pts:%lldHz (%lldHz)\n", idx, pXcode->compoundLens[idx].length, pXcode->compoundLens[idx].pts, pXcode->pts);
      //    pData->curFrame.numCompoundFrames++;
      //  } else {
      //    break;
      //  }
      //}
  //fprintf(stderr, "VORBIS OUT len:%d %lldHz (%.3f)\n", pData->curFrame.lenData, pUData->outHz, (float)pUData->outHz/pXcode->cfgSampleRateOut);
      break;

    default:
      break;
  }

  //if(pData->curFrame.lenData > 0) {
  //aud_settime(pData, pXcode, pUData, encodeOutIdx);
  //}



  //
  // Check if the transcoded output data consists of mutiple frames
  // 
  if(pData->curFrame.numCompoundFrames == 1) {
    pData->curFrame.idxCompoundFrame = 1;
  } else if(pData->curFrame.numCompoundFrames > 1) {
    //fprintf(stderr, "NUMCOMPOUNDFRAMES:%d\n", pData->curFrame.numCompoundFrames);

    switch(pXcode->common.cfgFileTypeOut) {

      case XC_CODEC_TYPE_AAC:
        find_aac_frame_offsets(&pData->curFrame);
        pData->curFrame.idxCompoundFrame = 1;
        break;

      case XC_CODEC_TYPE_AC3:
        //fprintf(stderr, "ac3 %lluHz outHz incr by %u\n", pUData->outHz, pXcode->encoderSamplesInFrame * (pData->curFrame.numCompoundFrames - 1));
        pUData->outHz += (pXcode->encoderSamplesInFrame * (pData->curFrame.numCompoundFrames - 1));
        pData->curFrame.numCompoundFrames = 0;
        break;

      case XC_CODEC_TYPE_AMRNB:
        find_amr_frame_offsets(&pData->curFrame);
        pData->curFrame.idxCompoundFrame = 1;
        break;

      case XC_CODEC_TYPE_VORBIS:
        find_generic_frame_offsets(pXcode, &pData->curFrame);
        pData->curFrame.idxCompoundFrame = 1;
        break;

      case XC_CODEC_TYPE_SILK:
        find_generic_frame_offsets(pXcode, &pData->curFrame);
        pData->curFrame.idxCompoundFrame = 1;

        //fprintf(stderr, "out silk[%d] rc:%d got len:%d, %d fr, %.3f\n", pXcode->common.encodeOutIdx, rc, pData->curFrame.lenData, pData->curFrame.numCompoundFrames, (double)pData->curFrame.pkt.xtra.tm.pts/90000.0f);

        break;

      case XC_CODEC_TYPE_OPUS:
        find_generic_frame_offsets(pXcode, &pData->curFrame);
        pData->curFrame.idxCompoundFrame = 1;

        break;

      case XC_CODEC_TYPE_G711_MULAW:
      case XC_CODEC_TYPE_G711_ALAW:
        pUData->outHz += (pXcode->encoderSamplesInFrame * (pData->curFrame.numCompoundFrames - 1));
//fprintf(stderr, "LEN:%d at %uHz\n", pData->curFrame.lenData, pUData->outHz);
        pData->curFrame.numCompoundFrames = 0;
        break;

      default:
        pData->curFrame.numCompoundFrames = 0;
        break;
    }
    
  }

  delmetotEncBytes+=pData->curFrame.lenData;

  if(rc >= 0 && pData->curFrame.pkt.xtra.tm.pts < pData->curFrame.tm.seqstarttm && !pData->piXcode->vid.pip.active) {
    LOG(X_DEBUG("Dropping audio fr before seqstart pts:%.3f seqstart:%.3f, numcomp:%d"), 
          PTSF(pData->curFrame.pkt.xtra.tm.pts), PTSF(pData->curFrame.tm.seqstarttm),
          pData->curFrame.numCompoundFrames);
    pData->curFrame.lenData = 0;
    rc = STREAM_NET_ADVFR_RC_NOTAVAIL;
  }

  //LOG(X_DEBUG("xcode_aud[%d] tid:0x%x done dec:[%d] xcoder:%3f %.3f (pts:%.3f) len:%d rc:%d numcompound:[%d]/%d bytes:%u, tm:%lu, pip.active:%d, havePip:%d, codec:%d"), pXcode->common.encodeOutIdx, pthread_self(), pXcode->common.decodeOutIdx, PTSF(pXcode->common.inpts90Khz), PTSF(pXcode->common.outpts90Khz), PTSF(pData->curFrame.pkt.xtra.tm.pts), pData->curFrame.lenData, rc, pData->curFrame.idxCompoundFrame, pData->curFrame.numCompoundFrames, delmetotEncBytes, (long)(timer_GetTime()/TIME_VAL_MS), pData->piXcode->vid.pip.active, pData->piXcode->vid.overlay.havePip, pXcode->common.cfgFileTypeOut);

  return rc;
}

#define XCODE_UPDATE_SET_PARAM(old, new, u) if((old) != (new)) { (old) = (new); (u) = 1; } 

enum STREAM_NET_ADVFR_RC xcode_aud_update(STREAM_XCODE_DATA_T *pData,
                                          const IXCODE_AUDIO_CTXT_T *pUpdateCfg) {

  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  IXCODE_AUDIO_CTXT_T *pXcode;
  int doUpdateIdx = 0;

  if(!pData || !pUpdateCfg) {
    return -1;
  }

  pXcode = &pData->piXcode->aud;

  //pXcode->cfgBitRateOut = pUpdateCfg->cfgBitRateOut;
  XCODE_UPDATE_SET_PARAM(pXcode->cfgVolumeAdjustment, pUpdateCfg->cfgVolumeAdjustment, doUpdateIdx);

  //
  // Pass arbitrary flags (directives) to xcode layer
  //
  XCODE_UPDATE_SET_PARAM(pXcode->common.cfgFlags, pUpdateCfg->common.cfgFlags, doUpdateIdx);

  XCODE_UPDATE_SET_PARAM(pXcode->common.cfgNoDecode, pUpdateCfg->common.cfgNoDecode, doUpdateIdx);

  if(doUpdateIdx) {

    xcode_dump_conf_audio(pXcode, 0, 0, S_DEBUG);

    if((rc = (enum STREAM_NET_ADVFR_RC) xcode_init_aud(pXcode)) != 0) {
      LOG(X_ERROR("xcode_init_aud (reconfig) returned error:%d"), rc);
    }

  } else {

  }

  return rc;
}
