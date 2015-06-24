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

#define READ_UINT8(psrc, pos)      *((uint8_t *) &((unsigned char *)psrc)[pos++]);
#define READ_UINT16(psrc, pos) ntohs(  *((uint16_t *) &((unsigned char *)psrc)[pos])); (pos)+=2;


int avcc_initCfg(const unsigned char *arena, unsigned int len,
                 AVC_DECODER_CFG_T *pCfg) {
  unsigned int idx;
  unsigned int pos = 0;

  if(!arena || !pCfg) {
    return -1;
  }

  //fprintf(stderr, "AVCC_INIT len %d, psps: 0x%x avcc:0x%x\n", len, pCfg->psps, pCfg); avc_dumpHex(stderr, arena, len, 1);

  pCfg->configversion = READ_UINT8(arena, pos);
  pCfg->avcprofile = READ_UINT8(arena, pos);
  pCfg->profilecompatibility = READ_UINT8(arena, pos);
  pCfg->avclevel = READ_UINT8(arena, pos);
  pCfg->lenminusone = READ_UINT8(arena, pos);
  pCfg->lenminusone &= 0x03;

  pCfg->numsps = READ_UINT8(arena, pos);
  pCfg->numsps &= 0x1f; 
  pCfg->psps = (AVC_DECODER_CFG_BLOB_T *) avc_calloc(pCfg->numsps, sizeof(AVC_DECODER_CFG_BLOB_T));
  for(idx = 0; idx < pCfg->numsps; idx++) {
    pCfg->psps[idx].len = READ_UINT16(arena, pos);
    if(pCfg->psps[idx].len > len - pos) {
      LOG(X_ERROR("Invalid avcC sps[%d] len: %d"), idx, pCfg->psps[idx].len);
      return -1;
    }
    pCfg->psps[idx].data = (unsigned char *) avc_calloc(pCfg->psps[idx].len, 1);
    memcpy(pCfg->psps[idx].data, &arena[pos], pCfg->psps[idx].len);
    pos += pCfg->psps[idx].len;
  }
  pCfg->numpps = READ_UINT8(arena, pos);
  pCfg->numpps &= 0x01f; 
  pCfg->ppps = (AVC_DECODER_CFG_BLOB_T *) avc_calloc(pCfg->numsps, sizeof(AVC_DECODER_CFG_BLOB_T));
  for(idx = 0; idx < pCfg->numpps; idx++) {
    pCfg->ppps[idx].len = READ_UINT16(arena, pos);
    if(pCfg->ppps[idx].len > len - pos) {
      LOG(X_ERROR("Invalid avcC pps[%d] len: %d"), idx, pCfg->ppps[idx].len);
      return -1;
    }
//pCfg->ppps[idx].len++;
    pCfg->ppps[idx].data = (unsigned char *) avc_calloc(pCfg->ppps[idx].len, 1);
    memcpy(pCfg->ppps[idx].data, &arena[pos], pCfg->ppps[idx].len);
    pos += pCfg->ppps[idx].len;
//pCfg->ppps[idx].data[pCfg->ppps[idx].len-1]=0x80;
  }

#if 0

  unsigned char _sps[] = { 0x67, 0x42, 0x80, 0x20, 0x96, 0x54, 0x12, 0x6d, 0x08, 0x00, 0x01, 0x38, 0x80, 0x00, 0x49, 0xfb, 0x60, 0x20 };
  pCfg->psps[0].len = 18;
  pCfg->psps[0].data = avc_calloc(pCfg->psps[0].len, 1); 
  memcpy(pCfg->psps[0].data, _sps, pCfg->psps[0].len);

  unsigned char _pps[] = { 0x68, 0xde, 0x02, 0x18, 0x80 };  
  pCfg->ppps[0].len = 5;
  pCfg->ppps[0].data = avc_calloc(pCfg->ppps[0].len, 1); 
  memcpy(pCfg->ppps[0].data, _pps, pCfg->ppps[0].len);
  fprintf(stderr, "AVCC SPS:\n"); avc_dumpHex(stderr, pCfg->psps[0].data, pCfg->psps[0].len, 1);
  fprintf(stderr, "AVCC PPS:\n"); avc_dumpHex(stderr, pCfg->ppps[0].data, pCfg->ppps[0].len, 1);
#endif


  return 0;
}

void avcc_freeCfg(AVC_DECODER_CFG_T *pCfg) {
  unsigned int idx;

  if(!pCfg) {
    return;
  }

  //fprintf(stderr, "AVCC_FREECFGlen psps: 0x%x, avcc:0x%x\n", pCfg->psps, pCfg);

  if(pCfg->psps) {

    for(idx = 0; idx < pCfg->numsps; idx++) {
      if(pCfg->psps[idx].data) {
        free(pCfg->psps[idx].data);
      }
    }
    free(pCfg->psps);
    pCfg->psps = NULL;
    pCfg->numsps = 0;
  }

  if(pCfg->ppps) {
    for(idx = 0; idx < pCfg->numpps; idx++) {
      if(pCfg->ppps[idx].data) {
        free(pCfg->ppps[idx].data);
      }
    }
    free(pCfg->ppps);
    pCfg->ppps = NULL;
    pCfg->numpps = 0;
  }
}

int avcc_writeNAL(FILE_STREAM_T *pStreamOut, unsigned char *pdata, unsigned int len) {
  uint32_t startCode = htonl(1);
  int lenwrote = 0;

  if(!pStreamOut || !pdata) {
    return -1;
  }

  if(len < 4 || pdata[0] != 0x00 || pdata[1] != 0x00 || pdata[2] != 0x00 || pdata[3] != 0x01) {
      if(WriteFileStream(pStreamOut, &startCode, 4) < 0) {
        return -1;
      }
      lenwrote += 4;
  }

  if(WriteFileStream(pStreamOut, pdata, len) < 0) {
    return -1;
  }
  lenwrote += len;

  return lenwrote;
}

static int avcc_copyNAL(FILE_STREAM_T *pStreamIn, FILE_STREAM_T *pStreamOut, 
                        unsigned int lenNal, unsigned char *arena, unsigned int szArena) {

  unsigned int lenNalRead = 0;
  unsigned int idxByteInNal = 0;
  const uint32_t startCode = htonl(1);

  if(WriteFileStream(pStreamOut, &startCode, 4) < 0) {
    return -1;
  }

  while(idxByteInNal < lenNal) {
    if((lenNalRead = lenNal - idxByteInNal) > szArena) {
      lenNalRead = szArena;
    }

    // TODO: need to look for 0x00 00 00 context and escape first 2 bytes with 0x 00 00 03

    if(ReadFileStream(pStreamIn, arena, lenNalRead) < 0) {
      return -1;
    }

    if((idxByteInNal += lenNalRead) > lenNal) {
      return -1;
    }

    if(WriteFileStream(pStreamOut, arena, lenNalRead) < 0) {
      return -1;
    }

  }  // end of while

  return idxByteInNal;
}



static int avcc_writeSPSwVUI(FILE_STREAM_T *pStreamOut, const AVC_DECODER_CFG_T *pAvcCfg,
                    uint32_t sampledelta, uint32_t timescale) {

  H264_NAL_SPS_T sps;
  H264_STREAM_CHUNK_T stream1;
  H264_STREAM_CHUNK_T stream2;
  unsigned char buf[768];
  unsigned char buf2[1024];
  int rc = 0;

  if(!pStreamOut || !pAvcCfg || pAvcCfg->numsps < 1 || !pAvcCfg->psps ||
     pAvcCfg->psps[0].len < 1) {
    return 0;
  }

  H264_INIT_BITPARSER(stream1.bs, &pAvcCfg->psps[0].data[1], pAvcCfg->psps[0].len -1);
  stream1.startCodeMatch = 0;
/*
  memset(&stream1, 0, sizeof(stream1));
  stream1.bs.emulPrevSeqLen = 3;
  stream1.bs.emulPrevSeq[2] = 0x03;
  stream1.bs.buf = &pAvcCfg->psps[0].data[1];
  stream1.bs.sz = pAvcCfg->psps[0].len -1;
*/

  // Write timing information from mdhd box into SPS
  if(h264_decodeSPS(&stream1.bs, &sps) == 0) {

    if(sps.vui.timing_timescale == 0) {

      sps.vui.params_present_flag = 1;
      sps.vui.timing_info_present = 1;
      sps.vui.timing_num_units_in_tick = sampledelta;
      sps.vui.timing_timescale = timescale * 2;
      //sps.timing_fixed_frame_rate = 1; ??

      memset(buf, 0, sizeof(buf));
      buf[0] = 0x60 | NAL_TYPE_SEQ_PARAM;
      memset(&stream1, 0, sizeof(stream1));
      stream1.bs.buf = buf;
      stream1.bs.byteIdx = 1;
      stream1.bs.sz = sizeof(buf);

      if(h264_encodeSPS(&stream1.bs, &sps) == 0) {

        memset(buf2, 0, sizeof(buf2));
        memset(&stream2, 0, sizeof(stream2));
        stream2.bs.buf = buf2;
        stream2.bs.byteIdx = 0;
        stream2.bs.sz = sizeof(buf2);

        if((rc = h264_escapeRawByteStream(&stream1.bs, &stream2.bs)) < 0) {
          LOG(X_ERROR("Unable to escape newly created SPS of length: %d"), stream1.bs.byteIdx);
          return 0;
        } else {
          //LOG(X_DEBUG("Handled %d escape sequences in SPS"), rc);
        }

        rc = avcc_writeNAL(pStreamOut, stream2.bs.buf, stream2.bs.byteIdx);
      }

    }

  } else {
    LOG(X_WARNING("Failed to decode SPS"));
  }

  return rc;
}

int avcc_writeSPSPPS(FILE_STREAM_T *pStreamOut, const AVC_DECODER_CFG_T *pCfg,
                      uint32_t sampledelta, uint32_t timescale) {

  int rc = 0;

  if(pCfg == NULL ||
     pCfg->numsps == 0 || !pCfg->psps || pCfg->psps[0].len == 0 ||
     pCfg->numpps == 0 || !pCfg->ppps || pCfg->ppps[0].len == 0) {
    LOG(X_ERROR("Incompleted avc decoder configuration"));
    return -1;
  }

  if(sampledelta > 0 && timescale > 0) {
    if((rc = avcc_writeSPSwVUI(pStreamOut, pCfg, sampledelta, timescale)) < 0) {
      return -1;
    } else if(rc > 0) {
      LOG(X_DEBUG("Adding VUI timing info into original SPS"));
    }
  }

  if(rc == 0) {
    rc = avcc_writeNAL(pStreamOut, pCfg->psps[0].data, pCfg->psps[0].len);
  }

  if(rc >= 0) {
    rc = avcc_writeNAL(pStreamOut, pCfg->ppps[0].data, pCfg->ppps[0].len);
  }

  if(rc > 0) {
    return 0; 
  }

  return -1;
}

int avcc_cbWriteSample(FILE_STREAM_T *pStreamIn, uint32_t sampleSize, 
                       void *pCbData, int chunkIdx, uint32_t sampleDurationHz) {

  AVCC_CBDATA_WRITE_SAMPLE_T *pCbDataAvcC = NULL;
  unsigned int idxByteInSample = 0;
  unsigned int lenNal;
  unsigned int numNals = 0;
  uint32_t tmp;
  int rc = 0;

  if((pCbDataAvcC = (AVCC_CBDATA_WRITE_SAMPLE_T *) pCbData) == NULL) {
    return -1;
  }

  while(idxByteInSample < sampleSize) {

    if(ReadFileStream(pStreamIn, pCbDataAvcC->arena, pCbDataAvcC->lenNalSzField) < 0) {
      return -1;
    }
    idxByteInSample += pCbDataAvcC->lenNalSzField;

    if(pCbDataAvcC->lenNalSzField == 4) {
      memcpy(&tmp, pCbDataAvcC->arena, 4);
      lenNal = htonl(tmp);
      //lenNal = htonl(  *((uint32_t *)pCbDataAvcC->arena) );
    } else if(pCbDataAvcC->lenNalSzField == 2) {
      memcpy(&tmp, pCbDataAvcC->arena, 2);
      lenNal = htons(tmp);
      //lenNal = htons(  *((uint16_t *)pCbDataAvcC->arena) );
    } else {
      LOG(X_ERROR("avcC length field: %d not supported"), pCbDataAvcC->lenNalSzField);
      return -1;
    }
    if(lenNal > sampleSize - idxByteInSample) {
      LOG(X_ERROR("nal length: %u > sample length: %d not supported."), lenNal, sampleSize);
      return -1;
    }

    if((rc = avcc_copyNAL(pStreamIn, pCbDataAvcC->pStreamOut, lenNal,
            pCbDataAvcC->arena, sizeof(pCbDataAvcC->arena))) < 0) {
      return -1;
    }
    if((idxByteInSample += rc) > (unsigned int) sampleSize) {
      return -1;
    }

    numNals++;

  }

  if(idxByteInSample > (unsigned int) sampleSize) {
    LOG(X_ERROR("DEBUG...Invalid NAL lengths %u > %u"), idxByteInSample, sampleSize);
  }

  return rc;
}

int avcc_create(unsigned char *out, unsigned int len, const SPSPPS_RAW_T *pspspps,
                unsigned int nalHdrSz) {
  unsigned int idx = 6;

  if(!pspspps || !out || !pspspps->sps || !pspspps->pps ||
     len < 15 + pspspps->sps_len + pspspps->pps_len ||
     pspspps->sps_len < 4 || pspspps->pps_len < 1 || nalHdrSz > 4) {
    return -1;
  } 

  ((AVC_DECODER_CFG_T *) out)->configversion = 1;
  ((AVC_DECODER_CFG_T *) out)->avcprofile = pspspps->sps[1];
  ((AVC_DECODER_CFG_T *) out)->profilecompatibility = pspspps->sps[2];
  ((AVC_DECODER_CFG_T *) out)->avclevel = pspspps->sps[3];
  ((AVC_DECODER_CFG_T *) out)->lenminusone = 0xfc | (nalHdrSz - 1);
  ((AVC_DECODER_CFG_T *) out)->numsps = (0xe0 | 1);

  *((uint16_t *) &out[idx]) = htons(pspspps->sps_len);
  idx += 2;
  if(pspspps->sps_len > len - idx) {
    LOG(X_ERROR("SPS length %u > buffer size %u"), pspspps->pps_len, len - idx); 
    return -1;
  }
  memcpy(&out[idx], pspspps->sps, pspspps->sps_len);
  idx += pspspps->sps_len;
  *((uint8_t *) &out[idx++]) = 1;
  *((uint16_t *) &out[idx]) = htons(pspspps->pps_len);
  idx += 2;
  if(pspspps->pps_len > len - idx) {
    LOG(X_ERROR("PPS length %u > buffer size %u"), pspspps->pps_len, len - idx); 
    return -1;
  }
  memcpy(&out[idx], pspspps->pps, pspspps->pps_len);
  idx += pspspps->pps_len;

  return idx;
}

int avcc_encodebase64(SPSPPS_RAW_T *pspspps, char *pOut, unsigned int lenOut) {
  unsigned char buf[sizeof(pspspps->sps_buf) + sizeof(pspspps->pps_buf)];
  int rc = 0;

  if(!pspspps || !pspspps->sps || !pspspps->pps || !pOut) {
    return -1;
  }

  memcpy(buf, pspspps->sps, pspspps->sps_len);
  memcpy(&buf[pspspps->sps_len], pspspps->pps, pspspps->pps_len);

  if((rc = base64_encode(buf, pspspps->sps_len + pspspps->pps_len, pOut, lenOut)) < 0) {
    return rc;
  }
  
  return rc;
}



