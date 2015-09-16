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

typedef struct MP4_NET_STREAM {
  NETIO_SOCK_T            *pNetSock;
  const struct sockaddr   *psa;
  unsigned int            rcvTmtMs;

  struct CAP_HTTP_MP4    *pCapHttpMp4;
} MP4_NET_STREAM_T;


typedef struct MP4_RCV_STATE {
  enum MP4_RCV_ERROR     error;
  int                    haveFtyp;
  int                    haveStyp;
  int                    haveMoov;
  int                    haveMoof;
  int                    haveMdat;


  //int                    readingMdat;
  //FILE_OFFSET_T          mdatIdx;
  //FILE_OFFSET_T          mdatSize;
  FILE_STREAM_T          fStream;
} MP4_RCV_STATE_T;

typedef struct CAP_HTTP_MP4 {
  CAP_HTTP_COMMON_T     common;
  //CODEC_AV_CTXT_T          av;
  MP4_NET_STREAM_T         mp4NetStream;
  MP4_CONTAINER_T         *pMp4FromNet;
  MP4_RCV_STATE_T          rcvState;

} CAP_HTTP_MP4_T;


int mp4_cbReadDataNet(void *pArg, void *pData, unsigned int len) {
  int rc = 0;
  unsigned int cachedRdSz = 0;
  MP4_FILE_STREAM_T *pMp4Fs = (MP4_FILE_STREAM_T *) pArg;
  MP4_NET_STREAM_T *pNs = (MP4_NET_STREAM_T *) pMp4Fs->pCbData;
  CAP_HTTP_MP4_T *pCapHttpMp4 = pNs->pCapHttpMp4;

  //fprintf(stderr, "mp4_cbReadDataNet len:%d, mdat:%d, %llu/%llu\n", len, pCapHttpMp4->rcvState.readingMdat, pCapHttpMp4->rcvState.mdatIdx, pCapHttpMp4->rcvState.mdatSize);

  if(pCapHttpMp4->common.pCfg->running != 0 || g_proc_exit) {
    return -1;
  }

  //
  // Check pre-buffered content since reading of the HTTP headers may have eaten some of the actual content
  //
  if(pCapHttpMp4->common.pdata && pCapHttpMp4->common.datasz > pCapHttpMp4->common.dataoffset) {
    cachedRdSz = MIN(len, pCapHttpMp4->common.datasz - pCapHttpMp4->common.dataoffset);
    memcpy(pData, &pCapHttpMp4->common.pdata[pCapHttpMp4->common.dataoffset], cachedRdSz);
    //fprintf(stderr, "mp4_cbReadDataNet CACHED RD sz:%d, %d/%d\n", cachedRdSz, pCapHttpMp4->common.dataoffset, pCapHttpMp4->common.datasz);
    pCapHttpMp4->common.dataoffset += cachedRdSz;
  }

  if(len > cachedRdSz &&
     (rc = netio_recvnb_exact(pNs->pNetSock, &((unsigned char *)pData)[cachedRdSz], 
                              len - cachedRdSz, pNs->rcvTmtMs)) != (len - cachedRdSz)) {
    if(rc == 0) {
      return rc;
    }
    return -1;
  }

  pMp4Fs->offset += len;

//if(pMp4Fs->offset < 300000)
  //
  // Write the frame content to the (temporary) mp4 cached file
  //
  if((rc = WriteFileStream(&pCapHttpMp4->rcvState.fStream, pData, len)) < 0) {
    return rc;
  }

  //usleep(8000);

  //if(pCapHttpMp4->rcvState.readingMdat) {
  //    pCapHttpMp4->rcvState.mdatIdx += len;
  //}

  //fprintf(stderr, "mp4_cbReadDataNet done reading len:%d, offset at: %llu, fd:%d\n", len, pMp4Fs->offset, pNs->pNeetsock->sock); //avc_dumpHex(stderr, pData, MIN(16, len), 1);

  return rc;
}

int mp4_cbSeekDataNet(void *pArg, FILE_OFFSET_T offset, int whence) {
  int rc = 0;
  unsigned char buf[4096]; 
  FILE_OFFSET_T idx = 0;
  MP4_FILE_STREAM_T *pMp4Fs = (MP4_FILE_STREAM_T *) pArg;
  //MP4_NET_STREAM_T *pNs = (MP4_NET_STREAM_T *) pMp4Fs->pCbData;
  FILE_OFFSET_T pos, count;

  switch(whence) {
    case SEEK_SET:
      pos = offset;
      break;
    case SEEK_CUR:
      pos = pMp4Fs->offset + offset;
      break;
    case SEEK_END:
    default:
      LOG(X_ERROR("Unable to seek to live mp4 content"));
      return -1;
  }

  //fprintf(stderr, "mp4_cbSeekDataNet offset:%lld, pos:%lld, bytesRead:%lld\n", offset, pos, pMp4Fs->offset);

  if(pos < pMp4Fs->offset) {
    LOG(X_ERROR("Unable to seek backwards in live mp4 content to position: %llu after reading %llu bytes"), 
        pos, pMp4Fs->offset);
    return -1;
  } else if((count = (pos - pMp4Fs->offset)) > 0) {

    while(idx < count) {
      if((rc = mp4_cbReadDataNet(pArg, buf, MIN(count - idx, sizeof(buf)))) < 0) {
        return rc;
      }
      idx += rc;
    }

  }

  return rc;
}


int mp4_cbCloseDataNet(void *pArg) {
  //int rc = 0;
  return 0;
}

int mp4_cbCheckFdDataNet(void *pArg) {
  return 1;
}

char *mp4_cbGetNameDataNet(void *pArg) {
  return "";
}

int mp4_cbNotifyType(void *pArg, enum MP4_NOTIFY_TYPE type, void *pArgBox) {
  int rc = 0;
  MP4_FILE_STREAM_T *pMp4Fs = (MP4_FILE_STREAM_T *) pArg;
  MP4_NET_STREAM_T *pNs = (MP4_NET_STREAM_T *) pMp4Fs->pCbData;
  BOX_T *pBox = (BOX_T *) pArgBox;
  CAP_HTTP_MP4_T *pCapHttpMp4 = pNs->pCapHttpMp4;
  CAP_HTTP_MP4_STREAM_T *pCapHttpMp4Stream = NULL;

  pCapHttpMp4Stream = (CAP_HTTP_MP4_STREAM_T *) pCapHttpMp4->common.pCfg->pUserData;

  //
  // This callback gets invoked after parsing an MP4 box header with type MP4_NOTIFY_TYPE_READHDR
  // and after parsing an mp4 box with MP4_NOTIFY_READBOX
  //

  if(pCapHttpMp4->common.pCfg->running != 0 || g_proc_exit) {
    return -1;
  }

  
  if(type == MP4_NOTIFY_TYPE_READBOXCHILDREN && pBox->type == *((uint32_t *) "ftyp")) {

    //
    // Completed reading the ftyp box
    //

    pCapHttpMp4->rcvState.haveFtyp = 1;
    //fprintf(stderr, "FTYP...\n");

  } else if(type == MP4_NOTIFY_TYPE_READBOXCHILDREN && pBox->type == *((uint32_t *) "styp")) {

    //
    // Completed reading the styp box
    //

    pCapHttpMp4->rcvState.haveStyp = 1;
    //fprintf(stderr, "STYP...\n");


  } else if(type == MP4_NOTIFY_TYPE_READBOXCHILDREN && pBox->type == *((uint32_t *) "moov")) {

    //
    // Completed reading the moov box (mp4 initialization)
    //

    pCapHttpMp4->rcvState.haveMoov = 1;
    //fprintf(stderr, "MOOV...\n");

    //
    // Indicate to the stream outupt that this mp4/moof is being downloaded
    //
    //if(pCapHttpMp4Stream) {
    //  mpdpl_update(&pCapHttpMp4Stream->pl, pCapHttpMp4->rcvState.fStream.filename, PLAYLIST_MPD_STATE_HAVEHDRS);
    //}

  } else if(type == MP4_NOTIFY_TYPE_READBOXCHILDREN && pBox->type == *((uint32_t *) "moof")) {

    //
    // Completed reading a moof box 
    //

    if(pCapHttpMp4->rcvState.haveMoof == 0) {
      pCapHttpMp4->rcvState.haveMoof = 1;
      //fprintf(stderr, "MOOF...\n");

      //if(pCapHttpMp4Stream) {
      //  mpdpl_update(&pCapHttpMp4Stream->pl, pCapHttpMp4->rcvState.fStream.filename, PLAYLIST_MPD_STATE_HAVEFRAGMENT);
      //}
    }

  } else if(pBox->type == *((uint32_t *) "mdat")) {

    if(type == MP4_NOTIFY_TYPE_READHDR) {

      //
      // Read the MDAT header
      //

      if(!pCapHttpMp4->rcvState.haveStyp && 
         (!pCapHttpMp4->rcvState.haveFtyp || !pCapHttpMp4->rcvState.haveMoov)) {
        //fprintf(stderr, "ftyp:%d moov:%d, styp:%d\n", pCapHttpMp4->rcvState.haveFtyp,pCapHttpMp4->rcvState.haveMoov, pCapHttpMp4->rcvState.haveStyp);
        LOG(X_ERROR("MP4 must not contain MDAT before MOOV and FTYP for live playback"));
        pCapHttpMp4->rcvState.error =  MP4_RCV_ERROR_NO_MOOV;
        rc = -1;
      }

      //
      // Prevent setting the HAVEMDAT flag for MOOF mp4 since the current mp4extractor read technique
      // relies on 1 parser call, which would be invoked before all moofs are present... 
      // TODO: need to update the ISOBMFF extractor to reparse each subseqent moof
      //
      if(!pCapHttpMp4->rcvState.haveMoof && pCapHttpMp4Stream) {
        mpdpl_update(&pCapHttpMp4Stream->pl, pCapHttpMp4->rcvState.fStream.filename, PLAYLIST_MPD_STATE_HAVEMDAT);
      }

      pCapHttpMp4->rcvState.haveMdat = 1;
      //pCapHttpMp4->rcvState.readingMdat = 1;
      //pCapHttpMp4->rcvState.mdatIdx = 0;
      //pCapHttpMp4->rcvState.mdatSize = pBox->szdata_ownbox;
      //fprintf(stderr, "MDAT...\n");

    } else if(type == MP4_NOTIFY_TYPE_READBOXCHILDREN) {

      //
      // Completed reading the MDAT box 
      //

      //pCapHttpMp4->rcvState.readingMdat = 0;

    }

  }

  //fprintf(stderr, "mp4_cbNotifyType type:%d, '%s', szhdr:%d, szdata:%llu, szdata_own:%d, subs:%d\n", type, pBox->name, pBox->szhdr, pBox->szdata, pBox->szdata_ownbox, pBox->subs);

  return rc;
}

/*
static unsigned char *read_net_mp4(CAP_HTTP_MP4_T *pMp4, unsigned int szToRead) {
  return http_read_net(&pMp4->common,
                  szToRead,
                  pMp4->in.buf,
                  pMp4->in.sz,
                  &pMp4->bytesRead);
}
*/

static MP4_CONTAINER_T *mp4net_open(CAP_HTTP_MP4_T *pCapCtxt) {
  MP4_CONTAINER_T *pMp4 = NULL;
  MP4_FILE_STREAM_T *pStream = NULL;

  //
  // Used to read an mp4 container directly from a network stream
  //

  if(!(pStream = avc_calloc(1, sizeof(MP4_FILE_STREAM_T)))) {
    return NULL;
  }

  pStream->cbRead = mp4_cbReadDataNet;
  pStream->cbSeek = mp4_cbSeekDataNet;
  pStream->cbClose = mp4_cbCloseDataNet;
  pStream->cbCheckFd = mp4_cbCheckFdDataNet;
  pStream->cbGetName = mp4_cbGetNameDataNet;
  pStream->cbNotifyType = mp4_cbNotifyType;
  pStream->pCbData = &pCapCtxt->mp4NetStream;

  if((pMp4 = mp4_createContainer()) == NULL) {
    pStream->cbClose(pStream);
    free(pStream);
    return NULL;
  }

  pMp4->pStream = pStream;
  //pMp4->proot->szdata = pMp4->pStream->size;
  pMp4->proot->subs = 1;

  return pMp4;
}

static void http_mp4_close(CAP_HTTP_MP4_T *pCapHttpMp4) {
  FILE_STREAM_T stream;


  pCapHttpMp4->rcvState.error =  MP4_RCV_ERROR_FORCE_EXIT;

/*
  if(pCapHttpMp4->in.buf) {
    avc_free((void **) &pCapHttpMp4->in.buf);
  }
  pCapHttpMp4->in.sz = 0;
*/

  //codecfmt_close(&pCapHttpMp4->av);

  if(pCapHttpMp4->pMp4FromNet) {
    mp4_close(pCapHttpMp4->pMp4FromNet);
    pCapHttpMp4->pMp4FromNet = NULL;
  }

  memcpy(&stream, &pCapHttpMp4->rcvState.fStream, sizeof(stream));
  CloseMediaFile(&pCapHttpMp4->rcvState.fStream);

  //
  // Delete the temporary mp4 file
  //
  //fileops_DeleteFile(stream.filename);

}

static int http_mp4_init(CAP_HTTP_MP4_T *pCapHttpMp4) {
  int rc = 0;
  struct stat st;

  if(pCapHttpMp4->rcvState.fStream.filename[0] == '\0') {
    return -1;
  }

  //if((codecfmt_init(&pCapHttpMp4->av, RTMP_TMPFRAME_VID_SZ, RTMP_TMPFRAME_AUD_SZ)) < 0) {
  //  http_mp4_close(pCapHttpMp4);
  //  return -1;
  //}

  if(!(pCapHttpMp4->pMp4FromNet = mp4net_open(pCapHttpMp4))) {
    http_mp4_close(pCapHttpMp4);
    return -1;
  }

  //
  // Delete any prior temporary mp4 file
  //
  if(fileops_stat(pCapHttpMp4->rcvState.fStream.filename, &st) == 0) {
    fileops_DeleteFile(pCapHttpMp4->rcvState.fStream.filename);
  }

  if((pCapHttpMp4->rcvState.fStream.fp = fileops_Open(pCapHttpMp4->rcvState.fStream.filename, 
                                                      O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open %s for writing"), pCapHttpMp4->rcvState.fStream.filename);
    http_mp4_close(pCapHttpMp4);
    return -1;
  }

  pCapHttpMp4->rcvState.fStream.offset = 0;
  pCapHttpMp4->rcvState.fStream.size = 0;

  //fprintf(stderr, "VID PQ maxPkts:%d, maxPktLen:%d, grow:%d,%d\n", pCapHttpMp4->client.pQVid->cfg.maxPkts, pCapHttpMp4->client.pQVid->cfg.maxPktLen, pCapHttpMp4->client.pQVid->cfg.growMaxPkts, pCapHttpMp4->client.pQVid->cfg.growMaxPktLen);

  return rc;
}

static int http_mp4_read_boxes(CAP_ASYNC_DESCR_T *pCfg, 
                               CAP_HTTP_MP4_T *pCapHttpMp4,
                               FILE_OFFSET_T contentLen) {
  BOX_T box;
  int rc = 0;
  uint32_t len;

  //
  // This mp4 box reader only scans the root level boxes and does not recursively descend
  //

  while(pCfg->running == 0 && !g_proc_exit &&
        (contentLen == 0 || pCapHttpMp4->pMp4FromNet->pStream->offset < contentLen)) {

   //fprintf(stderr, "http_mp4_read_boxes..\n");

    memset(&box, 0, sizeof(box));
    if((rc = mp4_read_box_hdr(&box, pCapHttpMp4->pMp4FromNet->pStream)) != 0) {
      //fprintf(stderr, "http_mp4_read_boxes read_box_hdr failed..\n");
      return -1;
    }

    //
    // Invoke the notify type callback to tell the reader what type of box is about to be consumed
    //
    if(pCapHttpMp4->pMp4FromNet->pStream->cbNotifyType && 
       (rc = pCapHttpMp4->pMp4FromNet->pStream->cbNotifyType(pCapHttpMp4->pMp4FromNet->pStream, 
                                                MP4_NOTIFY_TYPE_READHDR, &box)) < 0) {
      return -1;
    }

    len = (uint32_t) box.szdata;

    if(pCapHttpMp4->pMp4FromNet->pStream->cbSeek(pCapHttpMp4->pMp4FromNet->pStream, len, SEEK_CUR) < 0) {
      //fprintf(stderr, "http_mp4_read_boxes cbSeek len:%d failed..\n", len);
      return -1;
    }

    //
    // Invoke the notify type callback to tell the reader what type of box is about to be consumed
    //
    if(pCapHttpMp4->pMp4FromNet->pStream->cbNotifyType && 
       (rc = pCapHttpMp4->pMp4FromNet->pStream->cbNotifyType(pCapHttpMp4->pMp4FromNet->pStream, 
                                                      MP4_NOTIFY_TYPE_READBOXCHILDREN, &box)) < 0) {
      return -1;
    }

  }

  return rc;
}

int http_mp4_recv(CAP_ASYNC_DESCR_T *pCfg,
                  NETIO_SOCK_T *pNetSock,
                  const struct sockaddr *psa,
                  FILE_OFFSET_T contentLen,
                  HTTP_PARSE_CTXT_T *pHdrCtxt,
                  const char *outPath) {
                  //CAP_HTTP_MP4_STREAM_T *pCapHttpMp4Stream) {

  //unsigned char *pdata;
  //unsigned int ts0 = 0;
  //unsigned int ts;
  //unsigned int tsoffset = 0;
  //int havets0 = 0;
  //TIME_VAL tm0, tm;
  //unsigned int msElapsed;
  //int do_sleep;
  int rc;
  CAP_HTTP_MP4_T capHttpMp4;
  CAP_HTTP_MP4_STREAM_T *pCapHttpMp4Stream = NULL;
  unsigned int bytesRead = 0;
  //unsigned char arena[4096];

  if(!pCfg || !pNetSock || !psa || !pHdrCtxt) {
    return -1;
  }

  pCapHttpMp4Stream = (CAP_HTTP_MP4_STREAM_T *) pCfg->pUserData;

  memset(&capHttpMp4, 0, sizeof(capHttpMp4));
  capHttpMp4.common.pCfg = pCfg;
  capHttpMp4.common.pdata = (unsigned char *) pHdrCtxt->pbuf;
  capHttpMp4.common.dataoffset = pHdrCtxt->hdrslen;
  capHttpMp4.common.datasz = pHdrCtxt->idxbuf;
  capHttpMp4.mp4NetStream.pNetSock = pNetSock;
  capHttpMp4.mp4NetStream.psa = psa;
  capHttpMp4.mp4NetStream.rcvTmtMs = 10000;
  capHttpMp4.mp4NetStream.pCapHttpMp4 = &capHttpMp4;

  if(net_setsocknonblock(PNETIOSOCK_FD(capHttpMp4.mp4NetStream.pNetSock), 1) < 0) {
    return -1;
  }

  if(outPath) {
    snprintf(capHttpMp4.rcvState.fStream.filename, 
             sizeof(capHttpMp4.rcvState.fStream.filename), "%s", outPath);
  } else {
    //
    // Create an output filename for the downloaded mp4 file
    //
    snprintf(capHttpMp4.rcvState.fStream.filename,
             sizeof(capHttpMp4.rcvState.fStream.filename), "%s%c%s_%d.mp4",
             "tmp", DIR_DELIMETER, "dl", getpid());
  }

  capHttpMp4.rcvState.fStream.fp = FILEOPS_INVALID_FP;

  if(http_mp4_init(&capHttpMp4) < 0) {
    return -1;
  }

  //fprintf(stderr, "MOOF WILL RCV outPath:'%s'\n", outPath);

  //
  // Indicate to the stream outupt that this mp4/moof is being downloaded 
  //
  if(pCapHttpMp4Stream) {
    mpdpl_add(&pCapHttpMp4Stream->pl, capHttpMp4.rcvState.fStream.filename, PLAYLIST_MPD_STATE_DOWNLOADING);
  }

  //
  // Using the mp4 based file reader will parse all box contents, which will be duplicated
  // when using the stream_mp4 based file based reader for mp4 transmission
  //
  //rc = mp4_read_boxes(capHttpMp4.pMp4FromNet->pStream, capHttpMp4.pMp4FromNet->proot, arena, sizeof(arena));

  //
  // The http mp4 based reader only looks at the first level box nesting 
  //
  rc = http_mp4_read_boxes(pCfg, &capHttpMp4, contentLen);

  bytesRead = capHttpMp4.pMp4FromNet->pStream->offset;

  //
  // Indicate to the stream outupt that this mp4/moof has been downloaded
  //
  if(pCapHttpMp4Stream) {
    mpdpl_update(&pCapHttpMp4Stream->pl, capHttpMp4.rcvState.fStream.filename, 
            rc >= 0 ? PLAYLIST_MPD_STATE_DOWNLOADED : PLAYLIST_MPD_STATE_ERROR);
  }

  fprintf(stderr, "MOOF mp4_read_boxes done rc:%d, bytesRead:%d, contentLen:%llu\n", rc, bytesRead, contentLen);

  //tm0 = timer_GetTime();

  //if(pCfg->pcommon->caprealtime) {
  //  LOG(X_DEBUG("Treating capture as real-time."));
  //}

  //TODO: should signal the transmitter thread
  //pthread_mutex_lock(&pCfg->mtx);
  //pCfg->pUserData = NULL;
  //pthread_mutex_unlock(&pCfg->mtx);

  http_mp4_close(&capHttpMp4);

  return bytesRead;
}


static int waitForFileContent(MP4_FILE_STREAM_T *pMp4Fs, unsigned int len) {
  //int rc = 0;
  struct stat st;
  FILE_STREAM_T *pFs = (FILE_STREAM_T *) pMp4Fs->pCbData;
  CAP_ASYNC_DESCR_T *pCapCfg = (CAP_ASYNC_DESCR_T *) pMp4Fs->pUserData;
  CAP_HTTP_MP4_T *pCapHttpMp4 = (CAP_HTTP_MP4_T *) pCapCfg->pUserData;
  struct timeval tv0, tv;
  unsigned int rcvTmtMs = pCapHttpMp4->mp4NetStream.rcvTmtMs;

  //fprintf(stderr, "WAIT FOR FILE C PUSERDATA: 0x%x\n", pCapCfg->pUserData);

  gettimeofday(&tv0, NULL);

  while(pFs->offset + len > pFs->size) {

    if(pCapCfg->running != 0 || g_proc_exit) {
      return -1;
    }

    //
    // Check the current file size to see if we have enough content to read
    //
    if(fileops_stat(pFs->filename, &st) != 0) {
      LOG(X_ERROR("Unable to get file size of '%s'."), pFs->filename);
      return -1;
    } else if(st.st_size != pFs->size) {
      pFs->size = st.st_size;
      pMp4Fs->size = pFs->size;
    }

    //
    // If the file does not have enough content to read, sleep and try again
    //
    if(pFs->offset + len > pFs->size) {

      //fprintf(stderr, "waitForFileContent... '%s', offset:%llu, len:%u, size:%llu, sleeping...\n", pFs->filename, pFs->offset, len, pFs->size);

      gettimeofday(&tv, NULL);
      if(TIME_TV_DIFF_MS(tv, tv0) > rcvTmtMs) {
        LOG(X_WARNING("Timeout %u ms exceeded when waiting for frame in '%s'"), 
                      rcvTmtMs, pMp4Fs->cbGetName(pMp4Fs));
        pCapHttpMp4->rcvState.error = MP4_RCV_ERROR_RD_TMT;
        return -1;
      }

      usleep(10000);
    }

  }

  if(pCapCfg->running != 0 || g_proc_exit) {
    return -1;
  }

  return 0;
}

int mp4_cbReadDataFileLive(void *pArg, void *pData, unsigned int len) {
  int rc = 0;
  MP4_FILE_STREAM_T *pMp4Fs = (MP4_FILE_STREAM_T *) pArg;
  FILE_STREAM_T *pFs = (FILE_STREAM_T *) pMp4Fs->pCbData;
  CAP_ASYNC_DESCR_T *pCapCfg = (CAP_ASYNC_DESCR_T *) pMp4Fs->pUserData;
  CAP_HTTP_MP4_T *pCapHttpMp4 = NULL;

  if(!pFs || !pCapCfg || !(pCapHttpMp4 = (CAP_HTTP_MP4_T *) pCapCfg->pUserData)) {
    //fprintf(stderr, "cbReadDataFileLive error\n");
    return -1;
  } else if(pCapHttpMp4->rcvState.error < 0) {
    return -1;
  }

  //fprintf(stderr, "cbReadDataFileLive '%s', offset:%llu, len:%u, size:%llu, %d\n", pFs->filename, pFs->offset, len, pFs->size, (pFs->offset + len > pFs->size));

  if(pFs->offset + len > pFs->size && (rc = waitForFileContent(pMp4Fs, len)) < 0) {
    return rc;
  }

  rc = mp4_cbReadDataFile(pArg, pData, len);

  return rc;
}

int mp4_cbSeekDataFileLive(void *pArg, FILE_OFFSET_T offset, int whence) {
  int rc = 0;
  MP4_FILE_STREAM_T *pMp4Fs = (MP4_FILE_STREAM_T *) pArg;
  FILE_STREAM_T *pFs = (FILE_STREAM_T *) pMp4Fs->pCbData;
  CAP_ASYNC_DESCR_T *pCapCfg = (CAP_ASYNC_DESCR_T *) pMp4Fs->pUserData;
  CAP_HTTP_MP4_T *pCapHttpMp4 = NULL;
  FILE_OFFSET_T pos, count;

  if(!pFs || !pCapCfg || !(pCapHttpMp4 = (CAP_HTTP_MP4_T *) pCapCfg->pUserData)) {
    //fprintf(stderr, "cbReadDataFileLive error\n");
    return -1;
  } else if(pCapHttpMp4->rcvState.error < 0) {
    return -1;
  }

  switch(whence) {
    case SEEK_SET:
      pos = offset;
      break;
    case SEEK_CUR:
      pos = pMp4Fs->offset + offset;
      break;
    case SEEK_END:
     pos = pMp4Fs->size;
      break;
    default:
      return -1;
  }

  //fprintf(stderr, "mp4_cbSeekDataFileLive whence:%d, offset:%lld, pos:%lld, pMp4Fs->offset:%lld, size:%lld\n", whence, offset, pos, pMp4Fs->offset, pMp4Fs->size);

  if(pos > pMp4Fs->size) {
    count = pos - pMp4Fs->offset;

    if((rc = waitForFileContent(pMp4Fs, (unsigned int) count)) < 0) {
      return rc;
    }

  }

  rc = mp4_cbSeekDataFile(pArg, offset, whence);

  return rc;
}

static MP4_CONTAINER_T *cap_http_mp4_open(const char *path, CAP_ASYNC_DESCR_T *pCapCfg) {
  MP4_CONTAINER_T *pMp4 = NULL;

  if(!path || !pCapCfg) {
    return NULL;
  }

  //
  // Used to open an mp4 container which may have an mdat box that is dynamically growing
  //

  //fprintf(stderr, "OPENING MP4 FOR STREAMING '%s'\n", path);

  if((pMp4 = mp4_open(path, 1)) == NULL) {
    LOG(X_ERROR("Failed to read downloaded mp4 path %s"), path);
    return NULL;
  }

  //
  // Change the read data file callback to handle the dynamically growing MDAT
  //
  pMp4->pStream->pUserData = pCapCfg;
  pMp4->pStream->cbRead = mp4_cbReadDataFileLive;
  pMp4->pStream->cbSeek = mp4_cbSeekDataFileLive;

  return pMp4;
}

static PLAYLIST_MPD_ENTRY_T *cap_http_wait_for_next_mp4(CAP_ASYNC_DESCR_T *pCapCfg) {
  int rc;
  CAP_HTTP_MP4_STREAM_T *pCapHttpMp4Stream = NULL;
  PLAYLIST_MPD_ENTRY_T entry;
  PLAYLIST_MPD_ENTRY_T *pEntry = NULL;

  pCapHttpMp4Stream = (CAP_HTTP_MP4_STREAM_T *) pCapCfg->pUserData;

  //
  // Wait until there is an output file to be processed
  //
  while((rc = mpdpl_peek_head(&pCapHttpMp4Stream->pl, &entry)) != 1 || !pEntry) {

    if(entry.state == PLAYLIST_MPD_STATE_DOWNLOADED ||
       entry.state == PLAYLIST_MPD_STATE_HAVEMDAT ||
       entry.state ==  PLAYLIST_MPD_STATE_ERROR) {
      pEntry = mpdpl_pop_head(&pCapHttpMp4Stream->pl);
    }

    //fprintf(stderr, "WAIT FOR NEXT MP4 sz:%d pl: 0x%x, head: 0x%x, tail: 0x%x\n", pCapHttpMp4Stream->pl.numEntries, &pCapHttpMp4Stream->pl, pCapHttpMp4Stream->pl.pEntriesHead, pCapHttpMp4Stream->pl.pEntriesTail); mpdpl_dump(&pCapHttpMp4Stream->pl);

    if(pCapCfg->running != 0 || g_proc_exit) {
      fprintf(stderr, "CAP_HTTP_STREAM_MP4 exiting... running:%d, g_proc_exit:%d\n", pCapCfg->running, g_proc_exit);
      return NULL;
    } else if(pEntry) {
      break;
    }

    usleep(20000);

  }

  fprintf(stderr, "WAIT FOR NEXT MP4 GOT '%s', state: %d\n", pEntry->path, pEntry->state);

  return pEntry;
}

static int mp4_get_track_id(const MP4_EXTRACT_STATE_T *pExtSt) {

  if(pExtSt->trak.moofTrak.pTfhd)
    return pExtSt->trak.moofTrak.pTfhd->trackid;
  else if(pExtSt->trak.pTkhd)
    return pExtSt->trak.pTkhd->trackid;
  else
    return -1;
}

int cap_http_load_next_mp4(void *pArg) {
  int rc = 0;
  MP4_EXTRACT_STATE_INT_T *pExtractSt = (MP4_EXTRACT_STATE_INT_T *) pArg;
  MP4_CONTAINER_T *pMp4 = NULL;
  CAP_ASYNC_DESCR_T *pCapCfg = NULL;
  MP4_EXTRACT_STATE_T *pExtSt = NULL;
  PLAYLIST_MPD_ENTRY_T *pEntry = NULL;
  CAP_HTTP_MP4_STREAM_T *pCapHttpMp4Stream = NULL;
  CAP_HTTP_MP4_NEXT_T *pNext = NULL;
  unsigned int idx, idxUsed;
  int trackId;
  MP4_TRAK_T trak;
  //const char *path = NULL;
  MP4_LOADER_NEXT_T nextLoader;

  //fprintf(stderr, "CAP_HTTP_LOAD_NEXT_MP4\n");

  if(!pExtractSt || !(pExtSt = pExtractSt->pExtSt) || !pExtSt->pStream || 
     !(pCapCfg = (CAP_ASYNC_DESCR_T *) pExtractSt->pExtSt->nextLoader.pCbData) ||
     !(pCapHttpMp4Stream = (CAP_HTTP_MP4_STREAM_T *) pCapCfg->pUserData)) {

    return -1;
  }

  //
  // Ensure that this (capture) playlist has not reached the end
  //
  if(pCapHttpMp4Stream->pl.endOfList) {
    return -1;
  }

  if((trackId = mp4_get_track_id(pExtSt)) <= 0) {
    LOG(X_ERROR("Unable to determine track id from request"));
    return -1;
  }

  for(idx = 0; idx < sizeof(pCapHttpMp4Stream->next) / sizeof(pCapHttpMp4Stream->next[0]); idx++) {

    if(pCapHttpMp4Stream->next[idx].trackId == 0) {
      pCapHttpMp4Stream->next[idx].trackId = trackId;
    }
  
    if(trackId == pCapHttpMp4Stream->next[idx].trackId) {
      pNext = &pCapHttpMp4Stream->next[idx];
      break;
    }
  }

  if(!pNext) {
    return -1;
  }

  pNext->count++;

  if(pNext->count > pCapHttpMp4Stream->highestCount) {

    pCapHttpMp4Stream->highestCount = pNext->count;

    //
    // Wait until there is an output file to be processed
    //
    if(!(pEntry = cap_http_wait_for_next_mp4(pCapCfg))) {
      return -1;
    }

    if(!(pMp4 = cap_http_mp4_open(pEntry->path, pCapCfg) )) {
      mpdpl_free_entry(&pCapHttpMp4Stream->pl, pEntry);
      return -1;
    }

    idxUsed = 0;

    if(pCapHttpMp4Stream->plUsed[0].plEntryUsed) {

      idxUsed = 1;
      if(pCapHttpMp4Stream->plUsed[0].active > 0) {
        pCapHttpMp4Stream->plUsed[0].active--;
      }

      if(pCapHttpMp4Stream->plUsed[1].plEntryUsed) {

        if(pCapHttpMp4Stream->plUsed[0].active > 0) {
          mpdpl_free_entry(&pCapHttpMp4Stream->pl, pEntry);
          return -1;
        }

        mpdpl_free_entry(&pCapHttpMp4Stream->pl, pCapHttpMp4Stream->plUsed[0].plEntryUsed);
        memcpy(&pCapHttpMp4Stream->plUsed[0], &pCapHttpMp4Stream->plUsed[1], sizeof(pCapHttpMp4Stream->plUsed[0]));
        pCapHttpMp4Stream->plUsed[0].active--;
        pCapHttpMp4Stream->plUsed[1].plEntryUsed = NULL;
      }

    }

    pEntry->pUserData = pMp4;
    pCapHttpMp4Stream->plUsed[idxUsed].plEntryUsed = pEntry;
    pCapHttpMp4Stream->plUsed[idxUsed].count = pNext->count;
    pCapHttpMp4Stream->plUsed[idxUsed].active = 1;

  } else {

    for(idxUsed = 0; idxUsed < sizeof(pCapHttpMp4Stream->plUsed) / sizeof(pCapHttpMp4Stream->plUsed[0]); idxUsed++) {
      if(pNext->count == pCapHttpMp4Stream->plUsed[idxUsed].count) {
        pEntry = pCapHttpMp4Stream->plUsed[idxUsed].plEntryUsed;
        pCapHttpMp4Stream->plUsed[idxUsed].active++;
        if(idxUsed == 1) {
          pCapHttpMp4Stream->plUsed[0].active--;
        }
        break;
      }
    }


    if(pEntry) {  
      pMp4 = (MP4_CONTAINER_T *) pEntry->pUserData;
    }

  }

  if(!pMp4) {
    return -1;
  }

  if(!pMp4->haveMoov) {
    //
    // If the file is an mp4 fragment w/o the initializer MOOF parts
    // then preserve the trak components and only re-init the moof components
    // Note that the trak components will point to the original mp4 container context
    // which needs to be preserved
    //
    memcpy(&trak, &pExtSt->trak, sizeof(MP4_TRAK_T));
  }

  //fprintf(stderr, "BEGIN CALLING LOADTRAK %c%c%c%c from load_next_mp4 for '%s'\n",  MP4_BOX_NAME_INT2C(pExtSt->trak.stsdType), pMp4->pStream->cbGetName(pMp4->pStream));

  if(!(mp4_loadTrack(pMp4->proot, &trak, pExtSt->trak.stsdType, pMp4->haveMoov))) {
    //mp4_close(pMp4);
    LOG(X_ERROR("Error loading next %c%c%c%c track from '%s'"), 
                MP4_BOX_NAME_INT2C(pExtSt->trak.stsdType), pMp4->pStream->cbGetName(pMp4->pStream));
    return -1;
  }

  //fprintf(stderr, "DONE CALLING LOADTRAK %c%c%c%c from load_next_mp4 for '%s' has %d samples\n",  MP4_BOX_NAME_INT2C(pExtSt->trak.stsdType), pMp4->pStream->cbGetName(pMp4->pStream), pExtSt->cur.u.mf.countSamples);


  memcpy(&nextLoader, &pExtSt->nextLoader, sizeof(nextLoader));
  memset(pExtSt, 0, sizeof(MP4_EXTRACT_STATE_T));
  memcpy(&pExtSt->trak, &trak, sizeof(MP4_TRAK_T));
  memcpy(&pExtSt->nextLoader, &nextLoader, sizeof(pExtSt->nextLoader));
  pExtSt->cur.pExtSt = pExtSt;
  pExtSt->start.pExtSt = pExtSt;
  pExtSt->lastSync.pExtSt = pExtSt;
  pExtSt->pStream = pMp4->pStream;

  return rc;
}

void cap_http_free_plentry(PLAYLIST_MPD_ENTRY_T *pEntry) {
  MP4_CONTAINER_T *pMp4 = NULL;
  char filename[VSX_MAX_PATH_LEN];

  if(pEntry && (pMp4 = (MP4_CONTAINER_T *) pEntry->pUserData)) {
    strncpy(filename, pMp4->pStream->cbGetName(pMp4->pStream), sizeof(filename));
    mp4_close(pMp4);
    pEntry->pUserData = NULL;
  }

  fileops_DeleteFile(filename);

}

int cap_http_stream_mp4(CAP_ASYNC_DESCR_T *pCapCfg) {
  int rc = 0;
  MP4_CONTAINER_T *pMp4 = NULL;
  CAP_HTTP_MP4_STREAM_T *pCapHttpMp4Stream = NULL;
  PLAYLIST_MPD_ENTRY_T *pEntry = NULL;
  unsigned int idxUsed;

  pCapHttpMp4Stream = (CAP_HTTP_MP4_STREAM_T *) pCapCfg->pUserData;

  if(!pCapHttpMp4Stream) {
    return -1;
  }

  pCapHttpMp4Stream->pl.cbFreeEntry = cap_http_free_plentry;

  //
  // Wait until there is an output file to be processed
  //
  if(!(pEntry = cap_http_wait_for_next_mp4(pCapCfg))) {
    return 0;
  }

  //
  // Open the mp4 file path which is being stored int the 'tmp' directory
  //
  if(!(pMp4 = cap_http_mp4_open(pEntry->path, pCapCfg))) {
    mpdpl_free_entry(&pCapHttpMp4Stream->pl, pEntry);
    return -1;
  }

  pMp4->pNextLoader = avc_calloc(1, sizeof(MP4_LOADER_NEXT_T *));
  pMp4->pNextLoader->cbLoadNextMp4 = cap_http_load_next_mp4;
  pMp4->pNextLoader->pCbData = pCapCfg;
  pEntry->pUserData = pMp4;

  //fprintf(stderr, "STREAM_MP4 begin offset:%llu, size:%llu, pStream:0x%x\n", pMp4->pStream->offset, pMp4->pStream->size, pMp4->pStream);

  //
  // The stream_mp4 should not return until the entire playlist is processed
  //
  rc = stream_mp4(pMp4, pCapCfg->pStreamerCfg);

  //fprintf(stderr, "STREAM_MP4 END rc:%d\n", rc);

  //
  // Free the popped playlist entry
  //
  mpdpl_free_entry(&pCapHttpMp4Stream->pl, pEntry);
 

  for(idxUsed = 0; idxUsed < sizeof(pCapHttpMp4Stream->plUsed) / sizeof(pCapHttpMp4Stream->plUsed[0]); idxUsed++) {
    if(pCapHttpMp4Stream->plUsed[idxUsed].plEntryUsed) {
      mpdpl_free_entry(&pCapHttpMp4Stream->pl, pCapHttpMp4Stream->plUsed[idxUsed].plEntryUsed);
      pCapHttpMp4Stream->plUsed[idxUsed].plEntryUsed = NULL;
    }
  }

  return rc;
}


#endif // VSX_HAVE_CAPTURE
