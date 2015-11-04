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
#include "vsxlib_int.h"

//
// System globals
//
int g_proc_exit;
int g_verbosity;
//int g_usehttplog;
const char *g_http_log_path;
int g_proc_exit_flags;
TIME_VAL g_debug_ts;
int g_debug_flags = 0;
pthread_cond_t *g_proc_exit_cond;

#if defined(VSX_HAVE_SSL)
pthread_mutex_t g_ssl_mtx;
#endif // VSX_HAVE_SSL


static int isDevCapture(VSXLIB_STREAM_PARAMS_T *pParams) {

  if(pParams->inputs[0] && (!strncasecmp(pParams->inputs[0], "/dev/", 5) ||
      (pParams->strfilters[0] && strstr(pParams->strfilters[0], "trans=dev")))) {
    return 1;
  } 

  return 0;
}


#if defined(VSX_EXTRACT_CONTAINER)
static int extractFromM2t(int video, int audio,
                        const char *in, 
                        const char *userOut, 
                        char *outPath, 
                        unsigned int outPathTotLen,
                        unsigned int outPathEndIdx,
                        float fStart,
                        float fDuration) {
  FILE_STREAM_T fileIn;
  int rc = 0;

  if(fStart != 0 || fDuration != 0) {
    LOG(X_ERROR("Partial extract not implemented for mpeg2-ts"));
    return -1;
  }

  if(OpenMediaReadOnly(&fileIn, in) < 0) {
    return -1;
  }

  if(video && audio) {
    // TODO: sync time if duration given
  }

  rc = extractfrom_mp2ts(&fileIn, outPath, (userOut ? 1 : 0), fDuration, video, audio);

  CloseMediaFile(&fileIn);


  return 0;
}


static int extractFromMp4(int video, 
                          int audio,
                          const char *in, 
                          const char *userOut, 
                          char *outPath, 
                          unsigned int outPathTotLen,
                          unsigned int outPathEndIdx,
                          float fStart,
                          float fDuration) {
  MP4_CONTAINER_T *pMp4;
  int rc = 0;

  if((pMp4 = mp4_open(in, 1)) == NULL) {
    LOG(X_ERROR("Failed to read %s"), in);
    return -1;
  }

  if(video && audio) {
    // TODO: sync time if duration given
  }

   rc = mp4_extractRaw(pMp4, outPath, fStart, fDuration, userOut ? 1 : 0, video, audio);

  mp4_close(pMp4);

  return rc;
}

static int extractFromFlv(int video, 
                          int audio,
                          const char *in, 
                          const char *userOut, 
                          char *outPath, 
                          unsigned int outPathTotLen,
                          unsigned int outPathEndIdx,
                          float fStart,
                          float fDuration) {
  FLV_CONTAINER_T *pFlv;
  int rc = 0;

  if((pFlv = flv_open(in)) == NULL) {
    LOG(X_ERROR("Failed to read %s"), in);
    return -1;
  }

  if(video && audio) {
    // TODO: sync time if duration given
  }

  rc = flv_extractRaw(pFlv, outPath, fStart, fDuration, userOut ? 1 : 0, video, audio);

  flv_close(pFlv);

  return rc;
}

VSX_RC_T vsxlib_extractMedia(int video, 
                               int audio,
                               const char *in, 
                               const char *out, 
                               const char *startSec,
                               const char *durationSec) {
  VSX_RC_T rc = VSX_RC_OK;
  float fDuration = 0.0f;
  float fStart= 0.0f;
  size_t sz = 0;
  char outPath[VSX_MAX_PATH_LEN];
  FILE_STREAM_T fileIn;
  enum MEDIA_FILE_TYPE mediaType;

  if(!in) {
    LOG(X_ERROR("No input file specified"));
    return VSX_RC_ERROR;
  }

  if(OpenMediaReadOnly(&fileIn, in) < 0) {
    return VSX_RC_ERROR;
  }

  if((mediaType =  filetype_check(&fileIn)) == MEDIA_FILE_TYPE_UNKNOWN) {
    LOG(X_ERROR("Unrecognized file format '%s'"), fileIn.filename);
    CloseMediaFile(&fileIn);
    return VSX_RC_ERROR;
  }

  CloseMediaFile(&fileIn);

  if(durationSec) {
    if((fDuration = (float) atof(durationSec)) <= 0.0) {
      LOG(X_ERROR("Invalid duration %s"), durationSec);
      return VSX_RC_ERROR;
    }
  }
  if(startSec) {
    if((fStart = (float) atof(startSec)) <= 0.0) {
      LOG(X_ERROR("Invalid start offset%s"), startSec);
      return VSX_RC_ERROR;
    }
  }

  if(out) {
    if((sz = path_getLenNonPrefixPart(out)) > sizeof(outPath) - 6) {
      sz -= 6;
    }
    memset(outPath, 0, sizeof(outPath));
    strncpy(outPath, out, sizeof(outPath) - 6);
  } else {
    if((sz = path_getLenNonPrefixPart(in)) > sizeof(outPath) - 6) {
      sz -= 6;
    }
    memset(outPath, 0, sizeof(outPath));
    memcpy(outPath, in, sz);
  }

  switch(mediaType) {
    case MEDIA_FILE_TYPE_FLV:
      if(extractFromFlv(video, audio, in, out, outPath, sizeof(outPath), 
                          sz, fStart, fDuration) < 0) {
        rc = VSX_RC_ERROR;
      }
      break;
    case MEDIA_FILE_TYPE_MP4:
      if(extractFromMp4(video, audio, in, out, outPath, sizeof(outPath), 
                          sz, fStart, fDuration) < 0) {
        rc = VSX_RC_ERROR;
      }
      break;
    case MEDIA_FILE_TYPE_MP2TS:
      if(extractFromM2t(video, audio, in, out, outPath, sizeof(outPath), 
                          sz, fStart, fDuration) < 0) {
        rc = VSX_RC_ERROR;
      }
      break;
    default:
      LOG(X_ERROR("Unsupported file format '%s'"), fileIn.filename);
      return VSX_RC_ERROR;    
  }

  return rc;
}
#endif // VSX_EXTRACT_CONTAINER


#if defined(VSX_CREATE_CONTAINER)
VSX_RC_T vsxlib_createMp4(const char *in,  
                            const char *out, 
                            double fps, 
                            const char *avsyncin,
                            int esdsObjTypeIdxPlus1) {
  size_t sz;
  char outPath[VSX_MAX_PATH_LEN];
  FILE_HANDLE fp;
  const char *pOut = out;
  FILE_STREAM_T streamIn;
  enum MEDIA_FILE_TYPE mediaType;
  VSX_RC_T rc = VSX_RC_OK;

  if(!in) {
    LOG(X_ERROR("No input file specified"));
    return VSX_RC_ERROR;
  }

  if(OpenMediaReadOnly(&streamIn, in) < 0) {
    return VSX_RC_ERROR;
  }

  if((mediaType =  filetype_check(&streamIn)) == MEDIA_FILE_TYPE_UNKNOWN) {
    LOG(X_ERROR("Unrecognized file format '%s'"), streamIn.filename);
    CloseMediaFile(&streamIn);
    return VSX_RC_ERROR;
  }

  if(!out) {
    if((sz = path_getLenNonPrefixPart(in)) > sizeof(outPath) - 5) {
      sz -= 5;
    }
    memset(outPath, 0, sizeof(outPath));
    strncpy(outPath, in, sizeof(outPath) - 5);
    strncpy(&outPath[sz], ".mp4", sizeof(outPath) - sz - 1);

    if((fp = fileops_Open(outPath, O_RDONLY)) != FILEOPS_INVALID_FP) {
      fileops_Close(fp);
      LOG(X_ERROR("File %s already exists.  Use '--out=' to add content."), outPath);
      CloseMediaFile(&streamIn);
      return VSX_RC_OK;
    }

    pOut = outPath;
  }

  switch(mediaType) {
    case MEDIA_FILE_TYPE_H264b:
    case MEDIA_FILE_TYPE_MP4V:
      if(mp4_create(pOut, NULL, &streamIn, 0, fps, MEDIA_FILE_TYPE_UNKNOWN, 
                      mediaType, avsyncin, 0) < 0) {
        rc = VSX_RC_ERROR;
      }
      break;
    case MEDIA_FILE_TYPE_AAC_ADTS:
    case MEDIA_FILE_TYPE_AMRNB:
      if(fps == 0) {
        LOG(X_WARNING("Frame length not specified.  Using default of 1024 samples/frame."));
        fps = 1024;
      }
      if(mp4_create(pOut, &streamIn, NULL, fps, 0, mediaType, 
                  MEDIA_FILE_TYPE_UNKNOWN, avsyncin, esdsObjTypeIdxPlus1) < 0) {
        rc = VSX_RC_ERROR;
      }
      break;
    default:
      rc = VSX_RC_ERROR;
      LOG(X_ERROR("Unsupported file format '%s'"), streamIn.filename);
      break;
  }
  
  CloseMediaFile(&streamIn);
  
  if(rc == VSX_RC_OK) {
    LOG(X_INFO("Created %s"), pOut);
  }

  return rc;
}
#endif // VSX_CREATE_CONTAINER

#if defined(VSX_DUMP_CONTAINER)
static int dumpMp4_avsync(const MP4_CONTAINER_T *pMp4, const char *avsyncout) {
  //FILE *fp = stdout;
  FILE_HANDLE fp;
  MP4_TRAK_AVCC_T mp4BoxSetAvcc;
  MP4_TRAK_MP4V_T mp4BoxSetMp4v;
  MP4_TRAK_T *pTrak = NULL;
  char descr[16];
  char pathout[VSX_MAX_PATH_LEN];
  unsigned int idx = 0;

  if(!avsyncout || avsyncout[0] == '\0') {
    snprintf(pathout, sizeof(pathout), "%s.stts", pMp4->pStream->cbGetName(pMp4->pStream));
  } else {
    strncpy(pathout, avsyncout, sizeof(pathout) - 1);
    pathout[sizeof(pathout) - 1] = '\0';
  }

  memset(&mp4BoxSetAvcc, 0, sizeof(mp4BoxSetAvcc));
  if(mp4_loadTrack(pMp4->proot, &mp4BoxSetAvcc.tk, *((uint32_t *) "avc1"), 1)) {
    if(mp4_initAvcCTrack(pMp4, &mp4BoxSetAvcc) != 0) {
      return -1;
    }
    pTrak = &mp4BoxSetAvcc.tk;
    snprintf(descr, sizeof(descr), "avc1");
  } 

  if(!pTrak) {
    memset(&mp4BoxSetMp4v, 0, sizeof(mp4BoxSetMp4v));
    if(mp4_loadTrack(pMp4->proot, &mp4BoxSetMp4v.tk, *((uint32_t *) "mp4v"), 1)) {
      if(mp4_initMp4vTrack(pMp4, &mp4BoxSetMp4v) != 0) {
        return -1;
      }
    }
    pTrak = &mp4BoxSetMp4v.tk;
    snprintf(descr, sizeof(descr), "mp4v");
  }

  if(!pTrak) {
    LOG(X_ERROR("Unable to find suitable video track."));
    return -1;
  }

  if((fp = fileops_Open(pathout, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open file for writing: %s"), pathout);
    return -1;
  }

  LOG(X_INFO("Created %s"), pathout);

  fileops_WriteExt(fp, pathout, sizeof(pathout), "#av sync for %s\n", 
                      pMp4->pStream->cbGetName(pMp4->pStream));
  //fprintf(fp, "#%s %uHz \n", descr, pTrak->pMdhd->timescale); 
  fileops_WriteExt(fp, pathout, sizeof(pathout), "#track=%s\n", descr);
  fileops_WriteExt(fp, pathout, sizeof(pathout), "stts=%u\n", 
                      pTrak->pStts->list.entrycnt);
  fileops_WriteExt(fp, pathout, sizeof(pathout), "clock=%u\n", 
                      pTrak->pMdhd->timescale);
  for(idx = 0; idx < pTrak->pStts->list.entrycnt; idx++) {
    fileops_WriteExt(fp, pathout, sizeof(pathout), "count=%u,delta=%u\n", 
                    pTrak->pStts->list.pEntries[idx].samplecnt,
                    pTrak->pStts->list.pEntries[idx].sampledelta);
  }

  fileops_Close(fp);

  return 0;
}

static int dumpMp4(const char *in, int verbosity, const char *avsyncout) {
  MP4_CONTAINER_T *pMp4 = NULL;

  if(!in) {
    LOG(X_ERROR("No input file specified"));
    return -1;
  }

  // TODO: autodetect mp4 | .h264 (start code)

  if((pMp4 = mp4_open(in, 0)) == NULL) {
    LOG(X_ERROR("Failed to read %s"), in);
    return -1;
  }


  if(avsyncout) {
    if(dumpMp4_avsync(pMp4, avsyncout) < 0) {
      LOG(X_ERROR("Failed to dump av sync"));
    }
  } else {
    mp4_dump(pMp4, verbosity, stdout);
  }

  mp4_close(pMp4);
  return 0;
}

static int dumpMp2ts(const char *in, int verbosity, int readmeta) {
  MP2TS_CONTAINER_T *pMp2ts;

  if((!readmeta || !(pMp2ts = mp2ts_openfrommeta(in))) &&
     !(pMp2ts = mp2ts_open(in, NULL, 0, 0))) {
    LOG(X_ERROR("Failed to read %s"), in);
    return -1;
  }

  mp2ts_dump(pMp2ts, verbosity, stdout);

  mp2ts_close(pMp2ts);
  return 0;
}

static int dumpMpeg2(const char *in, int verbosity) {
  MPEG2_CONTAINER_T *pMpg2 = NULL;

  if((pMpg2 = mpg2_open(in))) {
    LOG(X_ERROR("Failed to read %s"), in);
    return -1;
  } 

  mpg2_close(pMpg2);
  return 0;
}

static int dumpFlv(const char *in, int verbosity) {
  FLV_CONTAINER_T *pFlv = NULL;
  int rc = 0;

  if(!(pFlv = flv_open(in))) {
    return -1;
  }

  flv_dump(pFlv, verbosity, stdout);

  flv_close(pFlv);

  return rc;
}

static int dumpMkv(const char *in, int verbosity) {
  MKV_CONTAINER_T *pMkv = NULL;
  int rc = 0;

  if(!(pMkv = mkv_open(in))) {
    return -1;
  }

  mkv_dump(stdout, pMkv);

  mkv_close(pMkv);

  return rc;
}

int vsxlib_dumpContainer(const char *in, int verbosity, const char *avsyncout, 
                         int readmeta) {
  enum MEDIA_FILE_TYPE mediaType;
  FILE_STREAM_T streamIn;
  int rc = -1;

  if(OpenMediaReadOnly(&streamIn, in) < 0) {
    return -1;
  }

  mediaType = filetype_check(&streamIn);

  switch(mediaType) {
    case MEDIA_FILE_TYPE_MP4:
      CloseMediaFile(&streamIn);
      return dumpMp4(in, verbosity, avsyncout);
    case MEDIA_FILE_TYPE_H262:
    case MEDIA_FILE_TYPE_H263:
    case MEDIA_FILE_TYPE_H263_PLUS:
    case MEDIA_FILE_TYPE_MPEG2:
    case MEDIA_FILE_TYPE_MP4V:
      CloseMediaFile(&streamIn);
      return dumpMpeg2(in, verbosity);
    case MEDIA_FILE_TYPE_FLV:
      CloseMediaFile(&streamIn);
      return dumpFlv(in, verbosity);
    case MEDIA_FILE_TYPE_MKV:
    case MEDIA_FILE_TYPE_WEBM:
      CloseMediaFile(&streamIn);
      return dumpMkv(in, verbosity);
    case MEDIA_FILE_TYPE_MP2TS:
      CloseMediaFile(&streamIn);
      return dumpMp2ts(in, verbosity, readmeta);
    case MEDIA_FILE_TYPE_AMRNB:
      rc = amr_dump(&streamIn, verbosity);
      break;
    default:
      LOG(X_ERROR("Unrecognized container format for '%s'"), in);
    break;
  }

  CloseMediaFile(&streamIn);

  return rc;
}
#endif // VSX_DUMP_CONTAINER

#if defined(VSX_HAVE_VIDANALYZE)
int vsxlib_analyzeH264(const char *in, double fps, int verbosity) {
  MP4_CONTAINER_T *pMp4 = NULL;
  FLV_CONTAINER_T *pFlv= NULL;
  FILE_STREAM_T streamIn;
  enum MEDIA_FILE_TYPE mediaType;
  int rc = -1;

  if(!in) {
    LOG(X_ERROR("No input file specified"));
    return -1;
  }

  if(OpenMediaReadOnly(&streamIn, in) < 0) {
    return -1;
  }

  mediaType = filetype_check(&streamIn);

  if(streamIn.offset != 0) {
    if(fileops_Fseek(streamIn.fp, 0, SEEK_SET) != 0) {
      LOG(X_ERROR("Unable to seek to start of file '%s'."), streamIn.filename);
      CloseMediaFile(&streamIn);
      return -1;
    }
    streamIn.offset = 0;
  }

  switch(mediaType) {
    case MEDIA_FILE_TYPE_H264b:
      rc = h264_analyze_annexB(&streamIn, stdout, fps, verbosity);
      CloseMediaFile(&streamIn);
    break;
    case MEDIA_FILE_TYPE_FLV: 
      CloseMediaFile(&streamIn);
      pFlv = flv_open(in);
      rc = h264_analyze_flv(pFlv, stdout, fps, verbosity);
      flv_close(pFlv);
      break;
    case MEDIA_FILE_TYPE_MP4: 
      CloseMediaFile(&streamIn);
      pMp4 = mp4_open(in, 1);
      rc = h264_analyze_mp4(pMp4, stdout, fps, verbosity);
      mp4_close(pMp4);
      break;
    case MEDIA_FILE_TYPE_MP2TS:
    case MEDIA_FILE_TYPE_MP2TS_BDAV:
      CloseMediaFile(&streamIn);
      LOG(X_ERROR("Unsupported file type"));
    default:
      LOG(X_ERROR("Unrecognized file type"));
      CloseMediaFile(&streamIn); 
      break;
  }

  return rc;
}
#endif // (VSX_HAVE_VIDANALYZE)

#if defined(VSX_HAVE_CAPTURE)

int vsxlib_streamLiveCaptureCfg(CAPTURE_LOCAL_DESCR_T *pCapCfg,
                                STREAMER_CFG_T *pStreamerCfg) {
  int rc;

  rc = capture_net_async(pCapCfg,  pStreamerCfg, NULL);

  return rc;
}

VSX_RC_T vsxlib_captureNet(VSXLIB_STREAM_PARAMS_T *pParams) {

  VSX_RC_T rc = VSX_RC_OK;
  int sz;
  SDP_DESCR_T sdp;
  CAPTURE_RECORD_DESCR_T recCfg;
  CAPTURE_LOCAL_DESCR_T capCfg;
  SOCKET_LIST_T sockList;
  CAPTURE_FILTER_TRANSPORT_T transType;
  //unsigned int pktQueueSlotsCnt = PKTQUEUE_LEN_DEFAULT;
  VSXLIB_PRIVDATA_T *pPrivData;
  unsigned int idx;
  int isdev = 0;
  char buf[256];
  const char *inputs[2];
  char input1[VSX_MAX_PATH_LEN];

  if(!pParams || !pParams->pPrivate) {
    return VSX_RC_ERROR;
  }

  inputs[0] = avc_dequote(pParams->inputs[0], input1, sizeof(input1));
  inputs[1] = pParams->inputs[1];

  pPrivData = (VSXLIB_PRIVDATA_T *) pParams->pPrivate;

  memset(&capCfg, 0, sizeof(capCfg));

  vsxlib_initlog(pParams->verbosity, pParams->logfile, NULL, pParams->logmaxsz, pParams->logrollmax, NULL);
  http_log_setfile(pParams->httpaccesslogfile);

  vsxlib_stream_setupcap_params(pParams, &capCfg);

  capCfg.common.rtsptranstype = pParams->rtsptranstype;
  capCfg.common.localAddrs[0] = inputs[0];
  capCfg.common.localAddrs[1] = inputs[1];
  transType = capture_parseTransportStr(&capCfg.common.localAddrs[0]);

  //TODO: should enable upon testcase
  //if(IS_CAPTURE_FILTER_TRANSPORT_DTLS(capCfg.common.filt.filters[0].transType) && vsxlib_stream_setupcap_dtls(pParams, pCapCfg) < 0) {
  //  return -1;
  //} 

  //
  // Check if input is actually an sdp file 
  //
  if(inputs[0] != NULL && (sdp_isvalidFilePath(inputs[0]) || !strncasecmp(inputs[0], "sdp://", 6))) {

    if((sz = capture_filterFromSdp(inputs[0], capCfg.common.filt.filters, buf, 
                                   sizeof(buf), &sdp, pParams->novid, pParams->noaud)) <= 0) {
      return VSX_RC_ERROR;
    }

    if(pParams->verbosity >= VSX_VERBOSITY_HIGH) {
      sdp_write(&sdp, NULL, S_DEBUG);
    }

    capCfg.common.filt.numFilters = sz;
    capCfg.common.localAddrs[0] = buf;
    capCfg.common.localAddrs[1] = NULL;

    pParams->strfilters[0] = pParams->strfilters[1] = NULL;

  } else if(isDevCapture(pParams)) {
    isdev = 1;
    capCfg.common.pCapDevCfg = &((VSXLIB_PRIVDATA_T *)pParams->pPrivate)->capDevCfg;
  }

  for(idx = 0; idx < 2; idx++) {
    if(pParams->strfilters[idx]) {
      if(capture_parseFilter(pParams->strfilters[idx], &capCfg.common.filt.filters[idx]) < 0) {
        return VSX_RC_ERROR;
      }
      capCfg.common.filt.numFilters++;
    }
  }

  if(capCfg.common.filt.filters[0].transType == CAPTURE_FILTER_TRANSPORT_UNKNOWN &&
     transType != CAPTURE_FILTER_TRANSPORT_UNKNOWN) {
    capCfg.common.filt.filters[0].transType = transType;
  }
  capCfg.common.filt.filters[1].transType = capCfg.common.filt.filters[1].transType;

  if(pParams->pktqslots > PKTQUEUE_LEN_MAX) {
    pParams->pktqslots = PKTQUEUE_LEN_MAX;
  } else if(pParams->pktqslots <= 0) {
    pParams->pktqslots = PKTQUEUE_LEN_DEFAULT;
  }

  pPrivData->inUse = 1;

  recCfg.outDir = pParams->dir;
  recCfg.overwriteOut = pParams->overwritefile;

  if(isdev || (capCfg.common.filt.numFilters > 0 && pParams->pktqslots > 0 &&
               capture_parseLocalAddr(capCfg.common.localAddrs[0], &sockList)) > 0) {

    for(idx = 0; idx < capCfg.common.filt.numFilters; idx++) {
      capCfg.capActions[idx].cmd = CAPTURE_PKT_ACTION_QUEUE;
      capCfg.capActions[idx].pktqslots = pParams->pktqslots; 
      capCfg.common.filt.filters[idx].pCapAction = &capCfg.capActions[idx];
    }

    if(capture_net_async(&capCfg,  NULL, &recCfg) < 0) {
      rc = VSX_RC_ERROR;
    }
  } else {

    // pktqueue not implemented for pcap based capture
    if(capture_net_sync(&capCfg,  &recCfg) < 0) {
      rc = VSX_RC_ERROR; 
    }
  }

  capture_freeFilters(capCfg.common.filt.filters, capCfg.common.filt.numFilters);
  pPrivData->inUse = 0;

  return rc;
}

#endif // VSX_HAVE_CAPTURE

#if defined(VSX_HAVE_STREAMER)

static int checkEnablePesExtract(int isRtpOutOn, int isXcodeOn, 
                                 int isRtspOn, int isRtmpOn, int isFlvliveOn, int isMkvliveOn,
                                 int isHttpliveOn, int isTsLiveOn, int isMoofOn) {
  if(isXcodeOn || isRtpOutOn || isRtspOn || isRtmpOn || isFlvliveOn || 
     isMkvliveOn || isHttpliveOn || isTsLiveOn || isMoofOn) {
    LOG(X_DEBUGV("Enabling PES extraction for output stream"));
    return BOOL_ENABLED_OVERRIDE;
  } else {
    return BOOL_DISABLED_OVERRIDE;
  }
}

static int setupDash(MOOFSRV_CTXT_T *pMoofCtxts, STREAMER_CFG_T *pStreamerCfg,
                     const VSXLIB_STREAM_PARAMS_T *pParams) {
  int rc = 0;
  //size_t sz;
  unsigned int outidx, idx;
  STREAMER_OUTFMT_T *pLiveFmt;
  unsigned int maxMoofRecord = 0;
  //int alter_filename = 0;
  char buf[VSX_MAX_PATH_LEN];
  buf[0] = '\0';

#if defined(VSX_HAVE_LICENSE)

  if((pStreamerCfg->lic.capabilities & LIC_CAP_NO_DASHLIVE)) {
    LOG(X_ERROR("MPEG-DASH server not enabled in license"));
    return -1;
  }

#endif // VSX_HAVE_LICENSE

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
    if(outidx == 0 || pStreamerCfg->xcode.vid.out[outidx].active) {
      maxMoofRecord++;
    }
  }

  //
  // Setup Moof server parameters
  //
  pLiveFmt = &pStreamerCfg->action.liveFmts.out[STREAMER_OUTFMT_IDX_MOOF];
  if((*((unsigned int *) &pLiveFmt->max) = maxMoofRecord) > 0 &&
    !(pLiveFmt->poutFmts = (OUTFMT_CFG_T *)
                  avc_calloc(maxMoofRecord, sizeof(OUTFMT_CFG_T)))) {
    return -1;
  }
  pthread_mutex_init(&pLiveFmt->mtx, NULL);
  pLiveFmt->qCfg.id = STREAMER_QID_MOOF;
  pLiveFmt->qCfg.maxPkts = pStreamerCfg->action.outfmtQCfg.cfgMoof.maxPkts;
  pLiveFmt->qCfg.maxPktLen = pStreamerCfg->action.outfmtQCfg.cfgMoof.maxPktLen;
  pLiveFmt->qCfg.growMaxPktLen = pStreamerCfg->action.outfmtQCfg.cfgMoof.growMaxPktLen;
  pLiveFmt->do_outfmt = 1;

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(outidx > 0 && !pStreamerCfg->xcode.vid.out[outidx].active) {
      continue;
    }

    pMoofCtxts[outidx].av.vid.pStreamerCfg = pStreamerCfg;
    pMoofCtxts[outidx].av.aud.pStreamerCfg = pStreamerCfg;
    pMoofCtxts[outidx].pnovid = &pStreamerCfg->novid;
    pMoofCtxts[outidx].pnoaud = &pStreamerCfg->noaud;
    pMoofCtxts[outidx].requestOutIdx = outidx;

    if(pParams->dash_moof_segments == 1) {

      pMoofCtxts[outidx].do_moof_segmentor = 1;
      pMoofCtxts[outidx].moofInitCtxt.clockHz = 3000;
      //pMoofCtxts[outidx].moofInitCtxt.clockHz = 90000;
      //pMoofCtxts[outidx].moofInitCtxt.clockHz = 10000000;
      pMoofCtxts[outidx].moofInitCtxt.nodelete = pParams->moof_nodelete;
      pMoofCtxts[outidx].moofInitCtxt.useInitMp4 = pParams->moofUseInitMp4;

      if((pMoofCtxts[outidx].moofInitCtxt.useRAP = pParams->moofUseRAP)) {
        //
        // Attempt to create new mp4 on keyframe after min duration has been reached
        //
        pMoofCtxts[outidx].moofInitCtxt.mp4MinDurationSec = pParams->moofMp4MinDurationSec; 
      } else {
        pMoofCtxts[outidx].moofInitCtxt.mp4MinDurationSec = 0;
      }
      pMoofCtxts[outidx].moofInitCtxt.mp4MaxDurationSec = pParams->moofMp4MaxDurationSec;
      pMoofCtxts[outidx].moofInitCtxt.moofTrafMaxDurationSec = pParams->moofTrafMaxDurationSec; 
      pMoofCtxts[outidx].moofInitCtxt.requestOutidx = outidx;
      pMoofCtxts[outidx].moofInitCtxt.mixAudVid = 0;

      //
      // Check if we should create a new .m4s segment bondary for each xcode outidx 
      // at the same timestamp boundary
      //
      if(pParams->moofSyncSegments > 0 ||
         (maxMoofRecord > 1 && pParams->moofSyncSegments == -1)) {

        pMoofCtxts[outidx].moofInitCtxt.syncMultiOutidx = 1;

        if(outidx == 0) {
          pMoofCtxts[outidx].moofInitCtxt.syncedByOtherOutidx = 0;
          for(idx = 1; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
            if(pStreamerCfg->xcode.vid.out[idx].active) {
              pMoofCtxts[outidx].moofInitCtxt.psharedCtxts[idx] = &pMoofCtxts[idx].createCtxt;
            }
          }
        } else {
          pMoofCtxts[outidx].moofInitCtxt.syncedByOtherOutidx = 1;
        }

      }

    } // end of if(do_moofrecord...

    //
    // Set the MOOF recording output directory
    //
    if(pParams->moofdir && pParams->moofdir[0] != DIR_DELIMETER) {
      mediadb_prepend_dir(NULL, pParams->moofdir,
                         (char *) pMoofCtxts[outidx].dashInitCtxt.outdir, 
                         sizeof(pMoofCtxts[outidx].dashInitCtxt.outdir));
    } else if(pParams->moofdir) {
      strncpy((char *) pMoofCtxts[outidx].dashInitCtxt.outdir, 
              pParams->moofdir, sizeof(pMoofCtxts[outidx].dashInitCtxt.outdir));
    }

    //
    // Configure the DASH mpd playlist generator
    //
    pMoofCtxts[outidx].dashInitCtxt.enableDash = 1;
    pMoofCtxts[outidx].dashInitCtxt.indexcount = pParams->moofindexcount;
    pMoofCtxts[outidx].dashInitCtxt.outfileprefix = pParams->mooffileprefix;
    pMoofCtxts[outidx].dashInitCtxt.uriprefix = pParams->moofuriprefix;
    pMoofCtxts[outidx].dashInitCtxt.pAuthTokenId = pParams->tokenid;

    if((pMoofCtxts[outidx].dashInitCtxt.dash_mpd_type = pParams->dash_mpd_type) == 
       DASH_MPD_TYPE_INVALID) {
      pMoofCtxts[outidx].dashInitCtxt.dash_mpd_type = DASH_MPD_TYPE_DEFAULT;
      LOG(X_DEBUG("Using dash_mpd_type: %d"), pMoofCtxts[outidx].dashInitCtxt.dash_mpd_type);
    }

    if((rc = moofsrv_record_start(&pMoofCtxts[outidx], pLiveFmt)) < 0) {
      break;
    }

  } // end of for(outidx...

  if(rc < 0) {
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      moofsrv_record_stop(&pMoofCtxts[outidx]);
    }
  }

  return rc;
}


int vsxlib_streamPlaylistCfg(PLAYLIST_M3U_T *pl,
                             STREAMER_CFG_T *pStreamerCfg,
                             int loopCurFile,
                             int loopPl,
                             int virtPl) {

  char filepath[VSX_MAX_PATH_LEN];
  char *pfilepath;
  struct stat st;
  int rc = 0;
  int consec_errors = 0;
  int numplayed;
  PLAYLIST_M3U_ENTRY_T *pCur = NULL;

  if(!pl || !pl->pEntries || !pStreamerCfg) {
    return -1;
  }

  pStreamerCfg->loop = loopCurFile;
  pStreamerCfg->loop_pl = loopPl;

  if(!(pCur = pl->pCur)) {
    pCur = &pl->pEntries[0];
  }

  do {

    numplayed = 0;

    while(pCur) {

      pfilepath = NULL;

      if(fileops_stat(pCur->path, &st) == 0) {
        pfilepath = pCur->path;
      } else {
        mediadb_prepend_dir(pl->dirPl, pCur->path, filepath, sizeof(filepath));
        if(fileops_stat(filepath, &st) == 0) {
          pfilepath = filepath;
        } else {
          LOG(X_WARNING("Unable to find %s or %s"), pCur->path, filepath);
        }
      }

      if(pfilepath) {

        LOG(X_DEBUG("Playing playlist entry %s"), pCur->path);

        if(pStreamerCfg->status.useStatus) {
          strncpy(pStreamerCfg->status.pathFile, pfilepath, 
                  sizeof(pStreamerCfg->status.pathFile) -1);
          pStreamerCfg->status.vidTnCnt = VID_TN_CNT_NOT_SET;
        }
        //TODO: need to memset pStreamerCfg->pStorage->xcodeCtxt contents, other than what is 
        //to be preserved as config values
        if((rc = vsxlib_streamFileCfg(pfilepath, 0, pStreamerCfg)) == 0) {
          numplayed++;
          consec_errors = 0;
        }
        if(pStreamerCfg->proto == STREAM_PROTO_MP2TS) {
          pStreamerCfg->u.cfgmp2ts.firstProgId += 2;
        }

      }

      if(g_proc_exit != 0 || pStreamerCfg->quitRequested) {
        LOG(X_DEBUG("breaking out of playlist"));
        break;
      } else if(!pStreamerCfg->loop_pl  && !pStreamerCfg->loop) {

        if(virtPl || !pCur->pnext) {
          break;
        }

      } else if(rc != 0) {
        if(++consec_errors > 2) {
          LOG(X_ERROR("breaking out of playlist after %d consecutive errors"), 
              consec_errors);
          break;
        }
      } 

      pCur = pCur->pnext;
    }

    // Avoid endless looping of empty contents
    if(pStreamerCfg->loop_pl && numplayed == 0) {
      LOG(X_DEBUG("Avoiding looping empty or invalid playlist in %s"), pl->dirPl);
      break;
    } else if(pStreamerCfg->loop_pl) {
      pCur = &pl->pEntries[0];
    }

  } while(g_proc_exit == 0 && pStreamerCfg->loop_pl && 
          pStreamerCfg->quitRequested == 0);

  return 0;
}

static int stream_playlist(const char *path,
                          STREAMER_CFG_T *pStreamerCfg,
                          int loop) {

  PLAYLIST_M3U_T *pl = NULL;
  int rc;

  if(!pStreamerCfg) {
    return -1;
  }

  if(pStreamerCfg->curPlRecurseLevel >= pStreamerCfg->maxPlRecurseLevel) {
    LOG(X_WARNING("sub[%d] playlist %s ignored"), 
        pStreamerCfg->curPlRecurseLevel + 1, path); 
    return 0;
  }
  pStreamerCfg->curPlRecurseLevel++;

  if((pl = m3u_create(path)) == NULL) {
    LOG(X_ERROR("Unable to read playlist %s"), path);
    return -1;
  }

  LOG(X_DEBUG("Read playlist %s"), path);

  rc = vsxlib_streamPlaylistCfg(pl,pStreamerCfg, 0, loop, 0);

  m3u_free(pl, 1);

  pStreamerCfg->curPlRecurseLevel--;

  return rc;
}


int vsxlib_streamFileCfg(const char *filePath, double fps, 
                           STREAMER_CFG_T *pStreamerCfg) {

  int rc = 0;
  enum MEDIA_FILE_TYPE mediaType;
  FILE_STREAM_T streamIn;

  //
  // Since we're not streaming from a live capture, we don't use any capture RTP sockets
  //
  pStreamerCfg->cfgrtp.rtp_useCaptureSrcPort = 0;

  //
  // We may wish to setup TURN relay(s) when adding a stream output destination since we're not 
  // streaming live input.  For live input capture, TURN is setup from capture_socket
  //
  pStreamerCfg->cfgrtp.do_setup_turn = 1;

  if(pStreamerCfg->pip.pMixerCfg && pStreamerCfg->pip.pMixerCfg->conferenceInputDriver) {
    mediaType = MEDIA_FILE_TYPE_VID_CONFERENCE;
  } else {

    if(!filePath) {
      LOG(X_ERROR("No input file specified"));
      return -1;
    } else if(OpenMediaReadOnly(&streamIn, filePath) < 0) {
       return -1;
    } else if((mediaType =  filetype_check(&streamIn)) == MEDIA_FILE_TYPE_UNKNOWN) {
      LOG(X_ERROR("Unrecognized file format '%s'"), streamIn.filename);
      CloseMediaFile(&streamIn);
      return -1;
    }

  }

  if(pStreamerCfg->status.useStatus) {
    if(filePath) {
      strncpy(pStreamerCfg->status.pathFile, filePath,
          sizeof(pStreamerCfg->status.pathFile) -1);
    }
    pStreamerCfg->status.vidProps[0].mediaType = mediaType; 
  }

  switch(mediaType) {

    case MEDIA_FILE_TYPE_H264b:
      if(pStreamerCfg->proto == STREAM_PROTO_AUTO) {
        pStreamerCfg->proto = STREAM_PROTO_MP2TS;
      }
      rc = stream_h264b(&streamIn, pStreamerCfg, fps);
      CloseMediaFile(&streamIn);
      break;

    case MEDIA_FILE_TYPE_AAC_ADTS:
      if(fps == 0) {
        LOG(X_INFO("Frame length not specified.  Using default of 1024 samples/frame."));
        fps = 1024;
      }
      if(pStreamerCfg->proto == STREAM_PROTO_AUTO) {
        pStreamerCfg->proto = STREAM_PROTO_MP2TS;
      }
      rc = stream_aacAdts(&streamIn, pStreamerCfg, (unsigned int) fps);
      CloseMediaFile(&streamIn);
      break;

    case MEDIA_FILE_TYPE_MP4:
      CloseMediaFile(&streamIn);
      if(pStreamerCfg->proto == STREAM_PROTO_AUTO) {
        pStreamerCfg->proto = STREAM_PROTO_MP2TS;
      }
      rc = stream_mp4_path(filePath, pStreamerCfg);
      break;

    case MEDIA_FILE_TYPE_FLV:
      CloseMediaFile(&streamIn);
      if(pStreamerCfg->proto == STREAM_PROTO_AUTO) {
        pStreamerCfg->proto = STREAM_PROTO_MP2TS;
      }
      rc = stream_flv(filePath, pStreamerCfg, fps);
      break;

    case MEDIA_FILE_TYPE_MKV:
    case MEDIA_FILE_TYPE_WEBM:
      CloseMediaFile(&streamIn);
      if(pStreamerCfg->proto == STREAM_PROTO_AUTO) {
        pStreamerCfg->proto = STREAM_PROTO_MP2TS;
      }
      rc = stream_mkv(filePath, pStreamerCfg, fps);
      break;

    case MEDIA_FILE_TYPE_MP2TS:
    case MEDIA_FILE_TYPE_MP2TS_BDAV:

      if(pStreamerCfg->proto == STREAM_PROTO_AUTO) {
        pStreamerCfg->proto = STREAM_PROTO_MP2TS;
      }

      if(BOOL_ISDFLT(pStreamerCfg->mp2tfile_extractpes)) {
        pStreamerCfg->mp2tfile_extractpes = 
                  checkEnablePesExtract((pStreamerCfg->proto & STREAM_PROTO_RTP),
                         ((pStreamerCfg->xcode.vid.common.cfgDo_xcode ||
                         pStreamerCfg->xcode.aud.common.cfgDo_xcode) ? 1 : 0),  
                         ((pStreamerCfg->action.do_rtsplive || pStreamerCfg->action.do_rtspannounce) ? 1 : 0),
                         ((pStreamerCfg->action.do_rtmplive || pStreamerCfg->action.do_rtmppublish) ? 1 : 0),
                         ((pStreamerCfg->action.do_flvlive || pStreamerCfg->action.do_flvliverecord) ? 1 : 0),
                         ((pStreamerCfg->action.do_mkvlive || pStreamerCfg->action.do_mkvliverecord) ? 1 : 0),
                         (pStreamerCfg->action.do_httplive? 1 : 0),
                         (pStreamerCfg->action.do_tslive? 1 : 0),
                         (pStreamerCfg->action.do_moofliverecord ? 1 : 0));
      }

      // TODO: hardcoded... take out from command line option & func args
      pStreamerCfg->u.cfgmp2ts.replayUseLocalTiming = 1;

      CloseMediaFile(&streamIn);

      if(BOOL_ISENABLED(pStreamerCfg->mp2tfile_extractpes)) {

        rc = stream_mp2ts_pes_file(filePath, pStreamerCfg, 
             (mediaType == MEDIA_FILE_TYPE_MP2TS_BDAV ? MP2TS_BDAV_LEN : MP2TS_LEN));
             //VIDFRAMEQ_LEN_DEFAULT, AUDFRAMEQ_LEN_DEFAULT);
      } else {
        LOG(X_DEBUG("Restreaming raw input file without PES extraction"));
        rc = stream_mp2ts_raw_file(filePath, pStreamerCfg, 0,
             (mediaType == MEDIA_FILE_TYPE_MP2TS_BDAV ? MP2TS_BDAV_LEN : MP2TS_LEN));
      }

      break;

    case MEDIA_FILE_TYPE_M3U:
      CloseMediaFile(&streamIn);
      if(pStreamerCfg->proto == STREAM_PROTO_AUTO) {
        pStreamerCfg->proto = STREAM_PROTO_MP2TS;
      }
 
      //
      // setting rtp ssrc here will force reuse for all playlist items
      //
      pStreamerCfg->cfgrtp.ssrcs[0] = CREATE_RTP_SSRC();
      pStreamerCfg->cfgrtp.ssrcs[1] = CREATE_RTP_SSRC();

      if(pStreamerCfg->proto == STREAM_PROTO_MP2TS) {
        pStreamerCfg->u.cfgmp2ts.useMp2tsContinuity = 1;
        pStreamerCfg->u.cfgmp2ts.lastPatContIdx = MP2TS_HDR_TS_CONTINUITY;
        pStreamerCfg->u.cfgmp2ts.lastPmtContIdx = MP2TS_HDR_TS_CONTINUITY;
        pStreamerCfg->u.cfgmp2ts.firstProgId = MP2TS_FIRST_PROGRAM_ID;
      }

      rc = stream_playlist(filePath, pStreamerCfg, pStreamerCfg->loop);
      break;

    case MEDIA_FILE_TYPE_PNG:
    case MEDIA_FILE_TYPE_BMP:
      CloseMediaFile(&streamIn);
      if(pStreamerCfg->proto == STREAM_PROTO_AUTO) {
        pStreamerCfg->proto = STREAM_PROTO_MP2TS;
      }
      rc = stream_image(filePath, pStreamerCfg, mediaType);
      break;

    case MEDIA_FILE_TYPE_VID_CONFERENCE:
    case MEDIA_FILE_TYPE_AUD_CONFERENCE:

      if(pStreamerCfg->proto == STREAM_PROTO_AUTO) {
        pStreamerCfg->proto = STREAM_PROTO_MP2TS;
      }
      rc = stream_conference(pStreamerCfg);
      break;
    default:
      LOG(X_ERROR("Unsupported file format '%s'"), filePath);
      break;
  }

  if(filePath) {
    LOG(X_DEBUG("Finished streaming %s"), filePath);
  }

  return rc;
}


/*
static TIME_VAL t0st,t0,t1;
static int testreportcb(void *pCbData, VSXLIB_STREAM_REPORT_T *pReport) {
  t1=timer_GetTime();
  if(t0==0)t0=t1-2000000;
  fprintf(stderr, "REPORT CB: xmit pkts%d(%lld), bytes:%d(%lld) rcv lost %d(%d) / %d(%lld) %.3f(%.3f)Kb/s %.3f(%.3f)pkt/s\n", pReport->pktsSent, pReport->pktsSentTot, pReport->bytesSent, pReport->bytesSentTot, pReport->reportedPktsLost, pReport->reportedPktsLostTot, pReport->reportedPkts, pReport->reportedPktsTot, (float)pReport->bytesSentTot*8000/(t1-t0st), (float)pReport->bytesSent*8000/(t1-t0), (float)pReport->pktsSent*1000000/(t1-t0), (float)pReport->pktsSentTot*1000000/(t1-t0st));
  t0=t1;
  if(t0st==0) t0st=t0;
  return 0;
}
*/

static int parse_output_dest(STREAMER_CFG_T *pStreamerCfg,
                             unsigned int idx,
                             const char *output, 
                             const char *rtcpPorts, 
                             CAPTURE_FILTER_TRANSPORT_T outTransType,
                             const SRTP_CFG_T *pSrtpCfg,
                             const STUN_REQUESTOR_CFG_T *pStunCfg,
                             const TURN_CFG_T *pTurnCfg,
                             int *pnumChannels) {

  STREAMER_DEST_CFG_T *pdestsCfg = &pStreamerCfg->pdestsCfg[idx];
  unsigned int idxPort;
  int is_dtls = 0;
  int portsRtcp[2];
  int rcStreams;
  char buftmp[64];

  if((*pnumChannels = strutil_convertDstStr(output, pdestsCfg->dstHost, sizeof(pdestsCfg->dstHost), 
                                 pdestsCfg->ports, 2, strutil_getDefaultPort(outTransType),
                                 pdestsCfg->dstUri, sizeof(pdestsCfg->dstUri))) < 0) {
    return (int) VSX_RC_ERROR;
  }

  //
  // If we specified '--rtp-mux' then set the rtp ports to match as if using the argument '--output=rtp://a.b.c.d:x,y'
  //
  if((pStreamerCfg->proto == STREAM_PROTO_INVALID || pStreamerCfg->proto == STREAM_PROTO_RTP)) {
    if((pStreamerCfg->streamflags & VSX_STREAMFLAGS_RTPMUX) ||
       (*pnumChannels == 1 && pdestsCfg->ports[0] != 0 && pdestsCfg->ports[1] == 0)) {
      pdestsCfg->ports[1] = pdestsCfg->ports[0];
      if(*pnumChannels == 1) {
        (*pnumChannels)++;
      }
    }
  }

  //
  // In the case where audio & video share the same port,
  // pdestsCfg->numPorts should be set to the number of unique sockets we will create
  //
  if((pdestsCfg->numPorts = *pnumChannels) == 2 && pdestsCfg->ports[0] == pdestsCfg->ports[1]) {
    pdestsCfg->numPorts = 1;
  }

  //
  // If we specified '--rtcp-mux' then set the rtcp ports as if using the argument '--rtcpports=x,y'
  //
  if((pStreamerCfg->streamflags & VSX_STREAMFLAGS_RTCPMUX)) {
    snprintf(buftmp, sizeof(buftmp), "%d,%d", pdestsCfg->ports[0], pdestsCfg->ports[1]);
    rtcpPorts = buftmp;
  }

  if(rtcpPorts && (rcStreams = capture_parsePayloadTypes(rtcpPorts, &portsRtcp[0], &portsRtcp[1])) > 0) {

    pdestsCfg->portsRtcp[0] = portsRtcp[0];

    if(rcStreams & 0x02) {
      //
      // We have a configured  audio stream specific RTCP port
      //
      pdestsCfg->portsRtcp[1] = portsRtcp[1];
    } else if(pdestsCfg->numPorts == 1) {
      //
      // We only have a configured video stream specific RTCP port, so set the audio RTCP port 
      // to the same value
      //
      pdestsCfg->portsRtcp[1] = portsRtcp[0];
    }
  } 

  if(pdestsCfg->numPorts != 2) {
    for(idxPort = 0; idxPort < pdestsCfg[0].numPorts; idxPort++) {
      if(pdestsCfg->portsRtcp[idxPort] == 0) {
        pdestsCfg->portsRtcp[idxPort] = RTCP_PORT_FROM_RTP(pdestsCfg->ports[idxPort]);
      }
    }
  }

  if(pStunCfg && vsxlib_init_stunoutput(pdestsCfg, *pnumChannels, pStunCfg) < 0) {
    return (int) VSX_RC_ERROR;
  }

  if(pTurnCfg && vsxlib_init_turnoutput(pdestsCfg, *pnumChannels, pTurnCfg) < 0) {
    return (int) VSX_RC_ERROR;
  }

  if(pSrtpCfg && (pSrtpCfg->dtls || outTransType == CAPTURE_FILTER_TRANSPORT_UDPDTLS || 
                                    outTransType == CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP)) {
    if(vsxlib_init_dtls(pdestsCfg, *pnumChannels, pSrtpCfg,  
                         (pSrtpCfg->srtp || outTransType == CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP) ? 1 : 0) < 0) {
      return (int) VSX_RC_ERROR;
    }
    is_dtls = 1;
  }

  //
  // If is_dtls is set, (and SRTP is enabled hence dtls-srtp mode) init_srtp has to be called after 
  // the SRTP keys have been obtained post any dtls handshaking.
  //
  if(pSrtpCfg && !is_dtls && (pSrtpCfg->srtp || outTransType == CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES) &&
     vsxlib_init_srtp(pdestsCfg->srtps, *pnumChannels, pSrtpCfg, pdestsCfg->dstHost, pdestsCfg->ports) < 0) {
    return (int) VSX_RC_ERROR;
  }

  return pdestsCfg->numPorts;
}

static void set_qslot_sizes(STREAMER_CFG_T *pStreamerCfg, const VSXLIB_STREAM_PARAMS_T *pParams) {
  unsigned int qslots;
  unsigned int szslot;
  unsigned int szslotmax;

  //
  // Set any outfmt queue size defaults
  //
  if((qslots = pParams->rtmpq_slots) <= 0) {
    qslots = STREAMER_RTMPQ_SLOTS_DEFAULT;
  }
  if((szslot =pParams->rtmpq_szslot) <= 0) {
    szslot = STREAMER_RTMPQ_SZSLOT_DEFAULT;
  }
  if((szslotmax = pParams->rtmpq_szslotmax) == 0) {
    szslotmax = STREAMER_RTMPQ_SZSLOT_MAX;
  } else if(pParams->rtmpq_szslotmax > STREAMER_RTMPQ_SZSLOT_MAX_LIMIT) {
    szslotmax = STREAMER_RTMPQ_SZSLOT_MAX_LIMIT;
  }

  pStreamerCfg->action.outfmtQCfg.cfgRtmp.maxPkts = qslots;
  pStreamerCfg->action.outfmtQCfg.cfgRtmp.maxPktLen = szslot;
  pStreamerCfg->action.outfmtQCfg.cfgRtmp.growMaxPktLen = szslotmax;
  memcpy(&pStreamerCfg->action.outfmtQCfg.cfgFlv, &pStreamerCfg->action.outfmtQCfg.cfgRtmp,
         sizeof(pStreamerCfg->action.outfmtQCfg.cfgFlv));
  memcpy(&pStreamerCfg->action.outfmtQCfg.cfgMkv, &pStreamerCfg->action.outfmtQCfg.cfgRtmp,
         sizeof(pStreamerCfg->action.outfmtQCfg.cfgMkv));
  memcpy(&pStreamerCfg->action.outfmtQCfg.cfgMoof, &pStreamerCfg->action.outfmtQCfg.cfgRtmp,
         sizeof(pStreamerCfg->action.outfmtQCfg.cfgMoof));
  pStreamerCfg->action.outfmtQCfg.cfgRtsp.maxPkts = pParams->rtspinterq_slots;
  pStreamerCfg->action.outfmtQCfg.cfgRtsp.maxPktLen = pParams->rtspinterq_szslot;
  pStreamerCfg->action.outfmtQCfg.cfgRtsp.growMaxPktLen = pParams->rtspinterq_szslotmax;
  pStreamerCfg->action.outfmtQCfg.cfgTslive.maxPktLen = pParams->tsliveq_szslot;
  pStreamerCfg->action.outfmtQCfg.cfgTslive.maxPkts = pParams->tsliveq_slots;
  pStreamerCfg->action.outfmtQCfg.cfgTslive.growMaxPkts = pParams->tsliveq_slotsmax;
}

static int vsxlib_set_output_int(STREAMER_CFG_T *pStreamerCfg,
                      const char *outputs[IXCODE_VIDEO_OUT_MAX],
                      const char *rtcpPorts[IXCODE_VIDEO_OUT_MAX],
                      const char *transproto,
                      const char *sdpoutpath,
                      const SRTP_CFG_T srtpCfgs[IXCODE_VIDEO_OUT_MAX],
                      const STUN_REQUESTOR_CFG_T *pstunCfg,
                      const TURN_CFG_T *pTurnCfg) {
  unsigned int idx;
  size_t sz;
  CAPTURE_FILTER_TRANSPORT_T outTransType = CAPTURE_FILTER_TRANSPORT_UNKNOWN;
  CAPTURE_FILTER_TRANSPORT_T outTransTypeTmp;
  int numChannels = 0;

  //
  // Get the output streaming method type
  //
  if(outputs[0]) {

    pStreamerCfg->pdestsCfg[0].outTransType = outTransType = capture_parseTransportStr(&outputs[0]);

    //
    // Get any username or password from the output string
    //
    if(capture_parseAuthUrl(&outputs[0], &pStreamerCfg->creds[STREAMER_AUTH_IDX_RTSPANNOUNCE].stores[0]) < 0) {
      return (int) VSX_RC_ERROR;
    }
    memcpy(&pStreamerCfg->creds[STREAMER_AUTH_IDX_RTMPPUBLISH].stores[0],
           &pStreamerCfg->creds[STREAMER_AUTH_IDX_RTSPANNOUNCE].stores[0],
           sizeof(pStreamerCfg->creds[STREAMER_AUTH_IDX_RTMPPUBLISH].stores[0]));

    if(IS_CAPTURE_FILTER_TRANSPORT_RTSP(outTransType)) {
      pStreamerCfg->action.do_rtspannounce = 1;
    } else if(IS_CAPTURE_FILTER_TRANSPORT_RTMP(outTransType)) {
      pStreamerCfg->action.do_rtmppublish = 1;
    }

    if(pStreamerCfg->action.do_rtspannounce || pStreamerCfg->action.do_rtmppublish) {
      if(strutil_convertDstStr(outputs[0], pStreamerCfg->pdestsCfg[0].dstHost, 
               sizeof(pStreamerCfg->pdestsCfg[0].dstHost), 
               pStreamerCfg->pdestsCfg[0].ports, 1, strutil_getDefaultPort(outTransType), 
               pStreamerCfg->pdestsCfg[0].dstUri, sizeof(pStreamerCfg->pdestsCfg[0].dstUri)) < 0) {
        return (int) VSX_RC_ERROR;
      }
    }
  }

  //
  // Set RTP header output and RTCP Sender Reports
  //
  if(IS_CAPTURE_FILTER_TRANSPORT_RTP(outTransType) || pStreamerCfg->action.do_rtspannounce) {

    pStreamerCfg->action.do_output_rtphdr = 1;

    if(pStreamerCfg->frtcp_sr_intervalsec == 0) {
      LOG(X_DEBUG("RTCP SR disabled"));
    } else if(pStreamerCfg->frtcp_sr_intervalsec != STREAM_RTCP_SR_INTERVAL_SEC) {
      LOG(X_DEBUG("RTCP SR interval set to %.2f sec"), pStreamerCfg->frtcp_sr_intervalsec); 
    }

  } else {
    pStreamerCfg->action.do_output_rtphdr = 0;
  }

  //
  // Get output transport type (rtp|m2t)
  //
  if(transproto && ((pStreamerCfg->proto = strutil_convertProtoStr(transproto)) == STREAM_PROTO_INVALID)) {
    return VSX_RC_ERROR;
  }

  if(outputs[0]) {

    if(outTransType == CAPTURE_FILTER_PROTOCOL_UNKNOWN) {
      //
      // If outTransType == CAPTURE_FILTER_PROTOCOL_UNKNOWN, output could be a local file,
      // and pdestsCfg dstHost, ports, numPorts will all be 0.
      //
      strncpy(pStreamerCfg->pdestsCfg[0].dstHost, outputs[0], sizeof(pStreamerCfg->pdestsCfg[0].dstHost) -1);
      pStreamerCfg->numDests = 1;

    } else if(IS_CAPTURE_FILTER_TRANSPORT_RTP(outTransType) ||
              outTransType == CAPTURE_FILTER_TRANSPORT_UDPRAW) {

      pStreamerCfg->numDests = 1;
      if(parse_output_dest(pStreamerCfg, 
                                0,
                                outputs[0], 
                                rtcpPorts ? rtcpPorts[0] : NULL, 
                                outTransType,
                                (srtpCfgs[0].srtp || srtpCfgs[0].dtls ||
                                 outTransType == CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES ||
                                 outTransType == CAPTURE_FILTER_TRANSPORT_UDPDTLS||
                                 outTransType == CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP) ? &srtpCfgs[0] : NULL,
                                pstunCfg && pstunCfg->bindingRequest ? pstunCfg : NULL,
                                pTurnCfg,
                                &numChannels) < 0) {
        return VSX_RC_ERROR;
      }
    }

    pStreamerCfg->action.do_output = 1;

    //
    // If more than one output port is specified, assume native rtp codec
    // specific packetization (non-m2t) mode is used
    //
    //if(pStreamerCfg->pdestsCfg[0].numPorts > 1 && !transproto && pStreamerCfg->proto == STREAM_PROTO_INVALID) {
    if(numChannels > 1 && !transproto && pStreamerCfg->proto == STREAM_PROTO_INVALID) {
      pStreamerCfg->proto = STREAM_PROTO_RTP;
    }

    //
    // Fix output ports if audio or video is disabled
    //
    if(pStreamerCfg->novid || pStreamerCfg->noaud) {
      for(idx = 0; idx < *pStreamerCfg->pmaxRtp; idx++) {
        if(pStreamerCfg->pdestsCfg[idx].numPorts > 1) {
          if(pStreamerCfg->novid) {
            pStreamerCfg->pdestsCfg[idx].ports[0] = pStreamerCfg->pdestsCfg[idx].ports[1];
          } 
          pStreamerCfg->pdestsCfg[idx].numPorts = 1;
        }
      }
    }

  }

  //
  // Set multiple output destinations
  //
  for(idx = 1; idx < MIN(IXCODE_VIDEO_OUT_MAX, *pStreamerCfg->pmaxRtp); idx++) {

    if(outputs[idx]) {

      if((outTransTypeTmp = capture_parseTransportStr(&outputs[idx])) != outTransType &&
       ((outTransType != CAPTURE_FILTER_TRANSPORT_UDPRTP && outTransType != CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES) ||
       (outTransTypeTmp != CAPTURE_FILTER_TRANSPORT_UDPRTP && outTransTypeTmp != CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES))) {
        LOG(X_ERROR("Unable to assign output '%s' based on output protocol type"), outputs[idx]);
        return VSX_RC_ERROR;
      }
      pStreamerCfg->pdestsCfg[idx].outTransType = outTransTypeTmp;

      if(outTransTypeTmp == CAPTURE_FILTER_PROTOCOL_UNKNOWN) {
        //
        // If outTransType == CAPTURE_FILTER_PROTOCOL_UNKNOWN, output could be a local file,
        // and pdestsCfg dstHost, ports, numPorts will all be 0.
        //
        strncpy(pStreamerCfg->pdestsCfg[idx].dstHost, outputs[idx], sizeof(pStreamerCfg->pdestsCfg[idx].dstHost) -1);

      } else if(IS_CAPTURE_FILTER_TRANSPORT_RTP(outTransType)) {

        if(parse_output_dest(pStreamerCfg,
                                  idx,
                                  outputs[idx], 
                                  rtcpPorts ? rtcpPorts[idx] : NULL,
                                  outTransTypeTmp,
                                  (srtpCfgs[0].srtp || srtpCfgs[0].dtls ||
                                   outTransType == CAPTURE_FILTER_TRANSPORT_UDPSRTPSDES ||
                                   outTransType == CAPTURE_FILTER_TRANSPORT_UDPDTLS||
                                   outTransType == CAPTURE_FILTER_TRANSPORT_UDPDTLSSRTP) ? &srtpCfgs[idx] : NULL,
                                  pstunCfg && pstunCfg->bindingRequest ? pstunCfg : NULL,
                                  pTurnCfg,
                                  &numChannels) < 0) {
          return VSX_RC_ERROR;
        } else {

          //
          // Create output index specific named .sdp files
          //
          if(sdpoutpath) {
            if(!pStreamerCfg->pStorageBuf) {
              return VSX_RC_ERROR;
            }
            strncpy(pStreamerCfg->pStorageBuf->sdppath[idx], sdpoutpath, 
                    sizeof(pStreamerCfg->pStorageBuf->sdppath[idx]));
            if((sz = strlen(pStreamerCfg->pStorageBuf->sdppath[idx])) >= 4 &&
               !strncasecmp(&pStreamerCfg->pStorageBuf->sdppath[idx][sz - 4], ".sdp", 4)) {
             sz -= 4;
            }
            snprintf(&pStreamerCfg->pStorageBuf->sdppath[idx][sz], 
                     sizeof(pStreamerCfg->pStorageBuf->sdppath[idx]) - sz, "%d", idx + 1);
            pStreamerCfg->sdpActions[idx].pPathOutFile = pStreamerCfg->pStorageBuf->sdppath[idx];
          }
          pStreamerCfg->numDests++;

        } // end of if(parse_output_dest...
      } // end of if IS_CAPTURE_FILTER_TRANSPORT_RTP(...

    } // end of if(outputs[idx]...
  } // end of for(idx...

  return VSX_RC_OK;
}

static int output_load_turn_candidate(const SDP_STREAM_DESCR_T *pSdpDescr, uint16_t *psdpRtpPort, 
                                      uint16_t *psdpRtcpPort, const struct sockaddr **ppaddrTurnHost) {
  int rc = 0;
  unsigned int candidateIdx;
  const struct sockaddr *psa = NULL;

  if(!pSdpDescr->available) {
    return 0;
  }

  for(candidateIdx = 0; candidateIdx < SDP_ICE_CANDIDATES_MAX; candidateIdx++) {
    if(pSdpDescr->iceCandidates[candidateIdx].type == SDP_ICE_TYPE_RELAY) {

      //psa = &pSdpDescr->iceCandidates[candidateIdx].address;
      psa = (const struct sockaddr *) &pSdpDescr->iceCandidates[candidateIdx].raddress;

      if(pSdpDescr->iceCandidates[candidateIdx].component == 1) { 
        *psdpRtpPort = htons(PINET_PORT(psa));
      } else if(pSdpDescr->iceCandidates[candidateIdx].component == 2) { 
        *psdpRtcpPort = htons(PINET_PORT(psa));
      }
      *ppaddrTurnHost = psa;
      rc = 1;
    }
  }

  return rc;
}

static int output_create_from_sdp(const char *path, const TURN_CFG_T *pTurnCfg, const SDP_DESCR_T *pSdp, 
                                  char *outputStr, unsigned int szOutputStr, 
                                  char *rtcpPortStr, unsigned int szRtcpPortStr) {
  int rc = 0;
  unsigned int idx = 0;
  const SDP_STREAM_DESCR_T *pSdpDescr = NULL;
  const char *dstHost = NULL;
  int use_turn = 0;
  const struct sockaddr *paddrTurnHost = NULL;
  uint16_t sdpRtpPorts[2];
  uint16_t sdpRtcpPorts[2];

  if(pSdp->aud.common.available) {
    pSdpDescr = &pSdp->aud.common;
  } else if(pSdp->vid.common.available) {
    pSdpDescr = &pSdp->vid.common;
  } else {
    LOG(X_ERROR("Output SDP '%s' does not contain recognized media description"), path);
    return -1;
  }

  sdpRtpPorts[0] = sdpRtpPorts[1] = 0;
  sdpRtcpPorts[0] = sdpRtcpPorts[1] = 0;

  if(pSdpDescr->transType == SDP_TRANS_TYPE_RTP_UDP) {
    rc = snprintf(outputStr, szOutputStr, "rtp://");
  } else if(pSdpDescr->transType == SDP_TRANS_TYPE_RAW_UDP) {
    rc = snprintf(outputStr, szOutputStr, "udp://");
  } else if(pSdpDescr->transType == SDP_TRANS_TYPE_SRTP_SDES_UDP) {
    rc = snprintf(outputStr, szOutputStr, "srtp://");
  } else if(pSdpDescr->transType == SDP_TRANS_TYPE_SRTP_DTLS_UDP) {
    rc = snprintf(outputStr, szOutputStr, "dtlssrtp://");
  } else if(pSdpDescr->transType == SDP_TRANS_TYPE_DTLS_UDP) {
    rc = snprintf(outputStr, szOutputStr, "dtls://");
  } else {
    LOG(X_ERROR("Output SDP '%s' contains invalid or unrecognized media transport type"), path);
    return -1;
  }
  if(rc > 0) {
    idx = rc;
  }

  //
  // If the SDP contains a TURN relay ice-candidate then automatically default to a TURN server
  //
  if(pTurnCfg && pTurnCfg->turnServer) {
    if(output_load_turn_candidate(&pSdp->vid.common, &sdpRtpPorts[0], &sdpRtcpPorts[0], &paddrTurnHost)) {
      use_turn = 1;
    }
    if(output_load_turn_candidate(&pSdp->aud.common, &sdpRtpPorts[1], &sdpRtcpPorts[1], &paddrTurnHost)) {
      use_turn = 1;
    }
  }

  if(pSdp->vid.common.available) {
    if(sdpRtpPorts[0] == 0 && pSdp->vid.common.port > 0) {
      sdpRtpPorts[0] = pSdp->vid.common.port;
    }
    if(sdpRtpPorts[0] != 0 && sdpRtcpPorts[0] == 0 && pSdp->vid.common.portRtcp > 0) {
      sdpRtcpPorts[0] = pSdp->vid.common.portRtcp;
    }
  }
  if(pSdp->aud.common.available) {
    if(sdpRtpPorts[1] == 0 && pSdp->aud.common.port > 0) {
      sdpRtpPorts[1] = pSdp->aud.common.port;
    }
    if(sdpRtpPorts[0] != 0 && sdpRtcpPorts[1] == 0 && pSdp->aud.common.portRtcp > 0) {
      sdpRtcpPorts[1] = pSdp->aud.common.portRtcp;
    }
  }

  //
  // If just one rtp/rtcp port is present, shift it to the first slot
  //
  if(sdpRtpPorts[0] == 0) {
    sdpRtpPorts[0] = sdpRtpPorts[1];
    sdpRtpPorts[1] = 0;
  }
  if(sdpRtcpPorts[0] == 0) {
    sdpRtcpPorts[0] = sdpRtcpPorts[1];
    sdpRtcpPorts[1] = 0;
  }

  if(!dstHost) {
    dstHost = pSdp->c.iphost;
  }

  //
  // Construct an --output= argument from the SDP output transport type, host, and ports
  //
  if((rc = snprintf(&outputStr[idx], szOutputStr - idx, "%s:%d", dstHost, sdpRtpPorts[0])) < 0) {
    return rc;
  }
  idx += rc;
  if(sdpRtpPorts[1] > 0) {
    if((rc = snprintf(&outputStr[idx], szOutputStr - idx, ",%d", sdpRtpPorts[1])) < 0) {
      return rc;
    }
    idx += rc;
  }

  //
  // Construct an --rtcpports= argument from any non-default SDP RTCP ports
  //
  idx = 0;
  if(sdpRtcpPorts[0] != 0) {
    if((rc = snprintf(&rtcpPortStr[idx], szRtcpPortStr - idx, "%d", sdpRtcpPorts[0])) < 0) {
      return rc;
    }
    idx += rc;
  }
  if(sdpRtcpPorts[1] != 0) {
    if((rc = snprintf(&rtcpPortStr[idx], szRtcpPortStr - idx, "%s%d", sdpRtcpPorts[0] != 0 ? "," : "",
                      sdpRtcpPorts[1])) < 0) {
      return rc;
    }
    idx += rc;
  }
/*
  if(pTurnCfg && pTurnCfg->turnServer && use_turn) {

    tmpbuf[0] = '\0';

    if(paddrTurnHost) {
       safe_strncpy(tmpbuf, INET_NTOP(*paddrTurnHost, tmp, sizeof(tmp)), sizeof(tmpbuf));
    }

    if(strncasecmp(tmpbuf, pTurnCfg->turnServer, sizeof(tmpbuf))) {
      LOG(X_ERROR("Output SDP TURN server '%s' does not match configured TURN server '%s'"), 
                  tmpbuf, pTurnCfg->turnServer);
      return -1;
    }
  }
*/
  LOG(X_DEBUG("Using output configuration obtained from SDP:  output=%s rtcpports=%s"), outputStr, rtcpPortStr);

  return rc;
}

int vsxlib_set_output(STREAMER_CFG_T *pStreamerCfg,
                      const char *outputs[IXCODE_VIDEO_OUT_MAX],
                      const char *rtcpPorts[IXCODE_VIDEO_OUT_MAX],
                      const char *transproto,
                      const char *sdpoutpath,
                      const SRTP_CFG_T srtpCfgs[IXCODE_VIDEO_OUT_MAX],
                      const STUN_REQUESTOR_CFG_T *pstunCfg,
                      const TURN_CFG_T *pTurnCfg) {

  int rc = 0;
  SDP_DESCR_T sdp;
  char outputStr[256];
  char rtcpPortStr[256];
  FILE_STREAM_T fs;
  const char *arrOutputs[IXCODE_VIDEO_OUT_MAX];
  const char *arrRtcpPorts[IXCODE_VIDEO_OUT_MAX];

  memcpy(arrOutputs, outputs, sizeof(arrOutputs));
  memcpy(arrRtcpPorts, rtcpPorts, sizeof(arrRtcpPorts));

  if(outputs[0]) {

    //
    // Check if the output string is an SDP file
    //
    memset(&fs, 0, sizeof(fs));
    strncpy(fs.filename, outputs[0], sizeof(fs.filename) - 1);
    if(sdp_isvalidFile(&fs)) {

      if((rc = sdp_isvalidFilePath(outputs[0])) < 0 || (rc = sdp_load(&sdp, outputs[0])) < 0) {
        LOG(X_ERROR("Unable to load output SDP configuration from file '%s'"), outputs[0]);
        return rc;
      }

      LOG(X_DEBUG("Loading output configuration from SDP file '%s'"), outputs[0]);
      outputStr[0] = '\0';
      rtcpPortStr[0] = '\0';

      if((rc = output_create_from_sdp(outputs[0], pTurnCfg, &sdp, outputStr, sizeof(outputStr), 
                                      rtcpPortStr, sizeof(rtcpPortStr))) < 0) {
        return rc;
      }
      memset(arrOutputs, 0, sizeof(arrOutputs));
      memset(arrRtcpPorts, 0, sizeof(arrRtcpPorts));
      arrOutputs[0] = outputStr;   
      arrRtcpPorts[0] = rtcpPortStr;   
    }
  }

  rc = vsxlib_set_output_int(pStreamerCfg, arrOutputs, arrRtcpPorts, transproto, sdpoutpath, 
                              srtpCfgs, pstunCfg, pTurnCfg);

  return rc;
}

VSX_RC_T vsxlib_stream(VSXLIB_STREAM_PARAMS_T *pParams) {

  STREAM_STORAGE_T *pS = NULL;
  IXCODE_PIP_MOTION_T *pipMotion = NULL;
  unsigned int pipMaxRtp = 1;
  char buf[VSX_MAX_PATH_LEN];
  unsigned int idx, idx2;
  int sz;
  SRV_CONF_T *pConf = NULL;
  SDP_DESCR_T sdp;
  VSX_RC_T rc = VSX_RC_OK;
  int irc;
  int do_server = 0;
  int do_httplive_segmentor = 0;
  int do_flvrecord = 0;
  int do_mkvrecord = 0;
  //int do_moofrecord = 0;
  VSXLIB_PRIVDATA_T *pPrivData;
  META_FILE_T metafile;
  int init_pip = 0;
  int pip_running = 0;
  const char *inputs[2];
  const char *log_tag = NULL;
  char input1[VSX_MAX_PATH_LEN];
  char tokenid[META_FILE_TOKEN_LEN];

  if(!pParams || !pParams->pPrivate) {
    return VSX_RC_ERROR;
  }

  pPrivData = (VSXLIB_PRIVDATA_T *) pParams->pPrivate;

  if(pParams->httpliveaddr[0] || pParams->httplive_chunkduration != 0 || pParams->httplivedir ||
     pParams->dash_ts_segments == 1) {
    do_httplive_segmentor = 1; 
  }

  if(!pParams->dash_ts_segments == -1 && pParams->dashliveaddr[0] && do_httplive_segmentor) {
    pParams->dash_ts_segments = 1;
  }

  if(pParams->rtmpliveaddr[0] || pParams->rtspliveaddr[0] || 
     pParams->tsliveaddr[0] || pParams->flvliveaddr[0] ||  pParams->mkvliveaddr[0] || 
      (pParams->httpliveaddr[0] || do_httplive_segmentor) || 
      //(pParams->dashliveaddr[0] || pParams->dashlivemax > 0) ||
      (pParams->dashliveaddr[0]) ||
      (pParams->statusaddr[0] || pParams->statusmax > 0) ||
      (pParams->configaddr[0] || pParams->configmax > 0)) {
    do_server = 1;
  }

  inputs[0] = avc_dequote(pParams->inputs[0], input1, sizeof(input1));
  inputs[1] = pParams->inputs[1];

  if(pParams->mixerCfg.conferenceInputDriver || pParams->pipCfg[0].input || 
     pParams->pipCfg[0].cfgfile || pParams->pipaddr[0] || g_debug_flags) {
    //
    // This will enable LOG_OUTPUT_PRINT_TAG when calling vsxlib_initlog
    //
    log_tag = "main";
  }


  if(pParams->confpath) {
    if(!(pConf = vsxlib_loadconf(pParams))) {
      return VSX_RC_ERROR;
    }

    vsxlib_initlog(pParams->verbosity, pParams->logfile, NULL, pParams->logmaxsz, pParams->logrollmax, log_tag);
    if(pParams->logfile) {
      LOG(X_INFO("Read configuration from %s"), pParams->confpath);
    }
  } else {
    vsxlib_initlog(pParams->verbosity, pParams->logfile, NULL, pParams->logmaxsz, pParams->logrollmax, log_tag);
  }  
  http_log_setfile(pParams->httpaccesslogfile);

  if(inputs[0] && !strcasecmp(FILETYPE_CONFERENCE, inputs[0])) {
    pParams->mixerCfg.conferenceInputDriver = 1;
  } 
  if(pParams->mixerCfg.conferenceInputDriver && !pParams->mixerCfg.active) {
    pParams->mixerCfg.active = 1;
  }
  if(pParams->novid && pParams->noaud) {
    LOG(X_ERROR("Nothing to stream!"));
    return VSX_RC_ERROR;
  }

  if(!(pS = (STREAM_STORAGE_T *) avc_calloc(1, sizeof(STREAM_STORAGE_T)))) {
    return VSX_RC_ERROR;
  }

  pS->pParams = pParams;
  pS->streamerCfg.pXcodeCtxt = &pS->xcodeCtxt;
  pS->xcodeCtxt.vidUData.pVidData = &pS->xcodeCtxt.vidData;
  pS->xcodeCtxt.vidUData.type = XC_CODEC_TYPE_VID_GENERIC;
  pS->xcodeCtxt.vidUData.pStreamerPip = &pS->streamerCfg.pip;
  pS->xcodeCtxt.vidUData.enableOutputSharing = 1;
  pS->xcodeCtxt.audUData.pAudData = &pS->xcodeCtxt.audData;
  pS->xcodeCtxt.audUData.type = XC_CODEC_TYPE_AUD_GENERIC;
  pS->streamerCfg.xcode.vid.pUserData = &pS->xcodeCtxt.vidUData;
  pS->streamerCfg.xcode.aud.pUserData = &pS->xcodeCtxt.audUData;
  pS->xcodeCtxt.vidUData.pStreamerFbReq = &pS->streamerCfg.fbReq;
  //fprintf(stderr, "SET VID DATA 0x%x\n", pS->streamerCfg.xcode.vid.pUserData);
  pS->streamerCfg.pStorageBuf = pS;
  pS->streamerCfg.action.do_stream = 1;
  pS->streamerCfg.sdpActions[0].pPathOutFile = pParams->sdpoutpath; 


  // TODO: if this is off, then reading a file (via stream_net_xxx) 
  // through the xcoder may be broken
  pS->streamerCfg.running = STREAMER_STATE_STARTING;
  pS->streamerCfg.cfgstream.includeSeqHdrs = !pParams->noIncludeSeqHdrs;
  pS->streamerCfg.loop = pParams->loop;
  pS->streamerCfg.verbosity = pParams->verbosity;
  pS->streamerCfg.rawxmit = pParams->rawxmit;
  pS->streamerCfg.maxPlRecurseLevel = 3;
  pS->streamerCfg.mp2tfile_extractpes = pParams->extractpes;
  pS->streamerCfg.novid = pParams->novid;
  pS->streamerCfg.noaud = pParams->noaud;
  pS->streamerCfg.streamflags = pParams->streamflags;
  pS->streamerCfg.avReaderType = pParams->avReaderType;
  pS->streamerCfg.overwritefile = pParams->overwritefile;
  pS->streamerCfg.audsamplesbufdurationms = pParams->outaudbufdurationms;
  pS->streamerCfg.rtspannounce.connectretrycntminone = pParams->connectretrycntminone;
  pS->streamerCfg.rtmppublish.connectretrycntminone = pParams->connectretrycntminone;
  vsxlib_stream_setup_rtmpclient(&pS->streamerCfg.rtmppublish.cfg, pParams);
  //pS->streamerCfg.frtcp_sr_intervalsec = pParams->frtcp_sr_intervalsec;

  pParams->tokenid = avc_dequote(pParams->tokenid, tokenid, sizeof(tokenid));
  pS->streamerCfg.pAuthTokenId = pParams->tokenid;

  if(pParams->haveavoffsetrtcp) {
    pS->streamerCfg.status.favoffsetrtcp = pParams->favoffsetrtcp;
  }
  for(idx = 0; idx < STREAMER_AUTH_IDX_MAX; idx++) {
    for(idx2 = 0; idx2 < SRV_LISTENER_MAX; idx2++) {
      pS->streamerCfg.creds[idx].stores[idx2].authtype = pParams->httpauthtype;
    }
  }

  //
  // Setup RTP output parameters such as SSRC, PT, etc
  //
  vsxlib_setup_rtpout(&pS->streamerCfg, pParams);

  //fprintf(stderr, "SSRCS: '%s' 0x%x, 0x%x, irc:%d\n", pParams->output_rtpssrcsstr, pS->streamerCfg.cfgrtp.ssrcs[0],pS->streamerCfg.cfgrtp.ssrcs[1], irc);

  pS->streamerCfg.fbReq.firCfg.fir_encoder = MAKE_BOOL(pParams->firCfg.fir_encoder);
  pS->streamerCfg.fbReq.firCfg.fir_recv_via_rtcp = MAKE_BOOL(pParams->firCfg.fir_recv_via_rtcp);
  pS->streamerCfg.fbReq.firCfg.fir_recv_from_connect = MAKE_BOOL(pParams->firCfg.fir_recv_from_connect);
  pS->streamerCfg.fbReq.firCfg.fir_recv_from_remote = MAKE_BOOL(pParams->firCfg.fir_recv_from_remote);
  pS->streamerCfg.fbReq.firCfg.fir_send_from_decoder = MAKE_BOOL(pParams->firCfg.fir_send_from_decoder);
  pS->streamerCfg.fbReq.firCfg.fir_send_from_local = MAKE_BOOL(pParams->firCfg.fir_send_from_local);
  pS->streamerCfg.fbReq.firCfg.fir_send_from_remote = MAKE_BOOL(pParams->firCfg.fir_send_from_remote);
  pS->streamerCfg.fbReq.firCfg.fir_send_from_capture = MAKE_BOOL(pParams->firCfg.fir_send_from_capture);
  //LOG(X_DEBUG("NACK... rtpretransmit:%d, send-nack%d"), pParams->nackRtpRetransmitVideo, pParams->nackRtcpSendVideo); 
  pS->streamerCfg.fbReq.nackRtpRetransmit = MAKE_BOOL(pParams->nackRtpRetransmitVideo);
  pS->streamerCfg.fbReq.nackRtcpSend = MAKE_BOOL(pParams->nackRtcpSendVideo);
  pS->streamerCfg.fbReq.apprembRtcpSend = MAKE_BOOL(pParams->apprembRtcpSendVideo);
  pS->streamerCfg.fbReq.apprembRtcpSendMaxRateBps = pParams->apprembRtcpSendVideoMaxRateBps;
  pS->streamerCfg.fbReq.apprembRtcpSendMinRateBps = pParams->apprembRtcpSendVideoMinRateBps;
  pS->streamerCfg.fbReq.apprembRtcpSendForceAdjustment = MAKE_BOOL(pParams->apprembRtcpSendVideoForceAdjustment);
  pS->streamerCfg.frameThin = MAKE_BOOL(pParams->frameThin);

  //
  // Audio / video sync adjustments
  // 

  pS->streamerCfg.status.faoffset_m2tpktz = pParams->faoffset_m2tpktz;
  pS->streamerCfg.status.faoffset_mkvpktz = pParams->faoffset_mkvpktz;

  for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
    if((pS->streamerCfg.status.haveavoffsets0[idx] = pParams->haveavoffsets[idx])) {
      pS->streamerCfg.status.avs[idx].offset0 = (int64_t) (pParams->favoffsets[idx] * 90000);
      pS->streamerCfg.status.avs[idx].offsetCur = pS->streamerCfg.status.avs[idx].offset0;
    }
  }

  pS->streamerCfg.status.msDelayTx = (unsigned int) abs((pParams->favdelay * 1000));
  pS->streamerCfg.status.useStatus = 1;
  pS->streamerCfg.status.ignoreencdelay_av = pParams->ignoreencdelay;
  pS->streamerCfg.pRawOutCfg = &pPrivData->rawOutCfg;
  pS->streamerCfg.pReportCfg = &pPrivData->reportDataCfg;
  pS->streamerCfg.pmaxRtp = &pParams->rtplivemax;
  pS->streamerCfg.u.cfgmp2ts.maxPcrIntervalMs = pParams->m2t_maxpcrintervalms;
  pS->streamerCfg.u.cfgmp2ts.maxPatIntervalMs = pParams->m2t_maxpatintervalms;
  pS->streamerCfg.u.cfgmp2ts.xmitflushpkts = pParams->m2t_xmitflushpkts; 

  pS->rtspSessions.staticLocalPort = pParams->rtsplocalport;
  pS->rtspSessions.rtspsecuritytype = pParams->rtspsecuritytype;
  pS->rtspSessions.rtsptranstype = pParams->rtsptranstype;
  pS->rtspSessions.rtspForceTcpUAList.str = pParams->rtspualist;
  pS->streamerCfg.pRtspSessions = &pS->rtspSessions;

  if(vsxlib_checklicense(pParams, &pS->streamerCfg.lic, pConf) < 0) {
    avc_free((void **) &pS);
    return VSX_RC_ERROR;
  }

  //
  // Should not call return after this point, to allow for cleanup at end of function
  //

  if(rc == VSX_RC_OK && vsxlib_alloc_destCfg(&pS->streamerCfg, 0) < 0) {
    rc = VSX_RC_ERROR;
  }

  //
  // Check if the input source is a metafile referring to a media resource
  //
  if(rc == VSX_RC_OK && inputs[0] != NULL && metafile_isvalidFile(inputs[0])) {
    memset(&metafile, 0, sizeof(metafile)); 
    if(metafile_open(inputs[0], &metafile, 0, 0) == 0) {
      LOG(X_INFO("Input meta file %s refers to %s"), inputs[0], metafile.filestr);
      inputs[0] = metafile.filestr;  
      if(!pParams->strxcode && metafile.xcodestr[0] != '\0') {
        pParams->strxcode = metafile.xcodestr;
      } 
    }
    metafile_close(&metafile);
  }

  if(rc == VSX_RC_OK) {

    //
    // Parse any transcoding configuration string
    //
    if(pParams->strxcode) {

      if(xcode_enabled(1) < XCODE_CFG_OK) {
        rc = VSX_RC_ERROR;
      } 

      if(rc == VSX_RC_OK) {

        if(pParams->mixerCfg.conferenceInputDriver) {
          // 
          // If using the conference input driver, set a default RAW output type 
          // to allow the xcode options to be absent
          //
          pS->streamerCfg.xcode.vid.common.cfgFileTypeOut = XC_CODEC_TYPE_RAWV_YUV420P;
          pS->streamerCfg.xcode.aud.common.cfgFileTypeOut = XC_CODEC_TYPE_RAWA_PCM16LE;
        }

        if((irc = xcode_parse_configstr(pParams->strxcode, &pS->streamerCfg.xcode, 1, 1)) < 0) {
          if(irc == -2) {
            pS->streamerCfg.xcodecfgfile = pParams->strxcode;
          } else {
            rc = VSX_RC_ERROR;
          }
        }
      }

      if(rc >= 0 && pParams->mixerCfg.conferenceInputDriver && 
         pS->streamerCfg.xcode.aud.cfgSampleRateOut == 0) {
         pS->streamerCfg.xcode.aud.cfgSampleRateOut = 16000;
         LOG(X_WARNING("Audio mixer sample rate not set.  Using default %dHz"),
             pS->streamerCfg.xcode.aud.cfgSampleRateOut);
         
      }

      if(pS->streamerCfg.novid) {
        pS->streamerCfg.xcode.vid.common.cfgDo_xcode = 0;
      }
      if(pS->streamerCfg.noaud) {
        pS->streamerCfg.xcode.aud.common.cfgDo_xcode = 0;
      }
      pS->streamerCfg.xcode.vid.common.pLogCtxt = logger_getContext();
      pS->streamerCfg.xcode.aud.common.pLogCtxt = logger_getContext();

//      if(pParams->mixerCfg.conferenceInputDriver) {
        //fprintf(stderr, "do_xcode:%d, out:%d %dx\n", pS->streamerCfg.xcode.vid.common.cfgDo_xcode, pS->streamerCfg.xcode.vid.common.cfgFileTypeOut, pS->streamerCfg.xcode.vid.out[0].cfgOutH);
//      }

    } else if(pParams->pipCfg[0].input || pParams->pipCfg[0].cfgfile) {
      LOG(X_ERROR("xcode configuration must be present when using PIP"));
      rc = VSX_RC_ERROR;
    }

  }

  //
  // Set flv/mkv output recording flag
  //
  if(rc == VSX_RC_OK) {
    for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
      if(pParams->flvrecord[idx] && (idx == 0 || pS->streamerCfg.xcode.vid.out[idx].active)) {
        do_flvrecord = 1;
      } 
      if(pParams->mkvrecord[idx] && (idx == 0 || pS->streamerCfg.xcode.vid.out[idx].active)) {
        do_mkvrecord = 1;
      }
    }
  }

  //if(rc == VSX_RC_OK) {
  //  for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
  //    if(pParams->moofrecord[idx] && (idx == 0 || pS->streamerCfg.xcode.vid.out[idx].active)) {
  //      do_moofrecord = 1;
  //      break;
  //    }
  //  }
  //}

  //
  // Check if input is actually an SDP file meant for live capture
  //
  if(rc == VSX_RC_OK && inputs[0] != NULL &&
     (sdp_isvalidFilePath(inputs[0]) || !strncasecmp(inputs[0], "sdp://", 6))) {

    if((sz = capture_filterFromSdp(inputs[0], pS->capCfg.common.filt.filters,
                                   buf, sizeof(buf), &sdp, pParams->novid, pParams->noaud)) <= 0) {
      rc = VSX_RC_ERROR;

    } else {

      if(pParams->verbosity >= VSX_VERBOSITY_HIGH) {
        sdp_write(&sdp, NULL, S_DEBUG);
      }

      vsxlib_set_sdp_from_capture(&pS->streamerCfg, &sdp);

      pS->capCfg.common.filt.numFilters = sz;
      inputs[0] = buf;
      inputs[1] = NULL;
      pParams->islive = 1;
      pParams->strfilters[0] = pParams->strfilters[1] = NULL;
    }

  } else if(rc == VSX_RC_OK && isDevCapture(pParams)) {
    pParams->islive = 1;
    pParams->pktqslots = 2; // frame buffer queue
    pS->capCfg.common.pCapDevCfg = &pPrivData->capDevCfg;
  }

  if(rc == VSX_RC_OK) {
    rc = vsxlib_set_output(&pS->streamerCfg, pParams->outputs, pParams->rtcpPorts, 
                            pParams->transproto, pParams->sdpoutpath, 
                            pParams->srtpCfgs, &pParams->stunRequestorCfg, &pParams->turnCfg);
  } 

  if(rc == VSX_RC_OK) {

    if(pParams->pktqslots  > PKTQUEUE_LEN_MAX) {
      pParams->pktqslots = PKTQUEUE_LEN_MAX;
    } else if(pParams->pktqslots <= 0) {
      pParams->pktqslots = PKTQUEUE_LEN_DEFAULT;
    }

    if(pParams->frvidqslots > FRAMEQUEUE_LEN_MAX) {
      pParams->frvidqslots = FRAMEQUEUE_LEN_MAX;
    } else if(pParams->frvidqslots < 2) {
      pParams->frvidqslots = VIDFRAMEQ_LEN_DEFAULT;
    }
    if(pParams->fraudqslots > FRAMEQUEUE_LEN_MAX) {
      pParams->fraudqslots = FRAMEQUEUE_LEN_MAX;
    } else if(pParams->fraudqslots < 2) {
      pParams->fraudqslots = AUDFRAMEQ_LEN_DEFAULT;
    }

    pS->streamerCfg.frvidqslots = pParams->frvidqslots;
    pS->streamerCfg.fraudqslots = pParams->fraudqslots;

  }

  if(rc == VSX_RC_OK && !pParams->islive) {

    if(pS->streamerCfg.proto == STREAM_PROTO_INVALID) {
      pS->streamerCfg.proto = STREAM_PROTO_AUTO;
    }
    //pS->streamerCfg.mp2tfile_extractpes = pParams->extractpes;

    if(pParams->strstartoffset) {
      if((pS->streamerCfg.fStart = (float) atof(pParams->strstartoffset)) <= 0.0) {
        LOG(X_ERROR("Invalid start offset %s"), pParams->strstartoffset);
        capture_freeFilters(pS->capCfg.common.filt.filters, pS->capCfg.common.filt.numFilters);
        rc = VSX_RC_ERROR;
      }
    }

    if(rc == VSX_RC_OK && pParams->strdurationmax) {
      if((pS->streamerCfg.fDurationMax = (float) atof(pParams->strdurationmax)) <= 0.0) {
        LOG(X_DEBUG("Stream duration set to %.1f sec"), pS->streamerCfg.fDurationMax);
        pS->streamerCfg.fDurationMax = 0;
      }
    }

  } else if(rc == VSX_RC_OK) {

#if defined(VSX_HAVE_CAPTURE)

    if(BOOL_ISDFLT(pParams->extractpes)) {
      pParams->extractpes = checkEnablePesExtract((pS->streamerCfg.proto & STREAM_PROTO_RTP),
                       (pParams->strxcode ? 1 : 0),
                       (pParams->rtspliveaddr[0] ? 1 : 0),
                       (pParams->rtmpliveaddr[0] ? 1 : 0),
                       ((pParams->flvliveaddr[0] || do_flvrecord) ? 1 : 0),
                       ((pParams->mkvliveaddr[0] || do_mkvrecord) ? 1 : 0),
                       ((pParams->httpliveaddr[0] || do_httplive_segmentor) ? 1 : 0),
                       (pParams->tsliveaddr[0] ? 1 : 0),
                       (pParams->dashliveaddr[0] ? 1 : 0));
    }

    if(pS->streamerCfg.proto == STREAM_PROTO_INVALID || 
       pS->streamerCfg.proto == STREAM_PROTO_AUTO) {
      pS->streamerCfg.proto = STREAM_PROTO_MP2TS;
    }
    pS->streamerCfg.u.cfgmp2ts.replayUseLocalTiming = pParams->uselocalpcr;

    pS->capCfg.common.localAddrs[0] = inputs[0];
    pS->capCfg.common.localAddrs[1] = inputs[1];
    pS->capCfg.common.rtsptranstype = pParams->rtsptranstype;
    pS->capCfg.common.streamflags = pParams->streamflags;

    if(vsxlib_stream_setupcap(pParams, &pS->capCfg, &pS->streamerCfg, &pS->streamerCfg.sdpsx[1][0]) < 0) {
      rc = VSX_RC_ERROR;
    }

#endif // VSX_HAVE_CAPTURE

  } // end of islive

  if(pParams->strlocalhost) {
    if(net_setlocaliphost(pParams->strlocalhost) < 0) {
      LOG(X_ERROR("Unable to use specified ip/host %s"), pParams->strlocalhost);
    } else {
      LOG(X_DEBUG("Using local hostname %s"), pParams->strlocalhost);
    }
  }

  pthread_mutex_init(&pS->streamerCfg.mtxStrmr, NULL);
  if(rc == VSX_RC_OK && (do_server || do_flvrecord || do_mkvrecord || pParams->dash_moof_segments == 1 || 
                         pS->streamerCfg.action.do_rtspannounce || pS->streamerCfg.action.do_rtmppublish)) {
    vsxlib_setsrvconflimits(pParams, STREAMER_OUTFMT_MAX, STREAMER_LIVEQ_MAX, 
                             STREAMER_OUTFMT_MAX, STREAMER_OUTFMT_MAX, VSX_CONNECTIONS_MAX);
  }

  //
  // Set the outfmt queue slot sizes
  //
  set_qslot_sizes(&pS->streamerCfg, pParams);


  if(rc == VSX_RC_OK && (do_flvrecord || do_mkvrecord)) {
#if !defined(ENABLE_RECORDING)
    LOG(X_ERROR(VSX_RECORDING_DISABLED_BANNER));
    LOG(X_ERROR(VSX_FEATURE_DISABLED_BANNER));
#endif // !(ENABLE_RECORDING)
  }

  //
  // Setup any stream recorders
  //
  if(rc == VSX_RC_OK && do_flvrecord) {
    
    if(vsxlib_setupFlvRecord(pS->flvRecordCtxts, &pS->streamerCfg, pParams) < 0) {
      rc = VSX_RC_ERROR;
    } else {
      pS->streamerCfg.action.do_flvliverecord = 1;
    }
  }

  if(rc == VSX_RC_OK && do_mkvrecord) {
    
    if(vsxlib_setupMkvRecord(pS->mkvRecordCtxts, &pS->streamerCfg, pParams) < 0) {
      rc = VSX_RC_ERROR;
    } else {
      pS->streamerCfg.action.do_mkvliverecord = 1;
    }
  }

  if(rc == VSX_RC_OK && pS->streamerCfg.action.do_rtmppublish) {
    if(vsxlib_setupRtmpPublish(&pS->streamerCfg, pParams) < 0) {
      rc = VSX_RC_ERROR;
    }
  }

  if(rc == VSX_RC_OK && (pParams->dash_moof_segments == 1|| pParams->dash_ts_segments == 1)) {

    if(setupDash(pS->moofRecordCtxts, &pS->streamerCfg, pParams) < 0) {
      rc = VSX_RC_ERROR;
    } else {
      pS->streamerCfg.action.do_moofliverecord = 1;
    }
  }

  //
  // Set up any server listeners
  //
  //TODO: these really don't need to be duplicated between VSXLIB_STREAM_PARAMS and SRV_CFG
  //memset(&srvParam, 0, sizeof(srvParam));

  if(rc == VSX_RC_OK && do_server) {

    if(!pParams->devconfpath ) {
      pParams->devconfpath = VSX_DEVICESFILE_PATH; 
    } 
   
    if(pParams->livepwd && pParams->livepwd[0] != '\0') {
      pS->srvParam.startcfg.cfgShared.livepwd = pParams->livepwd;
    }

    pS->srvParam.pStreamerCfg = &pS->streamerCfg;
    pS->srvParam.startcfg.cfgShared.enable_symlink = pParams->enable_symlink;
    pS->srvParam.startcfg.cfgShared.disable_root_dirlist = pParams->disable_root_dirlist;
    pS->srvParam.startcfg.cfgShared.pRtspSessions = &pS->rtspSessions;
    pS->srvParam.startcfg.cfgShared.pListenRtmp = pS->srvParam.startcfg.listenRtmp;
    pS->srvParam.startcfg.cfgShared.pListenRtsp = pS->srvParam.startcfg.listenRtsp;
    pS->srvParam.startcfg.cfgShared.pListenHttp = pS->srvParam.startcfg.listenHttp;

    for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) { 
      pS->srvParam.startcfg.cfgShared.pMoofCtxts[idx] = &pS->moofRecordCtxts[idx];
      pS->mpdMp2tsCtxt[idx].pAvCtxt = &pS->moofRecordCtxts[idx].av;
      pS->httpLiveDatas[idx].fs.fp = FILEOPS_INVALID_FP;
    }

    if(fileops_getcwd(pS->srvParam.cwd, sizeof(pS->srvParam.cwd))) {
      pS->srvParam.startcfg.phomedir = pS->srvParam.mediaDb.homeDir = pS->srvParam.cwd;
      pS->srvParam.startcfg.cfgShared.pMediaDb = &pS->srvParam.mediaDb;
    }

    if(vsxlib_setupServer(&pS->srvParam, pParams, do_httplive_segmentor) < 0) {
      rc = VSX_RC_ERROR;
    }

  }

  //
  // Init RTSP interleaved output
  //
  if(rc == VSX_RC_OK && pS->streamerCfg.action.do_rtspannounce) {
    if(vsxlib_initRtspInterleaved(&pS->streamerCfg, 1, pParams) < 0) {  
      rc = VSX_RC_ERROR;
    }
  }

  pS->streamerCfg.abrSelfThrottle.active = pParams->abrSelf;
  //pS->streamerCfg.frameThin = pParams->frameThin;

  //
  // Start the stream output monitor / abr
  //
  if(rc == VSX_RC_OK && (pParams->statfilepath || pParams->statusaddr[0] || pParams->abrAuto)) {

    if(pParams->abrAuto) {
      pS->streamerCfg.doAbrAuto = pParams->abrAuto;
      if(stream_abr_monitor(&pS->streamMonitor, &pS->streamerCfg) < 0) {
        rc = VSX_RC_ERROR;
      } 
    } else if(stream_monitor_start(&pS->streamMonitor, pParams->statfilepath, pParams->statdumpintervalms) < 0) {
      rc = VSX_RC_ERROR;
    }
    pS->streamerCfg.pMonitor = &pS->streamMonitor;
  }

  pPrivData->inUse = 1;

#if defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)

  if(rc == VSX_RC_OK && (pParams->pipCfg[0].input || pParams->pipaddr[0])) {
    init_pip = 1;
  }

  if(rc == VSX_RC_OK) {
    pthread_mutex_init(&pS->streamerCfg.rtspannounce.mtx, NULL);
    pthread_mutex_init(&pS->streamerCfg.sharedCtxt.mtxRtcpHdlr, NULL);
    //pthread_mutex_init(&pS->streamerCfg.sharedCtxt.mtxStunXmit, NULL);
    pthread_mutex_init(&pS->streamerCfg.sharedCtxt.turnThreadCtxt.mtx, NULL);
    pthread_mutex_init(&pS->streamerCfg.sharedCtxt.turnThreadCtxt.notify.mtx, NULL);
    pthread_cond_init(&pS->streamerCfg.sharedCtxt.turnThreadCtxt.notify.cond, NULL);
  }

  if(init_pip) {

  if(rc == VSX_RC_OK) {

    //
    // Initialize PIPs
    //
//TODO: some of these need to be init regardless of pip
    pthread_mutex_init(&pS->streamerCfg.xcode.vid.overlay.mtx, NULL);
    pthread_mutex_init(&pS->streamerCfg.pip.mtx, NULL);

    pS->pipLayout.pPip = &pS->streamerCfg.pip;
    pS->streamerCfg.pip.pipMax = pParams->pipMax;
    pS->streamerCfg.pip.pCfgOverlay = &pS->streamerCfg;
    pParams->pipCfg[0].fps = pParams->fps;
    pS->streamerCfg.pip.tvLastPipAction = timer_GetTime();
    for(idx = 0; idx < MAX_PIPS; idx++) {

      pS->streamerCfg.pip.pCfgPips[idx] = &pS->streamerCfgPips[idx];
      pthread_mutex_init(&pS->streamerCfgPips[idx].xcode.vid.overlay.mtx, NULL);
      pthread_mutex_init(&pS->streamerCfgPips[idx].mtxStrmr, NULL);
      pthread_mutex_init(&pS->streamerCfgPips[idx].rtspannounce.mtx, NULL);
      pthread_mutex_init(&pS->streamerCfgPips[idx].sharedCtxt.mtxRtcpHdlr, NULL);
      pthread_mutex_init(&pS->streamerCfgPips[idx].sharedCtxt.turnThreadCtxt.mtx, NULL);
      pthread_mutex_init(&pS->streamerCfgPips[idx].sharedCtxt.turnThreadCtxt.notify.mtx, NULL);
      pthread_cond_init(&pS->streamerCfgPips[idx].sharedCtxt.turnThreadCtxt.notify.cond, NULL);
      //pthread_mutex_init(&pS->streamerCfgPips[idx].sharedCtxt.mtxStunXmit, NULL);
      pS->streamerCfgPips[idx].running = STREAMER_STATE_FINISHED_PIP;
      pS->streamerCfgPips[idx].pmaxRtp = &pipMaxRtp;
      pS->streamerCfgPips[idx].pStorageBuf = pS;

      pS->streamerCfgPips[idx].pXcodeCtxt = &pS->xcodeCtxtPips[idx];
      pS->xcodeCtxtPips[idx].vidUData.pVidData = &pS->xcodeCtxtPips[idx].vidData;
      pS->xcodeCtxtPips[idx].vidUData.type = XC_CODEC_TYPE_VID_GENERIC;
      pS->xcodeCtxtPips[idx].vidUData.pStreamerPip = &pS->streamerCfgPips[idx].pip;
      pS->xcodeCtxtPips[idx].vidUData.enableOutputSharing = 0;
      pS->xcodeCtxtPips[idx].vidUData.pStreamerFbReq = &pS->streamerCfgPips[idx].fbReq;
     
      pS->xcodeCtxtPips[idx].audUData.pAudData = &pS->xcodeCtxtPips[idx].audData;
      pS->xcodeCtxtPips[idx].audUData.type = XC_CODEC_TYPE_AUD_GENERIC;
      pS->xcodeCtxtPips[idx].audUData.pPipLayout =  &pS->pipLayout;

      pS->streamerCfgPips[idx].xcode.vid.pUserData = &pS->xcodeCtxtPips[idx].vidUData;
      pS->streamerCfgPips[idx].xcode.aud.pUserData = &pS->xcodeCtxtPips[idx].audUData;

      pthread_mutex_init(&pS->streamerCfgPips[idx].pip.mtx, NULL);
      pthread_mutex_init(&pS->streamerCfgPips[idx].pip.mtxCond, NULL);
      pthread_cond_init(&pS->streamerCfgPips[idx].pip.cond, NULL);

      pS->srvParamPips[idx].startcfg.cfgShared.livepwd = pS->srvParam.startcfg.cfgShared.livepwd;
      pS->srvParamPips[idx].pStreamerCfg = &pS->streamerCfgPips[idx];
      pS->srvParamPips[idx].startcfg.cfgShared.enable_symlink = pS->srvParam.startcfg.cfgShared.enable_symlink;
      pS->srvParamPips[idx].startcfg.cfgShared.pRtspSessions = &pS->rtspSessionsPips[idx];
      pS->srvParamPips[idx].startcfg.cfgShared.pListenRtmp = pS->srvParamPips[idx].startcfg.listenRtmp;
      pS->srvParamPips[idx].startcfg.cfgShared.pListenRtsp = pS->srvParamPips[idx].startcfg.listenRtsp;
      pS->srvParamPips[idx].startcfg.cfgShared.pListenHttp = pS->srvParamPips[idx].startcfg.listenHttp;
      pS->srvParamPips[idx].startcfg.phomedir = pS->srvParamPips[idx].mediaDb.homeDir = pS->srvParam.cwd;
      pS->srvParamPips[idx].startcfg.cfgShared.pMediaDb = &pS->srvParam.mediaDb;
      set_qslot_sizes(&pS->streamerCfgPips[idx], pParams);

      pS->rtspSessionsPips[idx].staticLocalPort = pParams->rtsplocalport;
      pS->rtspSessionsPips[idx].rtspsecuritytype = pParams->rtspsecuritytype;
      pS->rtspSessionsPips[idx].rtsptranstype = pParams->rtsptranstype;
      pS->rtspSessionsPips[idx].rtspForceTcpUAList.str = pParams->rtspualist;
      pS->streamerCfgPips[idx].pRtspSessions = &pS->rtspSessionsPips[idx];

      if(vsxlib_alloc_destCfg(&pS->streamerCfgPips[idx], 0) < 0) {
        rc = VSX_RC_ERROR;
      }
    } // end of for(idx = 0 ...

  } // end of if(rc == VSX_RC_OK... 

  pS->streamerCfg.pip.layoutType = PIP_LAYOUT_TYPE_MANUAL;

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  if(rc == VSX_RC_OK) {

    pS->streamerCfg.xcode.aud.pXcodeV = &pS->streamerCfg.xcode.vid;
    pS->streamerCfg.pip.pMixerCfg = &pParams->mixerCfg;
    if(pParams->mixerCfg.active) {

      pS->streamerCfg.pip.layoutType = pParams->mixerCfg.layoutType;
      if(!pS->streamerCfg.pip.pMixerCfg->vad) {
        if(pS->streamerCfg.pip.layoutType == PIP_LAYOUT_TYPE_VAD_P2P) {
          pS->streamerCfg.pip.layoutType = PIP_LAYOUT_TYPE_GRID_P2P;
        } else if(pS->streamerCfg.pip.layoutType == PIP_LAYOUT_TYPE_VAD) {
          pS->streamerCfg.pip.layoutType = PIP_LAYOUT_TYPE_GRID;
        }
      }

      pS->streamerCfg.pip.maxUniquePipLayouts = MAX_PIPS;

      LOG(X_DEBUG("Conference overlay layout type is %s"), pip_layout2str(pS->streamerCfg.pip.layoutType)); 

      if(pParams->mixerCfg.conferenceInputDriver) {
        //
        // Create the audio mixer instance wince the main overlay is not processing any audio & video, but
        // is only used as a fps frame driver for overlay encoding
        //
        if(pip_create_mixer(&pS->streamerCfg, STREAMER_DO_OUTPUT(pS->streamerCfg.action) ? 1 : 0) < 0) {
          rc = VSX_RC_ERROR;
        }
      } else {
        //
        // Delay the mixer creation until the first input audio sample from the main overlay
        // determines the input Hz
        //
        pS->xcodeCtxt.audUData.pStartMixer = &pS->streamerCfg;
      }
    }
  }

#endif // XCODE_HAVE_PIP_AUDIO

  if(rc == VSX_RC_OK) {

    //
    // Initialize PIPs
    //
    //pthread_mutex_init(&pS->streamerCfg.pip.mtx, NULL);
    //pthread_mutex_init(&pS->streamerCfg.pip.mtxCond, NULL);
    //pthread_cond_init(&pS->streamerCfg.pip.cond, NULL);

    //if(pParams->pipCfg[0].input || pParams->pipaddr[0]) {
    //  init_pip = 1;
    //}

    //
    // Check if the user is actually specifying a --pipconf file not --pip media file
    //
    if(pParams->pipCfg[0].input != NULL && pParams->pipCfg[0].cfgfile == NULL) {
      if(strutil_isTextFile(pParams->pipCfg[0].input)) {
        pParams->pipCfg[0].cfgfile = pParams->pipCfg[0].input;
        pParams->pipCfg[0].input = NULL;
      }
    }

    //
    // Start any PIP input feeds
    //
    if(pParams->pipCfg[0].cfgfile != NULL) {
      pS->pipxcodeStrbuf[0][0] = '\0';
      pipMotion = pip_read_conf(&pParams->pipCfg[0], 
                                pS->streamerCfg.status.pathFile,
                                pS->pipxcodeStrbuf[0]);
      if(!pipMotion && !pParams->pipCfg[0].input) {
        rc = VSX_RC_ERROR;
      }
    }

    if(rc == VSX_RC_OK && pParams->pipCfg[0].input != NULL) {
      if(pParams->pipCfg[0].layoutType == PIP_LAYOUT_TYPE_UNKNOWN) {
        pParams->pipCfg[0].layoutType = PIP_LAYOUT_TYPE_MANUAL;
      }
      if((irc = pip_start(&pParams->pipCfg[0], &pS->streamerCfg, pipMotion)) < VSX_RC_OK) {
      }
      pipMotion = NULL;
    }

  } // end of if(rc == VSX_RC_OK...

  } // init_pip

#endif //XCODE_HAVE_PIP

  if(rc == VSX_RC_OK && !pParams->islive) {

    if(vsxlib_streamFileCfg(inputs[0], pParams->fps, &pS->streamerCfg) < 0) {
      rc = VSX_RC_ERROR;
    }

  } else if(rc == VSX_RC_OK) {

#if defined(VSX_HAVE_CAPTURE)

    if(vsxlib_streamLiveCaptureCfg(&pS->capCfg, &pS->streamerCfg) < 0) {
      rc = VSX_RC_ERROR;
    }

#else
    rc = VSX_RC_ERROR;
#endif // VSX_HAVE_CAPTURE

  }

  //
  // sleep to allow delayed dash / httplive downloaders to finish
  //

  if(!g_proc_exit && rc == VSX_RC_OK) {

    sz = 0;
    buf[0] = '\0';

    if(pS->streamerCfg.xcode.vid.common.cfgDo_xcode) {
      if(4 > sz) {
        sz = 4; 
        snprintf(buf, sizeof(buf), "transcoding delay");
      }
    }

    if(pS->streamerCfg.action.do_httplive && !pParams->httplive_nodelete) {
      if(3 * HTTPLIVE_DURATION_DEFAULT > sz) {
        sz = 3 * HTTPLIVE_DURATION_DEFAULT;
        snprintf(buf, sizeof(buf), "httplive");
      }
    } 

    if(pS->streamerCfg.action.do_moofliverecord && !pParams->moof_nodelete) {
      if(3 * MOOF_MP4_MAX_DURATION_SEC_DEFAULT > sz) {
        sz = 3 * MOOF_MP4_MAX_DURATION_SEC_DEFAULT;
        snprintf(buf, sizeof(buf), "dash");
      }
    }

    if(pS->streamerCfg.action.do_flvlive ||  pS->streamerCfg.action.do_rtmplive ||
      pS->streamerCfg.action.do_mkvlive || pS->streamerCfg.action.do_rtsplive || 
      pS->streamerCfg.action.do_rtspannounce || pS->streamerCfg.action.do_rtmppublish) {
      if(2 > sz) {
        sz = 2; 
        snprintf(buf, sizeof(buf), "http clients");
      }
    }

    if(sz > 0) {
      LOG(X_DEBUG("Waiting for %d sec to accomodate %s"), sz, buf);
      sleep(sz);
    }
  }

  //
  // Do Cleanup
  // 

  if(do_flvrecord) {
    for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
      flvsrv_record_stop(&pS->flvRecordCtxts[idx]);
    }
    vsxlib_closeOutFmt(&pS->streamerCfg.action.liveFmts.out[STREAMER_OUTFMT_IDX_FLVRECORD]);
  }

  if(do_mkvrecord) {
    for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
      mkvsrv_record_stop(&pS->mkvRecordCtxts[idx]);
    }
    vsxlib_closeOutFmt(&pS->streamerCfg.action.liveFmts.out[STREAMER_OUTFMT_IDX_MKVRECORD]);
  }

  if(pS->streamerCfg.action.do_moofliverecord) {
    for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
      moofsrv_record_stop(&pS->moofRecordCtxts[idx]);
    }
    vsxlib_closeOutFmt(&pS->streamerCfg.action.liveFmts.out[STREAMER_OUTFMT_IDX_MOOF]);
  }

  if(pS->streamerCfg.action.do_rtspannounce) {
    //TODO: RTSP is not available for PIP(s)
    vsxlib_closeRtspInterleaved(&pS->streamerCfg);
  }
  if(pS->streamerCfg.action.do_rtmppublish) {
    //TODO: RTMP is not available for PIP(s)
    vsxlib_closeOutFmt(&pS->streamerCfg.action.liveFmts.out[STREAMER_OUTFMT_IDX_RTMPPUBLISH]);
    //vsxlib_closeRtmpPublish(&pS->streamerCfg);
  }
  if(pS->srvParam.isinit) {
    vsxlib_closeServer(&pS->srvParam);
  }

  if(init_pip) {

#if defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)

    //
    // Stop any PIP input feeds
    //

    do { 
      pip_running = 0;
      for(idx = 0; idx < MAX_PIPS; idx++) {

        //fprintf(stderr, "CHECK STOPPING PIP[%d] running:%d, pip_running:%d 0x%x\n", idx, pS->streamerCfgPips[idx].running, pip_running, &pS->streamerCfgPips[idx]);

        if(pS->streamerCfgPips[idx].running != STREAMER_STATE_FINISHED_PIP &&
           pS->streamerCfgPips[idx].running != STREAMER_STATE_ERROR) {

          //fprintf(stderr, "STOPPING PIP[%d] running:%d, pip_running:%d 0x%x\n", idx, pS->streamerCfgPips[idx].running, pip_running, &pS->streamerCfgPips[idx]);
          //pip_stop(&pS->streamerCfg, idx);
          pip_stop(&pS->streamerCfg, pS->streamerCfgPips[idx].xcode.vid.pip.id);

          if(pS->streamerCfgPips[idx].running != STREAMER_STATE_FINISHED_PIP &&
             pS->streamerCfgPips[idx].running != STREAMER_STATE_ERROR) {
            pip_running++;
          }
        }

        vsxlib_cond_broadcast(&pS->streamerCfgPips[idx].pip.cond, &pS->streamerCfgPips[idx].pip.mtxCond);
        //pthread_cond_destroy(&pS->streamerCfgPips[idx].pip.cond);
        //pthread_mutex_destroy(&pS->streamerCfgPips[idx].pip.mtxCond);

      }
      if(pip_running) {
        usleep(20000);
      }

    } while(pip_running > 0);

    //usleep(10000);
//TODO: put breakpoint here and see where the streamerCfgPips[0] is... I think its sets running to -2 before capture_net stream_capture.. has a chance to finish..
    //fprintf(stderr, "DONE STOPPING PIPS...\n");

    //pthread_mutex_destroy(&pS->streamerCfg.sharedCtxt.mtxStunXmit);
    pthread_mutex_destroy(&pS->streamerCfg.pip.mtx);
    pip_free_overlay(&pS->streamerCfg.xcode.vid.overlay);
    if(pipMotion) {
      pip_free_motion(pipMotion, 1);
    }
    for(idx = 0; idx < MAX_PIPS; idx++) {
      pthread_mutex_lock(&pS->streamerCfgPips[idx].mtxStrmr);
      pthread_mutex_unlock(&pS->streamerCfgPips[idx].mtxStrmr);
      pthread_mutex_destroy(&pS->streamerCfgPips[idx].mtxStrmr);
      pthread_cond_destroy(&pS->streamerCfgPips[idx].pip.cond);
      pthread_mutex_destroy(&pS->streamerCfgPips[idx].pip.mtx);
      pthread_mutex_destroy(&pS->streamerCfgPips[idx].pip.mtxCond);
      pthread_mutex_destroy(&pS->streamerCfgPips[idx].rtspannounce.mtx);
      pthread_mutex_destroy(&pS->streamerCfgPips[idx].sharedCtxt.mtxRtcpHdlr);
      vsxlib_cond_broadcast(&pS->streamerCfgPips[idx].sharedCtxt.turnThreadCtxt.notify.cond,
                             &pS->streamerCfgPips[idx].sharedCtxt.turnThreadCtxt.notify.mtx);
      pthread_mutex_destroy(&pS->streamerCfgPips[idx].sharedCtxt.turnThreadCtxt.notify.mtx);
      pthread_cond_destroy(&pS->streamerCfgPips[idx].sharedCtxt.turnThreadCtxt.notify.cond);
      pthread_mutex_destroy(&pS->streamerCfgPips[idx].sharedCtxt.turnThreadCtxt.mtx);
      //pthread_mutex_destroy(&pS->streamerCfgPips[idx].sharedCtxt.mtxStunXmit);
      pip_free_overlay(&pS->streamerCfgPips[idx].xcode.vid.overlay);

      vsxlib_free_destCfg(&pS->streamerCfgPips[idx]);
    }

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
    if(pS->streamerCfg.xcode.aud.pMixer) {
     conference_destroy(pS->streamerCfg.xcode.aud.pMixer);
    }
#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

#endif // XCODE_HAVE_PIP

  } // end of if(init_pip...

  pthread_mutex_destroy(&pS->streamerCfg.rtspannounce.mtx);
  pthread_mutex_destroy(&pS->streamerCfg.sharedCtxt.mtxRtcpHdlr);
  vsxlib_cond_broadcast(&pS->streamerCfg.sharedCtxt.turnThreadCtxt.notify.cond,
                         &pS->streamerCfg.sharedCtxt.turnThreadCtxt.notify.mtx);
  pthread_mutex_destroy(&pS->streamerCfg.sharedCtxt.turnThreadCtxt.notify.mtx);
  pthread_cond_destroy(&pS->streamerCfg.sharedCtxt.turnThreadCtxt.notify.cond);
  pthread_mutex_destroy(&pS->streamerCfg.sharedCtxt.turnThreadCtxt.mtx);

  if(pS->streamMonitor.active) {
    stream_monitor_stop(&pS->streamMonitor);
  }

  if(pS->capCfg.common.filt.filters) {
    capture_freeFilters(pS->capCfg.common.filt.filters, pS->capCfg.common.filt.numFilters);
  }
  if(pConf) {
    conf_free(pConf);
  }

  vsxlib_free_destCfg(&pS->streamerCfg);

  //
  // Don't exit this func & lose the stack if anything is still using the streamer,
  // such as a call to vsxlib_update / xcode_update
  //
  pthread_mutex_lock(&pS->streamerCfg.mtxStrmr);
  pthread_mutex_unlock(&pS->streamerCfg.mtxStrmr);
  pthread_mutex_destroy(&pS->streamerCfg.mtxStrmr);
  pPrivData->pStorageBuf = NULL;
  pPrivData->inUse = 0;

  LOG(X_DEBUG("All streaming done"));

  usleep(100000);
  avc_free((void **) &pS);

  return rc;
}

#endif // VSX_HAVE_STREAMER

//extern int g_proc_exit;
//extern pthread_cond_t *g_proc_exit_cond;

VSX_RC_T vsxlib_open(VSXLIB_STREAM_PARAMS_T *pParams) {
  VSX_RC_T rc = VSX_RC_OK;

  if(!pParams) {
    return VSX_RC_ERROR;
  }

  g_proc_exit = 0;

  srandom((unsigned int) time(NULL));

  memset(pParams, 0, sizeof(VSXLIB_STREAM_PARAMS_T));

  if(!(pParams->pPrivate = avc_calloc(1, sizeof(VSXLIB_PRIVDATA_T)))) {
    return VSX_RC_ERROR;
  }  

  //
  // Setup default parameters
  //
  pParams->frtcp_sr_intervalsec = STREAM_RTCP_SR_INTERVAL_SEC;
  pParams->capture_rtp_frame_drop_policy = CAPTURE_FRAME_DROP_POLICY_DEFAULT;
  pParams->rtpPktzMode = -1;
  pParams->rtp_useCaptureSrcPort = -1;

  pParams->firCfg.fir_encoder = XCODE_IDR_VIA_FIR ? BOOL_ENABLED_DFLT : BOOL_DISABLED_DFLT;
  pParams->firCfg.fir_recv_via_rtcp = BOOL_ENABLED_DFLT;
  pParams->firCfg.fir_recv_from_connect = BOOL_ENABLED_DFLT;
  pParams->firCfg.fir_recv_from_remote = BOOL_ENABLED_DFLT;
  pParams->firCfg.fir_send_from_local = BOOL_ENABLED_DFLT;
  pParams->firCfg.fir_send_from_decoder = BOOL_ENABLED_DFLT;
  pParams->firCfg.fir_send_from_remote = BOOL_DISABLED_DFLT;
  pParams->firCfg.fir_send_from_capture = BOOL_DISABLED_DFLT;

  pParams->nackRtpRetransmitVideo = BOOL_DISABLED_DFLT;
  pParams->nackRtcpSendVideo = BOOL_ENABLED_DFLT;
  pParams->apprembRtcpSendVideo = BOOL_ENABLED_DFLT;
  pParams->apprembRtcpSendVideoMaxRateBps = 0;
  pParams->apprembRtcpSendVideoMinRateBps = 0;
  pParams->apprembRtcpSendVideoForceAdjustment = BOOL_DISABLED_DFLT;
  pParams->frameThin = BOOL_DISABLED_DFLT;
  pParams->extractpes = BOOL_DISABLED_DFLT;

  pParams->srtpCfgs[0].dtls_handshake_server = -1;

  pParams->capture_idletmt_ms = STREAM_CAPTURE_IDLETMT_MS;
  pParams->capture_idletmt_1stpkt_ms = STREAM_CAPTURE_IDLETMT_1STPKT_MS;
  //pParams->capture_max_rtpplayoutviddelayms = CAPTURE_RTP_VID_JTBUF_GAP_TS_MAXWAIT_MS;
  //pParams->capture_max_rtpplayoutauddelayms = CAPTURE_RTP_AUD_JTBUF_GAP_TS_MAXWAIT_MS;
  pParams->caphighprio = 1;
  pParams->xmithighprio = 1;
  pParams->rtmpfp9 = RTMP_CLIENT_FP9;
  pParams->rtmpdotunnel = BOOL_ENABLED_DFLT;
  pParams->statusmax = 0;
  pParams->enable_symlink = HTTP_FOLLOW_SYMLINKS_DEFAULT; // Follow symlinks by default
  //pParams->httpmax = VSX_CONNECTIONS_DEFAULT;
  pParams->httpmax = 0;
  pParams->rtplivemax = STREAMER_RTP_DEFAULT;
  pParams->rtmplivemax = STREAMER_OUTFMT_DEFAULT;
  pParams->flvlivemax = STREAMER_OUTFMT_DEFAULT;
  pParams->mkvlivemax = STREAMER_OUTFMT_DEFAULT;
  pParams->rtsplivemax = STREAMER_LIVEQ_DEFAULT;
  pParams->tslivemax = STREAMER_LIVEQ_DEFAULT;
  pParams->httplivemax = STREAMER_LIVEQ_DEFAULT;
  pParams->dashlivemax = STREAMER_LIVEQ_DEFAULT;
  pParams->logmaxsz = LOGGER_MAX_FILE_SZ;
  pParams->logrollmax = LOGGER_MAX_FILES;
  pParams->rtsprefreshtimeoutviartcp = 1;
  pParams->flvlivedelay = FLVLIVE_DELAY_DEFAULT;
  pParams->mkvlivedelay = MKVLIVE_DELAY_DEFAULT;
  pParams->outq_prealloc = BOOL_ENABLED_DFLT;
  
  // MOOF mp4 creation defaults
  pParams->dash_moof_segments = -1;
  pParams->dash_ts_segments = -1;
  pParams->dash_mpd_type = DASH_MPD_TYPE_DEFAULT;
  pParams->moofUseInitMp4 = MOOF_USE_INITMP4_DEFAULT;
  pParams->moofUseRAP = MOOF_USE_RAP_DEFAULT;
  pParams->moofSyncSegments = -1;
  pParams->moofTrafMaxDurationSec = MOOF_TRAF_MAX_DURATION_SEC_DEFAULT;
  pParams->moofMp4MaxDurationSec = MOOF_MP4_MAX_DURATION_SEC_DEFAULT;
  pParams->moofMp4MinDurationSec = MOOF_MP4_MIN_DURATION_SEC_DEFAULT;

#if defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)
  pParams->pipMax = -1;
#endif // (XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
  pParams->mixerCfg.vad = 1;
  pParams->mixerCfg.agc = 1;
  pParams->mixerCfg.denoise = 1;
  pParams->mixerCfg.layoutType = PIP_LAYOUT_TYPE_GRID_P2P;  // Default conferencing layout
  pParams->mixerCfg.pip_idletmt_ms = STREAM_PIP_IDLETMT_MS;
  pParams->mixerCfg.pip_idletmt_1stpip_ms = STREAM_PIP_IDLETMT_1STPIP_MS;
#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  g_verbosity = pParams->verbosity = VSX_VERBOSITY_NORMAL;

#if defined(VSX_HAVE_SSL)
  pthread_mutex_init(&g_ssl_mtx, NULL);
#endif // VSX_HAVE_SSL

  return rc;
}

VSX_RC_T vsxlib_close(VSXLIB_STREAM_PARAMS_T *pParams) {
  VSX_RC_T rc = 0;
  VSXLIB_PRIVDATA_T *pPrivData;

  //fprintf(stderr, "vsxlib_close called\n");

  g_proc_exit = 1;
  if(g_proc_exit_cond) {
    pthread_cond_broadcast(g_proc_exit_cond);
  }

  if(!pParams) {
    return VSX_RC_ERROR;
  }

  if(pParams->pPrivate) {
    pPrivData = (VSXLIB_PRIVDATA_T *) pParams->pPrivate;

    while(pPrivData->inUse) {
      usleep(20000);
      if(g_proc_exit_cond) {
        pthread_cond_broadcast(g_proc_exit_cond);
      }
    }

    usleep(300000);
    //LOG(X_DEBUG("avc_free from close called 0x%x"), pParams->pPrivate);
    avc_free(&pParams->pPrivate);
    //LOG(X_DEBUG("avc_free from close done"));
  }

#if defined(VSX_HAVE_SSL)
  pthread_mutex_destroy(&g_ssl_mtx);
#endif // VSX_HAVE_SSL

  return rc;
}


VSX_RC_T vsxlib_onVidFrame(VSXLIB_STREAM_PARAMS_T *pParams,
                             const unsigned char *pData, unsigned int len) {
  VSX_RC_T rc = VSX_RC_OK;
  CAPTURE_DEVICE_CFG_T *pCapDevCfg = NULL;

  if(!pParams || !pParams->pPrivate) {
    return VSX_RC_ERROR;
  }

  pCapDevCfg = &((VSXLIB_PRIVDATA_T *)pParams->pPrivate)->capDevCfg;

  if(!pCapDevCfg->pCapVidData) {
    return VSX_RC_OK;
  }

  if(capture_devCbOnFrame(pCapDevCfg->pCapVidData, pData, len) < 0) {
    rc = VSX_RC_ERROR;
  } 

  return rc;
}

VSX_RC_T vsxlib_onAudSamples(VSXLIB_STREAM_PARAMS_T *pParams,
                         const unsigned char *pData, unsigned int len) {
  VSX_RC_T rc = VSX_RC_OK;
  CAPTURE_DEVICE_CFG_T *pCapDevCfg = NULL;

  if(!pParams || !pParams->pPrivate) {
    return VSX_RC_ERROR;
  }

  pCapDevCfg =  &((VSXLIB_PRIVDATA_T *)pParams->pPrivate)->capDevCfg;

  if(!pCapDevCfg->pCapAudData) {
    return VSX_RC_OK;
  }

  if(capture_devCbOnFrame(pCapDevCfg->pCapAudData, pData, len) < 0) {
    rc = VSX_RC_ERROR;
  }

  return rc;
}

VSX_RC_T vsxlib_readVidFrame(VSXLIB_STREAM_PARAMS_T *pParams,
                               unsigned char *pData, unsigned int len,
                               struct timeval *ptv) {
  VSX_RC_T rc = VSX_RC_OK;
  STREAMER_RAWOUT_CFG_T *pRawOutCfg = NULL;
  PKTQ_EXTRADATA_T xtra;

  if(!pParams || !pParams->pPrivate) {
    return VSX_RC_ERROR;
  }

  pRawOutCfg =  &((VSXLIB_PRIVDATA_T *)pParams->pPrivate)->rawOutCfg;

  if(!pRawOutCfg->pVidFrBuf) {
    return VSX_RC_NOTREADY;
  }

  //
  // simply check and return if data is available to be read
  //
  if(!pData || len <= 0) {
    if(pktqueue_havepkt(pRawOutCfg->pVidFrBuf)) {
      return 1;
    } else {
      return VSX_RC_OK;
    }
  }

  //LOG(X_DEBUG("vsxlib_readV haveData:%d"), pRawOutCfg->pVidFrBuf->haveData);
  pktqueue_waitforunreaddata(pRawOutCfg->pVidFrBuf);
  xtra.pQUserData = NULL;

  if((rc = pktqueue_readpkt(pRawOutCfg->pVidFrBuf, pData, len, &xtra, NULL)) < 0) {
    return VSX_RC_ERROR;
  }
  
  if(ptv) {
    if(rc > 0) {
      ptv->tv_sec = (uint32_t) (xtra.tm.pts / 90000);
      ptv->tv_usec = (uint32_t) ((xtra.tm.pts % 90000) * (float)(100.0f / 9.f));
    } else {
      memset(ptv, 0, sizeof(struct timeval));
    }
  }

  return rc;
}

//static FILE_HANDLE delme_a;

VSX_RC_T vsxlib_readAudSamples(VSXLIB_STREAM_PARAMS_T *pParams,
                                 unsigned char *pData, unsigned int len,
                                 struct timeval *ptv) {
  int readrc;
  unsigned int readidx = 0;
  STREAMER_RAWOUT_CFG_T *pRawOutCfg = NULL;
  PKTQ_EXTRADATA_T xtra;

  if(!pParams || !pParams->pPrivate) {
    return VSX_RC_ERROR;
  }

  pRawOutCfg =  &((VSXLIB_PRIVDATA_T *)pParams->pPrivate)->rawOutCfg;

  if(!pRawOutCfg->pAudFrBuf) {
    return VSX_RC_NOTREADY;
  }

  //
  // simply check and return if data is available to be read
  //
  if(!pData || len <= 0) {
    if((readrc = pktqueue_havepkt(pRawOutCfg->pAudFrBuf)) > 0) {
      return readrc;
      //return 1;
    } else {
      return VSX_RC_OK;
    }
  }

  //LOG(X_DEBUG("vsxlib_readA haveData:%d"), pRawOutCfg->pAudFrBuf->haveData);

  while(readidx < len) {
    pktqueue_waitforunreaddata(pRawOutCfg->pAudFrBuf);
    xtra.pQUserData = NULL;

    if((readrc = pktqueue_readpkt(pRawOutCfg->pAudFrBuf, &pData[readidx], 
                   len - readidx, (readidx == 0 ? &xtra : NULL), NULL)) < 0) {
      return VSX_RC_ERROR;
    } else if(readrc == 0) {
      // should no happen
      LOG(X_WARNING("ringbuf_read returned 0"));
      break; 
    }
    readidx += readrc;
  }

  //LOG(X_DEBUG("vsxlib_readA returning %d/%d (qrd:%d wr:%d) at %llums"), readidx, len, pRawOutCfg->pAudFrBuf->idxInRd, pRawOutCfg->pAudFrBuf->idxInWr, timer_GetTime()/1000); 

  //if(delme_a == 0) delme_a  = fileops_Open("/sdcard/delme2.pcm", O_RDWR | O_CREAT);
  //fileops_WriteBinary(delme_a, pData, len);

  
  if(ptv) {
    if(readidx > 0) {
      ptv->tv_sec = (uint32_t) (xtra.tm.pts / 90000);
      ptv->tv_usec = (uint32_t) ((xtra.tm.pts % 90000) * (float)(100.0f / 9.f));
    } else {
      memset(ptv, 0, sizeof(struct timeval));
    }
  }


  return (VSX_RC_T) readidx;
}

VSX_RC_T vsxlib_registerReportCb(VSXLIB_STREAM_PARAMS_T *pParams,
                                  VSXLIB_CB_STREAM_REPORT cbStreamReport,
                                  void *pCbData) {

  VSXLIB_PRIVDATA_T *pPriv;

  if(!pParams || !pParams->pPrivate || !cbStreamReport) {
    return VSX_RC_ERROR;
  }

  pPriv =  (VSXLIB_PRIVDATA_T *) pParams->pPrivate;

  memset(&pPriv->reportDataCfg.reportCbData, 0, sizeof(pPriv->reportDataCfg.reportCbData));
  pPriv->reportDataCfg.reportCb = cbStreamReport;
  pPriv->reportDataCfg.pCbData = pCbData;

  return VSX_RC_OK;
}

VSX_RC_T vsxlib_update(VSXLIB_STREAM_PARAMS_T *pParams) {

  VSX_RC_T rc = VSX_RC_OK;
  int rci;
  VSXLIB_PRIVDATA_T *pPriv;
  enum STREAM_XCODE_FLAGS flags = 0;

  if(!pParams || !(pPriv =  (VSXLIB_PRIVDATA_T *) pParams->pPrivate) || !(pPriv->pStorageBuf)) {
    return VSX_RC_ERROR;
  }

  if(pParams->strxcode) {
    if((rci = xcode_update(&pPriv->pStorageBuf->streamerCfg, pParams->strxcode, flags)) < 0) {
      rc = VSX_RC_ERROR; 
    }
  }

  return rc;
}


VSX_RC_T vsxlib_showSdp(VSXLIB_STREAM_PARAMS_T *pParam,
                          char *strSdp, unsigned int *plen) {
  VSX_RC_T rc = VSX_RC_OK;

  rc = VSX_RC_ERROR;
  return rc;
}

