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

typedef struct STTS_EXTRACT_CTXT {
  MP4_TRAK_T                       trak;
  BOX_MDHD_T                       boxMdhd;
  BOX_STTS_T                       boxStts;

  unsigned int                     clockHz;
  unsigned int                     frameDeltaHz;
  uint64_t                         totOffsetHz;
  uint64_t                         priorOffsetHz;
  uint64_t                         totOffsetHz2;
  uint64_t                         priorOffsetHz2;
  unsigned int                     allocInStts;
  unsigned int                     maxStts;

} STTS_EXTRACT_CTXT_T;

typedef struct MKV_EXTRACT_CTXT_VID_H264 {
  AVCC_CBDATA_WRITE_SAMPLE_T       cbSampleData;
} MKV_EXTRACT_CTXT_VID_H264_T;

typedef struct MKV_EXTRACT_CTXT_COMMON {
  const char                       *pathprefix;
  FILE_STREAM_T                    fStreamOut;
  int                              wroteSeqHdr;
  STTS_EXTRACT_CTXT_T              sttsCtxt;
  unsigned int                     clockHz;
  int                              do_media;
  int                              do_stts;
} MKV_EXTRACT_CTXT_COMMON_T;

typedef struct MKV_EXTRACT_CTXT_VID {

  MKV_EXTRACT_CTXT_COMMON_T        c;

  union {
    MKV_EXTRACT_CTXT_VID_H264_T    h264;
  } u;

} MKV_EXTRACT_CTXT_VID_T;

typedef struct MKV_EXTRACT_CTXT_AUD_AAC {
  ESDS_CBDATA_WRITE_SAMPLE_T       cbSampleData;
  unsigned char                    buf[4096];
} MKV_EXTRACT_CTXT_AUD_AAC_T;

typedef struct MKV_EXTRACT_CTXT_AUD {
  MKV_EXTRACT_CTXT_COMMON_T        c;

  union {
    MKV_EXTRACT_CTXT_AUD_AAC_T     aac;
  } u;
} MKV_EXTRACT_CTXT_AUD_T;

typedef struct MKV_EXTRACT_CTXT {
  MKV_CONTAINER_T                 *pMkv;
  MKV_EXTRACT_CTXT_VID_T           vid;
  MKV_EXTRACT_CTXT_AUD_T           aud;
} MKV_EXTRACT_CTXT_T;

static int open_output(MKV_EXTRACT_CTXT_COMMON_T *pC, const char *ext) {
  int rc;

  if((rc = snprintf(pC->fStreamOut.filename, sizeof(pC->fStreamOut.filename), "%s.%s", pC->pathprefix, ext)) < 0) {
    return rc;
  }

  if((pC->fStreamOut.fp = fileops_Open(pC->fStreamOut.filename, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_CRITICAL("Unable to open file for writing: %s"), pC->fStreamOut.filename);
    return -1;
  }
  LOG(X_INFO("Created %s"), pC->fStreamOut.filename);

  return 0;
}

static void init_stts(STTS_EXTRACT_CTXT_T *pSttsCtxt, unsigned int clockHz, unsigned int frameDeltaHz) {

  pSttsCtxt->clockHz = clockHz;
  pSttsCtxt->frameDeltaHz = frameDeltaHz;
  pSttsCtxt->trak.pMdhd = &pSttsCtxt->boxMdhd;
  pSttsCtxt->trak.pMdhd->timescale = clockHz;
  pSttsCtxt->trak.pStts = &pSttsCtxt->boxStts;
  pSttsCtxt->trak.pStts->list.clockHz = clockHz;
  pSttsCtxt->trak.pStts->list.totsamplesduration = 0;
  pSttsCtxt->maxStts = 10;

}

static int mkv_updateStts(MKV_EXTRACT_CTXT_COMMON_T *pCommon, const MP4_MDAT_CONTENT_NODE_T *pFrame) {
  int rc = 0;
  STTS_EXTRACT_CTXT_T *pSttsCtxt = &pCommon->sttsCtxt;
  uint64_t curOffsetHz, curOffsetHz2;
  uint64_t playOffsetHz2;

  if(!pSttsCtxt->trak.pStts) {
    return -1;
  }

  //
  // The offset in mkv track units (ms) from the start 
  //
  curOffsetHz = pFrame->playOffsetHz - pSttsCtxt->totOffsetHz;

  //
  // Calculate the offset in media track specific Hz from the start
  // TODO: should smooth this value within a given deviation threshold to prevent "99,99,102,99,99,102,..."
  //
  if(pSttsCtxt->clockHz > 0 && pSttsCtxt->frameDeltaHz > 0) {
    playOffsetHz2 = ((float) pFrame->playOffsetHz / pCommon->clockHz) * pSttsCtxt->clockHz;
    curOffsetHz2 = ((float) (playOffsetHz2 - pSttsCtxt->totOffsetHz2));
  } else {
    curOffsetHz2 = curOffsetHz;
  }

  LOG(X_DEBUG("convert %llu / %u -> %u/%u = %llu, playOffsetHz: %llu, %llu"), curOffsetHz, pCommon->clockHz, pSttsCtxt->clockHz, pSttsCtxt->frameDeltaHz, curOffsetHz2, pFrame->playOffsetHz, playOffsetHz2);

  if(pSttsCtxt->trak.pStts->list.entrycnt == 0 || curOffsetHz != pSttsCtxt->priorOffsetHz) {
    pSttsCtxt->trak.pStts->list.entrycnt++;
  }

  if(pSttsCtxt->trak.pStts->list.entrycnt > pSttsCtxt->allocInStts) {
    //LOG(X_DEBUG("stts maxStts %d -> %d"), pSttsCtxt->maxStts, pSttsCtxt->maxStts * 4);
    if(!(pSttsCtxt->trak.pStts->list.pEntries = (BOX_STTS_ENTRY_T *) avc_recalloc(pSttsCtxt->trak.pStts->list.pEntries,
                                                                 pSttsCtxt->maxStts * sizeof(BOX_STTS_ENTRY_T),
                                                                 pSttsCtxt->allocInStts * sizeof(BOX_STTS_ENTRY_T)))) {
      return -1;
    }
    pSttsCtxt->allocInStts = pSttsCtxt->maxStts;
    pSttsCtxt->maxStts *= 5;
  }

  pSttsCtxt->trak.pStts->list.pEntries[pSttsCtxt->trak.pStts->list.entrycnt - 1].samplecnt++;
  //pSttsCtxt->trak.pStts->list.pEntries[pSttsCtxt->trak.pStts->list.entrycnt - 1].sampledelta = curOffsetHz;
  pSttsCtxt->trak.pStts->list.pEntries[pSttsCtxt->trak.pStts->list.entrycnt - 1].sampledelta = curOffsetHz2;

  pSttsCtxt->priorOffsetHz = curOffsetHz;
  pSttsCtxt->priorOffsetHz2 = curOffsetHz2;
  pSttsCtxt->totOffsetHz = pFrame->playOffsetHz;
  pSttsCtxt->totOffsetHz2 = playOffsetHz2;

  return rc;
}

static int mkv_onReadFrameH264(MKV_EXTRACT_CTXT_T *pExtract, const MP4_MDAT_CONTENT_NODE_T *pFrame) {
  int rc = 0;
  unsigned int clockHz = 0;
  unsigned int frameDeltaHz = 0;
  MKV_CONTAINER_T *pMkv = NULL;
  MKV_EXTRACT_CTXT_VID_T *pV = &pExtract->vid;
  MKV_EXTRACT_CTXT_COMMON_T *pC = &pV->c;

  if(!(pMkv = pExtract->pMkv)) {
    return -1;
  } else if(!pMkv->vid.u.pCfgH264) {
    return -1;
  }

  if(!pC->wroteSeqHdr) {

    pC->wroteSeqHdr = 1;

    if((rc = open_output(pC, "h264")) < 0) {
      return rc;
    }

    if(pC->do_media && (rc = avcc_writeSPSPPS(&pC->fStreamOut, pMkv->vid.u.pCfgH264, 0, 0)) < 0) {
      return -1;
    }
    pV->u.h264.cbSampleData.lenNalSzField = pMkv->vid.u.pCfgH264->lenminusone + 1;
    pV->u.h264.cbSampleData.pStreamOut = &pC->fStreamOut;


    if(pC->do_stts) {
      //
      // Override mkv track timing with clock set in SPS VUI (if any)
      //
      h264_getVUITimingFromSps(pMkv->vid.u.pCfgH264->psps, &clockHz, &frameDeltaHz);
      if(clockHz ==  0) {
        clockHz = pC->clockHz;
        frameDeltaHz = 0;
      } else {
        LOG(X_DEBUG("Using video clock: %d"), clockHz);
      }

      init_stts(&pC->sttsCtxt, clockHz, frameDeltaHz);
    }

  }

  if(pC->do_media && 
     (rc = avcc_cbWriteSample(pFrame->pStreamIn, pFrame->sizeRd, &pV->u.h264.cbSampleData, 0, 0)) < 0) {
    return rc;
  }

  return rc;
}

static int mkv_onReadFrameAAC(MKV_EXTRACT_CTXT_T *pExtract, const MP4_MDAT_CONTENT_NODE_T *pFrame) {
  int rc = 0;
  MKV_CONTAINER_T *pMkv = NULL;
  MKV_EXTRACT_CTXT_COMMON_T *pC = &pExtract->aud.c;

  if(!(pMkv = pExtract->pMkv)) {
    return -1;
  } else if(pMkv->aud.u.aacCfg.objTypeIdx == 0) {
    return -1;
  }

  if(!pExtract->aud.c.wroteSeqHdr) {

    pExtract->aud.c.wroteSeqHdr = 1;

    if((rc = open_output(pC, "aac")) < 0) {
      return rc;
    }

    pExtract->aud.u.aac.cbSampleData.bs.buf = pExtract->aud.u.aac.buf;
    pExtract->aud.u.aac.cbSampleData.bs.sz = sizeof(pExtract->aud.u.aac.buf);
    pExtract->aud.u.aac.cbSampleData.bs.idx = 0;
    pExtract->aud.u.aac.cbSampleData.pStreamOut = &pExtract->aud.c.fStreamOut;
    memcpy(&pExtract->aud.u.aac.cbSampleData.decoderCfg, &pMkv->aud.u.aacCfg, sizeof(ESDS_DECODER_CFG_T));

    if(pExtract->aud.c.do_stts) {
      init_stts(&pExtract->aud.c.sttsCtxt, pExtract->aud.c.clockHz, 0);
    }
  }

  if(pExtract->aud.c.do_media) {
    rc = esds_cbWriteSample(pFrame->pStreamIn, pFrame->sizeRd, &pExtract->aud.u.aac.cbSampleData, 0, 0);
  }

  return rc;
}

int mkv_onReadFrame(void *pArg, const MP4_MDAT_CONTENT_NODE_T *pFrame) {
  MKV_EXTRACT_CTXT_T *pExtract = (MKV_EXTRACT_CTXT_T *) pArg;
  MKV_CONTAINER_T *pMkv = NULL;
  int rc = 0;

  if(!pExtract || !pFrame || !(pMkv = pExtract->pMkv)) {
    return -1;
  } else if(!(pExtract->vid.c.do_media || pExtract->vid.c.do_stts) && pFrame->isvid) {
    return 0;
  } else if(!(pExtract->aud.c.do_media || pExtract->aud.c.do_stts) && !pFrame->isvid) {
    return 0;
  }

  //LOG(X_DEBUG("mkv_onReadFrame: (v:%d,%d) fr[%d], isvid: %d, size:%d, frame.offsetHz:%lldHz, flags: 0x%x, fileOffset: %lld"), pExtract->vid.c.do_media, pExtract->vid.c.do_stts, pFrame->frameId, pFrame->isvid, pFrame->sizeRd, pFrame->playOffsetHz, pFrame->flags, pFrame->fileOffset);

  if(SeekMediaFile(pFrame->pStreamIn, pFrame->fileOffset, SEEK_SET) != 0) {
    return -1;
  }

  if(pFrame->isvid && pMkv->vid.pTrack) {

    switch(pMkv->vid.pTrack->codecType) {
      case XC_CODEC_TYPE_H264:
        if((rc = mkv_onReadFrameH264(pExtract, pFrame)) < 0) {
          LOG(X_ERROR("Failed to process video H.264 frame"));
        }
        break;
      default:
        LOG(X_DEBUG("MKV extract of %s video not supported"), codecType_getCodecDescrStr(pMkv->vid.pTrack->codecType));
        return -1;
    }

    if(rc >= 0 && pExtract->vid.c.do_stts && (rc = mkv_updateStts(&pExtract->vid.c, pFrame)) < 0) {
      LOG(X_ERROR("Failed to update video stts"));
    }

  } else if(!pFrame->isvid && pMkv->aud.pTrack) {

    switch(pMkv->aud.pTrack->codecType) {
      case XC_CODEC_TYPE_AAC:
        if((rc = mkv_onReadFrameAAC(pExtract, pFrame)) < 0) {
          LOG(X_ERROR("Failed to process audio AAC frame"));
        }
        break;
      default:
        LOG(X_DEBUG("MKV extract of %s audio not supported"), codecType_getCodecDescrStr(pMkv->vid.pTrack->codecType));
        return -1;
    }

    if(rc >= 0 && pExtract->aud.c.do_stts && (rc = mkv_updateStts(&pExtract->aud.c, pFrame)) < 0) {
      LOG(X_ERROR("Failed to update audio stts"));
    }
  }

 
  return rc;
}

int mkv_extractMedia(MKV_CONTAINER_T *pMkv, int do_vid, int do_aud, int do_vidstts, int do_audstts,
                     const char *outpath, uint64_t startHz, uint64_t durationHz) {
  int rc = 0;
  MKV_EXTRACT_CTXT_T extractCtxt;
  char path[VSX_MAX_PATH_LEN];

  if(!pMkv || !pMkv->pStream || pMkv->pStream->fp == FILEOPS_INVALID_FP) {
    return -1;
  } else if((do_vid|| do_vidstts) && !(do_aud || do_audstts) && !pMkv->vid.pTrack) {
    LOG(X_ERROR("No video track to extract"));
    return -1;
  } else if((do_aud || do_audstts) && !(do_vid || do_vidstts) && !pMkv->aud.pTrack) {
    LOG(X_ERROR("No audio track to extract"));
    return -1;
  }

  memset(&extractCtxt, 0, sizeof(extractCtxt));
  extractCtxt.vid.c.fStreamOut.fp = extractCtxt.aud.c.fStreamOut.fp = FILEOPS_INVALID_FP;
  extractCtxt.pMkv = pMkv;

  if((do_vid || do_vidstts) && pMkv->vid.pTrack) {

    if(do_vid) {
      extractCtxt.vid.c.do_media = 1;
    }
    if(do_vidstts) {
      extractCtxt.vid.c.do_stts = 1;
    }

    extractCtxt.vid.c.pathprefix = outpath; 

    if((extractCtxt.vid.c.clockHz = pMkv->vid.pTrack->timeCodeScale) == 1) {
      extractCtxt.vid.c.clockHz *= 1000;
    }

  } 

  if((do_aud || do_audstts) && pMkv->aud.pTrack) {

    if(do_aud) {
      extractCtxt.aud.c.do_media = 1;
    }
    if(do_audstts) {
      extractCtxt.aud.c.do_stts = 1;
    }

    extractCtxt.aud.c.pathprefix = outpath; 

    if((extractCtxt.aud.c.clockHz = pMkv->aud.pTrack->timeCodeScale) == 1) {
      extractCtxt.aud.c.clockHz *= 1000;
    }

  }

  if(rc >= 0) {
    rc = mkv_loadFrames(pMkv, &extractCtxt, mkv_onReadFrame);
  }

  if(extractCtxt.vid.c.sttsCtxt.trak.pStts && extractCtxt.vid.c.sttsCtxt.trak.pStts->list.entrycnt > 0) {
    if((rc = snprintf(path, sizeof(path), "%s.%s", outpath, "sttsv")) >= 0) {
      mp4_dumpStts(path, &extractCtxt.vid.c.sttsCtxt.trak, pMkv->pStream->filename, NULL);
      mp4_free_avsync_stts(&extractCtxt.vid.c.sttsCtxt.boxStts.list);
    }
  }

  if(extractCtxt.aud.c.sttsCtxt.trak.pStts && extractCtxt.aud.c.sttsCtxt.trak.pStts->list.entrycnt > 0) {
    if((rc = snprintf(path, sizeof(path), "%s.%s", outpath, "sttsa")) >= 0) {
      mp4_dumpStts(path, &extractCtxt.aud.c.sttsCtxt.trak, pMkv->pStream->filename, NULL);
      mp4_free_avsync_stts(&extractCtxt.aud.c.sttsCtxt.boxStts.list);
    }
  }

  if(extractCtxt.vid.c.fStreamOut.fp != FILEOPS_INVALID_FP) {
    fileops_Close(extractCtxt.vid.c.fStreamOut.fp);
  }
  if(extractCtxt.aud.c.fStreamOut.fp != FILEOPS_INVALID_FP) {
    fileops_Close(extractCtxt.aud.c.fStreamOut.fp);
  }

  return rc;
}
