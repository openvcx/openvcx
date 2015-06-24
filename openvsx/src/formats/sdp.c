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


//Max sdp file size processed
#define MAX_SDP_FILE_SIZE               4096

//SDP fmtp custom attributes
#define SDP_ATTR_PACKETIZATION_MODE              "packetization-mode"
#define SDP_ATTR_PROFILE_LEVEL_ID                "profile-level-id"
#define SDP_ATTR_SPROP_PARAMETER_SETS            "sprop-parameter-sets"
#define SDP_ATTR_MODE                            "mode"
#define SDP_ATTR_SIZELENGTH                      "sizelength"
#define SDP_ATTR_INDEXLENGTH                     "indexlength"
#define SDP_ATTR_INDEXDELTALENGTH                "indexdeltalength"
#define SDP_ATTR_CONFIG                          "config"
#define SDP_ATTR_OCTET_ALIGN                     "octet-align"

#define SDP_ICE_TRANS_STR_UDP  "udp"
#define SDP_ICE_TRANS_STR_TCP  "tcp"

#define SDP_ICE_STR_PRFFLX    "prflx"
#define SDP_ICE_STR_SRFFLX    "srflx"
#define SDP_ICE_STR_HOST      "host"
#define SDP_ICE_STR_RELAY     "relay"


enum SDP_MEDIA_TYPE {
  SDP_MEDIA_TYPE_UNKNOWN = 0,
  SDP_MEDIA_TYPE_VIDEO   = 1,
  SDP_MEDIA_TYPE_AUDIO   = 2
};

typedef struct SDP_PARSE_CTXT {
  SDP_DESCR_T         *pSdp;
  int                  verify;
  int                  allow_noport_rtsp;
  int                  lastFmt;
  enum SDP_MEDIA_TYPE  lastMediaType; 
} SDP_PARSE_CTXT_T;

typedef int (*SDP_PARSE_CB_FMTP_PARAM)(SDP_PARSE_CTXT_T *, const char *, const char *);


const char *nextval(const char **pp, char delim) {
  const char *p;

  while(**pp == ' ' || **pp == '\t') {
    (*pp)++;
  }
  if(delim) {
    while(**pp == delim) {
      (*pp)++;
    }
  }

  p = *pp; 

  while(((delim && **pp != delim) || (!delim && **pp != ' ' && **pp != '\t')) &&
            **pp != '\0' && **pp != '\r' && **pp != '\n') {
    //fprintf(stderr, "move past %c\n", **pp);
    (*pp)++;
  }

  return p;
}

static void safe_copyto(const char *p, const char *p2, char *buf, unsigned int maxlen) {
  unsigned int sz;
  if((sz = (ptrdiff_t) (p2 - p)) > maxlen - 1) {
    sz = (ptrdiff_t) (p2 - p) - 1;
  }
  memcpy(buf, p, sz);
  buf[sz] = '\0';
}

static int sdp_parse_connect(SDP_PARSE_CTXT_T *pParseCtxt, const char *val) {
  int rc = 0;
  char buf[64];
  const char *p;

  memset(&pParseCtxt->pSdp->c, 0, sizeof(pParseCtxt->pSdp->c));
  pParseCtxt->pSdp->c.iphost[0] = '\0';
  pParseCtxt->pSdp->c.numaddr = 1;

  //
  // consume nettype
  //
  p = nextval(&val, 0);
  if(strncmp(p, "IN", 2)) {
    LOG(X_ERROR("Unsupported SDP connect net type in %s"), val);
    return -1;
  }

  //
  // consume IP address type
  //
  p = nextval(&val, 0);
  if(strncmp(p, "IP4", 2)) {
    LOG(X_ERROR("Unsupported SDP connect address type in %s"), val);
    return -1;
  }

  //
  // consume IP address 
  //
  if(!(p = nextval(&val,'/')) || (ptrdiff_t)(val - p) <= 0) {
    LOG(X_ERROR("SDP missing connect address"));
    return -1;
  }
  safe_copyto(p, val, pParseCtxt->pSdp->c.iphost, sizeof(pParseCtxt->pSdp->c.iphost));
  if(pParseCtxt->pSdp->c.iphost[0] == '\0') {
    LOG(X_ERROR("SDP contains invalid connect address '%s'"), pParseCtxt->pSdp->c.iphost); 
    return -1;
  }

  //
  // consume optional TTL
  //
  if(!(p = nextval(&val,'/')) || (ptrdiff_t) (val - p) <= 0) {
    return rc;
  }
  safe_copyto(p, val, buf, sizeof(buf));
  pParseCtxt->pSdp->c.ttl = atoi(buf);

  //
  // consume optional address count
  //
  if(!(p = nextval(&val,'/')) || (ptrdiff_t) (val - p) <= 0) {
    return rc;
  }
  safe_copyto(p, val, buf, sizeof(buf));
  pParseCtxt->pSdp->c.numaddr = atoi(buf);

  return rc;
}

static int sdp_parse_media(SDP_PARSE_CTXT_T *pParseCtxt, const char *val) {
  int rc = 0;
  char buf[64];
  const char *p;
  const char *line = val;
  int i = 0;
  SDP_STREAM_DESCR_T *pCommon;

  pParseCtxt->lastFmt = -1;
  pParseCtxt->lastMediaType = SDP_MEDIA_TYPE_UNKNOWN;

  //
  // consume media type  
  //
  p = nextval(&val, 0);
  if(!strncmp(p, "video", 5)) {
    pCommon = &pParseCtxt->pSdp->vid.common;
    pParseCtxt->lastMediaType = SDP_MEDIA_TYPE_VIDEO;
  } else if(!strncmp(p, "audio", 5)) {
    pCommon = &pParseCtxt->pSdp->aud.common;
    pParseCtxt->lastMediaType = SDP_MEDIA_TYPE_AUDIO;
  } else {
    LOG(X_DEBUG("SDP ignoring media type in m=%s"), line);
    return 0;
  }

  //
  // consume port 
  //
  if(!(p = nextval(&val, 0)) || (ptrdiff_t)(val - p) <= 0) {
    LOG(X_ERROR("SDP missing media port in m=%s"), line);
    return -1;
  }
  safe_copyto(p, val, buf, sizeof(buf));
  i = atoi(buf);
  if(pParseCtxt->verify && (i < 0 || i > 0xffff)) {
    LOG(X_ERROR("SDP contains invalid media port in %s"), buf); 
    return -1;
  }
  if((pCommon->port = (uint16_t) i) == 0 && !pParseCtxt->allow_noport_rtsp) {
    LOG(X_DEBUG("Ignoring SDP m=%s"), line);
    return 0;
  }

  //
  // consume proto 
  //
  p = nextval(&val, 0);
  safe_copyto(p, val, buf, sizeof(buf));

  if(!strncmp(buf, SDP_TRANS_RTP_AVP_UDP, strlen(SDP_TRANS_RTP_AVP)+1)) {
    pCommon->transType = SDP_TRANS_TYPE_RTP_UDP;
  } else if(!strncmp(buf, SDP_TRANS_RTP_AVP_TCP, strlen(SDP_TRANS_RTP_AVP_TCP)+1)) {
    //pCommon->transType = SDP_TRANS_TYPE_RTP_TCP;
    LOG(X_ERROR("Not implemented SDP media proto in m=%s"), line);
    return -1;
  } else if(!strncmp(buf, SDP_TRANS_RTP_AVP, strlen(SDP_TRANS_RTP_AVP)+1)) {
    pCommon->transType = SDP_TRANS_TYPE_RTP_UDP;
  } else if(!strncmp(buf, SDP_TRANS_RTP_AVPF, strlen(SDP_TRANS_RTP_AVPF)+1)) {
    pCommon->transType = SDP_TRANS_TYPE_RTP_UDP;
    pCommon->avpf = 1;
  } else if(!strncmp(buf, SDP_TRANS_SRTP_AVP, strlen(SDP_TRANS_SRTP_AVP)+1)) {
    pCommon->transType = SDP_TRANS_TYPE_SRTP_SDES_UDP;
  } else if(!strncmp(buf, SDP_TRANS_SRTP_AVPF, strlen(SDP_TRANS_SRTP_AVPF)+1)) {
    pCommon->avpf = 1;
    pCommon->transType = SDP_TRANS_TYPE_SRTP_SDES_UDP;

  } else if(!strncmp(buf, SDP_TRANS_DTLS_AVP, strlen(SDP_TRANS_DTLS_AVP)+1)) {
    pCommon->transType = SDP_TRANS_TYPE_DTLS_UDP;
  } else if(!strncmp(buf, SDP_TRANS_DTLS_AVPF, strlen(SDP_TRANS_DTLS_AVPF)+1)) {
    pCommon->avpf = 1;
    pCommon->transType = SDP_TRANS_TYPE_DTLS_UDP;

  } else if(!strncmp(buf, SDP_TRANS_UDP_AVP, strlen(SDP_TRANS_UDP_AVP)+1)) {
    pCommon->transType = SDP_TRANS_TYPE_RAW_UDP;
  } else {
    LOG(X_ERROR("Unsupported SDP media proto %s in m=%s"), buf, line);
    return -1;
  }

  //
  // consume fmt (payload type)
  //
  if(!(p = nextval(&val, 0)) || (ptrdiff_t)(val - p) <= 0) {
    LOG(X_ERROR("SDP missing media fmt type %s ni m=%s"), val, line);
    return -1;
  }
  safe_copyto(p, val, buf, sizeof(buf));
  if((i = atoi(buf)) < 0 || i > 0x7f) {
    LOG(X_ERROR("SDP contains invalid media port (%d) in %s"), i, val); 
    return -1;
  } 
  pParseCtxt->lastFmt = i;
  pCommon->payloadType = (uint8_t) i;
  pCommon->available = 1;

  return rc;
}

static XC_CODEC_TYPE_T findCodecBySdpName(const char *name) {

  if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_H264, strlen(SDP_RTPMAP_ENCODINGNAME_H264))) {
    return XC_CODEC_TYPE_H264;
  } else if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_MPEG4V, strlen(SDP_RTPMAP_ENCODINGNAME_MPEG4V))) {
    return XC_CODEC_TYPE_MPEG4V;
  } else if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_H263_PLUS, strlen(SDP_RTPMAP_ENCODINGNAME_H263_PLUS))) {
    return XC_CODEC_TYPE_H263_PLUS;
  } else if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_H263, strlen(SDP_RTPMAP_ENCODINGNAME_H263))) {
    return XC_CODEC_TYPE_H263;
  } else if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_AAC, strlen(SDP_RTPMAP_ENCODINGNAME_AAC))) {
    return XC_CODEC_TYPE_AAC;
  } else if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_VP8, strlen(SDP_RTPMAP_ENCODINGNAME_VP8))) {
    return XC_CODEC_TYPE_VP8;
  } else if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_AMR, strlen(SDP_RTPMAP_ENCODINGNAME_AMR))) {
    return XC_CODEC_TYPE_AMRNB;
  } else if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_SILK, strlen(SDP_RTPMAP_ENCODINGNAME_SILK))) {
    return XC_CODEC_TYPE_SILK;
  } else if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_OPUS, strlen(SDP_RTPMAP_ENCODINGNAME_OPUS))) {
    return XC_CODEC_TYPE_OPUS;
  } else if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_MP2TS, strlen(SDP_RTPMAP_ENCODINGNAME_MP2TS))) {
    return (XC_CODEC_TYPE_T) MEDIA_FILE_TYPE_MP2TS;
  } else if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_PCMU, strlen(SDP_RTPMAP_ENCODINGNAME_PCMU))) {
    return  XC_CODEC_TYPE_G711_MULAW;
  } else if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_PCMA, strlen(SDP_RTPMAP_ENCODINGNAME_PCMA))) {
    return  XC_CODEC_TYPE_G711_ALAW;
  } else if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_TELEPHONE_EVENT, 
            strlen(SDP_RTPMAP_ENCODINGNAME_TELEPHONE_EVENT))) {
    return  XC_CODEC_TYPE_TELEPHONE_EVENT;
  } else if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_VID_CONFERENCE, 
            strlen(SDP_RTPMAP_ENCODINGNAME_VID_CONFERENCE))) {
    return XC_CODEC_TYPE_VID_CONFERENCE;
  } else if(!strncasecmp(name, SDP_RTPMAP_ENCODINGNAME_AUD_CONFERENCE, 
            strlen(SDP_RTPMAP_ENCODINGNAME_AUD_CONFERENCE))) {
    return XC_CODEC_TYPE_AUD_CONFERENCE;
  } else {
    return XC_CODEC_TYPE_UNKNOWN;
  }
}

static int sdp_parse_attrib_rtpmap(SDP_PARSE_CTXT_T *pParseCtxt, 
                                   const char *val, unsigned int fmtid) {
  int rc = 0;
  char buf[64];
  char encodingName[sizeof(pParseCtxt->pSdp->vid.common.encodingName)];
  const char *val0 = val;
  const char *p;
  XC_CODEC_TYPE_T codecType;
  unsigned int clockHz = 0;
  unsigned int channels = 1;

  //
  // consume the encoding name
  //
  if(!(p = nextval(&val, '/')) || (ptrdiff_t)(val - p) <= 0) {
    LOG(X_ERROR("SDP rtpmap:%d missing encoding name in %s"), fmtid, val0);
    return -1;
  }
  safe_copyto(p, val, encodingName, sizeof(encodingName));
  if((codecType = findCodecBySdpName(p))  == XC_CODEC_TYPE_UNKNOWN) {
    //if(pSdp->vid.common.payloadType == fmtid) {
    //  pSdp->vid.common.available = 0;
    //} else if(pSdp->aud.common.payloadType == fmtid) {
    //  pSdp->aud.common.available = 0;
    //}
    //LOG(X_WARNING("SDP contains unsupported rtpmap:%d encoding name %s"), fmtid, val0);
    //return 0;
    LOG(X_ERROR("SDP contains unsupported rtpmap:%d encoding name %s"), fmtid, val0);
    return -1;
  }

  //
  // consume the clock rate 
  //
  if(!(p = nextval(&val, '/')) || (ptrdiff_t)(val - p) <= 0) {
    LOG(X_ERROR("SDP rtpmap:%d missing clock rate in %s"), fmtid, val0);
    return -1;
  }
  safe_copyto(p, val, buf, sizeof(buf));
  clockHz = atoi(buf);

  if(codectype_isVid(codecType) || codecType == MEDIA_FILE_TYPE_MP2TS) {

    if(pParseCtxt->pSdp->vid.common.payloadType == 0 && 
       pParseCtxt->pSdp->vid.common.payloadType != fmtid) {
      pParseCtxt->pSdp->vid.common.payloadType = fmtid;
    }
    pParseCtxt->pSdp->vid.common.codecType = codecType;
    pParseCtxt->pSdp->vid.common.clockHz = clockHz;
    memcpy(pParseCtxt->pSdp->vid.common.encodingName, encodingName, sizeof(encodingName));

    //
    // Special codec type for dummy input conference driver
    //
    if(codecType == XC_CODEC_TYPE_VID_CONFERENCE) {
      pParseCtxt->pSdp->vid.common.xmitType = SDP_XMIT_TYPE_SENDONLY;
    }

  } else if(codectype_isAud(codecType)) {

    if(codecType == XC_CODEC_TYPE_TELEPHONE_EVENT) {

      pParseCtxt->pSdp->aud.aux.available = 1;
      pParseCtxt->pSdp->aud.aux.codecType = codecType;
      pParseCtxt->pSdp->aud.aux.payloadType = fmtid;
      memcpy(pParseCtxt->pSdp->aud.aux.encodingName, encodingName, sizeof(encodingName));

    } else {

      if(pParseCtxt->pSdp->aud.common.payloadType == 0 && 
         pParseCtxt->pSdp->aud.common.payloadType != fmtid) {
        pParseCtxt->pSdp->aud.common.payloadType = fmtid;
      }

      //
      // consume the audio channel count
      //
      if(!(p = nextval(&val, '/')) || (ptrdiff_t)(val - p) > 0) {
        safe_copyto(p, val, buf, sizeof(buf));
        channels = atoi(buf);
      }
      pParseCtxt->pSdp->aud.channels = channels;
      pParseCtxt->pSdp->aud.common.codecType = codecType;
      pParseCtxt->pSdp->aud.common.clockHz = clockHz;
      memcpy(pParseCtxt->pSdp->aud.common.encodingName, encodingName, sizeof(encodingName));

      //
      // Special codec type for dummy input conference driver
      //
      if(codecType == XC_CODEC_TYPE_AUD_CONFERENCE) {
        pParseCtxt->pSdp->aud.common.xmitType = SDP_XMIT_TYPE_SENDONLY;
      }

    }

  } else {
    LOG(X_ERROR("SDP rtpmap:%d contains unsupported codec in %s"), fmtid, val0);
  }

  return rc;
}

int sdp_parse_seqhdrs(XC_CODEC_TYPE_T codecType, const char *strhdrs, void *pArg) {
  int rc = 0;
  int sz;
  char *p;
  SDP_DESCR_AUD_T *pAud = NULL;
  SDP_DESCR_VID_T *pVid = NULL;

  if(!strhdrs || !pArg) {
    return -1;
  }

  switch(codecType) {
    case XC_CODEC_TYPE_H264:
      pVid = (SDP_DESCR_VID_T *) pArg;
      memset(&pVid->u.h264.spspps, 0, sizeof(pVid->u.h264.spspps));

      if((p = strchr(strhdrs, ','))) {
        *((char *) p) = '\0';
        p++;
      }
      if((sz = base64_decode(strhdrs, pVid->u.h264.spspps.sps_buf,
                          sizeof(pVid->u.h264.spspps.sps_buf))) > 0) {
        pVid->u.h264.spspps.sps_len = sz;
        pVid->u.h264.spspps.sps = pVid->u.h264.spspps.sps_buf;

      }
      if(p && (sz = base64_decode(p, pVid->u.h264.spspps.pps_buf,
                            sizeof(pVid->u.h264.spspps.pps_buf))) > 0) {
        pVid->u.h264.spspps.pps_len = sz;
        pVid->u.h264.spspps.pps = pVid->u.h264.spspps.pps_buf;
      }

      if(pVid->u.h264.spspps.sps_len == 0 || 
        h264_getNALType(pVid->u.h264.spspps.sps_buf[0]) != NAL_TYPE_SEQ_PARAM) {
        LOG(X_WARNING("SDP does not contain valid h264 sps '%s'"), strhdrs);
        rc = -1;
      }
      if(pVid->u.h264.spspps.pps_len == 0 ||
        h264_getNALType(pVid->u.h264.spspps.pps_buf[0]) != NAL_TYPE_PIC_PARAM) {
        LOG(X_WARNING("SDP does not contain valid h264 pps '%s'"), strhdrs);
        rc = -1;
      }

      break;

    case XC_CODEC_TYPE_MPEG4V:

      pVid = (SDP_DESCR_VID_T *) pArg;
      memset(&pVid->u.mpg4v.seqHdrs, 0, sizeof(pVid->u.mpg4v.seqHdrs));

      if((sz = hex_decode(strhdrs, pVid->u.mpg4v.seqHdrs.hdrsBuf,
                   sizeof(pVid->u.mpg4v.seqHdrs.hdrsBuf))) >= 0) {
        pVid->u.mpg4v.seqHdrs.hdrsLen = sz;
        //pVid->u.mpg4v.seqHdrs.pHdrs = pVid->u.mpg4v.seqHdrs.hdrsBuf;
      }
      if(pVid->u.mpg4v.seqHdrs.hdrsLen == 0) {
       LOG(X_WARNING("SDP mpeg4v contains invalid config '%s'"), strhdrs);
      }

      break;

    case XC_CODEC_TYPE_H263:
    case XC_CODEC_TYPE_H263_PLUS:
      break;

    case XC_CODEC_TYPE_AAC:
      pAud = (SDP_DESCR_AUD_T *) pArg;
      memset(&pAud->u.aac.config, 0, sizeof(pAud->u.aac.config));
      if((sz = hex_decode(strhdrs, pAud->u.aac.config, sizeof(pAud->u.aac.config))) <= 0 ||
        esds_decodeDecoderSp(pAud->u.aac.config, sz, &pAud->u.aac.decoderCfg) < 0) {
        LOG(X_ERROR("Failed to decode SDP aac decoder configuration '%s'"), strhdrs);
        rc = -1;
      }

      break;

    case XC_CODEC_TYPE_AMRNB:
    case XC_CODEC_TYPE_SILK:
    case XC_CODEC_TYPE_OPUS:
    case XC_CODEC_TYPE_G711_MULAW:
    case XC_CODEC_TYPE_G711_ALAW:

      break;

    default:
      LOG(X_ERROR("Invalid SDP sequence headers for %s (%d)"),
                 codecType_getCodecDescrStr(codecType), codecType);
      return -1;
  }

  return rc;
}

static int sdp_parse_cb_fmtp_param_h264(SDP_PARSE_CTXT_T *pParseCtxt, 
                                       const char *k, const char *v) {
  int rc = 0;
  
  if(!strncasecmp(k, SDP_ATTR_PACKETIZATION_MODE, strlen(SDP_ATTR_PACKETIZATION_MODE))) {
    pParseCtxt->pSdp->vid.u.h264.packetization_mode = atoi(v);
  } else if(!strncasecmp(k, SDP_ATTR_PROFILE_LEVEL_ID, strlen(SDP_ATTR_PROFILE_LEVEL_ID))) {
    if(strlen(v) >= 6) {
      hex_decode(v, pParseCtxt->pSdp->vid.u.h264.profile_level_id, 
                 sizeof(pParseCtxt->pSdp->vid.u.h264.profile_level_id));
    }
  } else if(!strncasecmp(k, SDP_ATTR_SPROP_PARAMETER_SETS, 
                         strlen(SDP_ATTR_SPROP_PARAMETER_SETS))) { 
    sdp_parse_seqhdrs(pParseCtxt->pSdp->vid.common.codecType, v, &pParseCtxt->pSdp->vid);
  }

  return rc;
}

static int sdp_parse_cb_fmtp_param_aac(SDP_PARSE_CTXT_T *pParseCtxt, 
                                       const char *k, const char *v) {
  int rc = 0;

  if(!strncasecmp(k, SDP_ATTR_PROFILE_LEVEL_ID, strlen(SDP_ATTR_PROFILE_LEVEL_ID))) { 
    pParseCtxt->pSdp->aud.u.aac.profile_level_id = atoi(v);
  } else if(!strncasecmp(k, SDP_ATTR_MODE, strlen(SDP_ATTR_MODE))) { 
    strncpy(pParseCtxt->pSdp->aud.u.aac.mode, v, sizeof(pParseCtxt->pSdp->aud.u.aac.mode));
  } else if(!strncasecmp(k, SDP_ATTR_SIZELENGTH, strlen(SDP_ATTR_SIZELENGTH))) {
    pParseCtxt->pSdp->aud.u.aac.sizelength  = atoi(v);
  } else if(!strncasecmp(k, SDP_ATTR_INDEXLENGTH, strlen(SDP_ATTR_INDEXLENGTH))) { 
    pParseCtxt->pSdp->aud.u.aac.indexlength = atoi(v);
  } else if(!strncasecmp(k, SDP_ATTR_INDEXDELTALENGTH, strlen(SDP_ATTR_INDEXDELTALENGTH))) { 
    pParseCtxt->pSdp->aud.u.aac.indexdeltalength = atoi(v);
  } else if(!strncasecmp(k, SDP_ATTR_CONFIG, strlen(SDP_ATTR_CONFIG))) {
    sdp_parse_seqhdrs(pParseCtxt->pSdp->aud.common.codecType, v, &pParseCtxt->pSdp->aud);
  }

  return rc;
}

static int sdp_parse_cb_fmtp_param_telephone_event(SDP_PARSE_CTXT_T *pParseCtxt, 
                                                   const char *k, const char *v) {
  int rc = 0;
  //TODO: parse string, such as '0-15'
  //fprintf(stderr, "PARSE TELEPHONE_EVENT k:'%s', v:'%s'\n", k, v);

  return rc;
}
static int sdp_parse_cb_fmtp_param_mpg4v(SDP_PARSE_CTXT_T *pParseCtxt, 
                                        const char *k, const char *v) {
  int rc = 0;

  //fprintf(stderr, "mpg4v '%s' -> '%s'\n", k, v);

  if(!strncasecmp(k, SDP_ATTR_PROFILE_LEVEL_ID, strlen(SDP_ATTR_PROFILE_LEVEL_ID))) { 
    pParseCtxt->pSdp->vid.u.mpg4v.profile_level_id = atoi(v);
  } else if(!strncasecmp(k, SDP_ATTR_CONFIG, strlen(SDP_ATTR_CONFIG))) { 
    sdp_parse_seqhdrs(pParseCtxt->pSdp->vid.common.codecType, v, &pParseCtxt->pSdp->vid);
  }
  
  return rc;
}

static int sdp_parse_cb_fmtp_param_amr(SDP_PARSE_CTXT_T *pParseCtxt, 
                                      const char *k, const char *v) {
  int rc = 0;
  int octet_align;
  
  //
  //Check for unsupported clock rate, such as 16000Hz AMR-WB
  //
  if(pParseCtxt->pSdp->aud.common.clockHz == 0) {
    pParseCtxt->pSdp->aud.common.clockHz = AMRNB_CLOCKHZ;
  } else if(pParseCtxt->pSdp->aud.common.clockHz != AMRNB_CLOCKHZ) {
    LOG(X_ERROR("SDP fmtp AMR clock %dHz not supported"), pParseCtxt->pSdp->aud.common.clockHz);
    return -1;
  }
  
  if(!strncasecmp(k, SDP_ATTR_OCTET_ALIGN, strlen(SDP_ATTR_OCTET_ALIGN))) { 
    if((octet_align = atoi(v)) != 1) {
      LOG(X_ERROR("SDP fmtp AMR octet-align=%d not supported"), octet_align);
      return -1;
    }
    pParseCtxt->pSdp->aud.u.amr.octet_align = octet_align;
  }
  return rc;
}

static int sdp_parse_cb_fmtp_param_g711(SDP_PARSE_CTXT_T *pParseCtxt, 
                                        const char *k, const char *v) {
  int rc = 0;
  
  //
  //Check for unsupported clock rate
  //
  if(pParseCtxt->pSdp->aud.common.clockHz == 0) {
    pParseCtxt->pSdp->aud.common.clockHz = 8000;
  } else if(pParseCtxt->pSdp->aud.common.clockHz != 8000) {
    LOG(X_ERROR("SDP fmtp PCM clock %dHz not supported"), pParseCtxt->pSdp->aud.common.clockHz);
    return -1;
  }
  
  return rc;
}

static int sdp_parse_fmtp_key_vals(SDP_PARSE_CTXT_T *pParseCtxt, const char *str, 
                              SDP_PARSE_CB_FMTP_PARAM cbOnFmtpParam) {
  int rc = 0;
  char key[128];
  char param[1024];
  const char *p;
  const char *ps, *ps2;

  //
  // Iterate through each custom parameter
  //
  // note: nextval will always return a valid pointer
  while(!(p = nextval(&str, ';')) || (ptrdiff_t)(str - p) >  0) {

    param[0] = '\0';
    ps = ps2 = p;
    ps = nextval(&ps2, '=');
    if(ps2 > str) {
      ps2 = str;
    }
    if((ptrdiff_t)(ps2 - ps) <=  0) {
      continue;
    }
    safe_copyto(ps, ps2, key, sizeof(key));

    if(ps2 < str) {
      ps2++;
      ps = ps2;
      ps = nextval(&ps2, ';');
      if((ptrdiff_t)(ps2 - ps) >  0) {
        safe_copyto(ps, ps2, param, sizeof(param));
      }
    }

    //fprintf(stderr, "sdp attrib '%s' -> '%s'  %c%c%c\n", key, param, val[0], val[1], val[2]);
    if((rc = cbOnFmtpParam(pParseCtxt, key, param)) < 0) {
      return rc;
    }
  }

  return rc;
}

static int sdp_parse_attrib_fmtp(SDP_PARSE_CTXT_T *pParseCtxt, const char *val, 
                                 unsigned int fmtid) {
  int rc = 0;
  SDP_PARSE_CB_FMTP_PARAM cbOnFmtpParam = NULL;

  //TODO: since this is parsing sequentially, rtpmap codec definition must precede fmtp

  if(pParseCtxt->pSdp->vid.common.payloadType == fmtid) {

    switch(pParseCtxt->pSdp->vid.common.codecType) {
      case XC_CODEC_TYPE_H264:
        cbOnFmtpParam = sdp_parse_cb_fmtp_param_h264;
        break;
      case XC_CODEC_TYPE_MPEG4V:
        cbOnFmtpParam = sdp_parse_cb_fmtp_param_mpg4v;
        break;
      case MEDIA_FILE_TYPE_MP2TS:
        return 0;
      default:
        break;
    }

  } else if(pParseCtxt->pSdp->aud.common.payloadType == fmtid) {

    switch(pParseCtxt->pSdp->aud.common.codecType) {
      case XC_CODEC_TYPE_AAC:
        cbOnFmtpParam = sdp_parse_cb_fmtp_param_aac;
        break;
      case XC_CODEC_TYPE_AMRNB:
        cbOnFmtpParam = sdp_parse_cb_fmtp_param_amr;
        break;
      case XC_CODEC_TYPE_G711_MULAW:
      case XC_CODEC_TYPE_G711_ALAW:
        cbOnFmtpParam = sdp_parse_cb_fmtp_param_g711;
        break;
      default:
        break;
    }

  } else if(pParseCtxt->pSdp->aud.aux.available && pParseCtxt->pSdp->aud.aux.payloadType == fmtid) {
    switch(pParseCtxt->pSdp->aud.aux.codecType) {
      case XC_CODEC_TYPE_TELEPHONE_EVENT:
        cbOnFmtpParam = sdp_parse_cb_fmtp_param_telephone_event;
      default:
        break;
    }
  }

  if(rc != 0) {
    LOG(X_WARNING("SDP contains unrecognized fmtype %d in %s"), fmtid, val);
    return rc;
  } else if(!cbOnFmtpParam) {
    return rc;
  }

  rc = sdp_parse_fmtp_key_vals(pParseCtxt, val, cbOnFmtpParam);

  return rc;
}

static int sdp_parse_cb_fmtp_param_control(SDP_PARSE_CTXT_T *pParseCtxt, 
                                           const char *k, const char *v) {
  int rc = 0;
  SDP_STREAM_DESCR_T *pDescr = NULL;

  if(pParseCtxt->lastFmt > 0) {
    if(pParseCtxt->pSdp->vid.common.payloadType == pParseCtxt->lastFmt) {
      pDescr = &pParseCtxt->pSdp->vid.common;
    } else if(pParseCtxt->pSdp->aud.common.payloadType == pParseCtxt->lastFmt) {
      pDescr = &pParseCtxt->pSdp->aud.common;
    }
  }
    
  if(!pDescr) {
    LOG(X_WARNING("SDP control attribute %s=%s references unknown media section"), k, v);
    return 0;
  }

  //fprintf(stderr, "CONTROL fmt:%d '%s' -> '%s'\n", pParseCtxt->lastFmt, k, v);

  pDescr->controlId = atoi(v);

  return rc;
}


static int sdp_parse_attrib_control(SDP_PARSE_CTXT_T *pParseCtxt, const char *val, 
                                    const char *trackId) {
  int rc = 0;

  //fprintf(stderr, "control extra options: '%s'\n", val);

  rc = sdp_parse_fmtp_key_vals(pParseCtxt, trackId, sdp_parse_cb_fmtp_param_control);

  return rc;
}

static DTLS_FINGERPRINT_TYPE_T sdp_dtls_get_fingerprint_type(const char *str) {

  if(!str) {
    return DTLS_FINGERPRINT_TYPE_UNKNOWN;
  } else if(!strncasecmp(str, "sha-256", 7)) {
    return DTLS_FINGERPRINT_TYPE_SHA256;
  } else if(!strncasecmp(str, "sha-1", 5)) {
    return DTLS_FINGERPRINT_TYPE_SHA1;
  } else if(!strncasecmp(str, "sha-512", 7)) {
    return DTLS_FINGERPRINT_TYPE_SHA512;
  } else if(!strncasecmp(str, "sha-224", 7)) {
    return DTLS_FINGERPRINT_TYPE_SHA224;
  } else if(!strncasecmp(str, "sha-384", 7)) {
    return DTLS_FINGERPRINT_TYPE_SHA384;
  } else if(!strncasecmp(str, "md5", 3)) {
    return DTLS_FINGERPRINT_TYPE_MD5;
  } else if(!strncasecmp(str, "md2", 3)) {
    return DTLS_FINGERPRINT_TYPE_MD2;
  } else {
    return DTLS_FINGERPRINT_TYPE_UNKNOWN;
  }

}

char *sdp_dtls_get_fingerprint_typestr(DTLS_FINGERPRINT_TYPE_T type) {

  switch(type) {
    case DTLS_FINGERPRINT_TYPE_SHA256:
      return "sha-256";
    case DTLS_FINGERPRINT_TYPE_SHA512:
      return "sha-512";
    case DTLS_FINGERPRINT_TYPE_SHA1:
      return "sha-1";
    case DTLS_FINGERPRINT_TYPE_SHA224:
      return "sha-224";
    case DTLS_FINGERPRINT_TYPE_SHA384:
      return "sha-384";
    case DTLS_FINGERPRINT_TYPE_MD5:
      return "md5";
    case DTLS_FINGERPRINT_TYPE_MD2:
      return "md2";
    default:
    return "";
  }

}

static int sdp_parse_attrib_fingerprint(SDP_PARSE_CTXT_T *pParseCtxt, const char *val, const char *tag) {
  int rc = 0;
  DTLS_FINGERPRINT_T *pFingerprint = NULL;
  DTLS_FINGERPRINT_T fingerprint;
  int master_fingerprint = 0;

  //
  // a=fingerprint:sha-256 13:8D:95:C2:26:6C:03:02:50:16:84:9D:5D:1D:D5:08:89:20:62:9B:EF:0F:CA:62:9E:37:7C:F0:D5:DC:9B:7F
  //

  if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_VIDEO) {
    pFingerprint = &pParseCtxt->pSdp->vid.common.fingerprint;
  } else if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_AUDIO) {
    pFingerprint = &pParseCtxt->pSdp->aud.common.fingerprint;
  } else {
    memset(&fingerprint, 0, sizeof(fingerprint));
    pFingerprint = &fingerprint;
    master_fingerprint = 1;
  }

  pFingerprint->type = sdp_dtls_get_fingerprint_type(tag);

  if((rc = dtls_str2fingerprint(val, pFingerprint)) < 0) {
    LOG(X_ERROR("SDP invalid a=fingerprint:%s %s "), tag ? tag : "", val);
    return rc;
  }

  if(master_fingerprint) {
    memcpy(&pParseCtxt->pSdp->vid.common.fingerprint, &fingerprint, 
           sizeof(pParseCtxt->pSdp->vid.common.fingerprint));
    memcpy(&pParseCtxt->pSdp->aud.common.fingerprint, &fingerprint, 
           sizeof(pParseCtxt->pSdp->aud.common.fingerprint));
  }
  
  //LOG(X_DEBUG("FINGERPRINT val:'%s', tag:'%s', type:%d"), val, tag, pFingerprint->type);
  //LOGHEX_DEBUG(pFingerprint->buf, pFingerprint->len);

  return rc;
}

static int sdp_parse_attrib_crypto(SDP_PARSE_CTXT_T *pParseCtxt, const char *val, const char *tag) {
  int rc = 0;
  SDP_STREAM_SRTP_T *pSrtp = NULL;
  SDP_STREAM_SRTP_T masterSrtp;
  int master_srtp = 0;
  char algo[1024];
  const char *p;
  const char *line = val;
  const char *key0 = NULL, *key1 = NULL;

  //
  // http://tools.ietf.org/html/rfc4568
  //
  // a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:m8BM9wJXNsOToyQVz3bSNZ32iF5yhxvDOJ8J9sG+|2^20|1:4 UNENCRYPTED_SRTP
  //

  //
  // consume crypto-suite 
  //
  p = nextval(&val, 0);
  if((ptrdiff_t)(val - p) <= 0) {
    LOG(X_ERROR("SDP missing crypto-suite in a=crypto:%s %s "), tag ? tag : "", line);
    return -1;
  }
  safe_copyto(p, val, algo, sizeof(algo));

  //
  // consume key-param
  //
  p = nextval(&val, 0);
  if((ptrdiff_t)(val - p) <= 0) {
    LOG(X_ERROR("SDP missing key-param in a=crypto:%s %s "), tag ? tag : "",line);
    return -1;
  } else if(strncasecmp(p, "inline:", 7)) {
    LOG(X_ERROR("SDP contains invalid key-param in a=crypto:%s %s "), tag ? tag : "", line);
    return -1;
  }
  key1 = key0 = p + 7;
  while(*key1 != '|' && *key1 != ' ' && *key1 != '\0') {
    key1++;
  }

  //
  // consume session-parameters
  //
  p = nextval(&val, 0);
  if((ptrdiff_t)(val - p) > 0) {

  }

  //fprintf(stderr, "RTCP SDP line: '%s' '%s' lastFmt:%d, lastMediaType:%d\n", val, line, pParseCtxt->lastFmt, pParseCtxt->lastMediaType);

  if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_VIDEO) {
    pSrtp = &pParseCtxt->pSdp->vid.common.srtp;
  } else if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_AUDIO) {
    pSrtp = &pParseCtxt->pSdp->aud.common.srtp;
  } else {
    return 0;
    memset(&masterSrtp, 0, sizeof(masterSrtp));
    pSrtp = &masterSrtp;
    master_srtp = 1;
  }

  if(pSrtp->tag[0] != '\0') {
    LOG(X_WARNING("SDP ignoring a=crypto:%s %s "), tag ? tag : "", line);
    return 0;
  }

  //fprintf(stderr, "crypto tag:'%s', val:'%s', p:'%s', algo:'%s', key_param:'%s'\n", tag, val, p, algo, keybuf);

  if((rc = srtp_streamType(algo, &pSrtp->type.authType, &pSrtp->type.confType)) < 0) {
    LOG(X_ERROR("Invalid or unsupported crypto-suite '%s'"), algo);
    return rc;
  } else if(key1 - key0 >= sizeof(pSrtp->keyBase64)) {
    LOG(X_ERROR("Base64 Key length exceeds %d"), sizeof(pSrtp->keyBase64));
    return -1;
  }

  safe_copyto(key0, key1, pSrtp->keyBase64, sizeof(pSrtp->keyBase64));
  pSrtp->type.authTypeRtcp = pSrtp->type.authType;
  pSrtp->type.confTypeRtcp = pSrtp->type.confType;
  strncpy(pSrtp->tag, tag, sizeof(pSrtp->tag) - 1);

  if(master_srtp) {
    memcpy(&pParseCtxt->pSdp->vid.common.srtp, &masterSrtp, sizeof(pParseCtxt->pSdp->vid.common.srtp));
    memcpy(&pParseCtxt->pSdp->aud.common.srtp, &masterSrtp, sizeof(pParseCtxt->pSdp->aud.common.srtp));
  }

  //fprintf(stderr, "KEY:'%s', auth:%d, conf:%d, tag:'%s'\n", pSrtp->keyBase64, pSrtp->authType, pSrtp->confType, pSrtp->tag);

  return rc;
}

static int sdp_parse_attrib_rtcp(SDP_PARSE_CTXT_T *pParseCtxt, const char *val, int port) {
  int rc = 0;
  SDP_STREAM_DESCR_T *pDescr = NULL;

  //fprintf(stderr, "RTCP SDP line: '%s' port:%d lastFmt:%d, lastMediaType:%d\n", val, port, pParseCtxt->lastFmt, pParseCtxt->lastMediaType);

  if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_VIDEO) {
    pDescr = &pParseCtxt->pSdp->vid.common;
  } else if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_AUDIO) {
    pDescr = &pParseCtxt->pSdp->aud.common;
  } else {
    return 0;
  }

  if(port == -1) {
    pDescr->rtcpmux = 1;
    pDescr->portRtcp = pDescr->port;
  } else {
    pDescr->portRtcp = (uint16_t) port;
  }

  return rc;
}

static int rtcpfb_parse_param(const char *param, int type, SDP_STREAM_RTCPFB_T *pRtpFb) {

  switch(type) {

    case SDP_RTCPFB_TYPE_ACK:

      if(!strcasecmp("rpsi", param)) {
        pRtpFb->flags |= (SDP_RTCPFB_TYPE_ACK_RPSI | SDP_RTCPFB_TYPE_ACK);
      } else if(!strcasecmp("app", param)) {
        pRtpFb->flags |= (SDP_RTCPFB_TYPE_ACK_APP | SDP_RTCPFB_TYPE_ACK);
      } else {
        return -1; 
      }
      break;

    case SDP_RTCPFB_TYPE_NACK:

      if(param[0] == '\0') {
        pRtpFb->flags |= (SDP_RTCPFB_TYPE_NACK_GENERIC | SDP_RTCPFB_TYPE_NACK);
      } else if(!strcasecmp("pli", param)) {
        pRtpFb->flags |= (SDP_RTCPFB_TYPE_NACK_PLI | SDP_RTCPFB_TYPE_NACK);
      } else if(!strcasecmp("sli", param)) {
        pRtpFb->flags |= (SDP_RTCPFB_TYPE_NACK_SLI | SDP_RTCPFB_TYPE_NACK);
      } else if(!strcasecmp("rpsi", param)) {
        pRtpFb->flags |= (SDP_RTCPFB_TYPE_NACK_RPSI | SDP_RTCPFB_TYPE_NACK);
      } else if(!strcasecmp("app", param)) {
        pRtpFb->flags |= (SDP_RTCPFB_TYPE_NACK_APP | SDP_RTCPFB_TYPE_NACK);
      } else {
        return -1; 
      }
      break;

    case SDP_RTCPFB_TYPE_TRRINT:
      pRtpFb->trrIntervalMs = atoi(param); 
      break;

    case SDP_RTCPFB_TYPE_CCM:

      if(!strcasecmp("fir", param)) {
        pRtpFb->flags |= (SDP_RTCPFB_TYPE_CCM_FIR | SDP_RTCPFB_TYPE_CCM);
      } else if(!strcasecmp("tmmbr", param)) {
        pRtpFb->flags |= (SDP_RTCPFB_TYPE_CCM_TMMBR | SDP_RTCPFB_TYPE_CCM);
      } else if(!strcasecmp("tstr", param)) {
        pRtpFb->flags |= (SDP_RTCPFB_TYPE_CCM_TSTR | SDP_RTCPFB_TYPE_CCM);
      } else if(!strcasecmp("vbcm", param)) {
        pRtpFb->flags |= (SDP_RTCPFB_TYPE_CCM_VBCM | SDP_RTCPFB_TYPE_CCM);
      } else {
        return -1; 
      }
      break;
    
    default:
      return -1;
  }

  return 0;
}

static int sdp_parse_attrib_rtcpfb(SDP_PARSE_CTXT_T *pParseCtxt, const char *val0, const char *fmt) {
  int rc = 0;
  unsigned int idx = 0;
  int type = 0;
  char typebuf[16];
  char tmp[32];
  const char *val = val0;
  const char *p;
  SDP_STREAM_DESCR_T *pDescr = NULL;

  if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_VIDEO) {
    pDescr = &pParseCtxt->pSdp->vid.common;
  } else if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_AUDIO) {
    pDescr = &pParseCtxt->pSdp->aud.common;
  } else {
    return 0;
  }

  //
  // parse 'a=rtcp-fb: ...' feedback parameters according RFC 4585, RFC 5104
  // http://tools.ietf.org/html/rfc4585
  // http://tools.ietf.org/html/rfc5104
  //

  //
  // fmt can be either '*' or fmtid
  //
  if(!fmt || fmt[0] == '*' || fmt[0] == '\0') {
    pDescr->rtcpfb.fmtidmin1 = 0;
  } else {
    pDescr->rtcpfb.fmtidmin1 = atoi(fmt) + 1;
  }

  //
  // consume ack|nack|ccm
  //
  p = nextval(&val, 0);
  if((ptrdiff_t)(val - p) <= 0) {
    LOG(X_WARNING("SDP missing type in a=rtcp-fb:%s%s"), fmt ? fmt : "", val0);
    return 0;
  }
  safe_copyto(p, val, typebuf, sizeof(typebuf));
  if(!strcasecmp("nack", typebuf)) {
    type = SDP_RTCPFB_TYPE_NACK;
  } else if(!strcasecmp("ack", typebuf)) {
    type = SDP_RTCPFB_TYPE_ACK;
  } else if(!strcasecmp("ccm", typebuf)) {
    type = SDP_RTCPFB_TYPE_CCM;
  } else if(!strcasecmp("trr-int", typebuf)) {
    type = SDP_RTCPFB_TYPE_TRRINT;
  } else if(!strcasecmp("goog-remb", typebuf) || !strcasecmp("remb", typebuf)) {
    type = 0;
    pDescr->rtcpfb.flags |= (SDP_RTCPFB_TYPE_APP_REMB);
  } else {
    LOG(X_WARNING("SDP contains unknown type '%s' in a=rtcp-fb:%s%s"), typebuf, fmt ? fmt : "", val0);
    return 0;
  }
  
  //
  // consume one or more feedback params
  //
  do {
    p = nextval(&val, 0);
    if((ptrdiff_t)(val - p) <= 0) {
       if(idx == 0 && type == SDP_RTCPFB_TYPE_NACK) {
         if(rtcpfb_parse_param("", type, &pDescr->rtcpfb) == -1) {
           LOG(X_WARNING("SDP contains unknown param'%s' in a=rtcp-fb:%s%s"), tmp, fmt ? fmt : "", val0);
           break;
         }
       }
       break;
    }
    safe_copyto(p, val, tmp, sizeof(tmp));

    if(rtcpfb_parse_param(tmp, type, &pDescr->rtcpfb) == -1) {
      LOG(X_WARNING("SDP contains unknown param'%s' in a=rtcp-fb:%s%s"), tmp, fmt ? fmt : "", val0);
      break;
    }

    idx++;
  } while(p);
  
  //fprintf(stderr, "RTCP_FB fmt:'%s', val0:'%s', type:'%s', p:'%s', val:'%s'\n", pDescr->rtcpfb.fmtid, val0, typebuf, p, val);

  return rc;
}

static int sdp_parse_attrib_iceufrag(SDP_PARSE_CTXT_T *pParseCtxt, const char *val) {
  size_t sz;
  SDP_STREAM_DESCR_T *pDescr = NULL;

  if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_VIDEO) {
    pDescr = &pParseCtxt->pSdp->vid.common;
  } else if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_AUDIO) {
    pDescr = &pParseCtxt->pSdp->aud.common;
  } else {
    return 0;
  }

  if((sz = strlen(val)) + 1 >= sizeof(pDescr->ice.ufrag)) {
    LOG(X_WARNING("Truncating ice-ufrag attribute '%s' to %d chars"), val, sz); 
  }
  safe_copyto(val, val + sz, pDescr->ice.ufrag, sizeof(pDescr->ice.ufrag));

  return 0;
}

static int sdp_parse_attrib_icepwd(SDP_PARSE_CTXT_T *pParseCtxt, const char *val) {
  size_t sz;
  SDP_STREAM_DESCR_T *pDescr = NULL;

  if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_VIDEO) {
    pDescr = &pParseCtxt->pSdp->vid.common;
  } else if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_AUDIO) {
    pDescr = &pParseCtxt->pSdp->aud.common;
  } else {
    return 0;
  }

  if((sz = strlen(val)) + 1 >= sizeof(pDescr->ice.pwd)) {
    LOG(X_WARNING("Truncating ice-pwd attribute '%s' to %d chars"), val, sz); 
  }
  safe_copyto(val, val + sz, pDescr->ice.pwd, sizeof(pDescr->ice.pwd));

  return 0;
}

static const char *ice_candidate_transport2str(SDP_ICE_TRANS_TYPE_T transport) {
  switch(transport) {
    case SDP_ICE_TRANS_TYPE_UDP:
      return SDP_ICE_TRANS_STR_UDP;
    case SDP_ICE_TRANS_TYPE_TCP:
      return SDP_ICE_TRANS_STR_TCP;
    default:
      return "";
  }
}

static const char *ice_candidate_type2str(SDP_ICE_TYPE_T type) {
  switch(type) {
    case SDP_ICE_TYPE_PRFLX:
      return SDP_ICE_STR_PRFFLX;
    case SDP_ICE_TYPE_SRFLX:
      return SDP_ICE_STR_SRFFLX;
    case SDP_ICE_TYPE_HOST:
      return SDP_ICE_STR_HOST;
    case SDP_ICE_TYPE_RELAY:
      return SDP_ICE_STR_RELAY;
    default:
      return "";
  }
}
    

static int sdp_parse_attrib_candidate(SDP_PARSE_CTXT_T *pParseCtxt, const char *val0, const char *fmt) {
  int rc = 0;
  char buf[64];
  int id;
  const char *p;
  const char *val = val0;
  SDP_STREAM_DESCR_T *pDescr = NULL;
  SDP_CANDIDATE_T *pCandidate = NULL;

  if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_VIDEO) {
    pDescr = &pParseCtxt->pSdp->vid.common;
  } else if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_AUDIO) {
    pDescr = &pParseCtxt->pSdp->aud.common;
  } else {
    return 0;
  }

  //
  //             foundation  (component-)id transport    priority   address       port      type attr<br>
  // a=candidate:2530088836  1              udp          2113937151 192.168.1.100 56382 typ host generation 0
  //
  // a=candidate:1983091787 2 udp 33562367 10.10.164.177 54023 typ relay raddr 10.10.164.177 rport 52502 generation 0
  //

  if(!fmt || fmt[0] == '\0') {
    LOG(X_ERROR("SDP contains missing foundation in candidate line: %s"),  val); 
    return -1;
  }

  //
  // Move pasat foundation to start of component
  //
  p = nextval(&val, 0);
  if((ptrdiff_t)(val - p) <= 0) {
    LOG(X_ERROR("SDP contains missing component in candidate line: %s"),  val); 
    return -1;
  }

  safe_copyto(p, val, buf, sizeof(buf));
  if((id = atoi(buf)) == 1) {
    pCandidate = &pDescr->iceCandidates[0]; // RTP
  } else if(id == 2) {
    pCandidate = &pDescr->iceCandidates[1]; // RTCP
  } else {
    LOG(X_ERROR("SDP contains invalid component id:%d in candidate line: %s"),  id, val); 
    return -1;
  }

  memset(pCandidate, 0, sizeof(SDP_CANDIDATE_T));
  pCandidate->component = atoi(buf);
  pCandidate->foundation = atol(fmt);

  //
  // Move to transport type
  //
  p = nextval(&val, 0);
  if((ptrdiff_t)(val - p) <= 0) {
    LOG(X_ERROR("SDP contains missing transport type in candidate line: %s"),  val); 
    return -1;
  }

  safe_copyto(p, val, buf, sizeof(buf));
  if(!strncasecmp(buf, SDP_ICE_TRANS_STR_UDP, strlen(SDP_ICE_TRANS_STR_UDP))) {
    pCandidate->transport = SDP_ICE_TRANS_TYPE_UDP;
  } else if(!strncasecmp(buf, SDP_ICE_TRANS_STR_TCP, strlen(SDP_ICE_TRANS_STR_TCP))) {
    pCandidate->transport = SDP_ICE_TRANS_TYPE_TCP;
  } else {
    LOG(X_ERROR("SDP contains invalid transport type '%s' in candidate line : %s"),  buf, val); 
    return -1;
  }

  //
  // Move to priority
  //
  p = nextval(&val, 0);
  if((ptrdiff_t)(val - p) <= 0) {
    LOG(X_ERROR("SDP contains missing priority in candidate line: %s"),  val); 
    return -1;
  }
  safe_copyto(p, val, buf, sizeof(buf));
  pCandidate->priority = atoi(buf);

  //
  // Move to IP address
  //
  p = nextval(&val, 0);
  if((ptrdiff_t)(val - p) <= 0) {
    LOG(X_ERROR("SDP contains missing address in candidate line: %s"),  val); 
    return -1;
  }
  safe_copyto(p, val, buf, sizeof(buf));
  pCandidate->address.sin_addr.s_addr = inet_addr(buf);

  //
  // Move to port
  //
  p = nextval(&val, 0);
  if((ptrdiff_t)(val - p) <= 0) {
    LOG(X_ERROR("SDP contains missing port in candidate line: %s"),  val); 
    return -1;
  }
  safe_copyto(p, val, buf, sizeof(buf));
  pCandidate->address.sin_port = htons(atoi(buf));

  //
  // consume 'typ'
  //
  p = nextval(&val, 0);

  //
  // Move to candidate type 'relay'
  //
  p = nextval(&val, 0);
  if((ptrdiff_t)(val - p) <= 0) {
    LOG(X_ERROR("SDP contains missing address in candidate line: %s"),  val); 
    return -1;
  }

  safe_copyto(p, val, buf, sizeof(buf));
  if(!strncasecmp(buf, SDP_ICE_STR_PRFFLX, strlen(SDP_ICE_STR_PRFFLX))) {
    pCandidate->type = SDP_ICE_TYPE_PRFLX;
  } else if(!strncasecmp(buf, SDP_ICE_STR_SRFFLX, strlen(SDP_ICE_STR_SRFFLX))) {
    pCandidate->type = SDP_ICE_TYPE_SRFLX;
  } else if(!strncasecmp(buf, SDP_ICE_STR_HOST, strlen(SDP_ICE_STR_HOST))) {
    pCandidate->type = SDP_ICE_TYPE_HOST;
  } else if(!strncasecmp(buf, SDP_ICE_STR_RELAY, strlen(SDP_ICE_STR_RELAY))) {
    pCandidate->type = SDP_ICE_TYPE_RELAY;
  } else {
    LOG(X_ERROR("SDP contains invalid type '%s' in candidate line : %s"),  buf, val);
    return -1;
  }

  //
  // Move to next field, such as 'raddr'
  //
  p = nextval(&val, 0);
  if((ptrdiff_t)(val - p) > 0) {
    safe_copyto(p, val, buf, sizeof(buf));

    //
    // Parse raddr
    //
    if(!strncmp(buf, "raddr", sizeof(buf))) {

      //
      // Move to raddr IP address
      //
      p = nextval(&val, 0);
      if((ptrdiff_t)(val - p) <= 0) {
        LOG(X_ERROR("SDP contains missing raddr address value in candidate line : %s"),  val);
        return -1;
      }
      safe_copyto(p, val, buf, sizeof(buf));
      pCandidate->raddress.sin_addr.s_addr = inet_addr(buf);

      //
      // Move to rport 
      //
      p = nextval(&val, 0);
      if((ptrdiff_t)(val - p) > 0) {
        safe_copyto(p, val, buf, sizeof(buf));

        if(!strncmp(buf, "rport", sizeof(buf))) {

          //
          // Move to rport port value
          //
          p = nextval(&val, 0);
          if((ptrdiff_t)(val - p) <= 0) {
            LOG(X_ERROR("SDP contains missing rport value in candidate line : %s"),  val);
            return -1;
          }
          safe_copyto(p, val, buf, sizeof(buf));
          pCandidate->raddress.sin_port = htons(atoi(buf));
        }

      }

      p = nextval(&val, 0);

    //LOG(X_DEBUG("RADDR:%s:%d"), inet_ntoa(pCandidate->raddress.sin_addr), htons(pCandidate->raddress.sin_port));
  //pCandidate->address.sin_addr.s_addr = inet_addr(buf);
    }
  }

  //val++;
  //p2 = nextval(&val, 0);
  //if((ptrdiff_t)(val - p2) > 0) {
  //  safe_copyto(p2, val, buf, sizeof(buf));
  //}
  
  //safe_copyto(p, val, typebuf, sizeof(typebuf));

  //LOG(X_DEBUG("foundation:%ld, type:%d, BUF:'%s', port:%d"), pCandidate->foundation, pCandidate->type, buf, htons(pCandidate->address.sin_port));
  //LOG(X_DEBUG("val0:'%s', p:'%s', val:'%s'"), val0, p, val);

  return rc;
}

static int sdp_parse_attrib_sendrecv(SDP_PARSE_CTXT_T *pParseCtxt, SDP_XMIT_TYPE_T xmitType) {
  int rc = 0;
  SDP_STREAM_DESCR_T *pDescr = NULL;

  if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_VIDEO) {
    pDescr = &pParseCtxt->pSdp->vid.common;
  } else if(pParseCtxt->lastMediaType == SDP_MEDIA_TYPE_AUDIO) {
    pDescr = &pParseCtxt->pSdp->aud.common;
  } else {
    return 0;
  }

  pDescr->xmitType = xmitType;

  return rc;
}

static int sdp_parse_attrib(SDP_PARSE_CTXT_T *pParseCtxt, const char *val0) {
  int rc = 0;
  char fmtidbuf[256];
  const char *p, *p2;
  const char *val = val0;
  int haveFmt = 0;
  unsigned int fmtid = 0;

  //
  // consume atribute type  
  //
  p = nextval(&val, ':');

  if((ptrdiff_t)(val - p) <= 0) {
    LOG(X_ERROR("SDP missing attributes in line %s"), val);
    return -1;
  }

  //
  // consume fmt id
  //
  val++;
  p2 = nextval(&val, 0);
  if((ptrdiff_t)(val - p2) > 0) {
    safe_copyto(p2, val, fmtidbuf, sizeof(fmtidbuf));
    haveFmt = 1;
  }

  if(!strncmp(p, "rtpmap", 6)) {

    if(!haveFmt) {
      LOG(X_ERROR("SDP a=rtpmap missing fmt id %s"), val);
      return -1;
    }
    fmtid = atoi(fmtidbuf);
    return sdp_parse_attrib_rtpmap(pParseCtxt, val, fmtid);

  } else if(!strncmp(p, "fmtp", 4)) {

    if(!haveFmt) {
      LOG(X_ERROR("SDP a=fmtp missing fmt id %s"), val);
      return -1;
    }
    fmtid = atoi(fmtidbuf);
    return sdp_parse_attrib_fmtp(pParseCtxt, val, fmtid);

  } else if(!strncmp(p, "control", 7)) {

    if(!haveFmt) {
      LOG(X_ERROR("SDP a=control missing fmt id %s"), val);
      return -1;
    }
    return sdp_parse_attrib_control(pParseCtxt, val, fmtidbuf);

  } else if(!strncmp(p, "crypto", 6)) {

    return sdp_parse_attrib_crypto(pParseCtxt, val, haveFmt ? fmtidbuf : NULL);

  } else if(!strncmp(p, "fingerprint", 11)) {

    return sdp_parse_attrib_fingerprint(pParseCtxt, val, haveFmt ? fmtidbuf : NULL);

  } else if(!strncmp(p, "rtcp-fb", 7)) {

    return sdp_parse_attrib_rtcpfb(pParseCtxt, val, fmtidbuf);

  } else if(!strncmp(p, "rtcp-mux", 8)) {

    return sdp_parse_attrib_rtcp(pParseCtxt, val, -1);

  } else if(!strncmp(p, "rtcp", 4)) {

    if(!haveFmt) {
      LOG(X_ERROR("SDP a=rtcp missing port %s"), val);
      return -1;
    }
    fmtid = atoi(fmtidbuf);
    return sdp_parse_attrib_rtcp(pParseCtxt, val, fmtid);

  } else if(!strncmp(p, "ice-ufrag", 9)) {
    return sdp_parse_attrib_iceufrag(pParseCtxt, fmtidbuf);
  } else if(!strncmp(p, "ice-pwd", 7)) {
    return sdp_parse_attrib_icepwd(pParseCtxt, fmtidbuf);
  } else if(!strncmp(p, "recvonly", 8)) {
    return sdp_parse_attrib_sendrecv(pParseCtxt, SDP_XMIT_TYPE_RECVONLY);
  } else if(!strncmp(p, "sendonly", 8)) {
    return sdp_parse_attrib_sendrecv(pParseCtxt, SDP_XMIT_TYPE_SENDONLY);
  } else if(!strncmp(p, "sendrecv", 8)) {
    return sdp_parse_attrib_sendrecv(pParseCtxt, SDP_XMIT_TYPE_SENDRECV);
  } else if(!strncmp(p, "candidate", 9)) {
    return sdp_parse_attrib_candidate(pParseCtxt, val, fmtidbuf);
  } else {
    LOG(X_DEBUG("SDP ignoring atributes '%s'"), p);
    return 0;
  }

  return rc;
}

static void sdp_parse_onend(SDP_STREAM_DESCR_T *pCommon) {

  if(pCommon->available) {

    //
    // Since when we are reading the media transport type, upon seeing SRTP we defaulted to SRTP-SDES, 
    // Now ensure that if we have a 'a=fingerprint' we intend to use DTLS-SRTP.
    // If we are lacking an SDES key (no 'a=crypto' present) than we inted to use DTLS-SRTP.
    //

    if(pCommon->transType == SDP_TRANS_TYPE_SRTP_SDES_UDP &&
       (pCommon->fingerprint.type != DTLS_FINGERPRINT_TYPE_UNKNOWN ||
        pCommon->srtp.keyBase64[0] == '\0')) {

      pCommon->transType = SDP_TRANS_TYPE_SRTP_DTLS_UDP;

    } else if(pCommon->transType == SDP_TRANS_TYPE_SRTP_DTLS_UDP &&
              pCommon->fingerprint.type == DTLS_FINGERPRINT_TYPE_UNKNOWN &&
              pCommon->srtp.keyBase64[0] != '\0') {

      pCommon->transType = SDP_TRANS_TYPE_SRTP_SDES_UDP;
    }

  } else {

    //
    // In case one root level "a=fingerprint" or "a=crypto" element  was provided, 
    // make sure to clear the media level attribute if there is no active media description
    //
    if(pCommon->fingerprint.len > 0) {
      pCommon->fingerprint.len = 0;
    }
    if(pCommon->srtp.tag[0] != '\0') {
      pCommon->srtp.tag[0] = '\0';
    }
  }

}

int sdp_parse(SDP_DESCR_T *pSdp, const char *pData, unsigned int len, int verify, int allow_noport_rtsp) {
  int rc = 0;
  KEYVAL_PAIR_T kv;
  const char *p;
  const char *p2 = pData;
  SDP_PARSE_CTXT_T parseCtxt;

  if(!pSdp || !pData || len <= 0) {
    return -1;
  }
  memset(pSdp, 0, sizeof(SDP_DESCR_T));
  memset(&parseCtxt, 0, sizeof(parseCtxt));
  parseCtxt.pSdp = pSdp;
  parseCtxt.verify = verify;
  parseCtxt.allow_noport_rtsp = allow_noport_rtsp;
  parseCtxt.lastFmt = -1;

  while((ptrdiff_t) (p2 - pData) < len) {

    p = p2;
    // find end of line
    while((ptrdiff_t) (p2 - pData) < (ptrdiff_t) len && *p2 != '\r' && *p2 !='\n') {
      p2++;
    }

    //fprintf(stderr, "line:%d\n", p2 - p);
    //avc_dumpHex(stderr, p, (p2 - p), 1);
    if(conf_parse_keyval(&kv, p, (p2 - p) ,'=', 0) == 2 && kv.key[1] == '\0') {

      switch(kv.key[0]) {
        case 'c':
          if((rc = sdp_parse_connect(&parseCtxt, kv.val)) < 0) {
            return rc;
          } 
          break;
        case 'm':
          if((rc = sdp_parse_media(&parseCtxt, kv.val)) < 0) {
            return rc;
          } 
          break;
        case 'a':
          if((rc = sdp_parse_attrib(&parseCtxt, kv.val)) < 0) {
            return rc;
          } 
          break;
        default:
          break;
      }

    }

    while((ptrdiff_t) (p2 - pData) < (ptrdiff_t) len && (*p2 == '\r' || *p2 =='\n')) {
      p2++;
    }

  } 

  //
  // Finalize each parsed media description
  //
  sdp_parse_onend(&pSdp->vid.common);
  sdp_parse_onend(&pSdp->aud.common);

  return rc;
}


int sdp_load(SDP_DESCR_T *pSdp, const char *path) {
  int rc = 0;
  FILE_STREAM_T file;
  char *pData = NULL;
  unsigned int lenData;

  if(!pSdp) {
    return -1;
  }

  if(OpenMediaReadOnly(&file, path) < 0) {
    return -1;
  }

  LOG(X_DEBUG("Reading SDP content from %s"), path);

  if(file.size > MAX_SDP_FILE_SIZE) {
    LOG(X_ERROR("SDP %s size exceeds maximum %d"), path, MAX_SDP_FILE_SIZE);
    rc = -1;
  }
  
  lenData = (unsigned int) file.size;
  if((pData = avc_calloc(1, lenData)) < 0) {
    rc = -1;
  }


  if(rc == 0 && ReadFileStream(&file, pData, lenData) != lenData) {
    rc = -1;
  }

  if(rc == 0) {

    //LOG(X_DEBUG("%s"), pData);

    rc = sdp_parse(pSdp, pData, lenData, 1, 0);
  }
  
  if(pData) {
    free(pData);
  }
  
  CloseMediaFile(&file);

  return rc;
}

static int sdp_dump_fmtp_h264(const SDP_DESCR_T *pSdp, char *pBuf, unsigned int len) {
  int rc = 0;
  unsigned int idx = 0;
  char sps[AVCC_SPS_MAX_LEN * 4 / 3];
  char pps[AVCC_PPS_MAX_LEN * 4 / 3];
  const SDP_DESCR_VID_H264_T *pH264 = &pSdp->vid.u.h264;
  sps[0] = pps[0] = '\0';

  if(pH264->spspps.sps && pH264->spspps.sps_len > 0 &&
     base64_encode(pH264->spspps.sps, pH264->spspps.sps_len, sps, sizeof(sps)) <= 0) {
    LOG(X_WARNING("SDP unable to encode H.264 sps"));
    sps[0] = '\0';
  }
  if(pH264->spspps.pps && pH264->spspps.pps_len > 0 &&
     base64_encode(pH264->spspps.pps, pH264->spspps.pps_len, pps, sizeof(pps)) <= 0) {
    LOG(X_WARNING("SDP unable to encode H.264 pps"));
    pps[0] = '\0';
  }

  // Avoid including packetization mode when output is over mpeg-2 TS
  if(pSdp->vid.common.codecType == XC_CODEC_TYPE_H264) {
    if((rc = snprintf(&pBuf[idx], len - idx, SDP_ATTR_PACKETIZATION_MODE"=%d;",
                  pSdp->vid.u.h264.packetization_mode)) < 0) {
      return rc;
    }
    idx += rc;
  }

  if((rc = snprintf(&pBuf[idx], len - idx, SDP_ATTR_PROFILE_LEVEL_ID"=%2.2X%2.2X%2.2X;"
               SDP_ATTR_SPROP_PARAMETER_SETS"=%s,%s;",
         pSdp->vid.u.h264.profile_level_id[0], pSdp->vid.u.h264.profile_level_id[1],
         pSdp->vid.u.h264.profile_level_id[2], sps, pps)) < 0) {
    return rc;
  }
  idx += rc;

  return idx;
}

static int sdp_dump_fmtp_aac(const SDP_DESCR_T *pSdp, char *pBuf, unsigned int len) {
  int rc = 0;
  char buf[32];

  if(hex_encode((const unsigned char *) pSdp->aud.u.aac.config, 
                strlen((char *) pSdp->aud.u.aac.config), buf, sizeof(buf)) <= 0) {
    buf[0] = '\0';
  }

  rc = snprintf(pBuf, len, SDP_ATTR_PROFILE_LEVEL_ID"=%d;"
               SDP_ATTR_MODE"=%s;" SDP_ATTR_SIZELENGTH"=%d;"SDP_ATTR_INDEXLENGTH"=%d;"
               SDP_ATTR_INDEXDELTALENGTH"=%d;"SDP_ATTR_CONFIG"=%s;",
         pSdp->aud.u.aac.profile_level_id, pSdp->aud.u.aac.mode,
         pSdp->aud.u.aac.sizelength,  pSdp->aud.u.aac.indexlength,
         pSdp->aud.u.aac.indexdeltalength, buf);


  return rc;
}

static int sdp_dump_fmtp_mpg4v(const SDP_DESCR_T *pSdp, char *pBuf, unsigned int len) {
  int rc = 0;
  char buf[ESDS_STARTCODES_SZMAX * 2]; 
  buf[0] = '\0';

  if(pSdp->vid.u.mpg4v.seqHdrs.hdrsLen > 0 &&
    hex_encode(pSdp->vid.u.mpg4v.seqHdrs.hdrsBuf, pSdp->vid.u.mpg4v.seqHdrs.hdrsLen,
                buf, sizeof(buf)) <= 0) {
    buf[0] = '\0';
  }

  rc = snprintf(pBuf, len, SDP_ATTR_PROFILE_LEVEL_ID"=%d;"SDP_ATTR_CONFIG"=%s;",
               pSdp->vid.u.mpg4v.profile_level_id,  buf);

  return rc;
}

static int sdp_dump_fmtp_amr(const SDP_DESCR_T *pSdp, char *pBuf, unsigned int len) {
  int rc = 0;

  rc = snprintf(pBuf, len, SDP_ATTR_OCTET_ALIGN"=%d;", pSdp->aud.u.amr.octet_align);

  return rc;
}

static int sdp_dump_attrib_rtcp(const SDP_STREAM_DESCR_T *pSdp, char *pBuf, unsigned int len) {
  int rc = 0;
  unsigned int idx = 0;

  if((pSdp->portRtcp > 0 && pSdp->port == pSdp->portRtcp) || pSdp->rtcpmux) {
    //
    // RFC 5761
    //
    if((rc = snprintf(&pBuf[idx], len - idx, "a=rtcp-mux\n")) < 0) {
      return rc;
    }
    idx += rc;
  } 

  if(pSdp->portRtcp > 0) {
    //
    // RFC 3605
    //
    if((rc = snprintf(&pBuf[idx], len - idx, "a=rtcp:%d\n", pSdp->portRtcp)) < 0) {
      return rc;
    }
    idx += rc;
  }

  return idx;
}

static int sdp_dump_attrib_rtcpfb_param(char *pBuf, unsigned int len, unsigned int *pidx, const char *fmtid, 
                                        const char *type, const char *attrib) {
  int rc;

  if((rc = snprintf(pBuf, len - *pidx, "a=rtcp-fb:%s %s%s%s\n", fmtid, type, 
                    attrib ? " " : "", attrib ? attrib : "")) >= 0) {
    *pidx += rc;
  }
  return rc;
}

static int sdp_dump_attrib_rtcpfb(const SDP_STREAM_RTCPFB_T *pRtcpfb, char *pBuf, unsigned int len) {
  int rc;
  unsigned int idx = 0;
  char fmt[16];
  char buf[32];

  if(pRtcpfb->fmtidmin1 > 0) {
    snprintf(fmt, sizeof(fmt), "%d", pRtcpfb->fmtidmin1 - 1); 
  } else {
    sprintf(fmt, "*");
  }

  if(pRtcpfb->flags & SDP_RTCPFB_TYPE_ACK) {

    if((pRtcpfb->flags & SDP_RTCPFB_TYPE_ACK_RPSI) && (rc = sdp_dump_attrib_rtcpfb_param(&pBuf[idx], len, &idx, fmt, "ack", "rpsi")) < 0) {
      return -1; 
    }
    if((pRtcpfb->flags & SDP_RTCPFB_TYPE_ACK_APP) && (rc = sdp_dump_attrib_rtcpfb_param(&pBuf[idx], len, &idx, fmt, "ack", "app")) < 0) {
      return -1; 
    }

  }

  if(pRtcpfb->flags & SDP_RTCPFB_TYPE_NACK) {

    if((pRtcpfb->flags & SDP_RTCPFB_TYPE_NACK_GENERIC) && (rc = sdp_dump_attrib_rtcpfb_param(&pBuf[idx], len, &idx, fmt, "nack", "")) < 0) {
      return -1; 
    }
    if((pRtcpfb->flags & SDP_RTCPFB_TYPE_NACK_PLI) && (rc = sdp_dump_attrib_rtcpfb_param(&pBuf[idx], len, &idx, fmt, "nack", "pli")) < 0) {
      return -1; 
    }
    if((pRtcpfb->flags & SDP_RTCPFB_TYPE_NACK_SLI) && (rc = sdp_dump_attrib_rtcpfb_param(&pBuf[idx], len, &idx, fmt, "nack", "sli")) < 0) {
      return -1; 
    }
    if((pRtcpfb->flags & SDP_RTCPFB_TYPE_NACK_RPSI) && (rc = sdp_dump_attrib_rtcpfb_param(&pBuf[idx], len, &idx, fmt, "nack", "rpsi")) < 0) {
      return -1; 
    }
    if((pRtcpfb->flags & SDP_RTCPFB_TYPE_NACK_APP) && (rc = sdp_dump_attrib_rtcpfb_param(&pBuf[idx], len, &idx, fmt, "nack", "app")) < 0) {
      return -1; 
    }

  }

  if((pRtcpfb->flags & SDP_RTCPFB_TYPE_TRRINT) || pRtcpfb->trrIntervalMs > 0) {
    snprintf(buf, sizeof(buf), "%d", pRtcpfb->trrIntervalMs);
    if((rc = sdp_dump_attrib_rtcpfb_param(&pBuf[idx], len, &idx, fmt, "trr-int", buf)) < 0) {
      return -1; 
    }

  }

  if(pRtcpfb->flags & SDP_RTCPFB_TYPE_CCM) {

    if((pRtcpfb->flags & SDP_RTCPFB_TYPE_CCM_FIR) && (rc = sdp_dump_attrib_rtcpfb_param(&pBuf[idx], len, &idx, fmt, "ccm", "fir")) < 0) {
      return -1; 
    }
    if((pRtcpfb->flags & SDP_RTCPFB_TYPE_CCM_TMMBR) && (rc = sdp_dump_attrib_rtcpfb_param(&pBuf[idx], len, &idx, fmt, "ccm", "tmmbr")) < 0) {
      return -1; 
    }
    if((pRtcpfb->flags & SDP_RTCPFB_TYPE_CCM_TSTR) && (rc = sdp_dump_attrib_rtcpfb_param(&pBuf[idx], len, &idx, fmt, "ccm", "tstr")) < 0) {
      return -1; 
    }
    if((pRtcpfb->flags & SDP_RTCPFB_TYPE_CCM_VBCM) && (rc = sdp_dump_attrib_rtcpfb_param(&pBuf[idx], len, &idx, fmt, "ccm", "vbcm")) < 0) {
      return -1; 
    }

  }

  return idx;
}

static int sdp_dump_attrib_ice(const SDP_ICE_T *pIce, char *pBuf, unsigned int len) {
  int rc;
  unsigned int idx = 0;

  if(pIce->ufrag[0] != '\0' && (rc = snprintf(&pBuf[idx], len - idx, "a=ice-ufrag:%s\n", pIce->ufrag)) >= 0) {
    idx += rc;
  }

  if(pIce->pwd[0] != '\0' && (rc = snprintf(&pBuf[idx], len - idx, "a=ice-pwd:%s\n", pIce->pwd)) >= 0) {
    idx += rc;
  }

  return idx;
}

static const char *sdp_transTypeStr(SDP_TRANS_TYPE_T transType, int avpf) {

  switch(transType) {

    case SDP_TRANS_TYPE_SRTP_SDES_UDP:
    case SDP_TRANS_TYPE_SRTP_DTLS_UDP:
      if(avpf) {
        return SDP_TRANS_SRTP_AVPF;
      } else {
        return SDP_TRANS_SRTP_AVP;
      }

    case SDP_TRANS_TYPE_DTLS_UDP:
      if(avpf) {
        return SDP_TRANS_DTLS_AVPF;
      } else {
        return SDP_TRANS_DTLS_AVP;
      }

    case SDP_TRANS_TYPE_RAW_UDP:
      return SDP_TRANS_UDP_AVP;

    case SDP_TRANS_TYPE_RTP_UDP:
      if(avpf) {
        return SDP_TRANS_RTP_AVPF;
      } else {
        return SDP_TRANS_RTP_AVP;
      }

    case SDP_TRANS_TYPE_UNKNOWN:

    default:
      return SDP_TRANS_RTP_AVP;
  }
}

static int sdp_dump_attrib_crypto(const SDP_STREAM_SRTP_T *pSrtp, char *pBuf, unsigned int len) {
  int rc;
  const char *keyType = srtp_streamTypeStr(pSrtp->type.authType, pSrtp->type.confType, 0); 

  if(pSrtp->tag == '\0' || keyType == NULL || pSrtp->keyBase64[0] == '\0') {
    LOG(X_ERROR("SDP crypto invalid tag: '%s', keyType: '%s', key: '%s'"), pSrtp->tag, keyType, pSrtp->keyBase64[0]);
    return -1;
  }

  if((rc = snprintf(pBuf, len, "a=crypto:%s %s inline:%s\n", pSrtp->tag, keyType, pSrtp->keyBase64)) < 0) {
    return -1;
  }

  return rc;
}

static int sdp_dump_attrib_fingerprint(const DTLS_FINGERPRINT_T *pFingerprint, char *pBuf, unsigned int len) {
  int rc;
  char buf[256];
  const char *fingerprintType = sdp_dtls_get_fingerprint_typestr(pFingerprint->type);

  if(pFingerprint->len <= 0) {
    return 0;
  }

  if((rc = snprintf(pBuf, len, "a=fingerprint:%s %s\n", fingerprintType, dtls_fingerprint2str(pFingerprint, buf, sizeof(buf)))) < 0) {
    return -1;
  }

  return rc;
}

static int sdp_dump_vid_framerate(const FRAME_RATE_T *pFps, char *pBuf, unsigned int len) {
  int rc = 0;
  char buf[64];

  if(pFps->clockHz > 0 && pFps->frameDeltaHz > 0) {
    if((rc = snprintf(buf, sizeof(buf), "%.3f", (double)pFps->clockHz / pFps->frameDeltaHz)) > 0) {
      while(rc > 2 && buf[rc - 1] == '0' && buf[rc - 2] != '.') {
        buf[rc -1] = '\0';
        rc--;
      }
      rc = snprintf(pBuf, len, "a=framerate:%s\n", buf);
    }
  }

  return rc;
}

static int sdp_dump_attrib_control(const SDP_STREAM_DESCR_T *pDescr, char *pBuf, unsigned int len) {
  int rc;

  if((rc = snprintf(pBuf, len , "a=control:%s=%d\n", 
             SDP_ATTRIB_CONTROL_TRACKID, pDescr->controlId)) < 0) {
    return -1;
  }

  return rc;
}

static int sdp_dump_attrib_candidate(const SDP_CANDIDATE_T *pCandidate, char *pBuf, unsigned int len) {
  int rc;
  char raddr[64];
  
  if(IS_ADDR_VALID(pCandidate->raddress.sin_addr) && pCandidate->raddress.sin_port != 0) {
    snprintf(raddr, sizeof(raddr), " raddr %s rport %d", 
             inet_ntoa(pCandidate->raddress.sin_addr), htons(pCandidate->raddress.sin_port));
  } else {
    raddr[0] = '\0';
  }

  if((rc = snprintf(pBuf, len , "a=candidate:%ld %d %s %ld %s %d typ %s%s\n", 
             pCandidate->foundation, 
             pCandidate->component, 
             ice_candidate_transport2str(pCandidate->transport),
             pCandidate->priority,
             inet_ntoa(pCandidate->address.sin_addr),
             htons(pCandidate->address.sin_port),
             ice_candidate_type2str(pCandidate->type), 
             raddr)) < 0) {
    return -1;
  }

  return rc;
}

static int sdp_dump_attrib_custom(const SDP_STREAM_DESCR_T *pDescr, char *pBuf, unsigned int len) {
  int rc;
  unsigned int idx;
  unsigned int totlen = 0;

  for(idx = 0; idx < sizeof(pDescr->attribCustom) / sizeof(pDescr->attribCustom[0]); 
      idx++) {
    if(pDescr->attribCustom[idx]) {
      if((rc = snprintf(&pBuf[totlen], len - idx, "a=%s\n", pDescr->attribCustom[idx])) < 0) {
        return -1;
      }
      totlen += rc;
    }
  }

  return totlen;
}

static unsigned int create_sesid() {
  unsigned int sesid;
  sesid = random();
  return sesid;
}

static unsigned int create_sesver() {
  unsigned int sesver;
  sesver = time(NULL);
  return sesver;
}

const char *create_sesip() {
  char *sesip;
  struct in_addr addr;

  addr.s_addr = net_getlocalip();

  sesip = inet_ntoa(addr);
  
  return sesip;
}

int sdp_dump(const SDP_DESCR_T *pSdp, char *buf, unsigned int len) {
  int rc = 0;
  int sz;
  unsigned int idx = 0;
  unsigned int idx2;
  char tmpbuf[1024];

  if(!pSdp || !buf) {
    return -1;
  }

  //
  // print v= version
  //
  if((rc = snprintf(&buf[idx], len - idx, "v=0\n")) < 0) {
    return rc;   
  }
  idx += rc;

  //
  // print o= origin
  //
  if((rc = snprintf(&buf[idx], len - idx, "o=%s %u %u IN IP4 %s\n", 
          (pSdp->o.username[0] ? pSdp->o.username : "-"), 
           (pSdp->o.sessionid ? pSdp->o.sessionid : create_sesid()), 
           (pSdp->o.sessionver ? pSdp->o.sessionver : create_sesver()), 
          (pSdp->o.iphost[0] ? pSdp->o.iphost: create_sesip()))) < 0) {
    return rc;
  }
  idx += rc;

  //
  // print s= session name
  //
  if((rc = snprintf(&buf[idx], len - idx, "s=%s\n", pSdp->sessionName)) < 0) {
    return rc;
  }
  idx += rc;

  //
  // print c= connection data
  //
  if((rc = snprintf(&buf[idx], len - idx, "c=IN IP4 %s", pSdp->c.iphost)) < 0) {
    return rc;
  }
  idx += rc;

  if(pSdp->c.ttl > 0) {
    if((rc = snprintf(&buf[idx], len - idx, "/%d", pSdp->c.ttl)) < 0) {
      return rc;
    }
    idx += rc;
  }
  if(pSdp->c.numaddr > 1) {
    if((rc = snprintf(&buf[idx], len - idx, "/%d", pSdp->c.numaddr)) < 0) {
      return rc; 
    }
    idx += rc;
  }
  if((rc = snprintf(&buf[idx], len - idx, "\n")) < 0) {
    return rc;
  }
  idx += rc;

  //
  // print t= timing information
  //
  if((rc = snprintf(&buf[idx], len - idx, 
      "t=%"LL64"u %"LL64"u\n", pSdp->t.tm_start, pSdp->t.tm_stop)) < 0) {
    return rc;
  }
  idx += rc;

  //
  // print a= attributes
  //

  //
  //TODO: the a=control:* should be uncommented in the future as some mobile RTSP clients
  // will fail if a session control URL is not provided
  //
  //if((rc = snprintf(&buf[idx], len - idx, "a=control:*\n")) < 0) { 
  //  return rc; 
  //} 
  //idx+=rc;


  //
  // print m= media video
  //
  if(pSdp->vid.common.available) {

    if((rc = snprintf(&buf[idx], len - idx, "m=video %d %s %d\n", 
        pSdp->vid.common.port, 
        sdp_transTypeStr(pSdp->vid.common.transType, pSdp->vid.common.avpf),
        pSdp->vid.common.payloadType)) < 0) {
      return -1;
    }
    idx += rc;

    for(idx2 = 0; idx2 < 2; idx2++) {
      if(pSdp->vid.common.iceCandidates[idx2].foundation != 0) {
        if((rc = sdp_dump_attrib_candidate(&pSdp->vid.common.iceCandidates[idx2], &buf[idx], len - idx)) < 0) {
          return -1;
        }
        idx += rc;
      }
    }

    if((rc = snprintf(&buf[idx], len - idx, "a=rtpmap:%d %s/%d\n", 
                pSdp->vid.common.payloadType, pSdp->vid.common.encodingName, 
                pSdp->vid.common.clockHz)) < 0) {
      return -1;
    }

    idx += rc;
    if((rc = snprintf(&buf[idx], len - idx, "a=fmtp:%d", 
                      pSdp->vid.common.payloadType)) < 0) {
      return -1;
    }

    idx += rc;
    sz = 0;
    switch(pSdp->vid.common.codecType) {
      case XC_CODEC_TYPE_H264: 
        sz = sdp_dump_fmtp_h264(pSdp, tmpbuf, sizeof(tmpbuf));
        break;
      case XC_CODEC_TYPE_MPEG4V: 
        sz = sdp_dump_fmtp_mpg4v(pSdp, tmpbuf, sizeof(tmpbuf));
        break;
      default:
        break;
    }
    if(sz > 0) {
      if((rc = snprintf(&buf[idx], len - idx, " %s", tmpbuf)) < 0) {
        return -1;
      }
      idx += rc;
    }
    if((rc = snprintf(&buf[idx], len - idx, "\n")) < 0) {
      return -1; 
    }
    idx += rc;

    //
    // SRTP SDES crypto key
    //
    if(pSdp->vid.common.transType == SDP_TRANS_TYPE_SRTP_SDES_UDP) {
      if((rc = sdp_dump_attrib_crypto(&pSdp->vid.common.srtp, &buf[idx], len - idx)) < 0) {
        return -1;
      }
      idx += rc;
    }

    //
    // DTLS fingerprint 
    //
    if(pSdp->vid.common.fingerprint.len > 0) {
      if((rc = sdp_dump_attrib_fingerprint(&pSdp->vid.common.fingerprint, &buf[idx], len - idx)) < 0) {
        return -1;
      }
      idx += rc;
    }

    //
    // Non-default (non RTP + 1) RTCP ports
    //
    if(pSdp->vid.common.portRtcp > 0 || pSdp->vid.common.rtcpmux) {
      if((rc = sdp_dump_attrib_rtcp(&pSdp->vid.common, &buf[idx], len - idx)) < 0) {
        return -1;
      }
      idx += rc;
    }

    //
    // RTCP FB types a=rtcp-fb:
    //
    if(pSdp->vid.common.rtcpfb.flags != 0) {
      if((rc = sdp_dump_attrib_rtcpfb(&pSdp->vid.common.rtcpfb, &buf[idx], len - idx)) < 0) {
        return -1;
      }
      idx += rc;
    }

    //
    // ice-ufrag, ice-pwd
    //
    if((rc = (sdp_dump_attrib_ice(&pSdp->vid.common.ice, &buf[idx], len - idx))) < 0) {
      return -1;
    } 
    idx += rc;

    if(pSdp->vid.common.controlId > 0) {
      if((rc = sdp_dump_attrib_control(&pSdp->vid.common, &buf[idx], len - idx)) < 0) {
        return -1;
      }
      idx += rc;
    }

    if(pSdp->vid.fps.clockHz > 0 && pSdp->vid.fps.frameDeltaHz > 0) {
      if((rc = sdp_dump_vid_framerate(&pSdp->vid.fps, &buf[idx], len - idx)) < 0) {
        return -1;
      }
      idx += rc;
    }


    if((rc = sdp_dump_attrib_custom(&pSdp->vid.common, &buf[idx], len - idx)) < 0) {
      return -1;
    }
    idx += rc;
  }

  //
  // print m= media audio
  //
  if(pSdp->aud.common.available) {

    if((rc = snprintf(&buf[idx], len - idx, "m=audio %d %s %d\n", 
        pSdp->aud.common.port, 
        sdp_transTypeStr(pSdp->aud.common.transType, pSdp->aud.common.avpf),
        pSdp->aud.common.payloadType)) < 0) {
      return -1;
    }
    idx += rc;

    for(idx2 = 0; idx2 < 2; idx2++) {
      if(pSdp->aud.common.iceCandidates[idx2].foundation != 0) {
        if((rc = sdp_dump_attrib_candidate(&pSdp->aud.common.iceCandidates[idx2], &buf[idx], len - idx)) < 0) {
          return -1;
        }
        idx += rc;
      }
    }

    if((rc = snprintf(&buf[idx], len - idx, "a=rtpmap:%d %s/%d/%d\n", 
                pSdp->aud.common.payloadType, pSdp->aud.common.encodingName, 
                pSdp->aud.common.clockHz, pSdp->aud.channels)) < 0) {
      return -1;
    }
    idx += rc;
    if((rc = snprintf(&buf[idx], len - idx, "a=fmtp:%d", 
                      pSdp->aud.common.payloadType)) < 0) {
      return -1;
    }
    idx += rc;
    sz = 0;
    switch(pSdp->aud.common.codecType) {
      case XC_CODEC_TYPE_AAC: 
        sz = sdp_dump_fmtp_aac(pSdp, tmpbuf, sizeof(tmpbuf));
        break;
      case XC_CODEC_TYPE_AMRNB: 
        sz = sdp_dump_fmtp_amr(pSdp, tmpbuf, sizeof(tmpbuf));
        break;
      default:
        break;
    }
    if(sz > 0) {
      if((rc = snprintf(&buf[idx], len - idx, " %s", tmpbuf)) < 0) {
        return -1;
      }
      idx += rc;
    }
    if((rc = snprintf(&buf[idx], len - idx, "\n")) < 0) {
      return -1; 
    }
    idx += rc;

    //
    // SRTP SDES crypto key
    //
    if(pSdp->aud.common.transType == SDP_TRANS_TYPE_SRTP_SDES_UDP) {
      if((rc = sdp_dump_attrib_crypto(&pSdp->aud.common.srtp, &buf[idx], len - idx)) < 0) {
        return -1;
      }
      idx += rc;
    }

    //
    // DTLS fingerprint 
    //
    if(pSdp->aud.common.fingerprint.len > 0) {
      if((rc = sdp_dump_attrib_fingerprint(&pSdp->aud.common.fingerprint, &buf[idx], len - idx)) < 0) {
        return -1;
      }
      idx += rc;
    }

    //
    // Non-default (non RTP + 1) RTCP ports
    //
    if(pSdp->aud.common.portRtcp > 0 || pSdp->aud.common.rtcpmux) {
      if((rc = sdp_dump_attrib_rtcp(&pSdp->aud.common, &buf[idx], len - idx)) < 0) {
        return -1;
      }
      idx += rc;
    }

    //
    // RTCP FB types a=rtcp-fb:
    //
    if(pSdp->aud.common.rtcpfb.flags != 0) {
      if((rc = sdp_dump_attrib_rtcpfb(&pSdp->aud.common.rtcpfb, &buf[idx], len - idx)) < 0) {
        return -1;
      }
      idx += rc;
    }

    //
    // ice-ufrag, ice-pwd
    //
    if((rc = (sdp_dump_attrib_ice(&pSdp->aud.common.ice, &buf[idx], len - idx))) < 0) {
      return -1;
    }
    idx += rc;

    if(pSdp->aud.common.controlId > 0) {
      if((rc = sdp_dump_attrib_control(&pSdp->aud.common, &buf[idx], len - idx)) < 0) {
        return -1;
      }
      idx += rc;
    }

    if((rc = sdp_dump_attrib_custom(&pSdp->aud.common, &buf[idx], len - idx)) < 0) {
      return -1;
    }
    idx += rc;
  }

  return idx;
}

int sdp_write(const SDP_DESCR_T *pSdp, void *pFp, int logLevel) {
  //FILE *fp = (FILE *) pFp;
  char buf[4096];
  int rc = 0;

  if(!pSdp) {
    return -1;
  }

  if((rc = sdp_dump(pSdp, buf, sizeof(buf))) < 0) {
    return rc;
  }

  if(logLevel > 0) {
    LOG(logLevel, "%s\n", buf);
  }

  if(pFp) {
    rc = fwrite(buf, 1, rc, pFp);
  }

  return rc;
}

int sdp_write_path(const SDP_DESCR_T *pSdp, const char *path) {

  FILE_STREAM_T output;
  char sdpBuf[4096];
  int rc = 0;
  int sz;

  memset(&output, 0, sizeof(output));
  strncpy(output.filename, path, sizeof(output.filename) - 5);
  if((sz = strlen(output.filename)) < 4 ||
     strncasecmp(&output.filename[sz - 4], ".sdp", 4)) {
    strncpy(&output.filename[sz], ".sdp", sizeof(output.filename) - sz);
  }

  if(capture_openOutputFile(&output, 1, NULL) == 0) {

    if((rc = sdp_dump(pSdp, sdpBuf, sizeof(sdpBuf))) >= 0) {
      WriteFileStream(&output, sdpBuf, rc);
    }

    LOG(X_DEBUG("Created %s"), output.filename);

    CloseMediaFile(&output);
  }

  return rc;
}



int sdp_isvalidFile(FILE_STREAM_T *pFile) {
  unsigned int sz;

  if(!pFile) {
    return 0;
  }

  if((sz = strlen(pFile->filename)) >= 4 && 
     !strncasecmp(&pFile->filename[sz - 4], ".sdp", 4)) {
    return 1;
  }

  return 0;
}

int sdp_isvalidFilePath(const char *path) {
  FILE_STREAM_T fs;
  int rc = 0;
  struct stat st;

  if(!path) {
    return 0;
  }

  if(!strncasecmp(path, "/dev/", 5)) {
    return 0;
  }

  if(fileops_stat(path, &st) != 0) {
    return 0;
  }

  if(OpenMediaReadOnly(&fs, path) != 0) {
    return 0;
  }

  rc = sdp_isvalidFile(&fs);

  CloseMediaFile(&fs);

  return rc;
}
