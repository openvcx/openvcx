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

//#define BITRATE_RESTRICT 1

static int resp_sendmediafile(SOCKET_DESCR_T *pSd, HTTP_REQ_T *pReq,
                               HTTP_MEDIA_STREAM_T *pMedia,  
                               STREAM_STATS_T *pStats, 
                               float throttleMultiplier,
                               float prebufSec) { 
  int rc = 0;
  FILE_OFFSET_T contentLen = 0;
  FILE_OFFSET_T idxFile = 0;
  FILE_OFFSET_T idxContent = 0;
  FILE_OFFSET_T idxTmp;
  unsigned int lenread;
  HTTP_RANGE_HDR_T reqRange;
  const char *pRangeHdr;
  double throttleRate = 0;
  struct timeval tv0;
  struct timeval tv;
  double secElapsed = 0;
  FILE_OFFSET_T prebufIdx = 0;
  enum HTTP_STATUS statusCode = HTTP_STATUS_OK;
  char etagbuf[64];
  char tmp[128];
  const char *etag = NULL;
  const char *pHdr;
  unsigned char buf[4096];

//struct timeval tvP;
//FILE_OFFSET_T idx0=0;

  if(!pMedia || !pMedia->pFileStream || pMedia->pFileStream->fp == FILEOPS_INVALID_FP) {
    http_resp_send(pSd, pReq, HTTP_STATUS_NOTFOUND, NULL, 0);
    return -1;
  }

  if(pMedia->pFileStream->offset > 0) {
    if(SeekMediaFile(pMedia->pFileStream, 0, SEEK_SET) < 0) {
      http_resp_send(pSd, pReq, HTTP_STATUS_SERVERERROR, NULL, 0);
      return -1;
    }
  }

  if(throttleMultiplier > 0 && pMedia->media.durationSec == 0) {
    LOG(X_WARNING("Throttling %s not available since media duration is not known"), 
       pMedia->pFileStream->filename);
  }

  //fprintf(stderr, "http_resp_sendmediafile throttleMultiplier: %.3f, prebufSec: %.3f, media duration:%.1f\n", throttleMultiplier, prebufSec, pMedia->media.durationSec);

  etag = http_make_etag(pMedia->pFileStream->filename, etagbuf, sizeof(etagbuf));

  if((pHdr = conf_find_keyval(pReq->reqPairs, HTTP_HDR_IF_NONE_MATCH)) &&
       (pHdr = avc_dequote(pHdr, NULL, 0)) &&
       !strcmp(pHdr, etag)) {

    statusCode = HTTP_STATUS_NOTMODIFIED;
  } else if((pHdr = conf_find_keyval(pReq->reqPairs, HTTP_HDR_IF_MOD_SINCE))) {

    //TODO: check last modification date!!!
    statusCode = HTTP_STATUS_NOTMODIFIED;

  }

  if(statusCode == HTTP_STATUS_NOTMODIFIED) {

    LOG(X_DEBUG("'%s' Not modified for etag '%s'"), pMedia->pFileStream->filename, etag);

    http_log(pSd, pReq, statusCode, 0);

    if((rc = http_resp_sendhdr(pSd, pReq->version, statusCode,
                        0, NULL,
                        http_getConnTypeStr(pReq->connType), pReq->cookie,
                        NULL, etag, NULL, NULL, NULL)) < 0) {
    }
    return rc;
  }

  contentLen = pMedia->pFileStream->size;
  memset(&reqRange, 0, sizeof(reqRange));

  if((pRangeHdr = conf_find_keyval(pReq->reqPairs, HTTP_HDR_RANGE))) {

    reqRange.acceptRanges = 1;

    if(http_parse_rangehdr(pRangeHdr, &reqRange) == 0) {

      if(SeekMediaFile(pMedia->pFileStream, reqRange.start, SEEK_SET) < 0) {
        http_resp_send(pSd, pReq, HTTP_STATUS_SERVERERROR, NULL, 0);
        return -1;
      }

      if(reqRange.unlimited) {
        if((reqRange.end = pMedia->pFileStream->size) > 0) {
          reqRange.end--;
        }
      }

      reqRange.total = pMedia->pFileStream->size;
      idxFile = pMedia->pFileStream->offset;
      contentLen = reqRange.end - reqRange.start + 1;
      reqRange.contentRange = 1;
      statusCode = HTTP_STATUS_PARTIALCONTENT;

      //fprintf(stderr, "HTTP Parsed Range request accept:%d, content:%d, start:%llu, end:%llu, total:%llu, unlimited:%d\n", reqRange.acceptRanges, reqRange.contentRange, reqRange.start, reqRange.end, reqRange.total, reqRange.unlimited);

      LOG(X_DEBUG("HTTP Range request %"LL64"d - %"LL64"d / %"LL64"d "
                  "Content-Length:%lld, connType:%d"), 
       reqRange.start, reqRange.end, reqRange.total, contentLen, pReq->connType);

      //fprintf(stderr, "tid:%lu HTTP Range request '%s' %"LL64"d - %"LL64"d / %"LL64"d Content-Length:%"LL64"d connType:%d\n", pthread_self(), pRangeHdr, reqRange.start, reqRange.end, reqRange.total, contentLen, pReq->connType);
    }

  }

  http_log(pSd, pReq, statusCode, contentLen);

  if((rc = http_resp_sendhdr(pSd, pReq->version, statusCode,
                        contentLen, pMedia->pContentType, 
                        http_getConnTypeStr(pReq->connType), pReq->cookie, 
                        &reqRange, etag, NULL, NULL, NULL)) < 0) {
    return rc;
  }

  if(throttleMultiplier < 0) {
    throttleMultiplier = 0;
  //} else if(throttleMultiplier < 1) {
  //  throttleMultiplier = 1;
  }
  if(prebufSec < 1) {
    prebufSec = 1;
  }

  //
  // If the request was HEAD then just return not sending any content body
  //
  if(!strncmp(pReq->method, HTTP_METHOD_HEAD, 4)) {
    return rc;
  }

  while(idxContent < contentLen) {

#if defined(BITRATE_RESTRICT)
    LOG(X_DEBUG("HTTP MEDIA SEND:%lld/%lld '%s', throttleRate:%f, mult:%f, durationSec:%f, idxF:%llu/%llu"), idxContent, contentLen, pMedia->pFileStream->filename, throttleRate, throttleMultiplier, pMedia->media.durationSec, idxFile, pMedia->media.dataOffset);
#endif // BITRATE_RESTRICT

    if(throttleRate == 0 && throttleMultiplier > 0 && 
      pMedia->media.durationSec > 0 && idxFile > pMedia->media.dataOffset) {

      gettimeofday(&tv0, NULL);
      throttleRate = (double) (pMedia->media.totSz) / pMedia->media.durationSec; 
      prebufIdx = idxFile + (FILE_OFFSET_T) (prebufSec * throttleRate);

      LOG(X_DEBUG("Throttling %s after %llu bytes %.2fB/s (%llu / %.1f) prebuf %llu (%.1fs) factor:%.2f"), 
          pMedia->pFileStream->filename, idxFile, throttleRate, 
          (pMedia->media.totSz - idxFile), pMedia->media.durationSec, 
          prebufIdx, prebufSec, throttleMultiplier);

//idx0=idxFile;
//gettimeofday(&tvP, NULL);
    }

    if(throttleRate > 0 && idxFile > prebufIdx) {
      gettimeofday(&tv, NULL);
      secElapsed = (tv.tv_sec - tv0.tv_sec) + ((double)(tv.tv_usec - tv0.tv_usec) / 1000000);

//if(tv.tv_sec>tvP.tv_sec) {
//fprintf(stdout, "Throttling %lu:%"USECT" %"LL64"u bytes %.2f%% to %s:%d\n", tv.tv_sec, tv.tv_usec, idxFile-idx0, (double)idxContent/contentLen*100.0, inet_ntoa(pSd->sain.sin_addr), ntohs(pSd->sain.sin_port));
//idx0=idxFile;
//tvP.tv_sec=tv.tv_sec;
//}

      idxTmp = idxFile - reqRange.start;
      if(idxTmp > pMedia->media.dataOffset) {
        idxTmp -= pMedia->media.dataOffset;
      } else {
        idxTmp = 0;
      }
  
      //fprintf(stderr, "cl: %lld/%lld  idxTmp:%lld idxF:%lld file:%lld trate:%.2f secElapsed:%.2f prebufsec:%.2f\n", idxContent,  contentLen, idxTmp, idxFile, pMedia->media.dataOffset, throttleRate, secElapsed, prebufSec);
      while(idxTmp > throttleMultiplier * throttleRate * 
            (secElapsed + prebufSec)) {
        //fprintf(stderr, "throttling sleep zzz\n");
        usleep(50000);
        gettimeofday(&tv, NULL);
        secElapsed = (tv.tv_sec - tv0.tv_sec) + ((double)(tv.tv_usec - tv0.tv_usec) / 1000000);
      }
      //fprintf(stderr, "gona read\n");
    }

    if(net_issockremotelyclosed(NETIOSOCK_FD(pSd->netsocket), 1)) {
      rc = -1;
      LOG(X_DEBUG("HTTP media connection from %s:%d has been remotely closed"),
           FORMAT_NETADDR(pSd->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pSd->sa)) );
      break;
    }

    if((lenread = (unsigned int) (contentLen - idxContent)) > sizeof(buf)) {
      lenread = sizeof(buf);
    }

    if(ReadFileStream(pMedia->pFileStream, buf, lenread) != lenread) {
      return -1;
    }

#if defined(BITRATE_RESTRICT)
    if(strstr(pMedia->pFileStream->filename, ".m3u8")) {
      fprintf(stderr, "---%s\n", buf);
    } else if(strstr(pMedia->pFileStream->filename, ".ts")) {
       int tsIndex = 0 ; 
       int highBitrate = 0;

       if(!strncmp(pMedia->pFileStream->filename, "./html/httplive/out", 19)) {
         sscanf(pMedia->pFileStream->filename, "./html/httplive/out%d.ts", &tsIndex);
         //highBitrate = 1;
       } else if(!strncmp(pMedia->pFileStream->filename, "./html/httplive/2out", 20)) {
         highBitrate = 1;
         sscanf(pMedia->pFileStream->filename, "./html/httplive/2out%d.ts", &tsIndex);
       } else if(!strncmp(pMedia->pFileStream->filename, "./html/httplive/3out", 20)) {
         highBitrate = 2;
         sscanf(pMedia->pFileStream->filename, "./html/httplive/3out%d.ts", &tsIndex);
       }

       //if(highBitrate && tsIndex > 4 && tsIndex < 10) {
       //if(tsIndex > -1 && tsIndex < 10) {
       if(highBitrate && tsIndex > 7) {
         usleep(80000);
         fprintf(stderr, "SLEEPING... '%s', index:%d\n", pMedia->pFileStream->filename, tsIndex);
       }
    }
#endif // BITRATE_RESTRICT

    if(pStats) {
      stream_stats_addPktSample(pStats, NULL, lenread, 1);
    }

    if((rc = netio_send(&pSd->netsocket, (const struct sockaddr *) &pSd->sa, buf, lenread)) < 0) {
      LOG(X_ERROR("Failed to send media '%s' payload %u bytes (%llu/%llu)"), 
            pMedia->pFileStream->filename, lenread, idxContent, contentLen);

      return -1;
    }
    idxContent += lenread;
    idxFile += lenread;

  } // end of while(idxContent...

  if(rc >= 0) {
    LOG(X_DEBUG("Finished sending media '%s' %"LL64"u/%"LL64"u bytes to %s:%d"), 
        pMedia->pFileStream->filename, idxContent, contentLen,
        FORMAT_NETADDR(pSd->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pSd->sa)));
  }

  return rc;
}

int http_resp_sendmediafile(CLIENT_CONN_T *pConn, HTTP_STATUS_T *pHttpStatus,
                            HTTP_MEDIA_STREAM_T *pMedia, float throttleMultiplier,
                            float prebufSec) { 
  int rc = 0;

  if(!pConn) {
    return -1;
  } else if(!pMedia || !pMedia->pFileStream || pMedia->pFileStream->fp == FILEOPS_INVALID_FP) {
    if(pHttpStatus) {
      *pHttpStatus = HTTP_STATUS_NOTFOUND;
    } else {
      http_resp_error(&pConn->sd, &pConn->httpReq, HTTP_STATUS_NOTFOUND, 1, NULL, NULL);
    }
    return -1;
  }

  if(!srv_ctrl_islegal_fpath(pMedia->pFileStream->filename)) {

    LOG(X_ERROR("Illegal file path '%s' referenced by URL '%s'"), pMedia->pFileStream->filename, 
        pConn->httpReq.url);
    if(pHttpStatus) {
      *pHttpStatus = HTTP_STATUS_NOTFOUND;
    } else {
      http_resp_error(&pConn->sd, &pConn->httpReq, HTTP_STATUS_NOTFOUND, 1, NULL, NULL);
    }
    return -1;

  } else if(http_check_symlink(pConn, pMedia->pFileStream->filename) != 0) {

    LOG(X_ERROR("Loading media path %s is forbidden because following symbolic links is disabled"),
                 pMedia->pFileStream->filename);
    if(pHttpStatus) {
      *pHttpStatus = HTTP_STATUS_FORBIDDEN;
    } else {
      http_resp_error(&pConn->sd, &pConn->httpReq, HTTP_STATUS_FORBIDDEN, 1, NULL, NULL);
    }
    return -1;

  }

  rc = resp_sendmediafile(&pConn->sd, &pConn->httpReq, pMedia, pConn->pStats, throttleMultiplier, prebufSec);

  return rc;
}


//#define BITRATE_MEASURE 1

int http_resp_sendtslive(SOCKET_DESCR_T *pSd, HTTP_REQ_T *pReq,
                       PKTQUEUE_T *pQ, const char *pContentType) {
  int rc = 0;
  int sz;
  FILE_OFFSET_T totXmit = 0;
  FILE_OFFSET_T lenLive = 0;
  char tmp[128];
  unsigned char buf[PACKETGEN_PKT_UDP_DATA_SZ];
  unsigned int numOverwritten = 0;
  struct timeval tv0, tv1;

#if defined(BITRATE_MEASURE)
  BURSTMETER_SAMPLE_SET_T bwsets[2];
  //BITRATE_METER_T meters[2];
  BURSTMETER_METER_T *pmeter;
  int idx;
  struct timeval tvdelme;
  BURSTMETER_METER_STATS_T delme;
  gettimeofday(&tvdelme, NULL);
  memset(&delme, 0, sizeof(delme));
  memset(bwsets, 0, sizeof(bwsets));
  //memset(meters, 0, sizeof(meters));
  burstmeter_init(&bwsets[0], 100, 1000);
  burstmeter_init(&bwsets[1], 1000, 8000);
  //meters[0].rangeMs = 250;
  //meters[0].periodMs = bwset.meter.periodMs;
  //meters[1].rangeMs = 1000;
  //meters[1].periodMs = bwset.meter.periodMs;
#endif // BITRATE_MEASURE

  gettimeofday(&tv0, NULL);
  
  //
  // Set outbound QOS
  // 
  // TODO: make QOS configurable
  net_setqos(NETIOSOCK_FD(pSd->netsocket), (const struct sockaddr *) &pSd->sa, DSCP_AF36);

  //lenLive = 0x7fffffff;
  // TODO: smplayer senems to have crash w/ mpeg2-ts w/ content-length: 0

  http_log(pSd, pReq, HTTP_STATUS_OK, lenLive);

  if((rc = http_resp_sendhdr(pSd, pReq->version, HTTP_STATUS_OK,
                   lenLive, pContentType, http_getConnTypeStr(pReq->connType), 
                   pReq->cookie, NULL, NULL, NULL, NULL, NULL)) < 0) {
    return rc;
  }

  //
  // If the request was HEAD then just return not sending any content body
  //
  if(!strncmp(pReq->method, HTTP_METHOD_HEAD, 4)) {
    return rc;
  }

  do {

    sz = pktqueue_readpkt(pQ, buf, sizeof(buf), NULL, &numOverwritten);
    //usleep(60000); fprintf(stderr, "liveQ(id:%d) read:%d rd:%d, wr:%d, tot:%d\n", pQ->cfg.id, sz, pQ->idxRd, pQ->idxWr, pQ->cfg.maxPkts);

    if(sz < 0) {
      LOG(X_ERROR("Error reading %d from live media queue"), sizeof(buf));
      rc = sz;
      break;
    } else if(sz == 0) {

      //TODO: need to create a timeout here
      // if no data is available, clients are able to connect and disconnect
      // is not checked

      pktqueue_waitforunreaddata(pQ);

    } else {

      if(net_issockremotelyclosed(NETIOSOCK_FD(pSd->netsocket), 1)) { 
        rc = -1;
        LOG(X_DEBUG("HTTP live connection from %s:%d has been remotely closed"),
             FORMAT_NETADDR(pSd->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pSd->sa)));
        break;
      }

      if(numOverwritten > 0) {
        LOG(X_WARNING("Resetting live packet queue(id:%d) size:%u for %s:%d"), 
                          pQ->cfg.id, pQ->cfg.maxPkts,
                          FORMAT_NETADDR(pSd->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pSd->sa)));
        pktqueue_reset(pQ, 1);
      } else {

#if defined(BITRATE_MEASURE)
  gettimeofday(&tv1, NULL);
  //burstmeter_AddSample(&bwset, sz, &tv1, meters, 1);
  burstmeter_AddSample(&bwsets[0], sz, &tv1);
  burstmeter_AddSample(&bwsets[1], sz, &tv1);
  delme.bytes+=sz;
  delme.packets++;
  if(tv1.tv_sec-tvdelme.tv_sec >= 1) {
    for(idx=0;idx<2;idx++) {
      pmeter = &bwsets[idx].meter;
      fprintf(stderr, "%d,%dms: cur %ubytes, %.3f(%.3f)Kb/s {%.3fKb/s} %u, %.2f(%.2f)reads/s (%d bytes,%d reads)\n", 
             pmeter->periodMs, pmeter->rangeMs, 
             pmeter->cur.bytes,
             pmeter->cur.bytes * (1000.0f/pmeter->rangeMs)/125.0f,
             pmeter->max.bytes * (1000.0f/pmeter->rangeMs)/125.0f,

             (double) (totXmit+sz) / (((tv1.tv_sec - tv0.tv_sec) * 1000) +
                                ((tv1.tv_usec - tv0.tv_usec) / 1000)) * 8,

             pmeter->cur.packets,
             pmeter->cur.packets * (1000.0f/pmeter->rangeMs), 
             pmeter->max.packets * (1000.0f/pmeter->rangeMs), delme.bytes, delme.packets);
    }

    tvdelme.tv_sec = tv1.tv_sec; 
    delme.bytes = 0;
    delme.packets =0;
  }

#endif // BITRATE_MEASURE

        //if(pStats) {
        //  stream_stats_addPktSample(pStats, NULL, lenread, 1);
        //}

        if((rc = netio_send(&pSd->netsocket, (const struct sockaddr *) &pSd->sa, buf, sz)) < 0) {
          LOG(X_ERROR("Failed to send HTTP live payload %u bytes (%llu/%llu)"), 
               sz, totXmit, lenLive);
         break; 
        }
        totXmit += sz;
      }

    }

  } while(sz >= 0 && !pQ->quitRequested && !g_proc_exit);

  gettimeofday(&tv1, NULL);

  // Report bitrate / 1024 (not 1000)
  LOG(X_INFO("Finished sending tslive %llu bytes to %s:%d (%.1fKb/s)"), totXmit,
             FORMAT_NETADDR(pSd->sa, tmp, sizeof(tmp)), ntohs(INET_PORT(pSd->sa)),
             (double) totXmit / (((tv1.tv_sec - tv0.tv_sec) * 1000) + 
                       ((tv1.tv_usec - tv0.tv_usec) / 1000)) * 7.8125);
  return rc;
}

int http_check_symlink(const CLIENT_CONN_T *pConn, const char *path) {
  int rc = 0;
  struct stat st;

  memset(&st, 0, sizeof(st));

#if 1
  int len;
  char pathbuf[VSX_MAX_PATH_LEN];

  //
  // Check if the path refers to a symbolic link
  //
  if(BOOL_ISDISABLED(pConn->pCfg->enable_symlink)) {

    strncpy(pathbuf, path, sizeof(pathbuf) - 1);
    pathbuf[sizeof(pathbuf) - 1] = '\0';

    do {

      if(pathbuf[0] == '\0' || (pathbuf[0] == '.' && pathbuf[1] == '\0')) {
        break;
      }

      //fprintf(stderr, "CHECK PATH:'%s' for path '%s'\n", pathbuf, path);

      if(fileops_lstat(pathbuf, &st) == 0) {
        if(S_ISLNK(st.st_mode & S_IFLNK)) {
          rc = -1;
          break;
        }
      } else {
        LOG(X_WARNING("Failed to stat '%s' for path: '%s'"), pathbuf, path);
      }

      if((len = path_getLenDirPart(pathbuf)) > 0) {
        pathbuf[len - 1] = '\0';
      }

    } while(len > 0);
  }

#else

  //
  // Check if the path refers to a symbolic link
  //
  if(BOOL_ISDISABLED(pConn->pCfg->enable_symlink)) {
    if(fileops_lstat(path, &st) == 0) {
      if(S_ISLNK(st.st_mode & S_IFLNK)) {
        rc = -1;
      }
    } else {
      //TODO: if we return 1 here, then need to do fileops_stat and if the file exists
      //rc = 1;
    }
  }
#endif

  return rc;
}

/*
int http_check_pass(const CLIENT_CONN_T *pConn) {
  const char *parg;
  int rc = 0;

  if(pConn->pCfg->livepwd &&
     (!(parg = conf_find_keyval((const KEYVAL_PAIR_T *) &pConn->httpReq.uriPairs, "pass")) ||
       strcmp(pConn->pCfg->livepwd, parg))) {

    LOG(X_WARNING("Invalid password for :%d%s from %s:%d (%d char given)"),
         ntohs(pConn->pListenCfg->sain.sin_port), pConn->httpReq.url,
         inet_ntoa(pConn->sd.sain.sin_addr), ntohs(pConn->sd.sain.sin_port),
         parg ? strlen(parg) : 0);

    rc = -1;
  }

  return rc;
}
*/
