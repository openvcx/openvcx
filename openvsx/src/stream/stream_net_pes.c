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

static enum STREAM_NET_ADVFR_RC get_nextsyncedpkt(const STREAM_DATASRC_T *pDataSrc,
                                       MP2TS_PKT_T *pPkt, unsigned int *pNumOverwritten) {
  unsigned int idx;
  unsigned int lenTmp = 0;
  unsigned int lenRead;
  unsigned char bufTmp[MP2TS_BDAV_LEN];

  for(idx = 0; idx < pPkt->len; idx++) {
    if(pPkt->pData[idx] == MP2TS_SYNCBYTE_VAL) {
      lenTmp = pPkt->len - idx;
      memcpy(bufTmp, &pPkt->pData[idx], lenTmp);
      break;
    }
  }

  lenRead = pPkt->len - lenTmp;
  if(pDataSrc->cbGetData(pDataSrc->pCbData, &bufTmp[lenTmp], lenRead, 
                         NULL, pNumOverwritten) < 0) {
    return STREAM_NET_ADVFR_RC_ERROR;
  } else if(lenRead <  lenRead) {
    return STREAM_NET_ADVFR_RC_NOTAVAIL; 
  }

  memcpy(pPkt->pData, bufTmp, pPkt->len);

  if(pPkt->pData[0] != MP2TS_SYNCBYTE_VAL) {
    LOG(X_WARNING("Unable to find mpeg2-ts syncbyte"));
    return STREAM_NET_ADVFR_RC_ERROR;
  }

  return STREAM_NET_ADVFR_RC_OK;
}

static enum STREAM_NET_ADVFR_RC update_pat(STREAM_PES_T *pPes,
                                           MP2TS_PKT_T *pPkt) {
  uint8_t ver;
  int sz;
  unsigned int idxCrc;
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;

  idxCrc= pPkt->idx;
  if((sz = mp2_parse_pat(&pPes->pat, &pPkt->pData[pPkt->idx], pPkt->len - pPkt->idx)) > 0) {

  //TODO: track each version
  // Upon pmt change, update version id
    ver = ((pPkt->pData[idxCrc + 5]) >> 1) & 0x1f;
   
  } else if(sz < 0) {
    rc = STREAM_NET_ADVFR_RC_ERROR;
  }

  return rc;
}

static enum STREAM_NET_ADVFR_RC update_pmt(STREAM_PES_T *pPes,
                                           MP2TS_PKT_T *pPkt) {
  unsigned int idxProg;
  unsigned int idxCrc; 
  int sz = 0;
  int havever = pPes->pmt.havever;
  uint8_t numpids = pPes->pmt.numpids;
  int vidProg = -1;
  int audProg = -1;
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;

  idxCrc= pPkt->idx;
  pPes->pmt.numpids = 0;
  if((sz = mp2_parse_pmt(&pPes->pmt, pPkt->pid, NULL, NULL,
                         &pPkt->pData[pPkt->idx], pPkt->len - pPkt->idx, 0)) >= 0) {

    if(numpids != pPes->pmt.numpids || pPes->pmt.version != pPes->prevPmtVersion) {

      pPes->pmt_haveVid = pPes->pmt_haveAud = 0;
      for(idxProg = 0; idxProg < pPes->pmt.numpids; idxProg++) {
        if(!pPes->pmt_haveVid && (pPes->pmt_haveVid = codectype_isVid(mp2_getCodecType(
                                              pPes->pmt.pids[idxProg].streamtype)))) {
          vidProg = idxProg;
        }
        if(!pPes->pmt_haveAud && (pPes->pmt_haveAud = codectype_isAud(mp2_getCodecType(
                                              pPes->pmt.pids[idxProg].streamtype)))) {
          audProg = idxProg;
        }
      }

      LOG(X_DEBUG("pmt ver:%d, pids:%d, pcr:0x%x, v:%d, a:%d, [pid:0x%x, st:0x%x], [pid:0x%x, st:0x%x]"), 
      pPes->pmt.version, pPes->pmt.numpids, pPes->pmt.pcrpid, 
      pPes->pmt_haveVid, pPes->pmt_haveAud, 
      pPes->pmt.pids[vidProg >= 0 ? vidProg : 0].id, pPes->pmt.pids[vidProg >= 0 ? vidProg : 0].streamtype,
      pPes->pmt.pids[audProg >= 0 ? audProg : 0].id, pPes->pmt.pids[audProg >= 0 ? audProg : 0].streamtype);
    }


    if(!havever || pPes->pmt.version != pPes->prevPmtVersion) {
      LOG(X_DEBUG("Detected pmt version change %u -> %u.  Resetting timing."),
                  pPes->prevPmtVersion, pPes->pmt.version);

      memset(pPes->progs, 0, sizeof(pPes->progs));
      pPes->vid.pProg = pPes->aud.pProg = NULL;
      for(idxProg = 0; idxProg < pPes->pmt.numpids; idxProg++) {
        if(vidProg >= 0 && audProg >= 0) {
          if(idxProg == vidProg) {
            memcpy(&pPes->progs[0].pid, &pPes->pmt.pids[idxProg], sizeof(MP2_PID_T));
          } else if(idxProg == audProg) {
            memcpy(&pPes->progs[1].pid, &pPes->pmt.pids[idxProg], sizeof(MP2_PID_T));
          }
        } else if(idxProg < 2) {
          memcpy(&pPes->progs[idxProg].pid, &pPes->pmt.pids[idxProg], sizeof(MP2_PID_T));
        }
      }
      pPes->prevPmtVersion = pPes->pmt.version;
      if(havever) {
        rc = STREAM_NET_ADVFR_RC_NEWPROG;
      }
    }

    //fprintf(stderr, "PIDS:%d stype:0x%x stype:0x%x v:0x%x a:0x%x\n", pPes->pmt.numpids, pPes->progs[0].pid.streamtype, pPes->progs[1].pid.streamtype, pPes->vid.pProg, pPes->aud.pProg);

  } else {
    rc = STREAM_NET_ADVFR_RC_ERROR;
  }

  return rc;
}

static MP2_PES_PROG_T *findProg(STREAM_PES_T *pPes, uint16_t pid) {
  unsigned int idx = 0;

  for(idx = 0; idx < pPes->pmt.numpids; idx++) {
    //fprintf(stderr, "findProg comparing 0x%x 0x%x\n",  pPes->progs[idx].pid.id, pid);
    if(pPes->progs[idx].pid.id == pid) {
      return &pPes->progs[idx]; 
    }
  }

  return NULL;
}

/*
static int delmecnt0;
static long long delmecnt0_offset;
static int delmecnt1;
static long long delmecnt1_offset;
*/


static enum STREAM_NET_ADVFR_RC load_mp2ts_pkt(STREAM_PES_DATA_T *pData, 
                                               MP2TS_PKT_T *pPkt, 
                                               STREAM_PES_DATA_T **ppPktProgData) {

  STREAM_PES_T *pPes = (STREAM_PES_T *) pData->pPes;
  MP2_PES_PROG_T *pProg = NULL;
  int sz = 0;
  enum MP2TS_PARSE_RC parseRc = MP2TS_PARSE_UNKNOWN;
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  int haveTm = 0;
  unsigned int startIdx = 0;
  unsigned int numOverwritten = 0;

  pPkt->flags = 0;
  pPkt->idx = 0;
  pPkt->len = pPes->tsPktLen;
  pPkt->pData = pPes->buf;

  if((sz = pPes->pDataSrc->cbGetData(pPes->pDataSrc->pCbData, pPkt->pData, 
                                      pPes->tsPktLen, NULL, &numOverwritten)) < 0) {
    return STREAM_NET_ADVFR_RC_ERROR;
  } else if(sz < (int) pPes->tsPktLen) {
    return STREAM_NET_ADVFR_RC_NOTAVAIL;
  } else if(numOverwritten > 0) {

    //
    // The input packet queue reader is too slow for the writer.
    // If performing transcoding, reset the queue to the start since
    // it is likely that the encoder is continuously too slow
    // Otherwise, the default behavior of the packet queue is for the reader
    // to skip forward slightly
    //
    if(pData->pXcodeData->piXcode) {
      pPes->pDataSrc->cbResetData(pPes->pDataSrc->pCbData);
    }

    //return STREAM_NET_ADVFR_RC_RESET;
  }


  if(pPkt->pData[startIdx] != MP2TS_SYNCBYTE_VAL) {
    if(pPes->tsPktLen == MP2TS_BDAV_LEN) {
      startIdx += 4;
    }

    if(pPkt->pData[pPkt->idx] != MP2TS_SYNCBYTE_VAL) {

      LOG(X_WARNING("Invalid mpeg2-ts syncbyte at start"));
      if((rc = get_nextsyncedpkt(pPes->pDataSrc, pPkt, &numOverwritten)) != 
               STREAM_NET_ADVFR_RC_OK) {
        return rc;
      }
    }
  }

  if((parseRc = mp2ts_parse(pPkt, &pPes->pat, &pPes->pmt)) == MP2TS_PARSE_ERROR) {

    return STREAM_NET_ADVFR_RC_ERROR;

  } else if(parseRc == MP2TS_PARSE_OK_PAT) {

    rc = update_pat(pPes, pPkt);
    pPkt->idx = pPkt->len;
    return rc;

  } else if(parseRc == MP2TS_PARSE_OK_PMT) {

    rc = update_pmt(pPes, pPkt);
    pPkt->idx = pPkt->len;
    return rc;

  } else if(parseRc != MP2TS_PARSE_OK_PAYLOADSTART &&
            parseRc != MP2TS_PARSE_OK_PAYLOADCONT) {
 
    if(parseRc == MP2TS_PARSE_ERROR) {
      LOG(X_ERROR("mpeg2-ts pes parse error"));
      return STREAM_NET_ADVFR_RC_ERROR;
    }
    pPkt->idx = pPkt->len;
    return STREAM_NET_ADVFR_RC_OK;
  }

  //
  // No pmt has been observed yet
  //
  if(!pPes->pmt.havever) {
    pPkt->idx = pPkt->len;
    return STREAM_NET_ADVFR_RC_OK;
  }

  if(parseRc == MP2TS_PARSE_OK_PAYLOADSTART || parseRc == MP2TS_PARSE_OK_PAYLOADCONT) {
  
    pProg = findProg(pPes, pPkt->pid);

    //if(pProg) fprintf(stderr, "pkt pid:0x%x, prog pid: 0x%x, st:0x%x\n", pPkt->pid, pProg->pid.id, pProg->pid.streamtype); else { fprintf(stderr, "prog is null, pmt.numpids:%d, pmt[0]:0x%x, pmt[1]:0x%x, pmt[2]:0x%x\n", pPes->pmt.numpids, pPes->pmt.pids[0].id, pPes->pmt.pids[1].id, pPes->pmt.pids[2].id); }

    if(!pProg) {

      if(parseRc == MP2TS_PARSE_OK_PAYLOADSTART) {
        // TODO: handle > 2 pids, such as priv data pid
        //LOG(X_WARNING("Unable to find program pid:0x%x"), pPkt->pid);
      }
      pPkt->idx = pPkt->len;
      return STREAM_NET_ADVFR_RC_OK;

    } else if(pProg->pid.streamtype == pPes->vid.pXcodeData->inStreamType) {

      *ppPktProgData = &pPes->vid;

    } else if(pProg->pid.streamtype == pPes->aud.pXcodeData->inStreamType) {

      *ppPktProgData = &pPes->aud;

    } else if(pPes->vid.pXcodeData->inStreamType == MP2PES_STREAMTYPE_NOTSET &&
              codectype_isVid(mp2_getCodecType(pProg->pid.streamtype))) {

      // Use video prog as PCR pid
      if(pPes->pPcrData == NULL) {
        pPes->pPcrData = &pPes->vid;
      }

      pPes->vid.pXcodeData->inStreamType = pProg->pid.streamtype;
      *ppPktProgData = &pPes->vid;

      if(pPes->vid.pOutStreamType) {
        *pPes->vid.pOutStreamType = pPes->vid.pXcodeData->inStreamType;
      } 

      if(pPes->vid.pXcodeData->piXcode && pPes->vid.pXcodeData->piXcode->vid.common.cfgDo_xcode) {
        pPes->vid.pXcodeData->piXcode->vid.common.cfgFileTypeIn = 
                  mp2_getCodecType(pPes->vid.pXcodeData->inStreamType);
      }

      //fprintf(stderr, "auto set vid pkt... stype: 0x%x out: 0x%x\n", pProg->pid.streamtype, pPes->vid.pOutStreamType ? *pPes->vid.pOutStreamType : -1);

    } else if(pPes->aud.pXcodeData->inStreamType == MP2PES_STREAMTYPE_NOTSET &&
              codectype_isAud(mp2_getCodecType(pProg->pid.streamtype))) {


      //TODO: auto-set pPes->pPcrData to audio for audio only 
      
      pPes->aud.pXcodeData->inStreamType = pProg->pid.streamtype;
      *ppPktProgData = &pPes->aud;

      if(pPes->aud.pOutStreamType) {
        //fprintf(stderr, "setting s type 0x%x -> 0x%x\n", *pPes->aud.pOutStreamType, pPes->aud.pXcodeData->inStreamType);
        *pPes->aud.pOutStreamType = pPes->aud.pXcodeData->inStreamType;
      } 

      if(pPes->aud.pXcodeData->piXcode && pPes->aud.pXcodeData->piXcode->aud.common.cfgDo_xcode) {
        pPes->aud.pXcodeData->piXcode->aud.common.cfgFileTypeIn = 
                mp2_getCodecType(pPes->aud.pXcodeData->inStreamType);
      }

      //fprintf(stderr, "auto set aud pkt... stype: 0x%x out: 0x%x\n", pProg->pid.streamtype, pPes->aud.pOutStreamType ? *pPes->aud.pOutStreamType : -1);

    } else {

      LOG(X_DEBUG("Unable to find mpeg2-ts program data for pid: 0x%x stream type: 0x%x"),
                     pPkt->pid, pProg->pid.streamtype);
      pPkt->idx = pPkt->len;
      return STREAM_NET_ADVFR_RC_OK;
    }

    //if((*ppPktProgData)->pProg == NULL) fprintf(stderr, "----setting pProg to pid:0x%x st:0x%x\n", pProg->pid.id, pProg->pid.streamtype);
    (*ppPktProgData)->pProg = pProg;

  } else {
    fprintf(stderr, "neither parseRc:%d\n", parseRc);
  }

  if(parseRc == MP2TS_PARSE_OK_PAYLOADSTART) {

    if((sz = mp2_parse_pes(pProg, &pPkt->pData[pPkt->idx], pPkt->len - pPkt->idx,
                           &pPkt->tm.qtm, &haveTm)) < 0) {
      //fprintf(stderr, "failed to parse mpeg2-ts pes packet pid:0x%x\n", pPkt->pid);
      //return STREAM_NET_ADVFR_RC_ERROR;
      pPkt->idx = pPkt->len;
      (*ppPktProgData)->curPesDecodeErr = 1;
      return STREAM_NET_ADVFR_RC_OK;
    }

/*
  //unit test to force time jump back to 0, staggered for vid , then audio
if(pProg->pid.streamtype == 0x02) {
  delmecnt0++; 
  pPkt->tm.qtm.pts+=delmecnt0_offset; 
  if(pPkt->tm.qtm.dts>0) 
    pPkt->tm.qtm.dts+=delmecnt0_offset;
  if(delmecnt0==150) 
    delmecnt0_offset = -1 * ((long long) pPkt->tm.qtm.dts ? pPkt->tm.qtm.dts : pPkt->tm.qtm.pts); 
} else if(pProg->pid.streamtype == 0x81) {
  pPkt->tm.qtm.pts+=delmecnt1_offset; 
  if(pPkt->tm.qtm.dts>0) pPkt->tm.qtm.dts+=delmecnt1_offset;
  if(delmecnt0 >=150) 
    delmecnt1++; 
  if(delmecnt1==25)
   delmecnt1_offset = -1 * ((long long) pPkt->tm.qtm.dts ? pPkt->tm.qtm.dts : pPkt->tm.qtm.pts); 
}
fprintf(stderr, "(%d,%d) prog id:0x%x stype:0x%x pts:%llu %.3f, dts:%.3f\n", delmecnt0,delmecnt1,pProg->pid.id, pProg->pid.streamtype, pPkt->tm.qtm.pts, PTSF(pPkt->tm.qtm.pts), PTSF(pPkt->tm.qtm.dts));
*/
//fprintf(stderr, "prog id:0x%x stype:0x%x pts:%llu %.3f, dts:%.3f, len:%d\n", pProg->pid.id, pProg->pid.streamtype, pPkt->tm.qtm.pts, PTSF(pPkt->tm.qtm.pts), PTSF(pPkt->tm.qtm.dts), pProg->nextPesTotLen);


    pPkt->idx += sz;

    //fprintf(stderr, "PES: 0x%x, PES {pts:%.3f, dts:%.3f} CUR {pts:%.3f}, haveTm:%d\n", pProg->pid.id, PTSF(pPkt->tm.qtm.pts), PTSF(pPkt->tm.qtm.dts), PTSF((*ppPktProgData)->curPesTm.qtm.pts), haveTm); 

    if(haveTm) {
      memcpy(&(*ppPktProgData)->curPesTm, &pPkt->tm, sizeof(MP2TS_PKT_TIMING_T));
    }
    (*ppPktProgData)->curPesDecodeErr = 0;

  } else if(parseRc == MP2TS_PARSE_OK_PAYLOADCONT) {
    //rc = pPkt->len - pPkt->idx;
//    fprintf(stderr, "cont pid:0x%x len:%d (%d - %d)\n", pPkt->pid, pPkt->len - pPkt->idx, pPkt->idx, pPkt->len);
  } else {
    //fprintf(stderr, "parseRc: %d\n", parseRc);
    //rc = 0;
    pPkt->idx = pPkt->len;
  }

  
  return rc;
}

static enum STREAM_NET_ADVFR_RC store_mp2ts_framedata(STREAM_PES_DATA_T *pData) {
  MP2TS_PKT_T pkt;
  STREAM_PES_DATA_T *pPktProgData = NULL;
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  PKTQ_EXTRADATA_T pktqxtra;
  int sz;
  int64_t ptsdelta;
  uint64_t tm0, tm1;
  enum PKTQUEUE_RC queueRc;

  do {

    sz = 0;
    if((rc = load_mp2ts_pkt(pData, &pkt, &pPktProgData)) == STREAM_NET_ADVFR_RC_OK &&
        pPktProgData) {

      sz = pkt.len - pkt.idx;

      if(pPktProgData->pProg && !pPktProgData->pProg->havePesStart) {
        if(!(pkt.flags & MP2TS_PARSE_FLAG_PESSTART)) {
          sz = 0;
        } else {
          pPktProgData->pProg->havePesStart = 1;
        }
      }
    }

  } while(rc == STREAM_NET_ADVFR_RC_OK && sz == 0);

  //
  // inactive may be set if '--novid' or '--noaud' is configured
  //
  if(pData->inactive) {
    //fprintf(stderr, "RETURNING NOCONTENT 0x%x 0x%x 0x%x v:0x%x a:0x%x\n", pData, pData->pProg, pPktProgData, &((STREAM_PES_T *)pData->pPes)->vid, &((STREAM_PES_T *)pData->pPes)->aud);
    pData->pProg = NULL;
    return STREAM_NET_ADVFR_RC_NOCONTENT;
  } else if(pPktProgData && pPktProgData->inactive) {
    pPktProgData->pProg = NULL;
    //fprintf(stderr, "RETURNING OK but inactive 0x%x 0x%x 0x%x v:0x%x a:0x%x\n", pData, pData->pProg, pPktProgData, &((STREAM_PES_T *)pData->pPes)->vid, &((STREAM_PES_T *)pData->pPes)->aud);
    return STREAM_NET_ADVFR_RC_OK;
  }

  if(sz > 0 && pPktProgData) {

    if(pPktProgData->curPesDecodeErr) {
      return rc;
    }

    pktqxtra.tm.pts = pPktProgData->curPesTm.qtm.pts;
    pktqxtra.tm.dts = pPktProgData->curPesTm.qtm.dts;
    //pktqxtra.flags = 0; // key frame flags... 
    pktqxtra.pQUserData = NULL;
    if((queueRc = pktqueue_addpartialpkt(pPktProgData->pQueue, &pkt.pData[pkt.idx], 
          pkt.len - pkt.idx, &pktqxtra, 0, 0)) != PKTQUEUE_RC_OK) {
      // Mask error, such as frame size too large, or partial overwrite of contents

      if(queueRc == PKTQUEUE_RC_SIZETOOBIG) {
        pPktProgData->curPesDecodeErr = 1;
      } 
      else if(queueRc == PKTQUEUE_RC_WOULDOVERWRITE) {

       LOG(X_WARNING("mpeg2-ts aud vid gap too large pQ[%d] rd:%d wr:%d / %d"), 
           pPktProgData->pQueue->cfg.id, pPktProgData->pQueue->idxRd, 
           pPktProgData->pQueue->idxWr, pPktProgData->pQueue->cfg.maxPkts);

        pPktProgData->curPesDecodeErr = 1;
        return STREAM_NET_ADVFR_RC_OVERWRITE;
      }

    }

    if(pPktProgData->curPesTm.qtm.pts != pPktProgData->prevPesTm.qtm.pts) {

      if(pPktProgData->numFrames == 0) {

        //
        // Set the start of frame reception time
        //
        pPktProgData->pXcodeData->frameTmStartOffset = pPktProgData->curPesTm.qtm.dts ? 
                     pPktProgData->curPesTm.qtm.dts : pPktProgData->curPesTm.qtm.pts;

        LOG(X_DEBUG("Setting PES "MP2PES_STREAMTYPE_FMT_STR" pts start offset to %.3f"), 
             MP2PES_STREAMTYPE_FMT_ARGS(pPktProgData->pXcodeData->inStreamType),
            (float)pPktProgData->pXcodeData->frameTmStartOffset/90000.0f);

      } else {
        tm0 = pPktProgData->prevPesTm.qtm.dts ? pPktProgData->prevPesTm.qtm.dts : 
              pPktProgData->prevPesTm.qtm.pts;
        tm1 = pPktProgData->curPesTm.qtm.dts ? pPktProgData->curPesTm.qtm.dts : 
              pPktProgData->curPesTm.qtm.pts;

        ptsdelta = tm1 - tm0;
        if((ptsdelta > 0 && ptsdelta > MP2TS_LARGE_PCR_JUMP_HZ) || 
           (ptsdelta < 0 && ptsdelta < MP2TS_LARGE_PCR_JUMP_HZ)) {

          LOG(X_WARNING("Large pts jump pts:%.3f dts:%.3f -> pts: %.3f dts:%.3f "
                        "(%"LL64"d %.3fsec) pid:0x%x stype:0x%x"), 
            PTSF(pPktProgData->prevPesTm.qtm.pts), PTSF(pPktProgData->prevPesTm.qtm.dts), 
            PTSF(pPktProgData->curPesTm.qtm.pts), PTSF(pPktProgData->curPesTm.qtm.dts), 
            ptsdelta, PTSF(ptsdelta), 
            pPktProgData->pProg->pid.id, pPktProgData->pXcodeData->inStreamType);

          rc = STREAM_NET_ADVFR_RC_RESET;

        } else if(tm1 < tm0) {

          LOG(X_WARNING("pts time went backwards pts: %"LL64"u -> %"LL64"u stype:0x%x"), pPktProgData->prevPesTm.qtm.pts, pPktProgData->curPesTm.qtm.pts, pData->pXcodeData->inStreamType); 
        }
          
      }

      memcpy(&pPktProgData->prevPesTm.qtm, &pPktProgData->curPesTm.qtm, 
            sizeof(pPktProgData->prevPesTm.qtm));
      pPktProgData->numFrames++;
    }
  }

  return rc; 
}

static enum STREAM_NET_ADVFR_RC stream_net_load_frame(STREAM_PES_DATA_T *pData) {
  STREAM_PES_T *pPes = pData->pPes;
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;
  uint32_t numFrames = pData->numFrames;

  //fprintf(stderr, "load_frame vid:%d aud:%d\n", pPes->vid.inactive, pPes->aud.inactive);

  if(pPes->pPcrData == &pPes->vid && (pPes->vid.inactive ||
     (pPes->aud.pProg && pPes->pmt.pcrpid == pPes->aud.pProg->pid.id))) {
    LOG(X_DEBUG("Changing PCR pid from vid to aud"));
    pPes->pPcrData = &pPes->aud;
  }

  //if(!pPes->pPcrData) {
  //  LOG(X_ERROR("Cannot advance frame because pcr data stream not set"));
  //  return STREAM_NET_ADVFR_RC_ERROR;
  //}

  // 
  // Use the pcr program pid to scan the transport stream for new frame content
  // 
  while(1) {

    // Check if only one program stream is actually present
    //if(pData == &pPes->vid && (!pPes->vid.pProg || pPes->vid.inactive) && pPes->aud.pProg) {
    if(pData == &pPes->vid && ((pPes->pmt.prognum > 0 && !pPes->pmt_haveVid) || pPes->vid.inactive)) {
      //fprintf(stderr, "NOCONTENT... setting vid prog to null\n");
      pPes->vid.pProg = NULL;
      return STREAM_NET_ADVFR_RC_NOCONTENT;
    } else if(pData == &pPes->aud && ((pPes->pmt.prognum > 0 && !pPes->pmt_haveAud) || pPes->aud.inactive)) {
    //} else if(pData == &pPes->aud && (!pPes->aud.pProg || pPes->aud.inactive) && pPes->vid.pProg) {
      //fprintf(stderr, "NOCONTENT... setting aud prog to null\n");
      pPes->aud.pProg = NULL;
      return STREAM_NET_ADVFR_RC_NOCONTENT;
    }


    //if(pData->inactive) {
    //  pPes->aud.pProg=NULL;
    //}
    //fprintf(stderr, "loadFr %s numF:%d/%d v:%d a:%d v:0x%x(%d) a:0x%x(%d) pcr:%s datasrc:0x%x\n", pData == &pPes->vid ? "vid" : "aud", pData->numFrames, numFrames, pPes->vid.numFrames, pPes->aud.numFrames, pPes->vid.pProg, pPes->vid.inactive, pPes->aud.pProg, pPes->aud.inactive, pPes->pPcrData==&pPes->vid ? "vid" : (pPes->pPcrData==&pPes->aud ? "aud" : "none"), pData->pDataSrc);


    //
    // Check if the requested program is not active in the PMT
    //
    //if(!pData->pProg && pDataComplement->pProg && 
    //   pDataComplement->numFrames > numFramesComplement) {
    //  fprintf(stderr, "NOT AVAIL\n");
    //  return STREAM_NET_ADVFR_RC_NOTCONTENT;
    //}

    if(pData->numFrames > numFrames &&
       (((!pPes->vid.pProg  || pPes->vid.inactive) || pPes->vid.numFrames >= 2) && 
        ((!pPes->aud.pProg || pPes->aud.inactive) || pPes->aud.numFrames >= 2))) {
      break;
    }

    if((rc = store_mp2ts_framedata(pPes->pPcrData)) != STREAM_NET_ADVFR_RC_OK) {
      break;
    }

    //if(pPes->vid.numFrames < 2 || pPes->aud.numFrames < 2) {
      // first time pre-load
    //}

  }

  return rc;
}

static int stream_net_reset(STREAM_PES_T *pPes) {
  STREAM_PES_DATA_T *pData;
  unsigned int idx = 0;

  for(idx = 0; idx < 2; idx++) {

    if(idx == 0) {
      pData = &pPes->vid;
    } else {
      pData = &pPes->aud;
    }

    pData->pXcodeData->inStreamType = MP2PES_STREAMTYPE_NOTSET;
    if(pData->pProg) {
      pData->pProg->havePesStart = 0;
      pData->pProg->idxPes = 0;
      pData->pProg->idxInPes = 0;
      pData->pProg->nextPesTotLen = 0;
      pData->pProg->prevPesTotLen = 0;
      pData->pProg->prevCidx = 0;
    }

    if(pData->pQueue) {
      LOG(X_DEBUG("MPEG-2 TS PES input stream reset (net_pes) reset Q[%d]"), pData->pQueue->cfg.id);
      pktqueue_reset(pData->pQueue, 1);
    }

    xcode_resetdata(pData->pXcodeData);

    pData->numFrames = 0;
    memset(&pData->curPesTm, 0, sizeof(pData->curPesTm));
    memset(&pData->prevPesTm, 0, sizeof(pData->prevPesTm));
    pData->pProg = NULL;
    if(pData->pHaveSeqStart) {
      *pData->pHaveSeqStart = 0;
    }

  }

  return 0;
}

enum STREAM_NET_ADVFR_RC stream_net_pes_advanceFrame(STREAM_NET_ADVFR_DATA_T *pArg) {

  STREAM_PES_DATA_T *pData = (STREAM_PES_DATA_T *) pArg->pArgIn;
  STREAM_PES_T *pPes = (STREAM_PES_T *) pData->pPes;
  const PKTQUEUE_PKT_T *pQueuePkt;
  enum STREAM_NET_ADVFR_RC rc = STREAM_NET_ADVFR_RC_OK;

  //fprintf(stderr, "ADV FR FOR st:0x%x\n", pPes->vid.pProg ? pPes->vid.pProg->pid.streamtype : 0);

  if(pArg->pkeyframeIn) {
    *pArg->pkeyframeIn = 0;
  }

  if(pData == &pPes->vid && (!pPes->vid.pProg || pPes->vid.inactive) && pPes->aud.pProg) {
    pData = &pPes->aud;
  } 

  if( !pktqueue_havepkt(pData->pQueue)) {

    if((rc = stream_net_load_frame(pData)) != STREAM_NET_ADVFR_RC_OK) {
      if(rc == STREAM_NET_ADVFR_RC_NEWPROG || rc == STREAM_NET_ADVFR_RC_RESET) {
        //fprintf(stderr, "load_frame rc:%d , will reset stream_net\n", rc);
        stream_net_reset(pData->pPes);
      } else if(rc == STREAM_NET_ADVFR_RC_OVERWRITE) {
        //fprintf(stderr, "no content for input streamtype: 0x%x, will reset stream_net rc:%d\n", pData->pXcodeData->inStreamType, rc);
        stream_net_reset(pData->pPes);
        rc = STREAM_NET_ADVFR_RC_RESET;
      }

      return rc;
    }
  }

  //
  // Read a frame from the queue
  //
  if((pQueuePkt = pktqueue_readpktdirect(pData->pQueue))) {

    //
    // Avoid memcpy of the frame data
    //
    if(pktqueue_swapreadslots(pData->pQueue, 
                              &pData->pXcodeData->curFrame.pSwappedSlot) < 0) {
      LOG(X_ERROR("Failed to swap slot in queue id:%d"), pData->pQueue->cfg.id);
      pktqueue_readpktdirect_done(pData->pQueue);
      return STREAM_NET_ADVFR_RC_ERROR;
    } else {
      pQueuePkt = pData->pXcodeData->curFrame.pSwappedSlot;
    }

    // take not if curFrame.idx upon subsequent reads to ensure data
    // was not overwritten by subsequent frame data
    memcpy(&pData->pXcodeData->curFrame.pkt, pQueuePkt, sizeof(pData->pXcodeData->curFrame.pkt));
    pData->pXcodeData->curFrame.idxReadInFrame = 0;
    pData->pXcodeData->curFrame.idxReadFrame = pData->pQueue->idxRd;

    //fprintf(stderr, "READ PKT FROM Q pts:%.3f dts:%.3f\n", PTSF(pData->pXcodeData->curFrame.pkt.xtra.tm.pts), PTSF(pData->pXcodeData->curFrame.pkt.xtra.tm.dts));

    // Do not use PKTQUEUE_T pkt contents directly to allow for any
    // prebuf contents such as SPS / PPS packaged w/ each I-frame
    pData->pXcodeData->curFrame.pData = pData->pXcodeData->curFrame.pkt.pData;
    pData->pXcodeData->curFrame.lenData = pData->pXcodeData->curFrame.pkt.len;
    pArg->isvid = (pData == &pPes->vid) ? 1 : 0;

    if(pArg->plen) {
      *pArg->plen = pData->pXcodeData->curFrame.lenData;
    }

  } else {
    LOG(X_ERROR("Unable to get packet from read queue for stype: 0x%x"), 
        pData->pXcodeData->inStreamType);
    rc = STREAM_NET_ADVFR_RC_ERROR;
  }

  pktqueue_readpktdirect_done(pData->pQueue);


  if(pArg->pPts) {
    *pArg->pPts = xcode_getFrameTm(pData->pXcodeData, 0, 0);
    //fprintf(stderr, "curF: pts:%.3f dts:%.3f start:%.3f, fr:%.3f (%llu)\n", PTSF(pData->pXcodeData->curFrame.pkt.xtra.tm.pts), PTSF(pData->pXcodeData->curFrame.pkt.xtra.tm.dts), PTSF(pData->pXcodeData->frameTmStartOffset), PTSF(*pArg->pPts), *pArg->pPts);

    //if(pData == &pPes->aud) {
    //  if(*pArg->pPts < (uint64_t) pData->seqStartTm90Khz) {
    //    fprintf(stderr, "setting audio frame length to 0 orig len:%d %.3f < %.3f\n", *pArg->plen,(double)*pArg->pPts/90000.0f, (double)pData->seqStartTm90Khz/90000.0f);
    //    *pArg->plen = 0;
    //  }
    //}

  }

  //TODO: keyframe identification not set from TS content 
  //if(pArg->pkeyframe) {
  //  *pArg->pkeyframe = pData->pXcodeData->curFrame.keyframe;
  //avc_dumpHex(stderr, pData->pXcodeData->curFrame.pData, 16, 0);
  //}

  //fprintf(stderr, "stream_net_pes returning:%d len:%u(%u)\n", rc, pData->pXcodeData->curFrame.lenData, *pArg->plen);
  return rc;
}

int64_t stream_net_pes_frameDecodeHzOffset(void *pArg) {
  STREAM_PES_DATA_T *pData = (STREAM_PES_DATA_T *) pArg;
  int64_t delta = 0;
  
  if(pData->pXcodeData->curFrame.pkt.xtra.tm.dts > 0) {
    delta = pData->pXcodeData->curFrame.pkt.xtra.tm.pts - 
            pData->pXcodeData->curFrame.pkt.xtra.tm.dts;
  }

  return delta;
}

MP4_MDAT_CONTENT_NODE_T *stream_net_pes_getNextSlice(void *pArg) {
  static MP4_MDAT_CONTENT_NODE_T content;
  // return dummy to avoid NULL
  return &content;
}

int stream_net_pes_szNextSlice(void *pArg) {
  //fprintf(stderr, "szNextSlice\n");
  // Not implemented for mpeg2-ts
  return 0;
}

int stream_net_pes_haveMoreData(void *pArg) {
  STREAM_PES_DATA_T *pData = (STREAM_PES_DATA_T *) pArg;
  const STREAM_DATASRC_T *pDataSrc = NULL; 
  STREAM_PES_T *pPes = (STREAM_PES_T *) pData->pPes;
  int rc = 0;

  // 
  // If not reading from a file (live capture) then always
  // assume more source data is available
  // 
  if(pPes->pContainer == NULL) {

    if(pData->inactive) {
      return 0;
    }

    if(pData == &pPes->vid  && !pPes->pmt_haveVid) {
      return 0;
    } else if(pData == &pPes->aud  && !pPes->pmt_haveAud) {
      return 0;
    }

/*
    if(pData == &pPes->vid) {
      //if(!pData->pProg && pPes->aud.pProg) {
        return 0;
      }
    } else {
      //if(!pData->pProg && pPes->vid.pProg) {
        return 0;
      }
    }
*/

    return 1;
  }

  // TODO: check for end
  if(((STREAM_PES_T *) pData->pPes)->pPcrData) {
    //pDataSrc = ((STREAM_PES_T *) pData->pPes)->pPcrData->pDataSrc;
    pDataSrc = ((STREAM_PES_T *) pData->pPes)->pDataSrc;

    if(pDataSrc->cbHaveData(pDataSrc->pCbData)) {
      rc = 1;
    } else if(pData->pQueue && pktqueue_havepkt(pData->pQueue)) {
      rc = 2;
    }
  }
  
  return rc;
}


int stream_net_pes_haveNextSlice(void *pArg) {
  return 0;
}


int stream_net_pes_getReadSliceBytes(void *pArg) {
  return 0;
}


int stream_net_pes_getUnreadSliceBytes(void *pArg) {
  STREAM_PES_DATA_T *pData = (STREAM_PES_DATA_T *) pArg;
  uint32_t szSlice;

  if(pData->numFrames < 1) {
    szSlice = 0;
  } else {
    szSlice = pData->pXcodeData->curFrame.lenData - pData->pXcodeData->curFrame.idxReadInFrame;
  }

  return szSlice;
}


int stream_net_pes_getSliceBytes(void *pArg, unsigned char *pBuf, unsigned int len) {
  STREAM_PES_DATA_T *pData = (STREAM_PES_DATA_T *) pArg;
  int rc = 0;
  int szread = 0;

  pktqueue_lock(pData->pQueue, 1);

  // Check to make sure the contents being partially read have not been
  // overwritten by a subsequent pktqueue write operation
  if(pData->pQueue->pkts[pData->pXcodeData->curFrame.idxReadFrame].idx == 
                        pData->pXcodeData->curFrame.pkt.idx) {

    if((szread = pData->pXcodeData->curFrame.lenData - 
                 pData->pXcodeData->curFrame.idxReadInFrame) < 0) {
      LOG(X_ERROR("packet queue read invalid index [%d + %d / %d]"), 
        pData->pXcodeData->curFrame.idxReadInFrame, len, pData->pXcodeData->curFrame.lenData);
      szread = -1;
      rc = -1;
    } else if((int) len > szread) {
      LOG(X_WARNING("packet queue read invalid length idx[%d + %d / %d]"), 
                 pData->pXcodeData->curFrame.idxReadInFrame, len, 
                 pData->pXcodeData->curFrame.lenData);
      len = szread;
    } else if(szread > (int) len) {
      szread = len;
    }

    if(rc == 0) {
      memcpy(pBuf, &pData->pXcodeData->curFrame.pData[pData->pXcodeData->curFrame.idxReadInFrame], szread);
      pData->pXcodeData->curFrame.idxReadInFrame += szread;
      if(pData->pXcodeData->curFrame.idxReadInFrame >= pData->pXcodeData->curFrame.lenData) {
      }
    }
  
  } else {
    LOG(X_WARNING("packet queue[id:%d] (sz:%d, rd:%d, wr:%d) read contents have been "
                  "overwritten during partial read"),
                  pData->pQueue->cfg.id, pData->pQueue->cfg.maxPkts, 
                  pData->pQueue->idxRd, pData->pQueue->idxWr); 
    szread = -1;
  } 

  pktqueue_lock(pData->pQueue, 0);

  return szread;
}


float stream_net_pes_jump(STREAM_PES_DATA_T *pData, float fStartSec, int syncframe) {

  double fOffsetActual;
  MP2TS_CONTAINER_T *pMp2ts = NULL;
  MP2TS_STREAM_CHANGE_T *pmtChangesPrev = NULL;

  if(fStartSec >= 0 && pData && pData->pPes && ((STREAM_PES_T *)pData->pPes)->pContainer) {
    pMp2ts = ((STREAM_PES_T *)pData->pPes)->pContainer;
  } else {
    return -1;
  }

  if(!(pmtChangesPrev = mp2ts_jump_file(pMp2ts, fStartSec, &fOffsetActual))) {
    LOG(X_WARNING("mepg2-ts pes stream jump to %.2f out of range %.2f"),
                   fStartSec, (double)pMp2ts->durationHz / 90000.0f);
    return -1;
  }

  fOffsetActual += ((double) pmtChangesPrev->startHz / 90000.0f);

  stream_net_reset(pData->pPes);

  LOG(X_DEBUG("mpeg2-ts pes stream playout moved to  actual:%.1fs (%.1fs) file:0x%"LL64"x / 0x%"LL64"x"), 
               fOffsetActual, fStartSec, pMp2ts->pStream->offset, pMp2ts->pStream->size);

  return (float) fOffsetActual;
}

void stream_net_pes_init(STREAM_NET_T *pNet) {
  pNet->type = STREAM_NET_TYPE_UNKNOWN;
  pNet->cbAdvanceFrame = stream_net_pes_advanceFrame;
  pNet->cbFrameDecodeHzOffset = stream_net_pes_frameDecodeHzOffset;
  pNet->cbGetNextFrameSlice = stream_net_pes_getNextSlice;
  pNet->cbHaveMoreData = stream_net_pes_haveMoreData;
  pNet->cbHaveNextFrameSlice = stream_net_pes_haveNextSlice;
  pNet->cbSzNextFrameSlice = stream_net_pes_szNextSlice;
  pNet->cbSzRemainingInSlice = stream_net_pes_getUnreadSliceBytes;
  pNet->cbSzCopiedInSlice = stream_net_pes_getReadSliceBytes;
  pNet->cbCopyData = stream_net_pes_getSliceBytes;

}

int stream_mp2ts_pes_cbgetfiledata(void *p, unsigned char *pData, unsigned int len,
                                   PKTQ_EXTRADATA_T *pXtra, 
                                   unsigned int *pNumOverwritten) {

  struct stat st;
  struct timeval tv, tv0;
  STREAM_PES_FILE_CBDATA_T *pCbData = (STREAM_PES_FILE_CBDATA_T *) p;

  //fprintf(stderr, "stream_mp2ts_pes_cbgetfiledata %d offset:%llu / %llu\n", len, pCbData->pFile->offset, pCbData->pFile->size);

  if(!pCbData->liveFile ||
     pCbData->pFile->offset + len <= pCbData->pFile->size) {

    if(ReadFileStream(pCbData->pFile, pData, len) != len) {
      return -1;
    }
    return len;
  } 

#define PES_LIVEFILE_READ_TMT_MS   2000

  //
  // If the file is live (currently being asynchronously appended)
  // Try to read for up to PES_LIVEFILE_READ_TMT_MS duration
  //
  gettimeofday(&tv0, NULL);

  do {

    //fprintf(stderr, "stream_mp2ts_pes_cbgetfiledata sleeping 100ms\n");
    usleep(100000);
    gettimeofday(&tv, NULL);

    if(fileops_stat(pCbData->pFile->filename, &st) != 0) {
      LOG(X_ERROR("Failed to stat %s"), pCbData->pFile->filename);
      return -1;
    } else if(pCbData->pFile->size != st.st_size) {
      pCbData->pFile->size = st.st_size;
      if(pCbData->pFile->offset + len <= pCbData->pFile->size) {
        if(ReadFileStream(pCbData->pFile, pData, len) != len) {
          return -1;
        }
        return len;
      }
    }

  } while((((tv.tv_sec - tv0.tv_sec) *1000) + ((tv.tv_usec - tv0.tv_usec) /1000)) <
          PES_LIVEFILE_READ_TMT_MS);

  LOG(X_ERROR("Timeout waiting for append of live file %s"), pCbData->pFile->filename);
  pCbData->numLiveFileRdErr++;

  return -1;
}

int stream_mp2ts_pes_cbhavefiledata(void *p) {

  STREAM_PES_FILE_CBDATA_T *pCbData = (STREAM_PES_FILE_CBDATA_T *) p;

  if((pCbData->liveFile && pCbData->numLiveFileRdErr == 0) ||
     pCbData->pFile->offset < pCbData->pFile->size) {
    return 1;
  }

  LOG(X_DEBUG("stream_mp2ts_pes_cbhavefiledata: 0"));
  return 0;
}

int stream_mp2ts_pes_cbresetfiledata(void *p) {
  STREAM_PES_FILE_CBDATA_T *pCbData = (STREAM_PES_FILE_CBDATA_T *) p;

  if(SeekMediaFile(pCbData->pFile, 0, SEEK_SET) < 0) {
    return -1;
  }

  return 0;
}

#endif // VSX_HAVE_STREAMER
