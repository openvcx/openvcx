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


static int rtmp_parse_chunksz(RTMP_CTXT_T *pRtmp) {
  int rc = 0;
  unsigned int chunkSzIn = 0;

  if(pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt - pRtmp->in.idx < 4) {
    LOG(X_ERROR("Failed to parse rtmp packet chunk size (body len:%u)"), 
                pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt - pRtmp->in.idx);
    return -1;
  }

  chunkSzIn = htonl(*((uint32_t *) &pRtmp->in.buf[pRtmp->in.idx]));

  if(chunkSzIn <= 0 || chunkSzIn > 0xffff) {
    LOG(X_WARNING("refusing to set rtmp chunk size to %u"), chunkSzIn);
  } else if(chunkSzIn != pRtmp->chunkSzIn) {
    LOG(X_DEBUG("set rtmp chunk size %u -> %u"), pRtmp->chunkSzIn, chunkSzIn);
    pRtmp->chunkSzIn = chunkSzIn;
  }  

  return rc;
}

static int rtmp_parse_readpkt_chunkhdr(RTMP_CTXT_T *pRtmp) {
  uint8_t streamIdx = 0;
  int rc;
  unsigned char buf[12];

  if(pRtmp->streamIdx >= RTMP_STREAM_INDEXES || pRtmp->pkt[pRtmp->streamIdx].idxInHdr == 0) {

#ifdef DEBUG_RTMP_READ
    fprintf(stderr, "rtmp_parse_readpkt_chunkhdr reading 1 streamIdx:%d (prev:%d), idxInHdr:%d\n", pRtmp->streamIdx, pRtmp->streamIdxPrev, pRtmp->streamIdx < RTMP_STREAM_INDEXES ? pRtmp->pkt[pRtmp->streamIdx].idxInHdr : -1);
#endif // DEBUG_RTMP_READ

    if((rc = pRtmp->cbRead(pRtmp->pCbData, buf, 1)) < 0) { 
      return rc;
    } else if(rc == 0) {
      // the data is not available at this time
      return 0;
    }

    gettimeofday(&pRtmp->tvLastRd, NULL);

    streamIdx = buf[0] & 0x3f;
    if(streamIdx < RTMP_STREAM_IDX_CTRL || streamIdx > RTMP_STREAM_IDX_MAX) {
      LOG(X_ERROR("Invalid rtmp packet header id 0x%x"), buf[0]);
      return -1;
    }
    streamIdx -= RTMP_STREAM_IDX_CTRL;

    if(pRtmp->streamIdxPrev < RTMP_STREAM_INDEXES &&
       pRtmp->streamIdxPrev != streamIdx &&
       pRtmp->pkt[pRtmp->streamIdxPrev].idxInPkt < pRtmp->pkt[pRtmp->streamIdxPrev].hdr.szPkt) {

      LOG(X_DEBUG("Reading RTMP interleaved chunk (streamIdx:%d, streamIdxPrev:%u, idxInPkt:%u/%u)"),streamIdx,pRtmp->streamIdxPrev, pRtmp->pkt[pRtmp->streamIdxPrev].idxInPkt, pRtmp->pkt[pRtmp->streamIdxPrev].hdr.szPkt); 
      //LOG(X_ERROR("reading interleaved rtmp chunk not implemented (streamIdx:%d, streamIdxPrev:%u, idxInPkt:%u/%u)"),streamIdx,pRtmp->streamIdxPrev, pRtmp->pkt[pRtmp->streamIdxPrev].idxInPkt, pRtmp->pkt[pRtmp->streamIdxPrev].hdr.szPkt); 
      //return -1;
    }


    if(streamIdx != pRtmp->streamIdx) {

#ifdef DEBUG_RTMP_READ
    fprintf(stderr, "rtmp_parse_readpkt_chunkhdr resetting... streamIdx:%d, pRtmp->streamidx:%d, pRtmp->streamIdxPrev:%d, hdrsz:%d/%d id:%d chunk[%d/%d], pkt[%d/%d]\n", streamIdx, pRtmp->streamIdx, pRtmp->streamIdxPrev, pRtmp->pkt[pRtmp->streamIdxPrev].idxInHdr, pRtmp->pkt[pRtmp->streamIdxPrev].hdr.szHdr, pRtmp->pkt[pRtmp->streamIdxPrev].hdr.id, pRtmp->pkt[pRtmp->streamIdxPrev].idxInChunk, pRtmp->chunkSzIn, pRtmp->pkt[pRtmp->streamIdxPrev].idxInPkt, pRtmp->pkt[pRtmp->streamIdxPrev].hdr.szPkt);
#endif // DEBUG_RTMP_READ

      //Reset the previously used stream
      if(pRtmp->streamIdxPrev < RTMP_STREAM_INDEXES &&
         (pRtmp->pkt[pRtmp->streamIdxPrev].idxInPkt == 0 ||
          pRtmp->pkt[pRtmp->streamIdxPrev].idxInPkt == 
          pRtmp->pkt[pRtmp->streamIdxPrev].hdr.szPkt)) {
        pRtmp->pkt[pRtmp->streamIdxPrev].idxInHdr = 0;
        pRtmp->pkt[pRtmp->streamIdxPrev].idxInChunk= 0;
        pRtmp->pkt[pRtmp->streamIdxPrev].idxInPkt = 0;
      }
      //pRtmp->streamIdxPrev = pRtmp->streamIdx;
      pRtmp->streamIdx = streamIdx;
      pRtmp->streamIdxPrev = pRtmp->streamIdx;
    }

    pRtmp->pkt[streamIdx].idxInHdr = 1;
    pRtmp->pkt[streamIdx].hdr.id = (buf[0] & 0x3f);
    pRtmp->pkt[streamIdx].idxInChunk = 0;
    if(pRtmp->pkt[streamIdx].idxInPkt >= pRtmp->pkt[streamIdx].hdr.szPkt) {
      pRtmp->pkt[streamIdx].idxInPkt = 0;
    }

    switch(buf[0] >> 6) {
      case 0:
        // 0x00
        pRtmp->pkt[streamIdx].hdr.szHdr = 12; 
        break;
      case 1:
        // 0x40
        pRtmp->pkt[streamIdx].hdr.szHdr = 8; 
        break;
      case 2:
        // 0x80
        pRtmp->pkt[streamIdx].hdr.szHdr = 4; 
        break;
      case 3:
        // 0xc0
        pRtmp->pkt[streamIdx].hdr.szHdr = 1; 
        break;
      default:
        return -1;
    }

    if(pRtmp->pkt[streamIdx].idxInPkt == 0) {
      pRtmp->pkt[streamIdx].szHdr0 = pRtmp->pkt[streamIdx].hdr.szHdr;
    }

#ifdef DEBUG_RTMP_READ
    fprintf(stderr, "rtmp_parse_readpkt_chunkhdr hdrsz:%d/%d id:%d chunk[%d/%d], pkt[%d/%d]\n", pRtmp->pkt[streamIdx].idxInHdr, pRtmp->pkt[streamIdx].hdr.szHdr, pRtmp->pkt[streamIdx].hdr.id, pRtmp->pkt[streamIdx].idxInChunk, pRtmp->chunkSzIn, pRtmp->pkt[streamIdx].idxInPkt, pRtmp->pkt[streamIdx].hdr.szPkt);
#endif // DEBUG_RTMP_DUMP
 
  }

  // Read the rest of the chunk header
  if(pRtmp->pkt[pRtmp->streamIdx].idxInHdr > 0 &&
     pRtmp->pkt[pRtmp->streamIdx].idxInHdr < pRtmp->pkt[pRtmp->streamIdx].hdr.szHdr) {

    streamIdx = pRtmp->streamIdx;
    
    if((rc = pRtmp->cbRead(pRtmp->pCbData, 
          &pRtmp->pkt[streamIdx].hdr.buf[pRtmp->pkt[streamIdx].idxInHdr], 
          pRtmp->pkt[streamIdx].hdr.szHdr - pRtmp->pkt[streamIdx].idxInHdr)) < 0) { 
      return rc; 
    }
    //fprintf(stderr, "read rest of header:%d\n", rc);
    pRtmp->pkt[streamIdx].idxInHdr += rc;
    
    if(pRtmp->pkt[streamIdx].idxInHdr < pRtmp->pkt[streamIdx].hdr.szHdr) {
      //fprintf(stderr, "entire header not yet available %d/%d\n", pRtmp->pkt[streamIdx].idxInHdr, pRtmp->pkt[streamIdx].hdr.szHdr);
      // the entire chunk header is not available at this time
      return 0;
    }

    if(pRtmp->pkt[streamIdx].hdr.szHdr >=  4) {

      pRtmp->pkt[streamIdx].hdr.ts = 
              ntohl(*((uint32_t *) &pRtmp->pkt[streamIdx].hdr.buf[1]) << 8);
      // header size 12 appears to use absolute timestamps
      if(pRtmp->pkt[streamIdx].hdr.szHdr == 12) {
        pRtmp->pkt[streamIdx].tsAbsolute = pRtmp->pkt[streamIdx].hdr.ts;
      } else {
        pRtmp->pkt[streamIdx].tsAbsolute += pRtmp->pkt[streamIdx].hdr.ts;
      }

      if(pRtmp->pkt[streamIdx].hdr.szHdr >=  8) {

        pRtmp->pkt[streamIdx].hdr.szPkt = 
              ntohl(*((uint32_t *) &pRtmp->pkt[streamIdx].hdr.buf[4]) << 8);
        pRtmp->pkt[streamIdx].hdr.contentType = pRtmp->pkt[streamIdx].hdr.buf[7];

        if(pRtmp->pkt[streamIdx].hdr.szHdr == 12) {
          pRtmp->pkt[streamIdx].hdr.dest = 
                ntohl(*((uint32_t *) &pRtmp->pkt[streamIdx].hdr.buf[8]));
        }

        if(pRtmp->pkt[streamIdx].hdr.szPkt > pRtmp->in.sz) {
          LOG(X_ERROR("rtmp packet szPkt %u is too large for %u"), pRtmp->pkt[streamIdx].hdr.szPkt, pRtmp->in.sz);
          return -1;
        }
      }

    }

#ifdef DEBUG_RTMP_READ
    fprintf(stderr, "rtmp_parse_readpkt_chunkhdr ct:%d id:%d ts:%u(abs:%u) chunk[%d/%d], pkt[%d/%d]\n", pRtmp->pkt[streamIdx].hdr.contentType, pRtmp->pkt[streamIdx].hdr.id, pRtmp->pkt[streamIdx].hdr.ts, pRtmp->pkt[streamIdx].tsAbsolute, pRtmp->pkt[streamIdx].idxInChunk, pRtmp->chunkSzIn, pRtmp->pkt[streamIdx].idxInPkt, pRtmp->pkt[streamIdx].hdr.szPkt);
#endif // DEBUG_RTMP_READ

  } else if(pRtmp->pkt[pRtmp->streamIdx].hdr.szHdr == 1 && pRtmp->pkt[pRtmp->streamIdx].idxInPkt == 0 && pRtmp->pkt[pRtmp->streamIdx].hdr.ts > 0) {

    //fprintf(stderr, "ct:%d TSABSOLUTE:%u, TS:%u, IDXINHDR:%d, IDXINPKT:%d, IDXINCHUNK:%d\n", pRtmp->pkt[streamIdx].hdr.contentType, pRtmp->pkt[streamIdx].tsAbsolute, pRtmp->pkt[streamIdx].hdr.ts, pRtmp->pkt[pRtmp->streamIdx].idxInHdr, pRtmp->pkt[streamIdx].idxInPkt, pRtmp->pkt[streamIdx].idxInChunk);
    pRtmp->pkt[streamIdx].tsAbsolute += pRtmp->pkt[streamIdx].hdr.ts;
  }
  
  return pRtmp->pkt[pRtmp->streamIdx].idxInHdr;
}

static int rtmp_parse_readpkt_chunk(RTMP_CTXT_T *pRtmp) {
  int rc = 0;
  int readszhdr = 0;
  unsigned int readsz;

  if((readszhdr = rtmp_parse_readpkt_chunkhdr(pRtmp)) <= 0) {
    return readszhdr;
  }

  if((readsz = pRtmp->chunkSzIn - pRtmp->pkt[pRtmp->streamIdx].idxInChunk) > 
     pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt - pRtmp->pkt[pRtmp->streamIdx].idxInPkt) {
    readsz = pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt - pRtmp->pkt[pRtmp->streamIdx].idxInPkt;
  }

#ifdef DEBUG_RTMP_READ
  fprintf(stderr, "rtmp_parse_readpkt_chunk readsz:%u chunkSzIn:%u into idxInPkt[%d] idxInChunk:%d\n", readsz, pRtmp->chunkSzIn, pRtmp->pkt[pRtmp->streamIdx].idxInPkt, pRtmp->pkt[pRtmp->streamIdx].idxInChunk);
#endif // DEBUG_RTMP_READ

  if(pRtmp->pkt[pRtmp->streamIdx].idxInPkt + readsz > pRtmp->in.sz) {
    LOG(X_WARNING("RTMP packet 0x%x data not read at [idx:%d/%d].  Packet total size: %u "), 
      pRtmp->pkt[pRtmp->streamIdx].hdr.contentType, 
      pRtmp->pkt[pRtmp->streamIdx].idxInPkt, pRtmp->in.sz, pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt);

    rc = readsz;

  } else if(readsz > 0 && 
          (rc = pRtmp->cbRead(pRtmp->pCbData, &pRtmp->in.buf[pRtmp->pkt[pRtmp->streamIdx].idxInPkt], readsz)) < 0) {
    return rc; 
  } else if(readsz == 0) {
    rc = 0;
  }

  pRtmp->pkt[pRtmp->streamIdx].idxInChunk += rc;
  pRtmp->pkt[pRtmp->streamIdx].idxInPkt += rc;

  // reset stream idxInHdr to prepare for next stream chunk
  if(pRtmp->pkt[pRtmp->streamIdx].idxInChunk >= pRtmp->chunkSzIn ||
     pRtmp->pkt[pRtmp->streamIdx].idxInChunk >= pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt ||
     pRtmp->pkt[pRtmp->streamIdx].idxInPkt >= pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt) {
    pRtmp->pkt[pRtmp->streamIdx].idxInHdr = 0;
  }

  if(rc < readsz) {
    return 0;
  }

  return readszhdr + rc;
}

int rtmp_parse_readpkt(RTMP_CTXT_T *pRtmp) {
  int rc = 0;
  int numread = 0;

  if(!pRtmp || !pRtmp->cbRead || !pRtmp->pCbData) {
    return -1;
  }

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - parse_readpkt called... chunkSzIn: %d"), pRtmp->chunkSzIn) );

  do {

    if((rc = rtmp_parse_readpkt_chunk(pRtmp)) < 0) {

      if(pRtmp->streamIdx < RTMP_STREAM_INDEXES) {
        pRtmp->pkt[pRtmp->streamIdx].idxInHdr = 0;
        pRtmp->pkt[pRtmp->streamIdx].idxInChunk = 0;
        pRtmp->pkt[pRtmp->streamIdx].idxInPkt = 0;
      }

      // Reset input buffer after recovering from prior error
      pRtmp->streamIdx = RTMP_STREAM_INDEXES;
      pRtmp->in.idx = 0;

      return -1;
    } else if(rc == 0) {
      // Not all data in the chunk is available for a blocking read operation
      return rc;
    }

    numread += rc;

#ifdef DEBUG_RTMP_READ
  fprintf(stderr, "rtmp_parse_readpkt readsz:%u chunkSz:%u into idxInPkt[%d] idxInChunk:%d\n", numread, pRtmp->chunkSzIn, pRtmp->pkt[pRtmp->streamIdx].idxInPkt, pRtmp->pkt[pRtmp->streamIdx].idxInChunk);
#endif // DEBUG_RTMP_READ

  } while(pRtmp->pkt[pRtmp->streamIdx].idxInPkt < pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt);

  VSX_DEBUG_RTMP(
    LOG(X_DEBUG("RTMP - parse_readpkt have body sz:%d ct:0x%x, id:%d, ts:%d(%d), dest:0x%x, hdr:%d"),
       pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt, pRtmp->pkt[pRtmp->streamIdx].hdr.contentType,
       pRtmp->pkt[pRtmp->streamIdx].hdr.id, pRtmp->pkt[pRtmp->streamIdx].hdr.ts,
       pRtmp->pkt[pRtmp->streamIdx].tsAbsolute, pRtmp->pkt[pRtmp->streamIdx].hdr.dest, pRtmp->pkt[pRtmp->streamIdx].szHdr0);
    LOGHEXT_DEBUGV(pRtmp->in.buf, pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt > 16 ? 16 : pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt);
  );

  //
  // Handle any essential packets for properly decoding the rtmp stream
  //
  switch(pRtmp->pkt[pRtmp->streamIdx].hdr.contentType) {
    case RTMP_CONTENT_TYPE_CHUNKSZ:
      rc = rtmp_parse_chunksz(pRtmp);
      break;
    default:
      break;
  }

  return numread;
}

static int rtmp_parse_amf(BYTE_STREAM_T *bs, FLV_AMF_T *pAmfKeyVal) {
  uint8_t amfType;

  if(bs->buf[bs->idx] >= FLV_AMF_TYPE_STRING) {
    amfType = bs->buf[bs->idx++];
  } else if(bs->idx + 2 < bs->sz && bs->buf[bs->idx] == 0x00 && 
            bs->buf[bs->idx + 1] == 0x00 && bs->buf[bs->idx + 2] == 0x09) {
    bs->idx += 3;
    return 0;
  } else {
    amfType = FLV_AMF_TYPE_STRING;
  }

  //
  // Prevent resetting FLV_AMF_T::pnext
  //
  memset(&pAmfKeyVal->key, 0, sizeof(pAmfKeyVal->key));
  memset(&pAmfKeyVal->val, 0, sizeof(pAmfKeyVal->val));

  if(flv_amf_read(amfType, pAmfKeyVal, bs, 0) < 0) {
    //fbs.idx = bs->idx;
    LOG(X_ERROR("Failed to read amf object with key 0x%x"), amfType);
    return -1;
  }

  //fbs.idx = bs->idx;

  return 1;
}
static int rtmp_parse_invoke_generic(RTMP_CTXT_T *pRtmp, BYTE_STREAM_T *bs, 
                                     FLV_AMF_T *pAmfList) {

  int rc;
  FLV_AMF_T amfKeyVal;
  FLV_AMF_T *pAmf;

  if(pAmfList->val.type == FLV_AMF_TYPE_NUMBER) {
    //fprintf(stderr, "%s: %f\n", pAmfList->key.p, pAmfList->val.u.d);
  }

  amfKeyVal.pnext = NULL;
  pAmf = pAmfList->pnext;

  while(bs->idx < bs->sz) {

    if(!pAmf) {
      pAmf = &amfKeyVal;
    }

    if((rc = rtmp_parse_amf(bs, pAmf)) < 0) {
      return -1; 
    } else if(rc == 0) {
      continue;
    }

    //fprintf(stderr, "%d/%d '%s'->", bs->idx, bs->sz, pAmf->key.p ? (char *)pAmf->key.p : "");

    //if(pAmf->val.type == FLV_AMF_TYPE_STRING) {
    //  fprintf(stderr, "'%s'\n", pAmf->val.u.str.p);
    //} else if(pAmf->val.type == FLV_AMF_TYPE_NUMBER) {
    //  fprintf(stderr, "%f\n", pAmf->val.u.d);
    //} else {
    //  fprintf(stderr, "\n");
    //}
 
    pAmf = pAmf->pnext;

  }

  return 0;
}

static int rtmp_parse_invoke_create(RTMP_CTXT_T *pRtmp, BYTE_STREAM_T *bs, FLV_AMF_T *pAmf) {

  if(pAmf->val.type == FLV_AMF_TYPE_NUMBER) {
  //  LOG(X_DEBUG("createStream: %f"), pAmf->val.u.d);
    pRtmp->connect.createStreamCode = pAmf->val.u.d;
  }

  pRtmp->state = RTMP_STATE_CLI_CREATESTREAM;

  return 0;
}

static int rtmp_parse_invoke_delete(RTMP_CTXT_T *pRtmp, BYTE_STREAM_T *bs, FLV_AMF_T *pAmf) {

  if(pAmf->val.type == FLV_AMF_TYPE_NUMBER) {
    //LOG(X_DEBUG("deleteStream: %f"), pAmf->val.u.d);
  }

  pRtmp->state = RTMP_STATE_CLI_DELETESTREAM;

  return 0;
}

static int rtmp_parse_invoke_pause(RTMP_CTXT_T *pRtmp, BYTE_STREAM_T *bs, FLV_AMF_T *pAmf) {

  if(pAmf->val.type == FLV_AMF_TYPE_NUMBER) {
    LOG(X_DEBUG("pause: %f"), pAmf->val.u.d);
  }

  pRtmp->state = RTMP_STATE_CLI_PAUSE;

  return 0;
}

static int rtmp_parse_invoke_connect(RTMP_CTXT_T *pRtmp, BYTE_STREAM_T *bs, 
                                    FLV_AMF_T *pAmfList) {

  int rc;
  FLV_AMF_T amfKeyVal;
  FLV_AMF_T *pAmf;
  unsigned int len;

  if(pAmfList->val.type == FLV_AMF_TYPE_NUMBER) {
    //fprintf(stderr, "connect: %f\n", pAmfList->val.u.d);
  }

  amfKeyVal.pnext = NULL;
  pAmf = pAmfList->pnext;

  while(bs->idx < bs->sz) {

    if(!pAmf) {
      pAmf = &amfKeyVal;
    }

    if((rc = rtmp_parse_amf(bs, pAmf)) < 0) {
      return -1; 
    } else if(rc == 0) {
      continue;
    }

    if(pAmf->key.len == 12 && !memcmp(pAmf->key.p, "capabilities", 12)) {
      pRtmp->connect.capabilities = pAmf->val.u.d;
    } else if(pAmf->key.len == 14 && !memcmp(pAmf->key.p, "objectEncoding", 14)) {
      pRtmp->connect.objEncoding = pAmf->val.u.d;
    } else if(pAmf->key.len == 5 && !memcmp(pAmf->key.p, "tcUrl", 5)) {
      if((len = pAmf->val.u.str.len) > sizeof(pRtmp->connect.tcurl) - 1) {
        len = sizeof(pRtmp->connect.tcurl) - 1;
      }
      memcpy(pRtmp->connect.tcurl, pAmf->val.u.str.p, len);
      pRtmp->connect.tcurl[len] = '\0';
    } else if(pAmf->key.len == 3 && !memcmp(pAmf->key.p, "app", 3)) {
      if((len = pAmf->val.u.str.len) > sizeof(pRtmp->connect.app) - 1) {
        len = sizeof(pRtmp->connect.app) - 1;
      }
      memcpy(pRtmp->connect.app, pAmf->val.u.str.p, len);
      pRtmp->connect.app[len] = '\0';
    }

    //fprintf(stderr, "%d/%d '%s'->", bs->idx, bs->sz, pAmf->key.p);

    //if(pAmf->val.type == FLV_AMF_TYPE_STRING) {
      //fprintf(stderr, "'%s'\n", pAmf->val.u.str.p);
    //} else if(pAmf->val.type == FLV_AMF_TYPE_NUMBER) {
      //fprintf(stderr, "%f\n", pAmf->val.u.d);
    //} else {
      //fprintf(stderr, "\n");
    //}

    pAmf = pAmf->pnext;

  }

  pRtmp->state = RTMP_STATE_CLI_CONNECT;

  return 0;
}

static int rtmp_parse_invoke_play(RTMP_CTXT_T *pRtmp, BYTE_STREAM_T *bs, 
                                  FLV_AMF_T *pAmfList) {

  int rc;
  FLV_AMF_T amfKeyVal;
  FLV_AMF_T *pAmf;
  unsigned int len;
  char *p;

  //if(pAmfList->val.type == FLV_AMF_TYPE_NUMBER) {
    //LOG(X_DEBUG("rtmp play code: %f"), pAmfList->val.u.d);
  //}

  amfKeyVal.pnext = NULL;
  pAmf = pAmfList->pnext;

  while(bs->idx < bs->sz) {

    if(!pAmf) {
      pAmf = &amfKeyVal;
    }

    if((rc = rtmp_parse_amf(bs, pAmf)) < 0) {
      return -1; 
    } else if(rc == 0) {
      continue;
    }

    if(pAmf->val.type == FLV_AMF_TYPE_STRING) {
      p = (char *) pAmf->val.u.str.p;
      if((len = pAmf->val.u.str.len) > sizeof(pRtmp->connect.play) - 1) {
        len = sizeof(pRtmp->connect.play) - 1;
      }
      if(!strncasecmp(p, "mp4:", 4) || !strncasecmp(p, "flv:", 4)) {
        p += 4; 
        len -= 4;
      }
      memcpy(pRtmp->connect.play, p, len);
      pRtmp->connect.play[len] = '\0';
      LOG(X_DEBUG("rtmp play element: '%s'"), pRtmp->connect.play);
      break;
    }

    pAmf = pAmf->pnext;

  }

  pRtmp->state = RTMP_STATE_CLI_PLAY;

  return 0;
}

static int rtmp_parse_invoke_fcpublish(RTMP_CTXT_T *pRtmp, BYTE_STREAM_T *bs, 
                                  FLV_AMF_T *pAmfList) {

  int rc;
  FLV_AMF_T amfKeyVal;
  FLV_AMF_T *pAmf;
  unsigned int len;
  char *p;

  //if(pAmfList->val.type == FLV_AMF_TYPE_NUMBER) {
    //LOG(X_DEBUG("rtmp play code: %f"), pAmfList->val.u.d);
  //}

  amfKeyVal.pnext = NULL;
  pAmf = pAmfList->pnext;

  while(bs->idx < bs->sz) {

    if(!pAmf) {
      pAmf = &amfKeyVal;
    }

    if((rc = rtmp_parse_amf(bs, pAmf)) < 0) {
      return -1; 
    } else if(rc == 0) {
      continue;
    }

    if(pAmf->val.type == FLV_AMF_TYPE_STRING) {
      p = (char *) pAmf->val.u.str.p;
      if((len = pAmf->val.u.str.len) > sizeof(pRtmp->publish.fcpublishurl) - 1) {
        len = sizeof(pRtmp->publish.fcpublishurl) - 1;
      }
      if(!strncasecmp(p, "mp4:", 4) || !strncasecmp(p, "flv:", 4)) {
        p += 4; 
        len -= 4;
      }
      memcpy(pRtmp->publish.fcpublishurl, p, len);
      pRtmp->publish.fcpublishurl[len] = '\0';
      LOG(X_DEBUG("rtmp fcpublish element: '%s'"), pRtmp->publish.fcpublishurl);
      break;
    }

    pAmf = pAmf->pnext;

  }

  pRtmp->state = RTMP_STATE_CLI_FCPUBLISH;

  return 0;
}

static int rtmp_parse_invoke_publish(RTMP_CTXT_T *pRtmp, BYTE_STREAM_T *bs, 
                                     FLV_AMF_T *pAmfList) {

  int rc;
  FLV_AMF_T amfKeyVal;
  FLV_AMF_T *pAmf;
  unsigned int len;
  int idx = 0;
  char *p;

  //if(pAmfList->val.type == FLV_AMF_TYPE_NUMBER) {
    //LOG(X_DEBUG("rtmp play code: %f"), pAmfList->val.u.d);
  //}

  amfKeyVal.pnext = NULL;
  pAmf = pAmfList->pnext;

  while(bs->idx < bs->sz) {

    if(!pAmf) {
      pAmf = &amfKeyVal;
    }

    if((rc = rtmp_parse_amf(bs, pAmf)) < 0) {
      return -1; 
    } else if(rc == 0) {
      continue;
    }

    if(pAmf->val.type == FLV_AMF_TYPE_STRING) {
      p = (char *) pAmf->val.u.str.p;
      if(idx == 0) {
        if((len = pAmf->val.u.str.len) > sizeof(pRtmp->publish.publishurl) - 1) {
          len = sizeof(pRtmp->publish.publishurl) - 1;
        }
        memcpy(pRtmp->publish.publishurl, p, len);
        pRtmp->publish.publishurl[len] = '\0';
        LOG(X_DEBUG("rtmp publish url: '%s'"), pRtmp->publish.publishurl);

        if(!(pAmf = pAmf->pnext)) {
          amfKeyVal.pnext = NULL;
          pAmf = &amfKeyVal;
        }
        if(bs->buf[bs->idx] == FLV_AMF_TYPE_STRING) {
          bs->idx++;

          if(!(p = (char *) flv_amf_read_string(&pAmf->key, bs, 0))) {
            LOG(X_ERROR("Unable to read rtmp publish url2"));
            break;
          }

          if((len = pAmf->key.len) > sizeof(pRtmp->publish.publishurl2) - 1) {
            len = sizeof(pRtmp->publish.publishurl2) - 1;
          }
          memcpy(pRtmp->publish.publishurl2, p, len);
          pRtmp->publish.publishurl2[len] = '\0';
          LOG(X_DEBUG("rtmp publish stream: '%s'"), pRtmp->publish.publishurl2);

          break;
        }

      }      
     
    }

    pAmf = pAmf->pnext;
  }

  pRtmp->state = RTMP_STATE_CLI_PUBLISH;

  return 0;
}

int rtmp_parse_invoke(RTMP_CTXT_T *pRtmp, FLV_AMF_T *pAmf) {
  int rc;
  FLV_AMF_T amfKeyVal;
  BYTE_STREAM_T bs;
  unsigned int streamIdx = pRtmp->streamIdx;

  if(!pAmf) {
    memset(&amfKeyVal, 0, sizeof(amfKeyVal));
    pAmf = &amfKeyVal;
  }

  bs.buf = pRtmp->in.buf;
  bs.sz = pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt;
  bs.idx = 0;


  if(pRtmp->pkt[streamIdx].hdr.contentType == RTMP_CONTENT_TYPE_MSG) {
    bs.idx++; // RTMP_CONTENT_TYPE_MSG appears to have extra header byte 0x00
  }

  //fprintf(stderr, "rtmp_parse_invoke len:%d at 0x%.2x 0x%.2x 0x%.2x 0x%.2x\n", bs.sz - bs.idx, bs.buf[bs.idx], bs.buf[bs.idx+1], bs.buf[bs.idx+2], bs.buf[bs.idx+3]);
  //avc_dumpHex(stderr, bs.buf, bs.sz - bs.idx, 1);

  pRtmp->contentTypeLastInvoke = pRtmp->pkt[streamIdx].hdr.contentType;

  while(bs.idx < bs.sz) {

    if((rc = rtmp_parse_amf(&bs, pAmf)) < 0) {
      return -1; 
    } else if(rc == 0) {
      continue;
    }

    VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - parse_invoke contentType: 0x%x, method: %c%c%c%c%c%c..."), 
                pRtmp->contentTypeLastInvoke, pAmf->key.p[0], pAmf->key.p[1], pAmf->key.p[2], pAmf->key.p[3], 
                pAmf->key.p[4], pAmf->key.p[5]) );

    if(pAmf->key.len == 4 && !memcmp(pAmf->key.p, "play", 4)) {
      pRtmp->methodParsed = RTMP_METHOD_PLAY;
      return rtmp_parse_invoke_play(pRtmp, &bs, pAmf);
    } else if(pAmf->key.len == 7 && !memcmp(pAmf->key.p, "connect", 7)) {
      pRtmp->methodParsed = RTMP_METHOD_CONNECT;
      return rtmp_parse_invoke_connect(pRtmp, &bs, pAmf);
    } else if(pAmf->key.len == 12 && !memcmp(pAmf->key.p, "createStream", 12)) {
      pRtmp->methodParsed = RTMP_METHOD_CREATESTREAM;
      return rtmp_parse_invoke_create(pRtmp, &bs, pAmf);
    } else if(pAmf->key.len == 10 && !memcmp(pAmf->key.p, "onMetaData", 10)) {
      pRtmp->methodParsed = RTMP_METHOD_ONMETADATA;
      return rtmp_parse_invoke_generic(pRtmp, &bs, pAmf);
    } else if((pAmf->key.len == 7 && !memcmp(pAmf->key.p, "_result", 7))) {
      // for capture purposes only
      pRtmp->methodParsed = RTMP_METHOD_RESULT;
      return rtmp_parse_invoke_generic(pRtmp, &bs, pAmf);
    } else if((pAmf->key.len == 6 && !memcmp(pAmf->key.p, "_error", 6))) {
      // for capture purposes only
      pRtmp->methodParsed = RTMP_METHOD_ERROR;
      return rtmp_parse_invoke_generic(pRtmp, &bs, pAmf);
    } else if(pAmf->key.len == 8 && !memcmp(pAmf->key.p, "onStatus", 8)) {
      // for capture purposes only
      pRtmp->methodParsed = RTMP_METHOD_ONSTATUS;
      return rtmp_parse_invoke_generic(pRtmp, &bs, pAmf);
    } else if((pAmf->key.len == 12 && !memcmp(pAmf->key.p, "deleteStream", 12))) {
      pRtmp->methodParsed = RTMP_METHOD_DELETESTREAM;
      return rtmp_parse_invoke_delete(pRtmp, &bs, pAmf);
    } else if((pAmf->key.len == 11 && !memcmp(pAmf->key.p, "closeStream", 11))) {
      pRtmp->methodParsed = RTMP_METHOD_CLOSESTREAM;
      return rtmp_parse_invoke_delete(pRtmp, &bs, pAmf);
    } else if((pAmf->key.len >= 5 && !memcmp(pAmf->key.p, "pauseRaw", 5))) {
      pRtmp->methodParsed = RTMP_METHOD_PAUSERAW;
      return rtmp_parse_invoke_pause(pRtmp, &bs, pAmf);
    } else if((pAmf->key.len >= 17 && !memcmp(pAmf->key.p, "|RtmpSampleAccess", 17))) {
      pRtmp->methodParsed = RTMP_METHOD_SAMPLEACCESS;
      //rc = rtmp_parse_invoke_generic(pRtmp, &bs, pAmf);
      return 0;
    } else if((pAmf->key.len >= 13 && !memcmp(pAmf->key.p, "releaseStream", 13))) {
      pRtmp->methodParsed = RTMP_METHOD_RELEASESTREAM;
      rc = rtmp_parse_invoke_generic(pRtmp, &bs, pAmf);
      return 0;
    } else if((pAmf->key.len >= 13 && !memcmp(pAmf->key.p, "onFCSubscribe", 13))) {
      pRtmp->methodParsed = RTMP_METHOD_ONFCSUBSCRIBE;
      rc = rtmp_parse_invoke_generic(pRtmp, &bs, pAmf);
      return 0;
    } else if((pAmf->key.len >= 9 && !memcmp(pAmf->key.p, "FCPublish", 9))) {
      pRtmp->methodParsed = RTMP_METHOD_FCPUBLISH;
      return rtmp_parse_invoke_fcpublish(pRtmp, &bs, pAmf);
    } else if((pAmf->key.len >= 11 && !memcmp(pAmf->key.p, "FCUnpublish", 11))) {
      pRtmp->methodParsed = RTMP_METHOD_FCUNPUBLISH;
      return rtmp_parse_invoke_generic(pRtmp, &bs, pAmf);
    } else if((pAmf->key.len >= 11 && !memcmp(pAmf->key.p, "onFCPublish", 11))) {
      pRtmp->methodParsed = RTMP_METHOD_ONFCPUBLISH;
      return rtmp_parse_invoke_generic(pRtmp, &bs, pAmf);
    } else if((pAmf->key.len >= 7 && !memcmp(pAmf->key.p, "publish", 7))) {
      pRtmp->methodParsed = RTMP_METHOD_PUBLISH;
      return rtmp_parse_invoke_publish(pRtmp, &bs, pAmf);
    } else if((pAmf->key.len >= 13 && !memcmp(pAmf->key.p, "@setDataFrame", 13))) {
      pRtmp->methodParsed = RTMP_METHOD_SETDATAFRAME;
      //return rtmp_parse_invoke_generic(pRtmp, &bs, pAmf);
      return 0;
    } else {
      LOG(X_WARNING("Unhandled rtmp invoke command %c%c%c%c... length: %d"), 
        pAmf->key.p[0], pAmf->key.p[1], pAmf->key.p[2], pAmf->key.p[3], 
        pAmf->key.len);
      //avc_dumpHex(stderr, bs.buf, bs.sz, 1);
      return 0;
    }

   //if(pAmf->key.len>0) fprintf(stderr, "key:'%c%c%c%c' ", pAmf->key.p[0], pAmf->key.p[1], pAmf->key.p[2], pAmf->key.p[3]);

  }

  return 0;
}

int rtmp_parse_init(RTMP_CTXT_T *pRtmp, unsigned int insz, unsigned int outsz) {

  if(!pRtmp) {
    return -1;
  }

  memset(pRtmp, 0, sizeof(RTMP_CTXT_T));
  pRtmp->streamIdx = RTMP_STREAM_INDEXES;
  pRtmp->streamIdxPrev = RTMP_STREAM_INDEXES;
  pRtmp->rcvTmtMs = RTMP_RCV_TMT_MS;

  pRtmp->in.sz = insz;
  if(pRtmp->in.sz > 0 &&
     (pRtmp->in.buf = (unsigned char *) avc_calloc(1, pRtmp->in.sz)) == NULL) {
    return -1;
  }

  pRtmp->out.sz = outsz;
  if(pRtmp->out.sz > 0 &&
     (pRtmp->out.buf = (unsigned char *) avc_calloc(1, pRtmp->out.sz)) == NULL) {
    rtmp_parse_close(pRtmp);
    return -1;
  }

  pRtmp->chunkSzIn = RTMP_CHUNK_SZ_DEFAULT;
  pRtmp->chunkSzOut = pRtmp->chunkSzIn;

  // Default values
  pRtmp->contentTypeLastInvoke = RTMP_CONTENT_TYPE_INVOKE;
  pRtmp->connect.objEncoding = 3.0;
  pRtmp->connect.capabilities = 31.0;

  return 0;
}


void rtmp_parse_close(RTMP_CTXT_T *pRtmp) {

  if(pRtmp) {

    if(pRtmp->in.buf) {
      avc_free((void *) &pRtmp->in.buf);
      //pRtmp->in.buf = NULL;
    }

    pRtmp->in.sz = 0;

    if(pRtmp->out.buf) {
      avc_free((void *) &pRtmp->out.buf);
      //pRtmp->out.buf = NULL;
    }

    pRtmp->out.sz = 0;
  }

}


