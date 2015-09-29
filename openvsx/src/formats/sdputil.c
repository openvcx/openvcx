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


int sdputil_initsrtp(SDP_STREAM_SRTP_T *pSdp, const SRTP_CTXT_T *pSrtp) {

  if(!pSdp || !pSrtp) {
    return -1;
  }

  snprintf(pSdp->tag, sizeof(pSdp->tag), "%d", 1);
  memcpy(&pSdp->type, &pSrtp->kt.type, sizeof(pSdp->type));
  if(base64_encode(pSrtp->kt.k.key, pSrtp->kt.k.lenKey, pSdp->keyBase64, sizeof(pSdp->keyBase64)) < 0) {
    LOG(X_ERROR("Failed to base64 encode key length %d"), pSrtp->kt.k.lenKey);
    return -1;
  }

  return 0;
}

int sdputil_init(SDP_DESCR_T *pSdp,
                 uint8_t payloadType,
                 unsigned int clockRateHz,
                 XC_CODEC_TYPE_T codecType,
                 const char *pDstHost,
                 uint16_t dstPort,
                 uint16_t dstPortRtcp,
                 const SRTP_CTXT_T *pSrtp,
                 const DTLS_CFG_T *pDtlsCfg,
                 const STUN_REQUESTOR_CFG_T *pStunCfg,
                 const SDP_CODEC_PARAM_T *pCodecSpecific,
                 const FRAME_RATE_T *pFps,
                 const VID_ENCODER_FBREQUEST_T *pFbReq) {

  int rc = 0;
  char tmp[128];
  struct sockaddr_storage connectip;

  if(!pSdp || payloadType > 0x7f || !pDstHost) {
    return -1;
  }

  memset(&connectip, 0, sizeof(connectip));
  if(!net_isipv4(pDstHost) && !net_isipv6(pDstHost)) {
    //
    // INADDR_NONE
    //
    pDstHost = "0.0.0.0";
  }

  net_getaddress(pDstHost, &connectip);
  if(INET_ADDR_VALID(connectip) && INET_IS_MULTICAST(connectip)) {
    pSdp->c.ttl = 64;
  } else if(INET_ADDR_VALID(connectip) && INET_ADDR_LOCALHOST(connectip)) {
    pSdp->c.ttl = 0;
  } else {
    //connectip.s_addr = net_getlocalip();

    //
    // For remote unicast destinations, leave the remote ip into the 'c=' field
    // which may be contrary to RFC4566
    //
    //connectip.s_addr = INADDR_ANY;

    pSdp->c.ttl = 0;
  }

  pSdp->c.ip_family = connectip.ss_family;
  strncpy(pSdp->c.iphost, INET_NTOP(connectip, tmp, sizeof(tmp)), sizeof(pSdp->c.iphost));

  //
  // Only write the RTCP port attribute in the SDP if using a non-default port
  //
  if(dstPortRtcp == RTCP_PORT_FROM_RTP(dstPort)) {
    dstPortRtcp = 0;   
  } 

  if(pFps && pFps->clockHz > 0 && pFps->frameDeltaHz > 0) {
    memcpy(&pSdp->vid.fps, pFps, sizeof(pSdp->vid.fps));
  }
  switch(codecType) {

    case XC_CODEC_TYPE_H264:
    case XC_CODEC_TYPE_MPEG4V:
    case XC_CODEC_TYPE_H263:
    case XC_CODEC_TYPE_H263_PLUS:
    case XC_CODEC_TYPE_VP8:
      pSdp->vid.common.available = 1;
      pSdp->vid.common.codecType = codecType;
      pSdp->vid.common.payloadType = payloadType;
      pSdp->vid.common.port = dstPort;
      pSdp->vid.common.portRtcp = dstPortRtcp;
      pSdp->vid.common.clockHz = clockRateHz;
      pSdp->vid.common.transType = SDP_TRANS_TYPE_RTP_UDP;

      if(pDtlsCfg) {
        if(pDtlsCfg->dtls_srtp) {
          pSdp->vid.common.transType = SDP_TRANS_TYPE_SRTP_DTLS_UDP;
        } else {
          pSdp->vid.common.transType = SDP_TRANS_TYPE_DTLS_UDP;
        }
        memcpy(&pSdp->vid.common.fingerprint, &pDtlsCfg->fingerprint, sizeof(pSdp->vid.common.fingerprint));
      }
      if(pSrtp) {
        pSdp->vid.common.transType =  SDP_TRANS_TYPE_SRTP_SDES_UDP;
        if((rc = sdputil_initsrtp(&pSdp->vid.common.srtp, pSrtp)) < 0) {
          return rc;
        }
      }
      if(pStunCfg && pStunCfg->bindingRequest) {
        if(pStunCfg->reqUsername) {
          strncpy(pSdp->vid.common.ice.ufrag, pStunCfg->reqUsername, STUN_STRING_MAX - 1);
        }
        if(pStunCfg->reqPass) {
          strncpy(pSdp->vid.common.ice.pwd, pStunCfg->reqPass, STUN_STRING_MAX - 1);
        }
      }
      
      if(codecType == XC_CODEC_TYPE_H264) {
        strncpy(pSdp->vid.common.encodingName, SDP_RTPMAP_ENCODINGNAME_H264,
          sizeof(pSdp->vid.common.encodingName));
      } else if(codecType == XC_CODEC_TYPE_MPEG4V) {
        strncpy(pSdp->vid.common.encodingName, SDP_RTPMAP_ENCODINGNAME_MPEG4V,
          sizeof(pSdp->vid.common.encodingName));
      } else if(codecType == XC_CODEC_TYPE_H263) {
        strncpy(pSdp->vid.common.encodingName, SDP_RTPMAP_ENCODINGNAME_H263,
          sizeof(pSdp->vid.common.encodingName));
      } else if(codecType == XC_CODEC_TYPE_H263_PLUS) {
        strncpy(pSdp->vid.common.encodingName, SDP_RTPMAP_ENCODINGNAME_H263_PLUS,
          sizeof(pSdp->vid.common.encodingName));
      } else if(codecType == XC_CODEC_TYPE_VP8) {
        strncpy(pSdp->vid.common.encodingName, SDP_RTPMAP_ENCODINGNAME_VP8,
          sizeof(pSdp->vid.common.encodingName));
      }

      break;
    case XC_CODEC_TYPE_AAC:
    case XC_CODEC_TYPE_AMRNB:
    case XC_CODEC_TYPE_SILK:
    case XC_CODEC_TYPE_OPUS:
    case XC_CODEC_TYPE_G711_MULAW:
    case XC_CODEC_TYPE_G711_ALAW:
      pSdp->aud.common.available = 1;
      pSdp->aud.common.codecType = codecType;
      pSdp->aud.common.payloadType = payloadType;
      pSdp->aud.common.port = dstPort;
      pSdp->aud.common.portRtcp = dstPortRtcp;
      pSdp->aud.common.clockHz = clockRateHz;
      pSdp->aud.common.transType = SDP_TRANS_TYPE_RTP_UDP;

      if(pDtlsCfg) {
        if(pDtlsCfg->dtls_srtp) {
          pSdp->aud.common.transType = SDP_TRANS_TYPE_SRTP_DTLS_UDP;
        } else {
          pSdp->aud.common.transType = SDP_TRANS_TYPE_DTLS_UDP;
        }
        memcpy(&pSdp->aud.common.fingerprint, &pDtlsCfg->fingerprint, sizeof(pSdp->aud.common.fingerprint));
      }
      if(pSrtp) {
        pSdp->aud.common.transType =  SDP_TRANS_TYPE_SRTP_SDES_UDP;
        if((rc = sdputil_initsrtp(&pSdp->aud.common.srtp, pSrtp)) < 0) {
          return rc;
        }
      }

      if(pStunCfg && pStunCfg->bindingRequest) { 
        if(pStunCfg->reqUsername) {
          strncpy(pSdp->aud.common.ice.ufrag, pStunCfg->reqUsername, STUN_STRING_MAX - 1);
        }
        if(pStunCfg->reqPass) {
          strncpy(pSdp->aud.common.ice.pwd, pStunCfg->reqPass, STUN_STRING_MAX - 1);
        }
      }

      if(codecType == XC_CODEC_TYPE_AAC) {
        strncpy(pSdp->aud.common.encodingName, SDP_RTPMAP_ENCODINGNAME_AAC,
          sizeof(pSdp->aud.common.encodingName));
      } else if(codecType == XC_CODEC_TYPE_AMRNB) {
        strncpy(pSdp->aud.common.encodingName, SDP_RTPMAP_ENCODINGNAME_AMR,
          sizeof(pSdp->aud.common.encodingName));
      } else if(codecType == XC_CODEC_TYPE_SILK) {
        strncpy(pSdp->aud.common.encodingName, SDP_RTPMAP_ENCODINGNAME_SILK,
          sizeof(pSdp->aud.common.encodingName));
      } else if(codecType == XC_CODEC_TYPE_OPUS) {
        strncpy(pSdp->aud.common.encodingName, SDP_RTPMAP_ENCODINGNAME_OPUS,
          sizeof(pSdp->aud.common.encodingName));
      } else if(codecType == XC_CODEC_TYPE_G711_MULAW) {
        strncpy(pSdp->aud.common.encodingName, SDP_RTPMAP_ENCODINGNAME_PCMU,
          sizeof(pSdp->aud.common.encodingName));
      } else if(codecType == XC_CODEC_TYPE_G711_ALAW) {
        strncpy(pSdp->aud.common.encodingName, SDP_RTPMAP_ENCODINGNAME_PCMA,
          sizeof(pSdp->aud.common.encodingName));
      }

      break;
    case MEDIA_FILE_TYPE_MP2TS:
      pSdp->vid.common.available = 1;
      pSdp->vid.common.codecType = MEDIA_FILE_TYPE_MP2TS;
      pSdp->vid.common.payloadType = payloadType;
      pSdp->vid.common.port = dstPort;
      pSdp->vid.common.portRtcp = dstPortRtcp;
      pSdp->vid.common.clockHz = 90000;

      if(pSrtp) {
        pSdp->vid.common.transType =  SDP_TRANS_TYPE_SRTP_SDES_UDP;
        if((rc = sdputil_initsrtp(&pSdp->vid.common.srtp, pSrtp)) < 0) {
          return rc;
        }
      }

      strncpy(pSdp->vid.common.encodingName, SDP_RTPMAP_ENCODINGNAME_MP2TS,
          sizeof(pSdp->vid.common.encodingName));
      break;
    default:
      return -1;
  }
 
  //
  // Advertise any a=rtcp-fb:  SDP flags
  //
  if(pSdp->vid.common.available) {
   if(pFbReq && (pFbReq->firCfg.fir_send_from_decoder || pFbReq->firCfg.fir_send_from_local ||
                 pFbReq->firCfg.fir_send_from_remote || pFbReq->firCfg.fir_send_from_capture)) {
      pSdp->vid.common.rtcpfb.fmtidmin1 = pSdp->vid.common.payloadType + 1;
      pSdp->vid.common.rtcpfb.flags |= SDP_RTCPFB_TYPE_CCM | SDP_RTCPFB_TYPE_CCM_FIR;
    }
   if(pFbReq && pFbReq->nackRtpRetransmit) {
    pSdp->vid.common.rtcpfb.flags |= SDP_RTCPFB_TYPE_NACK | SDP_RTCPFB_TYPE_NACK_GENERIC;
   }
    //pSdp->vid.common.rtcpfb.flags |= SDP_RTCPFB_TYPE_TRRINT;
    //pSdp->vid.common.rtcpfb.trrIntervalMs |= 30;
  }

  //
  // Codec specific default settings
  //
  switch(codecType) {

    case XC_CODEC_TYPE_H264:

      if(pCodecSpecific && (pCodecSpecific->flags & SDP_CODEC_PARAM_FLAGS_PKTZMODE)) {

        switch(pCodecSpecific->u.pktzMode) {
          case PKTZ_H264_MODE_0:
            pSdp->vid.u.h264.packetization_mode = 0;
            break;
          case PKTZ_H264_MODE_2:
            LOG(X_WARNING("H.264 NAL Packetization mode 2 not supported.  Using mode 1")); 
          case PKTZ_H264_MODE_1:
          case PKTZ_H264_MODE_NOTSET:
          default:
            pSdp->vid.u.h264.packetization_mode = 1;
            break;
       }

     }

     break;

    case XC_CODEC_TYPE_MPEG4V:

      pSdp->vid.u.mpg4v.profile_level_id = 1;
      break;

    case XC_CODEC_TYPE_VP8:
      break;

    case XC_CODEC_TYPE_AAC:
      strncpy(pSdp->aud.u.aac.mode, "AAC-hbr", sizeof(pSdp->aud.u.aac.mode));
      pSdp->aud.u.aac.sizelength = 13;
      pSdp->aud.u.aac.indexlength = 3;
      pSdp->aud.u.aac.indexdeltalength = 3;
      break;

    case XC_CODEC_TYPE_AMRNB:

      pSdp->aud.channels = 1;
      pSdp->aud.u.amr.octet_align = 1;

      break;

    case XC_CODEC_TYPE_SILK:

      if(pCodecSpecific && (pCodecSpecific->flags & SDP_CODEC_PARAM_FLAGS_CHANNELS)) {
        pSdp->aud.channels = pCodecSpecific->u.channels;
      } else {
        pSdp->aud.channels = 1;
      }

      //pSdp->aud.u.silk.dummy = 0;

      break;

    case XC_CODEC_TYPE_OPUS:

      if(pCodecSpecific && (pCodecSpecific->flags & SDP_CODEC_PARAM_FLAGS_CHANNELS)) {
        pSdp->aud.channels = pCodecSpecific->u.channels;;
      } else {
        pSdp->aud.channels = 1;
      }

      break;

    case XC_CODEC_TYPE_G711_MULAW:
    case XC_CODEC_TYPE_G711_ALAW:

      pSdp->aud.channels = 1;
      break;

    default:
      break;
  }

  return rc;
}

int sdputil_createSdp(STREAMER_CFG_T *pCfg, SDP_DESCR_T *pSdp) {
  int rc = 0;
  unsigned char header[256];
  unsigned int headerLen = sizeof(header);
  STREAM_XCODE_CTXT_T xcodeCtxt;
  unsigned int outidx = 0;
  SDP_CODEC_PARAM_T codecParam;
  union {
    SPSPPS_RAW_T spspps;
  } uvidhdr;


  if(!pCfg || !pSdp) {
    return -1;
  }

  memset(pSdp, 0, sizeof(SDP_DESCR_T));
  memset(&uvidhdr, 0, sizeof(uvidhdr));
  memset(&xcodeCtxt, 0, sizeof(xcodeCtxt));
  xcodeCtxt.vidData.piXcode = &pCfg->xcode;


  if(!pCfg->novid && pCfg->xcode.vid.common.cfgDo_xcode) {
    codecParam.u.pktzMode = PKTZ_H264_MODE_NOTSET;

    if(sdputil_init(pSdp,
               codecType_getRtpPT(pCfg->xcode.vid.common.cfgFileTypeOut, pCfg->cfgrtp.payloadTypesMin1),
               90000,  // TODO: may vary
               pCfg->xcode.vid.common.cfgFileTypeOut,
               pCfg->pdestsCfg[0].dstHost,
               pCfg->pdestsCfg[0].ports[0],  
               pCfg->pdestsCfg[0].portsRtcp[0],
               NULL,
               NULL,
               NULL,
               &codecParam,
               NULL,
               &pCfg->fbReq) < 0) {
      return -1;
    }

    if(!pCfg->xcode.vid.pUserData) {
      pCfg->xcode.vid.pUserData = &xcodeCtxt.vidUData;
    }

    rc = xcode_getvidheader(&xcodeCtxt.vidData, outidx, header, &headerLen);

    if(pCfg->xcode.vid.pUserData == &xcodeCtxt.vidUData) {
      pCfg->xcode.vid.pUserData = NULL;
    }

    if(rc == 0) {
      switch(pCfg->xcode.vid.common.cfgFileTypeOut) {
        case XC_CODEC_TYPE_H264:
          //TODO: this should be xcoder output index specific
          if((rc = xcode_h264_find_spspps(&xcodeCtxt.vidUData.out[outidx].uout.xh264,
                   header, headerLen, &uvidhdr.spspps)) < 0) {
            return rc;
          }
          if(uvidhdr.spspps.sps_len <= 0) {
            LOG(X_WARNING("H.264 SPS not available"));
            return -1;
          }
          memcpy(pSdp->vid.u.h264.profile_level_id, &uvidhdr.spspps.sps[1], 3);
          memcpy(&pSdp->vid.u.h264.spspps, &uvidhdr.spspps, sizeof(SPSPPS_RAW_T));

          break;
        default:
          break; 
      }


    }

  }


  return rc;
}
































