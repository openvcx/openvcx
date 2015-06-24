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

  bs.idx = 12;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 


  if((rc = flvsrv_create_vidstart(&bs, &pRtmp->av.vid)) < 0) {
    return rc;
  }

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
        RTMP_STREAM_IDX_7, 0, bs.idx - 12, RTMP_CONTENT_TYPE_VIDDATA, htonl(0x01), 12)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_audstart(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;

  bs.idx = 12;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  if((rc = flvsrv_create_audstart(&bs, &pRtmp->av.aud)) < 0) {
    return rc;
  }

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
        RTMP_STREAM_IDX_6, 0, bs.idx - 12, RTMP_CONTENT_TYPE_AUDDATA, htonl(0x01), 12)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}



int rtmp_create_serverbw(RTMP_CTXT_T *pRtmp, unsigned int bw) {
  int rc;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
         RTMP_STREAM_IDX_CTRL, 0, 4, RTMP_CONTENT_TYPE_SERVERBW, 0, 12)) < 0) {
    return rc;
  }
  *((uint32_t *) &pRtmp->out.buf[pRtmp->out.idx + rc]) = htonl(bw);
  pRtmp->out.idx += (rc + 4);

  return rc + 4; 
}

int rtmp_create_clientbw(RTMP_CTXT_T *pRtmp, unsigned int bw) {
  int rc;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
         RTMP_STREAM_IDX_CTRL, 0, 5, RTMP_CONTENT_TYPE_CLIENTBW, 0, 12)) < 0) {
    return rc;
  }
  *((uint32_t *) &pRtmp->out.buf[pRtmp->out.idx + rc]) = htonl(bw);
  pRtmp->out.buf[pRtmp->out.idx + rc + 4] = 0x02;  // ??
  pRtmp->out.idx += (rc + 5); 

  return rc + 5; 
}

int rtmp_create_ping(RTMP_CTXT_T *pRtmp, uint16_t type, uint32_t arg) {
  int rc;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
            RTMP_STREAM_IDX_CTRL, 0, 6, RTMP_CONTENT_TYPE_PING, 0, 12)) < 0) {
    return rc;
  }
  memset(&pRtmp->out.buf[pRtmp->out.idx + rc], 0, 6);
  *((uint16_t *) &pRtmp->out.buf[pRtmp->out.idx + rc]) = htons(type);
  *((uint32_t *) &pRtmp->out.buf[pRtmp->out.idx + rc + 2]) = htonl(arg);

  //avc_dumpHex(stderr, &pRtmp->out.buf[pRtmp->out.idx], 6, 0);
  pRtmp->out.idx += (rc + 6);

  return rc + 6; 
}

int rtmp_create_chunksz(RTMP_CTXT_T *pRtmp, unsigned int sz) {
  int rc;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
            RTMP_STREAM_IDX_CTRL, 0, 4, RTMP_CONTENT_TYPE_CHUNKSZ, 0, 12)) < 0) {
    return rc;
  }

  *((uint32_t *) &pRtmp->out.buf[pRtmp->out.idx + rc]) = htonl(sz);
  pRtmp->out.idx += (rc + 4);

  return rc + 4; 
}

int rtmp_create_result_invoke(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;
  unsigned int objEncoding = pRtmp->advObjEncoding;

  if(pRtmp->connect.objEncoding != objEncoding) {
    objEncoding = pRtmp->connect.objEncoding;
    LOG(X_DEBUG("Echoing client objectEncoding: %d"), objEncoding);
  }

  bs.idx = 12;
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
  flv_write_key_val_string(&bs, "code", "NetConnection.Connect.Success");
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
      RTMP_STREAM_IDX_INVOKE, 0, bs.idx - 12, pRtmp->contentTypeLastInvoke, 0, 12)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_result(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;
  double code;

  bs.idx = 12;
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
        RTMP_STREAM_IDX_INVOKE, 0, bs.idx - 12, pRtmp->contentTypeLastInvoke, 0, 12)) < 0) {
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
  unsigned int streamIdx = RTMP_STREAM_IDX_5;

  bs.idx = 12;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_key_val(&bs, "onStatus", 0.0, FLV_AMF_TYPE_NUMBER);
  bs.buf[bs.idx++] = FLV_AMF_TYPE_NULL;
  bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;
  flv_write_key_val_string(&bs, "level", "status");

  switch(type) {
    case RTMP_ONSTATUS_TYPE_PUBNOTIFY: 
      snprintf(buf, sizeof(buf), "NetStream.Play.%s", RTMP_PLAY_PUBLISHNOTIFY); 
      snprintf(descr, sizeof(descr), "%s is now published.", pRtmp->connect.play); 
      break;
    case RTMP_ONSTATUS_TYPE_RESET: 
      snprintf(buf, sizeof(buf), "NetStream.Play.%s", RTMP_PLAY_RESET); 
      snprintf(descr, sizeof(descr), "Playing and resetting %s.", pRtmp->connect.play); 
      break;
    case RTMP_ONSTATUS_TYPE_START: 
      snprintf(buf, sizeof(buf), "NetStream.Play.%s", RTMP_PLAY_START); 
      snprintf(descr, sizeof(descr), "Started playing %s.", pRtmp->connect.play); 
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

  flv_write_key_val_string(&bs, "code", buf);
  flv_write_key_val_string(&bs, "description", descr);
  flv_write_key_val(&bs, "clientid", RTMP_CLIENTID, FLV_AMF_TYPE_NUMBER);
  if(type == 2) {
    flv_write_key_val(&bs, "isFastPlay", 0, FLV_AMF_TYPE_BOOL);
    //flv_write_key_val_string(&bs, "timecodeOffset", RTMP_TIMECODEOFFSET);
  }
  
  flv_write_objend(&bs);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
       streamIdx, 0, bs.idx - 12, RTMP_CONTENT_TYPE_INVOKE, htonl(0x01), 12)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_notify(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;

  bs.idx = 8;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_key_val(&bs, "|RtmpSampleAccess", 0, FLV_AMF_TYPE_BOOL);
  bs.buf[bs.idx++] = FLV_AMF_TYPE_BOOL;
  bs.buf[bs.idx++] = 0;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
        RTMP_STREAM_IDX_5, 0, bs.idx - 8, RTMP_CONTENT_TYPE_NOTIFY, 0, 8)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_notify_netstart(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;

  bs.idx = 12;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_string(&bs, "onStatus");
  bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;
  flv_write_key_val_string(&bs, "code", "NetStream.Data.Start");
  flv_write_objend(&bs);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx], 
        RTMP_STREAM_IDX_5, 0, bs.idx - 12, RTMP_CONTENT_TYPE_NOTIFY, htonl(0x01), 12)) < 0) {
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

  bs.idx = 8;
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
        RTMP_STREAM_IDX_5, 0, bs.idx - 8, RTMP_CONTENT_TYPE_NOTIFY, 0, 8)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_connect(RTMP_CTXT_T *pRtmp, RTMP_CLIENT_PARAMS_T *pClient) {
  int rc;
  BYTE_STREAM_T bs;

  if(!pRtmp || !pClient) {
    return -1;
  }

  bs.idx = 12;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_key_val(&bs, "connect", 1.0, FLV_AMF_TYPE_NUMBER);
  bs.buf[bs.idx++] = FLV_AMF_TYPE_OBJECT;
  if(pClient->app && pClient->app[0] != '\0') {
    flv_write_key_val_string(&bs, "app", pClient->app);
  }
  if(pClient->flashVer && pClient->flashVer[0] != '\0') {
    flv_write_key_val_string(&bs, "flashVer", pClient->flashVer);
  }
  if(pClient->swfUrl && pClient->swfUrl[0] != '\0') {
    flv_write_key_val_string(&bs, "swfUrl", pClient->swfUrl);
  }
  if(pClient->tcUrl && pClient->tcUrl[0] != '\0') {
    flv_write_key_val_string(&bs, "tcUrl", pClient->tcUrl);
  }
  flv_write_key_val(&bs, "fpad", 0, FLV_AMF_TYPE_BOOL);
  flv_write_key_val(&bs, "capabilities", pRtmp->connect.capabilities, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&bs, "audioCodecs", 3191, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&bs, "videoCodecs", 252, FLV_AMF_TYPE_NUMBER);
  flv_write_key_val(&bs, "videoFunction", 1.0, FLV_AMF_TYPE_NUMBER);
  if(pClient->pageUrl && pClient->pageUrl[0] != '\0') {
    flv_write_key_val_string(&bs, "pageUrl", pClient->pageUrl);
  }
  flv_write_key_val(&bs, "objectEncoding", pRtmp->connect.objEncoding, FLV_AMF_TYPE_NUMBER);

  flv_write_objend(&bs);

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx],
        RTMP_STREAM_IDX_INVOKE, 0, bs.idx - 12, RTMP_CONTENT_TYPE_INVOKE, 0, 12)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_createStream(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;

  if(!pRtmp) {
    return -1;
  }

  bs.idx = 8;
  bs.buf = &pRtmp->out.buf[pRtmp->out.idx];
  bs.sz = pRtmp->out.sz - pRtmp->out.idx; 


  // RTMP_CONTENT_TYPE_MSG appears to have extra header byte 0x00
  bs.buf[bs.idx++] = 0;

  bs.buf[bs.idx++] = FLV_AMF_TYPE_STRING;
  flv_write_key_val(&bs, "createStream", 2.0, FLV_AMF_TYPE_NUMBER);
  bs.buf[bs.idx++] = FLV_AMF_TYPE_NULL;

  if((rc = rtmp_create_hdr(&pRtmp->out.buf[pRtmp->out.idx],
        RTMP_STREAM_IDX_INVOKE, 0, bs.idx - 8, RTMP_CONTENT_TYPE_MSG, 0, 8)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_play(RTMP_CTXT_T *pRtmp, RTMP_CLIENT_PARAMS_T *pClient) {
  int rc;
  BYTE_STREAM_T bs;

  if(!pRtmp || !pClient || !pClient->playElem) {
    return -1;
  }

  bs.idx = 12;
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
        RTMP_STREAM_IDX_8, 0, bs.idx - 12, RTMP_CONTENT_TYPE_MSG, htonl(0x01), 12)) < 0) {
    return rc;
  }

  pRtmp->out.idx += bs.idx;

  return pRtmp->out.idx - bs.idx; 
}

int rtmp_create_onfcpublish(RTMP_CTXT_T *pRtmp) {
  int rc;
  BYTE_STREAM_T bs;
  char buf[512];

  if(!pRtmp) {
    return -1;
  }

  bs.idx = 12;
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
        RTMP_STREAM_IDX_INVOKE, 0, bs.idx - 12, RTMP_CONTENT_TYPE_INVOKE, htonl(0), 12)) < 0) {
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

  if(chunkStartIdx > pRtmp->chunkSz) {
    LOG(X_ERROR("rtmp chunk start idx %d > chunk size %d"), chunkStartIdx, pRtmp->chunkSz);
    return -1;
  }

  while(idxData < lenData) {

    //fprintf(stderr, "rtmp data chunk hdr len:%d\n", pRtmp->out.idx);
    //avc_dumpHex(stderr, pRtmp->out.buf, pRtmp->out.idx, 1);

    if((rc = netio_send(&pRtmp->pSd->netsocket, &pRtmp->pSd->sain, pRtmp->out.buf,
                        pRtmp->out.idx)) < 0) {
      return -1;
    }
    pRtmp->out.buf[0] = 0xc0 | (rtmpStreamIdx & 0x3f);
    pRtmp->out.idx = 1;

    //if(lenData - idxData + chunkStartIdx < RTMP_CHUNK_SZ_OUT) {
    if(lenData - idxData + chunkStartIdx < pRtmp->chunkSz) {
      lenXmit = lenData - idxData; 
    } else {
      //lenXmit = RTMP_CHUNK_SZ_OUT - chunkStartIdx;
      lenXmit = pRtmp->chunkSz - chunkStartIdx;
    }

    //fprintf(stderr, "rtmp data chunk [%d] len:%d chunkSz:%d\n", idxData, lenXmit, pRtmp->chunkSz);
    //avc_dumpHex(stderr, &pData[idxData], lenXmit, 1);

    if((rc = netio_send(&pRtmp->pSd->netsocket, &pRtmp->pSd->sain, &pData[idxData], lenXmit)) < 0) {
      return -1;
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

  pRtmp->bytesRead += len;

  return rc;
}

int rtmp_parse_readpkt_full(RTMP_CTXT_T *pRtmp, int needpacket) {
  int rc = 0;

  pRtmp->in.idx = 0;
  pRtmp->methodParsed = RTMP_METHOD_UNKNOWN;

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

  //fprintf(stderr, "rtmp full pkt sz:%d ct:%d, id:%d, ts:%d(%d), dest:0x%x, hdr:%d\n", pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt, pRtmp->pkt[pRtmp->streamIdx].hdr.contentType, pRtmp->pkt[pRtmp->streamIdx].hdr.id, pRtmp->pkt[pRtmp->streamIdx].hdr.ts, pRtmp->pkt[pRtmp->streamIdx].tsAbsolute, pRtmp->pkt[pRtmp->streamIdx].hdr.dest, pRtmp->pkt[pRtmp->streamIdx].szHdr0);

  switch(pRtmp->pkt[pRtmp->streamIdx].hdr.contentType) {
    case RTMP_CONTENT_TYPE_INVOKE:
    case RTMP_CONTENT_TYPE_MSG:
    case RTMP_CONTENT_TYPE_NOTIFY:
      //avc_dumpHex(stderr, pRtmp->in.buf, pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt, 1);
      rtmp_parse_invoke(pRtmp, NULL);
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
      //avc_dumpHex(stderr, pRtmp->in.buf, pRtmp->pkt[pRtmp->streamIdx].hdr.szPkt, 1);

      break;
  }

  return 0;
}



