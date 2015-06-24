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

//#define DEBUG_MP2TS 1
//#define DEBUG_MP2TS_XMIT

//TODO: make sure the following is only enabled for capture_net.. not direct xmit of .m2t
//#define DONT_CHECK_MP2TS_TIMING

static int check_next_pkt(STREAM_RTP_MP2TS_REPLAY_T *pMp2ts, TIME_VAL tvNow);
static int getnext_mp2pkt(STREAM_RTP_MP2TS_REPLAY_T *pMp2ts,
                          MP2TS_PKT_T *pPkt);

static void reset_timing(STREAM_RTP_MP2TS_REPLAY_T *pMp2ts) {
  pMp2ts->pcrStart = 0;
  pMp2ts->pcrPrev = 0;
  pMp2ts->pcrPrevDuration = 0;
  pMp2ts->pktCntPrevPcr = 0;
  pMp2ts->localPcrStart = 0;
  pMp2ts->cachedPcr = 0;
  pMp2ts->pktGapMultiplier = .99;
  pMp2ts->rtpMulti.payloadLen = 0;
}

int stream_rtp_mp2tsraw_init(STREAM_RTP_MP2TS_REPLAY_T *pMp2ts) {
                   
  int rc = 0;
  //unsigned int idx;
  //STREAM_RTP_INIT_T rtpInit;

  if(pMp2ts == NULL) {
    return -1;
  } else if(!pMp2ts->cfg.pCbData || !pMp2ts->cfg.cbGetData ||
            !pMp2ts->cfg.cbResetData || !pMp2ts->cfg.cbHaveData) {
    return -1;
  }

  if(pMp2ts->mp2tslen == 0) {
    pMp2ts->mp2tslen = MP2TS_LEN;
  }


  pMp2ts->tmLastTx = 0;
  pMp2ts->tmLastPcrTx = 0;
  pMp2ts->pktGapMs = 0.0f;
  pMp2ts->latencyMs = 10; 

  reset_timing(pMp2ts);

  if(pMp2ts->replayUseLocalTiming) {
    LOG(X_DEBUG("Using local clock for mpeg2-ts pcr playout"));
  }

  memset(&pMp2ts->pat, 0, sizeof(pMp2ts->pat));

  //for(idx = 0; idx < numInit; idx++) {
    //memcpy(&rtpInit, &pInit[idx], sizeof(STREAM_RTP_INIT_T));
    //if((rc = stream_rtp_init(&pMp2ts->rtpMulti, idx, &rtpInit)) < 0) {
    //  break;
    //}
    //pMp2ts->rtpMulti.numDests++;

    if(pMp2ts->rtpMulti.init.maxPayloadSz < MP2TS_LEN) {
      rc = -1;
      ///break;
    }

  //}
                                    
  if(rc < 0) {
    stream_rtp_mp2tsraw_close(pMp2ts);
  }

  return rc;                                
}

int stream_rtp_mp2tsraw_reset(STREAM_RTP_MP2TS_REPLAY_T *pMp2ts) {
  int rc;

  pMp2ts->pktsSent = 0;
  pMp2ts->tmLastPcrTx = 0;
  pMp2ts->rtpMulti.payloadLen = 0;
  pMp2ts->pktGapMs= 0.0f;

  reset_timing(pMp2ts);

  if((rc = check_next_pkt(pMp2ts, 0)) < 0) {
    return rc;
  }

  return 0;
}


int stream_rtp_mp2tsraw_close(void *pArg) {
  STREAM_RTP_MP2TS_REPLAY_T *pMp2ts = (STREAM_RTP_MP2TS_REPLAY_T *) pArg;

  if(pMp2ts == NULL) {
    return -1;
  }

  return 0;
}

static int update_pat(STREAM_RTP_MP2TS_REPLAY_T *pMp2ts, 
                          MP2TS_PKT_T *pPkt) {
  int rc = 0;
  uint8_t ver;
  unsigned int idxCrc;

  idxCrc= pPkt->idx;
  if((rc = mp2_parse_pat(&pMp2ts->pat, &pPkt->pData[pPkt->idx], pPkt->len - pPkt->idx)) > 0) {
  //TODO: track each version
  // Upon pmt change, update version id
    ver = ((pPkt->pData[idxCrc + 5]) >> 1) & 0x1f;
    
    //if(((ver >> 1) & 0x01) != pMp2->pat.lastPatVersion
    //pPkt->pData[idxCrcStart + 5] = 0xc0 | (pMp2->pat.lastPatVersion << 1) | (ver & 0x01);

    // crc
    //*((uint32_t *)&pPkt->pData[pPkt->idx-4]) = mp2ts_crc(&pPkt->pData[idxCrc], rc - 4);
  }

  return rc;  
}

static int update_container_pmtchange_idx(STREAM_RTP_MP2TS_REPLAY_T *pMp2ts) {

  double locationMs = 0;

  if(!pMp2ts->cfg.pContainer) {
    return -1;
  }

  if(pMp2ts->pCurStreamChange == NULL) {
    pMp2ts->pCurStreamChange = pMp2ts->cfg.pContainer->pmtChanges;

  } else if(pMp2ts->pCurStreamChange->pmtVersion == pMp2ts->pmt.version) {

    locationMs = pMp2ts->plocationMs ? *pMp2ts->plocationMs : 0;
    if(locationMs == 0) {
      return 0; 
    }

    if((pMp2ts->pCurStreamChange = pMp2ts->pCurStreamChange->pnext) == NULL) {
      pMp2ts->pCurStreamChange = pMp2ts->cfg.pContainer->pmtChanges;
    }
    pMp2ts->locationMsStart = 0.0f;

    if(!pMp2ts->pCurStreamChange || 
       pMp2ts->pCurStreamChange->pmtVersion != pMp2ts->pmt.version) {
      LOG(X_WARNING("Container pmt version %u does not match stream pmt version %u"),
         pMp2ts->pCurStreamChange ? pMp2ts->pCurStreamChange->pmtVersion : 0,
         pMp2ts->pmt.version);
    }

    return 0;
  } else {
    
    if((pMp2ts->pCurStreamChange = pMp2ts->pCurStreamChange->pnext) == NULL) {
      pMp2ts->pCurStreamChange = pMp2ts->cfg.pContainer->pmtChanges;
    }
    pMp2ts->locationMsStart = 0.0f;

    if(!pMp2ts->pCurStreamChange || 
       pMp2ts->pCurStreamChange->pmtVersion != pMp2ts->pmt.version) {
      LOG(X_WARNING("Container pmt version %u does not match stream pmt version %u"),
         pMp2ts->pCurStreamChange ? pMp2ts->pCurStreamChange->pmtVersion : 0,
         pMp2ts->pmt.version);
    }
  }

  return 0;
}


static int update_pmt(STREAM_RTP_MP2TS_REPLAY_T *pMp2ts,
                      MP2TS_PKT_T *pPkt) {
  int rc = 0;
  unsigned int idxCrc;

  idxCrc= pPkt->idx;
  pMp2ts->pmt.numpids = 0;
  if((rc = mp2_parse_pmt(&pMp2ts->pmt, pPkt->pid, NULL, NULL, 
                         &pPkt->pData[pPkt->idx], pPkt->len - pPkt->idx, 0)) >= 0) {

    // crc
    //*((uint32_t *)&pPkt->pData[pPkt->idx-4]) = mp2ts_crc(&pPkt->pData[idxCrc], rc - 4);
  }

  return rc;
}

static int get_nextsyncedpkt(STREAM_RTP_MP2TS_REPLAY_T *pMp2ts,
                             MP2TS_PKT_T *pPkt) {
  unsigned int idx;
  unsigned int lenTmp = 0;
  unsigned char bufTmp[MP2TS_BDAV_LEN];

  for(idx = 0; idx < pPkt->len; idx++) {
    if(pPkt->pData[idx] == MP2TS_SYNCBYTE_VAL) {
      lenTmp = pPkt->len - idx;
      memcpy(bufTmp, &pPkt->pData[idx], lenTmp);
      break;
    }
  }

  if(pMp2ts->cfg.cbGetData(pMp2ts->cfg.pCbData, &bufTmp[lenTmp], 
                          pPkt->len - lenTmp, NULL, NULL) < 0) {
    return -1;
  }

  memcpy(pPkt->pData, bufTmp, pPkt->len);

  if(pPkt->pData[0] != MP2TS_SYNCBYTE_VAL) {
    LOG(X_WARNING("Unable to find mpeg2-ts syncbyte"));
    return -1;
  }

  return 0;
}

static const MP2_PID_T *find_pid(STREAM_RTP_MP2TS_REPLAY_T *pMp2ts,
                           uint32_t pid) {
  unsigned int idx = 0;
  for(idx = 0; idx < pMp2ts->pmt.numpids; idx++) {
    if(pMp2ts->pmt.pids[idx].id == pid) {
      return &pMp2ts->pmt.pids[idx];
    }
  }
  return NULL;
}

static int getnext_mp2pkt(STREAM_RTP_MP2TS_REPLAY_T *pMp2ts, 
                          MP2TS_PKT_T *pPkt) {

  const MP2_PID_T *pPid = NULL;
  STREAM_MP2TS_PROGDATA_T vidData;
  uint8_t hdr;
  uint8_t streamId;
  uint8_t peshdrlen;
  unsigned int startIdx = 0;
  int rc = 0;
  int sz;
  enum MP2TS_PARSE_RC parseRc = MP2TS_PARSE_UNKNOWN;

  pPkt->idx = 0;
  pPkt->len = pMp2ts->mp2tslen;

  if((sz = pMp2ts->cfg.cbGetData(pMp2ts->cfg.pCbData, pPkt->pData, pPkt->len, NULL, NULL)) < 0) {
    return -1;
  }

  if(pPkt->pData[pPkt->idx] != MP2TS_SYNCBYTE_VAL) {
    if(pMp2ts->mp2tslen == MP2TS_BDAV_LEN) {
      startIdx = pPkt->idx = 4;
    }

    if(pPkt->pData[pPkt->idx] != MP2TS_SYNCBYTE_VAL) {

      LOG(X_WARNING("Invalid mpeg2-ts syncbyte value"));
      if(get_nextsyncedpkt(pMp2ts, pPkt) != 0) {
        return -1;
      }
    }
  }

  if((parseRc = mp2ts_parse(pPkt, &pMp2ts->pat, &pMp2ts->pmt)) == MP2TS_PARSE_ERROR) {
    rc = -1;
  } else if(parseRc == MP2TS_PARSE_OK_PAT) {
    rc = update_pat(pMp2ts, pPkt); 
  } else if(parseRc == MP2TS_PARSE_OK_PMT) {
    rc = update_pmt(pMp2ts, pPkt);
  }

  if(parseRc != MP2TS_PARSE_OK_PAYLOADSTART) {
    pPkt->idx = startIdx;
    return rc;
  }

  streamId = pPkt->pData[pPkt->idx + 3];
  hdr = pPkt->pData[pPkt->idx + 7];
  peshdrlen = pPkt->pData[pPkt->idx + 8];
  pPkt->idx += 9;
  if(hdr & 0x80) {
      pPkt->tm.qtm.pts = mp2_parse_pts(&pPkt->pData[pPkt->idx]);
      pPkt->flags |= MP2TS_PARSE_FLAG_HAVEPTS;
    if(pPkt->idx + 5 < pPkt->len && (hdr & 0x40)) {
      pPkt->tm.qtm.dts = mp2_parse_pts(&pPkt->pData[pPkt->idx]);
      pPkt->flags |= MP2TS_PARSE_FLAG_HAVEDTS;
    }
  }
  pPkt->idx += peshdrlen;



  if((pPid = find_pid(pMp2ts, pPkt->pid))) {

    if(pPid->streamtype == MP2PES_STREAM_VIDEO_H262) {

      if(mpg2_check_seq_hdr_start(&pPkt->pData[pPkt->idx], pPkt->len - pPkt->idx)) {

        mpg2_parse_seq_hdr(&pPkt->pData[pPkt->idx + 4], 
                         pPkt->len - pPkt->idx - 4,
                         &vidData.u.mpeg2);

        if(memcmp(&vidData.u.mpeg2, &pMp2ts->vidData.u.mpeg2, 
                ((char*)&vidData.u.mpeg2.pad - (char *)&vidData.u.mpeg2))) {
        fprintf(stdout, "pid:0x%x h262 resolution: %u x %u, asp:%u, frm:%u, btr:%u, vbufsz:%u\n", 
          pPkt->pid, vidData.u.mpeg2.hpx, vidData.u.mpeg2.vpx, 
          vidData.u.mpeg2.aspratio, vidData.u.mpeg2.frmrate, 
          vidData.u.mpeg2.bitrate, vidData.u.mpeg2.vbufsz);

          memcpy(&pMp2ts->vidData.u.mpeg2, &vidData.u.mpeg2,
                sizeof(STREAM_MP2TS_PROGDATA_T));
          if(pMp2ts->pVidProp) {
            pMp2ts->pVidProp->resH = vidData.u.mpeg2.hpx;
            pMp2ts->pVidProp->resV = vidData.u.mpeg2.vpx;
            pMp2ts->pVidProp->fps = mpg2_get_fps(&vidData.u.mpeg2, NULL, NULL);
          }
        }
      }

    }

  } else {
    //LOG(X_WARNING("no pmt yet for pid: 0x%x"), pPkt->pid);
  }


  pPkt->idx = startIdx;
  return 0;
}

int stream_rtp_mp2tsraw_preparepkt(void *pArg) {
  return 0;
}

static int check_next_pkt(STREAM_RTP_MP2TS_REPLAY_T *pMp2ts, TIME_VAL tvNowArg) {
  int rc;
  MP2TS_PKT_T pkt;
  unsigned int payloadIdx = 0;
  int64_t tm90Hz= 0;
  int64_t pcrNow;
  TIME_VAL tvNow;

#ifdef DEBUG_MP2TS_XMIT
  int64_t pcrOffset = 0;
  unsigned short pididx;
  unsigned short numpids = 0;
  unsigned short pids[10];
  memset(pids, 0xff, sizeof(pids));
#endif // DEBUG_MP2TS_XMIT


  pkt.tm.pcr = 0;
  pkt.pid = 0;
  if(tvNowArg == 0) { 
    tvNow = timer_GetTime();
  } else {
    tvNow = tvNowArg;
  }
  pcrNow  = (uint64_t) ((double)(tvNow / (double)(100.0 / 9.0f)));
  pMp2ts->rtpMulti.payloadLen = 0;

#ifdef DEBUG_MP2TS_XMIT
  pcrOffset = pMp2ts->localPcrStart - pMp2ts->pcrStart;
#endif // DEBUG_MP2TS_XMIT

  while(payloadIdx + MP2TS_LEN < pMp2ts->rtpMulti.init.maxPayloadSz) {

    pkt.pData = &pMp2ts->rtpMulti.pPayload[payloadIdx];
    tm90Hz = 0;

    if(pMp2ts->cachedPcr) {
      tm90Hz = pMp2ts->cachedPcr;

      if(pcrNow + (pMp2ts->latencyMs *90) < (pMp2ts->localPcrStart - pMp2ts->pcrStart) + 
         tm90Hz) {
        break; 
      }
      memcpy(pkt.pData, pMp2ts->buf, MP2TS_LEN);
      pMp2ts->cachedPcr = 0;

    } else if(getnext_mp2pkt(pMp2ts, &pkt) < 0) {
      return -1;
    }

    if(pMp2ts->pat.progmappid != 0 && pkt.pid == pMp2ts->pat.progmappid &&
       pMp2ts->pmt.version != pMp2ts->prevPmtVersion) {

      LOG(X_DEBUG("Detected pmt version change %u -> %u.  Resetting timing."),
                  pMp2ts->prevPmtVersion, pMp2ts->pmt.version);
      reset_timing(pMp2ts);
      if(pMp2ts->pVidProp) {
        memset(&pMp2ts->vidData, 0, sizeof(pMp2ts->vidData));
        //Preserve mediaType
        pMp2ts->pVidProp->resH = pMp2ts->pVidProp->resV = 0;
        pMp2ts->pVidProp->fps = 0;
      }
      if(pMp2ts->cfg.pContainer && !pMp2ts->cfg.pContainer->liveFile) {
        update_container_pmtchange_idx(pMp2ts);
      }

      pMp2ts->prevPmtVersion = pMp2ts->pmt.version;
      pkt.tm.pcr = 0;
    }

    if(pkt.tm.pcr != 0) {
      tm90Hz = (int64_t) pkt.tm.pcr;
    }

    if(tm90Hz != 0) {

      if(pMp2ts->pcrStart == 0) {
        pMp2ts->pcrStart = tm90Hz;
        pMp2ts->localPcrStart = pcrNow;
      }  else if(pMp2ts->pcrPrev > 0 && pMp2ts->tmLastPcrTx > 0 &&  pMp2ts->tmLastTx > 0 &&
         (double)((tm90Hz - pMp2ts->pcrPrev)/90.0f) > 
         (double)((pMp2ts->tmLastTx - pMp2ts->tmLastPcrTx)/1000.0f) + 
                 MP2TS_LARGE_PCR_JUMP_MS) {

        LOG(X_WARNING("Detected large pcr jump %.3f -> %.3f  Resetting timing."),
            (double)pMp2ts->pcrPrev/90000.0f, (double)tm90Hz/90000.0f);
        reset_timing(pMp2ts);
        if(pMp2ts->cfg.pContainer && !pMp2ts->cfg.pContainer->liveFile) {
          update_container_pmtchange_idx(pMp2ts);
        }
        break;
      }

      if(pMp2ts->replayUseLocalTiming && 
         pcrNow + (pMp2ts->latencyMs * 90) < (pMp2ts->localPcrStart - pMp2ts->pcrStart) + tm90Hz) {

        memcpy(pMp2ts->buf, pkt.pData, MP2TS_LEN);
        pMp2ts->cachedPcr = tm90Hz;
        break;
      }

      if(tm90Hz > pMp2ts->pcrPrev) {

        if(pMp2ts->pcrPrev != 0) {
          pMp2ts->pcrPrevDuration = tm90Hz - pMp2ts->pcrPrev;

          pMp2ts->pktGapMs = (double)((pMp2ts->pcrPrevDuration)/90.0f)/
                         (double)pMp2ts->pktCntPrevPcr * pMp2ts->pktGapMultiplier;
          if(pMp2ts->pktGapMs > 15) {
            pMp2ts->pktGapMs = 15;
          }
# ifdef DEBUG_MP2TS_XMIT
fprintf(stdout, "rate pkt gap set to %fms, 1/%fs ,pkts:%u, pcr delta:%fs\n", pMp2ts->pktGapMs, (double)pMp2ts->pktCntPrevPcr / (pMp2ts->pcrPrevDuration) * 90000.0f, pMp2ts->pktCntPrevPcr, (double)(tm90Hz-pMp2ts->pcrPrev)/90000.0f);
#endif // DEBUG_MP2TS_XMIT
        }

        pMp2ts->pktCntPrevPcr = 0;
        pMp2ts->pcrPrev = tm90Hz;
      }

      pMp2ts->tmLastPcrTx = tvNow;

    }

#ifdef DEBUG_MP2TS_XMIT
  for(pididx = 0; pididx < sizeof(pids)/sizeof(pids[0]); pididx++) {
    if(pids[pididx] == 0xffff) {
      pids[pididx] = pkt.pid;
      numpids++;
      break;
    } else if(pids[pididx] == pkt.pid) {
      break;
    }
  }
#endif // DEBUG_MP2TS_XMIT

    payloadIdx += MP2TS_LEN;
  }

  if(payloadIdx > 0 && pMp2ts->pcrStart > 0) {
    pMp2ts->rtpMulti.payloadLen = payloadIdx;

    pMp2ts->pktCntPrevPcr++;
    pMp2ts->rtpMulti.pRtp->timeStamp = htonl((uint32_t) (long)(tvNow));


#ifdef DEBUG_MP2TS_XMIT
fprintf(stdout, ":%llu,send[%d]%d,locpcr:%f,pcr:%f,%llu [", tvNow%1000000, pMp2ts->pktCntPrevPcr, payloadIdx, (double)pcrNow/90000.0f, tm90Hz>0 ? (double)(pcrOffset+tm90Hz)/90000.0f : 0,tm90Hz);
  for(pididx = 0; pididx < numpids; pididx++) {
    fprintf(stdout, ",0x%x", pids[pididx]);
  }
fprintf(stdout, "]\n");
#endif // DEBUG_MP2TS_XMIT

    if((rc = stream_rtp_preparepkt(&pMp2ts->rtpMulti)) < 0) {
      return rc;
    }

#ifdef DEBUG_MP2TS_XMIT
    if(tm90Hz > 0 && pMp2ts->pktGapMs > 0 && pMp2ts->pcrPrev > 0 &&
       pcrNow > (pMp2ts->localPcrStart - pMp2ts->pcrStart) + tm90Hz + (pMp2ts->latencyMs * 90)  * 5) {
#ifdef DEBUG_MP2TS_XMIT
      fprintf(stdout, "Decreasing packet smoothing factor %f %f\n", (double)pcrNow/90000.0f,(double) ((pMp2ts->localPcrStart - pMp2ts->pcrStart) + tm90Hz)/90000.f);
#endif // DEBUG_MP2TS_XMIT
     }
#endif // DEBUG_MP2TS_XMIT

  }

  return 0;
}

int stream_rtp_mp2tsraw_cansend(void *pArg) {
  STREAM_RTP_MP2TS_REPLAY_T *pMp2ts = (STREAM_RTP_MP2TS_REPLAY_T *) pArg;
  TIME_VAL tvNow;
  TIME_VAL tvAssumedNext; 

  tvNow = timer_GetTime();

  // Ensure pre-configured min packet xmit interval rate is not exceeded
  if(pMp2ts->tmLastTx > 0 && pMp2ts->tmMinPktInterval > 0 &&
     tvNow - pMp2ts->tmLastTx < pMp2ts->tmMinPktInterval) {
    return 0;
  }
 
  // Smooth packet xmit timing according to pre-computed packet gap values
  if(pMp2ts->replayUseLocalTiming && pMp2ts->tmLastPcrTx > 0 && pMp2ts->pktGapMs > 0) {
    if(pMp2ts->pcrPrevDuration > 0 && tvNow + (pMp2ts->latencyMs *1000) > pMp2ts->tmLastPcrTx +
      (TIME_VAL) ((double)(pMp2ts->pcrPrevDuration/9.0f)*100) + 1000) {
      pMp2ts->pktGapMs = 0;
#ifdef DEBUG_MP2TS_XMIT
fprintf(stdout, "throttling up:%llu > :%llu %f pkt gap set to:%fms\n", tvNow%1000000,pMp2ts->tmLastPcrTx%1000000, (double)pMp2ts->pcrPrevDuration/90000.0f, pMp2ts->pktGapMs);
#endif // DEBUG_MP2TS_XMIT
    } else {
      tvAssumedNext = pMp2ts->tmLastPcrTx + (TIME_VAL)(pMp2ts->pktCntPrevPcr * pMp2ts->pktGapMs * 1000);
      if(tvNow < tvAssumedNext) {
        return 0;
      }
    }
  }
  
  // check the next packet from input or from prior cache
  if(check_next_pkt(pMp2ts, tvNow) < 0) {
    if(!pMp2ts->cfg.cbHaveData(pMp2ts->cfg.pCbData)) {
      LOG(X_DEBUG("All programs have ended"));
      return -2;
    }
    return -1;
  }

  if(pMp2ts->rtpMulti.payloadLen > 0) {

    //if(pMp2ts->pktsSent == 0) {
      //pMp2ts->rtpMulti.tmStartXmit = tvNow;
    //}

    pMp2ts->pktsSent++;
    pMp2ts->tmLastTx = tvNow;

    if(pMp2ts->plocationMs && pMp2ts->pCurStreamChange) {
      *pMp2ts->plocationMs = (double) ((pMp2ts->pcrPrev - pMp2ts->pcrStart)/90.0f) + 
                                       (pMp2ts->pCurStreamChange->startHz/90.0f) +
                                       (pMp2ts->locationMsStart *1000);
    }

    return 1;
  }

  return 0;
}

int stream_rtp_mp2tsraw_cbgetfiledata(void *p, unsigned char *pData, unsigned int len,
                                   PKTQ_EXTRADATA_T *pXtra, unsigned int *pNumOverwritten) {

  FILE_STREAM_T *pFile = (FILE_STREAM_T *) p;

  if(ReadFileStream(pFile, pData, len) < 0) {
    return -1;
  }

  return 0;
}
int stream_rtp_mp2tsraw_cbhavefiledata(void *p) {
  FILE_STREAM_T *pFile = (FILE_STREAM_T *) p;

  if(pFile->offset < pFile->size) {
    return 1; 
  }

  return 0;
}

int stream_rtp_mp2tsraw_cbresetfiledata(void *p) {
  FILE_STREAM_T *pFile = (FILE_STREAM_T *) p;

  if(SeekMediaFile(pFile, 0, SEEK_SET) < 0) {
    return -1;
  }

  return 0;
}

#endif //VSX_HAVE_STREAMER
