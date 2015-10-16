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


#define RTMP_CLIENTID            1943563835
#define RTMP_SERVER_VERSTR       "3,5,4,210"
#define RTMP_TIMECODEOFFSET      "35437L"

#define RTMP_PLAY_PUBLISHNOTIFY        "PublishNotify"
#define RTMP_PLAY_RESET                "Reset"
#define RTMP_PLAY_START                "Start"



int rtmp_create_hdr(unsigned char *buf, uint8_t streamIdx, 
                    uint32_t ts, uint32_t sz, uint8_t contentType, 
                    uint32_t dest, int hdrlen) {

  //
  // 2 bits header length designator, 6 bits header type
  //
  buf[0] = 0x3f & streamIdx;
  if(hdrlen == 8) {
    buf[0] |= 0x40; 
  } else if(hdrlen == 4) {
    buf[0] |= 0x80; 
  } else if(hdrlen == 1) {
    buf[0] |= 0xc0; 
    return 1;
  } else if(hdrlen != 12) {
    LOG(X_ERROR("Invalid rtmp header size specified: %d"), hdrlen);
    return -1;
  }

  // 24 bits timestamp
  *((uint32_t *) &buf[1]) = htonl(ts & 0x00ffffff) >> 8;

  if(hdrlen == 4) {
    return 4;
  }
  
  // 24 bits body size 
  *((uint32_t *) &buf[4]) = htonl(sz & 0x00ffffff) >> 8;

  // 1 byte content type 
  buf[7] = contentType;

  if(hdrlen == 8) {
    return 8;
  }

  // 4 bytes destination mapping
  *((uint32_t *) &buf[8]) = htonl(dest);

  return hdrlen;
}


int rtmp_create_vidstart(RTMP_CTXT_T *pRtmp) {
  BYTE_STREAM_T bs;
  int rc;
  const unsigned int hdrlen = 12;

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 


  if((rc = flvsrv_create_vidstart(&bs, &pRtmp->av.vid)) < 0) {
    return rc;
  }

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
        RTMP_STREAM_IDX_7, 0, bs.idx - hdrlen, RTMP_CONTENT_TYPE_VIDDATA, htonl(0x01), hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_audstart(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;
  const unsigned int hdrlen = 12;

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  if((rc = flvsrv_create_audstart(&bs, &pRtmp->av.aud)) < 0) {
    return rc;
  }

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
        RTMP_STREAM_IDX_6, 0, bs.idx - hdrlen, RTMP_CONTENT_TYPE_AUDDATA, htonl(0x01), hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}



int rtmp_create_serverbw(RTMP_CTXT_T *pRtmp, unsigned int bw) {
  int rc;
  const unsigned int hdrlen = 12;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
         RTMP_STREAM_IDX_CTRL, 0, 4, RTMP_CONTENT_TYPE_SERVERBW, 0, hdrlen)) < 0) {
    return rc;
  }
  *((uint32_t *) &pRtmp->out.buf[pRtmp->out.idx + rc]) = htonl(bw);
  pRtmp->out.idx += (rc + 4);

  return rc + 4; 
}

int rtmp_create_clientbw(RTMP_CTXT_T *pRtmp, unsigned int bw) {
  int rc;
  const unsigned int hdrlen = 12;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
         RTMP_STREAM_IDX_CTRL, 0, 5, RTMP_CONTENT_TYPE_CLIENTBW, 0, hdrlen)) < 0) {
    return rc;
  }
  *((uint32_t *) &pRtmp->out.buf[pRtmp->out.idx + rc]) = htonl(bw);
  pRtmp->out.buf[pRtmp->out.idx + rc + 4] = RTMP_CLIENTBW_LIMIT_TYPE_DYNAMIC;
  pRtmp->out.idx += (rc + 5); 

  return rc + 5; 
}

int rtmp_create_ping(RTMP_CTXT_T *pRtmp, uint16_t type, uint32_t arg) {
  int rc;
  const unsigned int hdrlen = 12;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
            RTMP_STREAM_IDX_CTRL, 0, 6, RTMP_CONTENT_TYPE_PING, 0, hdrlen)) < 0) {
    return rc;
  }
  memset(&pRtmp->out.buf[pRtmp->out.idx + rc], 0, 6);
  *((uint16_t *) &pRtmp->out.buf[pRtmp->out.idx + rc]) = htons(type);
  *((uint32_t *) &pRtmp->out.buf[pRtmp->out.idx + rc + 2]) = htonl(arg);

  //avc_dumpHex(stderr, &pRtmp->out.buf[pRtmp->out.idx], 6, 0);
  pRtmp->out.idx += (rc + 6);

  return rc + 6; 
}

int rtmp_create_chunksz(RTMP_CTXT_T *pRtmp) {
  int rc;
  const unsigned int hdrlen = 12;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
            RTMP_STREAM_IDX_CTRL, 0, 4, RTMP_CONTENT_TYPE_CHUNKSZ, 0, hdrlen)) < 0) {
    return rc;
  }

  *((uint32_t *) &pRtmp->out.buf[pRtmp->out.idx + rc]) = htonl(pRtmp->chunkSzOut);
  pRtmp->out.idx += (rc + 4);

  return rc + 4; 
}

int rtmp_create_result_invoke(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;
  const unsigned int hdrlen = 12;
  unsigned int objEncoding = pRtmp->advObjEncoding;

  if(pRtmp->connect.objEncoding != objEncoding) {
    objEncoding = pRtmp->connect.objEncoding;
    LOG(X_DEBUG("Echoing client objectEncoding: %d"), objEncoding);
  }

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  // RTMP_CONTENT_TYPE_MSG appears to have extra header byte 0x00
  if(pRtmp->contentTypeLastInvoke == RTMP_CONTENT_TYPE_MSG) {
    bs.buf[bs.idx++] = 0x00; 
  }

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_key_val(&bs, "_result", 1.0, FLV_AMF_TYPE_NUMBER);

  bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;
  flv_write_key_val_string(&bs, "fmsVer", "FMS/"RTMP_SERVER_VERSTR);
  flv_write_key_val(&bs, "capabilities", pRtmp->advCapabilities, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&bs, "mode", 1.0, FLV_AMF_TYPE_NUMBER);
  flv_write_objend(&bs);

  bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;
  flv_write_key_val_string(&bs, "level", "status");
  flv_write_key_val_string(&bs, "code", RTMP_NETCONNECT_SUCCESS);
  flv_write_key_val_string(&bs, "description", "Connection succeeded.");

  flv_write_string(&bs, "data");
  bs.buf[bs.idx++] = FLV_AMF_TYPE_ECMA;
  *((uint32_t *) &bs.buf[bs.idx]) = 0;
  bs.idx += 4;

  flv_write_key_val_string(&bs, "version", RTMP_SERVER_VERSTR);
  flv_write_objend(&bs);

  flv_write_key_val(&bs, "clientid", RTMP_CLIENTID, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&bs, "objectEncoding", objEncoding, FLV_AMF_TYPE_NUMBER);
  flv_write_objend(&bs);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
      RTMP_STREAM_IDX_INVOKE, 0, bs.idx - hdrlen, pRtmp->contentTypeLastInvoke, 0, hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_result(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;
  double code;
  const unsigned int hdrlen = 12;

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

   // RTMP_CONTENT_TYPE_MSG appears to have extra header byte 0x00
  if(pRtmp->contentTypeLastInvoke == RTMP_CONTENT_TYPE_MSG) {
    bs.buf[bs.idx++] = 0x00;  
  }

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  //TODO: should this match client's createStream value
  if((code = pRtmp->connect.createStreamCode) <= 0) {
    code = 2.0;
  }
  flv_write_key_val(&bs, "_result", code, FLV_AMF_TYPE_NUMBER);
  bs.buf[bs.idx++] = FLV_AMF_TYPE_NULL;
  flv_write_val_number(&bs, 1.0);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
        RTMP_STREAM_IDX_INVOKE, 0, bs.idx - hdrlen, pRtmp->contentTypeLastInvoke, 0, hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}


int rtmp_create_error(struct RTMP_CTXT *pRtmp, double value, const char *code, const char *descr) {
  int rc;
  BYTE_STREAM_T bs;
  const unsigned int hdrlen = 12;
  int in_obj = 0;

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

   // RTMP_CONTENT_TYPE_MSG appears to have extra header byte 0x00
  if(pRtmp->contentTypeLastInvoke == RTMP_CONTENT_TYPE_MSG) {
    bs.buf[bs.idx++] = 0x00;  
  }

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  //TODO: should this match client's createStream value
  flv_write_key_val(&bs, "_error", value, FLV_AMF_TYPE_NUMBER);

  if(code != NULL && descr != NULL) {
    bs.buf[bs.idx++] = FLV_AMF_TYPE_NULL;
    bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;
    in_obj++;
    flv_write_key_val_string(&bs, "level", "error");
  }

  if(code != NULL) {
    flv_write_key_val_string(&bs, "code", code);
  }
  if(descr != NULL) {
    flv_write_key_val_string(&bs, "description", descr);
  }

  while(in_obj > 0) {
    flv_write_objend(&bs);
    in_obj--;
  }

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
        RTMP_STREAM_IDX_INVOKE, 0, bs.idx - hdrlen, pRtmp->contentTypeLastInvoke, 0, hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_close(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;
  const unsigned int hdrlen = 12;

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_key_val(&bs, "close", 0.0, FLV_AMF_TYPE_NUMBER);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
        RTMP_STREAM_IDX_INVOKE, 0, bs.idx - hdrlen, pRtmp->contentTypeLastInvoke, htonl(0x01), hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_onstatus(RTMP_CTXT_T *pRtmp, enum RTMP_ONSTATUS_TYPE type) {
  int rc;
  BYTE_STREAM_T bs;
  char buf[128];
  char descr[128];
  int write_clientid = 1;
  unsigned int streamIdx = RTMP_STREAM_IDX_5;
  unsigned int contentType =  RTMP_CONTENT_TYPE_INVOKE;
  const unsigned int hdrlen = 12;

  buf[0] = '\0';
  descr[0] = '\0';
  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  if(type == RTMP_ONSTATUS_TYPE_DATASTART) {
    flv_write_string(&bs, "onStatus");
    bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;
  } else {
    flv_write_key_val(&bs, "onStatus", 0.0, FLV_AMF_TYPE_NUMBER);
    bs.buf[bs.idx++] = FLV_AMF_TYPE_NULL;
    bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;
    flv_write_key_val_string(&bs, "level", "status");
  }

  switch(type) {
    case RTMP_ONSTATUS_TYPE_PUBNOTIFY: 
      snprintf(buf, sizeof(buf), "NetStream.Play.%s", RTMP_PLAY_PUBLISHNOTIFY); 
      snprintf(descr, sizeof(descr), "%s is now published.", pRtmp->connect.play); 
      break;
    case RTMP_ONSTATUS_TYPE_RESET: 
      snprintf(buf, sizeof(buf), "NetStream.Play.%s", RTMP_PLAY_RESET); 
      snprintf(descr, sizeof(descr), "Playing and resetting %s.", pRtmp->connect.play); 
      break;
    case RTMP_ONSTATUS_TYPE_PLAYSTART: 
      snprintf(buf, sizeof(buf), "NetStream.Play.%s", RTMP_PLAY_START); 
      snprintf(descr, sizeof(descr), "Started playing %s.", pRtmp->connect.play); 
      break;
    case RTMP_ONSTATUS_TYPE_DATASTART: 
      streamIdx = RTMP_STREAM_IDX_INVOKE;
      contentType =  RTMP_CONTENT_TYPE_NOTIFY;
      write_clientid = 0;
      snprintf(buf, sizeof(buf), "NetStream.Data.%s", RTMP_PLAY_START); 
      break;
    case RTMP_ONSTATUS_TYPE_PUBLISH: 
      streamIdx = RTMP_STREAM_IDX_INVOKE;
      snprintf(buf, sizeof(buf), "NetStream.Publish.%s", RTMP_PLAY_START); 
      snprintf(descr, sizeof(descr), "Publishing %s.", pRtmp->publish.fcpublishurl); 
      break;
    default:
     LOG(X_ERROR("invalid rtmp onstatus type"));
     return -1;
  }

  if(buf[0] != '\0') {
    flv_write_key_val_string(&bs, "code", buf);
  }
  if(descr[0] != '\0') {
    flv_write_key_val_string(&bs, "description", descr);
  }
  if(write_clientid) {
    flv_write_key_val(&bs, "clientid", RTMP_CLIENTID, FLV_AMF_TYPE_NUMBER);
  }
  if(type ==  RTMP_ONSTATUS_TYPE_PLAYSTART) {
    flv_write_key_val(&bs, "isFastPlay", 0, FLV_AMF_TYPE_BOOL);
    //flv_write_key_val_string(&bs, "timecodeOffset", RTMP_TIMECODEOFFSET);
  }
  
  flv_write_objend(&bs);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
       streamIdx, 0, bs.idx - hdrlen, contentType, htonl(0x01), hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_notify(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;
  const unsigned int hdrlen = 8;

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_key_val(&bs, "|RtmpSampleAccess", 0, FLV_AMF_TYPE_BOOL);
  bs.buf[bs.idx++] = FLV_AMF_TYPE_BOOL;
  bs.buf[bs.idx++] = 0;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
        RTMP_STREAM_IDX_5, 0, bs.idx - hdrlen, RTMP_CONTENT_TYPE_NOTIFY, 0, hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_notify_netstart(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;
  const unsigned int hdrlen = 12;

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_string(&bs, "onStatus");
  bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;
  flv_write_key_val_string(&bs, "code", "NetStream.Data.Start");
  flv_write_objend(&bs);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
        RTMP_STREAM_IDX_5, 0, bs.idx - hdrlen, RTMP_CONTENT_TYPE_NOTIFY, htonl(0x01), hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}
int rtmp_create_setDataFrame(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;
  char profileLevelId[32];
  //char sps[AVCC_SPS_MAX_LEN * 4 / 3];
  //char pps[AVCC_PPS_MAX_LEN * 4 / 3];
  unsigned numArrElements = 0;
  double d;
  const unsigned int hdrlen = 12;

  if(pRtmp->av.vid.haveSeqHdr) {
    numArrElements++;
  }

  if(pRtmp->av.aud.haveSeqHdr) {
    numArrElements++;
  }

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_string(&bs, "@setDataFrame");

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_string(&bs, "onMetaData");

  bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;

  if(pRtmp->av.vid.haveSeqHdr) {
    flv_write_key_val_string(&bs, "videocodecid", "avc1");
    if(pRtmp->av.vid.ctxt.width > 0) {
      flv_write_key_val(&bs, "width", pRtmp->av.vid.ctxt.width, FLV_AMF_TYPE_NUMBER);
    }
    if(pRtmp->av.vid.ctxt.height > 0) {
      flv_write_key_val(&bs, "height", pRtmp->av.vid.ctxt.height, FLV_AMF_TYPE_NUMBER);
    }
    if(pRtmp->av.vid.ctxt.clockHz > 0 && pRtmp->av.vid.ctxt.frameDeltaHz > 0) {
      d = (float) pRtmp->av.vid.ctxt.clockHz / pRtmp->av.vid.ctxt.frameDeltaHz;
      flv_write_key_val(&bs, "framerate", d, FLV_AMF_TYPE_NUMBER);
    }
  }
  if(pRtmp->av.aud.haveSeqHdr) {
    flv_write_key_val_string(&bs, "audiocodecid", "mp4a");
    if(pRtmp->av.aud.ctxt.channels > 0) {
      flv_write_key_val(&bs, "audiochannels", pRtmp->av.aud.ctxt.channels, FLV_AMF_TYPE_NUMBER);
    }
    if(pRtmp->av.aud.ctxt.clockHz > 0) {
      flv_write_key_val(&bs, "audiosamplerate", pRtmp->av.aud.ctxt.clockHz, FLV_AMF_TYPE_NUMBER);
    }
  }

  flv_write_string(&bs, "trackinfo");
  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRICTARR;
  *((uint32_t *) &bs.buf[bs.idx]) = htonl(numArrElements);
  bs.idx += 4;

  if(pRtmp->av.vid.haveSeqHdr) {
    bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;

    if(pRtmp->av.vid.ctxt.clockHz > 0) {
      flv_write_key_val(&bs, "timescale", pRtmp->av.vid.ctxt.clockHz, FLV_AMF_TYPE_NUMBER);
    }
    flv_write_key_val_string(&bs, "type", "video");
    flv_write_key_val_string(&bs, "language", "eng");

    snprintf(profileLevelId, sizeof(profileLevelId), "%2.2x%2.2x%2.2x", pRtmp->av.vid.codecCtxt.h264.profile, 
                                  0xc0, pRtmp->av.vid.codecCtxt.h264.level);
    flv_write_key_val_string(&bs, "profileLevelId", profileLevelId);
    //flv_write_key_val_string(&bs, "spropParameterSets", spropParameterSets);
    flv_write_objend(&bs);
  }

  if(pRtmp->av.aud.haveSeqHdr) {
    bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;
    flv_write_key_val_string(&bs, "type", "audio");
    flv_write_key_val_string(&bs, "language", "eng");

    flv_write_objend(&bs);
  }

  flv_write_objend(&bs);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx],
        RTMP_STREAM_IDX_5, 0, bs.idx - hdrlen, RTMP_CONTENT_TYPE_NOTIFY, htonl(0x01), hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx;
}

int rtmp_create_onmeta(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;
  unsigned numArrElements = 0;
  double d;
  const unsigned int hdrlen = 8;

  if(pRtmp->av.vid.haveSeqHdr) {
    numArrElements++;
  }

  if(pRtmp->av.aud.haveSeqHdr) {
    numArrElements++;
  }

  LOG(X_DEBUG("rtmp onMetaData vid:{ profile:%d/%d %dx%d %d/%dHz }, aud:{ %dHz/%d }"),
     pRtmp->av.vid.codecCtxt.h264.profile, pRtmp->av.vid.codecCtxt.h264.level, 
     pRtmp->av.vid.ctxt.width, pRtmp->av.vid.ctxt.height, 
     pRtmp->av.vid.ctxt.clockHz, pRtmp->av.vid.ctxt.frameDeltaHz, 
     pRtmp->av.aud.ctxt.clockHz, pRtmp->av.aud.ctxt.channels);

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_string(&bs, "onMetaData");
  bs.buf[bs.idx++] = FLV_AMF_TYPE_ECMA;
  *((uint32_t *) &bs.buf[bs.idx]) = htonl(0x00);
  bs.idx += 4;

  flv_write_string(&bs, "trackinfo");
  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRICTARR;
  *((uint32_t *) &bs.buf[bs.idx]) = htonl(numArrElements);
  bs.idx += 4;

  if(pRtmp->av.vid.haveSeqHdr) {

    bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;

    if(pRtmp->av.vid.ctxt.clockHz > 0) {
      flv_write_key_val(&bs, "timescale", pRtmp->av.vid.ctxt.clockHz, FLV_AMF_TYPE_NUMBER);
    }
    //flv_write_key_val(&bs, "length", 794700, FLV_AMF_TYPE_NUMBER);
    flv_write_key_val_string(&bs, "language", "eng");

    flv_write_string(&bs, "sampledescription");
    bs.buf[bs.idx++] = FLV_AMF_TYPE_STRICTARR;
    *((uint32_t *) &bs.buf[bs.idx]) = htonl(0x01);
    bs.idx += 4;
    bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;
    flv_write_key_val_string(&bs, "sampletype", "avc1");
    flv_write_objend(&bs);
    flv_write_objend(&bs);

  }

  if(pRtmp->av.aud.haveSeqHdr) {
    bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;
    if(pRtmp->av.aud.ctxt.clockHz) {
      flv_write_key_val(&bs, "timescale", pRtmp->av.aud.ctxt.clockHz, FLV_AMF_TYPE_NUMBER);
    }
    //flv_write_key_val(&bs, "length", 5860352, FLV_AMF_TYPE_NUMBER);
    flv_write_key_val_string(&bs, "language", "eng");

    flv_write_string(&bs, "sampledescription");
    bs.buf[bs.idx++] = FLV_AMF_TYPE_STRICTARR;
    *((uint32_t *) &bs.buf[bs.idx]) = htonl(0x01);
    bs.idx += 4;
    bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;

    flv_write_key_val_string(&bs, "sampletype", "mp4a");

    flv_write_objend(&bs);
    flv_write_objend(&bs);

  }

  if(pRtmp->av.vid.haveSeqHdr) {
    flv_write_key_val_string(&bs, "videocodecid", "avc1");
    if(pRtmp->av.vid.ctxt.clockHz > 0 && pRtmp->av.vid.ctxt.frameDeltaHz > 0) {
      d = (float) pRtmp->av.vid.ctxt.clockHz / pRtmp->av.vid.ctxt.frameDeltaHz;
      flv_write_key_val(&bs, "videoframerate", d, FLV_AMF_TYPE_NUMBER);
    }
    flv_write_key_val(&bs, "avcprofile", pRtmp->av.vid.codecCtxt.h264.profile, FLV_AMF_TYPE_NUMBER);
    flv_write_key_val(&bs, "avclevel", pRtmp->av.vid.codecCtxt.h264.level, FLV_AMF_TYPE_NUMBER);
    if(pRtmp->av.vid.ctxt.width > 0) {
      flv_write_key_val(&bs, "width", pRtmp->av.vid.ctxt.width, FLV_AMF_TYPE_NUMBER);
      //flv_write_key_val(&bs, "frameWidth", pRtmp->av.vid.ctxt.width, FLV_AMF_TYPE_NUMBER);
      //flv_write_key_val(&bs, "displayWidth", pRtmp->av.vid.ctxt.width, FLV_AMF_TYPE_NUMBER);
    }
    if(pRtmp->av.vid.ctxt.height > 0) {
      flv_write_key_val(&bs, "height", pRtmp->av.vid.ctxt.height, FLV_AMF_TYPE_NUMBER);
      //flv_write_key_val(&bs, "frameHeight", pRtmp->av.vid.ctxt.height, FLV_AMF_TYPE_NUMBER);
      //flv_write_key_val(&bs, "displayHeight", pRtmp->av.vid.ctxt.height, FLV_AMF_TYPE_NUMBER);
    }
  }

  if(pRtmp->av.aud.haveSeqHdr) {
    flv_write_key_val_string(&bs, "audiocodecid", "mp4a");
    //flv_write_key_val(&bs, "aacaot", 2, FLV_AMF_TYPE_NUMBER);
    if(pRtmp->av.aud.ctxt.channels > 0) {
      flv_write_key_val(&bs, "audiochannels", pRtmp->av.aud.ctxt.channels, FLV_AMF_TYPE_NUMBER);
    }
    if(pRtmp->av.aud.ctxt.clockHz > 0) {
      flv_write_key_val(&bs, "audiosamplerate", pRtmp->av.aud.ctxt.clockHz, FLV_AMF_TYPE_NUMBER);
    }
  }

  //flv_write_key_val(&bs, "moovposition", 11051949, FLV_AMF_TYPE_NUMBER);
  //flv_write_key_val(&bs, "duration", 265.775601, FLV_AMF_TYPE_NUMBER);

  flv_write_objend(&bs);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
        RTMP_STREAM_IDX_5, 0, bs.idx - hdrlen, RTMP_CONTENT_TYPE_NOTIFY, 0, hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_connect(RTMP_CTXT_T *pRtmp, const RTMP_CLIENT_PARAMS_T *pClient) {
  int rc;
  BYTE_STREAM_T bs;
  const unsigned int hdrlen = 12;

  if(!pRtmp || !pClient) {
    return -1;
  }

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_key_val(&bs, "connect", 1.0, FLV_AMF_TYPE_NUMBER);
  bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;
  if(pRtmp->connect.app[0] != '\0') {
    flv_write_key_val_string(&bs, "app", pRtmp->connect.app);
  }
  if(pClient->flashVer && pClient->flashVer[0] != '\0') {
    flv_write_key_val_string(&bs, "flashVer", pClient->flashVer);
  }
  if(pClient->cfg.prtmpswfurl && pClient->cfg.prtmpswfurl[0] != '\0') {
    flv_write_key_val_string(&bs, "swfUrl", pClient->cfg.prtmpswfurl);
  }
  if(pRtmp->connect.tcUrl[0] != '\0') {
    flv_write_key_val_string(&bs, "tcUrl", pRtmp->connect.tcUrl);
  }

  //flv_write_key_val_string(&bs, "type", "nonprivate");
  flv_write_key_val(&bs, "fpad", 0, FLV_AMF_TYPE_BOOL);
  flv_write_key_val(&bs, "capabilities", pRtmp->connect.capabilities, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&bs, "audioCodecs", 3191, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&bs, "videoCodecs", 252, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&bs, "videoFunction", 1.0, FLV_AMF_TYPE_NUMBER);
  if(pClient->cfg.prtmppageurl && pClient->cfg.prtmppageurl[0] != '\0') {
    flv_write_key_val_string(&bs, "pageUrl", pClient->cfg.prtmppageurl);
  }
  //flv_write_key_val(&bs, "objectEncoding", pRtmp->connect.objEncoding, FLV_AMF_TYPE_NUMBER);
  //flv_write_key_val(&bs, "sendChunkSize", pRtmp->chunkSzOut, FLV_AMF_TYPE_NUMBER);

  flv_write_objend(&bs);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx],
        RTMP_STREAM_IDX_INVOKE, 0, bs.idx - hdrlen, RTMP_CONTENT_TYPE_INVOKE, 0, hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_releaseStream(RTMP_CTXT_T *pRtmp, const RTMP_CLIENT_PARAMS_T *pClient) {
  int rc;
  BYTE_STREAM_T bs;
  const unsigned int hdrlen = 12;

  if(!pRtmp) {
    return -1;
  }

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_key_val(&bs, "releaseStream", 2.0, FLV_AMF_TYPE_NUMBER);
  bs.buf[bs.idx++] = FLV_AMF_TYPE_NULL;
  if(pClient && pClient->playElem[0] != '\0') {
    bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
    flv_write_string(&bs, pClient->playElem);
  }

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx],
        RTMP_STREAM_IDX_INVOKE, 0, bs.idx - hdrlen, RTMP_CONTENT_TYPE_INVOKE, 0, hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_createStream(RTMP_CTXT_T *pRtmp, int contentType) {
  int rc;
  BYTE_STREAM_T bs;
  const unsigned int hdrlen = 12;

  if(!pRtmp || (contentType != RTMP_CONTENT_TYPE_MSG && contentType != RTMP_CONTENT_TYPE_INVOKE)) {
    return -1;
  }

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  if(contentType == RTMP_CONTENT_TYPE_MSG) {
    // RTMP_CONTENT_TYPE_MSG appears to have extra header byte 0x00
    bs.buf[bs.idx++] = 0;
  }

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_key_val(&bs, "createStream", 2.0, FLV_AMF_TYPE_NUMBER);
  bs.buf[bs.idx++] = FLV_AMF_TYPE_NULL;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx],
        RTMP_STREAM_IDX_INVOKE, 0, bs.idx - hdrlen, contentType, 0, hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_play(RTMP_CTXT_T *pRtmp, const RTMP_CLIENT_PARAMS_T *pClient) {
  int rc;
  BYTE_STREAM_T bs;
  const unsigned int hdrlen = 12;

  if(!pRtmp || !pClient || !pClient->playElem) {
    return -1;
  }

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  // RTMP_CONTENT_TYPE_MSG appears to have extra header byte 0x00
  bs.buf[bs.idx++] = 0;

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_key_val(&bs, "play", .0, FLV_AMF_TYPE_NUMBER);
  bs.buf[bs.idx++] = FLV_AMF_TYPE_NULL;
  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_string(&bs, pClient->playElem);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx],
        RTMP_STREAM_IDX_8, 0, bs.idx - hdrlen, RTMP_CONTENT_TYPE_MSG, htonl(0x01), hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_publish(RTMP_CTXT_T *pRtmp, const RTMP_CLIENT_PARAMS_T *pClient) {
  int rc;
  BYTE_STREAM_T bs;
  const unsigned int hdrlen = 12;

  if(!pRtmp || !pClient || !pClient->playElem) {
    LOG(X_ERROR("rtmp_create_publish invalid arguments '%s'"), pClient ? pClient->playElem : "");
    return -1;
  }

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_key_val(&bs, "publish", 0.0, FLV_AMF_TYPE_NUMBER);
  bs.buf[bs.idx++] = FLV_AMF_TYPE_NULL;
  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_string(&bs, pClient->playElem);

  //
  // "record", "append", "live"
  //
  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_string(&bs, "live");  

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx],
        RTMP_STREAM_IDX_INVOKE, 0, bs.idx - hdrlen, RTMP_CONTENT_TYPE_INVOKE, htonl(0x01), hdrlen)) < 0) {
        //RTMP_STREAM_IDX_INVOKE, 0, bs.idx - hdrlen, RTMP_CONTENT_TYPE_INVOKE, htonl(0x01), hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_fcpublish(RTMP_CTXT_T *pRtmp, const RTMP_CLIENT_PARAMS_T *pClient) {
  int rc;
  BYTE_STREAM_T bs;
  const unsigned int hdrlen = 12;

  if(!pRtmp || !pClient || !pClient->playElem) {
    LOG(X_ERROR("Unable to crate RTMP fcpublish request"));
    return -1;
  }

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_key_val(&bs, "FCPublish", 0.0, FLV_AMF_TYPE_NUMBER);
  bs.buf[bs.idx++] = FLV_AMF_TYPE_NULL;
  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_string(&bs, pClient->playElem);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx],
        RTMP_STREAM_IDX_INVOKE, 0, bs.idx - hdrlen, RTMP_CONTENT_TYPE_INVOKE, 0, hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_onfcpublish(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;
  char buf[512];
  const unsigned int hdrlen = 12;

  if(!pRtmp) {
    return -1;
  }

  bs.idx = hdrlen;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 


  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_key_val(&bs, "onFCPublish", .0, FLV_AMF_TYPE_NUMBER);
  bs.buf[bs.idx++] = FLV_AMF_TYPE_NULL;
  bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;
  flv_write_key_val_string(&bs, "level", "status");
  flv_write_key_val_string(&bs, "code", "Netstream.Publish.Start");
  snprintf(buf, sizeof(buf), "FCPublish to stream %s.", pRtmp->connect.play);
  flv_write_key_val_string(&bs, "description", buf);
  flv_write_key_val(&bs, "clientid", RTMP_CLIENTID, FLV_AMF_TYPE_NUMBER);
  flv_write_objend(&bs);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx],
        RTMP_STREAM_IDX_INVOKE, 0, bs.idx - hdrlen, RTMP_CONTENT_TYPE_INVOKE, htonl(0), hdrlen)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}



int rtmp_send_chunkdata(RTMP_CTXT_T *pRtmp, const uint8_t rtmpStreamIdx,
                        const unsigned char *pData, unsigned int lenData,
                        unsigned int chunkStartIdx) {
  int rc = 0;
  unsigned int lenXmit;
  unsigned int idxData = 0; 

  if(chunkStartIdx > pRtmp->chunkSzOut) {
    LOG(X_ERROR("rtmp chunk start idx %d > chunk size %d"), chunkStartIdx, pRtmp->chunkSzOut);
    return -1;
  }

  while(idxData < lenData) {

    if((rc = rtmp_send(pRtmp, "rtmp_send_chunkdata")) < 0) {
      return rc;
    }
    pRtmp->out.buf[0] = 0xc0 | (rtmpStreamIdx & 0x3f);
    pRtmp->out.idx = 1;

    if(lenData - idxData + chunkStartIdx < pRtmp->chunkSzOut) {
      lenXmit = lenData - idxData; 
    } else {
      lenXmit = pRtmp->chunkSzOut - chunkStartIdx;
    }

    VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - rtmp_send_chunkdata [%d]/%d, chunksz: %d, send len:%d"), 
                        idxData, lenData, pRtmp->chunkSzOut, lenXmit);
                    LOGHEXT_DEBUGVV(&pData[idxData], lenXmit); );

    if((rc = rtmp_sendbuf(pRtmp, &pData[idxData], lenXmit, "rtmp_send_chunkdata")) < 0) {
      return rc;
    }

    idxData += lenXmit;
    chunkStartIdx = 0;
  }

  pRtmp->out.idx = 0;

  return rc;
}


int rtmp_cbReadDataNet(void *pArg, unsigned char *pData, unsigned int len) {
  RTMP_CTXT_T *pRtmp = (RTMP_CTXT_T *) pArg;
  int rc;

  if((rc = netio_recvnb_exact(&pRtmp->pSd->netsocket, pData, len, pRtmp->rcvTmtMs)) != len) {
    if(rc == 0) {
      return rc;
    }
    return -1;
  }

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - rtmp_read len:%d/%d"), rc, len);
                  LOGHEXT_DEBUGV(pData, rc); );

  pRtmp->bytesRead += len;

  return rc;
}

int rtmp_parse_readpkt_full(RTMP_CTXT_T *pRtmp, int needpacket, FLV_AMF_T *pAmfList) {
  int rc = 0;

  pRtmp->in.idx = 0;
  pRtmp->methodParsed = RTMP_METHOD_UNKNOWN;

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - parse_readpkt_full called")) );

  if((rc = rtmp_parse_readpkt(pRtmp)) < 0) {
    return -1;
  } else if(rc == 0) {

    //
    // its possibly a nonblocking read (with timeout) returned 0 bytes
    //

    if(needpacket) {
      LOG(X_ERROR("Failed to read entire rtmp packet contents"));    
      return -1;  
    } else {
      return 0;
    }
  }  

  VSX_DEBUG_RTMP( 
    LOG(X_DEBUG("RTMP - parse_readpkt_full body sz:%d, ct:0x%x, id:%d, ts:%d(%d), dest:0x%x, hdr:%d"), 
       pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt, pRtmp->pkt[pRtmp->streamIdx].hdr.contentType, 
       pRtmp->pkt[pRtmp->streamIdx].hdr.id, pRtmp->pkt[pRtmp->streamIdx].hdr.ts, pRtmp->pkt[pRtmp->streamIdx].tsAbsolute, 
       pRtmp->pkt[pRtmp->streamIdx].hdr.dest, pRtmp->pkt[pRtmp->streamIdx].szHdr0); );

  switch(pRtmp->pkt[pRtmp->streamIdx].hdr.contentType) {
    case RTMP_CONTENT_TYPE_INVOKE:
    case RTMP_CONTENT_TYPE_MSG:
    case RTMP_CONTENT_TYPE_NOTIFY:
      //VSX_DEBUG_RTMP( LOGHEXT_DEBUGVV(pRtmp->in.buf, pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt) );
      rtmp_parse_invoke(pRtmp, pAmfList);
      rc = 1;
      break;
    case RTMP_CONTENT_TYPE_PING:
      pRtmp->methodParsed = RTMP_METHOD_PING;
      break;
    case RTMP_CONTENT_TYPE_CLIENTBW:
      pRtmp->methodParsed = RTMP_METHOD_CLIENTBW;
      break;
    case RTMP_CONTENT_TYPE_SERVERBW:
      pRtmp->methodParsed = RTMP_METHOD_SERVERBW;
      break;
    case RTMP_CONTENT_TYPE_BYTESREAD:
      pRtmp->methodParsed = RTMP_METHOD_BYTESREAD;
      break;
    case RTMP_CONTENT_TYPE_CHUNKSZ:
      pRtmp->methodParsed = RTMP_METHOD_CHUNKSZ;
      break;
    default:
      VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - contentType: 0x%x"), pRtmp->pkt[pRtmp->streamIdx].hdr.contentType);
                      LOGHEXT_DEBUGV(pRtmp->in.buf, pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt) );
      break;
  }

  return 0;
}

int rtmp_send(RTMP_CTXT_T *pRtmp, const char *descr) {
  return rtmp_sendbuf(pRtmp, pRtmp->out.buf, pRtmp->out.idx, descr);
}

int rtmp_sendbuf(RTMP_CTXT_T *pRtmp, const unsigned char *buf, unsigned int sz, const char *descr) {

  int rc = 0;
  if(!pRtmp) {
    return -1;
  }

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - %s send: %d"), descr ? descr : "", sz);
                  LOGHEXT_DEBUGV(buf, sz); );

  if((rc = netio_send(&pRtmp->pSd->netsocket, (const struct sockaddr *) &pRtmp->pSd->sa, buf, sz)) < 0) {
    return -1;
  }

  return rc;
}
