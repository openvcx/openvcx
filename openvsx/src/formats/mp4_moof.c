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
#include "mp4creator_int.h"

#if defined(VSX_CREATE_CONTAINER)

#define MOOF_TRACKID_VID   1
#define MOOF_TRACKID_AUD   (MOOF_TRACKID_VID + 1)

#define MOOF_AV_IDX_VID    0
#define MOOF_AV_IDX_AUD    1
#define MP4IDX(isvid) ((isvid) ? MOOF_AV_IDX_VID : MOOF_AV_IDX_AUD)

typedef struct MPD_UDPATE_CTXT {
  int                 no_init_state;
  int                 did_update;
  unsigned int        mediaSequenceIndex;
  int                 mediaSequenceMin;
  char                uniqIdBuf[128];
  MPD_MEDIA_FILE_T    media;
} MPD_UPDATE_CTXT_T;



static BOX_T *moof_create_stsd_child_h264(const BOX_HANDLER_T *pHandlerChain, CODEC_VID_CTXT_T *pVidCtxt) {
  int rc = 0;
  int init_avcc = 0;
  BOX_AVCC_T *pAvcC = NULL;
  BOX_AVC1_T *pAvc1 = NULL;

  //fprintf(stderr, "AVCC_INITCFG FROM MOOF_CREATE_STSD_CHILD_H264\n");
  //TODO: this is not needed anymore if srvflv::flvsrv_init_vid_h264 calls avcc_initCfg
  if(!pVidCtxt->codecCtxt.h264.avcc.psps) {
    //fprintf(stderr, "AVCC_INITCFG FROM MOOF_CREATE_STSD_CHILD_H264 INSIDE\n");
    if((rc = avcc_initCfg(pVidCtxt->codecCtxt.h264.avccRaw, pVidCtxt->codecCtxt.h264.avccRawLen, 
               &pVidCtxt->codecCtxt.h264.avcc)) < 0 ||
        !pVidCtxt->codecCtxt.h264.avcc.psps || !pVidCtxt->codecCtxt.h264.avcc.ppps) {
      return NULL;
    }
    init_avcc = 1;
  }
 
  if((pAvc1 = mp4_create_box_avc1(pHandlerChain, 1, pVidCtxt->ctxt.width, pVidCtxt->ctxt.height))) {
    pAvcC = (BOX_AVCC_T *) mp4_create_box_avcC(pHandlerChain,
           pVidCtxt->codecCtxt.h264.avcc.psps->data , pVidCtxt->codecCtxt.h264.avcc.psps->len, 
           pVidCtxt->codecCtxt.h264.avcc.ppps->data , pVidCtxt->codecCtxt.h264.avcc.ppps->len);
    pAvc1->box.child = (BOX_T *) pAvcC;
  }

  //avc_dumpHex(stderr, pVidCtxt->codecCtxt.h264.avccRaw, pVidCtxt->codecCtxt.h264.avccRawLen, 1);
  //if(pVidCtxt->codecCtxt.h264.avcc.psps) avc_dumpHex(stderr, pVidCtxt->codecCtxt.h264.avcc.psps->data, pVidCtxt->codecCtxt.h264.avcc.psps->len, 1);
  //if(pVidCtxt->codecCtxt.h264.avcc.ppps) avc_dumpHex(stderr, pVidCtxt->codecCtxt.h264.avcc.ppps->data, pVidCtxt->codecCtxt.h264.avcc.ppps->len, 1);

  //pTrak->lenSamplesHdr = pAvcC->avcc.lenminusone+1;

  if(init_avcc) {
    avcc_freeCfg(&pVidCtxt->codecCtxt.h264.avcc);
  }

  if(!pAvc1 || !pAvcC) {
    mp4_freeBoxes((BOX_T *) pAvc1); 
    pAvc1 = NULL;
  }

  return (BOX_T *) pAvc1;
}

static BOX_T *moof_create_stsd_child_aac(const BOX_HANDLER_T *pHandlerChain, 
                                         CODEC_AUD_CTXT_T *pAudCtxt,
                                         uint32_t *pstsd_timescale) {
  //double durationSec = 0;
  BOX_ESDS_DESCR_DECODERSPECIFIC_T decoderSp;
  BOX_MP4A_T *pBoxMp4a = NULL;
  BOX_ESDS_T *pBoxEsds = NULL; 

  if((pBoxMp4a = mp4_create_box_mp4a(pHandlerChain, &pAudCtxt->codecCtxt.aac.adtsFrame.sp))) {

    if(pstsd_timescale) {
      *pstsd_timescale = ntohl(pBoxMp4a->timescale) >> 16;
    }

    mp4_create_box_esds_decoderspecific(&decoderSp, &pAudCtxt->codecCtxt.aac.adtsFrame.sp);

    //durationSec =   ((double)pAac->lastFrameId * pAac->decoderSP.frameDeltaHz /
    //                (double)pAac->clockHz);

    pBoxEsds = (BOX_ESDS_T *) mp4_create_box_esds(pHandlerChain,
                                 ESDS_OBJTYPE_MP4A,
                                 ESDS_STREAMTYPE_AUDIO,
                                 0, //(uint32_t) ((double)pAac->szTot / durationSec * 8.0),
                                 0, // pAac->peakBwBytes * 8,
                                 0, // pAac->szLargestFrame,
                                 &decoderSp);

    pBoxMp4a->box.child = (BOX_T *) pBoxEsds;
  }

  if(!pBoxMp4a || !pBoxEsds) {
    mp4_freeBoxes((BOX_T *) pBoxMp4a); 
    pBoxMp4a = NULL;
  }

  return (BOX_T *) pBoxMp4a;

}

static BOX_TREX_T *moof_create_trex(const BOX_HANDLER_T *pHandlerChain, uint32_t trackId) {
  BOX_TREX_T *pBoxTrex = NULL;

  pBoxTrex = (BOX_TREX_T *) avc_calloc(1, sizeof(BOX_TREX_T));

  pBoxTrex->trackid = trackId;
  pBoxTrex->default_sample_description_index = 1;
  pBoxTrex->default_sample_duration = 0;
  pBoxTrex->default_sample_size = 0;
  pBoxTrex->default_sample_flags = 0;

  pBoxTrex->fullbox.flags[2] = 0x00;

  pBoxTrex->box.type = *((uint32_t *) "trex");
  memcpy(pBoxTrex->box.name, &pBoxTrex->box.type, 4);
  pBoxTrex->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxTrex->box.type);
  pBoxTrex->box.szhdr = 8;
  pBoxTrex->box.szdata_ownbox = 24;
  pBoxTrex->box.szdata = pBoxTrex->box.szdata_ownbox;

  return pBoxTrex;
}

static BOX_MVEX_T *moof_create_mvex(const BOX_HANDLER_T *pHandlerChain, XC_CODEC_TYPE_T vidCodecType, 
                                    XC_CODEC_TYPE_T audCodecType, uint32_t startTrackId) {
  BOX_MVEX_T *pBoxMvex;
  BOX_T *pBoxChild = NULL;

  pBoxMvex = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "mvex") );

  if(codectype_isVid(vidCodecType)) {
    pBoxMvex->child = pBoxChild = (BOX_T *) moof_create_trex(pHandlerChain, startTrackId);
  }

  if(codectype_isAud(audCodecType)) {
    pBoxChild = (BOX_T *) moof_create_trex(pHandlerChain, pBoxChild ? startTrackId + 1 : startTrackId);
    if(pBoxMvex->child) {
      pBoxMvex->child->pnext = pBoxChild;
    } else {
      pBoxMvex->child = pBoxChild;
    }
  }

  return pBoxMvex;
}

static BOX_TFHD_T *moof_create_tfhd(const BOX_HANDLER_T *pHandlerChain, uint32_t trackId) {
  BOX_TFHD_T *pBoxTfhd = NULL;
  unsigned int size = 4;

  pBoxTfhd = (BOX_TFHD_T *) avc_calloc(1, sizeof(BOX_TFHD_T));

  pBoxTfhd->trackid = trackId;
  pBoxTfhd->flags = 0;
  //pBoxTfhd->flags |= TFHD_FLAG_BASE_DATA_OFFSET;
  //pBoxTfhd->base_data_offset = 0; // If this is set to non-zero, it should be set after the TRUN box has
                                  // been completed
  //size += 4;
  pBoxTfhd->sample_description_index = 0;
  pBoxTfhd->default_sample_duration = 0;
  pBoxTfhd->default_sample_size = 0;

  pBoxTfhd->default_sample_flags  = 0;
  //size += 4;

  pBoxTfhd->fullbox.flags[2] = pBoxTfhd->flags;

  pBoxTfhd->box.type = *((uint32_t *) "tfhd");
  memcpy(pBoxTfhd->box.name, &pBoxTfhd->box.type, 4);
  pBoxTfhd->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxTfhd->box.type);
  pBoxTfhd->box.szhdr = 8;
  pBoxTfhd->box.szdata_ownbox = 4 + size;
  pBoxTfhd->box.szdata = pBoxTfhd->box.szdata_ownbox;

  return pBoxTfhd;
}

static BOX_TFDT_T *moof_create_tfdt(const BOX_HANDLER_T *pHandlerChain, uint32_t trackId, 
                                    uint64_t baseMediaDecodeTime) {
  BOX_TFDT_T *pBoxTfdt = NULL;
  unsigned int size = 4;

  pBoxTfdt = (BOX_TFDT_T *) avc_calloc(1, sizeof(BOX_TFDT_T));

  pBoxTfdt->fullbox.version = 0x01;
  size += 4;
  pBoxTfdt->baseMediaDecodeTime = baseMediaDecodeTime;

  pBoxTfdt->box.type = *((uint32_t *) "tfdt");
  memcpy(pBoxTfdt->box.name, &pBoxTfdt->box.type, 4);
  pBoxTfdt->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxTfdt->box.type);
  pBoxTfdt->box.szhdr = 8;
  pBoxTfdt->box.szdata_ownbox = 4 + size;
  pBoxTfdt->box.szdata = pBoxTfdt->box.szdata_ownbox;

  return pBoxTfdt;
}

static BOX_TRUN_T *moof_create_trun(const BOX_HANDLER_T *pHandlerChain, 
                                    const CODEC_AV_CTXT_T *pAvCtxt,
                                    const MP4_TRAK_T *pTrak,
                                    int isvid) {
  BOX_TRUN_T *pBoxTrun = NULL;
  unsigned int size = 4;
  BOX_AVCC_T *pBoxAvcC = NULL;

  pBoxTrun = (BOX_TRUN_T *) avc_calloc(1, sizeof(BOX_TRUN_T));

  pBoxTrun->flags |= TRUN_FLAG_DATA_OFFSET;
  size += 4;

  pBoxTrun->flags |= TRUN_FLAG_SAMPLE_DURATION;
  pBoxTrun->sample_size += 4;

  pBoxTrun->flags |= TRUN_FLAG_SAMPLE_SIZE;
  pBoxTrun->sample_size += 4;

  if(isvid && codectype_isVid(pAvCtxt->vid.codecType)) {

    //
    // Check for presence of B frames and add composition time offset field
    //
    if(pAvCtxt->vid.codecType == XC_CODEC_TYPE_H264 &&
       (pBoxAvcC = (BOX_AVCC_T *) mp4_findBoxChild((BOX_T *) pTrak->pStsd, *((uint32_t *) "avcC")))) {
      switch(pBoxAvcC->avcc.avcprofile) {
        case H264_PROFILE_BASELINE:
          break;
        default:
          pBoxTrun->flags |= TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET;
          pBoxTrun->sample_size += 4;
          break;
      }
    }

  } else if(!isvid && codectype_isAud(pAvCtxt->aud.codecType)) {

  }

  pBoxTrun->data_offset = 0; // The actual offset value will be set when the TRUN box has been completed

  pBoxTrun->fullbox.flags[2] = pBoxTrun->flags & 0xff;
  pBoxTrun->fullbox.flags[1] = (pBoxTrun->flags >> 8) & 0xff;

  pBoxTrun->box.type = *((uint32_t *) "trun");
  memcpy(pBoxTrun->box.name, &pBoxTrun->box.type, 4);
  pBoxTrun->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxTrun->box.type);
  pBoxTrun->box.szhdr = 8;
  pBoxTrun->box.szdata_ownbox = 4 + size + (pBoxTrun->sample_count * pBoxTrun->sample_size);
  pBoxTrun->box.szdata = pBoxTrun->box.szdata_ownbox;

  return pBoxTrun;
}

static unsigned char *moof_frame_aac(CODEC_AV_CTXT_T *pAvCtxt,
                                     const OUTFMT_FRAME_DATA_T *pFrame, 
                                     unsigned int *pDataLength) {

  unsigned int dataOffset = 0;
  unsigned char *pData = OUTFMT_DATA(pFrame);

  //
  // We don't need the ADTS 7 byte aac header
  //
  dataOffset = MIN(OUTFMT_LEN(pFrame), 7);
  pData += dataOffset;
  *pDataLength = (OUTFMT_LEN(pFrame) - dataOffset);

  return pData;
}

static unsigned char *moof_frame_h264(CODEC_AV_CTXT_T *pAvCtxt,
                                      const OUTFMT_FRAME_DATA_T *pFrame, 
                                      unsigned int *pDataLength) {
  int rc = 0;
  unsigned char *pData = NULL;

  //
  // Handle conversion from H.264 annexB to AVC format and NAL filtering
  //
  if((rc = flvsrv_create_vidframe_h264(NULL, &pAvCtxt->vid, pFrame,
                                       NULL, NULL, 0)) < 0) {
    return NULL;
  }

  pData = &pAvCtxt->vid.tmpFrame.buf[pAvCtxt->vid.tmpFrame.idx];
  *pDataLength = rc;

  return pData;
}

static int moof_addto_trun(MOOF_STATE_INT_T *pCtxtInt, 
                           const OUTFMT_FRAME_DATA_T *pFrame) {

  BOX_TRUN_T *pBoxTrun = NULL;
  BOX_TRUN_ENTRY_T *pSamples;
  unsigned int alloc_count;
  BOX_TRUN_ENTRY_T trunEntry;
  unsigned int dataLength = 0;
  unsigned char *pData = NULL;
  uint64_t ptsDuration = 0;
  int rc = 0;

  if(!(pBoxTrun = pCtxtInt->trak.moofTrak.pTrun)) {
    LOG(X_ERROR("MOOF add to TRUN box %s moofTrack.pTrun is null!"), pCtxtInt->isvid ? "video" : "audio");
    return -1;
  }

#define TRUN_ENTRY_ALLOC_INCREMENT   80

  //
  // Allocate or increment pSamples TRUN entries
  //
  if(pBoxTrun->sample_count >= pBoxTrun->alloc_count) {
    alloc_count = pBoxTrun->alloc_count + TRUN_ENTRY_ALLOC_INCREMENT;

    if((pSamples = (BOX_TRUN_ENTRY_T*) avc_calloc(alloc_count, sizeof(BOX_TRUN_ENTRY_T))) == NULL) {
      LOG(X_CRITICAL("Failed to allocate %d bytes for trun samples block"), alloc_count);
      return -1;
    }

    if(pBoxTrun->sample_count > 0 && pBoxTrun->pSamples) {

      memcpy(pSamples, pBoxTrun->pSamples, sizeof(BOX_TRUN_ENTRY_T) * pBoxTrun->sample_count);
      free(pBoxTrun->pSamples);
      if(pCtxtInt->pTrunEntryPrior) {
        pCtxtInt->pTrunEntryPrior = &pSamples[pBoxTrun->sample_count];
      }
    }
    pBoxTrun->pSamples = pSamples;
    pBoxTrun->alloc_count = alloc_count;

  }

  pData = OUTFMT_DATA(pFrame);
  dataLength = OUTFMT_LEN(pFrame);
  ptsDuration = (int64_t) OUTFMT_PTS(pFrame) - (int64_t) pCtxtInt->ptsElapsed;
  pCtxtInt->ptsElapsed += ptsDuration;

  //
  // Handle codec specific data formatting and conversion
  //
  if(pFrame->isvid) {

    if(pCtxtInt->pAvCtxt->vid.codecType == XC_CODEC_TYPE_H264) {
      pData = moof_frame_h264(pCtxtInt->pAvCtxt, pFrame, &dataLength);
    } 

  } else if(pFrame->isaud) {

    if(pCtxtInt->pAvCtxt->aud.codecType == XC_CODEC_TYPE_AAC) {
      pData = moof_frame_aac(pCtxtInt->pAvCtxt, pFrame, &dataLength);
    } 

    //
    //TODO: we should be ok as long as pts of first frame always starts at 0
    //

  }

  //
  // Create the TRUN box sample entry
  //
  trunEntry.sample_duration = (uint32_t) ((float) ptsDuration * pCtxtInt->trak.pMdhd->timescale / PTS_HZ_SECF);
  trunEntry.sample_size = dataLength;
  trunEntry.sample_flags = 0;
  trunEntry.sample_composition_time_offset = 0;

  VSX_DEBUG_DASH( LOG(X_DEBUGV("DASH - moof_addto_trun %s frame-len:%d (%d) pts: %.3f, ptsElapsedV:%.3f, "
                               "ptsDuration:%.3f (%lld) (%uHz)"), 
                    pFrame->isvid ? "video" : "audio", OUTFMT_LEN(pFrame), dataLength, 
                    PTSF(OUTFMT_PTS(pFrame)), PTSF(pCtxtInt->ptsElapsed), PTSF(ptsDuration), 
                    ptsDuration, trunEntry.sample_duration) );

  //
  // Set the sample duration of the prior sample now that the time of the subsequent (current) sample is known
  //
  if(pCtxtInt->pTrunEntryPrior) {
    pCtxtInt->pTrunEntryPrior->sample_duration = trunEntry.sample_duration;
  }

  pCtxtInt->pTrunEntryPrior = &pBoxTrun->pSamples[pBoxTrun->sample_count];
  memcpy(&pBoxTrun->pSamples[pBoxTrun->sample_count], &trunEntry, sizeof(BOX_TRUN_ENTRY_T));

  pBoxTrun->sample_count++;
  pBoxTrun->box.szdata_ownbox += pBoxTrun->sample_size;
  pBoxTrun->box.szdata = pBoxTrun->box.szdata_ownbox;

  //
  // Write the frame content to the (temporary) MDAT box
  //
  rc = WriteFileStream(&pCtxtInt->mdat.stream, pData, trunEntry.sample_size); 

  return rc;
}

static BOX_STTS_T *moof_create_box_stts(const BOX_HANDLER_T *pHandlerChain) {
  BOX_STTS_T *pBoxStts;

  pBoxStts = (BOX_STTS_T *) avc_calloc(1, sizeof(BOX_STTS_T));

  pBoxStts->list.entrycnt = 0;

  pBoxStts->fullbox.flags[2] = 0x00;

  pBoxStts->box.type = *((uint32_t *) "stts");
  memcpy(pBoxStts->box.name, &pBoxStts->box.type, 4);
  pBoxStts->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxStts->box.type);
  pBoxStts->box.szhdr = 8;
  pBoxStts->box.szdata_ownbox = 8 + (pBoxStts->list.entrycnt * 8);
  pBoxStts->box.szdata = pBoxStts->box.szdata_ownbox;

  return pBoxStts;
}

static BOX_MFHD_T *moof_create_box_mfhd(const BOX_HANDLER_T *pHandlerChain) {
  BOX_MFHD_T *pBoxMfhd;

  pBoxMfhd = (BOX_MFHD_T *) avc_calloc(1, sizeof(BOX_MFHD_T));

  pBoxMfhd->sequence = 0;

  pBoxMfhd->fullbox.flags[2] = 0x00;

  pBoxMfhd->box.type = *((uint32_t *) "mfhd");
  memcpy(pBoxMfhd->box.name, &pBoxMfhd->box.type, 4);
  pBoxMfhd->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxMfhd->box.type);
  pBoxMfhd->box.szhdr = 8;
  pBoxMfhd->box.szdata_ownbox = 8;
  pBoxMfhd->box.szdata = pBoxMfhd->box.szdata_ownbox;

  return pBoxMfhd;
}

static BOX_T *moof_create_track(const BOX_HANDLER_T *pHandlerChain,
                               MOOF_STATE_INT_T *pCtxtInt,
                               uint64_t mdhd_timescale) {
  uint32_t hdlr_name = 0;
  MP4_TRAK_T *pTrak = NULL;
  BOX_T *pBox = NULL;
  uint32_t stsd_timescale = (uint32_t) mdhd_timescale;
  char tmp[128];

  pTrak = &pCtxtInt->trak;
  pTrak->pTrak = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "trak") );
  if(pCtxtInt->isvid) {
    pTrak->pTkhd = mp4_create_box_tkhd(pHandlerChain, 0, MOOF_TRACKID_VID,
                                 pCtxtInt->pAvCtxt->vid.ctxt.width, pCtxtInt->pAvCtxt->vid.ctxt.height, 0, 0, 0);
    hdlr_name = *((uint32_t *) "vide");
  } else {
    pTrak->pTkhd = mp4_create_box_tkhd(pHandlerChain, 0, MOOF_TRACKID_AUD, 0, 0, 0, 0, 0x0100);
    hdlr_name = *((uint32_t *) "soun");
  }
  pTrak->pTkhd->creattime = pCtxtInt->pMvhd->creattime;
  pTrak->pTkhd->modificationtime = pCtxtInt->pMvhd->modificationtime;

  pTrak->pTrak->child = (BOX_T *) pTrak->pTkhd;
  pTrak->pTkhd->box.pnext = pTrak->pMdia = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "mdia") );

  pTrak->pMdhd = mp4_create_box_mdhd(pHandlerChain, 0, mdhd_timescale);
  pTrak->pMdhd->creattime = pCtxtInt->pMvhd->creattime;
  pTrak->pMdhd->modificationtime = pCtxtInt->pMvhd->modificationtime;

  pTrak->pMdia->child = (BOX_T *) pTrak->pMdhd;
  pTrak->pHdlr = mp4_create_box_hdlr(pHandlerChain, hdlr_name, vsxlib_get_appnamestr(tmp, sizeof(tmp)));
  pTrak->pMdhd->box.pnext = (BOX_T *) pTrak->pHdlr;
  pTrak->pHdlr->box.pnext = pTrak->pMinf = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "minf") );

  if(pCtxtInt->isvid) {
    pTrak->pVmhd = mp4_create_box_vmhd(pHandlerChain);
    pBox = (BOX_T *) pTrak->pVmhd;
  } else {
    pTrak->pSmhd = mp4_create_box_smhd(pHandlerChain);
    pBox = (BOX_T *) pTrak->pSmhd;
  }

  pTrak->pMinf->child = pBox;
  pBox->pnext = pTrak->pDinf = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "dinf") );
  pTrak->pDref = mp4_create_box_dref(pHandlerChain, mp4_create_box_dref_url(pHandlerChain, NULL));
  pTrak->pDinf->child = (BOX_T *) pTrak->pDref;
  pTrak->pDinf->pnext = pTrak->pStbl = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "stbl") );
  pTrak->pStsd = mp4_create_box_stsd(pHandlerChain);
  pTrak->pStbl->child = (BOX_T *) pTrak->pStsd;

  if(pCtxtInt->isvid) {
    switch(pCtxtInt->pAvCtxt->vid.codecType) {
      case XC_CODEC_TYPE_H264:
        pTrak->pStsd->box.child = moof_create_stsd_child_h264(pHandlerChain, &pCtxtInt->pAvCtxt->vid);
        break;
      default:
        LOG(X_ERROR("Unsupported mp4 moof video codec %s"), 
                     codecType_getCodecDescrStr(pCtxtInt->pAvCtxt->vid.codecType));   
        break;
    }

  } else {
    switch(pCtxtInt->pAvCtxt->aud.codecType) {
      case XC_CODEC_TYPE_AAC:
        pTrak->pStsd->box.child = moof_create_stsd_child_aac(pHandlerChain, &pCtxtInt->pAvCtxt->aud, 
                                                             &stsd_timescale);
        break;
      default:
        LOG(X_ERROR("Unsupported mp4 moof audio codec %s"), 
                    codecType_getCodecDescrStr(pCtxtInt->pAvCtxt->aud.codecType));   
        break;
    }

    //
    // The audio sampling rate is extracted here
    //
    if(stsd_timescale > 0) {
      pTrak->pMdhd->timescale = stsd_timescale;
    }

  }

  if(!pTrak->pStsd->box.child) {
    mp4_freeBoxes(pTrak->pTrak); 
    pTrak->pTrak = NULL;
    return NULL;
  }

  pTrak->pStts = moof_create_box_stts(pHandlerChain);
  pTrak->pStsd->box.pnext = (BOX_T *) pTrak->pStts;

  pTrak->pStsc = (BOX_STSC_T *) mp4_create_box_stsc(pHandlerChain, 0, NULL);
  pTrak->pStts->box.pnext = (BOX_T *) pTrak->pStsc;

  pTrak->pStco = (BOX_STCO_T *) mp4_create_box_stco(pHandlerChain, 0, NULL);
  pTrak->pStsc->box.pnext = (BOX_T *) pTrak->pStco;

  pTrak->pStsz = (BOX_STSZ_T *) mp4_create_box_stsz(pHandlerChain, 0, NULL);
  pTrak->pStco->box.pnext = (BOX_T *) pTrak->pStsz;

  pTrak->pStss = (BOX_STSS_T *) mp4_create_box_stss(pHandlerChain, 0, NULL);
  pTrak->pStsz->box.pnext = (BOX_T *) pTrak->pStss;

  return (BOX_T *) pTrak->pTrak;
}

static BOX_SIDX_T *moof_create_sidx(const BOX_HANDLER_T *pHandlerChain, 
                                    int isaud,
                                    uint32_t timescale,
                                    uint32_t duration, 
                                    uint32_t size,
                                    uint64_t earliest_presentation_time) {
  BOX_SIDX_T *pBoxSidx = NULL;
  unsigned int idx;
  int SAPType = isaud ? 1 : 0;
  int startsWithSAP = 1;

  pBoxSidx = (BOX_SIDX_T *) avc_calloc(1, sizeof(BOX_SIDX_T));

  pBoxSidx->reference_id = 1;
  pBoxSidx->timescale = timescale;
  pBoxSidx->earliest_presentation_time = earliest_presentation_time;
  pBoxSidx->first_offset = 0;
  pBoxSidx->reference_count = 1;

  if(pBoxSidx->reference_count > 0) { 
    pBoxSidx->pSamples = (BOX_SIDX_ENTRY_T *) avc_calloc(pBoxSidx->reference_count, sizeof(BOX_SIDX_ENTRY_T));

    for(idx = 0; idx < pBoxSidx->reference_count; idx++) {
      pBoxSidx->pSamples[idx].referenced_size = (0 << 31) | (SIDX_MASK_REF_SIZE & size);
      pBoxSidx->pSamples[idx].subsegment_duration = duration;
      pBoxSidx->pSamples[idx].sap = (startsWithSAP << 31) | (SAPType << 28) | 0;
    }
  }

  pBoxSidx->fullbox.version = 0x01;
  pBoxSidx->box.type = *((uint32_t *) "sidx");
  memcpy(pBoxSidx->box.name, &pBoxSidx->box.type, 4);
  pBoxSidx->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxSidx->box.type);
  pBoxSidx->box.szhdr = 8;
  pBoxSidx->box.szdata_ownbox = 24 + (pBoxSidx->fullbox.version ? 8 : 0) + 
                                (pBoxSidx->reference_count * sizeof(BOX_SIDX_ENTRY_T));
  pBoxSidx->box.szdata = pBoxSidx->box.szdata_ownbox;

  return pBoxSidx;
}

static void closeMoofTrak(MP4_TRAK_T *pTrak) {

  VSX_DEBUG_DASH(LOG(X_DEBUGV("DASH - closeMoofTrak...")));

  if(pTrak->moofTrak.pMoof) {
    mp4_freeBoxes((BOX_T *)  pTrak->moofTrak.pMoof);
    pTrak->moofTrak.pMoof = NULL;
  }
  memset(pTrak, 0, sizeof(MP4_TRAK_T));
}

static BOX_MOOF_T *moof_createMoof(const BOX_HANDLER_T *pHandlerChain,
                                   const CODEC_AV_CTXT_T *pAvCtxt,
                                   MP4_TRAK_T *pTrak,
                                   int isvid,
                                   uint64_t tfdt_baseMediaDecodeTime) {

  if(!pHandlerChain || !pAvCtxt || !pTrak) {
    return NULL;
  }

  VSX_DEBUG_DASH(LOG(X_DEBUG("DASH - moof_createMoof "), isvid ? "video" : "audio"));

  pTrak->moofTrak.pMoof = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "moof") );
  pTrak->moofTrak.pMfhd = moof_create_box_mfhd(pHandlerChain);
  pTrak->moofTrak.pMoof->child = (BOX_T *) pTrak->moofTrak.pMfhd;
  pTrak->moofTrak.pTraf =  mp4_create_box_generic(pHandlerChain, *((uint32_t *) "traf") );
  pTrak->moofTrak.pMfhd->box.pnext = (BOX_T *) pTrak->moofTrak.pTraf;
  pTrak->moofTrak.pTfhd = moof_create_tfhd(pHandlerChain, pTrak->pTkhd->trackid);
  pTrak->moofTrak.pTraf->child = (BOX_T *) pTrak->moofTrak.pTfhd;
  pTrak->moofTrak.pTfdt = moof_create_tfdt(pHandlerChain, pTrak->pTkhd->trackid, tfdt_baseMediaDecodeTime);
  pTrak->moofTrak.pTfhd->box.pnext = (BOX_T *) pTrak->moofTrak.pTfdt;
  pTrak->moofTrak.pTrun = moof_create_trun(pHandlerChain, pAvCtxt, pTrak, isvid);
  pTrak->moofTrak.pTfdt->box.pnext = (BOX_T *) pTrak->moofTrak.pTrun;

  return pTrak->moofTrak.pMoof;
}

static MP4_CONTAINER_T *moof_createEmptyMp4Isom(MOOF_STATE_INT_T *pCtxtIntV, 
                                                MOOF_STATE_INT_T *pCtxtIntA,
                                                uint32_t startTrackId) {
  BOX_T *pBoxMoov = NULL;
  BOX_T *pVTrak = NULL;
  BOX_T *pATrak = NULL;
  BOX_T *pBoxMvex = NULL;
  BOX_T *pBox = NULL;
  MP4_CONTAINER_T *pMp4 = NULL;
  MOOF_INIT_CTXT_T *pMoofInitCtxt = NULL;
  XC_CODEC_TYPE_T codecTypeV = XC_CODEC_TYPE_UNKNOWN;
  XC_CODEC_TYPE_T codecTypeA = XC_CODEC_TYPE_UNKNOWN;
  struct timeval tv;
  uint64_t mvhd_timescale;
  uint64_t mdhd_timescale;

  if(!pCtxtIntV && !pCtxtIntA) {
    return NULL;
  }

  pMoofInitCtxt = pCtxtIntV ? pCtxtIntV->pMoofInitCtxt : pCtxtIntA->pMoofInitCtxt;

  if((mdhd_timescale = mvhd_timescale = pMoofInitCtxt->clockHz) == 0) {
    LOG(X_ERROR("moof mvhd clock rate not set"));
    return NULL;
  }

  if((pMp4 = mp4_createContainer()) == NULL) {
    return NULL;
  }

  //
  // If creating distinct audio and video m4s files, use the original audio timescale for all references in the m4s
  //
  if(pCtxtIntA && !pCtxtIntV && pCtxtIntA->pAvCtxt->aud.ctxt.clockHz != 0) {
    mdhd_timescale = pCtxtIntA->pAvCtxt->aud.ctxt.clockHz;
  }

  gettimeofday(&tv, NULL);
  if(pCtxtIntV) {
    memset(&pCtxtIntV->trak, 0, sizeof(pCtxtIntV->trak));
    codecTypeV = pCtxtIntV->pAvCtxt->vid.codecType;
  }
  if(pCtxtIntA) {
    memset(&pCtxtIntA->trak, 0, sizeof(pCtxtIntA->trak));
    codecTypeA = pCtxtIntA->pAvCtxt->aud.codecType;
  }

  if(!(pMp4->proot->child =(BOX_T *) mp4_create_startheader(pMp4->proot->handler, 
                                                       *((uint32_t *) "ftyp"),
                                                       *((uint32_t *) "iso6"),
                                                       *((uint32_t *) "iso6"),
                                                       *((uint32_t *) "dash"),
                                                       0, 0))) {
    return NULL;
  }

  // ftyp -> isml, piff, iso2  (.ismv)
  // ftyp-> dash, iso6, avc, mp41
  // styp -> msdh, msdh, msix (.m4s)
  // ftyp -> isom, isom  (..._init... .mp4)

  pMp4->proot->child->pnext = pBoxMoov = mp4_create_box_generic(pMp4->proot->handler, *((uint32_t *) "moov") );
  pBoxMoov->child = pBox = (BOX_T *) mp4_create_box_mvhd(pMp4->proot->handler, 0, mvhd_timescale, 1,
                                             (uint64_t)tv.tv_sec + SEC_BETWEEN_EPOCH_1904_1970,
                                             (uint64_t)tv.tv_sec + SEC_BETWEEN_EPOCH_1904_1970);
  //
  // Override any timescale chosen by mp4_create_box_mvhd
  //
  ((BOX_MVHD_T *) pBox)->timescale = mvhd_timescale; 
  if(pCtxtIntV) {
    pCtxtIntV->pMvhd = (BOX_MVHD_T *) pBox;
  }
  if(pCtxtIntA) {
    pCtxtIntA->pMvhd = (BOX_MVHD_T *) pBox;
  }

  pBox->pnext = pBoxMvex = (BOX_T *) moof_create_mvex(pMp4->proot->handler, codecTypeV, codecTypeA, startTrackId);
  pBox = pBoxMvex;

  VSX_DEBUG_DASH( LOG(X_DEBUGV("DASH - moof_createEmptyMp4Isom pCtxtIntV: 0x%x, codecTypeV: 0x%x, pAvCtxt: 0x%x"), 
                              pCtxtIntV, codecTypeV, pCtxtIntV ? pCtxtIntV->pAvCtxt : NULL));
  if(pCtxtIntV && codectype_isVid(codecTypeV)) {
    
    if(!(pVTrak = moof_create_track(pMp4->proot->handler, pCtxtIntV, mdhd_timescale))) {
       mp4_close(pMp4);
      return NULL;
    }
    pBox->pnext = pVTrak;
    pBox = pVTrak;
    pCtxtIntV->mdhd_timescale = pCtxtIntV->trak.pMdhd->timescale;
  }

  VSX_DEBUG_DASH( LOG(X_DEBUGV("DASH - moof_createEmptyMp4Isom pCtxtIntA: 0x%x, codecTypeA: 0x%x, pAvCtxt: 0x%x"), 
                             pCtxtIntA, codecTypeA, pCtxtIntA ? pCtxtIntA->pAvCtxt : NULL));
  if(pCtxtIntA && codectype_isAud(codecTypeA)) {

    if(!(pATrak = moof_create_track(pMp4->proot->handler, pCtxtIntA, mdhd_timescale))) {
      mp4_close(pMp4);
      return NULL;
    }
    pBox->pnext = pATrak;
    pBox = pATrak;
    pCtxtIntA->mdhd_timescale = pCtxtIntA->trak.pMdhd->timescale;
  }

  if(!pATrak && !pVTrak) {
    LOG(X_ERROR("MOOF createEmptyMp4Isom no audio or video tracks available.  vid-codec: 0x%x, aud-codec: 0x%x"),
                   codecTypeV, codecTypeA);  
    mp4_close(pMp4);
    return NULL;
  }

  mp4_updateBoxSizes(pMp4->proot);

  return pMp4;
}

static void moof_mdat_close(MOOF_MDAT_WRITER_T *pMdatWr) {
  FILE_STREAM_T stream;

  if(pMdatWr->stream.fp != FILEOPS_INVALID_FP) {

    memcpy(&stream, &pMdatWr->stream, sizeof(stream));

    VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - moof_mdat_close deleting '%s'"), stream.filename) );
  
    CloseMediaFile(&pMdatWr->stream);

    //
    // Delete the temporary mdat file
    //
    fileops_DeleteFile(stream.filename);
  }

}

static int moof_mdat_open(MOOF_STATE_INT_T *pCtxtInt, 
                          const OUTFMT_FRAME_DATA_T *pFrame, 
                          unsigned int outidx) {
  int rc = 0;
  struct stat st;
  MOOF_MDAT_WRITER_T *pMdatWr = &pCtxtInt->mdat;

  //
  // Use a unique pid to allow multiple processes to function on the system
  //
  snprintf(pMdatWr->stream.filename, sizeof(pMdatWr->stream.filename), "%s%c%s%s_%d_%d.tmp", 
             "tmp", DIR_DELIMETER, "moofmdat", pCtxtInt->mpdAdaptation.padaptationTag, getpid(), outidx); 

  VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - moof_mdat_open '%s'"), pMdatWr->stream.filename) );

  //
  // Delete any temporary mdat file
  //
  if(fileops_stat(pMdatWr->stream.filename, &st) == 0) {
    fileops_DeleteFile(pMdatWr->stream.filename);
  }

  if((pMdatWr->stream.fp = fileops_Open(pMdatWr->stream.filename, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open %s for writing"), pMdatWr->stream.filename);
    return -1;
  }

  pMdatWr->stream.offset = 0;
  pMdatWr->stream.size = 0;

  return rc;
}

static int moof_mdat_finalize(MOOF_STATE_INT_T *pCtxtInt) {
  int rc = 0;
  MP4_TRAK_T *pTrak;

  pTrak = &pCtxtInt->trak;

  VSX_DEBUG_DASH(LOG(X_DEBUG("DASH - moof_mdat_finalize,%s%s pMoof: 0x%x"), 
                  pCtxtInt->isvid ? " video" : "", pCtxtInt->isaud ? " audio" : "", pCtxtInt->trak.moofTrak.pMoof));

  //
  // The respective MOOF box was never created and there is nothing to finalize
  // 
  //if(!pCtxtInt->pMoof) {
  if(!pCtxtInt->trak.moofTrak.pMoof) {
    return 0;
  } else if(!pCtxtInt->pMp4) {
    LOG(X_ERROR("DASH moof mdat finalize file mp4 does not exist"));
    return -1;
  }

  //
  // Create the unique incremental moof sequence id
  // 
  pCtxtInt->moofIdxInMp4++;
  pCtxtInt->moofSequence++;
  pTrak->moofTrak.pMfhd->sequence = pCtxtInt->moofSequence;

  mp4_updateBoxSizes(pTrak->moofTrak.pMoof);

  //
  // Establish the TRUN data offset to the start of the MDAT box, which will proceed the moof box
  //
  pTrak->moofTrak.pTrun->data_offset = pTrak->moofTrak.pMoof->szhdr + pTrak->moofTrak.pMoof->szdata + 8;

  return rc;
}

static int file_bcopy(FILE_STREAM_T *fsIn, FILE_STREAM_T *fsOut) {
  unsigned int idx = 0;
  unsigned int count, sz;
  char filename[VSX_MAX_PATH_LEN];
  unsigned char arena[4096];
  int rc = 0;

  count = (unsigned int) fsIn->offset;

  memcpy(filename, fsIn->filename, sizeof(filename));
  CloseMediaFile(fsIn);
  if(OpenMediaReadOnly(fsIn, filename) < 0) {
    return -1;
  }

  //if((SeekMediaFile(fsIn, 0, SEEK_SET)) < 0) {
  //  return rc;
  //}

  while(idx < count) {
    sz = MIN(count - idx, sizeof(arena));

    if((rc = fileops_Read(arena, 1, sz, fsIn->fp)) != sz) {
    //if((rc = ReadFileStream(fsIn, arena, sz) != sz)) {
      return -1;
    }

    if((rc = WriteFileStream(fsOut, arena, sz) != sz)) {
      return -1;
    }

    idx += sz;
  }

  return rc;
}

static int moof_write_mdat(MOOF_MDAT_WRITER_T *pMdatWr, FILE_STREAM_T *fStreamOut) {
  unsigned char arena[128];
  BOX_MDAT_T mdat;
  int rc = 0;

  //
  // Create the MDAT header
  //
  memset(&mdat, 0, sizeof(mdat));
  mdat.type = *((uint32_t *) "mdat");
  mdat.szhdr = 8;
  mdat.szdata = mdat.szdata_ownbox = pMdatWr->stream.offset;

  //
  // Create the MDAT box header
  //
  if((rc = mp4_write_box_header((BOX_T *) &mdat, arena, sizeof(arena))) < 0) {
    return rc;
  }

  //
  // Write the frame content to the (temporary) MDAT box
  //
  if((rc = WriteFileStream(fStreamOut, arena, rc)) < 0) {
    return rc;
  }

  VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - moof_write_mdat copy %d bytes in '%s' -> '%s' "), 
                           mdat.szdata_ownbox, pMdatWr->stream.filename, fStreamOut->filename) );

  //
  // Copy the cached mdat content to the correct location in the mp4 file
  //
  if((rc = file_bcopy(&pMdatWr->stream, fStreamOut)) < 0) {
    LOG(X_ERROR("Failed to copy mdat contents of size %d"), mdat.szdata_ownbox);
    return rc;
  }

  return rc;
}

static int moof_write_sidx(MOOF_STATE_INT_T *pCtxtInt, FILE_STREAM_T *pStreamOut) {
  int rc = 0;
  const BOX_HANDLER_T *pHandlerChain = NULL;
  BOX_SIDX_T *pBoxSidx = NULL;
  uint32_t size = 0;
  unsigned char arena[1024];

  if(!pCtxtInt->trak.moofTrak.pMoof) {
    LOG(X_ERROR("DASH moof write_sidx pMoof is NULL!"));
    return -1;
  }
  pHandlerChain = pCtxtInt->pMp4->proot->handler;

  //
  // size = size(mdat) + size(moof)
  //
  size = (8 + pCtxtInt->mdat.stream.offset) + 
         (pCtxtInt->trak.moofTrak.pMoof->szhdr + pCtxtInt->trak.moofTrak.pMoof->szdata);

  if(!(pBoxSidx = moof_create_sidx(pHandlerChain, pCtxtInt->isaud, pCtxtInt->trak.pMdhd->timescale, 
                                   pCtxtInt->trak.pMdhd->duration, size, pCtxtInt->durationAtMp4Start))) {
    return -1;
  }

  //
  // Serialize the sidx box content to the output stream
  //
  rc = mp4_write_boxes(pStreamOut, NULL, (BOX_T *) pBoxSidx, arena, sizeof(arena), NULL);

  mp4_freeBoxes((BOX_T *) pBoxSidx);

  if(rc < 0) {
    return rc;
  }

  return rc;
}

static int moof_write_styp(const BOX_HANDLER_T *pHandlerChain, FILE_STREAM_T *pStreamOut) {
  unsigned char arena[1024];
  BOX_FTYP_T *pBoxStyp = NULL;
  int rc = 0;


  //
  // TODO: the major, minor, compatibility types should be adjusted depending on profile
  //
  
  if(!(pBoxStyp =  mp4_create_startheader(pHandlerChain,
                                  *((uint32_t *) "styp"),
                                  *((uint32_t *) "iso6"),
                                  *((uint32_t *) "iso6"),
                                  *((uint32_t *) "msdh"),
                                  0, 0))) {
    return -1;
  }

  VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - moof_write_styp calling mp4_write_boxes... '%s'"), pStreamOut->filename) );

  //
  // Serialize the mp4 content to the output stream
  //
  rc = mp4_write_boxes(pStreamOut, NULL, (BOX_T *) pBoxStyp, arena, sizeof(arena), NULL);

  mp4_freeBoxes((BOX_T *) pBoxStyp);

  if(rc < 0) {
    return rc;
  }

  return rc;
}

static int moof_write(MOOF_STATE_INT_T *pCtxtInt) {
  FILE_STREAM_T fStreamOut;
  int rc = 0;
  const char *path = NULL;
  unsigned char arena[4096];

  //
  // Write the initializiation component of the mp4 (ftyp, moov)
  //
  if(!pCtxtInt->wroteMp4 && (!pCtxtInt->pMoofInitCtxt->useInitMp4 || !pCtxtInt->wroteInitMp4)) {

    if(pCtxtInt->pMoofInitCtxt->useInitMp4) {
      path = pCtxtInt->initPath;
      pCtxtInt->wroteInitMp4 = 1;
    } else {
      path = pCtxtInt->pathtmp;
    }

    VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - moof_write calling mp4_write '%s'"), path) );

    rc = mp4_write(pCtxtInt->pMp4, path, NULL);

    pCtxtInt->wroteMp4 = 1;
  }

  memset(&fStreamOut, 0, sizeof(fStreamOut));

  if(pCtxtInt->wroteMoof || !pCtxtInt->pMoofInitCtxt->useInitMp4) {

    VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - moof_write open and seek to end of '%s'"), pCtxtInt->pathtmp) );

    //
    // Append the content to the end of the moof mp4
    //
    if((fStreamOut.fp = fileops_OpenSeek(pCtxtInt->pathtmp, O_RDWR, 0, SEEK_END)) == FILEOPS_INVALID_FP) {
      LOG(X_ERROR("Unable to open file for appending: %s"), pCtxtInt->pathtmp);
      return -1;
    }
    strncpy(fStreamOut.filename, pCtxtInt->pathtmp, sizeof(fStreamOut.filename) - 1);

  } else {

    VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - moof_write opening for writing '%s'"), pCtxtInt->pathtmp) );

    //
    // Open the moof data mp4, which is separate from the initializer mp4
    //
    if((fStreamOut.fp = fileops_Open(pCtxtInt->pathtmp, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
      LOG(X_ERROR("Unable to open file for output: %s"), pCtxtInt->path);
      return -1;
    }
    strncpy(fStreamOut.filename, pCtxtInt->pathtmp, sizeof(fStreamOut.filename) - 1);

    if(moof_write_styp(pCtxtInt->pMp4->proot->handler, &fStreamOut) < 0) {
      LOG(X_ERROR("Unable to write styp box to mp4 moof start"));
      return -1;
    }

  }

  if(pCtxtInt->trak.moofTrak.pMoof) {

    VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - mp4_write_boxes '%s'"), fStreamOut.filename));

    if((rc = moof_write_sidx(pCtxtInt, &fStreamOut)) < 0) {
      LOG(X_ERROR("Failed to write sidx sequence %d"), pCtxtInt->trak.moofTrak.pMfhd->sequence);
    } else if((rc = mp4_write_boxes(&fStreamOut, NULL, pCtxtInt->trak.moofTrak.pMoof, 
                                    arena, sizeof(arena), NULL)) < 0) {
      LOG(X_ERROR("Failed to write moof sequence %d"), pCtxtInt->trak.moofTrak.pMfhd->sequence);
    } else if((rc = moof_write_mdat(&pCtxtInt->mdat, &fStreamOut)) < 0) {
      LOG(X_ERROR("Failed to write mdat header for moof sequence %d"), pCtxtInt->trak.moofTrak.pMfhd->sequence);
    }

  }

  VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - closing '%s', rc: %d"), fStreamOut.filename, rc));

  pCtxtInt->wroteMoof = 1;
  fileops_Close(fStreamOut.fp);

  return rc;
}

static void update_mvhd_trak(MOOF_STATE_INT_T *pCtxtInt, 
                             const struct timeval *ptv, 
                             const OUTFMT_FRAME_DATA_T *pFrame, 
                             uint64_t ptsMp4Start) {
  int64_t ptsElapsed = 0;

  ptsElapsed = (OUTFMT_PTS(pFrame) - ptsMp4Start);

  if(pCtxtInt->pMvhd) {
    pCtxtInt->pMvhd->duration = (uint64_t) ((float) ptsElapsed / PTS_HZ_SECF * pCtxtInt->pMvhd->timescale);

    pCtxtInt->pMvhd->modificationtime = (uint64_t) ptv->tv_sec + SEC_BETWEEN_EPOCH_1904_1970;

  }

  if(pCtxtInt->trak.pMdhd) {
    pCtxtInt->trak.pMdhd->duration = (uint64_t) ((float)ptsElapsed / PTS_HZ_SECF * pCtxtInt->trak.pMdhd->timescale);
    pCtxtInt->trak.pMdhd->modificationtime = (uint64_t )ptv->tv_sec + SEC_BETWEEN_EPOCH_1904_1970;
    if(pCtxtInt->trak.pTkhd) {
      pCtxtInt->trak.pTkhd->duration = pCtxtInt->trak.pMdhd->duration;
      pCtxtInt->trak.pTkhd->modificationtime = (uint64_t) ptv->tv_sec + SEC_BETWEEN_EPOCH_1904_1970;
    }

    pCtxtInt->mdhd_duration = pCtxtInt->trak.pMdhd->duration;
  }

  //TODO: the above changes are not synced to the current mp4

}

static int onNextMoofInFile(MP4_CREATE_STATE_MOOF_T *pCtxt, const OUTFMT_FRAME_DATA_T *pFrame) {
  struct timeval tv;
  int64_t ptsElapsed = 0;
  unsigned int mp4idx = 0;
  MOOF_STATE_INT_T *pCtxtInt = NULL;
  int rc = 0;

  VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - onNextMoofInFile pts:%.3f - %.3f > %.3f, mp4 duration:%.3f"), 
        PTSF(OUTFMT_PTS(pFrame)), PTSF(pCtxt->stateAV[0].ptsMoofBoxStart), 
        pCtxt->moofInitCtxt.moofTrafMaxDurationSec, PTSF(OUTFMT_PTS(pFrame) - pCtxt->ptsMp4Start)));

  //
  // Update the mp4 mvhd duration and the trak mdhd durations
  //
  gettimeofday(&tv, NULL);

  ptsElapsed = (OUTFMT_PTS(pFrame) - pCtxt->ptsStart);

  //
  // Update each audio and video track parameters such as duration and modification time
  //
  for(mp4idx = 0; mp4idx < MOOF_AV_MAX; mp4idx++) {
    pCtxtInt = &pCtxt->stateAV[mp4idx];
    if(pCtxtInt->trak.pMdhd && pCtxtInt->pMvhd) {
      pCtxtInt->duration = (uint64_t) ((float) ptsElapsed / PTS_HZ_SECF * pCtxtInt->trak.pMdhd->timescale);
      update_mvhd_trak(pCtxtInt, &tv, pFrame, pCtxt->ptsMp4Start);
    }
  }

  //
  // Finalize the MDAT content and write the current moof to the output stream 
  //
  moof_mdat_finalize(&pCtxt->stateAV[MOOF_AV_IDX_VID]);
  moof_mdat_finalize(&pCtxt->stateAV[MOOF_AV_IDX_AUD]);

  //
  // Audio MOOF is always written before video moof according to the order finalize is called
  // for audio & video MOOfs, so that moof sequences in mp4 are sequential
  //
  if(pCtxt->stateAV[MOOF_AV_IDX_AUD].path[0] != '\0' && 
    (rc = moof_write(&pCtxt->stateAV[MOOF_AV_IDX_AUD])) < 0) {
    return -1;
  }
  closeMoofTrak(&pCtxt->stateAV[MOOF_AV_IDX_AUD].trak);

  //
  // If mixing audio + video into one moof, mark the 
  //
  if(pCtxt->moofInitCtxt.mixAudVid) {
    if(pCtxt->stateAV[MOOF_AV_IDX_AUD].wroteInitMp4) {
      pCtxt->stateAV[MOOF_AV_IDX_VID].wroteInitMp4 = pCtxt->stateAV[MOOF_AV_IDX_AUD].wroteInitMp4;
    }
  }

  if(pCtxt->stateAV[MOOF_AV_IDX_VID].path[0] != '\0' && 
     (rc = moof_write(&pCtxt->stateAV[MOOF_AV_IDX_VID])) < 0) {
    return -1;
  }

  closeMoofTrak(&pCtxt->stateAV[MOOF_AV_IDX_VID].trak);

  pCtxt->stateAV[MOOF_AV_IDX_VID].ptsMoofBoxStart = OUTFMT_PTS(pFrame);
  pCtxt->stateAV[MOOF_AV_IDX_AUD].ptsMoofBoxStart = OUTFMT_PTS(pFrame);

  VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - onNextMoofInFile setting pts: %.3f"), PTSF(OUTFMT_PTS(pFrame))) );

  return rc;
}

static int update_mpd(MP4_CREATE_STATE_MOOF_T *pCtxt, int outidx, MPD_UPDATE_CTXT_T mpdUpdates[MOOF_AV_MAX]) {
  int rc = 0;
  MOOF_STATE_INT_T *pCtxtInt;
  MPD_ADAPTATION_T *pAdaptationHead;
  unsigned int mp4idx;
  int duplicateIter = 0;
  MPD_UPDATE_CTXT_T _mpdUpdates[MOOF_AV_MAX];
  MPD_UPDATE_CTXT_T *pMpdUpdates = mpdUpdates;

  if(pCtxt->mpdCtxt.outidxTot <= 1) {
    mpdUpdates = NULL;
    outidx = -1;
  }

  if(!mpdUpdates) {
    memset(_mpdUpdates, 0, sizeof(_mpdUpdates));
    pMpdUpdates = _mpdUpdates;
  }

  for(mp4idx = 0; mp4idx < MOOF_AV_MAX; mp4idx++) {
    pCtxtInt = &pCtxt->stateAV[mp4idx];

    //
    // If no_init_state is set, then use the existing input argument state 
    // likely because we're creating a master mpd with multiple representations, invoked after
    // all individual outidx output specific playlists have already been created
    //
    if(!pMpdUpdates[mp4idx].no_init_state) {
      strncpy(pMpdUpdates[mp4idx].uniqIdBuf, pCtxtInt->path_uniqidstr, sizeof(pMpdUpdates[mp4idx].uniqIdBuf) -1);
      pMpdUpdates[mp4idx].media.mediaUniqIdstr = pMpdUpdates[mp4idx].uniqIdBuf;
      pMpdUpdates[mp4idx].media.mediaDir = pCtxt->outdir;
      pMpdUpdates[mp4idx].media.timescale = pCtxtInt->mdhd_timescale;
      pMpdUpdates[mp4idx].media.duration = pCtxtInt->mdhd_duration;
      pMpdUpdates[mp4idx].media.durationPreceeding = pCtxtInt->durationAtMp4Start;
    } else {
      duplicateIter = 1;
    }

    pMpdUpdates[mp4idx].mediaSequenceIndex = pCtxt->mp4Sequence;
    pMpdUpdates[mp4idx].mediaSequenceMin = -1;

    pCtxtInt->mpdAdaptation.pMedia = &pMpdUpdates[mp4idx].media;
  }

  if(pCtxt->pAvCtxt->aud.pStreamerCfg->novid) {
    pAdaptationHead = &pCtxt->stateAV[MOOF_AV_IDX_AUD].mpdAdaptation;
  } else {
    pAdaptationHead = &pCtxt->stateAV[MOOF_AV_IDX_VID].mpdAdaptation;
  }


  VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - update_mpd calling mpd outidx:%d... pAdaptation: 0x%x "
                              "(vid: 0x%x, aud: 0x%x), pAdaptation->pnext: 0x%x, '%s', %lluHz, %lluHz"), 
                   outidx, pAdaptationHead, &pCtxt->stateAV[MOOF_AV_IDX_VID].mpdAdaptation, 
                   &pCtxt->stateAV[MOOF_AV_IDX_AUD].mpdAdaptation, pAdaptationHead->pnext,
                   pCtxtInt->path_uniqidstr, pCtxtInt->durationAtMp4Start, pCtxtInt->mdhd_duration));

  rc = mpd_on_new_mediafile(&pCtxt->mpdCtxt, pAdaptationHead, outidx, 
                            pMpdUpdates[0].mediaSequenceIndex, 
                            pMpdUpdates[0].mediaSequenceMin,
                            duplicateIter);
  pMpdUpdates[0].did_update = 1;
                             
  return rc;
}

static int onNextMp4(MP4_CREATE_STATE_MOOF_T *pCtxt, 
                     const OUTFMT_FRAME_DATA_T *pFrame, 
                     MPD_UPDATE_CTXT_T mpdUpdates[MOOF_AV_MAX]) {
  int rc = 0;
  MOOF_STATE_INT_T *pCtxtInt = NULL;
  unsigned int mp4idx = 0;
  int do_vid = 0;
  int do_aud = 0;
  char seqstr[128];
  char tmp[128];

  VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - onNextMp4...")) );

  for(mp4idx = 0; mp4idx < MOOF_AV_MAX; mp4idx++) {
    pCtxtInt = &pCtxt->stateAV[mp4idx];

    pCtxtInt->durationAtMp4Start = pCtxtInt->duration;

    if(pCtxtInt->pMp4 && pCtxtInt->path[0] != '\0') {

      VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - onNextMp4[%d] renaming '%s' -> '%s'"), 
                          mp4idx, pCtxtInt->pathtmp, pCtxtInt->path));

      //
      // Rename the html/dash/...tmp to html/dash/...mp4
      //
      if(fileops_Rename(pCtxtInt->pathtmp, pCtxtInt->path) != 0) {
        LOG(X_ERROR("Failed to rename %s -> %s"), pCtxtInt->pathtmp, pCtxtInt->path);
      }
      //const char *_path = strutil_getFileName(pCtxtInt->path);
      //LOG(X_DEBUG("SYMLINK '%s' -> '%s'"), pCtxtInt->path2, _path);
      //symlink(_path, pCtxtInt->path2);

    }
  }

   //
   // Update the mpd document context with the new filepath
   //
  if(pCtxt->mpdCtxt.active && (pCtxt->stateAV[MOOF_AV_IDX_VID].pMp4 || pCtxt->stateAV[MOOF_AV_IDX_AUD].pMp4)) {

    if((rc = update_mpd(pCtxt, pCtxt->moofInitCtxt.requestOutidx, mpdUpdates)) < 0) {
      LOG(X_ERROR("Failed to update DASH mpd"));
    }

  }

  //
  // Close the existing mp4 reference
  //
  for(mp4idx = 0; mp4idx < MOOF_AV_MAX; mp4idx++) {
    pCtxtInt = &pCtxt->stateAV[mp4idx];
    if(pCtxtInt->pMp4 && (mp4idx == 0 || !pCtxt->moofInitCtxt.mixAudVid)) {
      mp4_close(pCtxtInt->pMp4);
    }
    pCtxtInt->pMp4 = NULL;
  }

  //
  // Set the output path of the fragmented mp4 file
  //
  pCtxt->mp4Sequence++;
 
  for(mp4idx = 0; mp4idx < MOOF_AV_MAX; mp4idx++) {

    if((mp4idx == MOOF_AV_IDX_VID && pCtxt->pAvCtxt->vid.pStreamerCfg->novid) ||
       (mp4idx == MOOF_AV_IDX_AUD && pCtxt->pAvCtxt->aud.pStreamerCfg->noaud)) {
      continue;
    } else if(mp4idx > 0 && pCtxt->moofInitCtxt.mixAudVid) {

      //
      // Shared audio + video mp4 / mvhd context
      //
      pCtxt->stateAV[mp4idx].pMp4 = pCtxt->stateAV[0].pMp4;
      pCtxt->stateAV[mp4idx].pMvhd = pCtxt->stateAV[0].pMvhd;
      break;
    }

    if(!pCtxt->pAvCtxt->vid.pStreamerCfg->novid && mp4idx == MOOF_AV_IDX_VID) {
      do_vid = 1;
    } else {
      do_vid = 0;
    }
    if(!pCtxt->pAvCtxt->aud.pStreamerCfg->noaud && 
       ((pCtxt->moofInitCtxt.mixAudVid && mp4idx == 0) || 
        (!pCtxt->moofInitCtxt.mixAudVid && mp4idx == MOOF_AV_IDX_AUD))) {
      do_aud = 1;
    } else {
      do_aud = 0;
    }
    if(!do_vid &&  !do_aud) {
      continue;
    }

    pCtxtInt = &pCtxt->stateAV[mp4idx];

    VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - onNextMp4 calling moof_createEmptyMp4Isom [mp4idx:%d] "
              "ftyp, mp4Sequence: %d, do_vid:%d, do_aud: %d"), mp4idx, pCtxt->mp4Sequence, do_vid, do_aud));

    //
    // Create the mp4 container containing all initialization sequences
    //
    if(!(pCtxtInt->pMp4 = moof_createEmptyMp4Isom(
                                       do_vid ? &pCtxt->stateAV[MOOF_AV_IDX_VID] : NULL,
                                       do_aud ? &pCtxt->stateAV[MOOF_AV_IDX_AUD] : NULL,
                                       mp4idx + 1))) {
      return -1;
    }

    snprintf(seqstr, sizeof(seqstr), "%d", pCtxt->mp4Sequence);

    //
    // Set temp file .m4s path
    //
    mpd_format_path(tmp, sizeof(tmp), mpd_format_path_prefix, pCtxt->moofInitCtxt.requestOutidx, 
                    pCtxt->mpdCtxt.init.outfileprefix, seqstr, "tmp", 
                    pCtxtInt->mpdAdaptation.padaptationTag);
    mediadb_prepend_dir(pCtxt->outdir, tmp, pCtxtInt->pathtmp, sizeof(pCtxtInt->pathtmp));

    //
    // Set the .m4s path
    //
    if(pCtxt->mpdCtxt.init.dash_mpd_type == DASH_MPD_TYPE_MOOF_SEGMENT_TEMPLATE_NUMBER) {
      strncpy(pCtxtInt->path_uniqidstr, seqstr, sizeof(pCtxtInt->path_uniqidstr) - 1);
      mpd_format_path(tmp, sizeof(tmp), mpd_format_path_prefix, pCtxt->moofInitCtxt.requestOutidx, 
                      pCtxt->mpdCtxt.init.outfileprefix, seqstr, DASH_DEFAULT_SUFFIX_M4S, 
                      pCtxtInt->mpdAdaptation.padaptationTag);

    } else if(pCtxt->mpdCtxt.init.dash_mpd_type == DASH_MPD_TYPE_MOOF_SEGMENT_TEMPLATE_TIME) {
      snprintf(pCtxtInt->path_uniqidstr, sizeof(pCtxtInt->path_uniqidstr), "%llu", pCtxtInt->durationAtMp4Start);
      mpd_format_path(tmp, sizeof(tmp), mpd_format_path_prefix, pCtxt->moofInitCtxt.requestOutidx, 
                      pCtxt->mpdCtxt.init.outfileprefix, pCtxtInt->path_uniqidstr, DASH_DEFAULT_SUFFIX_M4S, 
                      pCtxtInt->mpdAdaptation.padaptationTag);
    } else {
      return -1;
    }
    mediadb_prepend_dir(pCtxt->outdir, tmp, pCtxtInt->path, sizeof(pCtxtInt->path));

    //
    // If configured to use an initializer mp4 file, establish its path
    //
    if(pCtxt->moofInitCtxt.useInitMp4 && pCtxtInt->initPath[0] == '\0') {

      mpd_format_path(tmp, sizeof(tmp), mpd_format_path_prefix, 
                          pCtxt->moofInitCtxt.requestOutidx, pCtxt->mpdCtxt.init.outfileprefix, 
                         "init", DASH_DEFAULT_SUFFIX_M4S, pCtxtInt->mpdAdaptation.padaptationTag);

      mediadb_prepend_dir(pCtxt->outdir, tmp, pCtxtInt->initPath, sizeof(pCtxtInt->initPath));
      
      pCtxt->mpdCtxt.useInitMp4 = pCtxt->moofInitCtxt.useInitMp4;
    }

    VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - onNextMp4 constructed segment filenames [mp4idx:%d], "
                                "path: '%s', tmp: '%s', init: '%s',  mp4Sequence: %d, pTkhd: 0x%x"), 
                      mp4idx, pCtxtInt->path, pCtxtInt->pathtmp, pCtxtInt->initPath, pCtxt->mp4Sequence,
                      pCtxtInt->trak.pTkhd ));

  } // end of for(mp4idx ...

  //
  // Sanity check
  //
  if(!pCtxt->stateAV[MOOF_AV_IDX_VID].pMp4 && !pCtxt->stateAV[MOOF_AV_IDX_AUD].pMp4) {
    LOG(X_ERROR("No m4s instance created!"));
    return -1;
  }

  if(pCtxt->mp4Sequence == 1) {
    pCtxt->ptsStart = OUTFMT_PTS(pFrame);
  }
  pCtxt->ptsMp4Start = OUTFMT_PTS(pFrame);
  pCtxt->stateAV[MOOF_AV_IDX_AUD].wroteMoof = pCtxt->stateAV[MOOF_AV_IDX_VID].wroteMoof = 0;

  return rc;
}

static int onNewMoofFile(MP4_CREATE_STATE_MOOF_T *pCtxt, 
                         const OUTFMT_FRAME_DATA_T *pFrame) {
  BOX_MOOF_T *pMoof = NULL;
  MOOF_STATE_INT_T *pCtxtInt = NULL;
  int rc = 0;

  VSX_DEBUG_DASH(LOG(X_DEBUG("DASH - onNewMoofFile %s..."), pFrame->isvid ? "video" : "audio"));
  pCtxtInt = &pCtxt->stateAV[MP4IDX(pFrame->isvid)];

  if(!(pMoof = moof_createMoof(pCtxtInt->pMp4->proot->handler, pCtxt->pAvCtxt, 
                               &pCtxtInt->trak, pCtxtInt->isvid, pCtxtInt->durationAtMp4Start))) {
    return -1;
  }

  pCtxtInt->trak.moofTrak.pMoof = pMoof;
  pCtxtInt->ptsMoofBoxStart = OUTFMT_PTS(pFrame);

  moof_mdat_close(&pCtxtInt->mdat);

  //
  // Create the temporary mdat file.  Do not use OUTFMT_IDX(pFrame) since this may always be 0
  // for an audio frame
  //
  if(moof_mdat_open(pCtxtInt, pFrame, pCtxt->moofInitCtxt.requestOutidx) < 0) {
    return -1;
  }

  return rc;
}

static void mp4moof_deletefiles(MP4_CREATE_STATE_MOOF_T *pCtxt) {

  char buf[VSX_MAX_PATH_LEN];
  unsigned int outidx;
  unsigned int mp4idx;

  //
  // Remove any media segments
  //
  for(mp4idx = 0; mp4idx < MOOF_AV_MAX; mp4idx++) {

    //if(pCtxt->stateAV[mp4idx].path[0] == '\0') {
    //  continue;
    //}

    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if(outidx > 0 && !pCtxt->pAvCtxt->vid.pStreamerCfg->xcode.vid.out[outidx].active) {
        continue;
      }

      if(mpd_format_path_prefix(buf, sizeof(buf), outidx, pCtxt->mpdCtxt.init.outfileprefix, 
                                pCtxt->stateAV[mp4idx].mpdAdaptation.padaptationTag) > 0) {

        VSX_DEBUG_DASH( LOG(X_DEBUG("mp4moof_deletefiles '%s'..."), buf) );

        http_purge_segments(pCtxt->outdir, buf, "."DASH_DEFAULT_SUFFIX_M4S, 0xffffffff, 1);
        http_purge_segments(pCtxt->outdir, buf, ".tmp", 0xffffffff, 1);
      }

    } // end of for(outidx...

  } // end of for(mp4idx...

}

static void init_adaptations(MP4_CREATE_STATE_MOOF_T *pCtxt) {

  pCtxt->stateAV[MOOF_AV_IDX_VID].mpdAdaptation.padaptationTag = 
                                  DASH_MP4ADAPTATION_NAME(pCtxt->moofInitCtxt.mixAudVid ? -1 : 1);
  pCtxt->stateAV[MOOF_AV_IDX_AUD].mpdAdaptation.padaptationTag = 
                                  DASH_MP4ADAPTATION_NAME(pCtxt->moofInitCtxt.mixAudVid ? -1 : 0);

  //
  // Video channel will go into first adaptation set
  //
  if(!pCtxt->pAvCtxt->vid.pStreamerCfg->novid) {
    pCtxt->stateAV[MOOF_AV_IDX_VID].mpdAdaptation.isvid = 1;
  }

  //
  // Audio channel will go into second adaptation set, unless muxing audio + video in one media file
  //
  if(!pCtxt->pAvCtxt->vid.pStreamerCfg->noaud) {
    if(pCtxt->moofInitCtxt.mixAudVid) {
      pCtxt->stateAV[MOOF_AV_IDX_VID].mpdAdaptation.isaud = 1;
    } else {
      pCtxt->stateAV[MOOF_AV_IDX_AUD].mpdAdaptation.isaud = 1;
    }
  }

  if(!pCtxt->moofInitCtxt.mixAudVid && !pCtxt->pAvCtxt->vid.pStreamerCfg->novid && 
     !pCtxt->pAvCtxt->vid.pStreamerCfg->noaud) {
    pCtxt->stateAV[MOOF_AV_IDX_VID].mpdAdaptation.pnext = &pCtxt->stateAV[MOOF_AV_IDX_AUD].mpdAdaptation; 
  }

}

int mp4moof_init(MP4_CREATE_STATE_MOOF_T *pCtxt, 
                 CODEC_AV_CTXT_T *pAvCtxt,
                 const MOOF_INIT_CTXT_T *pMoofInitCtxt,
                 const DASH_INIT_CTXT_T *pDashInitCtxt) {

  struct stat st;

  if(!pMoofInitCtxt || !pDashInitCtxt) {
    return -1;
  }

  pCtxt->pAvCtxt = pCtxt->stateAV[0].pAvCtxt = pCtxt->stateAV[1].pAvCtxt = pAvCtxt;
  pCtxt->stateAV[0].pMoofInitCtxt = pCtxt->stateAV[1].pMoofInitCtxt = &pCtxt->moofInitCtxt;

  memcpy(&pCtxt->moofInitCtxt, pMoofInitCtxt, sizeof(pCtxt->moofInitCtxt));
  //memcpy(&pCtxt->mpdCtxt.init, pDashInitCtxt, sizeof(pCtxt->mpdCtxt.init));

  //
  // Validate configuration settings
  //
  if(pCtxt->moofInitCtxt.moofTrafMaxDurationSec <= 0) {
    pCtxt->moofInitCtxt.moofTrafMaxDurationSec = MOOF_TRAF_MAX_DURATION_SEC_DEFAULT;
  }
  if(pCtxt->moofInitCtxt.mp4MaxDurationSec <  pCtxt->moofInitCtxt.mp4MinDurationSec) {
    pCtxt->moofInitCtxt.mp4MaxDurationSec = pCtxt->moofInitCtxt.mp4MinDurationSec;
  }
  if(pCtxt->moofInitCtxt.mp4MaxDurationSec <= 0) {
    pCtxt->moofInitCtxt.mp4MaxDurationSec = MOOF_MP4_MAX_DURATION_SEC_DEFAULT;
  }

  if(fileops_stat(pDashInitCtxt->outdir, &st) != 0) {
    LOG(X_ERROR("DASH output dir '%s' does not exist."), pDashInitCtxt->outdir);
    return -1;
  }

  pCtxt->mpdCtxt.init.outfileprefix = pDashInitCtxt->outfileprefix;
  if(!pCtxt->mpdCtxt.init.outfileprefix || pCtxt->mpdCtxt.init.outfileprefix[0] == '\0') {
    pCtxt->mpdCtxt.init.outfileprefix = DASH_DEFAULT_NAME_PRFX;
  }

  pCtxt->mpdCtxt.mixAudVid = pCtxt->moofInitCtxt.mixAudVid;

  init_adaptations(pCtxt);

  if(!pCtxt->pAvCtxt->vid.pStreamerCfg->novid) {
    pCtxt->stateAV[MOOF_AV_IDX_VID].isvid = 1;
  }
  if(!pCtxt->pAvCtxt->vid.pStreamerCfg->noaud) {
    pCtxt->stateAV[MOOF_AV_IDX_AUD].isaud = 1;
  }


/*
  pCtxt->clockHz = 3000;
  pCtxt->moofTrafMaxDurationSec = 0.5f;
  if(useRAP) {
    //
    // Attempt to create new mp4 on keyframe after min duration has been reached
    //
    pCtxt->mp4MinDurationSec = 5.0f;
    pCtxt->mp4MaxDurationSec = 10.0f;
  } else {
    pCtxt->mp4MinDurationSec = 0.0f;
    pCtxt->mp4MaxDurationSec = 5.0f;
  }
  pCtxt->useInitMp4 = useInitMp4;
*/

  pCtxt->stateAV[0].mdat.stream.fp = FILEOPS_INVALID_FP;
  pCtxt->stateAV[1].mdat.stream.fp = FILEOPS_INVALID_FP;
  pCtxt->outdir = pDashInitCtxt->outdir;

  VSX_DEBUG_DASH( 
    LOG(X_DEBUG("DASH - mp4moof_init[outidx:%d] moofTrafMaxDuration: %.2f, mp4MinDuration: %.2f, "
                "mp4MaxDuration: %.2f, outFilePrefix: '%s', mixAudVid:%d, syncSegments:%d"), 
               pCtxt->moofInitCtxt.requestOutidx, pCtxt->moofInitCtxt.moofTrafMaxDurationSec, 
               pCtxt->moofInitCtxt.mp4MinDurationSec, pCtxt->moofInitCtxt.mp4MaxDurationSec, 
               pCtxt->mpdCtxt.init.outfileprefix, pCtxt->moofInitCtxt.mixAudVid, 
               pCtxt->moofInitCtxt.syncMultiOutidx); 
  )

  // 
  // Init the MPD document context
  //
  if(pDashInitCtxt->enableDash) {

    if(mpd_init(&pCtxt->mpdCtxt, pDashInitCtxt, pCtxt->pAvCtxt) < 0) {
      return -1;
    }

    mp4moof_deletefiles(pCtxt);

    mpd_close(&pCtxt->mpdCtxt, 1, 0);

  }

  return 0;
}

static int mp4moof_close_int(MP4_CREATE_STATE_MOOF_T *pCtxt) {

  unsigned int mp4idx;
  MOOF_STATE_INT_T *pCtxtInt = NULL;

  VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - mp4moof_close_int...")) );

  for(mp4idx = 0; mp4idx < MOOF_AV_MAX;  mp4idx++) {

    pCtxtInt = &pCtxt->stateAV[mp4idx];

    moof_mdat_finalize(pCtxtInt);
    moof_mdat_close(&pCtxtInt->mdat);
    pCtxtInt->pTrunEntryPrior = NULL;
    pCtxtInt->pMvhd = NULL;

    if(pCtxtInt->pMp4) {
      mp4_close(pCtxtInt->pMp4);
      pCtxtInt->pMp4 = NULL;
    }
    closeMoofTrak(&pCtxtInt->trak);

    pCtxtInt->wroteMp4 = 0;
    pCtxtInt->wroteMoof = 0;
    pCtxtInt->moofIdxInMp4 = 0;
  }
  //pCtxt->moofSequence = 0;

  return 0;
}

int mp4moof_close(MP4_CREATE_STATE_MOOF_T *pCtxt) {
  int rc;

  if(!pCtxt->pAvCtxt) {
    return -1;
  }

  rc = mp4moof_close_int(pCtxt);

  //
  // Close the MPD document context
  //
  mpd_close(&pCtxt->mpdCtxt, !pCtxt->moofInitCtxt.nodelete, 1);

  if(!pCtxt->moofInitCtxt.nodelete) {
    mp4moof_deletefiles(pCtxt);
  }

  return rc;
}

//static vframes;
static int mp4moof_addFrame_int(MP4_CREATE_STATE_MOOF_T *pCtxt, 
                                const OUTFMT_FRAME_DATA_T *pFrame, 
                                int do_new_m4s,
                                MPD_UPDATE_CTXT_T mpdUpdates[MOOF_AV_MAX]) {
  MOOF_STATE_INT_T *pCtxtInt = NULL;
  unsigned int idx;
  int rc = 0;

  if(!pCtxt || !pCtxt->pAvCtxt || !pFrame) {
    return -1;
  } 

  //if(pFrame->isvid) vframes++;

  pCtxtInt = &pCtxt->stateAV[MP4IDX(pFrame->isvid)];

  if(!pCtxt->haveFirst) {
    LOG(X_DEBUG("DASH moof packetizer mp4 initializer:%d, moof duration: %.3fs, file duration: %.3fs - %.3fs"),
      pCtxt->moofInitCtxt.useInitMp4, pCtxt->moofInitCtxt.moofTrafMaxDurationSec, 
      pCtxt->moofInitCtxt.mp4MinDurationSec, pCtxt->moofInitCtxt.mp4MaxDurationSec);
    pCtxt->haveFirst = 1;
  }

  VSX_DEBUG_DASH(
    LOG(X_DEBUGV("DASH - mp4moof_addFrame %s [outidx:%d,req:%d], len:%d, key:%d pts:%llu (%.3f) "
                 "pCtxt: 0x%x, V.pMoof:0x%x, A.pMoof:0x%x"), 
           pFrame->isvid ? "vid" : "aud", pFrame->xout.outidx, pCtxt->moofInitCtxt.requestOutidx, 
           OUTFMT_LEN(pFrame), OUTFMT_KEYFRAME(pFrame), OUTFMT_PTS(pFrame), PTSF(OUTFMT_PTS(pFrame)), 
           pCtxt, pCtxt->stateAV[0].trak.moofTrak.pMoof, pCtxt->stateAV[1].trak.moofTrak.pMoof); 
        //LOG(X_DEBUGV("DASH - mp4moof_addFrame ptsElapsed:%llu, duration: %llu, ptsMoofBoxStart:%llu"), pCtxtInt->ptsElapsed, pCtxtInt->duration, pCtxtInt->ptsMoofBoxStart);
        LOGHEX_DEBUGV(OUTFMT_DATA(pFrame), MIN(OUTFMT_LEN(pFrame), 16));
//LOG(X_DEBUGV("DASH - xout.outidx:%d, xout.outbuf.buf: 0x%x, lenbuf:%d, { [0].p:0x%x, len:%d, 0x%x 0x%x 0x%x 0x%x ... 0x%x 0x%x 0x%x 0x%x}, [1]..p:0x%x, len:%d, 0x%x 0x%x 0x%x 0x%x ... 0x%x 0x%x 0x%x 0x%x"), pFrame->xout.outidx, pFrame->xout.outbuf.buf, pFrame->xout.outbuf.lenbuf, pFrame->xout.outbuf.poffsets[0], pFrame->xout.outbuf.lens[0],pFrame->xout.outbuf.poffsets[0][0], pFrame->xout.outbuf.poffsets[0][1],pFrame->xout.outbuf.poffsets[0][2],pFrame->xout.outbuf.poffsets[0][3],pFrame->xout.outbuf.poffsets[0][12],pFrame->xout.outbuf.poffsets[0][13],pFrame->xout.outbuf.poffsets[0][14], pFrame->xout.outbuf.poffsets[0][15],pFrame->xout.outbuf.poffsets[1], pFrame->xout.outbuf.lens[1],pFrame->xout.outbuf.poffsets[1]?pFrame->xout.outbuf.poffsets[1][0]:-1, pFrame->xout.outbuf.poffsets[1]?pFrame->xout.outbuf.poffsets[1][1]:-1,pFrame->xout.outbuf.poffsets[1]?pFrame->xout.outbuf.poffsets[1][2]:-1,pFrame->xout.outbuf.poffsets[1]?pFrame->xout.outbuf.poffsets[1][3]:-1,pFrame->xout.outbuf.poffsets[1]?pFrame->xout.outbuf.poffsets[1][12]:-1,pFrame->xout.outbuf.poffsets[1]?pFrame->xout.outbuf.poffsets[1][13]:-1,pFrame->xout.outbuf.poffsets[1]?pFrame->xout.outbuf.poffsets[1][14]:-1, pFrame->xout.outbuf.poffsets[1]?pFrame->xout.outbuf.poffsets[1][15]:-1);
  );

  if(!pFrame->isvid) {
    //
    // We don't want to start the beginning of content with a series of audio frames
    // since the first video may take a while to arrive and it would then be put into 
    // a future dated moof, possibly introducing a/v sync problems.
    //
    if(!pCtxt->pAvCtxt->aud.pStreamerCfg->novid &&!pCtxt->stateAV[MOOF_AV_IDX_VID].haveFirst) {
      return 0;
    }
  }

  pCtxtInt->haveFirst = 1;

  //
  // Check if the MOOF box max duration has been exceeded
  //
  if(pCtxtInt->pMp4 && pCtxt->moofInitCtxt.moofTrafMaxDurationSec > 0 && 
     OUTFMT_PTS(pFrame) > pCtxtInt->ptsMoofBoxStart && 
     OUTFMT_PTS(pFrame) - pCtxtInt->ptsMoofBoxStart > pCtxt->moofInitCtxt.moofTrafMaxDurationSec * PTS_HZ_SECF) {

    VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - mp4moof_addFrame moof %s duration exceeded %.3f > %.3f, "
                                "calling onNextMoofInFile..."), 
                   pCtxtInt->isvid ? "video" : "audio", PTSF(OUTFMT_PTS(pFrame) - pCtxtInt->ptsMoofBoxStart), 
                   PTSF(pCtxt->moofInitCtxt.moofTrafMaxDurationSec * PTS_HZ_SECF))); 

    if((rc = onNextMoofInFile(pCtxt, pFrame)) < 0) {
      return rc;
    }

  }

  //
  // Check if the mp4 duration has been exceeded
  // If mp4 min duration is set, then try to break on a keyframe boundary, until max duration
  // is reached
  //
  if(pCtxtInt->pMp4) {

    VSX_DEBUG_DASH( 
      LOG(X_DEBUGV("DASH - mp4moof_addFrame %s [outidx:%d] len:%d, mp4 duration %.3f, min: %.3f, max: %.3f, "
                   "keyframe: %d checking mp4 duration... "), pFrame->isvid ? "vid" : "aud",
                   pCtxt->moofInitCtxt.requestOutidx, OUTFMT_LEN(pFrame), 
                   PTSF(OUTFMT_PTS(pFrame) - pCtxt->ptsMp4Start), 
                   PTSF(pCtxt->moofInitCtxt.mp4MinDurationSec * PTS_HZ_SECF), 
                   PTSF(pCtxt->moofInitCtxt.mp4MaxDurationSec * PTS_HZ_SECF), OUTFMT_KEYFRAME(pFrame))); 

    if(do_new_m4s) {

      if(pCtxt->stateAV[MOOF_AV_IDX_VID].trak.moofTrak.pMoof || 
         pCtxt->stateAV[MOOF_AV_IDX_AUD].trak.moofTrak.pMoof) {

        VSX_DEBUG_DASH( 
          LOG(X_DEBUG("DASH - mp4moof_addFrame [outidx:%d] mp4 duration exceeded %.3f > "
                   "min: %.3f, max: %.3f, keyframe: %d, calling onNextMoofInFile..."), 
                   pCtxt->moofInitCtxt.requestOutidx, PTSF(OUTFMT_PTS(pFrame) - pCtxt->ptsMp4Start), 
                   PTSF(pCtxt->moofInitCtxt.mp4MinDurationSec * PTS_HZ_SECF), 
                   PTSF(pCtxt->moofInitCtxt.mp4MaxDurationSec * PTS_HZ_SECF), OUTFMT_KEYFRAME(pFrame))); 

        if((rc = onNextMoofInFile(pCtxt, pFrame)) < 0) {
          return rc;
        }
      }

      for(idx = 0; idx < MOOF_AV_MAX; idx++) {
        if(pCtxt->mpdCtxt.active && pCtxt->stateAV[idx].path[0] != '\0') {

          VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - mp4moof_addFrame rename tmpfile [mp4idx:%d] '%s' -> '%s'"), 
                                    idx, pCtxt->stateAV[idx].pathtmp, pCtxt->stateAV[idx].path) );

          //
          // Rename the html/dash/...tmp to html/dash/...mp4
          //
          if(fileops_Rename(pCtxt->stateAV[idx].pathtmp, pCtxt->stateAV[idx].path) != 0) {
            LOG(X_ERROR("Failed to rename %s -> %s"), pCtxt->stateAV[idx].pathtmp, pCtxt->stateAV[idx].path);
          }
          //const char *_path = strutil_getFileName(pCtxt->stateAV[idx].path);
          //LOG(X_DEBUG("SYMLINK '%s' -> '%s'"), pCtxt->stateAV[idx].path2, _path);
          //symlink(_path, pCtxt->stateAV[idx].path2);

        }
      }

//LOG(X_DEBUG("______ active:%d, [0].pMp4: 0x%x, [1].path:'%s', [1].pMp4:0x%x, DURATION: %llu, %llu"), pCtxt->mpdCtxt.active, pCtxt->stateAV[0].pMp4,pCtxt->stateAV[1].path, pCtxt->stateAV[1].pMp4, pCtxtInt->duration, pCtxtInt->mdhd_duration);

      //
      // Update the mpd document context with the new media segment filepath
      //
      if(pCtxt->mpdCtxt.active && (pCtxt->stateAV[MOOF_AV_IDX_VID].pMp4 ||
                                  (pCtxt->stateAV[MOOF_AV_IDX_AUD].pMp4))) {

        VSX_DEBUG_DASH( 
        LOG(X_DEBUG("DASH - mp4moof_addFrame calling write MPD... mp4Sequence: %d, '%s', "
             "duration:%llu (%.3f sec), timescale:%uHz, ptsMp4Start:%llu (%.3f)"), 
              pCtxt->mp4Sequence, pCtxtInt->path, pCtxtInt->mdhd_duration, 
              ((float) pCtxtInt->mdhd_duration / pCtxtInt->mdhd_timescale), 
              pCtxtInt->mdhd_timescale, pCtxt->ptsMp4Start, PTSF(pCtxt->ptsMp4Start))
        );

        if((rc = update_mpd(pCtxt, pCtxt->moofInitCtxt.requestOutidx, mpdUpdates)) < 0) {
          LOG(X_ERROR("Failed to update DASH mpd"));
        }

      }

      if(pCtxt->moofInitCtxt.mp4MinDurationSec > 0 && ! OUTFMT_KEYFRAME(pFrame)) {
        LOG(X_WARNING("Creating mp4 segment which does not start on keyframe after %.1f sec"
                      ", try increasing 'dashmaxduration'"), 
            PTSF(OUTFMT_PTS(pFrame) - pCtxt->ptsMp4Start));
      }

      mp4moof_close_int(pCtxt);
      pCtxt->ptsMp4Start = OUTFMT_PTS(pFrame);
    }

  }

  //
  // Create the mp4 container if it does not currently exist
  //
  if(!pCtxtInt->pMp4) {
    VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - mp4moof_addFrame calling onNextMp4...")) );
    if((rc = onNextMp4(pCtxt, pFrame, mpdUpdates)) < 0) {
      return rc;
    }

  }

  //
  // Create the moof fragment box.  We are using seperate moof boxes for video and audio content
  //
  if((pFrame->isvid && !pCtxt->stateAV[MOOF_AV_IDX_VID].trak.moofTrak.pMoof) || 
     (pFrame->isaud && !pCtxt->stateAV[MOOF_AV_IDX_AUD].trak.moofTrak.pMoof)) {

    VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - mp4moof_addFrame calling onNewMoofFile... outidx:[%d] %s "
                                "len:%d, V.pMoof:0x%x, A.pMoof:0x%x"), 
                     pFrame->xout.outidx, pFrame->isvid ? "video" : "audio", OUTFMT_LEN(pFrame), 
                     pCtxt->stateAV[0].trak.moofTrak.pMoof, pCtxt->stateAV[1].trak.moofTrak.pMoof) );

    if((rc = onNewMoofFile(pCtxt, pFrame)) < 0) {
      return rc;
    }

  }

  VSX_DEBUG_DASH( LOG(X_DEBUGV("DASH - mp4moof_addFrame calling moof_addto_trun outidx:[%d] %s len:%d"), 
                      pFrame->xout.outidx, pFrame->isvid ? "video" : "audio", OUTFMT_LEN(pFrame)));

  //
  // Add the media frame to the fragmented mp4 (moof) and TRUN box data entry
  //
  if((rc = moof_addto_trun(pCtxtInt, pFrame)) < 0) {
    return -1;
  }

  return rc;
}

int mp4moof_addFrame(MP4_CREATE_STATE_MOOF_T *pCtxt, const OUTFMT_FRAME_DATA_T *pFrame) {
  MOOF_STATE_INT_T *pCtxtInt = NULL;
  unsigned int outidx;
  int do_new_m4s = 0;
  OUTFMT_FRAME_DATA_T frame;
  MPD_UPDATE_CTXT_T mpdUpdates[MOOF_AV_MAX];
 
  int rc = 0;

  if(!pCtxt || !pCtxt->pAvCtxt || !pFrame) {
    return -1;
  } else if(pCtxt->moofInitCtxt.syncedByOtherOutidx) {
    //
    // We're called by the q reader for an xcode outidx with output .m4s delimeters on the same
    // timestamp boundaries as all other xcode outidx threads, so we just rely on any init to be done
    // from the parent moofsrv_addFrame context, and then return here; mp4moof_addFrame_int will be invoked
    // from the main xcode outidx mp4moof_addFrame context
    //
    return rc;
  }

  pCtxtInt = &pCtxt->stateAV[MP4IDX(pFrame->isvid)];

  //
  // Check if we should be starting a new .m4s file. 
  // If using syncMultiOutIdx to sync all xcode outidx outputs at the sime timestamp boundary
  // then this check is only done on the main xcode outidx
  //
  if(pCtxtInt->pMp4 && !pCtxt->moofInitCtxt.syncedByOtherOutidx) {

    if(((pCtxt->moofInitCtxt.mp4MinDurationSec > 0 && (OUTFMT_PTS(pFrame) > pCtxt->ptsMp4Start) &&
        (OUTFMT_PTS(pFrame) - pCtxt->ptsMp4Start) > pCtxt->moofInitCtxt.mp4MinDurationSec * PTS_HZ_SECF &&
        OUTFMT_KEYFRAME(pFrame)) ||
        (pCtxt->moofInitCtxt.mp4MaxDurationSec > 0 && (OUTFMT_PTS(pFrame) > pCtxt->ptsMp4Start) &&
        (OUTFMT_PTS(pFrame) - pCtxt->ptsMp4Start) > pCtxt->moofInitCtxt.mp4MaxDurationSec * PTS_HZ_SECF))) {

      do_new_m4s = 1;
    }
  }

  //LOG(X_DEBUG("mp4moof_addFrame pCtxt: 0x%x, requestOutIdx: %d, syncedByOtherOutidx:%d, pts: %.3f"), pCtxt, pCtxt->moofInitCtxt.requestOutidx, pCtxt->moofInitCtxt.syncedByOtherOutidx, PTSF(OUTFMT_PTS(pFrame)));

  memset(mpdUpdates, 0, sizeof(mpdUpdates));
  if(pCtxt->moofInitCtxt.syncedByOtherOutidx) {
    return rc;
  } else {
    rc = mp4moof_addFrame_int(pCtxt, pFrame, do_new_m4s, mpdUpdates);
  }

  if(pCtxt->moofInitCtxt.syncMultiOutidx && pCtxt->moofInitCtxt.requestOutidx == 0) {

    //
    // Call any additional xcode output outidx instances which are using syncedByOtherOutidx
    //
    for(outidx = 1; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
      //LOG(X_DEBUG("moofsrv_addFrame pCtxt: 0x%x psharedCtxts[%d]: 0x%x, pts: %.3f"), pCtxt, outidx, pCtxt->moofInitCtxt.psharedCtxts[outidx], PTSF(OUTFMT_PTS(pFrame)););
      if(pCtxt->moofInitCtxt.psharedCtxts[outidx]) {
        if(pFrame->isvid) {
          memcpy(&frame, pFrame, sizeof(frame));
          frame.xout.outidx = outidx;
          pFrame = &frame;
        }

        //LOG(X_DEBUG("moofsrv_addFrame pCtxt: 0x%x calling moofsrv_addFrame_int for [outidx:%d] pts: %.3f ..."), pCtxt, outidx, PTSF(OUTFMT_PTS(pFrame)));
        rc = mp4moof_addFrame_int(pCtxt->moofInitCtxt.psharedCtxts[outidx], pFrame, do_new_m4s, NULL);
      }
    }

 }

  //
  // If we called update_mpd for an xcode output specific outidx, call it again to create a playlist
  // with multiple representations of each xcode outidx 
  //
  if(mpdUpdates[0].did_update) {
    mpdUpdates[0].no_init_state = mpdUpdates[1].no_init_state = 1;
    update_mpd(pCtxt, -1, mpdUpdates);
  }


  return rc;
}

#endif // (VSX_CREATE_CONTAINER)
