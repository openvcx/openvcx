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

//
// Include header: EXT-X-PROGRAM-DATE-TIME
//
//#define HTTPLIVE_INCLUDE_PROGRAM_DATE_TIME 1

//
// Seems that iPhone needs HTTPLIVE_NUM_INDEXES_M3U >= 4 to display the 'live' icon,
// which it never does if there are 3 items in the playlist
//
#define HTTPLIVE_NUM_INDEXES_M3U_MIN               3 
#define HTTPLIVE_NUM_INDEXES_M3U_MAX               9999
#define HTTPLIVE_NUM_INDEXES_M3U_DEFAULT           4 
#define HTTPLIVE_NUM_INDEXES_KEEP(p)        ((p)->indexCount + 1)

#if defined(HTTPLIVE_INCLUDE_PROGRAM_DATE_TIME)
    static time_t s_httplive_tm;
#endif // HTTPLIVE_INCLUDE_PROGRAM_DATE_TIME

static void closeFp(HTTPLIVE_DATA_T *pLive) {
  fileops_Close(pLive->fs.fp);
  pLive->fs.fp = FILEOPS_INVALID_FP;
}

int http_purge_segments(const char *dirpath, 
                        const char *fileprefix, 
                        const char *ext, 
                        unsigned int idxmin, 
                        int purgeNonIdx) {

  DIR *pdir;
  struct dirent *direntry;
  //struct stat st;
  char path[VSX_MAX_PATH_LEN];
  int rc = 0;
  char stridx[32];
  unsigned int idx;
  size_t sz;
  size_t szext;
  unsigned int szprfx;
  int isNumeric;

  if(!dirpath || !fileprefix || !ext) {
    return -1;
  }

  //fprintf(stderr, "http_purge_segments '%s', '%s', '%s', idxmin:%d\n", dirpath, fileprefix, ext, idxmin); 

  if(!(pdir = fileops_OpenDir(dirpath))) {
    return -1;
  } 

  szprfx = strlen(fileprefix);
  szext = strlen(ext);

  while((direntry = fileops_ReadDir(pdir))) {

    if(!(direntry->d_type & DT_DIR) &&
       (sz = strlen(direntry->d_name)) >= 7 &&
       !strncasecmp(&direntry->d_name[sz - szext], ext, szext) &&
       !strncasecmp(direntry->d_name, fileprefix, szprfx)) {

      if((sz = (sz - szprfx - szext)) >= sizeof(stridx)) {
        sz = sizeof(stridx) - 1;
      }
      memcpy(stridx, &direntry->d_name[szprfx], sz);
      stridx[sz] = '\0';
      isNumeric = 1;
      for(idx = 0; idx < sz; idx++) {
        if(!CHAR_NUMERIC(stridx[idx])) {
          isNumeric = 0;
          break;
        }
      }    

      if(isNumeric) {
        idx = atoi(stridx);
      } else if(purgeNonIdx) {
        idx = 0;
      } else {
        idx = idxmin;
      }
  
      //fprintf(stderr, "purge_segment '%s' idx:%d ('%s') < %d\n", direntry->d_name, idx, stridx, idxmin);

      if(idx < idxmin) {

        mediadb_prepend_dir(dirpath, direntry->d_name, path, sizeof(path));

        LOG(X_DEBUG("Deleting '%s'"), path);
        if(fileops_DeleteFile(path) != 0) {
          LOG(X_ERROR("Failed to delete '%s'"), path);
        }
      }
    
    }
  }
 
  fileops_CloseDir(pdir);

  return rc;
}

static int httplive_format_path(char *buf, unsigned int sz, const char *prefix, unsigned int idx) {
  int rc;

  rc =  snprintf(buf, sz, "%s%d."HTTPLIVE_TS_NAME_EXT, prefix, idx);

  return rc;
}

int httplive_format_path_prefix(char *buf, unsigned int sz, int outidx, const char *prefix, 
                                const char *adaptationname) {
  int rc;
  char tmp[32];

  if(outidx > 0) {
    snprintf(tmp, sizeof(tmp), "%d", outidx + 1);
  } else {
     tmp[0] = '\0';
  }

  rc = snprintf(buf, sz, "%s%s", tmp, prefix);

  return rc;
}

static int httplive_purgetsfiles(const HTTPLIVE_DATA_T *pLive, unsigned int idxMin) {
  return http_purge_segments(pLive->dir, pLive->fileprefix, "."HTTPLIVE_TS_NAME_EXT, idxMin, 1);
}

static int httplive_delete(HTTPLIVE_DATA_T *pLive) {
  char path[VSX_MAX_PATH_LEN];
  char buf[VSX_MAX_PATH_LEN];
  struct stat st;

  httplive_purgetsfiles(pLive, 0xffffffff);

  // 
  // Delete the playlist
  //
  if(snprintf(buf, sizeof(buf), "%s"HTTPLIVE_PL_NAME_EXT, pLive->fileprefix) >= 0) {
    mediadb_prepend_dir(pLive->dir, buf, path, sizeof(path));
    if(fileops_stat(path, &st) == 0 && fileops_DeleteFile(path) != 0) {
      LOG(X_ERROR("Failed to delete '%s'"), path);
    }
  }

  // 
  // Delete any multi-bitrate playlist
  //
  if(snprintf(buf, sizeof(buf), "%s"HTTPLIVE_PL_NAME_EXT, HTTPLIVE_MULTIBITRATE_NAME_PRFX) >= 0) {
    mediadb_prepend_dir(pLive->dir, buf, path, sizeof(path));
    if(fileops_stat(path, &st) == 0 && fileops_DeleteFile(path) != 0) {
      LOG(X_ERROR("Failed to delete '%s'"), path);
    }
  }

  return 0;
}

static int httplive_writepl(const char *path, int idxmin, int idxmax, HTTPLIVE_DATA_T *pLive) {
  FILE_HANDLE fp;
  int idx;
  int sz = 0;
  int rc = 0;
  char filename[128];
  char buf[4096];
  const char *uriprfxdelimeter = "";
  unsigned int duration = (unsigned int) pLive->duration;
  
  if(pLive->duration > (float) duration) {
    duration++;
  }

  if((rc = snprintf(buf, sizeof(buf), 
                "#EXTM3U\r\n#%s:%d\r\n#%s:%d\r\n",
                HTTPLIVE_EXTX_TARGET_DURATION, duration,
                HTTPLIVE_EXTX_MEDIA_SEQUENCE, idxmin)) > 0) {
    sz += rc; 

    if(rc >= 0 && (rc = snprintf(&buf[sz], sizeof(buf) - sz, "#EXT-X-ALLOW-CACHE:NO\r\n")) > 0) {
      sz += rc;
    }

#if defined(HTTPLIVE_INCLUDE_PROGRAM_DATE_TIME)
    char tmbuf[256];
    time_t tm = s_httplive_tm + (pLive->curIdx * duration);
    strftime(tmbuf, sizeof(tmbuf) - 1, "%Y-%m-%dT%H:%M:%S+08:00", gmtime(&tm));
    //fprintf(stderr, "TM:%u, %u, %d * %d '%s'\n", tm, s_httplive_tm, pLive->curIdx, duration, tmbuf);
    if(rc >= 0 && (rc = snprintf(&buf[sz], sizeof(buf) - sz, "#EXT-X-PROGRAM-DATE-TIME:%s\r\n", tmbuf)) > 0) {
      sz += rc;
    }
#endif // HTTPLIVE_INCLUDE_PROGRAM_DATE_TIME

    if(pLive->uriprefix && (rc = strlen(pLive->uriprefix)) > 0 && pLive->uriprefix[rc - 1] != '/') {
      uriprfxdelimeter = "/";
    } 

    for(idx = idxmin; idx <= idxmax; idx++) {

      httplive_format_path(filename, sizeof(filename), pLive->fileprefix, idx);

      if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "#EXTINF:%d,\r\n%s%s%s\r\n"
          ,duration, 
          (pLive->uriprefix ? pLive->uriprefix : ""), 
          uriprfxdelimeter, filename)) < 0) {
        break;
      } else {
        sz += rc;
      }
    }

  }

  //
  // For live streams, do not end with #EXT-X-ENDLIST
  //

  if(rc >= 0) {

    if((fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
      LOG(X_ERROR("Failed to open httplive playlist '%s' for writing"), path);
      return -1;
    }

    if((rc = fileops_WriteBinary(fp, (unsigned char *) buf, sz)) < 0) {
      LOG(X_ERROR("Failed to write %d to httplive playlist '%s'"), sz, path);
    }

    fileops_Close(fp);

  }

  if(rc >= 0) {
    LOG(X_DEBUG("Updated httplive playlist '%s' (%d - %d)"), path, idxmin, idxmax);
  } else {
    LOG(X_ERROR("Failed to update httplive playlist '%s' (%d - %d)"), path, idxmin, idxmax);
  }

  return rc;
}

static int update_multibr(HTTPLIVE_DATA_T *pLive) {
  STREAMER_CFG_T *pStreamerCfg = (STREAMER_CFG_T *) pLive->pStreamerCfg;
  unsigned int outidx;
  unsigned int numOut = 1;

  if(!pLive || pLive->outidx > 0 || !pStreamerCfg) {
    return -1;
  }

  for(outidx = 1; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
    if(pStreamerCfg->xcode.vid.out[outidx].active) {
      numOut++;
    }
  }

  outidx = 0;
  while(pLive) {
    pLive->count = numOut;

    if(outidx < IXCODE_VIDEO_OUT_MAX) {

      //pLive->autoBitrate = pStreamerCfg->xcode.vid.out[outidx].cfgBitRateOut * 1.15;

      //fprintf(stderr, "CFG BITRATE[%d]: auto:%d, published:%d, (cfg:%d * 1.15)\n", outidx, pLive->publishedBitrate, pLive->autoBitrate, pStreamerCfg->xcode.vid.out[outidx].cfgBitRateOut);
    }

    outidx++;
    if(outidx >= IXCODE_VIDEO_OUT_MAX || !pStreamerCfg->xcode.vid.out[outidx].active) {
      pLive->pnext = NULL;
      break;
    }
    pLive = pLive->pnext;
  }

  return numOut;
}

static int httplive_writepl_multibr(const char *path, const HTTPLIVE_DATA_T *pLiveArg) {
  FILE_HANDLE fp;
  int sz = 0;
  int rc = 0;
  char buf[4096];
  char tmp[32];
  int progId = 1;
  unsigned int bitrate;
  const char *uriprfxdelimeter = "";
  const HTTPLIVE_DATA_T *pLive = pLiveArg;

  if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "#EXTM3U\r\n")) > 0) {
    sz += rc; 
  }

  while(pLive) {

    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, 
                      "#%s:PROGRAM-ID=%d", HTTPLIVE_EXTX_STREAM_INFO, progId)) > 0) {
      sz += rc; 
    }

#define HTTPLIVE_BANDWIDTH(bw) (bw>1000 ? bw/1000*1000 : bw>100 ? bw/100*100 : bw>10 ? bw/10*10 : bw)

    bitrate = pLive->publishedBitrate ? pLive->publishedBitrate : pLive->autoBitrate;

    if(bitrate > 0) {
      if((rc = snprintf(&buf[sz], sizeof(buf) - sz, 
                      ", BANDWIDTH=%u", HTTPLIVE_BANDWIDTH(bitrate))) > 0) {
        sz += rc; 
      }
    }

    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "\r\n")) > 0) {
      sz += rc; 
    }

    if((rc = strlen(pLive->uriprefix)) > 0 && pLive->uriprefix[rc - 1] != '/') {
      uriprfxdelimeter = "/";
    } 

    if(pLive->outidx > 0) {
      snprintf(tmp, sizeof(tmp), "%d", pLive->outidx + 1);
    } else {
      tmp[0] = '\0';
    }

    if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s%s%s%s%s\r\n",
          (pLive->uriprefix[0] != '\0' ? pLive->uriprefix : ""), 
          uriprfxdelimeter,
          tmp,
          //&pLive->fileprefix[pLive->outidx > 0 ? 1 : 0], HTTPLIVE_PL_NAME_EXT)) < 0) {
          HTTPLIVE_TS_NAME_PRFX, HTTPLIVE_PL_NAME_EXT)) < 0) {
      break;
    } else {
      sz += rc;
    }

    pLive = pLive->pnext;
  }

  if(rc >= 0) {

    if((fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
      LOG(X_ERROR("Failed to open '%s' for writing"), path);
      return -1;
    }

    if((rc = fileops_WriteBinary(fp, (unsigned char *) buf, sz)) < 0) {
      LOG(X_ERROR("Failed to write %d to '%s'"), sz, path);
    }

    fileops_Close(fp);

  }

  if(rc >= 0) {
    LOG(X_DEBUG("Updated master playlist '%s'"), path);
  } else {
    LOG(X_ERROR("Failed to update master playlist '%s'"), path);
  }

  return rc;
}

static int httplive_updatepl(HTTPLIVE_DATA_T *pLive) {
  int rc = 0;
  unsigned int idxMin;
  char filename[64];
  char path[VSX_MAX_PATH_LEN];

  if(pLive->curIdx <= 0) {
    return 0;
  }

  if(pLive->curIdx <= pLive->indexCount) {
    idxMin = 0;
  } else {
    idxMin = pLive->curIdx - pLive->indexCount;
  }

  snprintf(filename, sizeof(filename), "%s"HTTPLIVE_PL_NAME_EXT, pLive->fileprefix);
  mediadb_prepend_dir(pLive->dir, filename, path, sizeof(path));

  rc = httplive_writepl(path, idxMin, pLive->curIdx - 1, pLive);

  //
  // Write the master playlist containing bitrate specific playlists
  // only when multi xcode output is set
  //
  if(pLive->curIdx == 1 && pLive->outidx == 0) {

    update_multibr(pLive);
    if(pLive->count > 1) {
      snprintf(filename, sizeof(filename), "%s"HTTPLIVE_PL_NAME_EXT, HTTPLIVE_MULTIBITRATE_NAME_PRFX);
      mediadb_prepend_dir(pLive->dir, filename, path, sizeof(path));
      httplive_writepl_multibr(path, pLive);
    }
  }

  return rc;
}

static int on_new_ts(unsigned int mediaSequenceIndex, int mediaSequenceMin, unsigned int outidx,
                  const char *mediaDir, int curIdx, MPD_CREATE_CTXT_T *pCtxt,
                  uint32_t timescale, uint64_t duration, uint64_t durationPreceeding) {

  MPD_ADAPTATION_T adaptationSet;
  MPD_MEDIA_FILE_T mpdMedia;
  char uniqIdBuf[128];

  memset(&mpdMedia, 0, sizeof(mpdMedia));
  snprintf(uniqIdBuf, sizeof(uniqIdBuf), "%d", curIdx);
  mpdMedia.mediaDir = mediaDir;
  mpdMedia.mediaUniqIdstr = uniqIdBuf;
  mpdMedia.timescale = timescale;
  mpdMedia.duration = duration;
  mpdMedia.durationPreceeding = durationPreceeding;

  memset(&adaptationSet, 0, sizeof(adaptationSet));
  adaptationSet.isvid = 1;
  adaptationSet.isaud = 1;
  adaptationSet.padaptationTag = NULL;
  adaptationSet.pMedia = &mpdMedia;

  return mpd_on_new_mediafile(pCtxt, &adaptationSet, outidx, mediaSequenceIndex, mediaSequenceMin, 0);
}

int httplive_cbOnPkt(void *pUserData, const unsigned char *pPktData, unsigned int len) {
  int rc = 0, rc2;
  struct timeval tv;
  char filename[VSX_MAX_PATH_LEN];
  unsigned int idxMin = 0;
  int keepIdx;
  HTTPLIVE_DATA_T *pLive = (HTTPLIVE_DATA_T *) pUserData;

  if(!pLive || !pPktData) {
    return -1;
  }

  //fprintf(stderr, "httplive_cbOnPkt len:%d, outidx[%d]\n", len, pLive->outidx);

  if(len == 0) {
    LOG(X_WARNING("httplive cb called with 0 length"));
    return 0;  
  }

  gettimeofday(&tv, NULL);

  if(pLive->tvNextRoll.tv_sec == 0 ||
     TIME_TV_DIFF_MS(tv, pLive->tvNextRoll) >= (long) (pLive->duration * 1000.0f)) {

    if(pLive->fs.fp != FILEOPS_INVALID_FP) {
      //LOG(X_DEBUG("HTTPLIVE CLOSING fp:0x%x, fileno:%d %s"), pLive->fs.fp, pLive->fs.fp ? fileno(pLive->fs.fp) : -99, pLive->fs.filename);
      closeFp(pLive);
      //LOG(X_DEBUG("HTTPLIVE CLOSED fp:0x%x, fileno:%d %s"), pLive->fs.fp, pLive->fs.fp ? fileno(pLive->fs.fp) : -99, pLive->fs.filename);
    }

    keepIdx = HTTPLIVE_NUM_INDEXES_KEEP(pLive);

    if(pLive->curIdx > keepIdx + 2) {
      idxMin = pLive->curIdx - keepIdx - 2;
      httplive_purgetsfiles(pLive, idxMin);
    }

    //
    // Update MPEG-DASH playlist using .ts segments
    //
    if(pLive->pMpdMp2tsCtxt && pLive->curIdx > 0) {
      on_new_ts(pLive->curIdx-1, idxMin, pLive->outidx, 
                pLive->dir, pLive->curIdx,
               //pLive->fs.filename, 
                pLive->pMpdMp2tsCtxt, 90000, (pLive->duration * 90000), 
                TIME_TV_DIFF_MS(pLive->tvPriorRoll, pLive->tvRoll0) * 90);
    }

    httplive_format_path(filename, sizeof(filename), pLive->fileprefix, pLive->curIdx);
    mediadb_prepend_dir(pLive->dir, filename, pLive->fs.filename, sizeof(pLive->fs.filename));

    if((pLive->fs.fp = fileops_Open(pLive->fs.filename, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
      LOG(X_ERROR("Failed to open '%s' for writing"), pLive->fs.filename);
      rc = -1;    
    } else {

      LOG(X_DEBUG("Opened '%s'"), pLive->fs.filename);

      rc = httplive_updatepl(pLive);

    }

    pLive->curIdx++;
    if(pLive->tvNextRoll.tv_sec == 0) {
      TIME_TV_SET(pLive->tvNextRoll, tv);
      TIME_TV_SET(pLive->tvRoll0, tv);
    }
    //fprintf(stderr, "TV  at %u:%u\n", tv.tv_sec, tv.tv_usec);
    
    TIME_TV_SET(pLive->tvPriorRoll, pLive->tvNextRoll);
    TV_INCREMENT_MS(pLive->tvNextRoll, (pLive->duration * 1000.0f));
    //fprintf(stderr, "TV next at %u:%u, msElapsed:%u, idxMin:%d, curIdx:%d\n", pLive->tvNextRoll.tv_sec, pLive->tvNextRoll.tv_usec, TIME_TV_DIFF_MS(pLive->tvNextRoll, pLive->tvRoll0), idxMin, pLive->curIdx);

  }

  //LOG(X_DEBUG("HTTPLIVE WRITE  %d fp:0x%x, pPktData:0x%x, fileno:%d %s"), len, pLive->fs.fp, pPktData, pLive->fs.fp ? fileno(pLive->fs.fp) : -99, pLive->fs.filename);
  if(pLive->fs.fp != FILEOPS_INVALID_FP &&
     (rc2 = fileops_WriteBinary(pLive->fs.fp, (unsigned char *) pPktData, len)) != len) {
    LOG(X_ERROR("Failed to write %d/%d to output ts %s"), rc2, len, pLive->fs.filename);

    //LOG(X_DEBUG("HTTPLIVE WRITE ERROR %d, %d fp:0x%x, pPktData:0x%x, fileno:%d %s"), rc2, len, pLive->fs.fp, pPktData, pLive->fs.fp ? fileno(pLive->fs.fp) : -99, pLive->fs.filename);
    return -1;
  }


  return rc;
}

int httplive_init(HTTPLIVE_DATA_T *pLive) {
  struct stat st;
  int rc = 0;

  if(!pLive) {
    return -1;
  }

  pLive->tvNextRoll.tv_sec = 0;
  pLive->tvPriorRoll.tv_sec = 0;
  if(pLive->duration <= 0) {
    pLive->duration = HTTPLIVE_DURATION_DEFAULT;
  }

  if(pLive->fs.fp != FILEOPS_INVALID_FP) {
    CloseMediaFile(&pLive->fs);
  }

  if(fileops_stat(pLive->dir, &st) != 0) {
    LOG(X_ERROR("HTTPLive dir '%s' does not exist."), pLive->dir);
    return -1;
  }

  if(pLive->fileprefix[0] == '\0') {
    strncpy(pLive->fileprefix, HTTPLIVE_TS_NAME_PRFX, sizeof(pLive->fileprefix));
  }

  if(pLive->indexCount == 0) {
    pLive->indexCount = HTTPLIVE_NUM_INDEXES_M3U_DEFAULT;
  } else if(pLive->indexCount < HTTPLIVE_NUM_INDEXES_M3U_MIN) {
    pLive->indexCount = HTTPLIVE_NUM_INDEXES_M3U_MIN;
  } else if(pLive->indexCount > HTTPLIVE_NUM_INDEXES_M3U_MAX) {
    pLive->indexCount = HTTPLIVE_NUM_INDEXES_M3U_MAX;
  }

  httplive_delete(pLive);

  LOG(X_DEBUG("HTTPLive dir: '%s' duration: %.2fs file prefix: '%s'"), 
              pLive->dir, pLive->duration, pLive->fileprefix);

#if defined(HTTPLIVE_INCLUDE_PROGRAM_DATE_TIME) 
  if(s_httplive_tm == 0) {
    s_httplive_tm = time(NULL);
  }
#endif // HTTPLIVE_INCLUDE_PROGRAM_DATE_TIME 

  return rc;
}


int httplive_close(HTTPLIVE_DATA_T *pLive) {
  int rc = 0;

  if(!pLive) {
    return -1;
  }

  if(pLive->fs.fp != FILEOPS_INVALID_FP) {
    CloseMediaFile(&pLive->fs);
  }

  if(pLive->fileprefix[0] == '\0') {
    strncpy(pLive->fileprefix, HTTPLIVE_TS_NAME_PRFX, sizeof(pLive->fileprefix));
  }

  if(!pLive->nodelete) {
    httplive_delete(pLive);
  }

  //
  // Close any MPEG-DASH .mpd context which uses the same .ts segment chunks
  //
  if(pLive->pMpdMp2tsCtxt) {
    mpd_close(pLive->pMpdMp2tsCtxt, 1, 1);
  }

  return rc;
}

int httplive_getindexhtml(const char *host, const char *dir, const char *prfx,
                          unsigned char *pBuf, unsigned int *plen) {
  size_t sz;
  int rc = 0;

  if(!dir || !prfx|| !pBuf || !plen) {
    return -1;
  }

  if((sz = strlen(dir)) < 1) {
    sz = 1;
  }

  //TODO: iOS auto-play work around
  // http://www.roblaplaca.com/examples/html5AutoPlay/

  if((rc = snprintf((char *) pBuf, *plen,
             "<html><head>"
             "<title>"VSX_APPNAME" Live Broadcast</title>"
             "</head><body>"
             "<video controls=\"true\" autoplay=\"true\" src=\"%s%s%s%s"HTTPLIVE_PL_NAME_EXT"\"/>"
             "</body></html>", 
              (host && host[0] != '\0' ? host : ""), dir, dir[sz - 1] == '/' ? "" : "/", prfx)) >= 0) {
    *plen = rc;
  }

  return rc;
}

