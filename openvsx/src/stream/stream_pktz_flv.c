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

//TODO: this is deprecated - moved to stream_outfmt similar to RTMP output

#if 0

int stream_pktz_flv_init(void *pArg, const void *pInitArgs, unsigned int progIdx) {
  PKTZ_FLV_T *pPktzFlv = (PKTZ_FLV_T *) pArg;
  PKTZ_INIT_PARAMS_FLV_T *pInitParams = (PKTZ_INIT_PARAMS_FLV_T *) pInitArgs;
  int rc = 0;
  int firstProgId;

  if(!pPktzFlv || !pInitParams || 
     //!pInitParams->common.pRtpMulti ||
     !pInitParams->common.cbXmitPkt) {
    return -1;
  }

  //
  // Do not leave any lower prog idx un-init, esp if called w/ onlyaudio program
  // at progIdx=1
  //
  if(progIdx > pPktzFlv->numProg) {
    progIdx = pPktzFlv->numProg;
  }


  if(++pPktzFlv->numProg > 1) {
    return 0;
  }

  pPktzFlv->cbXmitPkt = pInitParams->common.cbXmitPkt;
  //pPktzFlv->pXmitNode = pInitParams->common.pXmitNode;
  pPktzFlv->tmLastTx = 0;
  //pPktzFlv->pRtpMulti = pInitParams->common.pRtpMulti;
  //pPktzFlv->pRtpMulti->pRtp->timeStamp = 0;

  //if(pPktzFlv->pRtpMulti->init.maxPayloadSz < MP2TS_LEN) {
  //  rc = -1;
  //}

  if(rc < 0) {
     stream_pktz_flv_close(pPktzFlv); 
  }

  return rc;
}

int stream_pktz_flv_reset(void *pArg, int fullReset, unsigned int progIdx) {
  PKTZ_FLV_T *pPktzFlv = (PKTZ_FLV_T *) pArg;
  unsigned int idx;

  if(progIdx > 0) {
    return 0;
  }

  return 0;
}

int stream_pktz_flv_close(void *pArg) {
  PKTZ_FLV_T *pPktzFlv= (PKTZ_FLV_T *) pArg;

  if(pPktzFlv == NULL) {
    return -1;
  }

  return 0;
}



static int copyFrameChunk(PKTZ_FLV_PROG_T *pProg, unsigned char *pBuf, unsigned int len) {

  unsigned int frameRdIdx;
  unsigned int lencopy = len;
/*
  if(!pProg->pFrameData || !pProg->pFrameData->pData) {
    LOG(X_ERROR("No program frame data set for prog: 0x%x %s"), 
                pProg->progId, pProg->pFrameData ? "" : "<NULL>");
    return -1;
  } else if(len > pProg->frameDataActualLen - pProg->frameRdIdx) {
    LOG(X_ERROR("prog: 0x%x copy len:[%d + %d / %d]"),
        pProg->progId, len, pProg->frameRdIdx, pProg->frameDataActualLen);
    return -1;
  }

  frameRdIdx = pProg->frameRdIdx;
  if(pProg->includeVidH264AUD) {
    frameRdIdx -= 6;
  }

  //
  // Copy the frame data from the encoded buffer
  //
  //fprintf(stderr, "copying %d [%d/%d(%d)]\n", lencopy, pProg->frameRdIdx, pProg->frameDataActualLen, pProg->pFrameData->len);
  memcpy(pBuf, &pProg->pFrameData->pData[frameRdIdx], lencopy);
  pProg->frameRdIdx += lencopy;
*/
  return len;
}

static int sendpkt(PKTZ_FLV_T *pPktzFlv) {
  int rc = 0;
/*
  pPktzFlv->pRtpMulti->pRtp->timeStamp = htonl((uint32_t)
   (long)(pPktzMp2ts->progs[pPktzMp2ts->pcrProgIdx].lastHz + pPktzMp2ts->clockStartHz));

//fprintf(stderr, "mp2ts rtp ts:%u pcrProgIdx:%d %.3f\n", htonl(pPktzMp2ts->pRtpMulti->pRtp->timeStamp),pPktzMp2ts->pcrProgIdx,(double)pPktzMp2ts->progs[pPktzMp2ts->pcrProgIdx].lastHz/90000.0f);

  rc = stream_rtp_preparepkt(pPktzMp2ts->pRtpMulti);

  if(rc >= 0) {
    //fprintf(stderr, "stream_sendpkt len:%d\n", pPktzMp2ts->pRtpMulti->payloadLen);
//if(delmecnt++ < 20 || delmecnt > 63)
    rc = pPktzMp2ts->cbXmitPkt(pPktzMp2ts->pXmitNode);
    //fprintf(stderr, "stream_pktz_mp2ts sendpkt rc:%d\n", rc); 
  }

  pPktzMp2ts->pRtpMulti->payloadLen = 0;

  //fprintf(stderr, "rc;%d\n", rc);
*/
  return rc;
}

int stream_pktz_flv_addframe(void *pArg, unsigned int progIdx) {
  PKTZ_FLV_T *pPktzFlv = (PKTZ_FLV_T *) pArg;
  PKTZ_FLV_PROG_T *pProg = NULL;
  int rc = 0;
  TIME_VAL tvNow;

  if(pPktzFlv->numProg == 1 && progIdx >= pPktzFlv->numProg) {
    progIdx = 0; 
  } else if(progIdx >= pPktzFlv->numProg) {
    return -1;
  }

  //pProg = (PKTZ_FLV_PROG_T *) &pPktzFlv->progs[progIdx];

  //if(*pProg->pinactive) {
  //  LOG(X_DEBUG("stream_pktz_flv_addframe called on inactive prog [%d]"), progIdx);
  //  return 0;
  //}

/*
  VSX_DEBUG(
    VSX_DEBUGLOG("pktz_mp2ts_addframe[%d] prog: 0x%x, len:%d pts:%.3f(%.3f) dts:%.3f "
             "key:%d fr:[%d/%d] pkt:[%d/%d] aud:%d\n", 
    progIdx, pProg->progId, pProg->pFrameData->len, PTSF(pProg->pFrameData->pts), 
    PTSF(pProg->pFrameData->pts - pProg->dbglastpts), 
    PTSF(pProg->pFrameData->dts), pProg->pFrameData->keyframe, pProg->frameRdIdx,
    pProg->frameDataActualLen, pPktzMp2ts->pRtpMulti->payloadLen,
    pPktzMp2ts->pRtpMulti->init.maxPayloadSz, pProg->includeVidH264AUD); 
    pProg->dbglastpts = pProg->pFrameData->pts;
    avc_dumpHex(stderr, pProg->pFrameData->pData, pProg->pFrameData->len > 32 ? 32 : pProg->pFrameData->len, 1);
  )
*/


  return 0;
}

#endif // 0

#endif // VSX_HAVE_STREAMER
