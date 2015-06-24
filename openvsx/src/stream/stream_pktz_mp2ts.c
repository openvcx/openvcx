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

//#define DEBUG_MP2TS_PTS 1
//#define DEBUG_MP2TS_XMIT 1


static struct timeval g_tv_mp2ts_sys_start;

#define MAX_PCR           0x1ffffffffULL
//#define MAX_PCR           1350000 
#define PCR_WRAP_SEC      46800

static uint64_t get_startHz(const PKTZ_MP2TS_PROG_T *pProg, int reset) {
  struct timeval tv;
  uint64_t startHz;

  gettimeofday(&tv, NULL);

  //
  // for 33 bit pcr,pts,dts startHz will wrap in 
  // 0x1ffffffff / 90000 / 3600  = ~ 26 hours 
  // Reset after ~ 13 hours to prevent ~ 26/2 hours ts wrap
  //  (0x1ffffffff - 0xffffffff (max tv.tv_sec value) )
  //
  if(reset || g_tv_mp2ts_sys_start.tv_sec == 0 || 
     (tv.tv_sec - g_tv_mp2ts_sys_start.tv_sec) > PCR_WRAP_SEC) {

    if(g_tv_mp2ts_sys_start.tv_sec > 0) {
      LOG(X_WARNING("Resetting system pcr start after %"SECT" sec (reset:%d) tm:%"SECT":%"USECT), 
                 tv.tv_sec - g_tv_mp2ts_sys_start.tv_sec, reset, tv.tv_sec, tv.tv_usec);
    } 

    g_tv_mp2ts_sys_start.tv_sec = tv.tv_sec;
    g_tv_mp2ts_sys_start.tv_usec = tv.tv_usec;

  }

  startHz = ((uint64_t) (((tv.tv_sec - g_tv_mp2ts_sys_start.tv_sec) * 1000) +
              ((tv.tv_usec - g_tv_mp2ts_sys_start.tv_usec) / 1000))) * 90;
 
  // Add ~ 2 seconds to prevent any avsync /dts offsets from resulting in negative val
  startHz += 180000;

  //if(startHz > (uint64_t) MAX_PCR) {
  //fprintf(stderr, "bogus large mpeg-ts clock tv:%lu:%"USECT" st tv:%lu:%"USECT" %"LL64"u\n", tv.tv_sec, tv.tv_usec, g_tv_mp2ts_sys_start.tv_sec, g_tv_mp2ts_sys_start.tv_usec, startHz);
  //}

  return startHz;
}

static void increment_patVersion(PKTZ_MP2TS_T *pPktzMp2ts) {
  if(++pPktzMp2ts->patVersion > 0x1f) {
    pPktzMp2ts->patVersion = 1;
  }
}

int stream_pktz_mp2ts_init(void *pArg, const void *pInitArgs, unsigned int progIdx) {
  PKTZ_MP2TS_T *pPktzMp2ts = (PKTZ_MP2TS_T *) pArg;
  PKTZ_INIT_PARAMS_MP2TS_T *pInitParams = (PKTZ_INIT_PARAMS_MP2TS_T *) pInitArgs;
  int rc = 0;
  int firstProgId;

  //fprintf(stderr, "stream_pktz_mp2ts_init progIdx:%d pRtpMulti:0x%x\n", progIdx,pInitParams->common.pRtpMulti);

  if(!pPktzMp2ts || !pInitParams || !pInitParams->common.pRtpMulti ||
     !pInitParams->common.cbXmitPkt) {
    return -1;
  }

  if((firstProgId = pInitParams->firstProgId) == 0) { 
    firstProgId = MP2TS_FIRST_PROGRAM_ID + progIdx;
  } else {

    //
    // Validate requested program id
    //
    firstProgId += progIdx;
    while(firstProgId < MP2TS_PMT_ID_DEFAULT + 1 ||
          firstProgId > MP2TS_HDR_MAXPID - 2) {

      if(firstProgId < MP2TS_PMT_ID_DEFAULT + 1) {
        firstProgId = MP2TS_PMT_ID_DEFAULT + 1;
      } else if(firstProgId > MP2TS_HDR_MAXPID - 2) {
        firstProgId = firstProgId % (MP2TS_HDR_MAXPID);
      }
    }
  } 

  //
  // Do not leave any lower prog idx un-init, esp if called w/ onlyaudio program
  // at progIdx=1
  //
  if(progIdx > pPktzMp2ts->numProg) {
    progIdx = pPktzMp2ts->numProg;
  }

  pPktzMp2ts->progs[progIdx].pinactive = pInitParams->common.pinactive;
  pPktzMp2ts->progs[progIdx].pPktzMp2ts =  pPktzMp2ts;
  pPktzMp2ts->progs[progIdx].pStreamId = pInitParams->common.pStreamId;
  pPktzMp2ts->progs[progIdx].pStreamType = pInitParams->common.pStreamType;
  pPktzMp2ts->progs[progIdx].lastStreamType = *pPktzMp2ts->progs[progIdx].pStreamType;
  pPktzMp2ts->progs[progIdx].pHaveDts = pInitParams->common.pHaveDts;
  pPktzMp2ts->progs[progIdx].pFrameData = pInitParams->common.pFrameData;
  pPktzMp2ts->progs[progIdx].prevContIdx = MP2TS_HDR_TS_CONTINUITY;
  pPktzMp2ts->progs[progIdx].progId = firstProgId;
  pPktzMp2ts->progs[progIdx].lastHz = 0;
  pPktzMp2ts->progs[progIdx].lastHzPcrOffset = 0;
  pPktzMp2ts->progs[progIdx].lastDtsHz = 0;
  pPktzMp2ts->progs[progIdx].frameRdIdx = 0;
  pPktzMp2ts->progs[progIdx].pavoffset_pktz = pInitParams->common.pavoffset_pktz;
  pPktzMp2ts->progs[progIdx].pPesLenZero = pInitParams->common.pPesLenZero;


  if(progIdx == 0) {
    pPktzMp2ts->progs[progIdx].includeVidH264AUD = pInitParams->includeVidH264AUD;
  }

  pPktzMp2ts->progs[progIdx].pvidProp = pInitParams->common.pvidProp;

  if(++pPktzMp2ts->numProg > 1) {
    return 0;
  }

  pPktzMp2ts->cbXmitPkt = pInitParams->common.cbXmitPkt;
  pPktzMp2ts->pXmitNode = pInitParams->common.pXmitNode;
  pPktzMp2ts->lastHz = 0;
  pPktzMp2ts->usePatDeltaHz = pInitParams->usePatDeltaHz;
  pPktzMp2ts->tmLastPcr = 0;
  pPktzMp2ts->tmLastPcrHz = 0;
  if((pPktzMp2ts->maxPcrIntervalMs = pInitParams->maxPcrIntervalMs) <= 0) {
    pPktzMp2ts->maxPcrIntervalMs = MP2TS_PCR_INTERVAL_MAX_MS;
  }
  if((pPktzMp2ts->maxPatIntervalMs = pInitParams->maxPatIntervalMs) <= 0) {
    pPktzMp2ts->maxPatIntervalMs = MP2TS_PAT_INTERVAL_MAX_MS;
  }

  pPktzMp2ts->maxPmtIntervalMs = pPktzMp2ts->maxPatIntervalMs;
  pPktzMp2ts->tmLastPat = 0;
  pPktzMp2ts->tmLastPatHz = 0;
  pPktzMp2ts->plastPatContIdx = pInitParams->plastPatContIdx;
  pPktzMp2ts->patVersion = random() % 0x1ff;
  pPktzMp2ts->patTsId = random() % RAND_MAX;
  pPktzMp2ts->tmLastPmt = 0;
  pPktzMp2ts->tmLastPmtHz = 0;
  pPktzMp2ts->plastPmtContIdx = pInitParams->plastPmtContIdx;
  pPktzMp2ts->pmtId = MP2TS_PMT_ID_DEFAULT;
  pPktzMp2ts->tmLastTx = 0;
  pPktzMp2ts->pcrProgIdx = 0;
  pPktzMp2ts->pRtpMulti = pInitParams->common.pRtpMulti;
  pPktzMp2ts->xmitflushpkts = pInitParams->common.xmitflushpkts;
  pPktzMp2ts->pRtpMulti->pRtp->timeStamp = 0;
  pPktzMp2ts->clockStartHz = get_startHz(NULL, 0);

  if(pPktzMp2ts->pRtpMulti->init.maxPayloadSz < MP2TS_LEN) {
    rc = -1;
  }

  if(rc < 0) {
     stream_pktz_mp2ts_close(pPktzMp2ts); 
  }

  return rc;
}


void stream_pktz_mp2ts_resettiming(PKTZ_MP2TS_T *pPktzMp2ts) {
  unsigned int idx;
  uint64_t lastHzPcrOffset[PKTZ_MP2TS_NUM_PROGS];

  lastHzPcrOffset[0] = pPktzMp2ts->progs[0].pFrameData ? OUTFMT_PTS(pPktzMp2ts->progs[0].pFrameData) : 
                       pPktzMp2ts->progs[0].lastHz;
  //if(lastHzPcrOffset[0] == 0) {
  lastHzPcrOffset[1] = lastHzPcrOffset[0];
  //} else {
  //  lastHzPcrOffset[1] = pPktzMp2ts->progs[1].pFrameData ? pPktzMp2ts->progs[1].pFrameData->pts : 
  //                     pPktzMp2ts->progs[1].lastHz;
  //}

  pPktzMp2ts->pcrProgIdx = 0;
  pPktzMp2ts->tmLastPat = 0;
  pPktzMp2ts->tmLastPatHz = 0;
  pPktzMp2ts->tmLastPmt = 0;
  pPktzMp2ts->tmLastPmtHz = 0;
  pPktzMp2ts->tmLastPcrHz = 0;
  pPktzMp2ts->clockStartHz = get_startHz(&pPktzMp2ts->progs[0], 0);

  pPktzMp2ts->lastHz = 0;

  for(idx= 0; idx < pPktzMp2ts->numProg; idx++) {
    pPktzMp2ts->progs[idx].lastHz = 0;
    //pPktzMp2ts->progs[idx].lastHzPcrOffset = 0;
  }

  //fprintf(stderr, "stream_pktz_mp2ts_resettiming lastHzPcrOffset:%.3f %.3f [ pts:%.3f, lastHz:%.3f ] [ pts:%.3f, lastHz:%.3f ]\n", PTSF(lastHzPcrOffset[0]), PTSF(lastHzPcrOffset[1]), PTSF(pPktzMp2ts->progs[0].pFrameData->pts), PTSF(pPktzMp2ts->progs[0].lastHz), PTSF(pPktzMp2ts->progs[1].pFrameData->pts), PTSF(pPktzMp2ts->progs[1].lastHz));

  pPktzMp2ts->progs[0].lastHzPcrOffset = lastHzPcrOffset[0];
  pPktzMp2ts->progs[1].lastHzPcrOffset = lastHzPcrOffset[1];

}

int stream_pktz_mp2ts_reset(void *pArg, int fullReset, unsigned int progIdx) {
  PKTZ_MP2TS_T *pPktzMp2ts = (PKTZ_MP2TS_T *) pArg;
  unsigned int idx;

  if(progIdx > 0) {
    return 0;
  }

  LOG(X_DEBUG("stream_pktz_mp2ts_reset full:%d"), fullReset);

  //pPktzMp2ts->pRtpMulti->pktsSent = 0;

  if(fullReset) {

    stream_pktz_mp2ts_resettiming(pPktzMp2ts);
/*
    pPktzMp2ts->pcrProgIdx = 0;
    pPktzMp2ts->tmLastPat = 0;
    pPktzMp2ts->tmLastPatHz = 0;
    pPktzMp2ts->tmLastPmt = 0;
    pPktzMp2ts->tmLastPmtHz = 0;
    pPktzMp2ts->tmLastPcrHz = 0;
    pPktzMp2ts->clockStartHz = get_startHz(&pPktzMp2ts->progs[idx], 0);
*/
    increment_patVersion(pPktzMp2ts);

  }

  pPktzMp2ts->lastHz = 0;

  for(idx= 0; idx < pPktzMp2ts->numProg; idx++) {
    pPktzMp2ts->progs[idx].lastHz = 0;

    //
    // TODO: should not be reset if after PCR rollover, should be reset for file jump
    // TODO: ok this is pretty bad
    // stream_av_reset (full:2) called via stream_net_xxx during terrible pts jump or programming
    // change should preserve l;astHzPcrOffset - where the value is used only to not exceed MAX_PCR
    // stream_av_reset (full:1) called via streamer_rtp  (during non-live seek jump) should reset this to 0
    //
    if(fullReset != 2) {
      pPktzMp2ts->progs[idx].lastHzPcrOffset = 0;
    }

    pPktzMp2ts->progs[idx].lastDtsHz = 0;
    pPktzMp2ts->progs[idx].noMoreData = 0;
    pPktzMp2ts->progs[idx].pesLenRemaining = 0;
    pPktzMp2ts->progs[idx].pesOverflowSz = 0;
    pPktzMp2ts->progs[idx].frameRdIdx = 0;
  }

  return 0;
}




int stream_pktz_mp2ts_close(void *pArg) {
  PKTZ_MP2TS_T *pPktzMp2ts = (PKTZ_MP2TS_T *) pArg;

  if(pPktzMp2ts == NULL) {
    return -1;
  }

  return 0;
}


static int get_next_continuity(int cidx) {
  if(++cidx > MP2TS_HDR_TS_CONTINUITY) {
    cidx = 0;
  }
  return cidx;
}

static int create_Pat(PKTZ_MP2TS_T *pMp2, MP2TS_PKT_T *pMp2Pkt) {
  unsigned int payloadLen;
  unsigned int idx;
  unsigned int patCrcStart;

  for(idx = 0; idx < pMp2->numProg; idx++) {
    if(*pMp2->progs[idx].pStreamType != pMp2->progs[idx].lastStreamType) {
      increment_patVersion(pMp2);
      pMp2->progs[idx].lastStreamType = *pMp2->progs[idx].pStreamType;
    }
  }

  payloadLen = 17;
  idx = MP2TS_LEN - payloadLen;
  *pMp2->plastPatContIdx = get_next_continuity(*pMp2->plastPatContIdx);

  pMp2Pkt->hdr = MP2TS_SYNCBYTE_VAL << 24;
  pMp2Pkt->hdr |= (MP2TS_HDR_PAYLOAD_UNIT_START | 
                       MP2TS_HDR_TS_ADAPTATION_EXISTS | 
                       MP2TS_HDR_TS_PAYLOAD_EXISTS |
                       (*pMp2->plastPatContIdx & MP2TS_HDR_TS_CONTINUITY));
  *((uint32_t *) pMp2Pkt->pData) = htonl(pMp2Pkt->hdr);

  pMp2Pkt->pData[4] = idx - 5; // adaptation len
  pMp2Pkt->pData[5] = 0; // adaptation header
  memset(&pMp2Pkt->pData[6], 0xff, idx - 6); // stuffing

  pMp2Pkt->pData[idx++] = 0; 
  patCrcStart = idx;
  pMp2Pkt->pData[idx++] = 0; // table id
  *((uint16_t *) &pMp2Pkt->pData[idx]) = 0xb0 | htons((payloadLen - 4) & 0x0fff);
  idx+= 2;
  //TODO: update id when changing content
  *((uint16_t *) &pMp2Pkt->pData[idx]) = htons(pMp2->patTsId); 
  idx += 2;
  pMp2Pkt->pData[idx++] = 0xc0 | (pMp2->patVersion << 1) | 0x01;
  pMp2Pkt->pData[idx++] = 0; // section number
  pMp2Pkt->pData[idx++] = 0; // last section number

  //1 program entry
  *((uint16_t *) &pMp2Pkt->pData[idx]) = htons(1);
  idx += 2;
  *((uint16_t *) &pMp2Pkt->pData[idx]) =  0xe0 | htons(pMp2->pmtId & 0x1fff);
  idx+= 2;

  // crc
  *((uint32_t *)&pMp2Pkt->pData[idx]) = mp2ts_crc(&pMp2Pkt->pData[patCrcStart], 
                                              idx - patCrcStart);
  //
  // Cache the last PAT packet
  //
  if(pMp2->cacheLastPat &&
     pMp2->lastPatCrc != *((uint32_t *)&pMp2Pkt->pData[idx])) {
    pMp2->lastPatCrc = *((uint32_t *)&pMp2Pkt->pData[idx]);
    memcpy(pMp2->lastPatBuf, pMp2Pkt->pData, MP2TS_LEN);
    //fprintf(stdout, "pat crc: 0x%x\n", *((uint32_t *)&pMp2Pkt->pData[idx]));
  }

  idx += 4;

  return MP2TS_LEN;
}

static int create_Pmt(PKTZ_MP2TS_T *pMp2, MP2TS_PKT_T *pMp2Pkt) {
  unsigned int pmtCrcStart;
  unsigned int progIdx;
  unsigned int numProg = 0;
  unsigned int payloadLen;
  unsigned int xtraLen = 0;
  unsigned int idx;


  for(progIdx = 0; progIdx < pMp2->numProg; progIdx++) {

    if(*pMp2->progs[progIdx].pStreamType != MP2PES_STREAMTYPE_NOTSET &&
       ! *pMp2->progs[progIdx].pinactive) { 
      numProg++;

//fprintf(stderr, "pmt %d pvid:0x%x mt:%d\n", progIdx, pMp2->progs[progIdx].pvidProp, pMp2->progs[progIdx].pvidProp ? pMp2->progs[progIdx].pvidProp->mediaType : 0);
      //
      // Insert private data field
      //
      if(pMp2->progs[progIdx].pvidProp &&
         (pMp2->progs[progIdx].pvidProp->mediaType == XC_CODEC_TYPE_H263 ||
          pMp2->progs[progIdx].pvidProp->mediaType == XC_CODEC_TYPE_H263_PLUS)) {

        //fprintf(stderr, "H.263 PMT private data %dx%d\n", pMp2->progs[progIdx].pvidProp->resH, pMp2->progs[progIdx].pvidProp->resV);
        // private
        pMp2->progs[progIdx].pmtXtraData[0] = MP2PES_STREAM_VIDEO_H263;
        // length
        pMp2->progs[progIdx].pmtXtraData[1] = 10;
        // 4Cs codec, width, height
        pMp2->progs[progIdx].pmtXtraData[2] = 'H';
        pMp2->progs[progIdx].pmtXtraData[3] = '2';
        pMp2->progs[progIdx].pmtXtraData[4] = '6';
        pMp2->progs[progIdx].pmtXtraData[5] = '3';
        *((uint16_t *) &pMp2->progs[progIdx].pmtXtraData[6]) =  
                               htons(pMp2->progs[progIdx].pvidProp->resH);
        *((uint16_t *) &pMp2->progs[progIdx].pmtXtraData[8]) = 
                               htons(pMp2->progs[progIdx].pvidProp->resV);
        *((uint16_t *) &pMp2->progs[progIdx].pmtXtraData[10]) = htons(0x00);
        pMp2->progs[progIdx].pmtXtraDataLen = 12;

      }

      xtraLen += pMp2->progs[progIdx].pmtXtraDataLen;

    } else {
      if(pMp2->pcrProgIdx == progIdx) {
        pMp2->pcrProgIdx++; 
      }
    }
  }


  //if(numProg > 0 && numProg < pMp2->numProg) {
  //  LOG(X_WARNING("PMT - streamtype not set for %d / %d"), pMp2->numProg - numProg, pMp2->numProg);
  //}

  //fprintf(stderr, "creating pmt with %d/%d progs\n", numProg, pMp2->numProg); 

  if(pMp2->numProg <= 0) {
    LOG(X_WARNING("Create PMT called with no configured programs"));
  } else if(numProg == 0) {
    LOG(X_WARNING("Create PMT called with no valid stream types"));
  }

  payloadLen = 17 + xtraLen + (5 * numProg);
  idx = MP2TS_LEN - payloadLen;

  *pMp2->plastPmtContIdx = get_next_continuity(*pMp2->plastPmtContIdx);

  pMp2Pkt->hdr = MP2TS_SYNCBYTE_VAL << 24;
  pMp2Pkt->hdr |= (MP2TS_HDR_PAYLOAD_UNIT_START | 
                      ((pMp2->pmtId << 8) & MP2TS_HDR_TS_PID) |
                       MP2TS_HDR_TS_ADAPTATION_EXISTS | 
                       MP2TS_HDR_TS_PAYLOAD_EXISTS |
                       (*pMp2->plastPmtContIdx & MP2TS_HDR_TS_CONTINUITY));
  *((uint32_t *) pMp2Pkt->pData) = htonl(pMp2Pkt->hdr);

  pMp2Pkt->pData[4] = idx - 5; // adaptation len
  pMp2Pkt->pData[5] = 0; // adaptation header
  memset(&pMp2Pkt->pData[6], 0xff, idx - 6); // stuffing

  pMp2Pkt->pData[idx++] = 0; 
  pmtCrcStart = idx;
  pMp2Pkt->pData[idx++] = 0x02; // ts-pmt section table id
  //pMp2Pkt->pData[idx] = 0xb0;
  *((uint16_t *) &pMp2Pkt->pData[idx]) = 0xb0 | htons((payloadLen - 4) & 0x0fff);
  idx+= 2;
  *((uint16_t *) &pMp2Pkt->pData[idx]) = htons(0x01); // program number
  idx += 2;
  pMp2Pkt->pData[idx++] = 0xc0 | (pMp2->patVersion << 1) | 0x01;
  pMp2Pkt->pData[idx++] = 0; // section number
  pMp2Pkt->pData[idx++] = 0; // last section number
  *((uint16_t *) &pMp2Pkt->pData[idx]) = 0xe0 | 
                htons((pMp2->progs[pMp2->pcrProgIdx].progId) & 0x1fff);
  idx+= 2;
  *((uint16_t *) &pMp2Pkt->pData[idx]) = 0xf0 | htons((0) & 0x0fff);//(extra) prog info len
  idx+= 2;

  // insert optional program descriptor(s) here
  for(progIdx = 0; progIdx < pMp2->numProg; progIdx++) {

    if(*pMp2->progs[progIdx].pStreamType != MP2PES_STREAMTYPE_NOTSET &&
       ! *pMp2->progs[progIdx].pinactive) {

                                  // stream type iso13818-1 table2-29
      pMp2Pkt->pData[idx++] = (uint8_t) *pMp2->progs[progIdx].pStreamType;                                    

      //fprintf(stderr, "create pmt for [%d] st:0x%x %s\n", progIdx, *pMp2->progs[progIdx].pStreamType, (pMp2->pcrProgIdx == progIdx ? "PCR" : ""));

      *((uint16_t *) &pMp2Pkt->pData[idx]) =  0xe0 | htons(pMp2->progs[progIdx].progId & 0x1fff);
      idx+= 2;
      *((uint16_t *) &pMp2Pkt->pData[idx]) =  0xf0 | 
          htons(pMp2->progs[progIdx].pmtXtraDataLen); // elementary stream info len
      idx+= 2;
      if(pMp2->progs[progIdx].pmtXtraDataLen > 0) {
        memcpy(&pMp2Pkt->pData[idx], pMp2->progs[progIdx].pmtXtraData, 
               pMp2->progs[progIdx].pmtXtraDataLen);
        idx += pMp2->progs[progIdx].pmtXtraDataLen;
      }

    }
  }

  // crc
  *((uint32_t *)&pMp2Pkt->pData[idx]) = mp2ts_crc(&pMp2Pkt->pData[pmtCrcStart], 
                                              idx - pmtCrcStart);

  //
  // Cache the last PMT packet
  //
  if(pMp2->cacheLastPmt && 
     pMp2->lastPmtCrc != *((uint32_t *)&pMp2Pkt->pData[idx])) {
    pMp2->lastPmtCrc =  *((uint32_t *)&pMp2Pkt->pData[idx]);
    memcpy(pMp2->lastPmtBuf, pMp2Pkt->pData, MP2TS_LEN); 
    //fprintf(stdout, "pmt crc: 0x%x\n", *((uint32_t *)&pMp2Pkt->pData[idx]));
  }

  idx += 4;

  //avc_dumpHex(stderr, pMp2Pkt->pData, idx, 1);

  return MP2TS_LEN;
}

static int write_pcr(uint64_t startHz,
                      unsigned char *buf, const PKTZ_MP2TS_PROG_T *pProg) {
  int rc = 0;
  uint64_t tm;

  tm  = pProg->lastHz;
  //if((tm  = pProg->lastHz) > pProg->lastHzPcrOffset) {
  //  tm -= pProg->lastHzPcrOffset;
  //}
  tm += startHz;

  //fprintf(stderr, "write_pcr[prog:0x%x] %"LL64"u + %"LL64"u = %"LL64"u (%.3f), lastHzPcrOffset:%"LL64"u (%.3f)\n", pProg->progId, pProg->lastHz, startHz, tm, PTSF(tm), pProg->lastHzPcrOffset, PTSF(pProg->lastHzPcrOffset));


  if(tm > (uint64_t) MAX_PCR) {
    LOG(X_WARNING("pcr too large %"LL64"u + %"LL64"u = %"LL64"u (%.3f), lastHzPcrOffset: %"LL64"u (%.3f)"), 
            pProg->lastHz, startHz, tm, PTSF(tm), pProg->lastHzPcrOffset, PTSF(pProg->lastHzPcrOffset));
    rc = -1;
  }

  // 33 bits PCR (90Khz), 6 bits reserved (1), 9 bits PCR ext (27MHz)
  buf[0] = (unsigned char) (tm >> 25);
  buf[1] = (unsigned char) (tm >> 17);
  buf[2] = (unsigned char) (tm >> 9);
  buf[3] = (unsigned char) (tm >> 1);
  buf[4] = ((tm & 1) << 7) | 0x7e;
  buf[5] = 0;

  return rc;
}

//static int g_tmidx;

static void write_pts(uint64_t tm, int hdr, unsigned char *buf) {

  //if(tm > (uint64_t) MAX_PCR) {
  //  fprintf(stdout, "pts too large: %"LL64"u\n", tm);
  //}

  //if((g_tmidx++ % 100)==0 && (hdr &0x02)) { tm = random(); fprintf(stderr, "GARBAGE TM %.3f\n", PTSF(tm)); }

  // 33 bits data value (in 90Khz) out of 40 bits :
  // 4 bits prefix, 3 bits data, 1 bit marker, 15 bits data, 1 bit marker, 
  // 15 bits data, 1 bit marker
  // prefix = 0011 if pts & dts present... 2nd 5 byte field dts prefx = 0001
  // prefix = 0010 if only pts

  buf[0] = (unsigned char) ((hdr << 4) | (((tm >> 30) & 0x07) << 1) | 1);
  buf[1] = (unsigned char) (tm >> 22);
  buf[2] = (unsigned char) ((((tm >> 15) & 0x7f) << 1) | 1);
  buf[3] = (unsigned char) (tm >> 7);
  buf[4] = (unsigned char) (((tm & 0x7f) << 1) | 1);

}

static int copyFrameChunk(PKTZ_MP2TS_PROG_T *pProg, unsigned char *pBuf, unsigned int len) {

  unsigned int frameRdIdx;
  unsigned int lencopy = len;

  //fprintf(stderr, "PFRAMEDATA: 0x%x pData:0x%x\n", pProg->pFrameData, pProg->pFrameData->pData);

  if(!pProg->pFrameData || ! OUTFMT_DATA(pProg->pFrameData)) {
    LOG(X_ERROR("No program frame data set for prog: 0x%x %s"), 
                pProg->progId, pProg->pFrameData ? "" : "<NULL>");
    return -1;
  } else if(len > pProg->frameDataActualLen - pProg->frameRdIdx) {
    LOG(X_ERROR("prog: 0x%x copy len:[%d + %d / %d]"),
        pProg->progId, len, pProg->frameRdIdx, pProg->frameDataActualLen);
    return -1;
  }

  //
  // Include NAL Access Unit Delimeters
  // AUDs are required for QuickTime X / Apple Live Streaming
  //
  if(pProg->frameRdIdx == 0 && pProg->includeVidH264AUD) {

    if(len < 6) {
      LOG(X_ERROR("Unable to add AUD NAL for requested length %d"), len);
      return -1;
    }
    *((uint32_t *) &pBuf[0]) = htonl(0x01);
    //
    // Mpeg4-Part10AVC Table 7-2 "Meaning of primary_pic_type"
    // 3 bits 0 - 7 ((7 << 5) = I,Si,P,SP,B possible slice types in primary coded picture)
    //
    pBuf[4] = NAL_NRI_NONE | NAL_TYPE_ACCESS_UNIT_DELIM;
    pBuf[5] = H264_AUD_I_SI_P_SP_B;
    pBuf += 6;
    pProg->frameRdIdx += 6;
    lencopy -= 6;
  }

  frameRdIdx = pProg->frameRdIdx;
  if(pProg->includeVidH264AUD) {
    frameRdIdx -= 6;
  }

  //
  // Copy the frame data from the encoded buffer
  //
  //fprintf(stderr, "copying %d [%d/%d(%d)] progid:0x%x\n", lencopy, pProg->frameRdIdx, pProg->frameDataActualLen, pProg->pFrameData->len, pProg->progId);
  memcpy(pBuf, &OUTFMT_DATA(pProg->pFrameData)[frameRdIdx], lencopy);
  pProg->frameRdIdx += lencopy;

  return len;
}

static int create_Pes(PKTZ_MP2TS_PROG_T *pProg, MP2TS_PKT_T *pMp2Pkt, 
                      const TIME_VAL *pTmNow) {

  PKTZ_MP2TS_T *pMp2 = (PKTZ_MP2TS_T *) pProg->pPktzMp2ts;
  int szFrameLeft;
  unsigned int idx = 0;
  unsigned int szPesHdr = 0;
  unsigned int szAdaptationBody = 0;
  unsigned int szStuffing = 0;
  unsigned int szData;
  unsigned int szPesFrame = 0;
  int haveAdaptation = 0;
  int includePcr = 0;
  //uint64_t lastHzPcrOffset[PKTZ_MP2TS_NUM_PROGS];
  unsigned int progIdx = (pProg == &pMp2->progs[0]) ? 0 : 1;

  if(pProg->pesOverflowSz > 0) {
    szFrameLeft = (int) pProg->pesLenRemaining;
    //fprintf(stderr, "LEN REM:%d\n", pProg->pesOverflowSz);
  } else if(pProg->frameRdIdx == 0) {
    szFrameLeft = 0;
  } else {
    szFrameLeft = pProg->frameDataActualLen - pProg->frameRdIdx;
  }

  //if(pProg->frameRdIdx == 0) fprintf(stderr, "create_Pes[%d]  tm:%.3f %d/%d, %s \n", progIdx, (double)pProg->lastHz/90000.0f, pProg->frameRdIdx, pProg->frameDataActualLen, OUTFMT_KEYFRAME(pProg->pFrameData) ? "--key--" : "");

  if(szFrameLeft == 0) { 

    //
    // If the current frame is a key frame, automatically include a PCR value
    // to allow any viewer to begin decode
    //
    if(OUTFMT_KEYFRAME(pProg->pFrameData)) {
      includePcr = 1;
    }

    szPesHdr = 14; // 9 byte header + 5 byte pts

    if(pProg->pesOverflowSz > 0) {
      // Continuation of prior PES > 0xffff
      szFrameLeft = pProg->pesOverflowSz;

      if(*pProg->pHaveDts && pProg->lastDtsHz != 0) {
        szPesHdr += 5;
      }

#ifdef DEBUG_MP2TS_PTS
      fprintf(stdout, "cont: %u ", szFrameLeft);
#endif // DEBUG_MP2TS_PTS


    } else {

      szFrameLeft = pProg->frameDataActualLen - pProg->frameRdIdx;

#ifdef DEBUG_MP2TS_PTS
      fprintf(stdout, "sz: %u ", szFrameLeft);
#endif // DEBUG_MP2TS_PTS

#ifdef DEBUG_MP2TS_XMIT
      fprintf(stdout, "prog:0x%x stype:0x%x frame sz: %u\n", 
              pProg->progId, *pProg->pStreamType, szFrameLeft);
#endif // DEBUG_MP2TS_XMIT


      //
      // insert deoder time stamp
      // Account for possible B frame pic order count index references
      //
      if(*pProg->pHaveDts && pProg->lastDtsHz != 0) {
        szPesHdr += 5;
      }

    }

    if(szFrameLeft > (int) (0xffff - szPesHdr - 10)) {

      pProg->pesOverflowSz = szFrameLeft - (0xffff - szPesHdr - 10); 
      szFrameLeft = (0xffff - szPesHdr - 10);

    } else if(pProg->pesOverflowSz > 0) {
      pProg->pesOverflowSz = 0;
    }

    szPesFrame = szFrameLeft;

    if(includePcr == 0 && pMp2->pcrProgIdx == progIdx) {
      if(pMp2->usePatDeltaHz) {
        if(pMp2->tmLastPcrHz == 0 ||
           pMp2->lastHz > pMp2->tmLastPcrHz + (pMp2->maxPcrIntervalMs* 90)) {
          includePcr = 1;
        }
      } else {
        LOG(X_ERROR("PCR insertion for !usePatDeltaHz not implemented"));

        //if(((double)pMp2->tmLastPcr/1000.0f +  pMp2->maxPcrIntervalMs) <= 
        //      (double)(*pTmNow)/1000.0f + 
        //      (((float) pProg->frameDeltaHz / pProg->clockRateHz)*1000.0f) ) {
        //  includePcr = 1;
        //}
      }
    }

    if(includePcr) {

      haveAdaptation = 1;
      szAdaptationBody += 6;
      pMp2->tmLastPcr = *pTmNow;
      pMp2->tmLastPcrHz = pProg->lastHz;

    }

    pProg->pesLenRemaining = szFrameLeft;
    
  } else {
    // PES stream continuation

  }

  if((szData = MP2TS_LEN - (4 + (haveAdaptation ? szAdaptationBody + 2 : 0) 
                              + szPesHdr)) > (unsigned int) szFrameLeft) {
    if(szData == szFrameLeft + 1) {
      szData = szFrameLeft - 2;
    } else {
      szData = szFrameLeft;
    }
    szStuffing = MP2TS_LEN - (4 + 2 + szAdaptationBody + szPesHdr + szData);
    haveAdaptation = 1;
  } 

  if(szPesFrame == 0) {
    szPesFrame = szData;
  }

  pProg->prevContIdx = get_next_continuity(pProg->prevContIdx);

  //
  // Create the m2t packet header 4 bytes
  //
  // xxxx xxxx                               - SYNCBYTE
  //           x                             - TSERROR
  //            x                            - PAYLOAD_UNIT_START
  //             x                           - TS_PRIORITY
  //              x xxxx xxxx xxxx           - (13 bits) - PID
  //                               xx        - SCRAMBLING_CTRL
  //                                 xx      - ADAPTATION
  //                                    xxxx - CONTINUITY
  //
  //
  pMp2Pkt->hdr = MP2TS_SYNCBYTE_VAL << 24;
  pMp2Pkt->hdr |= ((szPesHdr ? MP2TS_HDR_PAYLOAD_UNIT_START : 0) | 
                       ((pProg->progId << 8) & MP2TS_HDR_TS_PID) |
                       (haveAdaptation ? MP2TS_HDR_TS_ADAPTATION_EXISTS : 0) | 
                       MP2TS_HDR_TS_PAYLOAD_EXISTS |
                       (pProg->prevContIdx & MP2TS_HDR_TS_CONTINUITY));
  *((uint32_t *) pMp2Pkt->pData) = htonl(pMp2Pkt->hdr);
  idx += 4;

  if(pMp2Pkt->hdr & MP2TS_HDR_TS_ADAPTATION_EXISTS) {
    pMp2Pkt->pData[idx++] = 1 + szAdaptationBody + szStuffing; // adaptation field length
    pMp2Pkt->pData[idx++] = 0;
    if(includePcr && szAdaptationBody > 0) {

      pMp2Pkt->pData[idx-1] |= MP2TS_ADAPTATION_PCR;
//fprintf(stderr, "lastHz now:%lldHz (dts:%lldHz),prog.pts:%llu clockSt:%lld, lastHzPcrOffset:%lld\n", pProg->lastHz, pProg->lastDtsHz, pProg->pFrameData->pts, pMp2->clockStartHz, pProg->lastHzPcrOffset);
      if(write_pcr(pMp2->clockStartHz, &pMp2Pkt->pData[idx], pProg) < 0) {

        //lastHzPcrOffset[0] = pMp2->progs[0].lastHz;
        //lastHzPcrOffset[1] = pMp2->progs[1].lastHz;
        //lastHzPcrOffset[0] = pMp2->progs[0].pFrameData ? pMp2->progs[0].pFrameData->pts : pMp2->progs[0].lastHz;
        //lastHzPcrOffset[1] = pMp2->progs[1].pFrameData ? pMp2->progs[1].pFrameData->pts : pMp2->progs[1].lastHz;

        stream_pktz_mp2ts_resettiming(pMp2);

        //TODO: need to adjust audio lastHz as well as vid, along w PCR
        //pMp2->progs[0].lastHzPcrOffset = lastHzPcrOffset[0];
        //pMp2->progs[1].lastHzPcrOffset = lastHzPcrOffset[1];
        pMp2->clockStartHz = get_startHz(pProg, 1);
        if(write_pcr(pMp2->clockStartHz, &pMp2Pkt->pData[idx], pProg) < 0) {
          return -1;
        }
      }
      idx += szAdaptationBody;
    }
    if(szStuffing > 0) {
      memset(&pMp2Pkt->pData[idx], 0xff, szStuffing);
      idx += szStuffing;
    }
  }

  if(szPesHdr > 0) {

    //
    // Create the PES header
    //
    // 0x00 
    // 0x00 
    // 0x01  
    // 8 bits stream id 
    // 16 bits payload length 
    //
    // 10                      - 2 bit header bits always 10
    //   xx                    - scrambling control
    //      x                  - priority
    //       x                 - data alignmenet
    //        x                - copyright
    //         x               - original
    //           x             - pts
    //            x            - dts
    //             x           - escr
    //              x          - es rate
    //                x        - dsm trick
    //                 x       - additional copy info
    //                  x      - crc
    //                   x     - extension
    // 8 bits header data length
    // [ PES header data, PTS / DTS ] 
    //
    pMp2Pkt->pData[idx++] = 0x00;
    pMp2Pkt->pData[idx++] = 0x00;
    pMp2Pkt->pData[idx++] = 0x01;
    pMp2Pkt->pData[idx++] = (uint8_t) *pProg->pStreamId;
    if(*pProg->pPesLenZero) {
      *((uint16_t *) &pMp2Pkt->pData[idx]) = 0;
    } else {
      *((uint16_t *) &pMp2Pkt->pData[idx]) = htons((szPesHdr - 6) + szPesFrame);
    }
    idx += 2;
    pMp2Pkt->pData[idx++] = 0x80;
    pMp2Pkt->pData[idx++] = 0x80 | (pProg->lastDtsHz != 0 ? 0x40 : 0);
    pMp2Pkt->pData[idx++] = szPesHdr - 9;

    //fprintf(stderr, "write_pts[prog:0x%x] %llu (%.3f) lastHz:(%.3f) frame:(%.3f) lastHzPcrOffset:(%.3f), clockStart:(%.3f)\n", pProg->progId, pMp2->clockStartHz + pProg->lastHz + pProg->lastDtsHz + *pProg->pavoffset_pktz, PTSF(pMp2->clockStartHz + pProg->lastHz + pProg->lastDtsHz + *pProg->pavoffset_pktz), PTSF(pProg->lastHz), PTSF(pProg->pFrameData->pts), PTSF(pProg->lastHzPcrOffset), PTSF(pMp2->clockStartHz));

    write_pts(pMp2->clockStartHz + pProg->lastHz + pProg->lastDtsHz + *pProg->pavoffset_pktz,
              0x02 | (pProg->lastDtsHz != 0 ? 0x01 : 0x00), &pMp2Pkt->pData[idx]);

    idx += 5;

#ifdef DEBUG_MP2TS_PTS
    fprintf(stdout, "prog:0x%x stype:0x%x pes pts %llu (lastHz:%llu)", 
            pProg->progId, *pProg->pStreamType, 
            pMp2->clockStartHz + pProg->lastHz + pProg->lastDtsHz + pProg->avSyncOffsetHz, 
            pProg->lastHz);
#endif // DEBUG_MP2TS_PTS

    if(pProg->lastDtsHz != 0) {
      // insert decoder timestamp
      write_pts(pMp2->clockStartHz + pProg->lastHz + *pProg->pavoffset_pktz, 
                 0x01, &pMp2Pkt->pData[idx]);
      idx += 5;
#ifdef DEBUG_MP2TS_PTS
      fprintf(stdout, " dts%llu", pMp2->clockStartHz + pProg->lastHz + pProg->avSyncOffsetHz);
#endif // DEBUG_MP2TS_PTS

    }
#ifdef DEBUG_MP2TS_PTS
    fprintf(stdout, " avsync:%lld\n", pProg->avSyncOffsetHz);
#endif // DEBUG_MP2TS_PTS

  }

  //
  // Copy (a chunk) of the program frame data
  //
  //fprintf(stderr, "COPY FRAME CHUNK idx:%d, szData:%d\n", idx, szData);
  if(copyFrameChunk(pProg, &pMp2Pkt->pData[idx], szData) != szData) {
    return -1;
  }

  pProg->pesLenRemaining -= szData;
  
  if(idx + szData != MP2TS_LEN) {
    LOG(X_WARNING("Invalid mpeg2-ts frame length copied: %u+%u prog:0x%x, pcr:%d, adaptation:%d"), 
        idx, szData, pProg->progId, includePcr, szAdaptationBody);
  } 

  //fprintf(stderr, "created pes chunk pos:[%d,%d/%d] pts:%.3f dts:%.3f pcr:%d\n", pProg->frameRdIdx, pProg->pesLenRemaining, pProg->frameDataActualLen, (double)pProg->lastHz/90000.0f, (double)pProg->lastDtsHz/90000.0f, includePcr);
 
  return MP2TS_LEN;
}

//static int delmecnt;

static int sendpkt(PKTZ_MP2TS_T *pPktzMp2ts) {
  int rc = 0;

  pPktzMp2ts->pRtpMulti->pRtp->timeStamp = htonl((uint32_t)
   (long)(pPktzMp2ts->progs[pPktzMp2ts->pcrProgIdx].lastHz + pPktzMp2ts->clockStartHz));

//fprintf(stderr, "mp2ts rtp ts:%u pcrProgIdx:%d %.3f\n", htonl(pPktzMp2ts->pRtpMulti->pRtp->timeStamp),pPktzMp2ts->pcrProgIdx,(double)pPktzMp2ts->progs[pPktzMp2ts->pcrProgIdx].lastHz/90000.0f);

  rc = stream_rtp_preparepkt(pPktzMp2ts->pRtpMulti);

  if(rc >= 0) {
    //fprintf(stderr, "stream_sendpkt len:%d\n", pPktzMp2ts->pRtpMulti->payloadLen);
//if(delmecnt++ < 20 || delmecnt > 63)

    //
    // This calls streamer.c::stream_sendpkt
    //
    if((rc = pPktzMp2ts->cbXmitPkt(pPktzMp2ts->pXmitNode)) < 0) {
      LOG(X_ERROR("MPEG-2 TS packetizer failed to send output packet length %d"), stream_rtp_datalen(pPktzMp2ts->pXmitNode->pRtpMulti));
    }
    //fprintf(stderr, "stream_pktz_mp2ts sendpkt rc:%d\n", rc); 
  }

  pPktzMp2ts->pRtpMulti->payloadLen = 0;

  //fprintf(stderr, "rc;%d\n", rc);

  return rc;
}

#if 0

static int g_fd;
static int g_idx;
static void broken_bframes(const OUTFMT_FRAME_DATA_T *pFrame) {

  if(!g_fd) g_fd = open("test.h264", O_RDWR | O_CREAT, 0666);
  write(g_fd, pFrame->pData, pFrame->len);

  fprintf(stderr, "h.264 [%d] len:%5d pts:%.3f dts:%.3f %s {0x%x 0x%x 0x%x 0x%x 0x%x 0x%x}\n", g_idx++, pFrame->len, PTSF(pFrame->pts), PTSF(pFrame->dts), (pFrame->keyframe ? "KEYFRAME" : ""), pFrame->pData[0], pFrame->pData[1], pFrame->pData[2], pFrame->pData[3], pFrame->pData[4], pFrame->pData[5]);

}

#endif

//static int g_delme;

int stream_pktz_mp2ts_addframe(void *pArg, unsigned int progIdx) {
  PKTZ_MP2TS_T *pPktzMp2ts = (PKTZ_MP2TS_T *) pArg;
  PKTZ_MP2TS_PROG_T *pProg;
  int rc = 0;
  MP2TS_PKT_T mp2tsPkt;
  TIME_VAL tvNow;
  int sendPat = 0;
  int sendPmt = 0;
  unsigned int xmitflushpkts = 0;

  //if(progIdx==0 && g_delme++>50) { *pPktzMp2ts->progs[0].pinactive = 1; return 0; }
  //fprintf(stderr, "stream_pktz_mp2ts_addframe progIdx:%d\n", progIdx);

  //
  // xmitflushpkts is the smallest queued compound m2t packet sent to the network
  //
  if((xmitflushpkts = pPktzMp2ts->xmitflushpkts) == 0 || 
      xmitflushpkts > pPktzMp2ts->pRtpMulti->init.maxPayloadSz) {
    xmitflushpkts = pPktzMp2ts->pRtpMulti->init.maxPayloadSz;
  }

  if(pPktzMp2ts->numProg == 1 && progIdx >= pPktzMp2ts->numProg) {
    progIdx = 0; 
  } else if(progIdx >= pPktzMp2ts->numProg) {
    return -1;
  }

  pProg = (PKTZ_MP2TS_PROG_T *) &pPktzMp2ts->progs[progIdx];

  if(*pProg->pinactive) {
    LOG(X_DEBUG("stream_pktz_mp2ts_addframe called on inactive prog [%d]"), progIdx);
    return 0;
  }

  pProg->frameRdIdx = 0;
  pProg->lastHz = OUTFMT_PTS(pProg->pFrameData);
  if(pProg->lastHz > pProg->lastHzPcrOffset) {
    pProg->lastHz -= pProg->lastHzPcrOffset;
  }
  //fprintf(stderr, "lastHz:%llu, frame pts:%llu lastHzPcrOffset:%lld\n", pProg->lastHz,  pProg->pFrameData->pts, pProg->lastHzPcrOffset);

  pProg->lastDtsHz = OUTFMT_DTS(pProg->pFrameData);
  pPktzMp2ts->lastHz = pProg->lastHz;
  pProg->frameDataActualLen = OUTFMT_LEN(pProg->pFrameData);
  if(pProg->includeVidH264AUD) {
    pProg->frameDataActualLen += 6;
  }

  tvNow = timer_GetTime();
  
//if(pProg->pFrameData->isvid) broken_bframes(pProg->pFrameData);

  VSX_DEBUG_RTP (
    LOG(X_DEBUG("RTP - MPEG-2 TS packetizer[%d] prog: 0x%x, len:%d pts:%.3f(%.3f) dts:%.3f "
             "key:%d fr:[%d/%d] pkt:[%d/%d] aud:%d"), 
    progIdx, pProg->progId, OUTFMT_LEN(pProg->pFrameData), PTSF(OUTFMT_PTS(pProg->pFrameData)), 
    PTSF(OUTFMT_PTS(pProg->pFrameData) - pProg->dbglastpts), 
    PTSF(OUTFMT_DTS(pProg->pFrameData)), OUTFMT_KEYFRAME(pProg->pFrameData), pProg->frameRdIdx,
    pProg->frameDataActualLen, pPktzMp2ts->pRtpMulti->payloadLen,
    pPktzMp2ts->pRtpMulti->init.maxPayloadSz, pProg->includeVidH264AUD); 
    pProg->dbglastpts = OUTFMT_PTS(pProg->pFrameData);
    //avc_dumpHex(stderr, OUTFMT_DATA(pProg->pFrameData), OUTFMT_LEN(pProg->pFrameData) > 32 ? 32 : OUTFMT_LEN(pProg->pFrameData), 1);
  )

  if(pPktzMp2ts->usePatDeltaHz) {
    if(pPktzMp2ts->tmLastPatHz == 0 || pPktzMp2ts->lastHz > pPktzMp2ts->tmLastPatHz + 
                                       (pPktzMp2ts->maxPatIntervalMs * 90)) {
      sendPat = 1; 
;
      pPktzMp2ts->tmLastPatHz = pPktzMp2ts->lastHz + 1;
    }
    if(pPktzMp2ts->tmLastPmtHz == 0 || pPktzMp2ts->lastHz > pPktzMp2ts->tmLastPmtHz + 
                                       (pPktzMp2ts->maxPmtIntervalMs * 90)) {
      sendPmt = 1; 
      pPktzMp2ts->tmLastPmtHz = pPktzMp2ts->lastHz + 1;
      //fprintf(stderr, "sendPMT %u ms\n",  pPktzMp2ts->maxPmtIntervalMs);
    }

  } else {

    if(tvNow > pPktzMp2ts->tmLastPat + (pPktzMp2ts->maxPatIntervalMs * 1000)) {
      sendPat = 1;
    }
    if(tvNow > pPktzMp2ts->tmLastPmt + (pPktzMp2ts->maxPmtIntervalMs * 1000)) {
      sendPmt = 1;
    }

  }

  if(sendPat) {

    if(pPktzMp2ts->pRtpMulti->payloadLen + rc > 
       pPktzMp2ts->pRtpMulti->init.maxPayloadSz) {
      if(sendpkt(pPktzMp2ts) < 0) {
        return -1;
      }
    }

    mp2tsPkt.pData = 
        &pPktzMp2ts->pRtpMulti->pPayload[pPktzMp2ts->pRtpMulti->payloadLen];
    if((rc = create_Pat(pPktzMp2ts, &mp2tsPkt)) <= 0) {
      return -1;
    }

    pPktzMp2ts->tmLastPat = tvNow;
    pPktzMp2ts->pRtpMulti->payloadLen += rc;

  }

  if(sendPmt) {

    if(pPktzMp2ts->pRtpMulti->payloadLen + rc > 
       pPktzMp2ts->pRtpMulti->init.maxPayloadSz) {
      if(sendpkt(pPktzMp2ts) < 0) {
        return -1;
      }
    }

    mp2tsPkt.pData = 
        &pPktzMp2ts->pRtpMulti->pPayload[pPktzMp2ts->pRtpMulti->payloadLen];
    if((rc = create_Pmt(pPktzMp2ts, &mp2tsPkt)) <= 0) {
      return -1;
    }

    pPktzMp2ts->tmLastPmt = tvNow;
    pPktzMp2ts->pRtpMulti->payloadLen += rc;

  }

  while(pProg->frameRdIdx < pProg->frameDataActualLen) {

    //fprintf(stderr, "pktz ts[%d/%d] payload[%d/%d]\n", pProg->frameRdIdx, pProg->frameDataActualLen, pPktzMp2ts->pRtpMulti->payloadLen, pPktzMp2ts->pRtpMulti->init.maxPayloadSz);

    while(pPktzMp2ts->pRtpMulti->payloadLen + MP2TS_LEN <= 
          pPktzMp2ts->pRtpMulti->init.maxPayloadSz &&
          pProg->frameRdIdx < pProg->frameDataActualLen) {

      mp2tsPkt.pData = 
          &pPktzMp2ts->pRtpMulti->pPayload[pPktzMp2ts->pRtpMulti->payloadLen];

      if((rc = create_Pes(pProg, &mp2tsPkt, &tvNow)) < 0) {
        //fprintf(stderr, "create pes failed\n");
        return rc;
      }
      pPktzMp2ts->pRtpMulti->payloadLen += rc;
    }

    //if(pPktzMp2ts->xmitflushpkts || pPktzMp2ts->pRtpMulti->payloadLen + MP2TS_LEN >
    //                                pPktzMp2ts->pRtpMulti->init.maxPayloadSz) {
    if(pPktzMp2ts->pRtpMulti->payloadLen + MP2TS_LEN > xmitflushpkts) {
      if(sendpkt(pPktzMp2ts) < 0) {
        return -1;
      }
    }

  }

  return 0;
}


#endif // VSX_HAVE_STREAMER
