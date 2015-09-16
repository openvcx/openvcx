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


typedef struct CAP_HTTP_FLV {
  CAP_HTTP_COMMON_T     common;
  RTMP_CTXT_CLIENT_T    client;
  FLV_HDR_T             flvhdr; 
  FLV_TAG_HDR_T         taghdr;
  CAPTURE_STREAM_T     *pStream;
} CAP_HTTP_FLV_T;

//static int g_fd;

static unsigned char *read_net_flv(CAP_HTTP_FLV_T *pFlv, unsigned int szToRead) {
  unsigned char *p;

  p = http_read_net(&pFlv->common, 
                  &pFlv->common.pCfg->pSockList->netsockets[0],
                  (const struct sockaddr *) &pFlv->common.pCfg->pSockList->salist[0],
                  szToRead, 
                  pFlv->client.ctxt.in.buf, 
                  pFlv->client.ctxt.in.sz, 
                  &pFlv->client.ctxt.bytesRead);

  if(pFlv->pStream) {
    pFlv->pStream->numBytes = pFlv->client.ctxt.bytesRead;
  }

  return p;
}

static void http_flv_close(CAP_HTTP_FLV_T *pState) {

  rtmp_close(&pState->client.ctxt);

}

static int http_flv_init(CAP_HTTP_FLV_T *pState) {
  int rc = 0;

  if((rc = rtmp_parse_init(&pState->client.ctxt, RTMP_TMPFRAME_VID_SZ, 0)) < 0) {
    return rc;
  }

  if((rc = codecfmt_init(&pState->client.ctxt.av, RTMP_TMPFRAME_VID_SZ, RTMP_TMPFRAME_AUD_SZ)) < 0) {
    http_flv_close(pState);
    return rc;
  }

  //
  //TODO: if "flv://" or --filter="type=flv" is omitted, only 1 (m2t) queue is created
  // in capture_net.c::stream_capture, which is not suitable for this... that code should be extracted
  // into a module which can be invoked from here once we discover the capture has CONTENT_TYPE FLV
  //
  if(pState->common.pCfg->pcommon->filt.filters[0].pCapAction) {
    pState->client.pQVid = pState->common.pCfg->pcommon->filt.filters[0].pCapAction->pQueue;
  }
  if(pState->common.pCfg->pcommon->filt.filters[1].pCapAction) {
    pState->client.pQAud = pState->common.pCfg->pcommon->filt.filters[1].pCapAction->pQueue;
  }

  //fprintf(stderr, "VID PQ maxPkts:%d, maxPktLen:%d, grow:%d,%d\n", pState->client.pQVid->cfg.maxPkts, pState->client.pQVid->cfg.maxPktLen, pState->client.pQVid->cfg.growMaxPkts, pState->client.pQVid->cfg.growMaxPktLen);

  return rc;
}

int http_flv_recvloop(CAP_ASYNC_DESCR_T *pCfg,
                  unsigned int offset,
                  int sz, 
                  FILE_OFFSET_T contentLen,
                  unsigned char *pbuf,
                  unsigned int szbuf,
                  CAPTURE_STREAM_T *pStream) {

  CAP_HTTP_FLV_T ctxt;
  FLV_TAG_HDR_T *pTagHdr;
  unsigned char *pdata;
  unsigned int ts0 = 0;
  unsigned int ts;
  unsigned int tsoffset = 0;
  int havets0 = 0;
  TIME_VAL tm0, tm;
  unsigned int msElapsed;
  int do_sleep;
  int rc;

  memset(&ctxt, 0, sizeof(ctxt));
  ctxt.common.pCfg = pCfg;
  ctxt.common.pdata = pbuf;
  ctxt.common.dataoffset = offset;
  ctxt.common.datasz = offset + sz;
  ctxt.pStream = pStream;

  //if(sz > 0) {
  //  LOG(X_WARNING("Partial FLV HTTP Content-Header read offset %d not implemented. (offset:%d)"), sz, offset);
  //}

  if(net_setsocknonblock(NETIOSOCK_FD(pCfg->pSockList->netsockets[0]), 1) < 0) {
    return -1;
  }

  if(http_flv_init(&ctxt) < 0) {
    return -1;
  }

  //
  // Read 'FLV' 9 byte header
  //
  if(!(pdata = read_net_flv(&ctxt, 9))) {
    http_flv_close(&ctxt);
    return -1;
  }

  memcpy(&ctxt.flvhdr, pdata, 9);

  if(!(ctxt.flvhdr.type[0] == 'F' && ctxt.flvhdr.type[1] == 'L' && 
       ctxt.flvhdr.type[2] == 'V' && ctxt.flvhdr.ver == FLV_HDR_VER_1)) {
    LOG(X_ERROR("Invalid HTTP FLV start sequence 0x%x 0x%x 0x%x 0x%x"),
               pdata[0], pdata[1], pdata[2], pdata[3]); 
    http_flv_close(&ctxt);
    return -1;
  }

  if((FLV_HDR_HAVE_VIDEO((&ctxt.flvhdr)) && !ctxt.client.pQVid) ||
     (FLV_HDR_HAVE_AUDIO((&ctxt.flvhdr)) && !ctxt.client.pQAud)) {

    LOG(X_ERROR("FLV capture input queue(s) not propery configured: video: %d, audio: %d"),
                ctxt.client.pQVid ? 1 : 0, ctxt.client.pQAud ? 1 : 0);
    http_flv_close(&ctxt);
    return -1;
  }

  tm0 = timer_GetTime();

  if(pCfg->pcommon->caprealtime) {
    LOG(X_DEBUG("Treating capture as real-time."));
  }

  do {

    if(!(pTagHdr = (FLV_TAG_HDR_T *) read_net_flv(&ctxt, FLV_TAG_HDR_SZ))) {
      return -1;
    }

    ctxt.taghdr.szprev = htonl(pTagHdr->szprev);
    ctxt.taghdr.type = pTagHdr->type;
    memcpy(&ctxt.taghdr.size32, &pTagHdr->size, 3);
    ctxt.taghdr.size32 = htonl(ctxt.taghdr.size32 << 8);
    memcpy(&ctxt.taghdr.timestamp32, &pTagHdr->timestamp, 3);
    ctxt.taghdr.timestamp32 = htonl(ctxt.taghdr.timestamp32 << 8);
    ctxt.taghdr.timestamp32 |= (pTagHdr->tsext << 24);

    //
    // For live content, initial timestamp may not always begin at 0
    //
    if(!havets0) {
      if((ts0 = ctxt.taghdr.timestamp32) > 0) {
        havets0 = 1;
      }
    } else if(ctxt.taghdr.timestamp32 < ts0) {
      LOG(X_WARNING("HTTP FLV timestamp anamoly ts:%u, initial ts:%u (offset:%u)"),
          ctxt.taghdr.timestamp32, ts0, tsoffset);
      tsoffset += ts0;
      ts0 = ctxt.taghdr.timestamp32;
    }
    ts = ctxt.taghdr.timestamp32 - ts0 + tsoffset;

    //fprintf(stderr, "flv hdr type:0x%x sz:%d ts:%u(ts0:%u+%u) pkt_ts:%u\n", ctxt.taghdr.type, ctxt.taghdr.size32, ts, ts0, tsoffset, ctxt.taghdr.timestamp32);

    if(!(pdata = read_net_flv(&ctxt, ctxt.taghdr.size32))) {
      return -1;
    }

    //
    // For non-live content, throttle download rate inline with timestamps for real-time
    // restreaming
    //
    if(pCfg->pcommon->caprealtime) {
      do {
        do_sleep = 0;
        tm = timer_GetTime();
        msElapsed = (unsigned int) (tm - tm0) / TIME_VAL_MS;
//fprintf(stderr, "msElapsed:%d  ts:%d %lld %lld\n", msElapsed, ctxt.taghdr.timestamp32, tm, tm0);
        if(msElapsed < ts) {
          if((do_sleep = ts - msElapsed) > 500) {
            do_sleep = 500;
          }
//fprintf(stderr, "zzzz %d ms\n", do_sleep);
          usleep(do_sleep * 1000);
        }
      } while(do_sleep);
    }

    rc = 0;
    switch(ctxt.taghdr.type) {
      case FLV_TAG_VIDEODATA: 
        rc = rtmp_handle_vidpkt(&ctxt.client, pdata, ctxt.taghdr.size32, ts);
        break;
      case FLV_TAG_AUDIODATA: 
        rc = rtmp_handle_audpkt(&ctxt.client, pdata, ctxt.taghdr.size32, ts); 
      case FLV_TAG_SCRIPTDATA: 

        break;
      default:
        rc = -1;
        break;
    }

    if(rc < 0) {
      LOG(X_ERROR("Failed to process FLV packet type: 0x%x, len:%d"), 
                  ctxt.taghdr.type, ctxt.taghdr.size32);
      break;
    }


  } while(pCfg->running == 0 && 
          (contentLen == 0 || ctxt.client.ctxt.bytesRead < contentLen) &&
         !g_proc_exit);

  http_flv_close(&ctxt);

  return ctxt.client.ctxt.bytesRead;
}

#endif // VSX_HAVE_CAPTURE
