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

//#define DUMP_MP2TS	1


int mp2_check_pes_streamid(int streamId) {

  // iso13818-1 table 2-18 Stream Assignments
  if((streamId >= 0xc0 && streamId <= 0xdf) ||   // audio
     (streamId >= 0xe0 && streamId <= 0xef) ||   // video
     (streamId == 0xbd || streamId == 0xbf)) {    // private stream
    return 1;
  }
  return 0;
}

uint16_t mp2_getStreamType(XC_CODEC_TYPE_T codecType) {
  switch(codecType) {

    case XC_CODEC_TYPE_H264:
      return MP2PES_STREAM_VIDEO_H264;
    case XC_CODEC_TYPE_H262:
      return MP2PES_STREAM_VIDEO_H262;
    case XC_CODEC_TYPE_H263:
    case XC_CODEC_TYPE_H263_PLUS:
      return MP2PES_STREAM_VIDEO_H263;
    case XC_CODEC_TYPE_MPEG4V:
      return MP2PES_STREAM_VIDEO_MP4V;

    case XC_CODEC_TYPE_AAC:
      return MP2PES_STREAM_AUDIO_AACADTS;
    case XC_CODEC_TYPE_AC3:
      return MP2PES_STREAM_AUDIO_AC3;
    case XC_CODEC_TYPE_MPEGA_L2:
      return MP2PES_STREAM_AUDIO_MP2A_L2;
    case XC_CODEC_TYPE_MPEGA_L3:
      return MP2PES_STREAM_AUDIO_MP2A_L3;
    case XC_CODEC_TYPE_AMRNB:
      return MP2PES_STREAM_AUDIO_AMR;
    case XC_CODEC_TYPE_G711_MULAW:
    case XC_CODEC_TYPE_G711_ALAW:
      return MP2PES_STREAM_AUDIO_G711;

    case XC_CODEC_TYPE_RAWV_BGRA32:
    case XC_CODEC_TYPE_RAWV_RGBA32:
    case XC_CODEC_TYPE_RAWV_BGR24:
    case XC_CODEC_TYPE_RAWV_RGB24:
    case XC_CODEC_TYPE_RAWV_BGR565:
    case XC_CODEC_TYPE_RAWV_RGB565:
    case XC_CODEC_TYPE_RAWV_YUV420P:
    case XC_CODEC_TYPE_RAWV_YUVA420P:
    case XC_CODEC_TYPE_RAWV_NV21:
    case XC_CODEC_TYPE_RAWV_NV12:

    case XC_CODEC_TYPE_RAWA_PCM16LE:
    case XC_CODEC_TYPE_RAWA_PCMULAW:
    case XC_CODEC_TYPE_RAWA_PCMALAW:

    case XC_CODEC_TYPE_VP6:
    case XC_CODEC_TYPE_VP6F:
    case XC_CODEC_TYPE_VP8:
    case XC_CODEC_TYPE_VID_CONFERENCE:
    case XC_CODEC_TYPE_AUD_CONFERENCE:
    case XC_CODEC_TYPE_VORBIS:
    case XC_CODEC_TYPE_SILK:
    case XC_CODEC_TYPE_OPUS:

      return MP2PES_STREAM_PRIV_START;
    default:
      //return MP2PES_STREAM_PRIV_START;
      return MP2PES_STREAMTYPE_NOTSET;
  }
}

XC_CODEC_TYPE_T mp2_getCodecType(uint16_t streamType) {

  switch(streamType) {

    case MP2PES_STREAM_VIDEO_H262:
      return XC_CODEC_TYPE_H262;
    //case MP2PES_STREAM_VIDEO_H263:
    //  return XC_CODEC_TYPE_H263;
    case MP2PES_STREAM_VIDEO_MP4V:
      return XC_CODEC_TYPE_MPEG4V;
    case MP2PES_STREAM_VIDEO_H264:
      return XC_CODEC_TYPE_H264;

    case MP2PES_STREAM_AUDIO_AC3:
      return XC_CODEC_TYPE_AC3;
    case MP2PES_STREAM_AUDIO_AACADTS:
      return XC_CODEC_TYPE_AAC;
    case MP2PES_STREAM_AUDIO_AMR:
      return XC_CODEC_TYPE_AMRNB;
    case MP2PES_STREAM_AUDIO_G711:
      return XC_CODEC_TYPE_G711_MULAW;
    case MP2PES_STREAM_AUDIO_MP2A_L2:
      return XC_CODEC_TYPE_MPEGA_L2;
    case MP2PES_STREAM_AUDIO_MP2A_L3:
      return XC_CODEC_TYPE_MPEGA_L3;
   
    default:
      return XC_CODEC_TYPE_UNKNOWN;
  }
}


/*
static int reset_prev_aacAdtsHdr(CAPTURE_MP2_PROG_T *pProg) {
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
*/


MP2_PES_PROG_T *mp2_find_prog(MP2_PES_DESCR_T *pMp2, unsigned int progId) {
  unsigned int idx;
//fprintf(stderr, "searching %d active programs for %u\n", pMp2->activePrograms, progId);
  for(idx = 0; idx < pMp2->activePrograms; idx++) {
    if(progId == pMp2->progs[idx].pid.id) {
//fprintf(stderr, "found in [idx:%d]\n", idx);
      return &pMp2->progs[idx];
    }
  }
//fprintf(stderr, "not found\n");
  return NULL;
}

MP2_PES_PROG_T *mp2_new_prog(MP2_PES_DESCR_T *pMp2, unsigned int progPid) {
  unsigned int idx;
//fprintf(stderr, "new prog %d\n", progPid);
  if(progPid == 0) {
    return NULL;
  }

  for(idx = 0; idx < MP2PES_MAX_PROG; idx++) {
    if(pMp2->progs[idx].pid.id == 0) {
      pMp2->progs[idx].pid.id = progPid;
      pMp2->activePrograms++;
//fprintf(stderr, "[idx:%d] pid:%u activePrograms:%u\n", idx, progPid, pMp2->activePrograms);
      return &pMp2->progs[idx];
    }
  }
//fprintf(stderr, "new prog null\n");
  return NULL;
}

uint64_t mp2_parse_pts(const unsigned char *data) {
  uint64_t pts;

  // 33 bits data value (in 90Khz) out of 40 bits :
  // 4 bits prefix, 3 bits data, 1 bit marker, 15 bits data, 1 bit marker, 
  // 15 bits data, 1 bit marker
  // prefix = 0011 if pts & dts present... 2nd 5 byte field dts prefx = 0001
  // prefix = 0010 if only pts

  pts = (uint64_t)((*data >> 1) & 0x07) << 30;
  pts |= (htons( *((uint16_t *) &data[1])) >> 1) << 15;
  pts |=  htons( *((uint16_t *) &data[3])) >> 1;

  return pts;
}

uint64_t mp2_parse_pcr(const unsigned char *data, uint32_t *pPcr27Mhz) {
  uint64_t pcr90Hz;

  if(!data) {
    return 0;
  }

  pcr90Hz= (((uint64_t)htonl(*((uint32_t *) &data[1])) << 1) |
            (data[4] >> 7)) + 1;

  if(pPcr27Mhz) {
    *pPcr27Mhz = htonl(*((uint32_t *) &data[3])) & 0x0000001f;
  }

  return pcr90Hz;
}

int mp2_parse_pes(MP2_PES_PROG_T *pProg,
                 unsigned char *buf, unsigned int len, PKTQ_TIME_T *pTm, int *phaveTm) {

  uint8_t streamId;
  uint8_t hdrsLen;
  uint16_t pesTotLen;
  int payloadLen;
  unsigned char *pesHdr = NULL; 
  unsigned int idx = 0;


  if(pProg == NULL || len < 9) {
    LOG(X_ERROR("Invalid mpeg2 pes length: %u"), len);
    return -1;
  } else if(buf[0] != 0 || buf[1] != 0 || buf[2] != 0x01) {
    LOG(X_ERROR("Invalid MPEG-2 TS PES start code prefix: 0x%x 0x%x 0x%x"), buf[0], buf[1], buf[2]);
    return -1;
  }

  // check buf[0-3] == 0x00
  streamId = (uint8_t) buf[3];

  if(!mp2_check_pes_streamid(streamId)) {
    LOG(X_WARNING("Unsupported mpeg2 pes stream id:0x%x pid:0x%x stype:0x%x"), 
        streamId, pProg->pid.id, pProg->pid.streamtype);
  }

  pesTotLen = htons(*((uint16_t *) &buf[4]));
  idx += 6;
  pesHdr = &buf[idx];

  if((pesHdr[0] & 0xc0) != 0x80) {
    LOG(X_ERROR("Invalid mpeg2 pes header 0x%x 0x%x pid:0x%x stream id:0x%x"), 
        pesHdr[0], pesHdr[1], pProg->pid.id, streamId);
    return -1;
  }
  idx += 2;
  hdrsLen = buf[idx++];

  if(pTm) {

    // check PTS bit
    if((pesHdr[1] & 0x80) && hdrsLen >= 5) {
      pTm->pts = mp2_parse_pts(&buf[idx]);

      if(phaveTm) {
        *phaveTm = 1; 
      }

      // check DTS bit
      if((pesHdr[1] & 0x40)  && hdrsLen >= 10) {
        pTm->dts = mp2_parse_pts(&buf[idx + 5]);
      }
    } else if(phaveTm) {
      *phaveTm = 0;
    }
  }

  if(hdrsLen > 0) {
    idx += hdrsLen;
  }

  if(pesTotLen > hdrsLen + 3) {
    pesTotLen -= (hdrsLen + 3);
  }

  
#if defined(DUMP_MP2TS)
//if(pProg->pid==0x0801) 
  fprintf(stderr, "PES pid:0x%x id:0x%x,type:0x%x, len:%u hdrs:%u pts:%.3f, dts:%.3f\n", 
    pProg->pid.id, streamId, pProg->pid.streamtype, len, hdrsLen, pTm ?  (double)pTm->pts/90000.0f : 0, pTm ? (double)pTm->dts/90000.0f : 0);
#endif // DUMP_MP2TS

  if((payloadLen = len - idx) < 0) {
    LOG(X_ERROR("Invalid pes payload length: %d"), payloadLen);
    return -1;
  }

  if(pProg->streamId != streamId) {
    pProg->streamId = streamId;
  }
  pProg->nextPesTotLen = pesTotLen;

  //write_payload(pSp, pProg, pkt, payloadLen, len);

  return idx;
}


// program association table
int mp2_parse_pat(MP2_PAT_T *pPat,
                  unsigned char *buf, unsigned int len) {
  uint16_t sectionlen;
  uint16_t tsid;
  //uint16_t prognum;
  uint16_t mappid;
  uint8_t secnum;
  uint8_t lastsecnum;
  uint8_t version;
  uint8_t isPatActive = 0;
  unsigned int idx = 0;
  unsigned int numProgMapPid = 0;
  unsigned int iter;

  // iso13818-1 table 2-25 Program Association Table
  // table id (0 form table 2-26)                8 bit
  // section_syntax_indicator (always 1)         1 bit
  // 0                                           1 bit 
  // reserved (11)                               2 bit
  // section length (data len after this field) 12 bit
  // transport stream id                        16 bit
  // reserved (11)                               2 bit
  // version number                              5 bit
  // current next indicator                      1 bit
  // section number                              8 bit
  // last section number                         8 bit

  // for each nth section...until section length reached
  // program number (0 or 1)                     16 bit
  // reserved (111)                               3 bit
  // if program_number=0 network pid
  //  else program map pid                      13 bit

  // CRC                                        32 bit

  //eg. 00: 00:b0:0d:9f:57:c9:00:00:00:01:e0:42:39:4e:d1:80


//ENSURE IS DONE BEFORE IN CALLER
  //if(pkt->hdr & MP2TS_HDR_PAYLOAD_UNIT_START) {
  //  pkt->idx++;
  //}

  if(idx + 8 > len) {
    LOG(X_ERROR("Invalid mpeg2 pat length: %u"), len);
    return -1;
  } else if(buf[0] != 0) {
    LOG(X_ERROR("Invalid mpeg2 pat table id: %u"), buf[idx]);

    //dump_hex(stderr, buf, 32);

    return -1;
  }
  
  sectionlen = htons(*((uint16_t *) &buf[idx + 1])) & 0x0fff ;
  if(sectionlen > (len - idx) - 3) {
    LOG(X_ERROR("Invalid mpeg2 pat section len: %u, max: %u"), 
                sectionlen, (len - idx) - 3);
    return -1;
  }
  tsid = *((uint16_t *) &buf[idx + 3]);
  version = (buf[idx + 5] >> 1) & 0x1f;
  isPatActive = buf[idx + 5] & 0x01;
  secnum = buf[idx + 6];
  lastsecnum = buf[idx + 7];
  idx += 8;

  if(!isPatActive) {
    LOG(X_WARNING("pat pid:0x%x ver:%u tsid:0x%x not yet active"), 0, 
    pPat->tsid, pPat->version);
    return 0;
  }

  pPat->tsid = tsid;
  pPat->version = version;
  pPat->havever = 1;

  for(iter = 0; iter <= lastsecnum; iter++) {

    if(idx + 4 > len) {
      LOG(X_ERROR("Invalid mpeg2 pat length: %u (section-idx: %u / %u)"), 
          len, idx, secnum+1);
      return -1;
    }

    pPat->prognum = htons(*((uint16_t *) &buf[idx]));
    mappid = htons(*((uint16_t *) &buf[idx + 2])) & 0x1fff;
    idx += 4;

    if(pPat->prognum == 0) {
      // mappid refereces network information table
      pPat->progmappid = 0;
    } else {

      //pPat->lastPatVersion = version;
      //pPat->lastPatPrognum = pPat->prognum;

      if(++numProgMapPid > 1 && pPat->progmappid != mappid) {
        LOG(X_WARNING("mpeg2 pat contains mulitple program map ids: %u"), mappid);
      } else if(pPat->progmappid != 0 && pPat->progmappid != mappid) {
        LOG(X_WARNING("mpeg2 pat program map changes from %u -> %u, tsid:0x%x"), 
          pPat->progmappid, mappid, pPat->tsid);
      }
      pPat->progmappid = mappid;
    }
    idx += 4;
    // 4 byte CRC
  }

 

#if defined(DUMP_MP2TS)
  fprintf(stderr, "PAT pid:0x%x, tsid:0x%x, ver:%u, sec:%u/%u, "
                  "map:0x%x, pnum:0x%x\n", 
    0,  htons(tsid), pPat->version, secnum, 
    lastsecnum, pPat->progmappid, pPat->prognum);
#endif // DUMP_MP2TS

  return idx;
}


// program map table
int mp2_parse_pmt(MP2_PMT_T *pPmt,
                  unsigned int progId,
                  void *pMp2CbData,
                  CBMP2PMT_ONPROGADD cbOnProgAdd,
                  unsigned char *buf, unsigned int len,
                  int newpmtonly) {

  uint16_t sectionlen;
  uint16_t prognum;
  uint16_t proginfolen;
  uint8_t streamtype;
  uint8_t version;
  uint16_t elempid;
  uint16_t esinfolen;
  unsigned int pcrPid;
  int isPmtActive = 0;
  unsigned int proginfoidx = 0;
  unsigned int endByteIdx = 0;
  unsigned int idx = 0;
  unsigned int cbIdx = 0;

  // iso13818-1 table 2-28 Program Map Table
  // table id (2 form table 2-26)                8 bit
  // section_syntax_indicator (always 1)         1 bit
  // 0                                           1 bit 
  // reserved (11)                               2 bit 
  // section length (data len after this field) 12 bit
  // program num                                16 bit
  // reserved (11)                               2 bit
  // version number                              5 bit
  // current next indicator                      1 bit
  // section number (0)                          8 bit
  // last section number (0)                     8 bit
  // reserved (111)                              3 bit
  // PCR pid                                    13 bit
  // reserved (1111)                             4 bit
  // prog info len                              12 bit

  // one or more program descriptors table 2-39 
  // descriptor type                             8 bit
  // descriptor len(data len after this field)   8 bit
  // .. descriptor specific

  // for each nth section...until section length reached
  // stream type (table 2-29)                    8 bit
  // reserved (111)                              3 bit
  // elementary pid                             13 bit
  // reserved (1111)                             4 bit
  // elementary stream info len                 12 bit

  // CRC                                        32 bit

  //eg.  00: 02:b0:26:00:01:c1:00:00:e0:45:f0:0f:(iods 1d:0d:11:01:
  //         02:80:80:07:00:4f:ff:ff:fe:fe:ff):0f:e0:44:f0:00:
  //         1b:e0:45:f0:00:55:68:df:d8


//ENSURE IS DONE BEFORE IN CALLER
  //if(pkt->hdr & MP2TS_HDR_PAYLOAD_UNIT_START) {
  //  pkt->idx++;
  //}

  if(idx + 8 > len) {
    LOG(X_ERROR("Invalid mpeg2 pmt length: %u"), len);
    return -1;
  } else if(buf[0] != 0x02) {
    LOG(X_ERROR("Invalid mpeg2 pmt table id: %u"), buf[0]);
    return -1;
  }

  sectionlen = htons(*((uint16_t *) &buf[1])) & 0x0fff ;
  endByteIdx =  3 + sectionlen;
  if(endByteIdx > len) {
    LOG(X_ERROR("Invalid mpeg2 pmt section len: %u, max: %u"), 
      sectionlen, len - 3);
    return -1;
  }
  prognum = htons( *((uint16_t *) &buf[3]) );
  version = (buf[5] >> 1) & 0x1f;
  isPmtActive = buf[5] & 0x01;
  //8 bit section num should always be 0
  //8 bit last section num should always be 0
  pcrPid = htons(*((uint16_t *) &buf[8])) & 0x1fff;
  proginfolen = htons(*((uint16_t *) &buf[10])) & 0x0fff;

  if(newpmtonly && (pPmt->havever && version == pPmt->version)) {
    return 0;
  }

  idx += 12;

  if(idx + proginfolen > len) {
    LOG(X_ERROR("Invalid mpeg2 pmt proginfolen: %u"), proginfolen);
    return -1;
  } else if(pcrPid == 0) {
    LOG(X_ERROR("Invalid mpeg2 PCR pid:0x%x in mpeg pmt"), pcrPid);
    return -1;
  }

  //if(pPmt->pcrpid != 0 && 
  //  (version != pPmt->version || prognum != pPmt->prognum)) {
  //    LOG(X_DEBUG("pmt changes ver: %u->%u, prognum: %u->%u"), 
  //      pPmt->version, version,
  //      pPmt->prognum, prognum);
  //}

  if(!isPmtActive) {
    LOG(X_DEBUG("pmt pid:0x%x ver:%u pnum:0x%x not yet active"),  
        progId, prognum, version);

    return 0;
  }


  if(proginfolen > 0 && (pPmt->pcrpid == 0 || !pPmt->havever || pPmt->version != version)) {
    LOG(X_DEBUG("pmt pid:0x%x skipping prog info len:%u"), 
        progId, proginfolen);
  }

  idx += proginfolen;

  while(idx < endByteIdx - 4) {

    if(proginfoidx >= MP2PES_MAX_PROG) {
      LOG(X_ERROR("pmt pid: 0x%x has > %d maximum programs\n"), progId, MP2PES_MAX_PROG);
      break;
    }

    streamtype = buf[idx];
    elempid = htons(*((uint16_t *) &buf[idx + 1])) & 0x1fff;
    esinfolen = htons(*((uint16_t *) &buf[idx + 3])) & 0x0fff;
    idx += 5;

    if(esinfolen > 0) {

      if(pPmt->pcrpid == 0 || !pPmt->havever || pPmt->version != version) {
//fprintf(stderr, "%u %u %u!=%u\n", pPmt->pcrpid,pPmt->havever,pPmt->version,version);
        LOG(X_DEBUG("pmt pid:0x%x elem pid:0x%x skipping elem stream info len:%u"), 
            progId, elempid, esinfolen);
      }

      idx += esinfolen;
      if(idx > endByteIdx) {
        LOG(X_ERROR("Invalid mpeg2 pmt proginfo[%u] es info len: %u"), 
                proginfoidx, esinfolen);
        return -1;
      }
    }

#if defined(DUMP_MP2TS)
  fprintf(stderr, "     pmt pid:0x%x has streamtype:0x%x elempid:0x%x\n", 
          progId, streamtype, elempid);
#endif // DUMP_MP2TS


    //if(cbOnProgAdd) {
    //  cbOnProgAdd(pMp2CbData, elempid, streamtype);
    //}

    pPmt->pids[proginfoidx].id = elempid;
    pPmt->pids[proginfoidx].streamtype = streamtype;
    //fprintf(stderr, "PMT --- prog[%d] pid:0x%x, st:0x%x\n", proginfoidx, pPmt->pids[proginfoidx].id, pPmt->pids[proginfoidx].streamtype);
    proginfoidx++;
  }

  pPmt->numpids = proginfoidx;
  pPmt->pcrpid = pcrPid;
  pPmt->prognum = prognum;
  pPmt->version = version;
  pPmt->havever = 1;

  idx += 4; // CRC

  if(cbOnProgAdd) {
    for(cbIdx = 0; cbIdx < proginfoidx; cbIdx++) {
      cbOnProgAdd(pMp2CbData, &pPmt->pids[cbIdx]);
    }
  }

#if defined(DUMP_MP2TS)
  fprintf(stderr, "PMT pid:0x%x,  prog:0x%x, ver:%u\n", 
    progId, prognum,  version);
#endif // DUMP_MP2TS

  return idx;
}

int mp2_parse_adaptation(unsigned char *buf,
                         unsigned int len,                      
                         unsigned int pid, 
                         unsigned int pcrpid, 
                         uint64_t *pPcr90Khz, 
                         uint32_t *pPcr27Mhz) {
  unsigned int idx = 0;
  uint8_t lenadaptation; 
  uint8_t hdr;

  if(len < 2) {
    LOG(X_ERROR("Invalid mpeg2-ts adaptation length: %u"), len);
    return -1;
  }

  lenadaptation = (uint8_t) buf[idx++];
  hdr = (uint8_t) buf[idx];

  if(lenadaptation + idx > len) {
    LOG(X_ERROR("Invalid mpeg2-ts adaptation field length: %u"), len);
    return -1;
  } else if(lenadaptation == 0) {
//fprintf(stderr, "adaptation len:%u\n", lenadaptation+1);
    return 1;
  }

  if(hdr & MP2TS_ADAPTATION_PCR) {
//fprintf(stderr, "pcr in adaptation\n");
    if(pid == pcrpid && pPcr90Khz) {
      *pPcr90Khz = mp2_parse_pcr(&buf[idx], pPcr27Mhz);
//fprintf(stderr, "pcr in adaptation pid:0x%x %llu\n", pid, *pPcr90Khz);
    }
  }

  //pPkt->idx += len;
//fprintf(stderr, "adaptation len:%u\n", lenadaptation+1);
  return lenadaptation + 1;
}

