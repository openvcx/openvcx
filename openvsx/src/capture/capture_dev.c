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

typedef struct CAP_DEV_CFG_T { 
  pthread_mutex_t      *pmtx;
  CAPTURE_CBDATA_T     *pStreamCbData;
  CAPTURE_STREAM_T     *pStream;
  CAP_ASYNC_DESCR_T    *pCapCfg;
  unsigned int          idxFilter;
} CAP_DEV_CFG_T;

typedef struct CAP_DEV_CTXT {
  const char           *dev;
  PKTQUEUE_PKT_T        frame;
  int                   fd;
  int                   flags;
  unsigned int          idxFrame;
} CAP_DEV_CTXT_T;

#if !defined(WIN32)

static int readFrameFromDev(CAP_DEV_CTXT_T *pCtxt) {
  int rc = 0;
  unsigned int idx = 0;
  long readsz;

  //TEMP skip read
  //if(pCtxt->idxFrame > 0) {
  //  return 0;
  //}

  if(pCtxt->fd <= 0) {
    if((pCtxt->fd = open(pCtxt->dev, O_RDONLY)) < 0) {
      LOG(X_ERROR("Failed to open capture device %s "ERRNO_FMT_STR), 
            pCtxt->dev, ERRNO_FMT_ARGS);
      return -1;
    }
  }

  if((rc = lseek(pCtxt->fd, 0, SEEK_SET)) < 0) {
    LOG(X_ERROR("Failed to seek 0, SEEK_SET on capture device %s "ERRNO_FMT_STR),
                pCtxt->dev, ERRNO_FMT_ARGS); 
    return -1;
  }

  while(idx < pCtxt->frame.len) {
    if((readsz = (pCtxt->frame.len - idx)) > SSIZE_MAX) {
      readsz = SSIZE_MAX;
    }
    if((rc = read(pCtxt->fd, &pCtxt->frame.pData[idx], readsz)) < readsz) {
      LOG(X_ERROR("Failed to read [%d/%d] from capture device %s "ERRNO_FMT_STR),
       idx, pCtxt->frame.len, pCtxt->dev, ERRNO_FMT_ARGS); 
      break;
    }
    idx += rc;
  }

  if(rc >= 0) {
    pCtxt->idxFrame++;
  }

  //fprintf(stderr, "read %d raw input from %s\n", rc, pCtxt->dev);

  return rc;
}

static int readFramesSetup(CAPTURE_FILTER_T *pFilter, PKTQUEUE_PKT_T *pFrame) {
  unsigned int prebufOffset = 0;
  unsigned int bpp = 0;

  if(pFilter->pCapAction &&  pFilter->pCapAction->pQueue) {
    prebufOffset = pFilter->pCapAction->pQueue->cfg.prebufOffset;
  }

  if((bpp = codecType_getBitsPerPx(pFilter->mediaType)) <= 0) {
    LOG(X_ERROR("Unsupported raw device format %s (%d)"), 
        codecType_getCodecDescrStr(pFilter->mediaType), pFilter->mediaType);
    return -1;
  }

  memset(pFrame, 0, sizeof(PKTQUEUE_PKT_T));
  pFrame->allocSz = (pFilter->width * pFilter->height * bpp / 8);
  if(!(pFrame->pBuf = (unsigned char *) avc_calloc(1, prebufOffset + pFrame->allocSz))) {
    return -1;
  }
  pFrame->pData = pFrame->pBuf + prebufOffset;
  pFrame->len = pFrame->allocSz;

  return 0;
}

static int readFramesFromDev(CAP_DEV_CFG_T *pCapDevCfg) {
  int rc = 0;
  TIME_VAL tv0, tv, tvSleep, tvStart;
  TIME_VAL tvElapsed = 0;
  TIME_VAL tvMark = 0;
  unsigned int frameIdx = 0;
  COLLECT_STREAM_PKTDATA_T pktdata;
  CAP_DEV_CTXT_T capDevCtxt;
  CAPTURE_FILTER_T *pFilter = &pCapDevCfg->pCapCfg->pcommon->filt.filters[pCapDevCfg->idxFilter];
  //BMP_FILE_T bmp;

  //memset(&bmp, 0, sizeof(bmp));
  memset(&capDevCtxt, 0, sizeof(capDevCtxt));
  capDevCtxt.dev = pCapDevCfg->pCapCfg->pcommon->localAddrs[pCapDevCfg->idxFilter];
  //capDevCtxt.dev = "./rawfmt.bmp";
  if(readFramesSetup(pFilter, &capDevCtxt.frame) < 0) {
    return -1;
  }

  //TODO: seperate out and maintain better throttle mechanism

  tv0 = tvStart = timer_GetTime();
  while(!g_proc_exit && pCapDevCfg->pCapCfg->running == 0) {

    //
    // Reading too slow, reset time reference
    //
    if(tvElapsed > tvMark + 300000) {
      LOG(X_WARNING("Resetting read time for %s after %llu ms > %llu ms (%.3ffps x %ufr)"),
        capDevCtxt.dev, tvElapsed / TIME_VAL_MS, 
        (TIME_VAL)((1000.0f / pFilter->fps) * frameIdx), pFilter->fps, frameIdx); 
      tv0 = timer_GetTime();
      frameIdx = 0;
    }    

    do {
      tv = timer_GetTime();
      tvElapsed = (tv - tv0);
      tvMark = (frameIdx / pFilter->fps) * TIME_VAL_US;

      if(tvElapsed < tvMark) {
        if((tvSleep = (tvMark - tvElapsed)) < 1000) {
          tvSleep = 1;
        }
        //fprintf(stderr, "sleeping %lld ns\n", tvSleep);
        usleep(tvSleep);
        tv = timer_GetTime();
        tvElapsed = (tv - tv0);
      }

    } while(tvElapsed < tvMark);

//if(frameIdx > 50) usleep(50000);
     

    //fprintf(stderr, "reading frame %d supposed to be:%lld  at:%lld\n", frameIdx, (TIME_VAL) (frameIdx*1000.0f/pFilter->fps),(timer_GetTime() - tv0)/ TIME_VAL_MS);

    if((rc = readFrameFromDev(&capDevCtxt)) < 0) {
      break;
    }

/*
    if((rc = bmp_create(&bmp, pFilter->mediaType,
               pFilter->width, pFilter->height,
               capDevCtxt.frame.pData)) < 0) {
      LOG(X_ERROR("Failed to create bmp %d x %d for format %d"), 
           pFilter->width, pFilter->height, pFilter->mediaType);
      break;
    }
    PKTLEN32(pktdata.payload) = BMP_GET_LEN(&bmp);
    pktdata.payload.pData = BMP_GET_DATA(&bmp);
*/

    pktdata.tv.tv_sec = (tv - tvStart) / TIME_VAL_US;
    pktdata.tv.tv_usec = (tv - tvStart) % TIME_VAL_US;
    PKTLEN32(pktdata.payload) = capDevCtxt.frame.len;
    pktdata.payload.pData = capDevCtxt.frame.pData;

    //fprintf(stderr, "got %d  bmp %d from %s %llu\n", capDevCtxt.frame.len, BMP_GET_LEN(&bmp), capDevCtxt.dev, tv - tvStart);

    pCapDevCfg->pStream->cbOnPkt(&pCapDevCfg->pStreamCbData->sp[0],
                                 (const COLLECT_STREAM_PKTDATA_T *) &pktdata);

    frameIdx++;
  }

  if(capDevCtxt.fd > 0) {
    close(capDevCtxt.fd);
  }
  free(capDevCtxt.frame.pBuf);
  //bmp_destroy(&bmp);

  return rc;
}

#endif // WIN32

int capture_devCbOnFrame(void *pArg, const unsigned char *pData, unsigned int len) { 
  int rc = 0;
  COLLECT_STREAM_PKTDATA_T pktdata;
  CAP_DEV_CFG_T *pCapDevCfg = (CAP_DEV_CFG_T *) pArg;

  if(!pCapDevCfg || !pCapDevCfg->pmtx) {
    return -1;
  }

  VSX_DEBUG_INFRAME( LOG(X_DEBUG("INFRAME - capture_devCbOnFrame len:%d %s"), 
         len, codecType_getCodecDescrStr(pCapDevCfg->pStream->pFilter->mediaType)); );

  timer_getPreciseTime(&pktdata.tv);

  PKTLEN32(pktdata.payload) = len;
  pktdata.payload.pData = (unsigned char *) pData;

  //fprintf(stderr, "capture_devCbOnFrame len:%d 0x%x '%s' %u:%u\n",len, pCapDevCfg->pStream->pCbUserData, pCapDevCfg->pCapCfg->pcommon->localAddrs[pCapDevCfg->idxFilter], pktdata.tv.tv_sec, pktdata.tv.tv_usec);
  //__android_log_print(ANDROID_LOG_INFO, "vsx", "capture_devCbOnFrame len:%d 0x%x '%s'\n",len, pCapDevCfg->pStream->pCbUserData, pCapDevCfg->pCapCfg->pcommon->localAddrs[pCapDevCfg->idxFilter]);

  pthread_mutex_lock(pCapDevCfg->pmtx);

  rc = pCapDevCfg->pStream->cbOnPkt(pCapDevCfg->pStream->pCbUserData,
                                    (const COLLECT_STREAM_PKTDATA_T *) &pktdata);

  pthread_mutex_unlock(pCapDevCfg->pmtx);

  return rc;
}

static int readFramesFromCb(CAP_DEV_CFG_T *pCapCbDatas) {
  int rc = 0;
  unsigned int idx;
  pthread_mutex_t mtx;
  const char *dev;
  CAPTURE_DESCR_COMMON_T *pCapDescr = pCapCbDatas[0].pCapCfg->pcommon;

  pthread_mutex_init(&mtx, NULL);

  pCapCbDatas[0].pmtx = pCapCbDatas[1].pmtx = &mtx;

  for(idx = 0; idx < pCapDescr->filt.numFilters; idx++) {
    dev = pCapDescr->localAddrs[idx];

    if(!strncasecmp(dev, CAP_DEV_DUMMY_VID, strlen(CAP_DEV_DUMMY_VID))) {
      //
      // This is stored as part of ngbslib STREAM_PARAMS_T pPrivData context
      //
      pCapDescr->pCapDevCfg->pCapVidData = &pCapCbDatas[idx];
    } else if(!strncasecmp(dev, CAP_DEV_DUMMY_AUD, strlen(CAP_DEV_DUMMY_AUD))) {
      pCapDescr->pCapDevCfg->pCapAudData = &pCapCbDatas[idx];
    } else {
      LOG(X_ERROR("Unsupported input capture device %s"), dev);
      rc = -1;
    }
  }

  // 
  // User application will call capture_devCbOnFrame asynchronously
  //
  if(rc == 0) {
    while(!g_proc_exit && pCapCbDatas[0].pCapCfg->running == 0) {
      usleep(500000);
    }
  }

  pCapDescr->pCapDevCfg->pCapVidData = NULL;
  pCapDescr->pCapDevCfg->pCapAudData = NULL;
  pthread_mutex_destroy(&mtx);

  return rc;
}


int capture_devStart(CAP_ASYNC_DESCR_T *pCfg) {

  CAPTURE_CBDATA_T streamCbData;
  CAPTURE_STREAM_T streams[CAPTURE_MAX_FILTERS];
  unsigned int idx = 0;
  unsigned int numdevs = 1;
  int rc = 0;
  CAP_DEV_CFG_T capCbDatas[CAPTURE_MAX_FILTERS];

  if(!pCfg || !pCfg->pcommon->localAddrs[0]) {
    return -1;
  }

  if(pCfg->pcommon->localAddrs[1]) {
    numdevs = 2;
  }

  if(numdevs != pCfg->pcommon->filt.numFilters) {
    return -1;
  }

  memset(&streams, 0, sizeof(streams));
  capture_initCbData(&streamCbData, pCfg->record.outDir, pCfg->record.overwriteOut);

  for(idx = 0; idx < numdevs; idx++) {

    if(pCfg->pcommon->filt.filters[idx].transType != CAPTURE_FILTER_TRANSPORT_DEV ||
       !pCfg->pcommon->localAddrs[idx]) {
      LOG(X_ERROR("device[%d] %s uses invalid transport type: %d"), idx, pCfg->pcommon->localAddrs[idx], 
          pCfg->pcommon->filt.filters[idx].transType);
      capture_deleteCbData(&streamCbData);
      return -1;
    }

    capCbDatas[idx].idxFilter = idx;
    capCbDatas[idx].pStreamCbData = &streamCbData;
    capCbDatas[idx].pStream = &streams[idx];
    capCbDatas[idx].pCapCfg = pCfg;

    streams[idx].pFilter = &pCfg->pcommon->filt.filters[idx];

    //
    // Call cbOnStreamAdd to initialize the stream specific packet callback function
    //
    if(capture_cbOnStreamAdd(&streamCbData, &streams[idx], NULL, NULL) < 0 ||
       streams[idx].cbOnPkt == NULL) {
      LOG(X_ERROR("Unable to initialize device %s capture"),
          pCfg->pcommon->localAddrs[idx]);
      capture_deleteCbData(&streamCbData);
      return -1;
    }

  }

  if(!strncasecmp(CAP_DEV_DUMMY, pCfg->pcommon->localAddrs[0], strlen(CAP_DEV_DUMMY))) {
    rc = readFramesFromCb(capCbDatas);

  } else {

    if(numdevs > 1) {
      // TODO: create seperate thread
    }
#if !defined(WIN32)
    rc = readFramesFromDev(&capCbDatas[0]);
#else
    rc = -1;
#endif // WIN32
  }

  capture_deleteCbData(&streamCbData);
  //LOG(X_DEBUG("Capture device %s ended rc:%d (%d)"), rc, pCfg->running);

  return rc;
}


#endif // VSX_HAVE_CAPTURE

