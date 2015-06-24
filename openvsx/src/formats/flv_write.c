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


void flv_write_int24(BYTE_STREAM_T *bs, uint32_t val) {
  val = htonl(val) >> 8;
  memcpy(&bs->buf[bs->idx],&val , 3);
  bs->idx += 3;
}

int flv_write_filebody(FLV_BYTE_STREAM_T *fbs, int audio, int video) {

  if(!fbs || fbs->bs.idx + 13 > fbs->bs.sz) {
    return -1;
  }

  memset(fbs->bs.buf, 0, 13);

  fbs->bs.buf[0] = 'F';
  fbs->bs.buf[1] = 'L';
  fbs->bs.buf[2] = 'V';
  fbs->bs.buf[3] = 0x01;  // File version
  fbs->bs.buf[4] = (audio << 2) | video;
  fbs->bs.buf[8] = 9;  // 32bit size of header (9)

  fbs->bs.idx = 13;
  fbs->prevtagidx = 13;

  return fbs->bs.idx;
}

int flv_write_string(BYTE_STREAM_T *bs, const char *str) {
  unsigned int sz;

  sz = strlen(str);

  if(bs->idx + sz + 2 > bs->sz) {
    return -1;
  }

  *((uint16_t *)&bs->buf[bs->idx]) = htons(sz);
  bs->idx += 2;
  memcpy(&bs->buf[bs->idx], str, sz);
  bs->idx += sz;

  return 0;
}

static int flv_write_val_string(BYTE_STREAM_T *bs, const char *str, unsigned int len) {
  unsigned int sz = 0;
  unsigned int idx;

  if(str) {
    sz = strlen(str);
    if(len == 0) {
      len = sz;
    } else if(sz > len) {
      sz = len;
    }
  }

  if(bs->idx + len + 3 > bs->sz) {
    return -1;
  }

  bs->buf[bs->idx++] = FLV_AMF_TYPE_STRING;
  *((uint16_t *)&bs->buf[bs->idx]) = htons(len);
  bs->idx += 2;
  idx = bs->idx;
  if(str) {
    memcpy(&bs->buf[bs->idx], str, sz);
    bs->idx += sz;
  }

  if(len > (bs->idx - idx)) {
    memset(&bs->buf[bs->idx], 0, len - (bs->idx - idx));
    bs->idx += (len - (bs->idx - idx));
  }

  return 0;
}

int flv_write_val_number(BYTE_STREAM_T *bs, double dbl) {

  uint32_t ui = *((uint32_t *)&dbl);
  uint32_t ui2 =  *((uint32_t *) &(((unsigned char *)&dbl)[4]));

  if(bs->idx + 9 > bs->sz) {
    return -1;
  }

  bs->buf[bs->idx] = FLV_AMF_TYPE_NUMBER;
  *((uint32_t *)&bs->buf[bs->idx + 1]) = htonl(ui2);
  *((uint32_t *)&bs->buf[bs->idx + 5]) = htonl(ui);

  bs->idx += 9;

  return 0;
}

static int flv_write_val_bool(BYTE_STREAM_T *bs, int val) {

  if(bs->idx + 2 > bs->sz) {
    return -1;
  }

  bs->buf[bs->idx++] = FLV_AMF_TYPE_BOOL;
  bs->buf[bs->idx++] = val;

  return 0;
}

int flv_write_key_val(BYTE_STREAM_T *bs, const char *str, double dbl, int amfType) {

  if(flv_write_string(bs, str) < 0) {
    return -1; 
  }

  switch(amfType) {
    case FLV_AMF_TYPE_NUMBER:
      return flv_write_val_number(bs, dbl);
    case FLV_AMF_TYPE_BOOL:
      return flv_write_val_bool(bs, dbl);
    default:
      return -1;
  }

  return -1;
}

int flv_write_key_val_string(BYTE_STREAM_T *bs, const char *key, const char *val) { 
  if(!(flv_write_string(bs, key) == 0 && flv_write_val_string(bs, val, 0) == 0)) {
    return -1;
  }
  return 0;
}

int flv_write_objend(BYTE_STREAM_T *bs) {

  bs->buf[bs->idx++] = 0;
  bs->buf[bs->idx++] = 0;
  bs->buf[bs->idx++] = 9;

  return 0;
}

int flv_write_taghdr(BYTE_STREAM_T *bs, uint8_t tagType, uint32_t sz, uint32_t timestamp32) {

  //
  // 11 byte flv tag (no 4 byte prev tag size field)
  //
  bs->buf[bs->idx++] = tagType;
  flv_write_int24(bs, sz);
  flv_write_int24(bs, timestamp32);
  memset(&bs->buf[bs->idx], 0, 4); // 1 byte ts ext, 3 btyes stream id
  bs->idx += 4;

  return FLV_TAG_HDR_SZ_NOPREV;
}

int flv_write_onmeta(const CODEC_AV_CTXT_T *pAvCtxt, FLV_BYTE_STREAM_T *fbs) {
  int rc = 0;
  BYTE_STREAM_T bstmp;
  unsigned int cntEcma = 0;

  if(!pAvCtxt || !fbs) {
    return -1;
  }

  //if(11 + > fbs->bs.sz - fbs->bs.idx) {
  //  LOG(X_ERROR("aud flv frame size too large %d"), fbs->bs.sz - fbs->bs.idx);
  //  return -1;
  //}

  fbs->prevtagidx = fbs->bs.idx;
  memcpy(&bstmp, &fbs->bs, sizeof(bstmp));
  fbs->bs.idx += FLV_TAG_HDR_SZ_NOPREV;

  if(pAvCtxt->vid.haveSeqHdr) {
    //cntEcma += 2;
  }

  if(pAvCtxt->aud.haveSeqHdr) {

  }


  fbs->bs.buf[fbs->bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_string(&fbs->bs, "onMetaData");
  fbs->bs.buf[fbs->bs.idx++] = FLV_AMF_TYPE_ECMA;
  *((uint32_t *) &fbs->bs.buf[fbs->bs.idx]) = htonl(cntEcma);
  fbs->bs.idx += 4;

  //flv_write_key_val(&fbs->bs, "duration", 187.44, FLV_AMF_TYPE_NUMBER);
  //flv_write_key_val(&fbs->bs, "audiochannels", 2.0, FLV_AMF_TYPE_NUMBER);
  //flv_write_key_val(&fbs->bs, "audiosamplerate", 44100, FLV_AMF_TYPE_NUMBER);
  //flv_write_key_val(&fbs->bs, "starttime", 0, FLV_AMF_TYPE_NUMBER);
  //flv_write_key_val(&fbs->bs, "totalduration", 187.44, FLV_AMF_TYPE_NUMBER);

  if(pAvCtxt->vid.haveSeqHdr) {
    //flv_write_key_val(&fbs->bs, "width", 480, FLV_AMF_TYPE_NUMBER);
    //flv_write_key_val(&fbs->bs, "height", 360, FLV_AMF_TYPE_NUMBER);
  }

  //flv_write_key_val(&fbs->bs, "videodatarate", 0, FLV_AMF_TYPE_NUMBER);
  //flv_write_key_val(&fbs->bs, "audiodatarate", 0, FLV_AMF_TYPE_NUMBER);
  //flv_write_key_val(&fbs->bs, "totaldatarate", 0, FLV_AMF_TYPE_NUMBER);
  //flv_write_key_val(&fbs->bs, "framerate", 25.005335, FLV_AMF_TYPE_NUMBER);
  //flv_write_key_val(&fbs->bs, "bytelength", 20269666, FLV_AMF_TYPE_NUMBER);
  //flv_write_key_val(&fbs->bs, "canseekontime", 1, FLV_AMF_TYPE_BOOL);

  //flv_write_string(&fbs->bs, "sourcedata");
  //flv_write_val_string(&fbs->bs, "B4A7D0F08", 0x20);

  //flv_write_string(&fbs->bs, "purl");
  //flv_write_val_string(&fbs->bs, "", 0x80);

  //flv_write_string(&fbs->bs, "pmsg");
  //flv_write_val_string(&fbs->bs, "", 0x80);

  flv_write_objend(&fbs->bs);


  //
  // Now that the data length is known, fill in the 11 byte flv header
  //
  flv_write_taghdr(&bstmp, FLV_TAG_SCRIPTDATA, fbs->bs.idx - fbs->prevtagidx - FLV_TAG_HDR_SZ_NOPREV, 0);

  //
  // 4 byte previous tag size field
  //
  *((uint32_t *) &fbs->bs.buf[fbs->bs.idx]) = htonl(fbs->bs.idx - fbs->prevtagidx);
  fbs->bs.idx += 4;
  fbs->prevtagidx = fbs->bs.idx;

  return rc;
}

int flv_write_vidstart(FLV_BYTE_STREAM_T *fbs, CODEC_VID_CTXT_T *pV) {
  int rc;
  unsigned int idx0;
  BYTE_STREAM_T bstmp;

  idx0 = fbs->bs.idx;

  fbs->prevtagidx = fbs->bs.idx;
  memcpy(&bstmp, &fbs->bs, sizeof(bstmp));
  fbs->bs.idx += FLV_TAG_HDR_SZ_NOPREV;

  if((rc = flvsrv_create_vidstart(&fbs->bs, pV)) < 0) {
    return rc;
  }

  //
  // Now that the data length is known, fill in the 11 byte flv header
  //
  flv_write_taghdr(&bstmp, FLV_TAG_VIDEODATA, fbs->bs.idx - fbs->prevtagidx - FLV_TAG_HDR_SZ_NOPREV, 0);

  //
  // 4 byte previous tag size field
  //
  *((uint32_t *) &fbs->bs.buf[fbs->bs.idx]) = htonl(fbs->bs.idx - fbs->prevtagidx);
  fbs->bs.idx += 4;
  fbs->prevtagidx = fbs->bs.idx;

  return fbs->bs.idx - idx0;
}

int flv_write_audstart(FLV_BYTE_STREAM_T *fbs, CODEC_AUD_CTXT_T *pA) {
  int rc;
  unsigned int idx0;
  BYTE_STREAM_T bstmp;

  idx0 = fbs->bs.idx;

  fbs->prevtagidx = fbs->bs.idx;
  memcpy(&bstmp, &fbs->bs, sizeof(bstmp));
  fbs->bs.idx += FLV_TAG_HDR_SZ_NOPREV;

  if((rc = flvsrv_create_audstart(&fbs->bs, pA)) < 0) {
    return rc;
  }

  //
  // Now that the data length is known, fill in the 11 byte flv header
  //
  flv_write_taghdr(&bstmp, FLV_TAG_AUDIODATA, fbs->bs.idx - fbs->prevtagidx - FLV_TAG_HDR_SZ_NOPREV, 0);

  //
  // 4 byte previous tag size field
  //
  *((uint32_t *) &fbs->bs.buf[fbs->bs.idx]) = htonl(fbs->bs.idx - fbs->prevtagidx);
  fbs->bs.idx += 4;
  fbs->prevtagidx = fbs->bs.idx;

  return fbs->bs.idx - idx0;
}


#if 0

int flv_write_scriptdata(FLV_BYTE_STREAM_T *fbs) {
  int rc = 0;
  BYTE_STREAM_T bstmp;

  flv_write_filebody_old(fbs);

  fbs->bs.buf[fbs->bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_string(&fbs->bs, "onMetaData");
  fbs->bs.buf[fbs->bs.idx++] = FLV_AMF_TYPE_ECMA;
  *((uint32_t *) &fbs->bs.buf[fbs->bs.idx]) = htonl(0x0e);
  fbs->bs.idx += 4;

  flv_write_key_val(&fbs->bs, "duration", 187.44, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&fbs->bs, "audiochannels", 2.0, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&fbs->bs, "audiosamplerate", 44100, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&fbs->bs, "starttime", 0, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&fbs->bs, "totalduration", 187.44, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&fbs->bs, "width", 290, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&fbs->bs, "height", 180, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&fbs->bs, "videodatarate", 0, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&fbs->bs, "audiodatarate", 0, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&fbs->bs, "totaldatarate", 0, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&fbs->bs, "framerate", 25.005335, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&fbs->bs, "bytelength", 20269666, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&fbs->bs, "canseekontime", 1, FLV_AMF_TYPE_BOOL);

  flv_write_string(&fbs->bs, "sourcedata");
  flv_write_val_string(&fbs->bs, "B4A7D0F08", 0x20);

  flv_write_string(&fbs->bs, "purl");
  flv_write_val_string(&fbs->bs, "", 0x80);

  flv_write_string(&fbs->bs, "pmsg");
  flv_write_val_string(&fbs->bs, "", 0x80);

  fbs->bs.buf[fbs->bs.idx + 2] = 0x09;  // VariableEndMarker
  fbs->bs.idx += 3;

  // Now that it is known, fill in the script hdr length, 
  bstmp.buf = fbs->bs.buf;
  bstmp.sz = fbs->bs.sz;
  bstmp.idx = 14; 
  flv_write_int24(&bstmp, fbs->bs.idx - fbs->prevtagidx - 11);

  // 4 byte previous tag size field
  *((uint32_t *) &fbs->bs.buf[fbs->bs.idx]) = htonl(fbs->bs.idx - fbs->prevtagidx);
  fbs->bs.idx += 4;
  fbs->prevtagidx = fbs->bs.idx;

  return rc;
}
#endif // 0


#if 0
static int flv_write_vidtag_avcc(FLV_BYTE_STREAM_T *fbs, 
                                 SPSPPS_RAW_T *pspspps) {
  int rc;
  BYTE_STREAM_T bstmp;

  //
  // Create AVCC video tag
  //
  fbs->bs.buf[fbs->bs.idx++] = FLV_TAG_VIDEODATA;
  memset(&fbs->bs.buf[fbs->bs.idx], 0, 10);
                    // 3 byte data body size
                    // 3 byte timestamp
                    // 1 byte timestamp extended
                    // 3 byte stream id
  
  fbs->bs.idx += 10;
  fbs->bs.buf[fbs->bs.idx++] = (FLV_VID_FRM_KEYFRAME << 4) | (FLV_VID_CODEC_AVC);
  fbs->bs.buf[fbs->bs.idx++] = FLV_VID_AVC_PKTTYPE_SEQHDR;
  flv_write_int24(&fbs->bs, 0); // composition time 

  if((rc = avcc_create(&fbs->bs.buf[fbs->bs.idx], fbs->bs.sz - fbs->bs.idx, 
                      pspspps, 4)) > 0) {
    fbs->bs.idx += rc;
    rc = 0;
  }

  // Now that it is known, fill in the body hdr length, 
  bstmp.buf = fbs->bs.buf;
  bstmp.sz = fbs->bs.sz;
  bstmp.idx = fbs->prevtagidx + 1; 
  flv_write_int24(&bstmp, fbs->bs.idx - fbs->prevtagidx - 11);

  // 4 byte previous tag size field
  *((uint32_t *) &fbs->bs.buf[fbs->bs.idx]) = htonl(fbs->bs.idx - fbs->prevtagidx);
  fbs->bs.idx += 4;
  fbs->prevtagidx = fbs->bs.idx;

  return rc;
}

#if 0
//TODO: deprecated in favor of flvsrv_create_vidframe
int flv_write_vidtag(FLV_BYTE_STREAM_T *fbs, 
                     OUTFMT_FRAME_DATA_T *pFrame) { 
  int rc = 0;
  unsigned int dataIdx = 0;
  uint32_t avccHdr;
  uint64_t tm;

  //
  // Omit NAL AUD
  //
  if((rc = h264_check_NAL_AUD(pFrame->pData, pFrame->len)) > 0) {
    dataIdx += rc;
  }

  rc = 0;

  //
  // Skip past AnnexB Header
  // 
  if(pFrame->len >= dataIdx + 4 && *((uint32_t *) &pFrame->pData[dataIdx]) == htonl(0x01)) {
    dataIdx += 4;
  }


  if(11 + pFrame->len - dataIdx > fbs->bs.sz - fbs->bs.idx) {
    LOG(X_ERROR("vid flv frame size too large %d"), fbs->bs.sz - fbs->bs.idx);
    return -1;
  }

  fbs->bs.buf[fbs->bs.idx++] = FLV_TAG_VIDEODATA;
  flv_write_int24(&fbs->bs, pFrame->len + 9 - dataIdx); // data size
  //flv_write_int24(bs, (uint32_t)pFrame->pts/90.0f); // time
  flv_write_int24(&fbs->bs, 0); // time
                                // 1 byte timestamp extended
                                // 3 byte stream id
  memset(&fbs->bs.buf[fbs->bs.idx], 0, 4);
  fbs->bs.idx += 4;

  fbs->bs.buf[fbs->bs.idx++] = ((pFrame->keyframe ? FLV_VID_FRM_KEYFRAME : FLV_VID_FRM_INTERFRAME)
                           << 4) | (FLV_VID_CODEC_AVC);
  fbs->bs.buf[fbs->bs.idx++] = FLV_VID_AVC_PKTTYPE_NALU;
  
  //TODO: check if the dts is actually a +/- offset from the pts, or an absolute time
  tm = pFrame->pts + pFrame->dts;
  flv_write_int24(&fbs->bs, (uint32_t)(tm/90.0f)); // composition time 

  avccHdr = htonl(pFrame->len - dataIdx);
  memcpy(&fbs->bs.buf[fbs->bs.idx], &avccHdr, 4);
  fbs->bs.idx += 4;

  memcpy(&fbs->bs.buf[fbs->bs.idx], &pFrame->pData[dataIdx], pFrame->len - dataIdx);
  fbs->bs.idx += pFrame->len - dataIdx;

  // 4 byte previous tag size field
  *((uint32_t *) &fbs->bs.buf[fbs->bs.idx]) = htonl(fbs->bs.idx - fbs->prevtagidx);
  fbs->bs.idx += 4;
  fbs->prevtagidx = fbs->bs.idx;

  return rc;
}
#endif // 0

static int flv_write_audtag_esds(FLV_BYTE_STREAM_T *fbs, 
                                 OUTFMT_FRAME_DATA_T *pFrame) {
  int rc;
  BYTE_STREAM_T bstmp;
  AAC_ADTS_FRAME_T adtsFrame;
  int flvAudRate = 0;

  memset(&adtsFrame, 0, sizeof(adtsFrame));
  if(aac_decodeAdtsHdr(pFrame->pData, pFrame->len, &adtsFrame) < 0) {
    return -1;
  }

  fbs->bs.buf[fbs->bs.idx++] = FLV_TAG_AUDIODATA;
  memset(&fbs->bs.buf[fbs->bs.idx], 0, 10);
                    // 3 byte data body size
                    // 3 byte timestamp
                    // 1 byte timestamp extended
                    // 3 byte stream id
  fbs->bs.idx += 10;

  rc = (int) esds_getFrequencyVal(ESDS_FREQ_IDX(adtsFrame.sp));
  if(rc <= 5500) {
    flvAudRate = FLV_AUD_RATE_5_5KHZ; 
  } else if(rc <= 11000) {
    flvAudRate = FLV_AUD_RATE_11KHZ; 
  } else if(rc <= 22050) {
    flvAudRate = FLV_AUD_RATE_22KHZ; 
  } else if(rc <= 44100) {
    flvAudRate = FLV_AUD_RATE_44KHZ; 
  } else {
    //TODO: what to do here?... it may be ok for RTMP based transfer
    flvAudRate = FLV_AUD_RATE_44KHZ; 
  }


  fbs->bs.buf[fbs->bs.idx++] = (FLV_AUD_CODEC_AAC << 4) |
                       (flvAudRate << 2) | (FLV_AUD_SZ_16BIT << 1) |
        (adtsFrame.sp.channelIdx < 2 ? FLV_AUD_TYPE_MONO : FLV_AUD_TYPE_STEREO);
  fbs->bs.buf[fbs->bs.idx++] = FLV_AUD_AAC_PKTTYPE_SEQHDR;
  if((rc = esds_encodeDecoderSp(&fbs->bs.buf[fbs->bs.idx], 
                                fbs->bs.sz - fbs->bs.idx, &adtsFrame.sp)) > 0) {
    fbs->bs.idx += rc;
  }

  // Now that it is known, fill in the body hdr length, 
  bstmp.buf = fbs->bs.buf;
  bstmp.sz = fbs->bs.sz;
  bstmp.idx = fbs->prevtagidx + 1; 
  flv_write_int24(&bstmp, fbs->bs.idx - fbs->prevtagidx - 11);

  // 4 byte previous tag size field
  *((uint32_t *) &fbs->bs.buf[fbs->bs.idx]) = htonl(fbs->bs.idx - fbs->prevtagidx);
  fbs->bs.idx += 4;
  fbs->prevtagidx = fbs->bs.idx;

  return 0;
}

int flv_write_audtag(FLV_BYTE_STREAM_T *fbs, 
                            OUTFMT_FRAME_DATA_T *pFrame) {
  int rc = 0;
  unsigned int dataIdx = 0;

  if(pFrame->pData[0] == 0xff && (pFrame->pData[1] & 0xf0) == 0xf0) {
    if((dataIdx = 7) > pFrame->len) {
      return -1;
    }
  }

  if(11 + pFrame->len - dataIdx > fbs->bs.sz - fbs->bs.idx) {
    LOG(X_ERROR("aud flv frame size too large %d"), fbs->bs.sz - fbs->bs.idx);
    return -1;
  }

  fbs->bs.buf[fbs->bs.idx++] = FLV_TAG_AUDIODATA;
  flv_write_int24(&fbs->bs, pFrame->len - dataIdx + 2); // data size
  flv_write_int24(&fbs->bs, (uint32_t) (pFrame->pts/90.0f));  // timestamp
                    // 1 byte timestamp extended
                    // 3 byte stream id
  memset(&fbs->bs.buf[fbs->bs.idx], 0, 4);
  fbs->bs.idx += 4;
  fbs->bs.buf[fbs->bs.idx++] = (FLV_AUD_CODEC_AAC << 4) |
                       (FLV_AUD_RATE_44KHZ << 2) | (FLV_AUD_SZ_16BIT << 1) |
                        FLV_AUD_TYPE_STEREO; 
  fbs->bs.buf[fbs->bs.idx++] = FLV_AUD_AAC_PKTTYPE_RAW;

  //TODO: audio decoder sp config
  memcpy(&fbs->bs.buf[fbs->bs.idx], &pFrame->pData[dataIdx], pFrame->len - dataIdx);
  fbs->bs.idx += pFrame->len - dataIdx;

  // 4 byte previous tag size field
  *((uint32_t *) &fbs->bs.buf[fbs->bs.idx]) = htonl(fbs->bs.idx - fbs->prevtagidx);
  fbs->bs.idx += 4;
  fbs->prevtagidx = fbs->bs.idx;

  return rc;
}

#endif // 0
