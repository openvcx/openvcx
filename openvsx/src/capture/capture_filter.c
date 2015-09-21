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


CAPTURE_FILTER_PROTOCOL_T getFilterProtoType(const char *val) {

  if(!strncmp("h264", val, 4)) {
    return  CAPTURE_FILTER_PROTOCOL_H264;
  } else if(!strncmp("mpg4", val, 4)) {
    return CAPTURE_FILTER_PROTOCOL_MPEG4V;
  } else if(!strncmp("vp8", val, 4)) {
    return CAPTURE_FILTER_PROTOCOL_VP8;
  } else if(!strncmp("h263+", val, 5) || !strncmp("h263p", val, 5)) {
    return CAPTURE_FILTER_PROTOCOL_H263_PLUS;
  } else if(!strncmp("h263", val, 4)) {
    return CAPTURE_FILTER_PROTOCOL_H263;
  } else if(!strncmp("aac", val, 3)) {
    return CAPTURE_FILTER_PROTOCOL_AAC;
  } else if(!strncmp("ac3", val, 3)) {
    return CAPTURE_FILTER_PROTOCOL_AC3;
  } else if(!strncmp("amr", val, 3)) {
    return CAPTURE_FILTER_PROTOCOL_AMR;
  } else if(!strncmp("silk", val, 4)) {
    return CAPTURE_FILTER_PROTOCOL_SILK;
  } else if(!strncmp("opus", val, 4)) {
    return CAPTURE_FILTER_PROTOCOL_OPUS;
  } else if(!strncmp("g711a", val, 5)) {
    return CAPTURE_FILTER_PROTOCOL_G711_ALAW;
  } else if(!strncmp("g711", val, 4)) {
    return CAPTURE_FILTER_PROTOCOL_G711_MULAW;
  } else if(!strncmp("m2t", val, 3)) {
    return CAPTURE_FILTER_PROTOCOL_MP2TS;
  } else if(!strncmp("rtmp", val, 4)) {
    return CAPTURE_FILTER_PROTOCOL_RTMP;
  } else if(!strncmp("flv", val, 3)) {
    return CAPTURE_FILTER_PROTOCOL_FLV;
  } else if(!strncmp("mp4", val, 3)) {
    return CAPTURE_FILTER_PROTOCOL_MP4;
  } else if(!strncmp("dash", val, 4) || !strncmp("mpd", val, 3)) {
    return CAPTURE_FILTER_PROTOCOL_DASH;
  } else if(!strncmp("raw", val, 3)) {
    return CAPTURE_FILTER_PROTOCOL_RAW;
  //} else if(!strncmp("bmp", val, 3)) {
  //  return CAPTURE_FILTER_PROTOCOL_BMP;

  } else if(!strncmp("rgba", val, 4)) {
    return CAPTURE_FILTER_PROTOCOL_RAWV_RGBA32;
  } else if(!strncmp("bgra", val, 4)) {
    return CAPTURE_FILTER_PROTOCOL_RAWV_BGRA32;
  } else if(!strncmp("rgb565", val, 6)) {
    return CAPTURE_FILTER_PROTOCOL_RAWV_RGB565;
  } else if(!strncmp("bgr565", val, 6)) {
    return CAPTURE_FILTER_PROTOCOL_RAWV_BGR565;
  } else if(!strncmp("rgb", val, 3)) {
    return CAPTURE_FILTER_PROTOCOL_RAWV_RGB24;
  } else if(!strncmp("bgr", val, 3)) {
    return CAPTURE_FILTER_PROTOCOL_RAWV_BGR24;
  } else if(!strncmp("nv12", val, 4)) {
    return CAPTURE_FILTER_PROTOCOL_RAWV_NV12;
  } else if(!strncmp("nv21", val, 4) || !strncmp("yuv420sp", val, 8)) {
    return CAPTURE_FILTER_PROTOCOL_RAWV_NV21;
  } else if(!strncmp("yuva420p", val, 8)) {
    return CAPTURE_FILTER_PROTOCOL_RAWV_YUVA420P;
  } else if(!strncmp("yuv420p", val, 7) || !strncmp("yuv", val, 3)) {
    return CAPTURE_FILTER_PROTOCOL_RAWV_YUV420P;
  } else if(!strncmp("pcm", val, 3)) {
    return CAPTURE_FILTER_PROTOCOL_RAWA_PCM16LE;
  } else if(!strncmp("ulaw", val, 4)) {
    return CAPTURE_FILTER_PROTOCOL_RAWA_PCMULAW;
  } else if(!strncmp("alaw", val, 4)) {
    return CAPTURE_FILTER_PROTOCOL_RAWA_PCMALAW;

  } else {
    return -1;
  }

}


int capture_createOutPath(char *buf, unsigned int sz,
                                    const char *dir,
                                    const char *outputFilePrfx,
                                    const COLLECT_STREAM_HDR_T *pPktHdr,
                                    enum CAPTURE_FILTER_PROTOCOL mediaType) {
  int idx = 0; 
  int rc = 0;
  char dirDelim[2];

  dirDelim[0] = dirDelim[1] = '\0';
  if(dir && dir[0] != '\0' && dir[strlen(dir)-1] != DIR_DELIMETER) {
    dirDelim[0] = DIR_DELIMETER;
  } 

  if(outputFilePrfx[0] != '\0') {
    if((idx = snprintf(buf, sz - 1, "%s%s%s",
          (dir ? dir : ""),  dirDelim, outputFilePrfx)) < 0) {
      return -1;
    }
  } else if(pPktHdr && pPktHdr->key.lenKey > 0 && 
          (idx = snprintf(buf, sz - 1, "%s%srec_%x_%x_%x_%x_%x_pt%u",
          (dir ? dir : ""),  dirDelim,
          htonl(pPktHdr->key.ipAddr.ip_un.ipv4.srcIp.s_addr), pPktHdr->key.srcPort,
          htonl(pPktHdr->key.ipAddr.ip_un.ipv4.dstIp.s_addr), pPktHdr->key.dstPort,
          pPktHdr->key.ssrc, pPktHdr->payloadType)) < 0) {
    return -1;
  } 

  if(idx == 0 && (idx = snprintf(buf, sz - 1, "%s%srec_%ld",
          (dir ? dir : ""),  dirDelim, time(NULL))) < 0) {
    return -1;
  }          

  if(outputFilePrfx[0] == '\0' || !strchr(buf, '.')) {

    switch(mediaType) {
      case CAPTURE_FILTER_PROTOCOL_H264:
        rc = snprintf(&buf[idx], sz - idx - 1, ".h264");
        break;
      case CAPTURE_FILTER_PROTOCOL_AAC:
        rc = snprintf(&buf[idx], sz - idx - 1, ".aac");
        break;
      case CAPTURE_FILTER_PROTOCOL_MP2TS:
        rc = snprintf(&buf[idx], sz - idx - 1, ".m2t");
        break;
      case CAPTURE_FILTER_PROTOCOL_H262:
        rc = snprintf(&buf[idx], sz - idx - 1, ".h262");
        break;
      case CAPTURE_FILTER_PROTOCOL_MP4V:
      case CAPTURE_FILTER_PROTOCOL_H263:
      case CAPTURE_FILTER_PROTOCOL_H263_PLUS:
        rc = snprintf(&buf[idx], sz - idx - 1, ".h263");
        break;
      case CAPTURE_FILTER_PROTOCOL_AC3:
        rc = snprintf(&buf[idx], sz - idx - 1, ".ac3");
        break;
      case CAPTURE_FILTER_PROTOCOL_FLV:
        rc = snprintf(&buf[idx], sz - idx - 1, ".flv");
        break;
      case CAPTURE_FILTER_PROTOCOL_MP4:
        rc = snprintf(&buf[idx], sz - idx - 1, ".mp4");
        break;
      case CAPTURE_FILTER_PROTOCOL_MKV:
        rc = snprintf(&buf[idx], sz - idx - 1, ".mkv");
        break;
      default:
        rc = snprintf(&buf[idx], sz - idx - 1, ".raw");
        break;
    }
 
    if(rc < 0) {
      return -1;
    }

  }

  return idx + rc;
}

//#define MOVE_WHILE_CHAR(p,c)    if((p)) {while((*(p) == (c))) { (p)++; } }

static uint32_t parseUint32Val(const char *str) {
  const char *p = str;
  unsigned int len = strlen(str);
  unsigned int val = 0;
  unsigned int hex = 0;

  if(p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
    p += 2;
    while((unsigned int) (p - str) < len) {
      if(*p >= '0' && *p <= '9') {
        hex = *p - '0';
      } else if((*p >= 'A' && *p <= 'F')) {
        hex = *p - 'A' + 10;
      } else if(*p >= 'a' && *p <= 'f') {
        hex = *p - 'a' + 10;
      }

      val <<= 4;
      val |= hex;

      p++;
    }

  } else {
    val = atoi(str);
  }

  return val;
}

static int parseFilterExpr(const char *str, unsigned int len, CAPTURE_FILTER_T *pFilter) {
  const char *p;
  const char *key, *keyEnd, *val;
  char valbuf[256];
  
  // expression syntax is "key=value"
  p = str;

  MOVE_WHILE_CHAR(p, ' ');
  MOVE_WHILE_CHAR(p, ',');
  MOVE_WHILE_CHAR(p, ' ');
  key = p;
  while(*p != '\0' && *p != '=' && *p != ' ' && *p != '\t' && *p != ',') {
    p++;
  }
  keyEnd = p;

  MOVE_WHILE_CHAR(p, ' ');
  MOVE_WHILE_CHAR(p, '=');
  MOVE_WHILE_CHAR(p, ' ');
  val = p;
  while(*p != '\0' && *p !=',' && *p != ' ') {
    p++;
  }
  if(p - val > sizeof(valbuf) - 1) {
    p = (val + sizeof(valbuf) - 1);
  }
  //memset(valbuf, 0, sizeof(valbuf));
  memcpy(valbuf, val, p - val);
  valbuf[p - val] = '\0';

  if(!strncmp("dst", key, keyEnd - key)) {
    pFilter->dstIp = inet_addr(valbuf);
  } else if(!strncmp("src", key, keyEnd - key)) {
    pFilter->srcIp = inet_addr(valbuf);
  } else if(!strncmp("ip", key, keyEnd - key)) {
    pFilter->ip = inet_addr(valbuf);
  } else if(!strncmp("dstport", key, keyEnd - key)) {
    pFilter->dstPort = parseUint32Val(valbuf);
  } else if(!strncmp("srcport", key, keyEnd - key)) {
    pFilter->srcPort = parseUint32Val(valbuf);
  } else if(!strncmp("port", key, keyEnd - key)) {
    pFilter->port = parseUint32Val(valbuf);
  } else if(!strncmp("pt", key, keyEnd - key)) {
    pFilter->payloadType = (unsigned char) parseUint32Val(valbuf);
    pFilter->haveRtpPtFilter = 1;
  } else if(!strncmp("ssrc", key, keyEnd - key)) {
    pFilter->ssrcFilter = parseUint32Val(valbuf);
    pFilter->haveRtpSsrcFilter = 1;
  } else if(!strncmp("clock", key, keyEnd - key)) {
    pFilter->u_seqhdrs.aud.common.clockHz = parseUint32Val(valbuf);
    pFilter->u_seqhdrs.vid.common.clockHz = pFilter->u_seqhdrs.aud.common.clockHz; 
  } else if(!strncmp("channels", key, keyEnd - key)) {
    pFilter->u_seqhdrs.aud.channels = parseUint32Val(valbuf);
    pFilter->u_seqhdrs.aud.u.aac.decoderCfg.channelIdx = pFilter->u_seqhdrs.aud.channels;
  } else if(!strncmp("fps", key, keyEnd - key)) {
    pFilter->fps = atof(valbuf);
  } else if(!strncmp("width", key, keyEnd - key)) {
    pFilter->width = parseUint32Val(valbuf);
  } else if(!strncmp("height", key, keyEnd - key)) {
    pFilter->height = parseUint32Val(valbuf);
  } else if(!strncmp("size", key, keyEnd - key)) {
    pFilter->height = parseUint32Val(valbuf);
    if(strutil_parseDimensions(valbuf, &pFilter->width, &pFilter->height) < 0) {
      LOG(X_ERROR("Invalid filter parameter size format '%s'"), valbuf); 
      return -1;
    }
  } else if(!strncmp("type", key, keyEnd - key)) {

    if((int)(pFilter->mediaType = getFilterProtoType(val)) < CAPTURE_FILTER_PROTOCOL_UNKNOWN) {
      LOG(X_ERROR("Unrecognized filter type value '%s'. Valid types:\n               [%s]"), 
                  val, CAP_FILTER_TYPES_DISP_STR);
      return -1;
    } else if(pFilter->mediaType == CAPTURE_FILTER_PROTOCOL_AAC) {
      pFilter->u_seqhdrs.aud.u.aac.decoderCfg.objTypeIdx = 2; // AAC-LC default
    } else if(pFilter->mediaType == CAPTURE_FILTER_PROTOCOL_AMR) {
      pFilter->u_seqhdrs.aud.u.amr.octet_align = 1;
    }

  } else if(!strncmp("trans", key, keyEnd - key)) {

    // This has been more or less deprecated in favor of passing transport type 
    // as part of input file/url such as  'rtp://'

    if(!strncmp("udp", val, 3)) {
      pFilter->transType = CAPTURE_FILTER_TRANSPORT_UDPRAW;
    } else if(!strncmp("rtp", val, 3)) {
      pFilter->transType = CAPTURE_FILTER_TRANSPORT_UDPRTP;
    } else if(!strncmp("dtls-srtp", val, 9) ||  !strncmp("dtlssrtp", val, 8)) {
      pFilter->transType = CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP;
    } else if(!strncmp("dtls", val, 4)) {
      pFilter->transType = CAPTURE_FILTER_TRANSPORT_UDPDTLS;
    } else if(!strncmp("srtp", val, 4)) {
      pFilter->transType = CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES;
    } else if(!strncmp("tcp", val, 3)) {
      pFilter->transType = CAPTURE_FILTER_TRANSPORT_TCP;
    } else if(!strncmp("http", val, 4)) {
      pFilter->transType = CAPTURE_FILTER_TRANSPORT_HTTPGET;
    } else if(!strncmp("https", val, 5)) {
      pFilter->transType = CAPTURE_FILTER_TRANSPORT_HTTPSGET;
    } else if(!strncmp("rtsps", val, 5)) {
      pFilter->transType = CAPTURE_FILTER_TRANSPORT_RTSPS;
    } else if(!strncmp("rtsp", val, 4)) {
      pFilter->transType = CAPTURE_FILTER_TRANSPORT_RTSP;
    } else if(!strncmp("rtmps", val, 5)) {
      pFilter->transType = CAPTURE_FILTER_TRANSPORT_RTMPS;
    } else if(!strncmp("rtmp", val, 4)) {
      pFilter->transType = CAPTURE_FILTER_TRANSPORT_RTMP;
    } else if(!strncmp("dev", val, 3)) {
      pFilter->transType = CAPTURE_FILTER_TRANSPORT_DEV;
    } else {
      LOG(X_ERROR("Unrecognized filter transport type value '%s'. Valid types: [%s]"), val, CAP_FILTER_TRANS_DISP_STR);
      return -1;
    }

  } else if(!strncmp("file", key, keyEnd - key)) {
    strncpy(pFilter->outputFilePrfx, valbuf, sizeof(pFilter->outputFilePrfx) - 1);
    pFilter->outputFilePrfx[sizeof(pFilter->outputFilePrfx) - 1] = '\0';
  } else {
    LOG(X_ERROR("Unrecognized filter expression starting at '%s'."), key); 
    return -1;
  }


  return (p - str);
}

int capture_parseFilter(const char *str, CAPTURE_FILTER_T *pFilter) {
  const char *p;
  int rc = 0;
  
  if(!str) {
    return -1;
  }

  //
  // filter syntax is "key=value  key= value, key =value, key = value key == value"
  // eg.  dst=10.0.1.1 src=192.168.0.1 srcport=16384 dstport=1024 ssrc=0x01020304 pt=8,type=raw
  // 
  memset(pFilter, 0, sizeof(CAPTURE_FILTER_T));
  pFilter->mediaType = CAPTURE_FILTER_PROTOCOL_UNKNOWN;
  p = str;

  while(*p == '"') {
    p++;
  }

  while(*p != '\0') {

    if((rc = parseFilterExpr(p, (p - str), pFilter)) < 0) {
      break;
    }
    p += (rc);
  }

  if(rc < 0) {
    if(pFilter->mediaType == CAPTURE_FILTER_PROTOCOL_UNKNOWN) {
      LOG(X_ERROR("Filter does not contain media 'type' field. Include type=[%s] in the filter expression."), CAP_FILTER_TYPES_DISP_STR);
      rc = -1;
    } else if(pFilter->transType == CAPTURE_FILTER_TRANSPORT_UDPRAW) {
      if(pFilter->haveRtpSsrcFilter != 0) {
        LOG(X_ERROR("Transport type must be 'rtp' to use 'ssrc' filter."));
        rc = -1;
      } else if(pFilter->haveRtpPtFilter) {
        LOG(X_ERROR("Transport type must be 'rtp' to use 'pt' filter."));
        rc = -1;
      }
    }
  } 

  if(rc < 0) {
    capture_freeFilters(pFilter, 1);
    return -1;
  }

  return 0;
}

#endif // VSX_HAVE_CAPTURE

int capture_openOutputFile(FILE_STREAM_T *pFile,
                           int overWriteOutput,
                           const char *outDir) {
  DIR *pDir = NULL;

  if(!pFile || pFile->filename[0] == '\0') {
    return -1;
  }

  if(!overWriteOutput &&
     (pFile->fp = fileops_Open(pFile->filename, O_RDONLY)) != FILEOPS_INVALID_FP) {
    LOG(X_ERROR("File %s already exists.  Will not overwrite."), pFile->filename);
    CloseMediaFile(pFile);
    return -1;
  }

  if(outDir) {
    if((pDir = fileops_OpenDir(outDir)) == NULL) {
      if(fileops_MkDir(outDir, 0770) < 0) {
        LOG(X_ERROR("Unable to create directory %s"), outDir);
        return -1;
      }
    } else {
      fileops_CloseDir(pDir);
    }
  }

  if((pFile->fp = fileops_Open(pFile->filename, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_CRITICAL("Unable to open file for writing: %s"), pFile->filename);
    return -1;
  }

  pFile->offset = 0;
  //LOG(X_DEBUG("Created '%s'"), pFile->filename);

  return 0;
}

void capture_freeFilters(CAPTURE_FILTER_T pFilters[], unsigned int numFilters) {
  unsigned int idx = 0;

  if(!pFilters) {
    return;
  }
  for(idx = 0; idx < numFilters; idx++) {
    //TODO: free any dynamically allocated sequence headers
  } 

}

static CAPTURE_FILTER_PROTOCOL_T sdpTransTypeToCaptureProto(SDP_TRANS_TYPE_T transType) {

    switch(transType) {
      case SDP_TRANS_TYPE_RAW_UDP:
        return CAPTURE_FILTER_TRANSPORT_UDPRAW;
      case SDP_TRANS_TYPE_SRTP_SDES_UDP:
        return CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES;
      case SDP_TRANS_TYPE_SRTP_DTLS_UDP:
        return CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP;
      case SDP_TRANS_TYPE_DTLS_UDP:
        return CAPTURE_FILTER_TRANSPORT_UDPDTLS;
      default: 
        return CAPTURE_FILTER_TRANSPORT_UDPRTP;
    }
}

static int init_srtp(const SDP_STREAM_SRTP_T *pSdp, SRTP_CTXT_T *pSrtp) {

#if defined(VSX_HAVE_SRTP)

  if(pSdp->tag[0] == '\0') {
    return -1;
  }

  memcpy(&pSrtp->kt.type, &pSdp->type, sizeof(pSrtp->kt.type));
  pSrtp->do_rtcp = 1;
  pSrtp->do_rtcp_responder = 1;

  if(srtp_loadKeys(pSdp->keyBase64, pSrtp, NULL) < 0) {
    return -1;
  }

#else // (VSX_HAVE_SRTP)
  LOG(X_ERROR(VSX_SRTP_DISABLED_BANNER));
  LOG(X_ERROR(VSX_FEATURE_DISABLED_BANNER));
  return -1;
#endif // (VSX_HAVE_SRTP)

  return 0;
}

int capture_filterFromSdp(const char *sdppath,
                          CAPTURE_FILTER_T pFilters[], 
                          char *listenAddrOut, 
                          unsigned int listenAddrLen, 
                          SDP_DESCR_T *pSdp,
                          int novid,
                          int noaud) {
  int rc = 0;
  int sz;
  SDP_DESCR_T sdp;
  const uint16_t *p_port1 = NULL;
  const uint16_t *p_port2 = NULL;
  //struct in_addr addr;
  struct sockaddr_storage addr;
  char tmp[128];
  unsigned int numports = 0;

  if(!listenAddrOut || !pFilters || (!sdppath && !pSdp)) {
    return -1;
  }

  if(!pSdp) {
    pSdp = &sdp;
  }

  //
  // If sdp input path is present then load the contents,
  // otherwise just init the filters from the sdp argument
  //
  if(sdppath) {
    rc = -1;

    if(!strncasecmp(sdppath, "sdp://", 6)) {
      if((sz = strlen(sdppath)) > 6) {
        rc = sdp_parse(pSdp, &sdppath[6], sz - 6, 1, 0);
      }
    } else {
      rc = sdp_load(pSdp, sdppath);
    }

    if(rc < 0) {
      return rc;
    }
  }

  if(!pSdp->vid.common.available && !pSdp->aud.common.available) {
    LOG(X_ERROR("SDP %s does not contain any valid video or audio sources"), sdppath);
    return -1;
  }

  if(!novid && pSdp->vid.common.port > 0) {

    pFilters[numports].transType = sdpTransTypeToCaptureProto(pSdp->vid.common.transType);

    //
    // Set the SRTP key from the receiver
    //
    if(pFilters[numports].transType == CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES &&
       init_srtp(&pSdp->vid.common.srtp, &pFilters[numports].srtps[0]) < 0) {
       LOG(X_ERROR("Invalid video SRTP base64 key from SDP '%s'"), pSdp->vid.common.srtp.keyBase64);
      return -1;
    }

    //
    // Set the DTLS fingerprint 
    //
    if(pSdp->vid.common.fingerprint.len > 0) {
      pFilters[numports].dtlsFingerprintVerify.verified = 0;
      memcpy(&pFilters[numports].dtlsFingerprintVerify.fingerprint, &pSdp->vid.common.fingerprint, 
             sizeof(pFilters[numports].dtlsFingerprintVerify.fingerprint));
    }

    //
    // Set any ICE ice-ufrag, ice-pwd credentials (for STUN / TURN) from SDP
    //
    strncpy(pFilters[numports].stun.store.ufragbuf, pSdp->vid.common.ice.ufrag, STUN_STRING_MAX - 1);
    strncpy(pFilters[numports].stun.store.pwdbuf, pSdp->vid.common.ice.pwd, STUN_STRING_MAX - 1);

    p_port1 = &pSdp->vid.common.port;
    pFilters[numports].mediaType = pSdp->vid.common.codecType;
    pFilters[numports].dstPort = pSdp->vid.common.port;
    pFilters[numports].dstPortRtcp = pSdp->vid.common.portRtcp;
    if(pFilters[numports].dstPort == pFilters[numports].dstPortRtcp) {
      pFilters[numports].payloadType =  pSdp->vid.common.payloadType;
      pFilters[numports].haveRtpPtFilter = 1;
    }
    memcpy(&pFilters[numports].u_seqhdrs.vid, &pSdp->vid, sizeof(pSdp->vid));
    numports++;
    //fprintf(stderr, "VIDEO SDP RTCP flags: 0x%x\n", pSdp->vid.common.rtcpfb.flags);
  }

  if(!noaud && pSdp->aud.common.port > 0) {

    pFilters[numports].transType = sdpTransTypeToCaptureProto(pSdp->aud.common.transType);

    //
    // Set the SRTP key from the receiver
    //
    if(pFilters[numports].transType == CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES &&
       init_srtp(&pSdp->aud.common.srtp, &pFilters[numports].srtps[0]) < 0) {
       LOG(X_ERROR("Invalid audio SRTP base64 key from SDP '%s'"), pSdp->aud.common.srtp.keyBase64);
      return -1;
    }

    //
    // Set the DTLS fingerprint 
    //
    if(pSdp->aud.common.fingerprint.len > 0) {
      pFilters[numports].dtlsFingerprintVerify.verified = 0;
      memcpy(&pFilters[numports].dtlsFingerprintVerify.fingerprint, &pSdp->aud.common.fingerprint,
             sizeof(pFilters[numports].dtlsFingerprintVerify.fingerprint));
    }

    //
    // Set any ICE ice-ufrag, ice-pwd credentials (for STUN / TURN) from SDP
    //
    strncpy(pFilters[numports].stun.store.ufragbuf, pSdp->aud.common.ice.ufrag, STUN_STRING_MAX - 1);
    strncpy(pFilters[numports].stun.store.pwdbuf, pSdp->aud.common.ice.pwd, STUN_STRING_MAX - 1);

    if(numports > 0) {
      p_port2 = &pSdp->aud.common.port;
    } else {
      p_port1 = &pSdp->aud.common.port;
    }
    pFilters[numports].mediaType = pSdp->aud.common.codecType;
    pFilters[numports].dstPort = pSdp->aud.common.port;
    pFilters[numports].dstPortRtcp = pSdp->aud.common.portRtcp;

    //
    // Assign a payload type filter - if audio & video are using the same port
    //
    if(pSdp->vid.common.port > 0 && pSdp->vid.common.port == pSdp->aud.common.port) {
      pFilters[numports -1].payloadType =  pSdp->vid.common.payloadType;
      pFilters[numports -1].haveRtpPtFilter = 1;
      pFilters[numports].payloadType =  pSdp->aud.common.payloadType;
      pFilters[numports].haveRtpPtFilter = 1;
    }
    memcpy(&pFilters[numports].u_seqhdrs.aud, &pSdp->aud, sizeof(pSdp->aud));

    //
    // Store any telephone-event properties for RFC2833 / DTMF
    //
    if(pSdp->aud.aux.available) {
      memcpy(&(pFilters[numports].telephoneEvt), &pSdp->aud.aux, sizeof(pFilters[numports].telephoneEvt));
    }

    numports++;
  }

  if(numports > 1){
    pFilters[1].transType = pFilters[0].transType;

    //
    // Ensure that if mpeg2-ts is used as the input protocol,
    // only 1 filer is returned, and no matching of mp2ts / rtp specific proto exists
    //
    if((pFilters[0].mediaType == CAPTURE_FILTER_PROTOCOL_MP2TS ||
        pFilters[0].mediaType == CAPTURE_FILTER_PROTOCOL_MP2TS_BDAV) &&
       (pFilters[1].mediaType == CAPTURE_FILTER_PROTOCOL_MP2TS ||
        pFilters[1].mediaType == CAPTURE_FILTER_PROTOCOL_MP2TS_BDAV)) {
      LOG(X_ERROR("Multiple SDP mpeg2-ts listening ports not supported"));
      return -1;
    }
  }

  memset(&addr, 0, sizeof(addr));
  addr.ss_family = pSdp->c.ip_family;

  //
  // If the 'connect' address is not multicast, and not localhost, set it to INADDR_ANY
  // This prohibits binding to a specific interface based on address under the assumption that the 
  // 'c' field comes from a remote SDP with an address that is not appropriate on the local system.
  //
  if((1 != INET_PTON(pSdp->c.iphost, addr) || 
      (addr.ss_family != AF_INET6 && ((struct sockaddr_in *)&addr)->sin_addr.s_addr == INADDR_NONE)) ||
      (!INET_IS_MULTICAST(addr) && !INET_ADDR_LOCALHOST(addr))) {

    if(addr.ss_family == AF_INET6) {
      memset(&((struct sockaddr_in6 *) &addr)->sin6_addr.s6_addr[0], 0, ADDR_LEN_IPV6);
    } else {
      ((struct sockaddr_in *) &addr)->sin_addr.s_addr = INADDR_ANY;
    }
  }

  sz = snprintf(listenAddrOut, listenAddrLen, "%s%s%s", 
               addr.ss_family == AF_INET6 ? "[" : "", INET_NTOP(addr, tmp, sizeof(tmp)),
               addr.ss_family == AF_INET6 ? "]" : "");

  if(p_port1) {
    if((rc = snprintf(&listenAddrOut[sz], listenAddrLen - sz, ":%d", *p_port1)) > 0) {
      sz += rc;
    }
    if(p_port2) {
      if((rc = snprintf(&listenAddrOut[sz], listenAddrLen - sz, ",%d", *p_port2)) > 0) {
        sz += rc;
      }
    }
  }

  //fprintf(stderr, "SDP transType:%d, %d\n", pFilters[0].transType, pFilters[1].transType);

  //avc_dumpHex(stderr, pFilters[0].u_seqhdrs.vid.u.mpg4v.seqHdrs.pHdrs, pFilters[0].u_seqhdrs.vid.u.mpg4v.seqHdrs.hdrsLen, 0);
  //fprintf(stderr, "codecType:%d pps_len:%d\n", pFilters[0].mediaType, pFilters[0].u_seqhdrs.vid.u.h264.spspps.pps_len);
  //LOG(X_DEBUG("mt[0]:%d, port[0]:%d, mt[1]:%d, port[1]:%d, vid.common.port:%d, aud.common.port: %d"), pFilters[0].mediaType, pFilters[0].dstPort, pFilters[1].mediaType, pFilters[1].dstPort, pSdp->vid.common.port, pSdp->aud.common.port);

  return numports;
}



