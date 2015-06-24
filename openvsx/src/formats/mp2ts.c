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

#if defined(DEBUG_PES) || defined(DEBUG_M2TPKTS)

static int spat;
static int spmt;
static int spes;

#endif // DEBUG_PES



#define MAX_OFFSET_NODE_DELTA_MS        1000
#define MAX_OFFSET_NODES               (3600 * 3 * (1000/MAX_OFFSET_NODE_DELTA_MS)) 


static int read_container(MP2TS_CONTAINER_T *pMp2ts, unsigned int maxframes, const int *pabort);
static int mp2ts_writemeta(MP2TS_CONTAINER_T *pMp2ts);


MP2TS_CONTAINER_T *mp2ts_open(const char *path, const int *pabort, 
                              unsigned int maxframes, int writemeta) {
  FILE_STREAM_T *pStreamIn = NULL;
  MP2TS_CONTAINER_T *pMp2ts = NULL;

  if((pStreamIn = (FILE_STREAM_T *) avc_calloc(1, sizeof(FILE_STREAM_T))) == NULL) {
    return NULL;
  }

  if(OpenMediaReadOnly(pStreamIn, path) < 0) {
    free(pStreamIn);
    return NULL;
  }

  if((pMp2ts = (MP2TS_CONTAINER_T *) avc_calloc(1, sizeof(MP2TS_CONTAINER_T))) == NULL) {
    CloseMediaFile(pStreamIn);
    free(pStreamIn);
    return NULL;
  }

  pMp2ts->serializedver = MP2TS_CONTAINER_SERIALIZED_VER;
  pMp2ts->wordsz = sizeof(int);
  pMp2ts->szChangeElem = sizeof(MP2TS_STREAM_CHANGE_T);
  pMp2ts->pStream = pStreamIn;
  pMp2ts->size = pMp2ts->pStream->size;

  if(read_container(pMp2ts, maxframes, pabort) < 0) {
    mp2ts_close(pMp2ts);
    return NULL;
  }

  SeekMediaFile(pMp2ts->pStream, 0, SEEK_SET);

  if(writemeta) {
    mp2ts_writemeta(pMp2ts);
  }

  return pMp2ts;
}

void mp2ts_close(MP2TS_CONTAINER_T *pMp2ts) {
  MP2TS_STREAM_CHANGE_T  *pPmtChange = NULL;

  if(pMp2ts) {

    if(pMp2ts->pStream) {
      CloseMediaFile(pMp2ts->pStream);
      free(pMp2ts->pStream);
    }

    while(pMp2ts->pmtChanges) {
      pPmtChange = pMp2ts->pmtChanges->pnext;
      if(pMp2ts->pmtChanges->pPcrOffsets) {
        free(pMp2ts->pmtChanges->pPcrOffsets);
      }
      free(pMp2ts->pmtChanges);
      pMp2ts->pmtChanges = pPmtChange;
    }
  
    free(pMp2ts);
  }
}


typedef struct MP2TS_STREAM_ADD_CBDATA {
  MP2_PES_DESCR_T            *pMp2;
  MP2TS_CONTAINER_T          *pMp2ts; 
  FILE_OFFSET_T               fileOffset;
  unsigned int                idxPcrOffset;
  OFFSET_NODE_T              *pPcrOffsets;
} MP2TS_STREAM_ADD_CBDATA_T;


static int memcpy_pcroffsets(MP2TS_STREAM_CHANGE_T *pmtChangesLast,
                          OFFSET_NODE_T *pPcrOffsets,
                          unsigned int numPcrOffsets) {

  if(pmtChangesLast && numPcrOffsets > 0) {
    if(!(pmtChangesLast->pPcrOffsets = (OFFSET_NODE_T *)
                              avc_calloc(numPcrOffsets, sizeof(OFFSET_NODE_T)))) {
        return -1;
    }
    memcpy(pmtChangesLast->pPcrOffsets, pPcrOffsets,
           numPcrOffsets * sizeof(OFFSET_NODE_T));
    pmtChangesLast->numPcrOffsets = numPcrOffsets;
  }
  return 0;
}

int newPcrOffset(MP2TS_STREAM_ADD_CBDATA_T *pCbData) {
  MP2TS_STREAM_CHANGE_T *pMp2Change = NULL;

#ifdef DEBUG_PCR
  fprintf(stderr, "creating new pcr offset node\n");
#endif // DEBUG_PCR

  if(!(pMp2Change = (MP2TS_STREAM_CHANGE_T *) avc_calloc(1, sizeof(MP2TS_STREAM_CHANGE_T)))) {
    return -1;
  }

  memcpy_pcroffsets(pCbData->pMp2ts->pmtChangesLast,
                    pCbData->pPcrOffsets, pCbData->idxPcrOffset);
  pCbData->idxPcrOffset = 0;

  if(pCbData->pMp2ts->pmtChangesLast) {
    pCbData->pMp2ts->pmtChangesLast->pnext = pMp2Change;
    pCbData->pMp2ts->pmtChangesLast = pCbData->pMp2ts->pmtChangesLast->pnext;
  } else {
    pCbData->pMp2ts->pmtChanges = pCbData->pMp2ts->pmtChangesLast = pMp2Change;
  }
  pCbData->pMp2ts->numPmtChanges++;

  pCbData->pMp2ts->pmtChangesLast->pmtVersion = pCbData->pMp2->pmt.version; 
  pCbData->pMp2ts->pmtChangesLast->fileOffsetStart = pCbData->fileOffset;

//fprintf(stderr, "new pcr change %u\n", pCbData->pMp2ts->pmtChangesLast->vidProp.resH);

  return 0;
} 

static int onProgAdd(void *pArg, const MP2_PID_T *pPid) {

  MP2TS_STREAM_ADD_CBDATA_T *pCbData = (MP2TS_STREAM_ADD_CBDATA_T *) pArg;
  //MP2TS_STREAM_CHANGE_T *pMp2Change = NULL;
  unsigned int idx;


  //fprintf(stderr, "0x%x\n", pCbData->pMp2->pmt.pcrpid);

  //if(pCbData->pMp2->pmt.pcrpid != pPid->id) {
    //fprintf(stderr, "not pcr pid: 0x%x\n", pPid->id);
  //  return 0;
  //}

#ifdef DEBUG_PCR
  fprintf(stderr, "prog add pcrpid:0x%x streamType: 0x%x\n", pPid->id, pPid->streamtype);
#endif // DEBUG_PCR

  if(pCbData->pMp2ts->pmtChanges == NULL ||
     pCbData->pMp2ts->pmtChangesLast->pmtVersion != pCbData->pMp2->pmt.version) {

#ifdef DEBUG_PCR
fprintf(stderr, "pmt %u -> %u\n",pCbData->pMp2ts->pmtChanges ?pCbData->pMp2ts->pmtChangesLast->pmtVersion:0,pCbData->pMp2->pmt.version);
#endif // DEBUG_PCR

    if(newPcrOffset(pCbData) != 0) {
      return -1;
    }

  }

  if(pCbData->pMp2->pmt.pcrpid == pPid->id) {
    pCbData->pMp2ts->pmtChangesLast->startHz = pCbData->pMp2ts->durationHz;
  }

  for(idx = 0; idx < sizeof(pCbData->pMp2ts->pmtChangesLast->pidDescr)/
                     sizeof(pCbData->pMp2ts->pmtChangesLast->pidDescr[0]); idx++) {
    if(pCbData->pMp2ts->pmtChangesLast->pidDescr[idx].pid.id == 0) {
      pCbData->pMp2ts->pmtChangesLast->pidDescr[idx].pid.id = pPid->id;
      pCbData->pMp2ts->pmtChangesLast->pidDescr[idx].pid.streamtype = pPid->streamtype;

      // duplicate info for continuity tracking
      memset(&pCbData->pMp2->progs[idx], 0, sizeof(pCbData->pMp2->progs[idx]));
      pCbData->pMp2->progs[idx].pid.id = pPid->id;
      pCbData->pMp2->progs[idx].pid.streamtype = pPid->streamtype;
      pCbData->pMp2ts->pmtChangesLast->pidDescr[idx].numPes = 0;

      break;
    }
  }


  return 0;
}

static int on_mp2ts_pes_ac3(MP2TS_PKT_T *pPkt, const MP2_PES_PROG_T *pProg, 
                            MP2TS_PID_DESCR_T *pPidDescr) {

  if(pPkt->len < 4) {
    return -1; 
  }

  pPidDescr->u.audProp.mediaType = MEDIA_FILE_TYPE_AC3;

  return 0;
}

static int on_mp2ts_pes_mpg2(MP2TS_PKT_T *pPkt, const MP2_PES_PROG_T *pProg, 
                       MP2TS_PID_DESCR_T *pPidDescr) {
  MPEG2_SEQ_HDR_T seqHdr;

  if(pPkt->len < 4) {
    return -1; 
  }

  pPidDescr->u.vidProp.mediaType = MEDIA_FILE_TYPE_H262;

  if(pPidDescr->u.vidProp.resH > 0 && pPidDescr->u.vidProp.resV > 0 && 
     pPidDescr->u.vidProp.fps > 0) {
    return 0;
  }

  //fprintf(stderr, "h262 pid: 0x%x streamid:%u idx:%d\n", pProg->pid.id, pProg->streamId, pPkt->idx);

  //fprintf(stderr, "0x%x 0x%x 0x%x 0x%x\n", pPkt->pData[pPkt->idx], pPkt->pData[pPkt->idx+1], pPkt->pData[pPkt->idx+2], pPkt->pData[pPkt->idx+3]); 

  if(mpg2_check_seq_hdr_start(&pPkt->pData[pPkt->idx], pPkt->len - pPkt->idx) &&
     mpg2_parse_seq_hdr(&pPkt->pData[pPkt->idx + 4], pPkt->len - pPkt->idx - 4,
                         &seqHdr) == 0) {
    pPidDescr->u.vidProp.resH = seqHdr.hpx;
    pPidDescr->u.vidProp.resV = seqHdr.vpx;
    pPidDescr->u.vidProp.fps = mpg2_get_fps(&seqHdr, NULL, NULL);

//fprintf(stderr, "h262 pid: 0x%x %u %f\n", pPkt->pid, pPidDescr->u.vidProp.resH, pPidDescr->u.vidProp.fps);

  }

  return 0;
}

static int on_mp2ts_pes_h264(MP2TS_PKT_T *pPkt, const MP2_PES_PROG_T *pProg,
                             MP2TS_PID_DESCR_T *pPidDescr) {

  //const unsigned int idx0 = pPkt->idx + 6;
  if(pPkt->len < 4) {
    return -1;
  }

  pPidDescr->u.vidProp.mediaType = MEDIA_FILE_TYPE_H264b;

#if defined(DEBUG_PES)

  avc_dumpHex(stderr, &pPkt->pData[pPkt->idx], 96, 1);

  //fprintf(stderr, "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", 
  //        pPkt->pData[idx0], pPkt->pData[idx0+1],
  //        pPkt->pData[idx0+2], pPkt->pData[idx0+3],
  //        pPkt->pData[idx0+4], pPkt->pData[idx0+5]
  //         );

#endif // DEBUG_PES

  return 0;
}

static int on_mp2ts_pes_aac(MP2TS_PKT_T *pPkt, const MP2_PES_PROG_T *pProg, 
                            MP2TS_PID_DESCR_T *pPidDescr) {
  //int idx;

  if(pPkt->len < 4) {
    return -1; 
  }

  pPidDescr->u.audProp.mediaType = MEDIA_FILE_TYPE_AAC_ADTS;

  //fprintf(stderr, "%d %d\n", pPkt->len, pPkt->idx);
  //if((idx = mp2_parse_pes(pProg, &pPkt->pData[4], pPkt->len - 4, NULL, NULL)) >= 0) {
  //  fprintf(stderr, "pes len: %d\n", pProg->nextPesTotLen);
  //}

  //fprintf(stderr, "pes aac len:%d\n", pPkt->len);
  //fprintf(stderr, "0x%x 0x%x 0x%x 0x%x\n", 
  //       pPkt->pData[pPkt->idx], pPkt->pData[pPkt->idx+1],
  //       pPkt->pData[pPkt->idx+2], pPkt->pData[pPkt->idx+3]);

  return 0;
}

static int on_mp2ts_pes_mp2_l2_aud(MP2TS_PKT_T *pPkt, const MP2_PES_PROG_T *pProg, 
                                   MP2TS_PID_DESCR_T *pPidDescr) {
  if(pPkt->len < 4) {
    return -1; 
  }

  pPidDescr->u.audProp.mediaType = MEDIA_FILE_TYPE_MPA_L2;

  return 0;
}

static int on_mp2ts_pes_mp2_l3_aud(MP2TS_PKT_T *pPkt, const MP2_PES_PROG_T *pProg, 
                                   MP2TS_PID_DESCR_T *pPidDescr) {
  if(pPkt->len < 4) {
    return -1; 
  }

  pPidDescr->u.audProp.mediaType = MEDIA_FILE_TYPE_MPA_L3;

  return 0;
}

static int on_mp2ts_pes_mpg4v(MP2TS_PKT_T *pPkt, const MP2_PES_PROG_T *pProg,
                            MP2TS_PID_DESCR_T *pPidDescr) {

  if(pPkt->len < 4) {
    return -1;
  }

  pPidDescr->u.audProp.mediaType = MEDIA_FILE_TYPE_MP4V;

  return 0;
}


static int on_mp2ts_pes(MP2TS_PKT_T *pPkt, const MP2_PES_PROG_T *pProg, 
                        MP2TS_PID_DESCR_T *pPidDescr) {

  uint16_t payloadlen;
  uint8_t peshdrlen;
#ifdef DEBUG_PES
  int hdr;
  uint64_t tm = 0;
#endif // DEBUG_PES

  if(pPkt->idx + 9 >= pPkt->len) {
    return -1;
  }

  if(!(pPkt->pData[pPkt->idx] == 0x00 && pPkt->pData[pPkt->idx + 1] == 0x00 && 
       pPkt->pData[pPkt->idx + 2] == 0x01)) {
    LOG(X_ERROR("Invalid MPEG-2 TS PES header 0x%x 0x%x 0x%x"), 
       pPkt->pData[pPkt->idx], pPkt->pData[pPkt->idx + 1], pPkt->pData[pPkt->idx + 2]);
    return -1;
  }

  //
  // 8 bit stream id
  //
  pPidDescr->streamId = pPkt->pData[pPkt->idx + 3];

  //
  // 16 bit payload length
  //
  payloadlen = ntohs(*((uint16_t *) &pPkt->pData[pPkt->idx + 4]));

  //
  // 16 bit flags
  //

  //
  // 8 bit header length
  //
  peshdrlen = pPkt->pData[pPkt->idx + 8];  
//fprintf(stderr, "PES streamId: 0x%x 0x%x\n",  pPkt->pData[pPkt->idx + 3], pPidDescr->streamId);

#ifdef DEBUG_PES
  hdr = pPkt->pData[pPkt->idx + 7]; 
 if(hdr & 0x80) {
      pPkt->tm.qtm.pts = mp2_parse_pts(&pPkt->pData[pPkt->idx + 9]);
      pPkt->flags |= MP2TS_PARSE_FLAG_HAVEPTS;
    if(pPkt->idx + 9 + 5 < pPkt->len && (hdr & 0x40)) {
      pPkt->tm.qtm.dts = mp2_parse_pts(&pPkt->pData[pPkt->idx + 9 + 5]);
      pPkt->flags |= MP2TS_PARSE_FLAG_HAVEDTS;
    }
  }
  
fprintf(stderr, "pes[%d]:0x%x, st:0x%x, sid:0x%x", spes, pPkt->pid, pPidDescr->pid.streamtype, pPidDescr->streamId);
if(pPkt->flags & MP2TS_PARSE_FLAG_HAVEPTS) {

  if(pPkt->flags & MP2TS_PARSE_FLAG_HAVEDTS) 
    tm = pPkt->tm.qtm.dts;
  else
    tm = pPkt->tm.qtm.pts;
   
  fprintf(stderr, ", pts:%llu (%.3f)", pPkt->tm.qtm.pts, (double)pPkt->tm.qtm.pts/90000.0f);
  if(pPidDescr->prev_tm != 0) fprintf(stderr, " (%lld)", tm - pPidDescr->prev_tm);
}
if(pPkt->flags & MP2TS_PARSE_FLAG_HAVEDTS) {
  fprintf(stderr, ", dts:%llu (%.3f)", pPkt->tm.qtm.dts, (double)pPkt->tm.qtm.dts/90000.0f);
  //if(pPidDescr->prev_pts != 0) fprintf(stderr, " (%lld)", pPkt->dts-pPidDescr->prev_pts);
}

fprintf(stderr, " peshdrlen:%u, payload:%d\n", peshdrlen, payloadlen);

if(pPkt->flags & MP2TS_PARSE_FLAG_HAVEPTS) {
  pPidDescr->prev_tm = tm;
  pPidDescr->prev_dts = pPkt->tm.qtm.dts;
  pPidDescr->prev_pts = pPkt->tm.qtm.pts;
}
#endif // DEBUG_PES

  pPkt->idx += (9 + peshdrlen);

  if(pPkt->idx >= pPkt->len) {
    return -1;
  }
  //fprintf(stderr, "streamtype:0x%x\n", pProg->pid.streamtype);
  switch(pProg->pid.streamtype) {
    case MP2PES_STREAM_VIDEO_H262:
      on_mp2ts_pes_mpg2(pPkt, pProg, pPidDescr);
      break;
    case MP2PES_STREAM_AUDIO_AC3:
      on_mp2ts_pes_ac3(pPkt, pProg, pPidDescr);
      break;
    case MP2PES_STREAM_VIDEO_H264:
      on_mp2ts_pes_h264(pPkt, pProg, pPidDescr);
      break;
    case MP2PES_STREAM_VIDEO_MP4V:
      on_mp2ts_pes_mpg4v(pPkt, pProg, pPidDescr);
      break;
    case MP2PES_STREAM_AUDIO_AACADTS:
      on_mp2ts_pes_aac(pPkt, pProg, pPidDescr);
      break;
    case MP2PES_STREAM_AUDIO_MP2A_L2:
      on_mp2ts_pes_mp2_l2_aud(pPkt, pProg, pPidDescr);
      break;
    case MP2PES_STREAM_AUDIO_MP2A_L3:
      on_mp2ts_pes_mp2_l3_aud(pPkt, pProg, pPidDescr);
      break;
    default:
      break;
  }

  return 0;
}

static int read_container(MP2TS_CONTAINER_T *pMp2ts, unsigned int maxframes, 
                         const int *pabort) {
  int rc = 0;
  enum MP2TS_PARSE_RC parseRc;
  MP2TS_PKT_T pkt;
  MP2_PES_DESCR_T mp2;
  unsigned char buf[MP2TS_BDAV_LEN];
  unsigned int pktlen = MP2TS_LEN;
  MP2TS_STREAM_ADD_CBDATA_T  cbData;
  FILE_OFFSET_T lastPatOffset = 0;
  MP2TS_STREAM_CHANGE_T *pmtChangesPrev = NULL;
  unsigned int numFramesInIter = 0;
  unsigned int idx;

#ifdef DEBUG_PCR
  uint64_t pcr0=0;
  uint64_t pcrprev=0;
#endif // DEBUG_PCR

#if defined(DEBUG_PES) || defined(DEBUG_M2TPKTS)
  FILE_OFFSET_T fileOffsetPrior;
#endif // DEBUG_PES

  //memset(&cbData, 0, sizeof(&cbData));
  cbData.pMp2 = &mp2;
  cbData.pMp2ts = pMp2ts;

  if(!pMp2ts || !pMp2ts->pStream || pMp2ts->pStream->fp == FILEOPS_INVALID_FP) {
    return -1;
  }

  memset(&pkt, 0, sizeof(pkt));
  memset(&mp2, 0, sizeof(mp2));
  pMp2ts->numFrames = 0;
  pMp2ts->durationHz = 0;
  pkt.pData = buf;

  if(!(cbData.pPcrOffsets = (OFFSET_NODE_T *) avc_calloc(MAX_OFFSET_NODES, 
                                                         sizeof(OFFSET_NODE_T)))) {
    return -1;
  }
  
  // Warn if > 100MB
  if(pMp2ts->pStream->size > 0x6400000) {
    LOG(X_INFO("Opening large file %s - No .seek found"), pMp2ts->pStream->filename);
  }

  while(pMp2ts->pStream->offset < pMp2ts->pStream->size) {

    if(pabort && *pabort) {
      LOG(X_WARNING("Aborting reading %s"), pMp2ts->pStream->filename);
      rc = -1;
      break;
    }

    if(numFramesInIter > 557753) {
      // 100MB
      LOG(X_INFO("Looking for seek offsets in large file %s (%.1f%%)"), 
                 pMp2ts->pStream->filename,
               (double) pMp2ts->pStream->offset/ pMp2ts->pStream->size * 100.0);
      numFramesInIter = 0;
    }

    cbData.fileOffset = pMp2ts->pStream->offset;

#if defined(DEBUG_PES) || defined(DEBUG_M2TPKTS)
    fileOffsetPrior = pMp2ts->pStream->offset;
#endif // DEBUG_PES

    if(ReadFileStream(pMp2ts->pStream, pkt.pData, pktlen) != pktlen) {
      if(fileops_Feof(pMp2ts->pStream->fp)) {
        LOG(X_WARNING("Incomplete frame at eof"));
        break;
      }
      if(cbData.pPcrOffsets) {
        free(cbData.pPcrOffsets);
      }
      return -1;
    }

    pkt.len = pktlen;
    pkt.idx = 0;

    if((parseRc = mp2ts_parse(&pkt, &mp2.pat, &mp2.pmt)) == MP2TS_PARSE_ERROR) {
      break;
    } else if(parseRc == MP2TS_PARSE_OK_PAT) {

#if defined(DEBUG_PES) || defined(DEBUG_M2TPKTS)
      fprintf(stderr, "pat[%d]\n", ++spat);
#endif // DEBUG_PES

      if((rc = mp2_parse_pat(&mp2.pat, &pkt.pData[pkt.idx], pkt.len - pkt.idx)) < 0) {
        break;
      }

#ifdef DEBUG_M2TPKTS
      fprintf(stderr, "pat ver:%u\n", mp2.pat.version);
#endif
      lastPatOffset = cbData.fileOffset;
    } else if(parseRc == MP2TS_PARSE_OK_PMT) {

      if((rc = mp2_parse_pmt(&mp2.pmt, pkt.pid, &cbData, onProgAdd, 
                             &pkt.pData[pkt.idx], pkt.len - pkt.idx, 1)) < 0) {
        break;
      }
      if(pMp2ts->pmtChangesLast) {
        pMp2ts->pmtChangesLast->pcrpid = mp2.pmt.pcrpid;
      }

#if defined(DEBUG_PES) || defined(DEBUG_M2TPKTS)
      fprintf(stderr, "pmt[%d] pid:0x%x cidx: %d ver:%u\n", ++spmt, pkt.pid, pkt.cidx, mp2.pmt.version);
#endif // DEBUG_PES

    } else if(parseRc == MP2TS_PARSE_OK_PAYLOADSTART) {

#if defined(DEBUG_PES) || defined(DEBUG_M2TPKTS)
      spes++;
#endif // DEBUG_PES

#ifdef DEBUG_M2TPKTS
      fprintf(stderr, "pes[%d] file:0x%llx pid:0x%x cidx: %d %s\n", spes, fileOffsetPrior, pkt.pid, pkt.cidx, (pkt.flags & MP2TS_PARSE_FLAG_HAVEPCR) ? "PCR" : "");
#endif

//fprintf(stderr, "pes pid:0x%x pkt.idx:%u\n", pkt.pid, pkt.idx);

    } else {

#ifdef DEBUG_M2TPKTS
      fprintf(stderr, "non-pes pid:0x%x file: 0x%llx, cidx: %d %s\n", pkt.pid, fileOffsetPrior, pkt.cidx, (pkt.flags & MP2TS_PARSE_FLAG_HAVEPCR) ? "PCR" : "");
#endif
//fprintf(stderr, "... pid:0x%x\n", pkt.pid);
    }

    if(parseRc == MP2TS_PARSE_OK_PAYLOADSTART ||
       parseRc == MP2TS_PARSE_OK_PAYLOADCONT) {

      // Check continuity
      for(idx = 0; idx < MP2PES_MAX_PROG; idx++) {
        if(mp2.progs[idx].pid.id == pkt.pid) {

          if(pMp2ts->pmtChangesLast) {
            pMp2ts->pmtChangesLast->pidDescr[idx].numFrames++;

            if(parseRc == MP2TS_PARSE_OK_PAYLOADSTART) {
              mp2.progs[idx].idxPes++;
              pMp2ts->pmtChangesLast->pidDescr[idx].numPes++;

              //fprintf(stderr, "numFrames:%u\n", pMp2ts->numFrames);
              on_mp2ts_pes(&pkt, &mp2.progs[idx], &pMp2ts->pmtChangesLast->pidDescr[idx]);
            }

          }

//fprintf(stderr, "pid;0x%x %u -> %u\n", pkt.pid, mp2.progs[idx].prevCidx, pkt.cidx);
          if(mp2.progs[idx].idxPes > 1 && 
             ((pkt.cidx == 0 && mp2.progs[idx].prevCidx != 0x0f) ||
              (pkt.cidx != 0 && pkt.cidx != mp2.progs[idx].prevCidx + 1))) {

            LOG(X_WARNING("mpeg2-ts pid:0x%x continuity jump %u -> %u for frame %u"),
            pkt.pid, mp2.progs[idx].prevCidx, pkt.cidx, mp2.progs[idx].idxPes);
          }

          mp2.progs[idx].prevCidx = pkt.cidx;
          break;
        }
      }
    }

    if(pkt.flags & MP2TS_PARSE_FLAG_HAVEPCR) {

      if(pMp2ts->pmtChangesLast) {

        // force new MP2TS_STREAM_CHANGE_T node on large pcr jump
        if(pMp2ts->pmtChangesLast->havepcrEnd &&
           (pkt.tm.pcr < pMp2ts->pmtChangesLast->pcrEnd ||
          (pkt.tm.pcr - pMp2ts->pmtChangesLast->pcrEnd)/90.0f > MP2TS_LARGE_PCR_JUMP_MS)) {

          pmtChangesPrev = cbData.pMp2ts->pmtChangesLast;
          if(newPcrOffset(&cbData) < 0) {
            break;
          }
          cbData.pMp2ts->pmtChangesLast->startHz = cbData.pMp2ts->durationHz;
          if(pmtChangesPrev) {
            memcpy(&cbData.pMp2ts->pmtChangesLast->pidDescr, 
                   pmtChangesPrev->pidDescr, sizeof(cbData.pMp2ts->pmtChangesLast->pidDescr));
          }

        }

        
        //TODO: check idxPcrOffsets < MAX_OFFSET_NODES
        if(cbData.idxPcrOffset == 0 ||
          (pMp2ts->pmtChangesLast->havepcrEnd &&
             (pkt.tm.pcr - cbData.pPcrOffsets[cbData.idxPcrOffset-1].pcr)>
              MAX_OFFSET_NODE_DELTA_MS * 90)) {
          //cbData.pPcrOffsets[cbData.idxPcrOffset].fileOffset = cbData.fileOffset;
          cbData.pPcrOffsets[cbData.idxPcrOffset].fileOffset = lastPatOffset;
          cbData.pPcrOffsets[cbData.idxPcrOffset++].pcr = pkt.tm.pcr;
        }


        if(!pMp2ts->pmtChangesLast->havepcrStart) {
          pMp2ts->pmtChangesLast->pcrStart = pkt.tm.pcr;
          pMp2ts->pmtChangesLast->havepcrStart = 1;
#ifdef DEBUG_PCR
          pcr0=pkt.tm.pcr;
#endif // DEBUG_PCR
        } else if(pMp2ts->pmtChangesLast->havepcrEnd) {
          pMp2ts->durationHz += pkt.tm.pcr - pMp2ts->pmtChangesLast->pcrEnd;
        }
        pMp2ts->pmtChangesLast->pcrEnd = pkt.tm.pcr;
        pMp2ts->pmtChangesLast->havepcrEnd = 1;
      }

      //if(pcrprev > 0) {
      //  pMp2ts->durationHz += (pkt.tm.pcr - pcrprev);    
      //}
#ifdef DEBUG_PCR
      fprintf(stderr, "pcr pid:0x%x pcr:%llu (%.3f) dlta:%f prog:%f tot:%f\n", pkt.pid, pkt.tm.pcr, (double)pkt.tm.pcr/90000.0f, (double)(pkt.tm.pcr-pcrprev)/90000.0f, (double)(pkt.tm.pcr-pcr0)/90000.0f, (double)pMp2ts->durationHz/90000.0f);
      pcrprev = pkt.tm.pcr;
      if(pcr0 == 0) {
        pcr0 = pkt.tm.pcr;
      }
#endif // DEBUG_PCR

    }

    if(pMp2ts->pmtChangesLast) {
      pMp2ts->pmtChangesLast->numFrames++;
    }

    numFramesInIter++;
    pMp2ts->numFrames++;
    if(maxframes > 0 && pMp2ts->numFrames > maxframes) {
      LOG(X_DEBUG("Not reading more than %d frames in %s"), maxframes, pMp2ts->pStream->filename);
      pMp2ts->durationHz = 0;
      break;
    }
  }


  memcpy_pcroffsets(pMp2ts->pmtChangesLast, cbData.pPcrOffsets, cbData.idxPcrOffset);
  cbData.idxPcrOffset = 0;

  if(cbData.pPcrOffsets) {
    free(cbData.pPcrOffsets);
  }

  return rc;
}


//typedef int (*MP2TS_CB_ONPMT)(MP2TS_PKT_T *, void *);

#if defined(VSX_DUMP_CONTAINER)

void mp2ts_dump(MP2TS_CONTAINER_T *pMp2ts, int verbosity, FILE *fp) {

  MP2TS_STREAM_CHANGE_T *pMp2Change = NULL;
  MP2TS_PID_DESCR_T *pPidDescr = NULL;
  unsigned int pidIdx = 0;
  int isaudprog;

  if(!pMp2ts || !pMp2ts->pStream || pMp2ts->pStream->fp == FILEOPS_INVALID_FP || !fp) {
    return;
  }

  fprintf(fp, "filename: %s size: %"LL64"u\n", pMp2ts->pStream->filename, 
                                          pMp2ts->pStream->size);

  fprintf(fp, "mpeg2-ts fr: %u\n", pMp2ts->numFrames);
//pMp2ts->durationHz=3816*90000;
  fprintf(fp, "duration: %s (%.1fs) \n", avc_getPrintableDuration(pMp2ts->durationHz, 90000), 
               (double)pMp2ts->durationHz/90000.0f);

  pMp2Change = pMp2ts->pmtChanges;
  while(pMp2Change) {
    fprintf(fp, "pmt version:%u, pcrpid:0x%x", pMp2Change->pmtVersion, pMp2Change->pcrpid);
    fprintf(fp, ", start:%.2fs", (double)(pMp2Change->startHz/90000.0f));

    if(pMp2Change->havepcrStart && pMp2Change->havepcrEnd) {
      fprintf(fp, ", tot:%.2fs", (double)(pMp2Change->pcrEnd - pMp2Change->pcrStart)/90000.0f);
    } 

//fprintf(fp, "\n%f - %f %d pcr offsets (%f,file:0x%x)\n", (double)pMp2Change->pcrStart/90000.0f,(double)pMp2Change->pcrEnd/90000.0f,pMp2Change->numPcrOffsets, (double)pMp2Change->pPcrOffsets[0].pcr/90000.0f, pMp2Change->pPcrOffsets[0].fileOffset);

    fprintf(fp, ", file:0x%"LL64"x, mpeg2-ts fr: %u\n", 
          pMp2Change->fileOffsetStart, pMp2Change->numFrames);

    for(pidIdx = 0; 
        pidIdx < sizeof(pMp2Change->pidDescr) / sizeof(pMp2Change->pidDescr[0]); 
        pidIdx++ ) {

      pPidDescr = &pMp2Change->pidDescr[pidIdx];
      isaudprog = 0;

      if(pPidDescr->pid.id != 0) {
        if(pPidDescr->u.vidProp.mediaType != MEDIA_FILE_TYPE_UNKNOWN) {
          fprintf(fp, "%s ", codecType_getCodecDescrStr(pPidDescr->u.vidProp.mediaType));
        } else if(pPidDescr->u.audProp.mediaType != MEDIA_FILE_TYPE_UNKNOWN) {
          fprintf(fp, "%s ", codecType_getCodecDescrStr(pPidDescr->u.audProp.mediaType));
          isaudprog = 1;
        }
     
        if(!isaudprog) {
          if(pPidDescr->u.vidProp.resH) {
            fprintf(fp, "%ux%u ", pPidDescr->u.vidProp.resH, pPidDescr->u.vidProp.resV);
          }
          if(pPidDescr->u.vidProp.fps > 0.001) {
            fprintf(fp, "%.3ffps  ", pPidDescr->u.vidProp.fps);
          }
        } else {
          if(pPidDescr->u.audProp.clockHz > 0) {
            fprintf(fp, "%uHz", pPidDescr->u.audProp.clockHz);
          }
        }
        fprintf(fp, " [pid:0x%x, sid:0x%x, "MP2PES_STREAMTYPE_FMT_STR
                    ", mpeg2-ts fr:%u, es fr:%u]\n", 
              pPidDescr->pid.id, 
              pPidDescr->streamId,
              MP2PES_STREAMTYPE_FMT_ARGS(pPidDescr->pid.streamtype),
              pPidDescr->numFrames,
              pPidDescr->numPes);
      }
    }
    //fprintf(fp, "\n");

    pMp2Change = pMp2Change->pnext;
  }

}
#endif // VSX_DUMP_CONTAINER

//static uint64_t prevpcr=0;

enum MP2TS_PARSE_RC mp2ts_parse(MP2TS_PKT_T *pPkt, 
                                MP2_PAT_T *pPat,
                                MP2_PMT_T *pPmt) {

  int rc;
  uint64_t *pPcr = NULL;


  if(!pPkt |!pPkt->pData || !pPat || !pPmt) {
    return MP2TS_PARSE_ERROR;
  }

  pPkt->tm.pcr = 0;
  pPkt->tm.qtm.pts = 0;
  pPkt->tm.qtm.dts = 0;
  pPkt->flags = 0;

  if(pPkt->idx > pPkt->len || pPkt->len - pPkt->idx < MP2TS_LEN) {
    LOG(X_ERROR("Invalid mpeg2-ts length: %u"), pPkt->len - pPkt->idx);
    return MP2TS_PARSE_ERROR;
  }
  //startIdx = pPkt->idx;

  pPkt->hdr = htonl(*((uint32_t *)&pPkt->pData[pPkt->idx]));

  if(pPkt->pData[pPkt->idx] != MP2TS_SYNCBYTE_VAL) {
    LOG(X_ERROR("mpeg2-ts packet does not start with syncbyte"));
    return MP2TS_PARSE_ERROR; 
  }
  pPkt->pid = GET_MP2TS_HDR_TS_PID(pPkt);
  pPkt->cidx = GET_MP2TS_HDR_TS_CONTINUITY(pPkt);
  pPkt->idx += 4;

  if(pPkt->hdr & MP2TS_HDR_TS_ADAPTATION_EXISTS) {

    if(pPmt->pcrpid != 0 && pPkt->pid == pPmt->pcrpid) {
      pPcr = &pPkt->tm.pcr;
    }

//fprintf(stderr, "pid: 0x%x have adaptation\n", pPkt->pid);
    if((rc = mp2_parse_adaptation(&pPkt->pData[pPkt->idx], pPkt->len - pPkt->idx,
                         pPkt->pid, pPmt->pcrpid, pPcr, NULL)) < 0) {
//fprintf(stderr, "error parsing adaptation\n");
      return MP2TS_PARSE_ERROR;
    } else {
      
      if(pPkt->tm.pcr != 0) {
        pPkt->flags |= MP2TS_PARSE_FLAG_HAVEPCR;
//fprintf(stderr, "pid:0x%x pcr:%llu\n", pPkt->pid, pPkt->pcr);
//if(pPkt->pcr < prevpcr || pPkt->pcr > prevpcr +90000) {
//fprintf(stderr, "JUMP\n");
//}
//prevpcr=pPkt->tm.pcr;
      }
      pPkt->idx += rc;
     //fprintf(stderr, "pid:0x%x adaptation pcr flag: %u, val:%llu, len:%u\n", pPkt->pid, pPkt->flags & MP2TS_PARSE_FLAG_HAVEPCR, pPkt->tm.pcr, rc);
    }

    //if(pPcr == NULL) {
    //  pPkt->tm.pcr = 0;
    //}

    if(pPkt->idx > pPkt->len) {
      LOG(X_ERROR("Invalid adaptation length: %u"), pPkt->idx - 4);
      //pPkt->idx = startIdx;
      return MP2TS_PARSE_ERROR;
    }
  }

  if(! (pPkt->hdr & MP2TS_HDR_TS_PAYLOAD_EXISTS)) {

    if(! (pPkt->hdr & MP2TS_HDR_TS_ADAPTATION_EXISTS)) {
      LOG(X_WARNING("m2t payload bit not set"));
    }
    return MP2TS_PARSE_OK_EMPTY;

  } else if(pPkt->pid == 0) {

    if(pPkt->hdr & MP2TS_HDR_PAYLOAD_UNIT_START) {
      pPkt->idx++;
    }
    return MP2TS_PARSE_OK_PAT; 

  } else if(pPkt->pid == pPat->progmappid) {

    if(pPkt->hdr & MP2TS_HDR_PAYLOAD_UNIT_START) {
      pPkt->idx++;
    }
    return MP2TS_PARSE_OK_PMT;

  } else if(!(pPkt->hdr & MP2TS_HDR_PAYLOAD_UNIT_START)) {

    return MP2TS_PARSE_OK_PAYLOADCONT;

  } else if(pPkt->idx + 14 >= pPkt->len) {

    return MP2TS_PARSE_OK_PAYLOADSTART;
  }

  pPkt->flags |= MP2TS_PARSE_FLAG_PESSTART;

  return MP2TS_PARSE_OK_PAYLOADSTART;
}

int mp2ts_jump_file_offset(FILE_STREAM_T *pFileStream, float percent,
                           FILE_OFFSET_T offsetFromEnd) {

  struct stat st;
  FILE_OFFSET_T seekOffset = 0;

  if(!pFileStream) {
    return -1;
  }

  if(fileops_stat(pFileStream->filename, &st) != 0) {
    return -1;
  }

  //
  // Adjust seekOffset according to specified percent
  //
  if(percent < 0) {
    percent = 0;
  } else if(percent > 100.0) {
    percent = 100.0;
  }
  if(percent != 0) {
    seekOffset = (FILE_OFFSET_T) (st.st_size * (percent/100.0f));
  } 

  //
  // Ensure seekOffset is < offsetFromEnd
  //
  if(offsetFromEnd > st.st_size) {
    seekOffset = 0;
  } else if(offsetFromEnd > (st.st_size - seekOffset)) {
    seekOffset = st.st_size - offsetFromEnd;
  }

  LOG(X_DEBUG("seeking in %s to %.2f%% (- %llu) %llu/%llu"), 
       pFileStream->filename, percent, offsetFromEnd, seekOffset, st.st_size);

  if(SeekMediaFile(pFileStream, seekOffset, SEEK_SET) < 0) {
    return -1;
  }
 
  return 0;
}
                
MP2TS_STREAM_CHANGE_T *mp2ts_jump_file(MP2TS_CONTAINER_T *pMp2ts, 
                                       float fStartSec,
                                       double *pLocationMsStart) {
  MP2TS_STREAM_CHANGE_T *pmtChanges = NULL;
  MP2TS_STREAM_CHANGE_T *pmtChangesPrev = NULL;
  unsigned int idxPcrOffset;
  FILE_OFFSET_T fileOffset = 0;
  //int rc = 0;
  uint64_t jumpHz = (uint64_t) (fStartSec * 90000);

  //fprintf(stderr, "mp2ts jump requested : %f\n", fStartSec);

  if(!pMp2ts || fStartSec < 0) {
    return NULL;
  }

  if((pmtChanges = pMp2ts->pmtChanges) == NULL) {
    return NULL;
  }

  while(pmtChanges) {
    
//fprintf(stderr, "%f %f\n", (double)pmtChanges->startHz/90000.0f, (double)jumpHz/90000.0f);
    if(pmtChanges->startHz > jumpHz) {
//fprintf(stderr, "breaking\n");
      break;
    }

    pmtChangesPrev = pmtChanges;
    pmtChanges = pmtChanges->pnext;
  }

  if(pmtChangesPrev == NULL) {
//fprintf(stderr, "pmtChangesPrev is null\n");
    pmtChangesPrev = pMp2ts->pmtChanges;
  }
  //fprintf(stderr, "pmt version:%u, start:%.2fs\n", pmtChangesPrev->pmtVersion, (double)(pmtChangesPrev->startHz/90000.0f));

  fileOffset = pmtChangesPrev->fileOffsetStart;
  //if(pLocationMsStart) {
  //  *pLocationMsStart = (double) pmtChangesPrev->startHz / 90000.0f;
  //}

  if(pmtChangesPrev->pPcrOffsets) {

    for(idxPcrOffset = 0; idxPcrOffset < pmtChangesPrev->numPcrOffsets; idxPcrOffset++) {
      if((pmtChangesPrev->pPcrOffsets[idxPcrOffset].pcr - pmtChangesPrev->pcrStart) + 
         pmtChangesPrev->startHz > jumpHz) {
        break;
      }
    }

    if(idxPcrOffset > 0) {
      idxPcrOffset--;
    }

    fileOffset = pmtChangesPrev->pPcrOffsets[idxPcrOffset].fileOffset;
    if(pLocationMsStart) {
      *pLocationMsStart = (double) (pmtChangesPrev->pPcrOffsets[idxPcrOffset].pcr -
                                  pmtChangesPrev->pcrStart)/90000.0f;
    }
  }


  if(SeekMediaFile(pMp2ts->pStream, fileOffset, SEEK_SET) < 0) {
    return NULL;
  }
  LOG(X_DEBUG("mp2ts jumped to file: 0x%llx %f"), pMp2ts->pStream->offset, pLocationMsStart ? *pLocationMsStart : -1);

  return pmtChangesPrev;
}

static int mp2ts_serializemeta(MP2TS_CONTAINER_T *pMp2ts, FILE_STREAM_T *pfs) {
  int rc = 0;
  MP2TS_STREAM_CHANGE_T *pmtChanges;
  unsigned int idxPmt = 0;

  if((rc = WriteFileStream(pfs, pMp2ts, sizeof(MP2TS_CONTAINER_T))) < 0) {
    return rc;
  }

  pmtChanges = pMp2ts->pmtChanges;
  for(idxPmt = 0; idxPmt < pMp2ts->numPmtChanges && pmtChanges; idxPmt++) {

    if((rc = WriteFileStream(pfs, pmtChanges, sizeof(MP2TS_STREAM_CHANGE_T))) < 0) {
      return rc;
    }

    if(pmtChanges->numPcrOffsets > 0) {
      if((rc = WriteFileStream(pfs, pmtChanges->pPcrOffsets, 
                    pmtChanges->numPcrOffsets * sizeof(OFFSET_NODE_T))) < 0) {
        return rc;
      }
    }

    pmtChanges = pmtChanges->pnext;
  }

  return rc; 
}

static int mp2ts_writemeta(MP2TS_CONTAINER_T *pMp2ts) {
  FILE_STREAM_T fs;
  size_t sz;

  if(!pMp2ts || !pMp2ts->pStream) {
    return -1;
  }

  memset(&fs, 0, sizeof(fs));
  strncpy(fs.filename, pMp2ts->pStream->filename, sizeof(fs.filename) - 1);
  //if((sz = path_getLenNonPrefixPart(fs.filename)) > 0) {
  if((sz = strlen(fs.filename)) > 0) {
     strncpy(&fs.filename[sz], MP2TS_FILE_SEEKIDX_NAME, sizeof(fs.filename) - sz - 1);
  }

  if((fs.fp = fileops_Open(fs.filename, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open %s for writing"), fs.filename);
    return -1;
  }  

  if(mp2ts_serializemeta(pMp2ts, &fs) >= 0) {
    LOG(X_DEBUG("Wrote metadata to %s"), fs.filename);
  } else {
    LOG(X_ERROR("Failed to serialize meta data for %s"), pMp2ts->pStream->filename);
  }

  fileops_Close(fs.fp);

  return 0;
}

static int mp2ts_readmeta(MP2TS_CONTAINER_T *pMp2ts, FILE_STREAM_T *pfs) {
  int rc = 0;
  FILE_STREAM_T *pStream = pMp2ts->pStream;
  unsigned int idxPmtChanges = 0;
  MP2TS_STREAM_CHANGE_T *pmtChanges;

  //if(strstr(pfs->filename, "wsbk_kyalami")) {
  //  rc =rc;
  //}

  //fprintf(stderr, "reading %s %d\n", pfs->filename, sizeof(MP2TS_CONTAINER_T));

  rc = ReadFileStream(pfs, pMp2ts, sizeof(MP2TS_CONTAINER_T));

  //fprintf(stderr, "got %d sz: %lld ver: %d ws:%d fr:%d pmt:%d\n", rc, pMp2ts->size, pMp2ts->serializedver, pMp2ts->wordsz, pMp2ts->numFrames, pMp2ts->numPmtChanges);

  if(pStream->size != pMp2ts->size) {
    LOG(X_WARNING("mpeg2-ts %s metadata stored size %lld does not match file size %lld"), 
          pfs->filename, pMp2ts->size, pStream->size);
    rc = -2;
  } else if(pMp2ts->serializedver != MP2TS_CONTAINER_SERIALIZED_VER) {
    LOG(X_WARNING("mpeg2-ts %s version is out of date"), pfs->filename);
    rc = -2;
  } else if(pMp2ts->wordsz != sizeof(int) ||
            pMp2ts->szChangeElem != sizeof(MP2TS_STREAM_CHANGE_T)) {
    LOG(X_WARNING("mpeg2-ts %s was not created on this system"), pfs->filename);
    rc = -2;
  //} else if(pMp2ts->numPmtChanges > MAX_OFFSET_NODES) {
  //  LOG(X_WARNING("mpeg2-ts %s appears to contain excessive %u pmt changes"), 
  //      pfs->filename, pMp2ts->numPmtChanges);
  //  rc = -2;
  }

  pMp2ts->pmtChanges = pMp2ts->pmtChangesLast = NULL;
  pMp2ts->pStream = pStream;

  if(rc < 0) {
    return rc;
  }

  for(idxPmtChanges = 0; idxPmtChanges < pMp2ts->numPmtChanges; idxPmtChanges++) {

    if(!(pmtChanges = (MP2TS_STREAM_CHANGE_T *) avc_calloc(1, sizeof(MP2TS_STREAM_CHANGE_T)))) {
      rc = -1;
      break; 
    }

    if((rc = ReadFileStream(pfs, pmtChanges, sizeof(MP2TS_STREAM_CHANGE_T))) < 0) {
      free(pmtChanges);
      break;
    }

    pmtChanges->pPcrOffsets = NULL;
    pmtChanges->pnext = NULL;

    if(pmtChanges->numPcrOffsets > 0) {

      if(pmtChanges->numPcrOffsets * sizeof(OFFSET_NODE_T) > pfs->size - pfs->offset) {
        LOG(X_ERROR("seek file %s contains num pcr offsets %u * %u > file size %"LL64"u / %"LL64"u"), 
          pfs->filename, pmtChanges->numPcrOffsets, sizeof(OFFSET_NODE_T), pfs->offset, pfs->size);
        rc = -1;
        free(pmtChanges);
        break; 
      }

      if(!(pmtChanges->pPcrOffsets = (OFFSET_NODE_T *) 
                 calloc(pmtChanges->numPcrOffsets, sizeof(OFFSET_NODE_T)))) {
        LOG(X_CRITICAL("Failed to allocate %d x %d in mpeg2-ts prog add for %s"),
                    pmtChanges->numPcrOffsets, sizeof(OFFSET_NODE_T), pfs->filename);
        rc = -1;
        free(pmtChanges);
        break; 
      }

      if((rc = ReadFileStream(pfs, pmtChanges->pPcrOffsets, 
                    pmtChanges->numPcrOffsets * sizeof(OFFSET_NODE_T))) < 0) {
        rc = -1;
        free(pmtChanges);
        break;
      }
    }

    if(pMp2ts->pmtChanges == NULL) {
      pMp2ts->pmtChanges = pmtChanges;
    } else {
      pMp2ts->pmtChangesLast->pnext = pmtChanges;
    }
    pMp2ts->pmtChangesLast= pmtChanges;

  }

  return rc;
}

MP2TS_CONTAINER_T *mp2ts_openfrommeta(const char *path) {
  FILE_STREAM_T *pStreamIn = NULL;
  MP2TS_CONTAINER_T *pMp2ts = NULL;
  FILE_STREAM_T fs;
  size_t sz;
  struct stat st;
  char buf[VSX_MAX_PATH_LEN];
  int rc;
 
  if(!path) {
    return NULL;
  }

  //memset(&fs, 0, sizeof(fs));
  //strncpy(fs.filename, path, sizeof(fs.filename) - 1);
  strncpy(buf, path, sizeof(buf) - 1);
  
  if((sz = strlen(buf))) {
     strncpy(&buf[sz], MP2TS_FILE_SEEKIDX_NAME, sizeof(buf) - sz - 1);
  }

  if(fileops_stat(buf, &st) != 0 || OpenMediaReadOnly(&fs, buf) < 0) {
    return NULL;
  }  

  if((pStreamIn = (FILE_STREAM_T *) avc_calloc(1, sizeof(FILE_STREAM_T))) == NULL) {
    fileops_Close(fs.fp);
    return NULL;
  }

  if(OpenMediaReadOnly(pStreamIn, path) < 0) {
    free(pStreamIn);
    fileops_Close(fs.fp);
    return NULL;
  }

  if((pMp2ts = (MP2TS_CONTAINER_T *) avc_calloc(1, sizeof(MP2TS_CONTAINER_T))) == NULL) {
    CloseMediaFile(pStreamIn);
    free(pStreamIn);
    fileops_Close(fs.fp);
    return NULL;
  }

  pMp2ts->pStream = pStreamIn;

  if((rc = mp2ts_readmeta(pMp2ts, &fs)) < 0) {
    if(rc == -1) {
      LOG(X_DEBUG("Failed to read %s from stored metadata"), path);
    }
    //if(rc != -2) {
    mp2ts_close(pMp2ts);
    //}
    fileops_Close(fs.fp);
    return NULL;
  }

  LOG(X_DEBUG("Read %s from stored metadata"), path);

  fileops_Close(fs.fp);

  return pMp2ts; 
}

