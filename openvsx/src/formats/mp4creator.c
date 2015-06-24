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

typedef MP4_MDAT_CONTENT_NODE_T *(*CB_GET_NEXT_SAMPLE)(void *, int, int);
typedef BOX_T *(*CB_CREATE_STSD_CHILD)(const BOX_HANDLER_T *pHandlerChain, void *pCbData);


typedef struct SAMPLE_CHUNK_DESCR {
  unsigned int sz_stss;
  uint32_t *p_stss;
  unsigned int sz_stsz;
  uint32_t *p_stsz;
  unsigned int sz_stsc;
  BOX_STSC_ENTRY_T *p_stsc;

  unsigned int sz_ctts;
  BOX_CTTS_ENTRY_T *p_ctts;

  CHUNK_SZ_T stchk;
  uint64_t szdata;

} SAMPLE_CHUNK_DESCR_T;

typedef struct MP4_TRAK_VID {
  MP4_TRAK_T               *pTrak;
  unsigned int              clockHz;
  unsigned int              frameDeltaHz;
  unsigned int              numSamples;
  unsigned int              frameIdLast;
  uint64_t                  durationHz;
  unsigned int              width;
  unsigned int              height;
  int                       lenSamplesHdr;
  CB_GET_NEXT_SAMPLE        cbGetNextSample;
  void                     *pCbData;
  CB_CREATE_STSD_CHILD      cbCreateStsdChild;
} MP4_TRAK_VID_T;


BOX_T *mp4_create_box_generic(const BOX_HANDLER_T *pHandlerChain, uint32_t type) {
  BOX_T *pBox;

  pBox = (BOX_T *) avc_calloc(1, sizeof(BOX_T));
  pBox->type = type;
  memcpy(pBox->name, &pBox->type, 4);
  pBox->handler = mp4_findHandlerInChain(pHandlerChain, type);
  pBox->szhdr = 8;
  pBox->szdata_ownbox = 0;
  pBox->szdata = pBox->szdata_ownbox;
  return pBox;
}

BOX_FTYP_T *mp4_create_startheader(const BOX_HANDLER_T *pHandlerChain,
                                   uint32_t name,
                                   uint32_t major,
                                   uint32_t compat1, 
                                   uint32_t compat2, 
                                   uint32_t compat3, 
                                   uint32_t compat4) {
  BOX_FTYP_T *pBoxFtyp;
  unsigned int compatIdx = 0;

  pBoxFtyp = (BOX_FTYP_T *) avc_calloc(1, sizeof(BOX_FTYP_T));

  pBoxFtyp->major = major;
  pBoxFtyp->minor = 0;
  pBoxFtyp->pcompatbrands = (uint32_t *) avc_calloc(1, 4 * sizeof(uint32_t));
  pBoxFtyp->lencompatbrands = 0;
  if(compat1 != 0) {
    pBoxFtyp->pcompatbrands[compatIdx++] = compat1;
  }
  if(compat2 != 0) {
    pBoxFtyp->pcompatbrands[compatIdx++] = compat2;
  }
  if(compat3 != 0) {
    pBoxFtyp->pcompatbrands[compatIdx++] = compat3;
  }
  if(compat4 != 0) {
    pBoxFtyp->pcompatbrands[compatIdx++] = compat4;
  }
  pBoxFtyp->lencompatbrands = (compatIdx * sizeof(uint32_t));

  pBoxFtyp->box.type = name;
  memcpy(pBoxFtyp->box.name, &pBoxFtyp->box.type, 4);
  pBoxFtyp->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxFtyp->box.type);
  pBoxFtyp->box.szhdr = 8;
  pBoxFtyp->box.szdata_ownbox = 8 + pBoxFtyp->lencompatbrands;
  pBoxFtyp->box.szdata = pBoxFtyp->box.szdata_ownbox;

  return pBoxFtyp;
}

static BOX_FTYP_T *create_box_ftyp(const BOX_HANDLER_T *pHandlerChain) {

  return mp4_create_startheader(pHandlerChain,
                                *((uint32_t *) "ftyp"),  // box name
                                *((uint32_t *) "mp42"),  // major name
                                *((uint32_t *) "mp42"),  // compatible brands list
                                *((uint32_t *) "isom"), 
                                0, 0);
}

#define MVHD_TIMESCALE_FAILOVER  3000

BOX_MVHD_T *mp4_create_box_mvhd(const BOX_HANDLER_T *pHandlerChain, uint64_t duration, 
                            uint64_t timescale, uint32_t next_trak_id,
                            uint64_t create_tm, uint64_t mod_tm) {
  BOX_MVHD_T *pBoxMvhd;
  uint64_t timescaleorig = timescale;

  pBoxMvhd = (BOX_MVHD_T *) avc_calloc(1, sizeof(BOX_MVHD_T));
 
  //
  // Quicktime seems to only play the file if mvhd Hz matches first track
  //
  if(duration > 0xffffffff || timescale > MVHD_TIMESCALE_FAILOVER) {
    timescale = MVHD_TIMESCALE_FAILOVER;
    duration = (uint64_t) (double)(duration / ((double)timescaleorig / 
                                   timescale));
    LOG(X_DEBUG("Setting mp4 clock to %lluHz, duration to: %llu"), timescale, duration);
  }

  pBoxMvhd->creattime = create_tm;
  pBoxMvhd->modificationtime = mod_tm;
  pBoxMvhd->timescale = (uint32_t) timescale;

  pBoxMvhd->duration = duration;
  pBoxMvhd->rate = htonl(0x00010000);
  pBoxMvhd->volume = htons(0x0100);
  pBoxMvhd->matrix[0] = htonl(0x00010000);
  pBoxMvhd->matrix[4] = htonl(0x00010000);
  pBoxMvhd->matrix[8] = htonl(0x40000000);
  pBoxMvhd->nexttrack = htonl(next_trak_id);
 
  pBoxMvhd->fullbox.flags[2] = 0x00;

  pBoxMvhd->box.type = *((uint32_t *) "mvhd");
  memcpy(pBoxMvhd->box.name, &pBoxMvhd->box.type, 4);
  pBoxMvhd->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxMvhd->box.type);
  pBoxMvhd->box.szhdr = 8;
  pBoxMvhd->box.szdata_ownbox = 100;
  pBoxMvhd->box.szdata = pBoxMvhd->box.szdata_ownbox;

  return pBoxMvhd;
}

BOX_TKHD_T *mp4_create_box_tkhd(const BOX_HANDLER_T *pHandlerChain, uint64_t duration, 
                                uint32_t id, uint32_t pic_width, uint32_t pic_height,
                                uint64_t create_tm, uint64_t mod_tm, uint16_t volume) {
  BOX_TKHD_T *pBoxTkhd;

  pBoxTkhd = (BOX_TKHD_T *) avc_calloc(1, sizeof(BOX_TKHD_T));

  pBoxTkhd->creattime = create_tm;
  pBoxTkhd->modificationtime = mod_tm;
  pBoxTkhd->trackid = id;
  pBoxTkhd->duration = duration;
  pBoxTkhd->volume = htons(volume);
  pBoxTkhd->matrix[0] = htonl(0x00010000);
  pBoxTkhd->matrix[4] = htonl(0x00010000);
  pBoxTkhd->matrix[8] = htonl(0x40000000);
  pBoxTkhd->width = htonl(pic_width << 16);
  pBoxTkhd->height = htonl(pic_height << 16);

  pBoxTkhd->fullbox.flags[2] = 0x01;

  pBoxTkhd->box.type = *((uint32_t *) "tkhd");
  memcpy(pBoxTkhd->box.name, &pBoxTkhd->box.type, 4);
  pBoxTkhd->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxTkhd->box.type);
  pBoxTkhd->box.szhdr = 8;
  pBoxTkhd->box.szdata_ownbox = 84;
  pBoxTkhd->box.szdata = pBoxTkhd->box.szdata_ownbox;

  return pBoxTkhd;
}

BOX_MDHD_T *mp4_create_box_mdhd(const BOX_HANDLER_T *pHandlerChain, 
                                uint64_t duration, uint64_t timescale) {
  BOX_MDHD_T *pBoxMdhd;

  pBoxMdhd = (BOX_MDHD_T *) avc_calloc(1, sizeof(BOX_MDHD_T));

  pBoxMdhd->creattime = 0;
  pBoxMdhd->modificationtime = 0;
  pBoxMdhd->timescale = (uint32_t) timescale;
  pBoxMdhd->duration = duration;
  pBoxMdhd->language = 0;
  pBoxMdhd->predefined = 0;

  pBoxMdhd->fullbox.flags[2] = 0x00;

  pBoxMdhd->box.type = *((uint32_t *) "mdhd");
  memcpy(pBoxMdhd->box.name, &pBoxMdhd->box.type, 4);
  pBoxMdhd->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxMdhd->box.type);
  pBoxMdhd->box.szhdr = 8;
  pBoxMdhd->box.szdata_ownbox = 24;
  pBoxMdhd->box.szdata = pBoxMdhd->box.szdata_ownbox;

  return pBoxMdhd;
}

BOX_HDLR_T *mp4_create_box_hdlr(const BOX_HANDLER_T *pHandlerChain, uint32_t type, const char *name) {
  BOX_HDLR_T *pBoxHdlr;
  int lenName = 0;

  pBoxHdlr = (BOX_HDLR_T *) avc_calloc(1, sizeof(BOX_HDLR_T));

  pBoxHdlr->predefined = 0;
  pBoxHdlr->handlertype = type;

  if(name) {
    if((lenName = strlen(name)) >= sizeof(pBoxHdlr->name)) {
      lenName = sizeof(pBoxHdlr->name);
    }
    strncpy(pBoxHdlr->name, name, lenName++);
  }

  pBoxHdlr->fullbox.flags[2] = 0x00;

  pBoxHdlr->box.type = *((uint32_t *) "hdlr");
  memcpy(pBoxHdlr->box.name, &pBoxHdlr->box.type, 4);
  pBoxHdlr->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxHdlr->box.type);
  pBoxHdlr->box.szhdr = 8;
  pBoxHdlr->box.szdata_ownbox = 24 + lenName;
  pBoxHdlr->box.szdata = pBoxHdlr->box.szdata_ownbox;

  return pBoxHdlr;
}

BOX_VMHD_T *mp4_create_box_vmhd(const BOX_HANDLER_T *pHandlerChain) {
  BOX_VMHD_T *pBoxVmhd;

  pBoxVmhd = (BOX_VMHD_T *) avc_calloc(1, sizeof(BOX_VMHD_T));

  pBoxVmhd->graphicsmode = htons(0);
  pBoxVmhd->opcolor[0] = 0;

  pBoxVmhd->fullbox.flags[2] = 0x01;

  pBoxVmhd->box.type = *((uint32_t *) "vmhd");
  memcpy(pBoxVmhd->box.name, &pBoxVmhd->box.type, 4);
  pBoxVmhd->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxVmhd->box.type);
  pBoxVmhd->box.szhdr = 8;
  pBoxVmhd->box.szdata_ownbox = 12;
  pBoxVmhd->box.szdata = pBoxVmhd->box.szdata_ownbox;

  return pBoxVmhd;
}

BOX_SMHD_T *mp4_create_box_smhd(const BOX_HANDLER_T *pHandlerChain) {
  BOX_SMHD_T *pBoxSmhd;

  pBoxSmhd = (BOX_SMHD_T *) avc_calloc(1, sizeof(BOX_SMHD_T));

  pBoxSmhd->balance = 0;

  pBoxSmhd->fullbox.flags[2] = 0x00;

  pBoxSmhd->box.type = *((uint32_t *) "smhd");
  memcpy(pBoxSmhd->box.name, &pBoxSmhd->box.type, 4);
  pBoxSmhd->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxSmhd->box.type);
  pBoxSmhd->box.szhdr = 8;
  pBoxSmhd->box.szdata_ownbox = 8;
  pBoxSmhd->box.szdata = pBoxSmhd->box.szdata_ownbox;

  return pBoxSmhd;
}

BOX_DREF_ENTRY_T *mp4_create_box_dref_url(const BOX_HANDLER_T *pHandlerChain, const char *url) {
  BOX_DREF_ENTRY_T *pBoxDrefUrl;
  int lenUrl = 0;

  pBoxDrefUrl = (BOX_DREF_ENTRY_T *) avc_calloc(1, sizeof(BOX_DREF_ENTRY_T));

  pBoxDrefUrl->fullbox.flags[2] = 0x01;
  if(url && url[0] != '\0') {
    strncpy(pBoxDrefUrl->name, url, sizeof(pBoxDrefUrl->name) - 1);
    lenUrl = strlen(pBoxDrefUrl->name) + 1;
  }

  pBoxDrefUrl->box.type = *((uint32_t *) "url ");
  memcpy(pBoxDrefUrl->box.name, &pBoxDrefUrl->box.type, 4);
  pBoxDrefUrl->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxDrefUrl->box.type);
  pBoxDrefUrl->box.szhdr = 8;
  pBoxDrefUrl->box.szdata_ownbox = 4 + lenUrl;
  pBoxDrefUrl->box.szdata = pBoxDrefUrl->box.szdata_ownbox;
  
  return pBoxDrefUrl;
}

BOX_DREF_T *mp4_create_box_dref(const BOX_HANDLER_T *pHandlerChain, BOX_DREF_ENTRY_T *pEntry) {
  BOX_DREF_T *pBoxDref;

  pBoxDref = (BOX_DREF_T *) avc_calloc(1, sizeof(BOX_DREF_T));

  pBoxDref->fullbox.flags[2] = 0x00;

  if(pEntry) {
    pBoxDref->entrycnt = 1;
    pBoxDref->box.child = (BOX_T *) pEntry;
  }

  pBoxDref->box.type = *((uint32_t *) "dref");
  memcpy(pBoxDref->box.name, &pBoxDref->box.type, 4);
  pBoxDref->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxDref->box.type);
  pBoxDref->box.szhdr = 8;
  pBoxDref->box.szdata_ownbox = 8;
  pBoxDref->box.szdata = pBoxDref->box.szdata_ownbox;

  return pBoxDref;
}

BOX_STSD_T *mp4_create_box_stsd(const BOX_HANDLER_T *pHandlerChain) {
  BOX_STSD_T *pBoxStsd;

  pBoxStsd = (BOX_STSD_T *) avc_calloc(1, sizeof(BOX_STSD_T));

  pBoxStsd->entrycnt = 1;

  pBoxStsd->fullbox.flags[2] = 0x00;

  pBoxStsd->box.type = *((uint32_t *) "stsd");
  memcpy(pBoxStsd->box.name, &pBoxStsd->box.type, 4);
  pBoxStsd->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxStsd->box.type);
  pBoxStsd->box.szhdr = 8;
  pBoxStsd->box.szdata_ownbox = 8;
  pBoxStsd->box.szdata = pBoxStsd->box.szdata_ownbox;

  return pBoxStsd;
}

BOX_MP4A_T *mp4_create_box_mp4a(const BOX_HANDLER_T *pHandlerChain, const ESDS_DECODER_CFG_T *pSp) {
  BOX_MP4A_T *pBoxMp4a;
  uint32_t freq;
  pBoxMp4a = (BOX_MP4A_T *) avc_calloc(1, sizeof(BOX_MP4A_T));
  

  pBoxMp4a->datarefidx = htons(0x01);
  if(pSp->channelIdx < 7) {
    pBoxMp4a->channelcnt = htons(pSp->channelIdx);
  } else if(pBoxMp4a->channelcnt == 7) {
    pBoxMp4a->channelcnt = htons(8);
  } else {
    LOG(X_ERROR("Invalid channel idx: %u"), pSp->channelIdx);
    return NULL;
  }
  // TODO: hardcoded... how to obtain right value?
  pBoxMp4a->samplesize = htons(16); 
  if(ESDS_FREQ_IDX(*pSp) > 12) {
    LOG(X_ERROR("Unsupported frequency idx: %u"), ESDS_FREQ_IDX(*pSp));
    return NULL;
  }
  if((freq = esds_getFrequencyVal(ESDS_FREQ_IDX(*pSp))) <= 0xffff) {
    freq <<= 16;
  }
  pBoxMp4a->timescale = htonl((freq));

  pBoxMp4a->box.type = *((uint32_t *) "mp4a");
  memcpy(pBoxMp4a->box.name, &pBoxMp4a->box.type, 4);
  pBoxMp4a->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxMp4a->box.type);
  pBoxMp4a->box.szhdr = 8;
  pBoxMp4a->box.szdata_ownbox = 28;
  pBoxMp4a->box.szdata = pBoxMp4a->box.szdata_ownbox;

  return pBoxMp4a;
}

int mp4_create_box_esds_decoderspecific(BOX_ESDS_DESCR_DECODERSPECIFIC_T *pDecoderSp, 
                                        const ESDS_DECODER_CFG_T *pSp) {
  int rc;

  memset(pDecoderSp, 0, sizeof(pDecoderSp));
  pDecoderSp->type.descriptor = 0x05;

  if((rc = esds_encodeDecoderSp(pDecoderSp->startcodes, 
                                sizeof(pDecoderSp->startcodes), pSp)) < 0) {
    return -1;
  }
  pDecoderSp->type.length = (unsigned int) rc;

  return 0;
}

BOX_ESDS_T *mp4_create_box_esds(const BOX_HANDLER_T *pHandlerChain, 
                            uint8_t objtypeid,
                            uint8_t streamtype,
                            uint32_t avgbitrate,
                            uint32_t maxbitrate,
                            uint32_t largestframe,
                            BOX_ESDS_DESCR_DECODERSPECIFIC_T *pSp) {

  BOX_ESDS_T *pBoxEsds;
  unsigned int ui;

  pBoxEsds = (BOX_ESDS_T *) avc_calloc(1, sizeof(BOX_ESDS_T));

  pBoxEsds->fullbox.flags[2] = 0x00;

  pBoxEsds->sl.type.descriptor = 0x06;
  pBoxEsds->sl.type.length = 1;
  pBoxEsds->sl.val[0] = 0x02;

  if(pSp) {
    memcpy(&pBoxEsds->specific, pSp, sizeof(pBoxEsds->specific)); 
  }
  //create_box_esds_decoderspecific(&pBoxEsds->specific, &pAac->decoderSP);
 
  pBoxEsds->config.type.descriptor = 0x04;
  pBoxEsds->config.type.length = (pBoxEsds->specific.type.length + 2) + 13;
  pBoxEsds->config.objtypeid = objtypeid;
  pBoxEsds->config.streamtype = (streamtype << 2) | (0 << 1) | 1;

  ui = htonl(largestframe);
  pBoxEsds->config.bufsize[0] = ui >> 8;
  pBoxEsds->config.bufsize[1] = ui >> 16;
  pBoxEsds->config.bufsize[2] = ui >> 24;
  pBoxEsds->config.avgbitrate = htonl(avgbitrate);
  pBoxEsds->config.maxbitrate = htonl(maxbitrate);

  pBoxEsds->estype.type.descriptor = 0x03;
  pBoxEsds->estype.type.length = pBoxEsds->config.type.length + 5 + (pBoxEsds->sl.type.length + 2);
  pBoxEsds->estype.id = 0;
  pBoxEsds->estype.priority = 0;

  pBoxEsds->box.type = *((uint32_t *) "esds");
  memcpy(pBoxEsds->box.name, &pBoxEsds->box.type, 4);
  pBoxEsds->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxEsds->box.type);
  pBoxEsds->box.szhdr = 8;
  pBoxEsds->box.szdata_ownbox = 6 + pBoxEsds->estype.type.length;
  pBoxEsds->box.szdata = pBoxEsds->box.szdata_ownbox;

  return pBoxEsds;
}

static BOX_SAMR_T *create_box_samr(const BOX_HANDLER_T *pHandlerChain, unsigned int clockHz) {
  BOX_SAMR_T *pBoxSamr;

  pBoxSamr = (BOX_SAMR_T *) avc_calloc(1, sizeof(BOX_SAMR_T));

  pBoxSamr->datarefidx = htons(0x01);
  pBoxSamr->timescale = htons(clockHz);

  pBoxSamr->box.type = *((uint32_t *) "samr");
  memcpy(pBoxSamr->box.name, &pBoxSamr->box.type, 4);
  pBoxSamr->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxSamr->box.type);
  pBoxSamr->box.szhdr = 8;
  pBoxSamr->box.szdata_ownbox = 8;
  pBoxSamr->box.szdata = pBoxSamr->box.szdata_ownbox;

  return pBoxSamr;
}

static BOX_DAMR_T *create_box_damr(const BOX_HANDLER_T *pHandlerChain) {
  BOX_DAMR_T *pBoxDamr;

  pBoxDamr = (BOX_DAMR_T *) avc_calloc(1, sizeof(BOX_DAMR_T));

  pBoxDamr->vendor = *((uint32_t *) VSX_3GP_DAMR_VENDOR);
  pBoxDamr->decoderver = 0x00;
  pBoxDamr->modeset = htons(0x81ff);
  pBoxDamr->modechangeperiod = 0x00;
  pBoxDamr->framespersample = 0x01;

  pBoxDamr->box.type = *((uint32_t *) "damr");
  memcpy(pBoxDamr->box.name, &pBoxDamr->box.type, 4);
  pBoxDamr->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxDamr->box.type);
  pBoxDamr->box.szhdr = 8;
  pBoxDamr->box.szdata_ownbox = 8;
  pBoxDamr->box.szdata = pBoxDamr->box.szdata_ownbox;

  return pBoxDamr;
}


BOX_AVCC_T *mp4_create_box_avcC(const BOX_HANDLER_T *pHandlerChain,
                            const unsigned char *psps, unsigned int len_sps,
                            const unsigned char *ppps, unsigned int len_pps) {
  BOX_AVCC_T *pBoxAvcC;
  int len = 7;

  pBoxAvcC = (BOX_AVCC_T *) avc_calloc(1, sizeof(BOX_AVCC_T));

  pBoxAvcC->avcc.configversion = 0x01;
  pBoxAvcC->avcc.avcprofile = psps[1];
  pBoxAvcC->avcc.profilecompatibility = psps[2]; // ?? not sure about compatibility
  pBoxAvcC->avcc.avclevel = psps[3];
  pBoxAvcC->avcc.lenminusone = 3;

  if(len_sps > 0) {
    pBoxAvcC->avcc.numsps = 1;
    pBoxAvcC->avcc.psps = (AVC_DECODER_CFG_BLOB_T *) avc_calloc(1, sizeof(AVC_DECODER_CFG_BLOB_T));
    pBoxAvcC->avcc.psps->len = len_sps;
    pBoxAvcC->avcc.psps->data = (unsigned char *) avc_calloc(1, len_sps);
    memcpy(pBoxAvcC->avcc.psps->data, psps, len_sps);
    len += (2 + len_sps);
  }

  if(len_pps > 0) {
    pBoxAvcC->avcc.numpps = 1;
    pBoxAvcC->avcc.ppps = (AVC_DECODER_CFG_BLOB_T *) avc_calloc(1, sizeof(AVC_DECODER_CFG_BLOB_T));
    pBoxAvcC->avcc.ppps->len = len_pps;
    pBoxAvcC->avcc.ppps->data = (unsigned char *) avc_calloc(1, len_pps);
    memcpy(pBoxAvcC->avcc.ppps->data, ppps, len_pps);
    len += (2 + len_pps);
  }

  pBoxAvcC->box.type = *((uint32_t *) "avcC");
  memcpy(pBoxAvcC->box.name, &pBoxAvcC->box.type, 4);
  pBoxAvcC->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxAvcC->box.type);
  pBoxAvcC->box.szhdr = 8;
  pBoxAvcC->box.szdata_ownbox = len;
  pBoxAvcC->box.szdata = pBoxAvcC->box.szdata_ownbox;

  return pBoxAvcC;
}

BOX_AVC1_T *mp4_create_box_avc1(const BOX_HANDLER_T *pHandlerChain, uint16_t drefIdx, 
                            uint32_t pic_width, uint32_t pic_height) {
  BOX_AVC1_T *pBoxAvc1;

  pBoxAvc1 = (BOX_AVC1_T *) avc_calloc(1, sizeof(BOX_AVC1_T));

  pBoxAvc1->datarefidx = htons(drefIdx);
  pBoxAvc1->width = htons(pic_width);
  pBoxAvc1->height = htons(pic_height);
  pBoxAvc1->horizresolution = htonl(0x00480000);
  pBoxAvc1->vertresolution = htonl(0x00480000);
  pBoxAvc1->framecnt = htons(1);
  strncpy((char *) pBoxAvc1->compressorname, "AVC Coding", 
          sizeof(pBoxAvc1->compressorname) - 1);
  pBoxAvc1->depth = htons(0x0018);
  pBoxAvc1->predefined3 = htons(0xffff);

  pBoxAvc1->box.type = *((uint32_t *) "avc1");
  memcpy(pBoxAvc1->box.name, &pBoxAvc1->box.type, 4);
  pBoxAvc1->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxAvc1->box.type);
  pBoxAvc1->box.szhdr = 8;
  pBoxAvc1->box.szdata_ownbox = 78;
  pBoxAvc1->box.szdata = pBoxAvc1->box.szdata_ownbox;

  return pBoxAvc1;
}

static BOX_MP4V_T *create_box_mp4v(const BOX_HANDLER_T *pHandlerChain, uint16_t drefIdx, 
                            uint32_t pic_width, uint32_t pic_height) {
  BOX_MP4V_T *pBoxMp4v;

  pBoxMp4v = (BOX_MP4V_T *) avc_calloc(1, sizeof(BOX_MP4V_T));

  pBoxMp4v->datarefidx = htons(drefIdx);
  pBoxMp4v->wid = htons(pic_width);
  pBoxMp4v->ht = htons(pic_height);
  pBoxMp4v->horizresolution = htonl(0x00480000);
  pBoxMp4v->vertresolution = htonl(0x00480000);
  pBoxMp4v->datasz = 0;
  pBoxMp4v->framesz = htons(1);
  strncpy((char *) pBoxMp4v->codername, "", sizeof(pBoxMp4v->codername) - 1);
  pBoxMp4v->reserved3 = htons(0x0018);
  pBoxMp4v->reserved4 = 0xffff;

  pBoxMp4v->box.type = *((uint32_t *) "mp4v");
  memcpy(pBoxMp4v->box.name, &pBoxMp4v->box.type, 4);
  pBoxMp4v->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxMp4v->box.type);
  pBoxMp4v->box.szhdr = 8;
  pBoxMp4v->box.szdata_ownbox = 78;
  pBoxMp4v->box.szdata = pBoxMp4v->box.szdata_ownbox;

  return pBoxMp4v;
}

static int parse_avsync_line(const char *line, BOX_STTS_ENTRY_T *pSttsEntry) {
  const char *p = line;
  const char *p2;
  const char *pval;
  unsigned int kvidx = 0;
  KEYVAL_PAIR_T kv;

  memset(pSttsEntry, 0, sizeof(BOX_STTS_ENTRY_T));

  while(*p == ' ') {
    p++;
  }

  if(*p == '#') {
    return 0;
  }

  p2 = p;
  while(*p2 != '\0') {

    if(*p2 == ',' || *p2 == '\r' || *p2 == '\n') {

      if(conf_parse_keyval(&kv, p, p2 - p, '=', 0) < 0) {
        return -1; 
      }

      if((pval = conf_find_keyval(&kv, "stts"))) {
        pSttsEntry->samplecnt = atoi(pval);
        return 0; 
      } else if((pval = conf_find_keyval(&kv, "clock"))) {
        pSttsEntry->samplecnt = atoi(pval);
        return 1; 
      } else if((pval = conf_find_keyval(&kv, "count"))) {
        pSttsEntry->samplecnt = atoi(pval);
        kvidx |= 1;
      } else if((pval = conf_find_keyval(&kv, "delta"))) {
        pSttsEntry->sampledelta = atoi(pval);
        kvidx |= 2;
      }

      while(*p2 == ',') {
        p2++;
      }
  
      if(*p2 == '\r' || *p2 == '\n') {
        break;
      }
      
      p = p2;
    } 
    p2++;    
  }

  if(kvidx == 3) {
    return 2;
  } else {
    memset(pSttsEntry, 0, sizeof(BOX_STTS_ENTRY_T));
    return 0;
  }
}

BOX_STTS_ENTRY_LIST_T *mp4_avsync_readsttsfile(const char *path) {
  FILE_HANDLE fp;
  char buf[1024];
  BOX_STTS_ENTRY_T stts;
  BOX_STTS_ENTRY_LIST_T *pList = NULL;
  unsigned int idxEntry = 0;
  uint32_t clockHz = 0;
  uint64_t totsamplesduration = 0;
  int linenum=1;
  int rc;

  if((fp = fileops_Open(path, O_RDONLY)) == FILEOPS_INVALID_FP) {
    LOG(X_CRITICAL("Unable to open file for reading: %s"), path);
    return NULL;
  }

  pList = (BOX_STTS_ENTRY_LIST_T *) avc_calloc(1, sizeof(BOX_STTS_ENTRY_LIST_T));

  while(fileops_fgets(buf, sizeof(buf) - 1, fp)) {

    if((rc = parse_avsync_line(buf, &stts)) < 0) {
      LOG(X_ERROR("Failed to parse %s at line:%d"), path, linenum);
      return NULL;
    } else if(rc == 0 && pList->pEntries == NULL && stts.samplecnt > 0) {
      pList->pEntries = (BOX_STTS_ENTRY_T *) avc_calloc(stts.samplecnt, 
                                     sizeof(BOX_STTS_ENTRY_T));
      pList->entrycnt = stts.samplecnt;
    } else if(rc == 1) {
      clockHz = stts.samplecnt;
    } else if(rc > 1 && idxEntry < pList->entrycnt && pList->pEntries) {
      memcpy(&pList->pEntries[idxEntry++], &stts, sizeof(BOX_STTS_ENTRY_T));
      totsamplesduration += (stts.samplecnt * stts.sampledelta);
      //fprintf(stdout, "stts: %lld  (%d x %d)\n", totsamplesduration, stts.samplecnt, stts.sampledelta);
    }
    linenum++;
  }

  fileops_Close(fp);

  if(!pList->pEntries || pList->entrycnt == 0) {
    LOG(X_ERROR("%s does not contain enough sample decoding times"), path); 
    free(pList);
    return NULL;
  }

  pList->clockHz = clockHz;
  pList->totsamplesduration = totsamplesduration;

  LOG(X_DEBUG("Read %u sample decoding times from %s (%lld/%dHz)"), 
            pList->entrycnt, path, totsamplesduration, clockHz);

  return pList;
}

static BOX_STTS_T *create_box_stts(const BOX_HANDLER_T *pHandlerChain, uint32_t samples, uint32_t delta,
                            const BOX_STTS_ENTRY_LIST_T *pAVSync) {
  BOX_STTS_T *pBoxStts;

  pBoxStts = (BOX_STTS_T *) avc_calloc(1, sizeof(BOX_STTS_T));
    
  if(pAVSync) {
    pBoxStts->list.entrycnt = pAVSync->entrycnt;
  }

  if(pBoxStts->list.entrycnt == 0) {
    pBoxStts->list.entrycnt = 1;
  }

  pBoxStts->list.pEntries = (BOX_STTS_ENTRY_T *) avc_calloc(pBoxStts->list.entrycnt, 
                                     sizeof(BOX_STTS_ENTRY_T));
  if(pAVSync && pAVSync->pEntries) {
    memcpy(pBoxStts->list.pEntries, pAVSync->pEntries, 
                   pBoxStts->list.entrycnt * sizeof(BOX_STTS_ENTRY_T));
  } else {
    pBoxStts->list.pEntries[0].samplecnt = samples;
    pBoxStts->list.pEntries[0].sampledelta = delta;
  }
  
  pBoxStts->fullbox.flags[2] = 0x00;

  pBoxStts->box.type = *((uint32_t *) "stts");
  memcpy(pBoxStts->box.name, &pBoxStts->box.type, 4);
  pBoxStts->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxStts->box.type);
  pBoxStts->box.szhdr = 8;
  pBoxStts->box.szdata_ownbox = 8 + (pBoxStts->list.entrycnt * 8);
  pBoxStts->box.szdata = pBoxStts->box.szdata_ownbox;

  return pBoxStts;
}

BOX_STSZ_T *mp4_create_box_stsz(const BOX_HANDLER_T *pHandlerChain, uint32_t numEntries, uint32_t *pEntries) {
  BOX_STSZ_T *pBoxStsz;

  pBoxStsz = (BOX_STSZ_T *) avc_calloc(1, sizeof(BOX_STSZ_T));
  pBoxStsz->samplesize = 0;
  pBoxStsz->samplecount = numEntries;
  pBoxStsz->pSamples = pEntries;

  pBoxStsz->fullbox.flags[2] = 0x00;

  pBoxStsz->box.type = *((uint32_t *) "stsz");
  memcpy(pBoxStsz->box.name, &pBoxStsz->box.type, 4);
  pBoxStsz->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxStsz->box.type);
  pBoxStsz->box.szhdr = 8;
  pBoxStsz->box.szdata_ownbox = 12 + (pBoxStsz->samplecount * sizeof(uint32_t));
  pBoxStsz->box.szdata = pBoxStsz->box.szdata_ownbox;

  return pBoxStsz;
}


BOX_STCO_T *mp4_create_box_stco(const BOX_HANDLER_T *pHandlerChain, uint32_t numEntries, uint32_t *pEntries) {
  BOX_STCO_T *pBoxStco;

  pBoxStco = (BOX_STCO_T *) avc_calloc(1, sizeof(BOX_STCO_T));
  pBoxStco->entrycnt = numEntries;
  pBoxStco->pSamples = pEntries;

  pBoxStco->fullbox.flags[2] = 0x00;

  pBoxStco->box.type = *((uint32_t *) "stco");
  memcpy(pBoxStco->box.name, &pBoxStco->box.type, 4);
  pBoxStco->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxStco->box.type);
  pBoxStco->box.szhdr = 8;
  pBoxStco->box.szdata_ownbox = 8 + (pBoxStco->entrycnt * sizeof(uint32_t));
  pBoxStco->box.szdata = pBoxStco->box.szdata_ownbox;

  return pBoxStco;
}

BOX_STSS_T *mp4_create_box_stss(const BOX_HANDLER_T *pHandlerChain, uint32_t numEntries, uint32_t *pEntries) {
  BOX_STSS_T *pBoxStss;

  pBoxStss = (BOX_STCO_T *) avc_calloc(1, sizeof(BOX_STSS_T));
  pBoxStss->entrycnt = numEntries;
  pBoxStss->pSamples = pEntries;

  pBoxStss->fullbox.flags[2] = 0x00;

  pBoxStss->box.type = *((uint32_t *) "stss");
  memcpy(pBoxStss->box.name, &pBoxStss->box.type, 4);
  pBoxStss->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxStss->box.type);
  pBoxStss->box.szhdr = 8;
  pBoxStss->box.szdata_ownbox = 8 + (pBoxStss->entrycnt * sizeof(uint32_t));
  pBoxStss->box.szdata = pBoxStss->box.szdata_ownbox;

  return pBoxStss;
}

BOX_STSC_T *mp4_create_box_stsc(const BOX_HANDLER_T *pHandlerChain, 
                           uint32_t numEntries, BOX_STSC_ENTRY_T *pEntries) {
  BOX_STSC_T *pBoxStsc;

  pBoxStsc = (BOX_STSC_T *) avc_calloc(1, sizeof(BOX_STSC_T));
  pBoxStsc->entrycnt = numEntries;
  pBoxStsc->pEntries = pEntries;

  pBoxStsc->fullbox.flags[2] = 0x00;

  pBoxStsc->box.type = *((uint32_t *) "stsc");
  memcpy(pBoxStsc->box.name, &pBoxStsc->box.type, 4);
  pBoxStsc->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxStsc->box.type);
  pBoxStsc->box.szhdr = 8;
  pBoxStsc->box.szdata_ownbox = 8 + (pBoxStsc->entrycnt * sizeof(BOX_STSC_ENTRY_T));
  pBoxStsc->box.szdata = pBoxStsc->box.szdata_ownbox;

  return pBoxStsc;
}

static BOX_CTTS_T *create_box_ctts(const BOX_HANDLER_T *pHandlerChain, 
                            uint32_t numEntries, BOX_CTTS_ENTRY_T *pEntries) {
  BOX_CTTS_T *pBoxCtts;

  pBoxCtts = (BOX_CTTS_T *) avc_calloc(1, sizeof(BOX_CTTS_T));
  pBoxCtts->entrycnt = numEntries;
  pBoxCtts->pEntries = pEntries;

  pBoxCtts->fullbox.flags[2] = 0x00;

  pBoxCtts->box.type = *((uint32_t *) "ctts");
  memcpy(pBoxCtts->box.name, &pBoxCtts->box.type, 4);
  pBoxCtts->box.handler = mp4_findHandlerInChain(pHandlerChain, pBoxCtts->box.type);
  pBoxCtts->box.szhdr = 8;
  pBoxCtts->box.szdata_ownbox = 8 + (pBoxCtts->entrycnt * sizeof(BOX_CTTS_ENTRY_T));
  pBoxCtts->box.szdata = pBoxCtts->box.szdata_ownbox;

  return pBoxCtts;
}


static int update_stco(BOX_STCO_T *pBoxStco, uint64_t startOffset, CHUNK_SZ_T *pStchk) {
  unsigned int idx;
  uint32_t offset = (uint32_t) startOffset;

  //TODO: this needs to be updated for large mp4

  if(pBoxStco->entrycnt != pStchk->stchk_sz) {
    LOG(X_ERROR("box stco entry cnt: %d does not match parameter %d"), 
                pBoxStco->entrycnt, pStchk->stchk_sz);
    return -1;
  }

  for(idx = 0; idx < pBoxStco->entrycnt; idx++) {
    if(idx > 0) {
      offset += pStchk->p_stchk[idx -1].sz;
    }
    pBoxStco->pSamples[idx] = (uint32_t) offset;
    
  }

  return 0;
}

static int compute_chunks(CB_GET_NEXT_SAMPLE cbGetNextSample,
                          void *pCbUserData, 
                          unsigned int clockHz,
                          unsigned int frameDeltaHz,
                          SAMPLE_CHUNK_DESCR_T *pDescr, 
                          uint32_t stcoIdxOnStsd, 
                          int lenhdr) {
  unsigned int idxPass;
  unsigned int idxFrame;
  unsigned int idxChunk;
  unsigned int szFrame;
  unsigned int szChunk;
  unsigned int sec;
  unsigned int framesInChunk;
  unsigned int framesInPrevChunk;
  uint64_t hz;
  uint32_t lastFrameId;
  double secondsInChunk = 1.0;
  int8_t lowestDecodeOffset = 0;
  int8_t prevDecodeOffset;
  MP4_MDAT_CONTENT_NODE_T *pContent = NULL;

  memset(pDescr, 0, sizeof(SAMPLE_CHUNK_DESCR_T));

  for(idxPass = 0; idxPass < 2; idxPass++) {

    if(!(pContent = cbGetNextSample(pCbUserData, 0, 1))) {
      return -1;
    }

    idxFrame = 0;
    idxChunk = 0;
    szFrame = 0;
    szChunk = 0;
    sec = 1;
    framesInChunk = 1;
    framesInPrevChunk = 0;
    hz = 0;
    lastFrameId = pContent->frameId;
    prevDecodeOffset = 0;
    pDescr->stchk.stchk_sz = 0;
    pDescr->sz_stsc = 0;
    pDescr->sz_ctts = 0;
    pDescr->sz_stss = 0;
    pDescr->sz_stsz = 0;
    pDescr->szdata = 0;

    if(pContent->flags & MP4_MDAT_CONTENT_FLAG_SYNCSAMPLE) {
      if(pDescr->p_stss) {
        pDescr->p_stss[pDescr->sz_stss++] = idxFrame + 1;
      } else {
        pDescr->sz_stss++;
      }
    }

    if(pDescr->p_ctts) {
      pDescr->p_ctts[0].samplecount=1;
      pDescr->p_ctts[0].sampleoffset = (pContent->decodeOffset - lowestDecodeOffset) *
                                       frameDeltaHz;
    }
    if(pContent) {
      prevDecodeOffset = pContent->decodeOffset;
    }
    
    while(pContent) {

      if(pContent->frameId != lastFrameId) {   

        //
        // set STSZ
        //
        if(pDescr->p_stsz) {
          pDescr->p_stsz[pDescr->sz_stsz++] = szFrame;
        } else {
          pDescr->sz_stsz++;
        }
  
        //
        // set STSS
        // 
        if(pContent->flags & MP4_MDAT_CONTENT_FLAG_SYNCSAMPLE) {
          if(pDescr->p_stss) {
            pDescr->p_stss[pDescr->sz_stss++] = idxFrame + 1;
          } else {
            pDescr->sz_stss++;
          }
        }

        //
        // set CTTS
        //
        if(pContent->decodeOffset < lowestDecodeOffset) {
          lowestDecodeOffset = pContent->decodeOffset;
        }
        if(pContent->decodeOffset != prevDecodeOffset) {
          pDescr->sz_ctts++;
          if(pDescr->p_ctts) {
            pDescr->p_ctts[pDescr->sz_ctts].samplecount = 1;
            pDescr->p_ctts[pDescr->sz_ctts].sampleoffset = (pContent->decodeOffset - 
                                                  lowestDecodeOffset) * frameDeltaHz;
          }
          prevDecodeOffset = pContent->decodeOffset;
        } else if(pDescr->p_ctts) {
          pDescr->p_ctts[pDescr->sz_ctts].samplecount++;
        }

        szChunk += szFrame;

        if((hz += frameDeltaHz) >  secondsInChunk * (sec * clockHz)) {

          if(pDescr->stchk.p_stchk) {
            pDescr->stchk.p_stchk[pDescr->stchk.stchk_sz].sz = szChunk;
          } 
          pDescr->stchk.stchk_sz++;
          
          if(pDescr->sz_stsc == 0 || framesInChunk != framesInPrevChunk) {
            if(pDescr->p_stsc) {
              pDescr->p_stsc[pDescr->sz_stsc].firstchunk = idxChunk + 1;
              pDescr->p_stsc[pDescr->sz_stsc].sampleperchunk = framesInChunk;
              pDescr->p_stsc[pDescr->sz_stsc].sampledescidx = stcoIdxOnStsd;                            
            } 
            pDescr->sz_stsc++;
          }

          idxChunk++;
          framesInPrevChunk = framesInChunk;
          framesInChunk = 0;
          szChunk = 0;
          sec++;
        }
              
        idxFrame++;
        framesInChunk++;        
        szFrame = 0;
        lastFrameId = pContent->frameId;
        
      } 

      if(pContent->sizeWr != pContent->sizeRd) {
        if((lenhdr = pContent->sizeWr - pContent->sizeRd) < 0 &&
          (uint32_t) (-1 * lenhdr) > pContent->sizeRd) {
          LOG(X_ERROR("Sample header length exceeds sample total length"));
          return -1;
        }
      } else {
        pContent->sizeWr = pContent->sizeRd + lenhdr;
      }

      szFrame += (pContent->sizeRd + lenhdr);
      pDescr->szdata += (pContent->sizeRd + lenhdr);
      pContent->chunk_idx = idxChunk;

      pContent = cbGetNextSample(pCbUserData, 1, 1);

    } // end of while

    if(szFrame > 0) {
      szChunk += szFrame;
      if(pDescr->stchk.p_stchk) {
        pDescr->stchk.p_stchk[pDescr->stchk.stchk_sz].sz = szChunk;
      }
      pDescr->stchk.stchk_sz++;

      if(pDescr->p_stsz) {
        pDescr->p_stsz[pDescr->sz_stsz++] = szFrame;
      } else {
        pDescr->sz_stsz++;
      }
      if(pDescr->sz_stsc == 0 || framesInChunk != framesInPrevChunk) {
        if(pDescr->p_stsc) {
          pDescr->p_stsc[pDescr->sz_stsc].firstchunk = idxChunk + 1;
          pDescr->p_stsc[pDescr->sz_stsc].sampleperchunk = framesInChunk;
          pDescr->p_stsc[pDescr->sz_stsc].sampledescidx = stcoIdxOnStsd;
        } 
        pDescr->sz_stsc++;
      }
    }

    if(pDescr->sz_ctts > 0) {
      pDescr->sz_ctts++;
    }
    
    if(idxPass == 0) {
      pDescr->stchk.p_stchk = (CHUNK_DESCR_T *) avc_calloc(pDescr->stchk.stchk_sz, 
                                                       sizeof(CHUNK_DESCR_T));
      pDescr->p_stsc = (BOX_STSC_ENTRY_T *) avc_calloc(pDescr->sz_stsc, 
                                                   sizeof(BOX_STSC_ENTRY_T));
      if(pDescr->sz_stss > 0) {
        pDescr->p_stss = (uint32_t *) avc_calloc(pDescr->sz_stss, sizeof(uint32_t));
      }
      pDescr->p_stsz = (uint32_t *) avc_calloc(pDescr->sz_stsz, sizeof(uint32_t));
      if(pDescr->sz_ctts > 1) {
        pDescr->p_ctts = (BOX_CTTS_ENTRY_T *) avc_calloc(pDescr->sz_ctts, sizeof(BOX_CTTS_ENTRY_T));
      }
    }

  }

  return 0;
}

int write_box_mdat_custom(FILE_STREAM_T *pStreamOut, BOX_T *pBox, 
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  unsigned char buf[8];
  MP4_MDAT_CONTENT_NODE_T *pContent = (MP4_MDAT_CONTENT_NODE_T *) pUserData;

  *((uint32_t *)buf) = htonl((uint32_t)pBox->szdata + pBox->szhdr);
  *((uint32_t *)&buf[4]) = pBox->type;

  if(WriteFileStream(pStreamOut, buf, 8) < 0) {
    return -1;
  }

  while(pContent) {

    if(pContent->cbWriteSample(pStreamOut, pContent, arena, sz_arena) < 0) {
      LOG(X_ERROR("Unable to write sample to mdat"));
      return -1;
    }

    pContent = pContent->pnext;
  }

  return 0;
}



static int createTrackFromVideo(const BOX_HANDLER_T *pHandlerChain,
                                MP4_TRAK_VID_T *pTrakVid,
                                const BOX_STTS_ENTRY_LIST_T *pAVSync) {
  
  uint64_t mdhd_duration = 0;
  uint64_t mdhd_timescale = 0;
  SAMPLE_CHUNK_DESCR_T descr;
  BOX_T *pBox = NULL;
  char tmp[128];
  MP4_TRAK_T *pVTrak = pTrakVid->pTrak;

  if(!pTrakVid->cbCreateStsdChild || !pTrakVid->pCbData) {
    LOG(X_ERROR("stsd child creator not set"));
    return -1; 
  }

  if(pTrakVid->clockHz == 0 || pTrakVid->frameDeltaHz == 0) {
    LOG(X_ERROR("Invalid clock Hz: %u or frame delta Hz: %u"), pTrakVid->clockHz, pTrakVid->frameDeltaHz);
    return -1;
  }

  if(pTrakVid->frameIdLast <= 0) {
    LOG(X_ERROR("Unable to create content from empty samples list"));
    return -1;
  }

  if(pTrakVid->width == 0 || pTrakVid->height == 0) {
    LOG(X_ERROR("Unable to create content.  Picture dimensions not known. %dx%d"),
                pTrakVid->width, pTrakVid->height);
    return -1;
  }

  if(pVTrak->mvhd_duration == 0 || pVTrak->mvhd_timescale == 0) {
    LOG(X_ERROR("Invalid mvhd duration: %llu or timescale: %llu"), 
                pVTrak->mvhd_duration, pVTrak->mvhd_timescale);
    return -1;
  }

  mdhd_timescale = pTrakVid->clockHz;
  mdhd_duration = pTrakVid->durationHz;

  //fprintf(stderr, "mdhd_duration:%lld timescale:%lld\n", mdhd_duration, mdhd_timescale);

  pVTrak->pTrak = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "trak") );
  pVTrak->pTkhd = mp4_create_box_tkhd(pHandlerChain, mdhd_duration, pVTrak->tkhd_id,
                                  pTrakVid->width, pTrakVid->height, 0, 0, 0);
  pVTrak->pTrak->child = (BOX_T *) pVTrak->pTkhd;
  pVTrak->pTkhd->box.pnext = pVTrak->pMdia = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "mdia") );
  pVTrak->pMdhd = mp4_create_box_mdhd(pHandlerChain, mdhd_duration, mdhd_timescale);
  pVTrak->pMdia->child = (BOX_T *) pVTrak->pMdhd;
  pVTrak->pHdlr = mp4_create_box_hdlr(pHandlerChain, *((uint32_t *) "vide"), 
                                  vsxlib_get_appnamestr(tmp, sizeof(tmp)));
  pVTrak->pMdhd->box.pnext = (BOX_T *) pVTrak->pHdlr;
  pVTrak->pHdlr->box.pnext = pVTrak->pMinf = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "minf") );
  pVTrak->pVmhd = mp4_create_box_vmhd(pHandlerChain);
  pVTrak->pMinf->child = (BOX_T *) pVTrak->pVmhd;
  pVTrak->pVmhd->box.pnext = pVTrak->pDinf = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "dinf") );
  pVTrak->pDref = mp4_create_box_dref(pHandlerChain, mp4_create_box_dref_url(pHandlerChain, NULL));
  pVTrak->pDinf->child = (BOX_T *) pVTrak->pDref;
  pVTrak->pDinf->pnext = pVTrak->pStbl = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "stbl") );
  pVTrak->pStsd = mp4_create_box_stsd(pHandlerChain);
  pVTrak->pStbl->child = (BOX_T *) pVTrak->pStsd;

  if(!(pVTrak->pStsd->box.child = pTrakVid->cbCreateStsdChild(pHandlerChain, pTrakVid))) {
    return -1;
  }

  pVTrak->pStts = create_box_stts(pHandlerChain, 
                         //pTrakVid->pSamplesLast->frameId + 1, pTrakVid->frameDeltaHz,
                         pTrakVid->frameIdLast + 1, pTrakVid->frameDeltaHz,
                         pAVSync);
  pBox = (BOX_T *) pVTrak->pStts;
  pVTrak->pStsd->box.pnext = (BOX_T *) pVTrak->pStts;

  if(compute_chunks(pTrakVid->cbGetNextSample, pTrakVid->pCbData, 
                 pTrakVid->clockHz, pTrakVid->frameDeltaHz,  &descr, 1, 
                 pTrakVid->lenSamplesHdr) < 0) {
    return -1;
  }

  if(descr.p_ctts) {
    pVTrak->pCtts = (BOX_CTTS_T *) create_box_ctts(pHandlerChain, 
                                                   descr.sz_ctts, descr.p_ctts);
    pBox->pnext = (BOX_T *) pVTrak->pCtts;
    pBox = (BOX_T *) pVTrak->pCtts;
  }

  if(descr.p_stss) {
    pVTrak->pStss = (BOX_STSS_T *) mp4_create_box_stss(pHandlerChain, 
                                                   descr.sz_stss, descr.p_stss);
    pBox->pnext = (BOX_T *) pVTrak->pStss;
    pBox = (BOX_T *) pVTrak->pStss;
  }

  pVTrak->pStsc = (BOX_STSC_T *) mp4_create_box_stsc(pHandlerChain, descr.sz_stsc, descr.p_stsc);
  pBox->pnext = (BOX_T *) pVTrak->pStsc;

  pVTrak->pStsz = (BOX_STSZ_T *) mp4_create_box_stsz(pHandlerChain, descr.sz_stsz, descr.p_stsz);
  pVTrak->pStsc->box.pnext = (BOX_T *) pVTrak->pStsz;

  pVTrak->pStco = (BOX_STCO_T *) mp4_create_box_stco(pHandlerChain, descr.stchk.stchk_sz, (uint32_t *) 
                                                    avc_calloc(descr.stchk.stchk_sz, sizeof(uint32_t)));
  pVTrak->pStsz->box.pnext = (BOX_T *) pVTrak->pStco;

  pVTrak->sz_data = descr.szdata;
  pVTrak->stchk.p_stchk = descr.stchk.p_stchk;
  pVTrak->stchk.stchk_sz = descr.stchk.stchk_sz;

  mp4_updateBoxSizes(pVTrak->pTrak);

  return 0;
}

static int createTrackFromAac(const BOX_HANDLER_T *pHandlerChain,
                              const AAC_DESCR_T *pAac, MP4_TRAK_MP4A_T *pSTrak) {
  
  BOX_ESDS_DESCR_DECODERSPECIFIC_T decoderSp;
  uint64_t mdhd_duration = 0;
  uint64_t mdhd_timescale = 0;
  double durationSec;
  SAMPLE_CHUNK_DESCR_T descr;
  BOX_T *pBox = NULL;
  char tmp[128];

  if(pAac->clockHz == 0 || pAac->decoderSP.frameDeltaHz == 0) {
    LOG(X_ERROR("Invalid clock Hz: %u or frame delta Hz: %u"), pAac->clockHz, pAac->decoderSP.frameDeltaHz);
    return -1;
  }

  if(pAac->pStream == NULL || pAac->pStream->fp == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Invalid aac source"));
    return -1;
  }

  if(pSTrak->tk.mvhd_duration == 0 || pSTrak->tk.mvhd_timescale == 0) {
    LOG(X_ERROR("Invalid mvhd duration: %llu or timescale: %llu"), 
                pSTrak->tk.mvhd_duration, pSTrak->tk.mvhd_timescale);
    return -1;
  }

  mdhd_timescale = pAac->clockHz;
  mdhd_duration = pAac->lastPlayOffsetHz;

  pSTrak->tk.pTrak = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "trak") );
  pSTrak->tk.pTkhd = mp4_create_box_tkhd(pHandlerChain, mdhd_duration, pSTrak->tk.tkhd_id,
                                     0, 0, 0, 0, 0x0100);
  pSTrak->tk.pTrak->child = (BOX_T *) pSTrak->tk.pTkhd;
  pSTrak->tk.pTkhd->box.pnext = pSTrak->tk.pMdia = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "mdia") );
  pSTrak->tk.pMdhd = mp4_create_box_mdhd(pHandlerChain, mdhd_duration, mdhd_timescale);
  pSTrak->tk.pMdia->child = (BOX_T *) pSTrak->tk.pMdhd;
  pSTrak->tk.pHdlr = mp4_create_box_hdlr(pHandlerChain, *((uint32_t *) "soun"), 
                     vsxlib_get_appnamestr(tmp, sizeof(tmp)));
  pSTrak->tk.pMdhd->box.pnext = (BOX_T *) pSTrak->tk.pHdlr;
  pSTrak->tk.pHdlr->box.pnext = pSTrak->tk.pMinf = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "minf") );
  pSTrak->tk.pSmhd = mp4_create_box_smhd(pHandlerChain);
  pSTrak->tk.pMinf->child = (BOX_T *) pSTrak->tk.pSmhd;
  pSTrak->tk.pSmhd->box.pnext = pSTrak->tk.pDinf = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "dinf") );
  pSTrak->tk.pDref = mp4_create_box_dref(pHandlerChain, mp4_create_box_dref_url(pHandlerChain, NULL));
  pSTrak->tk.pDinf->child = (BOX_T *) pSTrak->tk.pDref;
  pSTrak->tk.pDinf->pnext = pSTrak->tk.pStbl = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "stbl") );
  pSTrak->tk.pStsd = mp4_create_box_stsd(pHandlerChain);
  pSTrak->tk.pStbl->child = (BOX_T *) pSTrak->tk.pStsd;
  pSTrak->pMp4a = (BOX_MP4A_T *) mp4_create_box_mp4a(pHandlerChain, &pAac->decoderSP);
  pSTrak->tk.pStsd->box.child = (BOX_T *) pSTrak->pMp4a;

  memset(&decoderSp, 0, sizeof(decoderSp));
  mp4_create_box_esds_decoderspecific(&decoderSp, &pAac->decoderSP);

  durationSec =   ((double)pAac->lastFrameId * pAac->decoderSP.frameDeltaHz / 
                   (double)pAac->clockHz);

  pSTrak->pEsds = (BOX_ESDS_T *) mp4_create_box_esds(pHandlerChain, 
                                 ESDS_OBJTYPE_MP4A,
                                 ESDS_STREAMTYPE_AUDIO,
                                 (uint32_t) ((double)pAac->szTot / durationSec * 8.0),
                                 pAac->peakBwBytes * 8,
                                 pAac->szLargestFrame,
                                 &decoderSp);

  pSTrak->pMp4a->box.child = (BOX_T *) pSTrak->pEsds;
  pSTrak->tk.pStts = create_box_stts(pHandlerChain, pAac->lastFrameId, 
                                     pAac->decoderSP.frameDeltaHz, NULL);
  pSTrak->tk.pStsd->box.pnext = (BOX_T *) pSTrak->tk.pStts;
  pBox = (BOX_T *) pSTrak->tk.pStts;

  if(compute_chunks(aac_cbGetNextSample,  (AAC_DESCR_T *) pAac, pAac->clockHz,
                 pAac->decoderSP.frameDeltaHz, &descr, 1, 0) < 0) {
    return -1;
  }

  if(descr.sz_stss > 0) {
    pSTrak->tk.pStss = (BOX_STSS_T *) mp4_create_box_stss(pHandlerChain, descr.sz_stss, 
                                                    descr.p_stss);
    pBox = (BOX_T *) pSTrak->tk.pStss;
    pSTrak->tk.pStts->box.pnext = (BOX_T *) pSTrak->tk.pStss;
  }

  pSTrak->tk.pStsc = (BOX_STSC_T *) mp4_create_box_stsc(pHandlerChain, 
                                                    descr.sz_stsc, descr.p_stsc);
  pBox->pnext = (BOX_T *) pSTrak->tk.pStsc;

  pSTrak->tk.pStsz = (BOX_STSZ_T *) mp4_create_box_stsz(pHandlerChain, 
                                                    descr.sz_stsz, descr.p_stsz);
  pSTrak->tk.pStsc->box.pnext = (BOX_T *) pSTrak->tk.pStsz;

  pSTrak->tk.pStco = (BOX_STCO_T *) mp4_create_box_stco(pHandlerChain, descr.stchk.stchk_sz, (uint32_t *) 
                                                 avc_calloc(descr.stchk.stchk_sz, sizeof(uint32_t)));
  pSTrak->tk.pStsz->box.pnext = (BOX_T *) pSTrak->tk.pStco;

  pSTrak->tk.sz_data = descr.szdata;
  pSTrak->tk.stchk.p_stchk = descr.stchk.p_stchk;
  pSTrak->tk.stchk.stchk_sz = descr.stchk.stchk_sz;


  mp4_updateBoxSizes(pSTrak->tk.pTrak);

  return 0;
}

static int createTrackFromAmr(const BOX_HANDLER_T *pHandlerChain,
                              const AMR_DESCR_T *pAmr, MP4_TRAK_SAMR_T *pSTrak) {
  
  //BOX_ESDS_DESCR_DECODERSPECIFIC_T decoderSp;
  uint64_t mdhd_duration = 0;
  uint64_t mdhd_timescale = 0;
  //double durationSec;
  SAMPLE_CHUNK_DESCR_T descr;
  BOX_T *pBox = NULL;
  char tmp[128];

  if(pAmr->clockHz == 0 || pAmr->frameDurationHz == 0) {
    LOG(X_ERROR("Invalid clock Hz: %u or frame delta Hz: %u"), pAmr->clockHz, pAmr->frameDurationHz);
    return -1;
  }

  if(pAmr->pStreamIn == NULL || pAmr->pStreamIn->fp == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Invalid amr source"));
    return -1;
  }

  if(pSTrak->tk.mvhd_duration == 0 || pSTrak->tk.mvhd_timescale == 0) {
    LOG(X_ERROR("Invalid mvhd duration: %llu or timescale: %llu"), 
                pSTrak->tk.mvhd_duration, pSTrak->tk.mvhd_timescale);
    return -1;
  }

  mdhd_timescale = pAmr->clockHz;
  mdhd_duration = pAmr->lastPlayOffsetHz;

  pSTrak->tk.pTrak = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "trak") );
  pSTrak->tk.pTkhd = mp4_create_box_tkhd(pHandlerChain, mdhd_duration, pSTrak->tk.tkhd_id,
                                     0, 0, 0, 0, 0x0100);
  pSTrak->tk.pTrak->child = (BOX_T *) pSTrak->tk.pTkhd;
  pSTrak->tk.pTkhd->box.pnext = pSTrak->tk.pMdia = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "mdia") );
  pSTrak->tk.pMdhd = mp4_create_box_mdhd(pHandlerChain, mdhd_duration, mdhd_timescale);
  pSTrak->tk.pMdia->child = (BOX_T *) pSTrak->tk.pMdhd;
  pSTrak->tk.pHdlr = mp4_create_box_hdlr(pHandlerChain, *((uint32_t *) "soun"), 
                     vsxlib_get_appnamestr(tmp, sizeof(tmp)));
  pSTrak->tk.pMdhd->box.pnext = (BOX_T *) pSTrak->tk.pHdlr;
  pSTrak->tk.pHdlr->box.pnext = pSTrak->tk.pMinf = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "minf") );
  pSTrak->tk.pSmhd = mp4_create_box_smhd(pHandlerChain);
  pSTrak->tk.pMinf->child = (BOX_T *) pSTrak->tk.pSmhd;
  pSTrak->tk.pSmhd->box.pnext = pSTrak->tk.pDinf = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "dinf") );
  pSTrak->tk.pDref = mp4_create_box_dref(pHandlerChain, mp4_create_box_dref_url(pHandlerChain, NULL));
  pSTrak->tk.pDinf->child = (BOX_T *) pSTrak->tk.pDref;
  pSTrak->tk.pDinf->pnext = pSTrak->tk.pStbl = mp4_create_box_generic(pHandlerChain, *((uint32_t *) "stbl") );
  pSTrak->tk.pStsd = mp4_create_box_stsd(pHandlerChain);
  pSTrak->tk.pStbl->child = (BOX_T *) pSTrak->tk.pStsd;
  pSTrak->pSamr = (BOX_SAMR_T *) create_box_samr(pHandlerChain, pAmr->clockHz);
  pSTrak->tk.pStsd->box.child = (BOX_T *) pSTrak->pSamr;
  pSTrak->pDamr = (BOX_DAMR_T *) create_box_damr(pHandlerChain);
  pSTrak->pSamr->box.child = (BOX_T *) pSTrak->pDamr;
  pSTrak->tk.pStts = create_box_stts(pHandlerChain, pAmr->lastFrameId, 
                                     pAmr->frameDurationHz, NULL);
  pSTrak->tk.pStsd->box.pnext = (BOX_T *) pSTrak->tk.pStts;
  pBox = (BOX_T *) pSTrak->tk.pStts;

  if(compute_chunks(amr_cbGetNextSample,  (AMR_DESCR_T *) pAmr, pAmr->clockHz,
                 pAmr->frameDurationHz, &descr, 1, 0) < 0) {
    return -1;
  }

  if(descr.sz_stss > 0) {
    pSTrak->tk.pStss = (BOX_STSS_T *) mp4_create_box_stss(pHandlerChain, descr.sz_stss, 
                                                    descr.p_stss);
    pBox = (BOX_T *) pSTrak->tk.pStss;
    pSTrak->tk.pStts->box.pnext = (BOX_T *) pSTrak->tk.pStss;
  }

  pSTrak->tk.pStsc = (BOX_STSC_T *) mp4_create_box_stsc(pHandlerChain, 
                                                    descr.sz_stsc, descr.p_stsc);
  pBox->pnext = (BOX_T *) pSTrak->tk.pStsc;

  pSTrak->tk.pStsz = (BOX_STSZ_T *) mp4_create_box_stsz(pHandlerChain, 
                                                    descr.sz_stsz, descr.p_stsz);
  pSTrak->tk.pStsc->box.pnext = (BOX_T *) pSTrak->tk.pStsz;

  pSTrak->tk.pStco = (BOX_STCO_T *) mp4_create_box_stco(pHandlerChain, descr.stchk.stchk_sz, (uint32_t *) 
                                                 avc_calloc(descr.stchk.stchk_sz, sizeof(uint32_t)));
  pSTrak->tk.pStsz->box.pnext = (BOX_T *) pSTrak->tk.pStco;

  pSTrak->tk.sz_data = descr.szdata;
  pSTrak->tk.stchk.p_stchk = descr.stchk.p_stchk;
  pSTrak->tk.stchk.stchk_sz = descr.stchk.stchk_sz;


  mp4_updateBoxSizes(pSTrak->tk.pTrak);

  return 0;
}


MP4_CONTAINER_T *mp4_createEmptyMp4Isom(uint64_t mvhd_duration, uint64_t mvhd_timescale) {
  BOX_T *pBoxMoov = NULL;
  BOX_T *pBoxMdat = NULL;
  MP4_CONTAINER_T *pMp4 = NULL;
  struct timeval tv;

  if(mvhd_timescale == 0) {
    LOG(X_ERROR("Clock rate not known"));
    return NULL;
  }

  if((pMp4 = mp4_createContainer()) == NULL) {
    return NULL;
  }

  gettimeofday(&tv, NULL);

  pMp4->proot->child =(BOX_T *) create_box_ftyp(pMp4->proot->handler);
  pMp4->proot->child->pnext = pBoxMoov = mp4_create_box_generic(pMp4->proot->handler, 
                                                            *((uint32_t *) "moov") );
  pBoxMoov->child = (BOX_T *) mp4_create_box_mvhd(pMp4->proot->handler, mvhd_duration, mvhd_timescale, 1,
                                             (uint64_t)tv.tv_sec + SEC_BETWEEN_EPOCH_1904_1970,
                                             (uint64_t)tv.tv_sec + SEC_BETWEEN_EPOCH_1904_1970);

  if((pBoxMoov->pnext = pBoxMdat = mp4_create_box_generic(pMp4->proot->handler, *((uint32_t *) "mdat") ))) {
    pBoxMdat->szdata_ownbox = 0;
    pBoxMdat->szdata = pBoxMdat->szdata_ownbox;
  }

  mp4_updateBoxSizes(pMp4->proot);

  return pMp4;
}

static void attachMp4Track(BOX_T *pBoxMoov, BOX_T *pTrak) {
  BOX_T *pBox;
  BOX_TKHD_T *pBoxTkhd = NULL;
  BOX_MVHD_T *pBoxMvhd = NULL;
  int numTracks = 0;

  if((pBox = pBoxMoov->child)) {
    while(pBox) {
    
      if(pBox->type == *((uint32_t *) "mvhd")) {
        pBoxMvhd = (BOX_MVHD_T *) pBox;
      }

      if(pBox->type == *((uint32_t *) "trak")) {
        numTracks++;
        if((pBoxTkhd  = (BOX_TKHD_T *) mp4_findBoxChild(pBox, *((uint32_t *) "tkhd")))) {
          pBoxTkhd->trackid = numTracks;
        }
      }
      if(pBox->pnext == NULL) {
        pBox->pnext = pTrak;
        break;
      }
      pBox = pBox->pnext;
    }
  } else {
    pBoxMoov->child = pTrak;
  }

  pBoxMvhd->nexttrack = htonl(numTracks + 2);
  if((pBoxTkhd  = (BOX_TKHD_T *) mp4_findBoxChild(pTrak, *((uint32_t *) "tkhd")))) {
    pBoxTkhd->trackid = numTracks + 1;
  }

}

static int addVideoTrackToMp4(MP4_CONTAINER_T *pMp4, MP4_TRAK_T *pVTrak) {

  BOX_MVHD_T *pBoxMvhd = NULL;
  BOX_T *pBoxMoov = NULL;
  BOX_T *pBoxMdat = NULL;

  if(!pMp4 || !pMp4->proot || 
     !mp4_findBoxChild(pMp4->proot, *((uint32_t *) "ftyp")) ||
     !(pBoxMdat = mp4_findBoxChild(pMp4->proot, *((uint32_t *) "mdat"))) ||
     !(pBoxMoov = mp4_findBoxChild(pMp4->proot, *((uint32_t *) "moov"))) ||
     !(pBoxMvhd = (BOX_MVHD_T *) mp4_findBoxChild(pBoxMoov, *((uint32_t *) "mvhd")))  ) {

    LOG(X_ERROR("Invalid mp4 root box"));
    return -1;
  }

  if(!pVTrak || !pVTrak->pTkhd || !pVTrak->pMdia || !pVTrak->pMdhd ||
    !pVTrak->pHdlr || !pVTrak->pMinf || !pVTrak->pVmhd || !pVTrak->pDinf ||
    !pVTrak->pStbl || !pVTrak->pStsd || !pVTrak->pStts || !pVTrak->pStsz ||
    !pVTrak->pStsc || !pVTrak->pStco) {
    LOG(X_ERROR("Invalid video track"));
    return -1;
  }

  pVTrak->pTkhd->creattime = pBoxMvhd->creattime;
  pVTrak->pTkhd->modificationtime = pBoxMvhd->modificationtime;
  pVTrak->pMdhd->creattime = pBoxMvhd->creattime;
  pVTrak->pMdhd->modificationtime = pBoxMvhd->modificationtime;
  
  attachMp4Track(pBoxMoov, pVTrak->pTrak);

  mp4_updateBoxSizes(pMp4->proot);

  if(update_stco(pVTrak->pStco, pMp4->proot->szdata, &pVTrak->stchk) < 0) {
    // Preserve state at mp4 box state at function entry
    return -1;
  }

  pBoxMdat->szdata_ownbox += (uint32_t) pVTrak->sz_data;
  pBoxMdat->szdata = pBoxMdat->szdata_ownbox;

  mp4_updateBoxSizes(pMp4->proot);

  return 0;
}


static int addAudioTrackToMp4(MP4_CONTAINER_T *pMp4, MP4_TRAK_T *pSTrak) {

  BOX_MVHD_T *pBoxMvhd = NULL;
  BOX_T *pBoxMoov = NULL;
  BOX_T *pBoxMdat = NULL;

  if(!pMp4 || !pMp4->proot || 
     !mp4_findBoxChild(pMp4->proot, *((uint32_t *) "ftyp")) ||
     !(pBoxMdat = mp4_findBoxChild(pMp4->proot, *((uint32_t *) "mdat"))) ||
     !(pBoxMoov = mp4_findBoxChild(pMp4->proot, *((uint32_t *) "moov"))) ||
     !(pBoxMvhd = (BOX_MVHD_T *) mp4_findBoxChild(pBoxMoov, *((uint32_t *) "mvhd")))  ) {

    LOG(X_ERROR("Invalid mp4 root box"));
    return -1;
  }

  if(!pSTrak || !pSTrak->pTkhd || !pSTrak->pMdia || !pSTrak->pMdhd ||
    !pSTrak->pHdlr || !pSTrak->pMinf || !pSTrak->pSmhd || !pSTrak->pDinf ||
    !pSTrak->pStbl || !pSTrak->pStsd || !pSTrak->pStts || !pSTrak->pStsz ||
    !pSTrak->pStsc || !pSTrak->pStco) {
    LOG(X_ERROR("Invalid sound track"));
    return -1;
  }

  pSTrak->pTkhd->creattime = pBoxMvhd->creattime;
  pSTrak->pTkhd->modificationtime = pBoxMvhd->modificationtime;
  pSTrak->pMdhd->creattime = pBoxMvhd->creattime;
  pSTrak->pMdhd->modificationtime = pBoxMvhd->modificationtime;
  
  attachMp4Track(pBoxMoov, pSTrak->pTrak);

  mp4_updateBoxSizes(pMp4->proot);

  if(update_stco(pSTrak->pStco, pMp4->proot->szdata, &pSTrak->stchk) < 0) {
    // Preserve state at mp4 box state at function entry
    return -1;
  }

  pBoxMdat->szdata_ownbox += (uint32_t) pSTrak->sz_data;
  pBoxMdat->szdata = pBoxMdat->szdata_ownbox;

  mp4_updateBoxSizes(pMp4->proot);

  return 0;
}

static MP4_CONTAINER_T *createMp4FromVideo(MP4_SAMPLES_EXT_T *pSamplesExt, 
                                           MP4_TRAK_VID_T *pTrakVid,
                                           const BOX_STTS_ENTRY_LIST_T *pAVSync) {
  MP4_TRAK_T vTrak;
  MP4_CONTAINER_T *pMp4 = NULL;

  if(!pTrakVid || pTrakVid->numSamples == 0 || pTrakVid->frameIdLast <= 0) {
    return NULL;
  }

  memset(&vTrak, 0, sizeof(vTrak));

  vTrak.tkhd_id = 1;
  vTrak.mvhd_timescale = pTrakVid->clockHz;
  vTrak.mvhd_duration = pTrakVid->durationHz;

  LOG(X_DEBUG("Using frame rate: %u / %uHz, duration: %lluHz"), pTrakVid->clockHz, pTrakVid->frameDeltaHz, vTrak.mvhd_duration);

  if(pSamplesExt == NULL &&
    (pMp4 = mp4_createEmptyMp4Isom(vTrak.mvhd_duration, vTrak.mvhd_timescale)) == NULL) {
    return NULL;
  } else if(pSamplesExt != NULL) {
    pMp4 = pSamplesExt->pMp4;
  }

  pTrakVid->pTrak = &vTrak;
  if(createTrackFromVideo(pMp4->proot->handler, pTrakVid, pAVSync) < 0) {    
    mp4_freeBoxes(vTrak.pTrak);
    pTrakVid->pTrak = NULL;
    if(pSamplesExt == NULL) {
      mp4_close(pMp4);
    }
    return NULL;
  }

  if(addVideoTrackToMp4(pMp4, &vTrak) < 0) {
    mp4_freeBoxes(vTrak.pTrak);
    pTrakVid->pTrak = NULL;
    if(pSamplesExt == NULL) {
      mp4_close(pMp4);
    }
    return NULL;
  }

  if(pSamplesExt) {
    pSamplesExt->stchkVid.p_stchk = vTrak.stchk.p_stchk;
    pSamplesExt->stchkVid.stchk_sz = vTrak.stchk.stchk_sz;
    pSamplesExt->pBoxStcoVid = vTrak.pStco;
  } else if( vTrak.stchk.p_stchk) {
    free(vTrak.stchk.p_stchk);
  }

  return pMp4;
}

static BOX_T *create_stsd_child_h264(const BOX_HANDLER_T *pHandlerChain, void *pCbData) {

  MP4_TRAK_VID_T *pTrak = (MP4_TRAK_VID_T *) pCbData;
  H264_AVC_DESCR_T *pH264 = (H264_AVC_DESCR_T *) pTrak->pCbData;
  BOX_AVCC_T *pAvcC = NULL;
  BOX_AVC1_T *pAvc1 = mp4_create_box_avc1(pHandlerChain, 1, pH264->frameWidthPx, 
                                      pH264->frameHeightPx);
  pAvcC = (BOX_AVCC_T *) mp4_create_box_avcC(pHandlerChain, 
           pH264->spspps.sps, pH264->spspps.sps_len, pH264->spspps.pps, pH264->spspps.pps_len);
  pAvc1->box.child = (BOX_T *) pAvcC;

  pTrak->lenSamplesHdr = pAvcC->avcc.lenminusone+1;

  return (BOX_T *) pAvc1;
}

static BOX_T *create_stsd_child_mpg4v(const BOX_HANDLER_T *pHandlerChain, void *pCbData) {

  MP4_TRAK_VID_T *pTrak = (MP4_TRAK_VID_T *) pCbData;
  MPG4V_DESCR_T *pMpg4v = (MPG4V_DESCR_T *) pTrak->pCbData;
  BOX_ESDS_DESCR_DECODERSPECIFIC_T decoderSp;
  BOX_ESDS_T *pBoxEsds = NULL;
  BOX_MP4V_T *pBoxMp4v = NULL;

  if(pMpg4v->seqHdrs.hdrsLen > ESDS_STARTCODES_SZMAX) {
    LOG(X_ERROR("mpeg-4 start codes length %d exceeds %d"), pMpg4v->seqHdrs.hdrsLen, 
                ESDS_STARTCODES_SZMAX); 
    return NULL;
  }

  pBoxMp4v = create_box_mp4v(pHandlerChain, 1, 
                          pMpg4v->param.frameWidthPx, pMpg4v->param.frameHeightPx); 

  memset(&decoderSp, 0, sizeof(decoderSp));
  decoderSp.type.descriptor = 0x05;
  decoderSp.type.length = pMpg4v->seqHdrs.hdrsLen;
  memcpy(decoderSp.startcodes, pMpg4v->seqHdrs.hdrsBuf, decoderSp.type.length); 

  pBoxEsds = (BOX_ESDS_T *) mp4_create_box_esds(pHandlerChain,
                                 ESDS_OBJTYPE_MP4V,
                                 ESDS_STREAMTYPE_VISUAL,
                                 0,
                                 0,
                                 0,
                                 &decoderSp);

  pBoxMp4v->box.child = (BOX_T *) pBoxEsds;

  return (BOX_T *) pBoxMp4v;
}


static MP4_CONTAINER_T *createMp4FromAnnexBList(MP4_SAMPLES_EXT_T *pSamplesExt, 
                                                H264_AVC_DESCR_T *pH264,
                                                const BOX_STTS_ENTRY_LIST_T *pAVSync,
                                                FILE_STREAM_T *pStreamInH264, 
                                                double fps) {
  MP4_TRAK_VID_T vidTrack;

  if(h264_createNalListFromAnnexB(pH264, pStreamInH264, 0, fps) < 0) {
    return NULL;
  }

  if(h264_updateFps(pH264, fps) < 0) {
    return NULL;
  }

  if(!pH264->pNalsVcl || !pH264->pNalsLastVcl) {
    LOG(X_ERROR("Unable to create content from empty samples list"));
    return NULL;
  }

  if(pH264->spspps.sps_len == 0 || pH264->spspps.pps_len == 0) {
    LOG(X_ERROR("Unable to create content.  sps_len: %d, pps_len: %d"),
        pH264->spspps.sps_len, pH264->spspps.pps_len);
    return NULL;
  }

  memset(&vidTrack, 0, sizeof(vidTrack));
  vidTrack.pTrak = NULL;
  vidTrack.clockHz = pH264->clockHz;
  vidTrack.frameDeltaHz = pH264->frameDeltaHz;
  vidTrack.numSamples = pH264->numNalsVcl;
  vidTrack.frameIdLast = pH264->pNalsLastVcl->content.frameId;
  vidTrack.width = pH264->frameWidthPx;
  vidTrack.height = pH264->frameHeightPx;
  vidTrack.lenSamplesHdr = 0; // should be set from within create_stsd_child_h264
  vidTrack.cbGetNextSample = h264_cbGetNextAnnexBSample;
  vidTrack.pCbData = pH264;
  vidTrack.cbCreateStsdChild = create_stsd_child_h264;

//fprintf(stderr, "VID h264 %d/%d, track:%d/%d duration:%lldHz av:%dHz tot:%lldHz %d entries, last:%d\n", pH264->clockHz, pH264->frameDeltaHz, vidTrack.clockHz, vidTrack.frameDeltaHz, vidTrack.durationHz,  pAVSync->clockHz, pAVSync->totsamplesduration, pAVSync->entrycnt, vidTrack.frameIdLast);

  if(pAVSync && pAVSync->totsamplesduration) {

    if(vidTrack.clockHz != pAVSync->clockHz) {
      vidTrack.clockHz = pAVSync->clockHz;
      vidTrack.frameDeltaHz = ((double)pAVSync->totsamplesduration / vidTrack.frameIdLast);
      vidTrack.durationHz = pAVSync->totsamplesduration;

//fprintf(stderr, "VID h264 %d/%d, track:%d/%d duration:%lldHz av:%dHz tot:%lldHz %d entries, last:%d\n", pH264->clockHz, pH264->frameDeltaHz, vidTrack.clockHz, vidTrack.frameDeltaHz, vidTrack.durationHz,  pAVSync->clockHz, pAVSync->totsamplesduration, pAVSync->entrycnt, vidTrack.frameIdLast);

    } else {
      vidTrack.durationHz = pAVSync->totsamplesduration * 
                             ((double)pH264->clockHz / pAVSync->clockHz);
    }

  } else if(pH264->lastPlayOffsetHz > 0) {
    vidTrack.durationHz = pH264->lastPlayOffsetHz;
  } else {
    vidTrack.durationHz = vidTrack.frameIdLast * vidTrack.frameDeltaHz;
  }


  return createMp4FromVideo(pSamplesExt, &vidTrack, pAVSync);
}

static MP4_CONTAINER_T *createMp4FromMpg4vList(MP4_SAMPLES_EXT_T *pSamplesExt, 
                                               MPG4V_DESCR_T *pMpg4v,
                                               const BOX_STTS_ENTRY_LIST_T *pAVSync,
                                               FILE_STREAM_T *pStreamInMpg4v, 
                                               double fps) {
  MP4_TRAK_VID_T vidTrack;

  if(!pMpg4v || !pMpg4v->pStreamIn) {
    return NULL;
  }

  if(mpg4v_getSamples(pMpg4v) < 0) {
    return NULL;
  }

  if(!pMpg4v->content.pSamplesBuf|| pMpg4v->numSamples <= 0) {
    LOG(X_ERROR("Unable to create content from empty samples list"));
    return NULL;
  }

  if(pMpg4v->seqHdrs.hdrsLen <= 0) {
    LOG(X_ERROR("Unable to create content.  mpeg-4 meta-data length: %d"), 
                 pMpg4v->seqHdrs.hdrsLen);
    return NULL;
  }

  memset(&vidTrack, 0, sizeof(vidTrack));
  vidTrack.pTrak = NULL;
  vidTrack.clockHz = pMpg4v->param.clockHz;
  vidTrack.frameDeltaHz = pMpg4v->param.frameDeltaHz;
  vidTrack.numSamples = pMpg4v->numSamples;
  vidTrack.frameIdLast = pMpg4v->content.pSamplesBuf[pMpg4v->numSamples - 1].frameId;
  vidTrack.width = pMpg4v->param.frameWidthPx;
  vidTrack.height = pMpg4v->param.frameHeightPx;
  vidTrack.lenSamplesHdr = 0; 
  vidTrack.cbGetNextSample = mpg4v_cbGetNextSample;
  vidTrack.pCbData = pMpg4v;
  vidTrack.cbCreateStsdChild = create_stsd_child_mpg4v;
  if(pAVSync && pAVSync->totsamplesduration) {
    vidTrack.durationHz = pAVSync->totsamplesduration * 
                           ((double)pMpg4v->param.clockHz / pAVSync->clockHz);
  //fprintf(stdout, "%lld %d/%d\n", vidTrack.durationHz, pAVSync->clockHz, pMpg4v->clockHz);
  } else if(pMpg4v->lastPlayOffsetHz > 0) {
    vidTrack.durationHz = pMpg4v->lastPlayOffsetHz; 
  } else {
    vidTrack.durationHz = vidTrack.frameIdLast * vidTrack.frameDeltaHz;
  }
;
  return createMp4FromVideo(pSamplesExt, &vidTrack, pAVSync);
}

static MP4_CONTAINER_T *createMp4FromAacADTS(MP4_SAMPLES_EXT_T *pSamplesExt, 
                                             AAC_DESCR_T *pAac, int storeSamples,
                                             int esdsObjTypeIdxPlus1) {
  
  AAC_ADTS_FRAME_T frame;
  MP4_CONTAINER_T *pMp4 = NULL;  
  MP4_TRAK_MP4A_T sTrack;

  memset(&frame, 0, sizeof(frame));
  frame.sp.frameDeltaHz = pAac->decoderSP.frameDeltaHz;

  if(pAac->decoderSP.frameDeltaHz != 960 && pAac->decoderSP.frameDeltaHz != 1024) {
    LOG(X_ERROR("Invalid number of samples / frame: %u"), pAac->decoderSP.frameDeltaHz);
    return NULL;
  } else if(aac_decodeAdtsHdrStream(pAac->pStream, &frame) != 0) {
    return NULL;
  }
  if(esdsObjTypeIdxPlus1 > 0) {
    LOG(X_INFO("Overriding aac object type idx %d with %d"),
               frame.sp.objTypeIdx, esdsObjTypeIdxPlus1 - 1);
    frame.sp.objTypeIdx = esdsObjTypeIdxPlus1 - 1;
  }
  memcpy(&pAac->decoderSP, &frame.sp, sizeof(ESDS_DECODER_CFG_T));
  pAac->clockHz = esds_getFrequencyVal(ESDS_FREQ_IDX(pAac->decoderSP));

  if(aac_getSamplesFromAacAdts(pAac, 0) != 0) {
    return NULL;
  }

  if(storeSamples) {
    if((pAac->content.pSamplesBuf = (MP4_MDAT_CONTENT_NODE_T *) 
      avc_calloc((size_t) pAac->lastFrameId, sizeof(MP4_MDAT_CONTENT_NODE_T))) == NULL) {
      LOG(X_CRITICAL("Failed to allocated %d x %d"), pAac->lastFrameId, 
                     sizeof(MP4_MDAT_CONTENT_NODE_T));
      return NULL;
    }
    pAac->content.samplesBufSz = (unsigned int) pAac->lastFrameId;

    if(aac_getSamplesFromAacAdts(pAac, 1) != 0) {
      aac_free(pAac);
      return NULL;
    }
  }

  memset(&sTrack, 0, sizeof(sTrack));
  sTrack.tk.tkhd_id = 1;
  sTrack.tk.mvhd_timescale = pAac->clockHz;
  sTrack.tk.mvhd_duration = pAac->lastFrameId * pAac->decoderSP.frameDeltaHz;

  if(pSamplesExt == NULL &&
    (pMp4 = mp4_createEmptyMp4Isom(sTrack.tk.mvhd_duration, sTrack.tk.mvhd_timescale)) == NULL) {
    aac_free(pAac);
    return NULL;
  } else if(pSamplesExt != NULL) {
    pMp4 = pSamplesExt->pMp4;
  }


  if(createTrackFromAac((const BOX_HANDLER_T *) pMp4->proot->handler, 
                        (const AAC_DESCR_T *) pAac, &sTrack) < 0) {    
    mp4_freeBoxes(sTrack.tk.pTrak);
    if(pSamplesExt == NULL) {
      mp4_close(pMp4);
    }
    // TODO: free sTrack contents
    aac_free(pAac);
    return NULL;
  }

  if(addAudioTrackToMp4(pMp4, &sTrack.tk) < 0) {
    mp4_freeBoxes(sTrack.tk.pTrak);
    if(pSamplesExt == NULL) {
      mp4_close(pMp4);
    }
    // TODO: free sTrack contents
    aac_free(pAac);
    return NULL;
  }

  if(pSamplesExt) {
    pSamplesExt->stchkAud.p_stchk = sTrack.tk.stchk.p_stchk;
    pSamplesExt->stchkAud.stchk_sz = sTrack.tk.stchk.stchk_sz;
    pSamplesExt->pBoxStcoAud = sTrack.tk.pStco;
  } else if(sTrack.tk.stchk.p_stchk) {
    free(sTrack.tk.stchk.p_stchk);
  }

  return pMp4;
}

static MP4_CONTAINER_T *createMp4FromAmr(MP4_SAMPLES_EXT_T *pSamplesExt, 
                                         AMR_DESCR_T *pAmr, int storeSamples) {
  
  MP4_CONTAINER_T *pMp4 = NULL;  
  MP4_TRAK_SAMR_T sTrack;

  if(amr_getSamples(pAmr) < 0) {
    return NULL;
  }

/*
  if(storeSamples) {
    if((pAac->content.pSamplesBuf = (MP4_MDAT_CONTENT_NODE_T *) 
      avc_calloc((size_t) pAac->lastFrameId, sizeof(MP4_MDAT_CONTENT_NODE_T))) == NULL) {
      LOG(X_CRITICAL("Failed to allocated %d x %d"), pAac->lastFrameId, 
                     sizeof(MP4_MDAT_CONTENT_NODE_T));
      return NULL;
    }
    pAac->content.samplesBufSz = (unsigned int) pAac->lastFrameId;

    if(aac_getSamplesFromAacAdts(pAac, 1) != 0) {
      aac_free(pAac);
      return NULL;
    }
  }
*/

  memset(&sTrack, 0, sizeof(sTrack));
  sTrack.tk.tkhd_id = 1;
  sTrack.tk.mvhd_timescale = pAmr->clockHz;
  sTrack.tk.mvhd_duration = pAmr->lastFrameId * pAmr->frameDurationHz;

  if(pSamplesExt == NULL &&
    (pMp4 = mp4_createEmptyMp4Isom(sTrack.tk.mvhd_duration, sTrack.tk.mvhd_timescale)) == NULL) {
    amr_free(pAmr);
    return NULL;
  } else if(pSamplesExt != NULL) {
    pMp4 = pSamplesExt->pMp4;
  }


  if(createTrackFromAmr((const BOX_HANDLER_T *) pMp4->proot->handler, 
                        (const AMR_DESCR_T *) pAmr, &sTrack) < 0) {    
    mp4_freeBoxes(sTrack.tk.pTrak);
    if(pSamplesExt == NULL) {
      mp4_close(pMp4);
    }
    // TODO: free sTrack contents
    amr_free(pAmr);
    return NULL;
  }

  if(addAudioTrackToMp4(pMp4, &sTrack.tk) < 0) {
    mp4_freeBoxes(sTrack.tk.pTrak);
    if(pSamplesExt == NULL) {
      mp4_close(pMp4);
    }
    // TODO: free sTrack contents
    amr_free(pAmr);
    return NULL;
  }

  if(pSamplesExt) {
    pSamplesExt->stchkAud.p_stchk = sTrack.tk.stchk.p_stchk;
    pSamplesExt->stchkAud.stchk_sz = sTrack.tk.stchk.stchk_sz;
    pSamplesExt->pBoxStcoAud = sTrack.tk.pStco;
  } else if(sTrack.tk.stchk.p_stchk) {
    free(sTrack.tk.stchk.p_stchk);
  }

  return pMp4;
}




static MP4_MDAT_CONTENT_NODE_T *mergeMdatContent(MP4_SAMPLES_EXT_T *pSamplesExt,
                                                 const uint32_t offset0) {

  MP4_MDAT_CONTENT_NODE_T *pList1 = pSamplesExt->aud.pSamplesBuf;
  MP4_MDAT_CONTENT_NODE_T *pList2 = pSamplesExt->vid.pSamplesBuf;

  MP4_MDAT_CONTENT_NODE_T *pPrev = NULL;
  MP4_MDAT_CONTENT_NODE_T *pCur = NULL;
  MP4_MDAT_CONTENT_NODE_T *pNext = NULL;
  MP4_MDAT_CONTENT_NODE_T *pStart = NULL;
  uint32_t offset = offset0;
  unsigned int idx = 0;
  BOX_T *pBoxMdat = mp4_findBoxChild(pSamplesExt->pMp4->proot, *((uint32_t *) "mdat"));

  if(!pList1) {
    return pList2;
  } else if(!pList2) {
    return pList1;
  } 

  while(idx < pSamplesExt->stchkAud.stchk_sz ||
        idx < pSamplesExt->stchkVid.stchk_sz) {

    if(idx < pSamplesExt->stchkAud.stchk_sz) {
      if(!pSamplesExt->pBoxStcoAud) {
        return NULL;
      }
      pSamplesExt->pBoxStcoAud->pSamples[idx] = offset;
      offset += pSamplesExt->stchkAud.p_stchk[idx].sz;
    }
    if(idx < pSamplesExt->stchkVid.stchk_sz) {
      if(!pSamplesExt->pBoxStcoVid) {
        return NULL;
      }
      pSamplesExt->pBoxStcoVid->pSamples[idx] = offset;
      offset += pSamplesExt->stchkVid.p_stchk[idx].sz;
    }

    idx++;
  }

  pBoxMdat->szdata_ownbox = (offset - offset0);
  pBoxMdat->szdata = pBoxMdat->szdata_ownbox;
  mp4_updateBoxSizes(pSamplesExt->pMp4->proot);

  if(pList1->chunk_idx <= pList2->chunk_idx) {
    pCur = pList1;
  } else {
    pCur = pList2;
  }
  pStart = pCur;

  while(pList1 && pList2) {

    if((pNext = pCur->pnext) && pNext->chunk_idx <= pCur->chunk_idx) {

      if(pCur == pList1) {
        pList1 = pList1->pnext;
        pCur = pList1;
      } else {
        pList2 = pList2->pnext;
        pCur = pList2;
      }

    } else {

      if(pCur == pList1) { 
        pList1 = pList1->pnext;
        pCur = pList2;
      } else {
        pList2 = pList2->pnext;
        pCur = pList1;
      }

      if(pPrev) {
        pPrev->pnext = pCur;
      }
    }

    pPrev = pCur;
  }

  return pStart;
}

static int mp4_createFromVideoList(MP4_SAMPLES_EXT_T *pSamplesExt,
                                   FILE_STREAM_T *pStreamIn, 
                                   enum MEDIA_FILE_TYPE mediaType,
                                   const char *pathOutMp4,
                                   double fps,
                                   const char *avsyncin) {
  H264_AVC_DESCR_T h264;
  MPG4V_DESCR_T mpg4v;
  MP4_CONTAINER_T *pMp4 = NULL;
  MP4_MDAT_CONTENT_NODE_T *pContent = NULL;
  BOX_HANDLER_T *pMdatHandler = NULL;
  BOX_STTS_ENTRY_LIST_T *pAVSync = NULL;
  int rc = 0;

  if(!pStreamIn|| !pathOutMp4) {
    return -1;
  } 

  if(avsyncin && (pAVSync = mp4_avsync_readsttsfile(avsyncin)) == NULL) {
    return -1;
  }

  switch(mediaType) {
    case MEDIA_FILE_TYPE_H264b:

      memset(&h264, 0, sizeof(h264));
      if((pMp4 = createMp4FromAnnexBList(pSamplesExt, &h264, pAVSync, pStreamIn, fps)) == NULL) { 
        mp4_free_avsync_stts(pAVSync);
        h264_free(&h264);
        return -1;
      }

      if(pSamplesExt) {
        pSamplesExt->vid.pSampleCbPrev = h264.pNalCbPrev;
        pSamplesExt->vid.pSamplesBuf = &h264.pNalsBuf->content;
        pSamplesExt->vid.samplesBufSz = h264.nalsBufSz;
      } else {
        pContent = (MP4_MDAT_CONTENT_NODE_T *) h264.pNalsVcl;
      }

      break;

    case MEDIA_FILE_TYPE_MP4V:

      memset(&mpg4v, 0, sizeof(mpg4v));
      mpg4v.pStreamIn = pStreamIn;
      if((pMp4 = createMp4FromMpg4vList(pSamplesExt, &mpg4v, pAVSync, pStreamIn, fps)) == NULL) { 
        mp4_free_avsync_stts(pAVSync);
        mpg4v_free(&mpg4v);
        return -1;
      }

      if(pSamplesExt) {
        pSamplesExt->vid.pSampleCbPrev = mpg4v.content.pSampleCbPrev;
        pSamplesExt->vid.pSamplesBuf = mpg4v.content.pSamplesBuf;
        pSamplesExt->vid.samplesBufSz = mpg4v.numSamples;
      } else {
        pContent = mpg4v.content.pSamplesBuf;
      }

      break;

    default:
      LOG(X_ERROR("Unsupported video media type %s"), codecType_getCodecDescrStr(mediaType));
      mp4_free_avsync_stts(pAVSync);
      return -1;
  }

  if(pSamplesExt) {
    if(!(pContent = mergeMdatContent(pSamplesExt, pSamplesExt->pBoxStcoVid->pSamples[0]))) {
      LOG(X_ERROR("Unable to merge audio and video data content"));
      rc = -1;
    }
    free(pSamplesExt->stchkVid.p_stchk);
  } 

  if((pMdatHandler = (BOX_HANDLER_T *) mp4_findHandlerInChain(pMp4->proot->handler, *((uint32_t *) "mdat") ))) {
    pMdatHandler->writefunc = write_box_mdat_custom;
  }

  if(rc == 0) {
    rc = mp4_write(pMp4, pathOutMp4, pContent);
  }
 
  if(!pSamplesExt) {
    mp4_close(pMp4);
  }

  switch(mediaType) {
    case MEDIA_FILE_TYPE_H264b:
      h264_free(&h264);
      break;
    case MEDIA_FILE_TYPE_MP4V:
      mpg4v_free(&mpg4v);
      break;
    default:
      break;
  }

  mp4_free_avsync_stts(pAVSync);

  return rc;
}

static int mp4_createFromAudioList(MP4_SAMPLES_EXT_T *pSamplesExt,
                                   FILE_STREAM_T *pStreamIn, 
                                   enum MEDIA_FILE_TYPE mediaType,
                                   const char *pathOutMp4,
                                   unsigned int frameDeltaHz, int esdsObjTypeIdxPlus1) {
  AAC_DESCR_T aac;
  AMR_DESCR_T amr;
  MP4_CONTAINER_T *pMp4 = NULL;
  //MP4_MDAT_CONTENT_NODE_T *pContent = NULL;
  void *pContent = NULL;
  BOX_HANDLER_T *pMdatHandler = NULL;
  //BOX_STTS_ENTRY_LIST_T *pAVSync = NULL;
  int storeSamples = 1;//TODO: change to 0
  int rc = 0;

  if(!pStreamIn|| !pathOutMp4) {
    return -1;
  } 

  switch(mediaType) {
    case MEDIA_FILE_TYPE_AAC_ADTS:

      memset(&aac, 0, sizeof(aac));
      aac.decoderSP.frameDeltaHz = frameDeltaHz;
      aac.pStream = pStreamIn;

      if(!(pMp4 = createMp4FromAacADTS(pSamplesExt, &aac, storeSamples, esdsObjTypeIdxPlus1))) {
        return -1;
      }

      if(pSamplesExt) {
        pSamplesExt->aud.pSampleCbPrev = aac.content.pSampleCbPrev;
        pSamplesExt->aud.pSamplesBuf = aac.content.pSamplesBuf;
        pSamplesExt->aud.samplesBufSz = aac.content.samplesBufSz;
      } else if(storeSamples) {
        pContent = aac.content.pSamplesBuf;
      } else {
        pContent = aac.pStream;
      }

      break;

    case MEDIA_FILE_TYPE_AMRNB:

      memset(&amr, 0, sizeof(amr));
      amr.pStreamIn = pStreamIn;

      if(!(pMp4 = createMp4FromAmr(pSamplesExt, &amr, storeSamples))) {
        return -1;
      }

      if(pSamplesExt) {
        //rc = amr_createSampleListFromMp4Direct(&amr, pSamplesExt->pMp4, 0);
      } else {
        LOG(X_ERROR("AMR track creation from non mp4 source not supported"));
        rc = -1;
        //rc = amr_getSamples(&amr);
      }

      if(pSamplesExt) {
        //pSamplesExt->aud.pSampleCbPrev = aac.content.pSampleCbPrev;
        //pSamplesExt->aud.pSamplesBuf = aac.content.pSamplesBuf;
        //pSamplesExt->aud.samplesBufSz = aac.content.samplesBufSz;
      } else if(storeSamples) {
        //pContent = aac.content.pSamplesBuf;
      } else {
        //pContent = aac.pStream;
      }

      break;

    default:
      LOG(X_ERROR("Unsupported audio media type %s"), codecType_getCodecDescrStr(mediaType));
      return -1;
  }

  if(rc == 0) {

    if(pSamplesExt) {
      if(!(pContent = mergeMdatContent(pSamplesExt, pSamplesExt->pBoxStcoAud->pSamples[0]))) {
        LOG(X_ERROR("Unable to merge audio and video data content"));
        rc = -1;
      }
      free(pSamplesExt->stchkAud.p_stchk);
    }
  }

  if(rc == 0) {
    if((pMdatHandler = (BOX_HANDLER_T *) mp4_findHandlerInChain(pMp4->proot->handler, *((uint32_t *) "mdat") ))) {
      if(storeSamples) {
        pMdatHandler->writefunc = write_box_mdat_custom;
      } else {
        pMdatHandler->writefunc = write_box_mdat_from_file_aacAdts;
      }
    }

    rc = mp4_write(pMp4, pathOutMp4, pContent);
  }

  if(!pSamplesExt) {
    mp4_close(pMp4);
  }

  switch(mediaType) {
    case MEDIA_FILE_TYPE_AAC_ADTS:
      aac_free(&aac);
      break;
    case MEDIA_FILE_TYPE_AMRNB:
      amr_free(&amr);
      break;
    default:
      break;
  }

  return rc;
}

#if 0
static int mp4_addFromAacAdts(MP4_SAMPLES_EXT_T *pSamplesExt,
                              FILE_STREAM_T *pStreamInAac, 
                              const char *pathOutMp4,
                              unsigned int frameDeltaHz,
                              int esdsObjTypeIdxPlus1) {

  AAC_DESCR_T aac;
  MP4_CONTAINER_T *pMp4 = NULL; 
  BOX_HANDLER_T *pMdatHandler = NULL;
  int rc = -1;
  MP4_MDAT_CONTENT_NODE_T *pJoinedNodes = NULL;

  if(!pStreamInAac || !pathOutMp4 || !pSamplesExt || !pSamplesExt->pMp4) {
    return -1;
  } 

  memset(&aac, 0, sizeof(aac));
  aac.decoderSP.frameDeltaHz = frameDeltaHz;
  aac.pStream = pStreamInAac;

  if((pMp4 = createMp4FromAacADTS(pSamplesExt, &aac, 1, esdsObjTypeIdxPlus1)) == NULL) {
    aac_free(&aac);
    return -1;
  }

  pSamplesExt->aud.pSampleCbPrev = aac.content.pSampleCbPrev;
  pSamplesExt->aud.pSamplesBuf = aac.content.pSamplesBuf;
  pSamplesExt->aud.samplesBufSz = aac.content.samplesBufSz;

  pJoinedNodes = mergeMdatContent(pSamplesExt, pSamplesExt->pBoxStcoAud->pSamples[0]);

  free(pSamplesExt->stchkAud.p_stchk);

  if((pMdatHandler = (BOX_HANDLER_T *) mp4_findHandlerInChain(pMp4->proot->handler, *((uint32_t *) "mdat") ))) {
    pMdatHandler->writefunc = write_box_mdat_custom;
  }
  rc = mp4_write(pMp4, pathOutMp4, pJoinedNodes);

  aac_free(&aac);

  return rc;
}

static int mp4_createFromAacAdts(FILE_STREAM_T *pStreamInAac, const char *pathOutMp4,
                                 unsigned int frameDeltaHz, int esdsObjTypeIdxPlus1) {

  AAC_DESCR_T aac;
  MP4_CONTAINER_T *pMp4 = NULL; 
  BOX_HANDLER_T *pMdatHandler = NULL;
  void *pArg = NULL;
  int storeSamples = 1;//TODO: change to 0
  int rc = -1;

  if(!pStreamInAac) {
    return -1;
  } 

  memset(&aac, 0, sizeof(aac));
  aac.decoderSP.frameDeltaHz = frameDeltaHz;
  aac.pStream = pStreamInAac;

  if((pMp4 = createMp4FromAacADTS(NULL, &aac, storeSamples, esdsObjTypeIdxPlus1))) {

    if((pMdatHandler = (BOX_HANDLER_T *) mp4_findHandlerInChain(pMp4->proot->handler, *((uint32_t *) "mdat") ))) {
      if(storeSamples) {
        pMdatHandler->writefunc = write_box_mdat_custom;
        pArg = aac.content.pSamplesBuf;
      } else {
        pMdatHandler->writefunc = write_box_mdat_from_file_aacAdts;
        pArg = aac.pStream;
      }
    }
    rc = mp4_write(pMp4, pathOutMp4, pArg);
   
    mp4_close(pMp4);

  } else {
    return -1;
  }

  aac_free(&aac);

  return rc;
}
#endif // 0

int mp4_create(const char *pathOut,
               FILE_STREAM_T *pStreamInAudio,
               FILE_STREAM_T *pStreamInVideo,
               double fpsAudio,
               double fpsVideo,
               enum MEDIA_FILE_TYPE audioType, 
               enum MEDIA_FILE_TYPE videoType, 
               const char *avsyncin,
               int esdsObjTypeIdxPlus1) {

  FILE_HANDLE fp;
  AAC_DESCR_T *pAac = NULL;
  H264_AVC_DESCR_T h264;
  MPG4V_DESCR_T mpg4v;
  MP4_SAMPLES_EXT_T samplesExt;
  MP4_CONTAINER_T *pMp4 = NULL;
  MP4_TRAK_AVCC_T mp4BoxSetAvc1;
  MP4_TRAK_MP4V_T mp4BoxSetMp4v;
  MP4_TRAK_MP4A_T mp4BoxSetMp4a;
  MP4_TRAK_SAMR_T mp4BoxSetSamr;
  BOX_T *pBoxMdat = NULL;
  BOX_T **ppBox = NULL;
  char buf[16];
  char pathOutTmp[256];
  size_t sz;
  int rc = -1;

  if(pStreamInAudio && pStreamInVideo) {
    LOG(X_ERROR("Audio and Video simultaneous add not implemented."));
    return -1;
  }

  if((fp = fileops_Open(pathOut, O_RDONLY)) != FILEOPS_INVALID_FP) {
    fileops_Close(fp);

    if((sz = strlen(pathOut) + 4) > sizeof(pathOutTmp)) {
      LOG(X_ERROR("Output path is too long: '%s'"), pathOut);
      return -1;
    }
    strncpy(pathOutTmp, pathOut, sizeof(pathOutTmp) - 4);
    strncat(pathOutTmp, "X", sizeof(pathOutTmp));

    if((pMp4 = mp4_open(pathOut, 1)) == NULL) {
      LOG(X_ERROR("The file %s may not be not be a properly formatted mp4."), pathOut);
      return -1;
    }


    // TODO: allow insertion into container w/ unrecognized aud/vid track
    memset(&mp4BoxSetAvc1, 0, sizeof(mp4BoxSetAvc1));
    memset(&mp4BoxSetMp4v, 0, sizeof(mp4BoxSetMp4v));
    memset(&mp4BoxSetMp4a, 0, sizeof(mp4BoxSetMp4a));
    memset(&mp4BoxSetSamr, 0, sizeof(mp4BoxSetSamr));

    if(!(mp4BoxSetAvc1.pAvc1 = (BOX_AVC1_T *) mp4_loadTrack(pMp4->proot, &mp4BoxSetAvc1.tk, 
                                    *((uint32_t *) "avc1"), 1))) {
      mp4BoxSetMp4v.pMp4v = (BOX_MP4V_T *) mp4_loadTrack(pMp4->proot, &mp4BoxSetMp4v.tk, 
                                    *((uint32_t *) "mp4v"), 1);
    }
    if(!(mp4BoxSetMp4a.pMp4a = (BOX_MP4A_T *) mp4_loadTrack(pMp4->proot, &mp4BoxSetMp4a.tk, 
                                    *((uint32_t *) "mp4a"), 1))) {
      mp4BoxSetSamr.pSamr = (BOX_SAMR_T *) mp4_loadTrack(pMp4->proot, &mp4BoxSetMp4a.tk, 
                                    *((uint32_t *) "samr"), 1);
    }
    
    ppBox = NULL;
    if(pStreamInAudio) {
      if(mp4BoxSetMp4a.pMp4a) {
        ppBox = &mp4BoxSetMp4a.tk.pTrak;
        snprintf(buf, sizeof(buf), "mp4a");
      } else if(mp4BoxSetSamr.pSamr) {
        ppBox = &mp4BoxSetSamr.tk.pTrak;
        snprintf(buf, sizeof(buf), "samr");
      }
    } else if(pStreamInVideo) {
      if(mp4BoxSetAvc1.pAvc1) {
        ppBox = &mp4BoxSetAvc1.tk.pTrak;
        snprintf(buf, sizeof(buf), "avc1");
      } else if(mp4BoxSetMp4v.pMp4v) {
        ppBox = &mp4BoxSetMp4v.tk.pTrak;
        snprintf(buf, sizeof(buf), "mp4v");
      }
    }

    if(ppBox) {
      LOG(X_INFO("Removing %s track"), buf);
      if(mp4_deleteTrack(pMp4, *ppBox) < 0) {
        LOG(X_ERROR("Unable to delete %s track"), buf);
        mp4_close(pMp4);
        return -1;
      }
      *ppBox = NULL;
    }
   
    memset(&samplesExt, 0, sizeof(samplesExt));

    if((pBoxMdat = mp4_findBoxChild(pMp4->proot, *((uint32_t *) "mdat")))) {
      pBoxMdat->szhdr = 8;
      pBoxMdat->szdata = 0;
      pBoxMdat->szdata_ownbox = 0;
      mp4_updateBoxSizes(pMp4->proot);
    }

    if(pStreamInVideo) {

      if((pAac = aac_getSamplesFromMp4(pMp4, &samplesExt.stchkAud, 0)) == NULL) {
        mp4_close(pMp4);
        return -1;
      }

      samplesExt.pMp4 = pMp4;
      samplesExt.aud.pSampleCbPrev = pAac->content.pSampleCbPrev;
      samplesExt.aud.pSamplesBuf = pAac->content.pSamplesBuf;
      samplesExt.aud.samplesBufSz = pAac->content.samplesBufSz;
      samplesExt.pBoxStcoAud = mp4BoxSetMp4a.tk.pStco;

      LOG(X_DEBUG("Adding video track in %s"), pathOut);

      rc = mp4_createFromVideoList(&samplesExt, pStreamInVideo, videoType, pathOutTmp, fpsVideo, avsyncin);
 

      aac_close(pAac);
      free(samplesExt.stchkAud.p_stchk);

    } else if(pStreamInAudio) {

      rc = 0;
      if(mp4BoxSetAvc1.pAvc1) { 
        memset(&h264, 0, sizeof(h264));

        if(h264_createNalListFromMp4(&h264, pMp4, &samplesExt.stchkVid, 0) != 0) {
          mp4_close(pMp4);
          return -1;
        }

        samplesExt.pMp4 = pMp4;
        samplesExt.vid.pSampleCbPrev = h264.pNalCbPrev;
        samplesExt.vid.pSamplesBuf = h264.pNalsBuf ? &h264.pNalsBuf->content : NULL;
        samplesExt.vid.samplesBufSz = h264.nalsBufSz;
        samplesExt.pBoxStcoVid = mp4BoxSetAvc1.tk.pStco;

      } else if(mp4BoxSetMp4v.pMp4v) {
        memset(&mpg4v, 0, sizeof(mpg4v));
 
        if(mpg4v_createSampleListFromMp4(&mpg4v, pMp4, &samplesExt.stchkVid, 0) != 0) {
          mp4_close(pMp4);
          return -1;
        }

        samplesExt.pMp4 = pMp4;
        samplesExt.vid.pSampleCbPrev = mpg4v.content.pSampleCbPrev;
        samplesExt.vid.pSamplesBuf = mpg4v.content.pSamplesBuf;
        samplesExt.vid.samplesBufSz = mpg4v.content.samplesBufSz;
        samplesExt.pBoxStcoVid = mp4BoxSetMp4v.tk.pStco;

      }

      LOG(X_DEBUG("Adding audio track in %s"), pathOut);

      //rc = mp4_addFromAacAdts(&samplesExt, pStreamInAudio, pathOutTmp, (unsigned int) fpsAudio, esdsObjTypeIdxPlus1);
      rc = mp4_createFromAudioList(&samplesExt, pStreamInAudio, audioType, pathOutTmp, (unsigned int) fpsAudio, esdsObjTypeIdxPlus1);

      if(mp4BoxSetAvc1.pAvc1) { 
        h264_free(&h264);
      } else if(mp4BoxSetMp4v.pMp4v) {
 
      }

      if(samplesExt.stchkVid.p_stchk) {
        free(samplesExt.stchkVid.p_stchk);
      }

    }

    mp4_close(pMp4);

    if(rc == 0 && fileops_MoveFile(pathOutTmp, pathOut) != 0) {
      LOG(X_ERROR("Failed to move file '%s' to '%s'"), pathOutTmp, pathOut); 
      return -1;
    }

  } else if(pStreamInAudio) {
    //rc = mp4_createFromAacAdts(pStreamInAudio, pathOut, (unsigned int) fpsAudio, esdsObjTypeIdxPlus1); 
    rc = mp4_createFromAudioList(NULL, pStreamInAudio, audioType, pathOut, (unsigned int) fpsAudio, esdsObjTypeIdxPlus1);
  } else if(pStreamInVideo) {

    rc = mp4_createFromVideoList(NULL, pStreamInVideo, videoType, pathOut, fpsVideo, avsyncin);
  }

  return rc;
}

#endif // VSX_CREATE_CONTAINER


