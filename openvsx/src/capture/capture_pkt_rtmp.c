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

/*
static double delme_hton_dbl(const unsigned char *arg) {
  double d;
  uint32_t ui = *((uint32_t *)arg);
  uint32_t ui2 =  *((uint32_t *) &((arg)[4]));

  *((uint32_t *)&d) = htonl(ui2);
  *((uint32_t *)&(((unsigned char *)&d)[4])) = htonl(ui);

  return  d;
}
*/


static int create_outputFile(RTMP_RECORD_T *pRtmp,
                             enum MEDIA_FILE_TYPE mediaType) {

  FILE_STREAM_T *pFs = NULL;

  if(codectype_isVid(mediaType)) {
    pFs = &pRtmp->fsVid;
  } else if(codectype_isAud(mediaType)) {
    pFs = &pRtmp->fsAud;
  } else {
    return -1;
  }

  if(pFs->fp != FILEOPS_INVALID_FP) {
    CloseMediaFile(pFs);
  }

  if(capture_createOutPath(pFs->filename, sizeof(pFs->filename),
                        pRtmp->outputDir, "rtmp",
                        NULL, mediaType) < 0) {
    LOG(X_ERROR("Invalid output file name or directory."));
    return -1;
  }

  if(capture_openOutputFile(pFs, pRtmp->overWriteFile, pRtmp->outputDir) < 0) {
    return -1;
  }

  LOG(X_INFO("Created '%s'"), pFs->filename);

  return 0;
}

static int handle_audpkt_aac(RTMP_RECORD_T *pRtmp, FLV_TAG_AUDIO_T *pTag, 
                      unsigned char *pData, unsigned int len) {

  int rc = 0;
  int idx;
  unsigned char buf[32];

  if(pTag->aac.pkttype == FLV_AUD_AAC_PKTTYPE_SEQHDR) {

    if((rc = esds_decodeDecoderSp(pData, len, &pRtmp->ctxt.av.aud.codecCtxt.aac.esds)) < 0) {
      return rc;
    }
    if(!pRtmp->ctxt.av.aud.haveSeqHdr) {
      if((rc = create_outputFile(pRtmp, MEDIA_FILE_TYPE_AAC_ADTS)) < 0) {
        return rc;
      }

      pRtmp->ctxt.av.aud.haveSeqHdr = 1;
    }

  } else if(pTag->aac.pkttype == FLV_AUD_AAC_PKTTYPE_RAW) {

    if(!pRtmp->ctxt.av.aud.haveSeqHdr) {
      return 0;
    } else if(pRtmp->fsAud.fp == FILEOPS_INVALID_FP) {
      return -1;
    }

    if((idx = aac_encodeADTSHdr(buf, sizeof(buf),
                 &pRtmp->ctxt.av.aud.codecCtxt.aac.esds, len)) < 0) {
      LOG(X_ERROR("Failed to create ADTS header for frame"));
      return -1;
    }
    if((rc = WriteFileStream(&pRtmp->fsAud, buf, idx)) < 0) {
      return -1;
    }
    if((rc = WriteFileStream(&pRtmp->fsAud, pData, len)) < 0) {
      return -1;
    }

  }

  return rc;
}

static int handle_audpkt_mp3(RTMP_RECORD_T *pRtmp, FLV_TAG_AUDIO_T *pTag, 
                      unsigned char *pData, unsigned int len) {

  int rc = 0;

  fprintf(stderr, "aud mp3 pkt len:%d\n", len);
  //avc_dumpHex(stderr, pData, len > 32 ? 32 : len, 1);

  return rc;
}

//static H264_DECODER_CTXT_T g_h264;

static int handle_vidpkt_avc(RTMP_RECORD_T *pRtmp, FLV_TAG_VIDEO_T *pTag, 
                      unsigned char *pData, unsigned int len) {
  int rc = 0;
  int lennal;
  unsigned int idx = 0;

/*
  int slice_type;
  int frame_idx;
  int poc;
  BIT_STREAM_T bs;
  memset(&bs, 0, sizeof(bs));
  bs.buf = pData+5;
  bs.sz = len-5;
  bits_readExpGolomb(&bs);
  if((slice_type = bits_readExpGolomb(&bs)) > H264_SLICE_TYPE_SI) slice_type -= (H264_SLICE_TYPE_SI + 1);
  bits_readExpGolomb(&bs);
  frame_idx = bits_read(&bs, 7);
  poc = bits_read(&bs, 7);
  fprintf(stderr, "H264 SLICE TYPE %d fridx:%d poc:%d\n", slice_type, frame_idx, poc);
  avc_dumpHex(stderr, pData, len > 32 ? 32 : len, 1);
  //if(h264_decode_NALHdr(&g_h264, &bs, 0) >= 0) {
  //  fprintf(stderr, "SLICE_TYPE:%d frIdx:%d poc:%d\n", g_h264.lastSliceType, g_h264.frameIdx, g_h264.picOrderCnt);
  //}
*/

  if(pTag->avc.pkttype == FLV_VID_AVC_PKTTYPE_SEQHDR) {

    avcc_freeCfg(&pRtmp->ctxt.av.vid.codecCtxt.h264.avcc);
    if((rc = avcc_initCfg(pData, len, &pRtmp->ctxt.av.vid.codecCtxt.h264.avcc)) < 0) {
      return rc; 
    }
    if(!pRtmp->ctxt.av.vid.haveSeqHdr) {
      if((rc = create_outputFile(pRtmp, MEDIA_FILE_TYPE_H264b)) < 0) {
        return rc;
      }

      if((rc = avcc_writeSPSPPS(&pRtmp->fsVid, &pRtmp->ctxt.av.vid.codecCtxt.h264.avcc, 0, 0))) {
        return rc;
      }

      pRtmp->ctxt.av.vid.haveSeqHdr = 1;
    }

  } else if(pTag->avc.pkttype == FLV_VID_AVC_PKTTYPE_NALU) {

    if(!pRtmp->ctxt.av.vid.haveSeqHdr) {
      return 0;
    } else if(pRtmp->fsVid.fp == FILEOPS_INVALID_FP) {
      return -1;
    }

    //fprintf(stderr, "avc pkt len:%u time:%u\n", len, pTag->avc.comptime32);
    //dumpHex(stderr, pData, len > 16 ?16 : len);

    while(idx < len) {
   
      if((lennal = h264_getAvcCNalLength(&pData[idx], len - idx,
          pRtmp->ctxt.av.vid.codecCtxt.h264.avcc.lenminusone + 1)) < 0) {
        return -1;
      }
      idx += pRtmp->ctxt.av.vid.codecCtxt.h264.avcc.lenminusone + 1;

      //fprintf(stderr, "h.264 nal len:%d\n", lennal);

      if((rc = avcc_writeNAL(&pRtmp->fsVid, &pData[idx], lennal)) < 0) {
        break;
      }

      idx += lennal;
    }

  }

  return rc;
}

static int handle_vidpkt(RTMP_RECORD_T *pRtmp, unsigned char *pData, unsigned int len) {
  int rc = 0;
  FLV_TAG_VIDEO_T vidTag;

  //fprintf(stderr, "vid pkt len:%d\n", len);
  //avc_dumpHex(stderr, pData, len > 48 ? 48 : len, 1);

  if(len < 5) {
    return -1;
  }

  vidTag.frmtypecodec = pData[0];
  switch((vidTag.frmtypecodec & 0x0f)) {
    case FLV_VID_CODEC_AVC:
      vidTag.avc.pkttype = pData[1];
      vidTag.avc.comptime32 = htonl(*((uint32_t *)&pData[2]) << 8);
      //fprintf(stderr, "comptime32:%d\n", vidTag.avc.comptime32);
      rc = handle_vidpkt_avc(pRtmp, &vidTag, &pData[5], len - 5);
      break;
    case FLV_VID_CODEC_ON2VP6:

      fprintf(stderr, "vid vp6 pkt len:%d\n", len);
      avc_dumpHex(stderr, pData, len > 48 ? 48 : len, 1);
      break;
    default:
      LOG(X_WARNING("FLV video codec id %d not supported"), (vidTag.frmtypecodec & 0x0f));
      return -1;
  }

  return rc;
}

static int handle_audpkt(RTMP_RECORD_T *pRtmp, unsigned char *pData, unsigned int len) {
  int rc = 0;
  FLV_TAG_AUDIO_T audTag;

  //fprintf(stderr, "aud pkt len:%d\n", len);
  //avc_dumpHex(stderr, pData, len > 48 ? 48 : len, 1);

  if(len < 1) {
    return -1;
  }

  audTag.frmratesz = pData[0];

  switch((audTag.frmratesz >> 4)) {
    case  FLV_AUD_CODEC_AAC:
      if(len < 2) {
        return -1;
      }
      audTag.aac.pkttype = pData[1];
      rc = handle_audpkt_aac(pRtmp, &audTag, &pData[2], len - 2);
      break;
    case FLV_AUD_CODEC_MP3:
      rc = handle_audpkt_mp3(pRtmp, &audTag,  &pData[1], len - 1);
      break;
    default:
      LOG(X_WARNING("FLV audio codec id %d not supported"), (audTag.frmratesz >> 4));
      return -1;
  }

  //fprintf(stderr, "audio len:%u 0x%x 0x%x 0x%x 0x%x ... 0x%x 0x%x 0x%x 0x%x\n", len, pData[0], pData[1], pData[2], pData[3], len>3 ? pData[len-4] :0 , len>2 ? pData[len-3]:0,len>1? pData[len-2]:0, len>0?pData[len-1]:0);

  return rc;
}

static int handle_flv(RTMP_RECORD_T *pRtmp, unsigned char *pData, unsigned int len) {
  int rc  = 0;
  unsigned int idx = 0;
  FLV_TAG_HDR_T hdr;

  //fprintf(stderr, "flv pkt len:%d\n", len);
  //avc_dumpHex(stderr, pData, len > 48 ? 48 : len, 1);

  while(idx < len) {

    if(idx + 11 >= len) {
      LOG(X_WARNING("RTMP FLV index %d/%d insufficient header length"), idx, len);
      return rc;
    }

    memset(&hdr, 0, sizeof(hdr));

    if(idx > 0) {
      if(idx + 15 >= len) {
        LOG(X_WARNING("RTMP FLV index %d/%d insufficient previous pkt length"), 
            idx, len);
        return rc;
      }
      hdr.szprev = htonl(*((uint32_t *) &pData[idx]));
      idx += 4;
    }
    hdr.type = pData[idx++];

    memcpy(&hdr.size32, &pData[idx], 3);
    idx += 3;
    hdr.size32 = htonl(hdr.size32 << 8);

    memcpy(&hdr.timestamp32, &pData[idx], 3);
    idx += 3;
    hdr.tsext = pData[idx++];
    hdr.timestamp32 = htonl(hdr.timestamp32 << 8);
    hdr.timestamp32 |= (hdr.tsext << 24);

    memcpy(&hdr.streamid, &pData[idx], 3);
    idx += 3;

    hdr.szhdr = 11;

    //fprintf(stderr, "FLV PKT TYPE:0x%x LEN:%d (%d/%d) ts:%u (0x%x %x %x %x)\n", hdr.type, hdr.size32, idx, len, hdr.timestamp32, pData[idx-7], pData[idx-6], pData[idx-5], pData[idx-4]);

    switch(hdr.type) {
      case RTMP_CONTENT_TYPE_VIDDATA:
        handle_vidpkt(pRtmp, &pData[idx], hdr.size32);
        break;
      case RTMP_CONTENT_TYPE_AUDDATA:
        handle_audpkt(pRtmp, &pData[idx], hdr.size32);
        break;
      default:
        fprintf(stderr, "Unhandled flv packet type: 0x%x\n", hdr.type);
        break;
    }

    idx += hdr.size32;

  }

  return rc;
}

int rtmp_cbReadDataBuf(void *pArg, unsigned char *pData, unsigned int len) {
  RTMP_RECORD_T *pRtmp = (RTMP_RECORD_T *) pArg;
  int lenread = len;

#ifdef DEBUG_RTMP_READ
  fprintf(stderr, "rtmp_cbReadDataBuf len:%d (buf:%d/%d)\n", len, pRtmp->bsIn.idx, pRtmp->bsIn.sz);
#endif // DEBUG_RTMP_READ

  if(!pRtmp->bsIn.buf) {
    return -1;
  } else if(pRtmp->bsIn.idx + len > pRtmp->bsIn.sz) {
    lenread = pRtmp->bsIn.sz - pRtmp->bsIn.idx;
  }
  
  // lenread may be a partial where lenread < len 
  if(lenread > 0) {
    memcpy(pData, &pRtmp->bsIn.buf[pRtmp->bsIn.idx], lenread);
    pRtmp->bsIn.idx += lenread;

#ifdef DEBUG_RTMP_READ
    if(lenread>=4)
      fprintf(stderr, "0x%x 0x%x 0x%x 0x%x ... 0x%x 0x%x 0x%x 0x%x\n", pData[0], pData[1], pData[2], pData[3], pData[lenread-4], pData[lenread-3], pData[lenread-2], pData[lenread-1]);
#endif // DEBUG_RTMP_READ

  }

  return lenread;
}

int rtmp_record_init(RTMP_RECORD_T *pRtmp) {
  int rc = 0;

  if(!pRtmp) {
    return -1;
  }

  memset(pRtmp, 0, sizeof(RTMP_RECORD_T));

  if((rc = rtmp_parse_init(&pRtmp->ctxt, RTMP_TMPFRAME_VID_SZ, 0)) < 0) {
    return rc;
  }

  //fprintf(stderr, "rtmp_record_init 0x%x\n", pRtmp);

  pRtmp->ctxt.state = RTMP_STATE_INVALID;
  pRtmp->ctxt.cbRead = rtmp_cbReadDataBuf;
  pRtmp->ctxt.pCbData = pRtmp;

  pRtmp->state.handshake = RTMP_HANDSHAKE_STATE_INPROGRESS;
  pRtmp->state.handshakeIdx = 0;

  fprintf(stderr, "rtmp_record_init chunksz:%u\n", pRtmp->ctxt.chunkSz); 


  return rc;
}

void rtmp_record_close(RTMP_RECORD_T *pRtmp) {

  if(!pRtmp) {
    return;
  }

  CloseMediaFile(&pRtmp->fsVid);
  CloseMediaFile(&pRtmp->fsAud);

  avcc_freeCfg(&pRtmp->ctxt.av.vid.codecCtxt.h264.avcc);
  rtmp_parse_close(&pRtmp->ctxt);

}


#ifdef DEBUG_RTMP_READ
static struct timeval delmetv0;
void printPkt(const RTMP_RECORD_T *pRtmp, const COLLECT_STREAM_PKTDATA_T *pPkt,
              const char *prfx) {
      fprintf(stderr, "-0x%llx- %s %u:%u %s rtmp full pkt sz:%d, ct:%d, id:%d, ts:%d(%d), dest:0x%x, hdr:%d\n", pRtmp->prevPktOffset, pRtmp->pDescr ? pRtmp->pDescr : "", pPkt->tv.tv_sec-delmetv0.tv_sec, pPkt->tv.tv_usec/1000, (prfx ? prfx : ""), pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.szPkt, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.contentType, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.id, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.ts, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].tsAbsolute, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.dest, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].szHdr0);
}
#endif // DEBUG_RTMP_READ


int cbOnPkt_rtmp(void *pArg, const COLLECT_STREAM_PKTDATA_T *pPkt) {

  int rc = 0;
  unsigned int handshakelen = 0;
  uint32_t offset = 0;
  CAPTURE_CBDATA_SP_T *pSp = (CAPTURE_CBDATA_SP_T *) pArg;
  RTMP_RECORD_T *pRtmp = (RTMP_RECORD_T *) &pSp->u.rtmp;
  const unsigned char *pData = pPkt->payload.pData;
  unsigned int len = PKTCAPLEN(pPkt->payload);
  unsigned int idx;
  FLV_AMF_T *pAmf;
  FLV_AMF_T amfEntries[20];


#ifdef DEBUG_RTMP_READ
  fprintf(stderr, "rtmp_ondata(0x%x) len:%d 0x%x 0x%x 0x%x 0x%x chunkSz:%u hshk:%d\n", pRtmp, len, pData[0], pData[1], pData[2], pData[3], pRtmp->ctxt.chunkSz, pRtmp->state.handshake);
  //avc_dumpHex(stderr, pData, len > 1024 ? 1024 : len, 1);
#endif // DEBUG_RTMP_READ

  if(!pData || len < 0) {
    return -1;
  }

  if(pRtmp->state.handshake == RTMP_HANDSHAKE_STATE_INPROGRESS) {

    if(pRtmp->state.handshakeIdx + len < (RTMP_HANDSHAKE_SZ * 2) + 1) {
      fprintf(stderr, "rtmp_ondata skipping all of : %d\n", len);
      avc_dumpHex(stderr, pData, len, 1);
      pRtmp->state.handshakeIdx += len;
      return 0; 
    } else {
      handshakelen = (RTMP_HANDSHAKE_SZ * 2) + 1 - pRtmp->state.handshakeIdx;
      pRtmp->state.handshakeIdx += handshakelen;
      pRtmp->state.handshake = RTMP_HANDSHAKE_STATE_DONE;    

      fprintf(stderr, "rtmp_ondata skipping: %d/%d\n", handshakelen, len);
      avc_dumpHex(stderr, pData, len, 1);
      pData += handshakelen;
      if((len -= handshakelen) <= 0) {
        return 0;
      }
    }
  }

  pRtmp->bsIn.buf = (unsigned char *) pData;
  pRtmp->bsIn.sz = len;
  pRtmp->bsIn.idx = 0;

  do {

    if((rc = rtmp_parse_readpkt(&pRtmp->ctxt)) < 0) {

      LOG(X_ERROR("Failed to process rtmp data at stream 0x%x.  Discarding %d bytes"),
                  pRtmp->prevPktOffset, len);

      pRtmp->bytesTot += len;
      pRtmp->prevPktOffset = pRtmp->bytesTot;
      return rc;

    } else if(rc == 0) {
      // need to get more input data to read whole rtmp packet 

#ifdef DEBUG_RTMP_READ
      fprintf(stderr, "rtmp partial pkt len:%d/%d ct:%d, id:%d idxHdr:%d\n", pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].idxInPkt, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.szPkt, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.contentType, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.id, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].idxInHdr);
#endif // DEBUG_RTMP_READ
      break;

    } else {

   
#ifdef DEBUG_RTMP_READ
if(delmetv0.tv_sec==0)gettimeofday(&delmetv0, NULL);
      printPkt(pRtmp, pPkt, NULL); 
#endif // DEBUG_RTMP_READ

      // TODO: check packet and dispatch callbacks

      //bs.buf = pRtmp->ctxt.in.buf;
      //bs.sz = pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.szPkt;
      //bs.idx = 0;

      switch(pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.contentType) {
        case RTMP_CONTENT_TYPE_BYTESREAD:
          fprintf(stderr, "BYTES READ: %d\n", htonl(*((uint32_t *) &pRtmp->ctxt.in.buf[0])));

          avc_dumpHex(stderr, pRtmp->ctxt.in.buf, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.szPkt, 1);
          break;
        case RTMP_CONTENT_TYPE_PING:
          fprintf(stderr, "PING\n");
          avc_dumpHex(stderr, pRtmp->ctxt.in.buf, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.szPkt, 1);
          break;
        case RTMP_CONTENT_TYPE_INVOKE:
        case RTMP_CONTENT_TYPE_MSG:
        case RTMP_CONTENT_TYPE_NOTIFY:
          avc_dumpHex(stderr, pRtmp->ctxt.in.buf, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.szPkt, 1);

         
          memset(amfEntries, 0, sizeof(amfEntries));
          for(idx = 0; idx < (sizeof(amfEntries) / sizeof(amfEntries[0])) - 1; idx++) {
            amfEntries[idx].pnext = &amfEntries[idx + 1];
          }

          if(rtmp_parse_invoke(&pRtmp->ctxt, &amfEntries[0]) >= 0) {

            //
            // Dump key value elements
            //
            pAmf = &amfEntries[0];
            while(pAmf) {
              if(pAmf->key.p) {
                fprintf(stderr, "'%s'->", pAmf->key.p ? (char *)pAmf->key.p : "");
                if(pAmf->val.type == FLV_AMF_TYPE_STRING) {
                  fprintf(stderr, "'%s'\n", pAmf->val.u.str.p);
                } else if(pAmf->val.type == FLV_AMF_TYPE_NUMBER) {
                  fprintf(stderr, "%f\n", pAmf->val.u.d);
                } else {
                  fprintf(stderr, "\n");
                }
              }
              pAmf = pAmf->pnext;
            }
          }

          break;
        case RTMP_CONTENT_TYPE_VIDDATA:
          handle_vidpkt(pRtmp, pRtmp->ctxt.in.buf, 
                        pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.szPkt);
          break;
        case RTMP_CONTENT_TYPE_AUDDATA:
          handle_audpkt(pRtmp, pRtmp->ctxt.in.buf, 
                        pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.szPkt);
          break;
        case RTMP_CONTENT_TYPE_FLV:
          handle_flv(pRtmp, pRtmp->ctxt.in.buf, 
                     pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.szPkt);
          break;
        default: 
          fprintf(stderr, "Unhandled packet\n");
          avc_dumpHex(stderr, pRtmp->ctxt.in.buf, pRtmp->ctxt.pkt[pRtmp->ctxt.streamIdx].hdr.szPkt, 1);
          break;
      }

      offset += rc;
      pRtmp->prevPktOffset = pRtmp->bytesTot + offset; 

      // Reset the read data index for subsequent read
      pRtmp->ctxt.in.idx = 0;
    }

  } while(pRtmp->bsIn.idx < pRtmp->bsIn.sz);

  pRtmp->bytesTot += len;

  return rc;
}

#endif // VSX_HAVE_CAPTURE
