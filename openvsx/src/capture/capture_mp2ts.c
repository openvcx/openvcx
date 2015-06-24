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

#if defined(VSX_HAVE_CAPTURE)

//#define DUMP_MP2TS	1


#define MP2TS_INCREMENT(p)     ((char *) (p)->pData) += (p)->idx; \
                                 (p)->len -= (p)->idx; \
                                 (p)->idx = 0;


typedef struct MP2_PMT_PROGADD_CBDATA {
  const char *outPrefix;
  int overWriteOut;
  MP2_PES_DESCR_T *pMp2;
  int extractVid;
  int extractAud;
} MP2_PMT_PROGADD_CBDATA_T;

#if defined(VSX_EXTRACT_CONTAINER)

static int create_outputFile(MP2_PMT_PROGADD_CBDATA_T *pCbData,
                             MP2_PES_PROG_T *pProg,
                             enum MEDIA_FILE_TYPE mediaType) {
  MP2_PES_DESCR_T *pMp2 = pCbData->pMp2;

  if(pProg->stream.fp != FILEOPS_INVALID_FP) {
    CloseMediaFile(&pProg->stream);    
  }

  if(capture_createOutPath(pProg->stream.filename, sizeof(pProg->stream.filename),
                        NULL, pCbData->outPrefix,
                        NULL, mediaType) < 0) {

    LOG(X_ERROR("Invalid output file name or directory."));
    return -1;
  }

  if(capture_openOutputFile(&pProg->stream,
                            pMp2->overWriteFile, pMp2->outputDir) < 0) {
    return -1;
  }

  LOG(X_INFO("Created '%s'"), pProg->stream.filename);

  return 0;
}


static int cbMp2pmt_onProgAdd(void *pArg, const MP2_PID_T *pPid) {
  MP2_PMT_PROGADD_CBDATA_T *pCbData = (MP2_PMT_PROGADD_CBDATA_T *) pArg;
  MP2_PES_DESCR_T *pMp2 = pCbData->pMp2;
  MP2_PES_PROG_T *pProg = NULL;
  CAPTURE_FILTER_PROTOCOL_T protoType = CAPTURE_FILTER_PROTOCOL_UNKNOWN;
  //enum MEDIA_FILE_TYPE mediaType = MEDIA_FILE_TYPE_UNKNOWN;

  if((pProg = mp2_find_prog(pMp2,  pPid->id))) {

    if(pProg->pid.streamtype != pPid->streamtype) {
      LOG(X_WARNING("mpeg2 pmt proginfo pid:0x%x streamtype change %u -> %u not supported"),
            pPid->id, pProg->pid.streamtype, pPid->streamtype);
      return 0;
    }
    return 0;
  }

  if((pProg = mp2_new_prog(pMp2, pPid->id)) == NULL) {
    LOG(X_ERROR("Reached max program limit:%u/%u"),
        pMp2->activePrograms, MP2PES_MAX_PROG);
    return -1;
  }


  pProg->pid.streamtype = pPid->streamtype;

  switch(pPid->streamtype) {
    case MP2PES_STREAM_AUDIO_AACADTS:
      if(!pCbData->extractAud) {
        return 0;
      }
      protoType = CAPTURE_FILTER_PROTOCOL_AAC;
      break;
    case MP2PES_STREAM_VIDEO_H264:
      if(!pCbData->extractVid) {
        return 0;
      }
      protoType = CAPTURE_FILTER_PROTOCOL_H264;
      break;
    case MP2PES_STREAM_VIDEO_H262:
      if(!pCbData->extractVid) {
        return 0;
      }
      protoType = CAPTURE_FILTER_PROTOCOL_H262;
      break;
    case MP2PES_STREAM_VIDEO_MP4V:
      if(!pCbData->extractVid) {
        return 0;
      }
      protoType = CAPTURE_FILTER_PROTOCOL_MP4V;
      break;
    case MP2PES_STREAM_AUDIO_AC3:
      if(!pCbData->extractAud) {
        return 0;
      }
      protoType = CAPTURE_FILTER_PROTOCOL_AC3;
      break;
    default:
      LOG(X_WARNING("Ignoring unsupported mpeg2 pmt proginfo pid:0x%x, streamtype:0x%x"),
              pPid->id, pPid->streamtype);
      break;
  }

  if(pProg->stream.fp == FILEOPS_INVALID_FP && 
     protoType != CAPTURE_FILTER_PROTOCOL_UNKNOWN) {
    if(create_outputFile(pCbData, pProg, protoType) != 0) {
      //LOG(X_WARNING("Failed to create output file."));
    }
  }

  return 0;
}


static int reset_prev_aacAdtsHdr(MP2_PES_PROG_T *pProg) {
  unsigned char buf[32];
  AAC_ADTS_FRAME_T aacFrame;

  if(SeekMediaFile(&pProg->stream, pProg->fileOffsetPrev, SEEK_SET) < 0) {
    return -1;
  }

  if(aac_decodeAdtsHdrStream(&pProg->stream, &aacFrame) < 0) {
    return -1;
  }

  aacFrame.lentot = aacFrame.lenhdr;

  if(aac_encodeADTSHdr(buf, sizeof(buf), &aacFrame.sp, 0) < 0) {
    return -1;
  }
            
  if(SeekMediaFile(&pProg->stream, pProg->fileOffsetPrev, SEEK_SET) < 0) {
    return -1;
  }

  if(WriteFileStream(&pProg->stream, buf, aacFrame.lenhdr) < 0) {
    return -1;
  }

  return 0;
}


/*
int ac3_find_starthdr(const unsigned char *pbuf, unsigned int len) {
  unsigned int idx;

  // TODO: need to read priv data hdr instead of looking for start code
  // does not keep track of last start code boundary in prev pkt
  for(idx = 1; idx < len; idx += 2) {
    if(pbuf[idx - 1] == 0x0b && pbuf[idx] == 0x77) {
      return idx - 1;
    }
  }
  return -1;
}
*/


static int write_payload(MP2_PES_DESCR_T *pMp2,
                         MP2_PES_PROG_T *pProg,
                         MP2TS_PKT_T *pkt,
                         unsigned int len,
                         unsigned int pesTotLen) {

  int rc = 0;
  int payloadUnitStart = (pkt->hdr & MP2TS_HDR_PAYLOAD_UNIT_START);
  unsigned int lenWr = len;

//static int64_t ptslast; if(pProg->pid.id== 0x44 && payloadUnitStart) { fprintf(stderr, "mp2ts pkt pid:0x%x, pts:%.3f (%.3f), pcr:%lld, len:%d,%d\n", pkt->pid, PTSF(pkt->tm.qtm.pts), PTSF(pkt->tm.qtm.pts - ptslast),pkt->tm.pcr, len, pesTotLen); if(PTSF(pkt->tm.qtm.pts - ptslast)>.5)fprintf(stderr, "LARGE PTS JUMP\n"); ptslast=pkt->tm.qtm.pts;}

  if(pProg->stream.fp == FILEOPS_INVALID_FP) {
    return 0;
  } else if(!mp2_check_pes_streamid(pProg->streamId)) {
    //LOG(X_WARNING("Unsupported mpeg2-ts stream id:0x%x in pid:0x%x"), pProg->streamId, pkt->pid);
    return 0;
  } else if(!pProg->havePesStart && !payloadUnitStart) {
    return 0;
  } else if(payloadUnitStart) {

//fprintf(stderr, "streamType:0x%x 0x%x, pesSt:%d\n", pProg->streamType, MP2TS_STREAM_VIDEO_H262,pProg->havePesStart);
    // TODO: move proto specific stuff to cb

    if(pProg->pid.streamtype == MP2PES_STREAM_VIDEO_H262) {

//fprintf(stderr, "pes for idx:%d/%d %d / %d\n", pkt->idx, pkt->len, len, pesTotLen);

      //
      // check for mpeg-2 elementary stream startcode matching 
      // mpeg-2 sequence header start
      //
      if(!pProg->havePesStart && !mpg2_check_seq_hdr_start(&pkt->pData[pkt->idx], lenWr)) {

#if defined(DUMP_MP2TS)
  fprintf(stdout, "pid:0x%4.4x no seq hdr yet in PES %2.2x %2.2x %2.2x %2.2x\n", pProg->pid.id,
    pkt->pData[pkt->idx+0],pkt->pData[pkt->idx+1],pkt->pData[pkt->idx+2],pkt->pData[pkt->idx+3]);
#endif // DUMP_MP2TS

        return 0;
      }
    } else if(pProg->pid.streamtype == MP2PES_STREAM_AUDIO_AC3) {

      //
      // check for ac3 start byte sequence
      //
      if(!pProg->havePesStart) {

        if((rc = ac3_find_starthdr(&pkt->pData[pkt->idx], lenWr)) < 0) {

#if defined(DUMP_MP2TS)
  fprintf(stdout, "pid:0x%4.4x ac3 hdr not found\n", pProg->pid.id);
#endif // DUMP_MP2TS

          return 0;

        } else if(rc > 0) {

#if defined(DUMP_MP2TS)
  fprintf(stdout, "pid:0x%4.4x ac3 going forward %d bytes\n", pProg->pid.id, rc);
#endif // DUMP_MP2TS

          lenWr -= rc;      
          pkt->idx += rc;
        }
      }
    }

    pProg->havePesStart = 1;
  }


/*
if(payloadUnitStart && pProg->pid.streamType == MP2PES_STREAM_VIDEO_H262 &&
   mpg2_check_seq_hdr_start(&pkt->pData[pkt->idx], lenWr)) {
  mpg2_parse_seq_hdr(&pkt->pData[pkt->idx + 4], lenWr - 4);
}
*/



  if(pProg->prevPesTotLen > 0 && 
    ((pkt->cidx == 0 && pProg->prevCidx != 0x0f) ||
    (pkt->cidx != 0 && pkt->cidx != pProg->prevCidx + 1))) {
    LOG(X_WARNING("mpeg2-ts pid:0x%x continuity jump %u -> %u for frame at 0x%llx"),
      pkt->pid, pProg->prevCidx, pkt->cidx, pProg->fileOffsetPrev);
  }

  if(payloadUnitStart) {

    if(pesTotLen && pProg->idxInPes != pProg->prevPesTotLen) {
      LOG(X_WARNING("mpeg2-ts pid:0x%x  Prior frame at "
                    "file:0x%llx + %u does not match next frame start at 0x%llx"),
        pkt->pid, pProg->fileOffsetPrev, pProg->prevPesTotLen, pProg->stream.offset);

      if(pProg->pid.streamtype == MP2PES_STREAM_AUDIO_AACADTS) {
        reset_prev_aacAdtsHdr(pProg);
      } 
    }

    pProg->fileOffsetPrev = pProg->stream.offset;
    pProg->prevPesTotLen = pesTotLen;
    pProg->idxInPes = 0;
  }

  pProg->idxInPes += len;
  pProg->prevCidx = pkt->cidx;

#if defined(DUMP_MP2TS)
//if(pkt->pid==0x0801) 
  fprintf(stdout, "pid[0x%4.4x,c=%2.2u,0x%x] len:%3u,%s%u(%5llu),file:0x%llx "
                  "%2.2x %2.2x %2.2x %2.2x %2.2x %2.2x...%2.2x %2.2x %2.2x %2.2x", 
    pkt->pid, pkt->cidx, pProg->streamId, lenWr, (pesTotLen>0?"pes:":""),pesTotLen, 
    pProg->stream.offset-pProg->fileOffsetPrev,
    pProg->stream.offset, pkt->pData[pkt->idx+0],pkt->pData[pkt->idx+1],
    pkt->pData[pkt->idx+2], pkt->pData[pkt->idx+3], pkt->pData[pkt->idx+4],  
    pkt->pData[pkt->idx+5], pkt->pData[pkt->idx+lenWr-4], pkt->pData[pkt->idx+lenWr-3], 
    pkt->pData[pkt->idx+lenWr-2], pkt->pData[pkt->idx+lenWr-1]);
  if(pkt->tm.pts) 
    fprintf(stdout, " pts:%llu\n", pkt->tm.pts);
  if(pkt->tm.dts) 
    fprintf(stdout, " dts:%llu\n", pkt->tm.dts);
  fprintf(stdout, "\n");
#endif // DUMP_MP2TS
  


  if((rc = WriteFileStream(&pProg->stream, &pkt->pData[pkt->idx], lenWr)) < 0) {
    return -1;
  }



  return rc;
}


static int mp2ts_parse_pkt(MP2_PES_DESCR_T *pMp2, MP2TS_PKT_T *pkt, 
                           int overWriteOut, const char *outPrefix,
                           int video, int audio) {
  int rc = 0;
  MP2_PMT_PROGADD_CBDATA_T cbData;
  enum MP2TS_PARSE_RC parseRc = MP2TS_PARSE_UNKNOWN;
  MP2_PES_PROG_T *pProg = NULL;
  unsigned int endIdx = MP2TS_LEN - pkt->idx;

  cbData.pMp2 = pMp2;
  cbData.outPrefix = outPrefix;
  cbData.overWriteOut = overWriteOut;
  cbData.extractVid = video;
  cbData.extractAud = audio;

  pkt->len = endIdx;

  if((parseRc = mp2ts_parse(pkt, &pMp2->pat, &pMp2->pmt)) == MP2TS_PARSE_ERROR) {
    rc = -1;
  } else if(parseRc == MP2TS_PARSE_OK_PAT) {

    if((rc = mp2_parse_pat(&pMp2->pat, &pkt->pData[pkt->idx], pkt->len - pkt->idx)) > 0) {
      //pMp2->lastPatVersion = pMp2->pat.version;
      //pMp2->lastPatPrognum = pMp2->pat.prognum;
    }

  } else if(parseRc == MP2TS_PARSE_OK_PMT) {

//fprintf(stderr, "pmt pid %u\n", pkt->pid);
    if((rc = mp2_parse_pmt(&pMp2->pmt, pkt->pid, &cbData, cbMp2pmt_onProgAdd, 
                  &pkt->pData[pkt->idx], pkt->len - pkt->idx, 0)) > 0) {

    }
#if defined(DUMP_MP2TS)
      fprintf(stdout, "pmt version:%d\n", pMp2->pmt.version);
#endif // DUMP_MP2TS


  } else if(parseRc == MP2TS_PARSE_OK_PAYLOADSTART) {

//fprintf(stderr, " PES pid:%u (%u/%u,%u)\n", pkt->pid, pkt->idx, pkt->len, endIdx);
    if((pProg = mp2_find_prog(pMp2, pkt->pid))) {
      if((rc = mp2_parse_pes(pProg, &pkt->pData[pkt->idx], endIdx - pkt->idx, &pkt->tm.qtm, NULL)) >= 0) {
        pkt->idx += rc;
        rc = write_payload(pMp2, pProg, pkt, endIdx - pkt->idx, 0);
      }
    }
  
  } else if(parseRc == MP2TS_PARSE_OK_PAYLOADCONT) {

//fprintf(stderr, " payload continuation...\n");
    if((pProg = mp2_find_prog(pMp2, pkt->pid))) {

      rc = write_payload(pMp2, pProg, pkt, endIdx - pkt->idx, 0);
    }

  } else if(parseRc == MP2TS_PARSE_OK_EMPTY) {
    //LOG(X_DEBUG("empty mpeg2-ts packet pid:0x%x"), pkt->pid);
  }

  pkt->idx = endIdx;

if(rc<0) {
fprintf(stdout, "rc:%d\n", rc);
}
  return rc;
}

/*
int writenet_mp2ts(void *pUserData, unsigned char *pData, unsigned int len, uint32_t ts) {
  CAPTURE_CBDATA_SP_T *pSp = (CAPTURE_CBDATA_SP_T *) pUserData;
  MP2_PES_DESCR_T *pMp2 = &pSp->u.mp2;
  int rc = 0;

  MP2TS_PKT_T pkt;

  if(pMp2 == NULL) {
    return -1;
  }

  if(pData == NULL) {
    // Packet was lost
    LOG(X_WARNING("Lost mpeg2-ts RTP packet ts:%u"), ts);
    return 0;
  } else if(len == 0) {
    return 0;
  }

  pkt.rtpts = ts;
  pkt.pData = pData;
  pkt.len = len;
  pkt.idx = 0;

//fprintf(stderr, "packet data len:%u\n", len);

  while(pkt.len > 0 && (rc = mp2ts_parse_pkt(pMp2, &pkt)) >= 0) {
    if(pkt.idx > len || pkt.idx == 0) {
      return -1;
    }
  
    len -= pkt.idx;
    pkt.pData += pkt.idx;
    pkt.len = len;
    pkt.idx = 0;
    
  }

  return rc;
}
*/

int extractfrom_mp2ts(FILE_STREAM_T *pFileIn, const char *outPrefix, 
                      int overWriteOut, float fDuration,
                      int video, int audio) {

  int rc = 0;
  MP2TS_PKT_T pkt;
  MP2_PES_DESCR_T mp2;
  unsigned char buf[MP2TS_LEN];
  unsigned int idx;

  // TODO:  obey duration, outPrefix

  if(!pFileIn || pFileIn->fp == FILEOPS_INVALID_FP) {
    return -1;
  }

  memset(&pkt, 0, sizeof(pkt));
  memset(&mp2, 0, sizeof(mp2));
  mp2.overWriteFile = overWriteOut;

  for(idx = 0; idx < MP2PES_MAX_PROG; idx++) {
    mp2.progs[idx].stream.fp = FILEOPS_INVALID_FP;
  }

  LOG(X_DEBUG("Extracting elementary streams from %s"), pFileIn->filename);

  while(pFileIn->offset < pFileIn->size) {
//fprintf(stderr, "file:0x%x\n", pFileIn->offset);
    if(ReadFileStream(pFileIn, buf, MP2TS_LEN) != MP2TS_LEN) {
      return -1;
    }

    pkt.pData = buf;
    pkt.len = MP2TS_LEN;
    pkt.idx = 0;    

    if((rc = mp2ts_parse_pkt(&mp2, &pkt, overWriteOut, outPrefix, video, audio)) < 0) {
      return -1;
    }

  }

  return 0;
}
#endif // VSX_EXTRACT_CONTAINER


#endif // VSX_HAVE_CAPTURE
