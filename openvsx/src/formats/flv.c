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


typedef int (* CBFLV_VIDDATA) (FLV_CONTAINER_T *, FLV_TAG_VIDEO_T *, void *);
typedef int (* CBFLV_AUDDATA) (FLV_CONTAINER_T *, FLV_TAG_AUDIO_T *, void *);
typedef int (* CBFLV_SCRIPTDATA) (FLV_CONTAINER_T *, FLV_TAG_SCRIPT_T *, void *);


static const FLV_AMF_VAL_T *flv_script_find(const FLV_CONTAINER_T *pFlv, 
                                               const char *keyName,
                                               const FLV_AMF_T *pEntry);

static double hton_dbl(double arg) {
  double d = 0;
  uint32_t ui[3];

  memcpy(ui, &arg, 8);
  ui[2] = htonl(ui[0]);
  ui[0] = htonl(ui[1]);
  ui[1] = ui[2];
  memcpy(&d, ui, 8);

  return  d;
}

static const char *flv_getAudioCodecStr(unsigned int codecIdx) {

  static const char *strAudioCodecs[] = { "Linear PCM (platform endian)",
                                        "ADPCM",
                                        "MP3",
                                        "Linear PCM (little endian)",
                                        "Nellymoser 16Khz mono",
                                        "Nellymoser 8Khz mono",
                                        "Nellymoser",
                                        "G.711 a-law",
                                        "G.711 u-law",
                                        "Reserved",
                                        "AAC",
                                        "Speex",
                                        "Invalid",
                                        "Invalid",
                                        "MP3 8KHz",
                                        "Devicde-specific",
                                        "Invalid" };
  static const unsigned int szAudioCodecs = 16; 
  if(codecIdx < szAudioCodecs) {
    return strAudioCodecs[codecIdx];
  } else {
    return strAudioCodecs[szAudioCodecs];
  }
}

static const char *flv_getVideoCodecStr(unsigned int codecIdx) {

  static const char *strVideoCodecs[] = { "JPEG",
                                        "Sorenson H.263",
                                        "Screen Video",
                                        "On2 VP6",
                                        "On2 VP6 with alpha channel",
                                        "Screen Video v2",
                                        "AVC",
                                        "Invalid" };
  static const unsigned int szVideoCodecs = 16; 
  if(codecIdx < szVideoCodecs) {
    return strVideoCodecs[codecIdx];
  } else {
    return strVideoCodecs[szVideoCodecs];
  }
}
/*
static void flv_dump_hdr(FLV_CONTAINER_T *pFlv, FILE *fp, FLV_TAG_HDR_T *pHdr) {

  fprintf(fp, "szprev: %u, type:%u, sz:%u, ts:%u\n",
          pHdr->szprev, pHdr->type, pHdr->size32, pHdr->timestamp32);
 
}
*/

static const double *flv_script_find_dbl(FLV_CONTAINER_T *pFlv,
                                  FLV_AMF_T *pEntry, 
                                  const char *keyName) {

  const FLV_AMF_VAL_T *pVal;

  if(!(pVal = flv_script_find(pFlv, keyName, pEntry))) {
    LOG(X_WARNING("Unable to find '%s' in meta data "), keyName);
  } else if(pVal->type != FLV_AMF_TYPE_NUMBER) {
    LOG(X_WARNING("Invalid '%s' type: %u"), keyName, pVal->type);
  } else {
    return &pVal->u.d;
  }

  return NULL;
}

static int flv_init_script_vars(FLV_CONTAINER_T *pFlv) {
  const double *pd;

  pFlv->timescaleHz = 0;
  pFlv->sampledeltaHz = 0;
  pFlv->vidtimescaleHz = 0;
  pFlv->vidsampledeltaHz = 0;
  pFlv->durationTotHz = 0;

  if((pd = flv_script_find_dbl(pFlv, &pFlv->scriptTmp.entry, "framerate"))) {
//fprintf(stdout, "framerate: %f\n", *pd);
    if(vid_convert_fps(*pd, &pFlv->timescaleHz, &pFlv->sampledeltaHz) != 0) {
      LOG(X_WARNING("Unable to flv script convert 'framerate':%.3f"), *pd);
    } else {
      LOG(X_DEBUG("Found flv script 'framerate':%.3f (%u / %u)"), *pd, 
                                       pFlv->timescaleHz, pFlv->sampledeltaHz);
    }
  }

  pFlv->vidtimescaleHz = pFlv->timescaleHz;
  pFlv->vidsampledeltaHz = pFlv->sampledeltaHz;
  //fprintf(stderr, " script timescale %d / %d\n", pFlv->vidtimescaleHz, pFlv->vidsampledeltaHz);
  if((pd = flv_script_find_dbl(pFlv, &pFlv->scriptTmp.entry, "totalduration"))) {
    pFlv->durationTotHz = (uint64_t) (*pd * pFlv->timescaleHz);
//fprintf(stdout, "totalduration: %f %llu\n", *pd, pFlv->durationTotHz);
  }

  if((pd = flv_script_find_dbl(pFlv, &pFlv->scriptTmp.entry, "width"))) {
//fprintf(stdout, "width: %f\n", *pd);
    pFlv->vidCfg.vidDescr.resH = (uint16_t) *pd;
  }
  if((pd = flv_script_find_dbl(pFlv, &pFlv->scriptTmp.entry, "height"))) {
//fprintf(stdout, "ht: %f\n", *pd);
    pFlv->vidCfg.vidDescr.resV = (uint16_t) *pd;
  }

  return 0;
}

static void flv_amf_free_string(FLV_AMF_STR_T *pStr) {

  if(pStr->pBuf) {
    free(pStr->pBuf);
    pStr->pBuf = NULL;
  }
  pStr->p = NULL;
  pStr->len = 0;

}

void flv_amf_free(FLV_AMF_T *pEntry) {
  unsigned int idx;
 
  if(!pEntry) {
    return;
  }

  flv_amf_free_string(&pEntry->key);

  switch(pEntry->val.type) {
    case FLV_AMF_TYPE_ECMA:
    case FLV_AMF_TYPE_STRICTARR:

      if(pEntry->val.u.arr.pEntries) {
        for(idx = 0; idx < pEntry->val.u.arr.count; idx++) {
          //fprintf(stderr, "freeing ecma data [%u/%d]\n", idx, pEntry->val.u.arr.count);
          flv_amf_free(&  ((FLV_AMF_T *)pEntry->val.u.arr.pEntries)[idx]);
        }
    //fprintf(stderr, "freeing ecma array size:%d\n", pEntry->val.u.arr.cntAlloced);
        free(pEntry->val.u.arr.pEntries);
        pEntry->val.u.arr.pEntries = NULL;
      }
      pEntry->val.u.arr.count = 0;
      pEntry->val.u.arr.cntAlloced = 0;
      break;

    case FLV_AMF_TYPE_STRING:

//fprintf(stdout, "freeing str data:'%s'\n", pEntry->val.u.str.p);
      flv_amf_free_string(&pEntry->val.u.str);
      break;
  }
 
}

const unsigned char *flv_amf_read_string(FLV_AMF_STR_T *pStr, BYTE_STREAM_T *bs, 
                                         int alloc) {

  unsigned char *p = NULL;

  if(!pStr || !bs) {
    return NULL;
  }

  pStr->len = htons(*((uint16_t *) &bs->buf[bs->idx]));
  bs->idx += 2;

  if(pStr->len > bs->sz - bs->idx) {
    LOG(X_ERROR("Invalid amf string value length: %u / %d"), pStr->len, bs->sz - bs->idx);
    return NULL;
  }

  if(alloc) {
    p = pStr->pBuf = (unsigned char *) avc_calloc(1, pStr->len + 1);
    memcpy(pStr->pBuf, &bs->buf[bs->idx], pStr->len);
  } else {
    pStr->pBuf = NULL;
  }

  pStr->p = &bs->buf[bs->idx];
  if(!p) {
    p = pStr->p;
  }
  bs->idx += pStr->len;

  return p;
}

static const void *flv_amf_read_val(FLV_AMF_VAL_T *pVal, BYTE_STREAM_T *bs, int alloc) {
  //unsigned int count;
  static int emptyval = 0;

  pVal->type = bs->buf[bs->idx++];

  //fprintf(stderr, "flv_amf_read_val type:0x%x alloc:%d idx:%d/%d\n", pVal->type, alloc, bs->idx, bs->sz);

  switch(pVal->type) {

    case FLV_AMF_TYPE_NUMBER:
      if(bs->idx + 8 > bs->sz) {
        LOG(X_ERROR("Invalid flv number value length 0x%x / 0x%x"), bs->idx + 8, bs->sz);
        return NULL; 
      }
      pVal->u.d = hton_dbl(*((double *)&bs->buf[bs->idx]));
      bs->idx += 8;
      return &pVal->u.d;

    case FLV_AMF_TYPE_BOOL:
      if(bs->idx + 1 > bs->sz) {
        LOG(X_ERROR("Invalid flv bool value length 0x%x / 0x%x"), bs->idx + 1, bs->sz);
        return NULL; 
      }
      pVal->u.b = bs->buf[bs->idx++];
      return &pVal->u.b; 

    case FLV_AMF_TYPE_OBJECT:
    case FLV_AMF_TYPE_NULL:
    case FLV_AMF_TYPE_UNDEFINED:

      return &emptyval;

    case FLV_AMF_TYPE_STRING:
      return flv_amf_read_string(&pVal->u.str, bs, alloc);

    case FLV_AMF_TYPE_STRICTARR:
    case FLV_AMF_TYPE_ECMA:

      if(bs->idx + 4 > bs->sz) {
        LOG(X_ERROR("Invalid flv array value length 0x%x / 0x%x"), bs->idx + 4, bs->sz);
        return NULL; 
      }
      if((pVal->u.arr.count = htonl(*((uint32_t *) &bs->buf[bs->idx]))) == 0) {
        // TODO: if array size is not specified, the array should be dynamically
        // grown for each item read
        if(alloc) {
          pVal->u.arr.count = 30;
          LOG(X_WARNING("%s size not known - allocating %d"), 
            pVal->type == FLV_AMF_TYPE_ECMA ? "ECMA" : "StrictArray", pVal->u.arr.count);
        } else {
          pVal->u.arr.count = 1;
        }
      }

      bs->idx += 4;
      if(alloc && (pVal->u.arr.cntAlloced = pVal->u.arr.count) > 30) {
        LOG(X_ERROR("FLV refusing to allocate array of size %d"), pVal->u.arr.cntAlloced);
        pVal->u.arr.cntAlloced = 0;
        return NULL;
      }

      //if(alloc)fprintf(stderr, "allocating type 0x%x array of %d (0x%x)at idx:%d\n", pVal->type, pVal->u.arr.cntAlloced, pVal->u.arr.count, bs->idx);
      if(alloc && !(pVal->u.arr.pEntries = avc_calloc(pVal->u.arr.cntAlloced, sizeof(FLV_AMF_T)))) {
        return NULL; 
      }

      return pVal->u.arr.pEntries;

    case FLV_AMF_TYPE_DATE:
      if(bs->idx + 10 > bs->sz) {
        LOG(X_ERROR("Invalid date value length 0x%x / 0x%x"), bs->idx + 10, bs->sz);
        return NULL; 
      }
      pVal->u.date.val = hton_dbl(*((double *)&bs->buf[bs->idx]));
      bs->idx += 8;
      pVal->u.date.offset = htons(*((uint16_t *)&bs->buf[bs->idx]));
      bs->idx += 2;

       return &pVal->u.date;
    default:
      LOG(X_WARNING("Unhandled flv script data type %u at script offset %u"), 
                    pVal->type, bs->idx);
      break;
  }

  return NULL;
}

int flv_amf_read(int amfKeyType, FLV_AMF_T *pEntry, BYTE_STREAM_T *bs, int alloc) {

  int rc = 0;

  //fprintf(stderr, "flv_amf_reading type: 0x%x 0x%x\n", amfKeyType, bs->buf[bs->idx]);
  
  switch(amfKeyType) {
    case FLV_AMF_TYPE_STRING:
    case FLV_AMF_TYPE_OBJECT:

      // This ghetto non-recursive parser ignores start and end of object types
      if(bs->buf[bs->idx] == FLV_AMF_TYPE_OBJECT) {
        bs->idx++;
      }

      if(!flv_amf_read_string(&pEntry->key, bs, alloc)) {
        return -1;
      }

      break;
    case FLV_AMF_TYPE_NULL:
      memset(&pEntry->key, 0, sizeof(FLV_AMF_STR_T));
      if(bs->idx == bs->sz) {
        memset(&pEntry->val, 0, sizeof(FLV_AMF_STR_T));
        return 0;
      }
      break;
    case 0:
      // Check for end of object (0x00 0x00 0x09) sequence
      if(bs->idx + 2 < bs->sz && bs->buf[bs->idx + 1] == 0x00 && 
         bs->buf[bs->idx + 2] == 0x09) {
        bs->idx += 2;
        memset(&pEntry->key, 0, sizeof(FLV_AMF_STR_T));
        memset(&pEntry->val, 0, sizeof(FLV_AMF_STR_T));
        return 0;
      }
    default:
      LOG(X_ERROR("Unhandled amf object key type: 0x%x [%d]/%d"), amfKeyType, bs->idx, bs->sz);
      return -1;
  }

  if(!flv_amf_read_val(&pEntry->val, bs, alloc)) {
    // if alloc is not set and the type was an array
    // then we did not iterate through the entire contents
    if(!(!alloc && (pEntry->val.type == FLV_AMF_TYPE_ECMA ||
                 pEntry->val.type == FLV_AMF_TYPE_STRICTARR))) {
      rc = -1;
    }
  }

  return rc;
}

static const FLV_AMF_VAL_T *flv_script_find(const FLV_CONTAINER_T *pFlv, 
                                               const char *keyName,
                                               const FLV_AMF_T *pEntry) {

  unsigned int idx;
  const FLV_AMF_VAL_T *pVal = NULL;

  if(!pEntry || !keyName) {
    return NULL;
  } 

  if(pEntry->key.len > 0 && pEntry->key.pBuf && !strcmp((char *)pEntry->key.pBuf, keyName)) {
    return &pEntry->val;
  }

  switch(pEntry->val.type) {
    case FLV_AMF_TYPE_ECMA:
    for(idx = 0; idx < pEntry->val.u.arr.count; idx++) {
      if((pVal = flv_script_find(pFlv, keyName,
                    &((FLV_AMF_T *)pEntry->val.u.arr.pEntries)[idx]))) {
        return pVal;
      }
    }
    break;
  }

  return NULL;
}

int flv_parse_onmeta(FLV_AMF_T *pEntry, BYTE_STREAM_T *bs) {
  const void *p;
  unsigned int idxEntry = 0;

  if(!pEntry || !bs || bs->sz < 1) {
    return -1;
  }

  //fprintf(stderr, "flv_parse_onmeta [%d/%d] 0x%x 0x%x 0x%x\n", bs->idx, bs->sz, bs->buf[bs->idx], bs->buf[bs->idx+1], bs->buf[bs->idx+2]);

  if(bs->buf[bs->idx++] != FLV_AMF_TYPE_STRING) {
    LOG(X_WARNING("First script entry is not a string (0x%x"), bs->buf[0]);
    return -1;
  }

  if(!(p = flv_amf_read_string(&pEntry->key, bs, 1)) || strncmp(p, "onMetaData", 10)) {
    LOG(X_WARNING("Unable to find onMetaData script entry"));
    return -1;
  }

  if(!(p = flv_amf_read_val(&pEntry->val, bs, 1))) {
    return -1;
  }

  if(pEntry->val.type != FLV_AMF_TYPE_ECMA && pEntry->val.type != FLV_AMF_TYPE_OBJECT) {
    LOG(X_ERROR("Unexpected type type for onMetaData 0x%x"), pEntry->val.type);
    return -1;
  }

  //for(idxEntry = 0; idxEntry < pEntry->val.u.arr.count; idxEntry++) {
  while(bs->idx + 3 < bs->sz) {

    // 0x00 0x00 0x09 denotes end of object type
    if(bs->buf[bs->idx] == 0 && bs->buf[bs->idx + 1] == 0 && bs->buf[bs->idx + 2] == 9) {
      bs->idx += 3;
      continue;
    } else if(idxEntry >= pEntry->val.u.arr.cntAlloced) {
      break;
    }

    //fprintf(stderr, "reading entry 0x%x 0x%x 0x%x\n", bs->buf[bs->idx], bs->buf[bs->idx+1], bs->buf[bs->idx+2]);
    if(flv_amf_read(FLV_AMF_TYPE_STRING,
                      &((FLV_AMF_T *)pEntry->val.u.arr.pEntries)[idxEntry], bs, 1) < 0) {
      LOG(X_ERROR("Failed to parse mixed array entry [%u/%u] [%u/%u]"), 
                   idxEntry, pEntry->val.u.arr.count, bs->idx, bs->sz);
      break;
    }
    //fprintf(stderr, "entry[%u/%u].key='%s' [%d/%d]\n", idxEntry, pEntry->val.u.arr.count, (((FLV_AMF_T *)pEntry->val.u.arr.pEntries)[idxEntry]).key.p, bs->idx, bs->sz);
    idxEntry++;
  }

  return 0;
}

static FLV_TAG_SCRIPT_T *flv_read_tag_script(FLV_CONTAINER_T *pFlv, FLV_TAG_SCRIPT_T *pTag) {
  unsigned char arena[4096];
  //FLV_AMF_T *pEntry = &pTag->entry;
  BYTE_STREAM_T bs;

  if((bs.sz = (int) pTag->hdr.size32) > sizeof(arena)) {
    bs.sz = sizeof(arena);
  } 
  bs.buf = arena;
  bs.idx = 0; 

  if(ReadFileStream(pFlv->pStream, bs.buf, bs.sz) < 0) {
    return NULL;
  }

  flv_amf_free(&pFlv->scriptTmp.entry);

  //fprintf(stderr, "%d/%d size32:%d szhdr:%d\n", bs.idx, bs.sz, pTag->hdr.size32, pTag->hdr.szhdr);

  if(flv_parse_onmeta(&pTag->entry, &bs) < 0) {
    LOG(X_ERROR("Failed to parse onMetaData entry at file:0x%llx"),
                  pTag->hdr.fileOffset + bs.idx);
    flv_amf_free(&pTag->entry);
    return NULL;
  }

  flv_init_script_vars(pFlv);

  //fprintf(stderr, "end idx:%d/%d\n", bs.idx, bs.sz);

  return pTag;
}

static int flv_read_tag_video_avc(FLV_CONTAINER_T *pFlv, FLV_TAG_VIDEO_T *pTagV) {
  unsigned int len;
  uint32_t tmp = 0;

  if(pTagV->hdr.size32 < 4) {
    LOG(X_ERROR("Invalid flv avc tag (file:0x%llx) body length: %u"), 
               pTagV->hdr.fileOffset, pTagV->hdr.size32);
    return -1;
  }

  if(ReadFileStream(pFlv->pStream, &pTagV->avc, 4) < 0) {
    return -1;
  }

  pTagV->szvidhdr += 4;

  if(pTagV->avc.pkttype == FLV_VID_AVC_PKTTYPE_SEQHDR) {
    //
    //AVCDecoderConfigurationRecord
    //

    if((len = pTagV->hdr.size32 - 5) > sizeof(pFlv->arena)) {
      LOG(X_ERROR("flv avc decoder config length too long: %u"), len);
      return -1;
    }
    if(ReadFileStream(pFlv->pStream, &pFlv->arena, len) < 0) {
      return -1;
    }
//fprintf(stdout, "vcfg %d %2.2x %2.2x \n", pTagV->hdr.size32, pFlv->arena[0], pFlv->arena[1]);
    if(pFlv->vidCfg.pAvcC) {
      avcc_freeCfg(pFlv->vidCfg.pAvcC);
    } else {
      pFlv->vidCfg.pAvcC = (AVC_DECODER_CFG_T *) avc_calloc(1, sizeof(AVC_DECODER_CFG_T));
    }

    if(avcc_initCfg(pFlv->arena, len,  pFlv->vidCfg.pAvcC)) {
      return -1; 
    }
    pFlv->vidCfg.vidDescr.mediaType = MEDIA_FILE_TYPE_H264b;
//fprintf(stdout, "sps %u , pps %u avcc prfx len:%u\n", pFlv->vidCfg.pAvcC->numsps, pFlv->vidCfg.pAvcC->numpps, pFlv->vidCfg.pAvcC->lenminusone+1);

  } else if(pTagV->avc.pkttype == FLV_VID_AVC_PKTTYPE_NALU) {
    memcpy(&tmp, pTagV->avc.comptime, MIN(sizeof(tmp), sizeof(pTagV->avc.comptime)));
    pTagV->avc.comptime32 = htonl(tmp << 8);
    //pTagV->avc.comptime32 = htonl(*((uint32_t *)pTagV->avc.comptime) << 8);
    // check for negative 24 bit value
    if(pTagV->avc.comptime32 & 0x00800000) {
      pTagV->avc.comptime32 |= 0xff000000;
    }

    //static int g_vidfr; fprintf(stderr, "flv avc comptime:%d 0x%x 0x%x 0x%x length:%d, fr:%d\n", pTagV->avc.comptime32, pTagV->avc.comptime[0], pTagV->avc.comptime[1], pTagV->avc.comptime[2], pTagV->hdr.size32, g_vidfr++);

  } else if(pTagV->avc.pkttype == FLV_VID_AVC_PKTTYPE_ENDSEQ) {
    // empty

  } else {
    LOG(X_ERROR("Invalid FLV avc packet type: %u at file: 0x%llx"), 
                pTagV->avc.pkttype, pTagV->hdr.fileOffset);
    return -1;
  }

//fprintf(stdout, "avc file:0x%x, sz:0x%x(%u) 0x%x %u\n", pTagV->hdr.fileOffset+pTagV->hdr.szhdr, pTagV->hdr.size32, pTagV->hdr.size32, pTagV->frmtypecodec, pTagV->avc.pkttype);

  return 0;
}

static FLV_TAG_VIDEO_T *flv_read_tag_video(FLV_CONTAINER_T *pFlv, FLV_TAG_VIDEO_T *pTagV) {

//fprintf(stdout, "video file:0x%x +%d, sz:0x%x(%u)\n", pHdr->fileOffset,pHdr->szhdr, pHdr->size32, pHdr->size32);
//fprintf(stderr, "video 0x%x 0x%x\n", pFlv->pStream->offset, pTag->hdr.size32);

  if(ReadFileStream(pFlv->pStream, &pTagV->frmtypecodec, 1) < 0) {
    return NULL;
  }

  pTagV->szvidhdr = 1;

  if((pTagV->frmtypecodec & 0x0f) != FLV_VID_CODEC_AVC) {
    LOG(X_ERROR("Unsupported video codec %s (%u)"), 
       flv_getVideoCodecStr(pTagV->frmtypecodec & 0x0f),pTagV->frmtypecodec & 0x0f);
    return pTagV;
  }
  
  if(flv_read_tag_video_avc(pFlv, pTagV) != 0) {
    return NULL;
  }

//fprintf(stderr, "0x%x\n", pFlv->pStream->offset);
  return pTagV;
}

static int flv_read_tag_audio_aac(FLV_CONTAINER_T *pFlv, FLV_TAG_AUDIO_T *pTagA) {
  unsigned int len;

  if(pTagA->hdr.size32 < 4) {
    LOG(X_ERROR("Invalid flv aac tag (file:0x%llx) body length: %u"), 
               pTagA->hdr.fileOffset, pTagA->hdr.size32);
    return -1;
  }

  if(ReadFileStream(pFlv->pStream, &pTagA->aac, 1) < 0) {
    return -1;
  }

  pTagA->szaudhdr++;

  if(pTagA->aac.pkttype == FLV_AUD_AAC_PKTTYPE_SEQHDR) {

    if((len = pTagA->hdr.size32 - 2) > sizeof(pFlv->arena)) {
      LOG(X_ERROR("flv aac decoder config length too long: %u"), len);
      return -1;
    }
    if(ReadFileStream(pFlv->pStream, &pFlv->arena, len) < 0) {
      return -1;
    }

    if(esds_decodeDecoderSp(pFlv->arena, len, &pFlv->audCfg.aacCfg) != 0) {
      LOG(X_ERROR("Failed to decode aac decoder config"));
      return -1;
    }

    pFlv->audCfg.audDescr.clockHz = esds_getFrequencyVal(ESDS_FREQ_IDX(pFlv->audCfg.aacCfg));
    // TODO: need to do proper translation of channelIdx to # of channels
    pFlv->audCfg.audDescr.channels = pFlv->audCfg.aacCfg.channelIdx;
    pFlv->audCfg.audDescr.mediaType = MEDIA_FILE_TYPE_AAC_ADTS;

    //fprintf(stdout, "aac clock:%u channels:%u\n", pFlv->audCfg.audDescr.clockHz, pFlv->audCfg.audDescr.channels);
    //fprintf(stdout, "aac type: %s (0x%x), freq: %uHZ (0x%x), channel: %s (0x%x) %u samples/frame }\n",
    //              esds_getMp4aObjTypeStr(pFlv->audCfg.aacSp.objTypeIdx), 
    //              pFlv->audCfg.aacSp.objTypeIdx,
    //              esds_getFrequencyVal(ESDS_FREQ_IDX(pFlv->audCfg.aacSp)), ESDS_FREQ_IDX(pFlv->audCfg.aacSp),
    //              esds_getChannelStr(pFlv->audCfg.aacSp.channelIdx), pFlv->audCfg.aacSp.channelIdx,
    //              pFlv->audCfg.aacSp.frameDeltaHz);
  } else {
    // raw aac data
  }

//fprintf(stdout, "aac file:0x%x body:0x%x aud:0x%x aac:0x%x\n", pFlv->pStream->offset, pAud->hdr.size32, pTagA->frmratesz, pTagA->aac.pkttype);

  return 0;
}

static FLV_TAG_AUDIO_T *flv_read_tag_audio(FLV_CONTAINER_T *pFlv, FLV_TAG_AUDIO_T *pTagA) {
  //FILE_OFFSET_T offset = pFlv->pStream->offset;

  pTagA->szaudhdr = 1;

  if(ReadFileStream(pFlv->pStream, &pTagA->frmratesz, 1) < 0) {
    return NULL;
  }

  if(((pTagA->frmratesz >> 4)& 0x0f) != FLV_AUD_CODEC_AAC) {
    LOG(X_ERROR("Unsupported audio codec %s (%u)"), 
       flv_getAudioCodecStr((pTagA->frmratesz >> 4) & 0x0f),(pTagA->frmratesz >> 4) & 0x0f);
    return pTagA;
  }

  if(flv_read_tag_audio_aac(pFlv, pTagA) != 0) {
    return NULL;
  }

//fprintf(stdout, "aud 0x%x 0x%x 0x%x \n", pFlv->pStream->offset, pTag->hdr.size32, pTag->frmratesz);
  //if(SeekMediaFile(pFlv->pStream, pHdr->size32, SEEK_CUR) < 0) {
  //  return NULL;
  //}

//fprintf(stderr, "0x%x\n", pFlv->pStream->offset);
  return pTagA;
}

static FLV_TAG_HDR_T *flv_read_tag(FLV_CONTAINER_T *pFlv,
                                   CBFLV_VIDDATA cbVid,
                                   void *pCbVidData,
                                   CBFLV_AUDDATA cbAud,
                                   void *pCbAudData) {

  FLV_TAG_HDR_T *pHdr = NULL;
  FLV_TAG_HDR_T hdr;

  //fprintf(stderr, "flv rd at 0x%x 0x%x\n", pFlv->pStream->offset, pFlv->pHdrPrev);

  memset(&hdr, 0, sizeof(hdr));
  hdr.fileOffset = pFlv->pStream->offset;
  if(ReadFileStream(pFlv->pStream, &hdr, FLV_TAG_HDR_SZ) < 0) {
    return NULL;
  }

  //
  // szprev should be the size of the prior tag + 11 (hdr excluding prev size field)
  //
  hdr.szprev = htonl(hdr.szprev);

  //
  // size32 should be the size of the tag content (excluding the 15 byte flv hdr)
  //
  memcpy(&hdr.size32, &hdr.size, 3);
  hdr.size32 = htonl(hdr.size32 << 8);

  memcpy(&hdr.timestamp32, &hdr.timestamp, 3);
  hdr.timestamp32 = htonl(hdr.timestamp32 << 8);
  hdr.timestamp32 |= (hdr.tsext << 24);
  hdr.szhdr = FLV_TAG_HDR_SZ;

  if(pFlv->pHdrPrev && 
     (hdr.szprev != pFlv->pHdrPrev->size32 + FLV_TAG_HDR_SZ_NOPREV) &&
     ((uint16_t)(hdr.szprev >> 16) != pFlv->pHdrPrev->size32 + FLV_TAG_HDR_SZ_NOPREV)) {

    LOG(X_ERROR("FLV Tag at file: 0x%llx with prev size: %d (0x%x) does not match 0x%x"), 
               hdr.fileOffset, hdr.szprev, hdr.szprev, pFlv->pHdrPrev->size32 + FLV_TAG_HDR_SZ - 4);
    return NULL;
  }

  //fprintf(stderr, "flv prevsz:0x%x,type:%d, file:0x%x +%d, sz:0x%x(%u)\n", hdr.szprev, hdr.type, hdr.fileOffset,hdr.szhdr, hdr.size32, hdr.size32);

  switch(hdr.type) {
    case FLV_TAG_AUDIODATA:
      memcpy(&pFlv->audioTmp.hdr, &hdr, sizeof(FLV_TAG_HDR_T));
      pHdr = &pFlv->audioTmp.hdr;
      if(cbAud || (!cbVid && !cbAud)) {
        pHdr = (FLV_TAG_HDR_T *) flv_read_tag_audio(pFlv, (FLV_TAG_AUDIO_T *) pHdr);
      } 
      break;
    case FLV_TAG_VIDEODATA:
      memcpy(&pFlv->videoTmp.hdr, &hdr, sizeof(FLV_TAG_HDR_T));
      pHdr = &pFlv->videoTmp.hdr;
      if(cbVid || (!cbVid && !cbAud)) {
        pHdr = (FLV_TAG_HDR_T *) flv_read_tag_video(pFlv, (FLV_TAG_VIDEO_T *) pHdr);
      }
      break;
    case FLV_TAG_SCRIPTDATA:
      memcpy(&pFlv->scriptTmp.hdr, &hdr, sizeof(FLV_TAG_HDR_T));
      pHdr = &pFlv->scriptTmp.hdr;
      if(!cbVid && !cbAud) {
        pHdr = (FLV_TAG_HDR_T *) flv_read_tag_script(pFlv, (FLV_TAG_SCRIPT_T *) pHdr);
      }
      break; 
  }


//flv_dump_hdr(pFlv, stderr, &hdr);

  return pHdr;
}

static int flv_parse(FLV_CONTAINER_T *pFlv,
                     CBFLV_VIDDATA cbVid,
                     void *pCbVidData,
                     CBFLV_AUDDATA cbAud,
                     void *pCbAudData,
                     CBFLV_SCRIPTDATA cbScript,
                     void *pCbScriptData,
                     uint64_t startHz,
                     uint64_t durationHz) {

  unsigned char arena[32];
  FLV_TAG_HDR_T *pTag = NULL;

  if(ReadFileStream(pFlv->pStream, arena, 9) < 0) {
    return -1;
  }

  memcpy(&pFlv->hdr.type, arena, 5);
  memcpy(&pFlv->hdr.offset, &arena[5], 4);
  pFlv->hdr.offset = htonl(pFlv->hdr.offset);

  if(pFlv->hdr.type[0] != 'F' || pFlv->hdr.type[1] != 'L' || pFlv->hdr.type[2] != 'V') {
    return -1;
  }

  pFlv->lastVidSampleIdx = 0;
  pFlv->lastAudSampleIdx = 0;

  if(startHz != 0 || durationHz != 0) {
    LOG(X_ERROR("Partial extract not implemented for FLV"));
    return -1;
  }

  while(g_proc_exit == 0 && (pTag = flv_read_tag(pFlv, cbVid, pCbVidData, cbAud, pCbAudData))) {

    pFlv->pHdrPrev = pTag;
    if(cbVid == NULL && cbAud == NULL) {
      // probe description only
      if(pFlv->scriptTmp.hdr.type != 0 &&
         (!FLV_HAVE_AUDIO(pFlv) || (FLV_HAVE_AUDIO(pFlv) && pFlv->audioTmp.hdr.type != 0)) &&
         (!FLV_HAVE_VIDEO(pFlv) || (FLV_HAVE_VIDEO(pFlv) && pFlv->videoTmp.hdr.type != 0))) {
        break;
      }
    } else if(cbVid && pTag->type == FLV_TAG_VIDEODATA) {
      cbVid(pFlv, (FLV_TAG_VIDEO_T *) pTag, pCbVidData);
      pFlv->lastVidSampleIdx++;
      //pFlv->lastVidPlayOffsetHz +=
    } else if(cbAud && pTag->type == FLV_TAG_AUDIODATA) {
      cbAud(pFlv, (FLV_TAG_AUDIO_T *) pTag, pCbAudData);
      pFlv->lastAudSampleIdx++;
    } else if(cbScript && pTag->type == FLV_TAG_SCRIPTDATA) {
      cbScript(pFlv, (FLV_TAG_SCRIPT_T *) pTag, pCbScriptData);
    }

//    if(pFlv->pStream->offset > pTag->fileOffset + pTag->szhdr + pTag->size32) {
//      fprintf(stderr, "READ TOO MUCH\n 0x%x  0x%x + %u + 0x%x\n", pFlv->pStream->offset, pTag->fileOffset, pTag->szhdr, pTag->size32);
//    }

//fprintf(stdout, "seeking from file 0x%x to 0x%x\n", pFlv->pStream->offset, pTag->fileOffset + pTag->szhdr + pTag->size32);
    if(SeekMediaFile(pFlv->pStream, pTag->fileOffset + pTag->szhdr + pTag->size32, SEEK_SET) < 0) {
      return 0;
    }

  }

  return 0;
}


static FLV_CONTAINER_T *flv_read_int(const char *path,
                                     CBFLV_VIDDATA cbVid,
                                     void *pCbVidData,
                                     CBFLV_AUDDATA cbAud,
                                     void *pCbAudData,
                                     CBFLV_SCRIPTDATA cbScript,
                                     void *pCbScriptData) {

  FILE_STREAM_T *pStreamIn = NULL;
  FLV_CONTAINER_T *pFlv = NULL;

  if((pStreamIn = (FILE_STREAM_T *) avc_calloc(1, sizeof(FILE_STREAM_T))) == NULL) {
    return NULL;
  }

  if(OpenMediaReadOnly(pStreamIn, path) < 0) {
    free(pStreamIn);
    return NULL;
  }

  if((pFlv = (FLV_CONTAINER_T *) avc_calloc(1, sizeof(FLV_CONTAINER_T))) == NULL) {
    free(pStreamIn);
    return NULL;
  }

  pFlv->pStream = pStreamIn;

  if(flv_parse(pFlv, cbVid, pCbVidData, cbAud, pCbAudData, cbScript, pCbScriptData, 0, 0) != 0) {
    LOG(X_ERROR("Failed to parse %s"), pFlv->pStream->filename);
    flv_close(pFlv);
    return NULL;
  }
//fprintf(stdout, "read done\n");
  return pFlv;
}

FLV_CONTAINER_T *flv_open(const char *path) {
  // Default to read probe only
  return flv_read_int(path, NULL, NULL, NULL, NULL, NULL, NULL);
}


void flv_close(FLV_CONTAINER_T *pFlv) {
  if(pFlv) {
    if(pFlv->pStream) {
      CloseMediaFile(pFlv->pStream);

      if(pFlv->vidCfg.pAvcC) {
        avcc_freeCfg(pFlv->vidCfg.pAvcC);
        avc_free((void **) &pFlv->vidCfg.pAvcC);
      }

      avc_free((void **) &pFlv->pStream);
    }

    flv_amf_free(&pFlv->scriptTmp.entry);

    free(pFlv);
  }
}

typedef struct CBDATA_FLV_ONREAD_SAMPLE {
  void *pCbData;
  CB_READ_SAMPLE cbReadSample;
  uint64_t startHz;
  uint64_t durationHz;
} CBDATA_FLV_ONREAD_SAMPLE_T;

static int cbFlv_onReadVideoSample(FLV_CONTAINER_T *pFlv, FLV_TAG_VIDEO_T *pVid, void *pArg) {
  CBDATA_FLV_ONREAD_SAMPLE_T * pFlvCbData = (CBDATA_FLV_ONREAD_SAMPLE_T *) pArg;
  int rc = 0;

  if((pVid->frmtypecodec & 0x0f) != FLV_VID_CODEC_AVC) {
    LOG(X_WARNING("avc callback called for non avc video type: %u at file:0x%llx"),
       (pVid->frmtypecodec & 0x0f), pVid->hdr.fileOffset);
    return -1;
  }

  if(pVid->avc.pkttype != 1) {
    // Skip avc decoder config record (pkttype = 0)
    // Skip empty end of sequence (pkttype = 2)
    return 0;
  }

//fprintf(stdout, "%llu %u %llu\n", pFlv->durationTotHz, pFlv->vidtimescaleHz, pFlv->vidsampledeltaHz);
//fprintf(stdout, "cbFlv_onRead vid sample %u\n", pVid->hdr.size32);
  // TODO:  check if samples falls within startHz & durationHz

  //TODO: pass valid sampleDurationHz
  if((rc = pFlvCbData->cbReadSample(pFlv->pStream, pVid->hdr.size32 - 5, 
                                    pFlvCbData->pCbData, 0, pFlv->vidsampledeltaHz)) < 0) {
    return rc;
  }


  return 0;
}

static int cbFlv_onReadAudioSample(FLV_CONTAINER_T *pFlv, FLV_TAG_AUDIO_T *pAud, void *pArg) {
  CBDATA_FLV_ONREAD_SAMPLE_T * pFlvCbData = (CBDATA_FLV_ONREAD_SAMPLE_T *) pArg;
  int rc = 0;

  if(((pAud->frmratesz >> 4) & 0x0f) != FLV_AUD_CODEC_AAC) {
    LOG(X_WARNING("aac callback called for non aac audio type: %u at file:0x%llx"),
       ((pAud->frmratesz >> 4) & 0x0f), pAud->hdr.fileOffset);
    return -1;
  }

  if(pAud->aac.pkttype != FLV_AUD_AAC_PKTTYPE_RAW) {
    // Skip aac decoder config record (pkttype = 0)
    return 0;
  }

  // TODO:  check if samples falls within startHz & durationHz

  // TODO: pass valid sampleDruationHz
  if((rc = pFlvCbData->cbReadSample(pFlv->pStream, pAud->hdr.size32 - 2,
                                    pFlvCbData->pCbData, 0, 0)) < 0) {
    return rc;
  }

  // TODO:  check if samples falls within startHz & durationHz
//  pCbData->cbReadSample(pFlv->pStream, 

  return 0;
}

static void reset(FLV_CONTAINER_T *pFlv) {

  if(pFlv->pStream->offset > 0 && SeekMediaFile(pFlv->pStream, 0, SEEK_SET) < 0) {
    return;
  }

  pFlv->pHdrPrev = NULL;
  memset(&pFlv->videoTmp, 0, sizeof(pFlv->videoTmp));
  memset(&pFlv->audioTmp, 0, sizeof(pFlv->scriptTmp));
  //memset(&pFlv->scriptTmp, 0, sizeof(pFlv->scriptTmp));

}


#if defined(VSX_DUMP_CONTAINER)

typedef struct CBDATA_FLV_ONDUMP_SAMPLE {
  FILE *fp;
  unsigned int dumpdatasz;
  unsigned int idxAudio;
  unsigned int idxVideo;
  uint32_t     timestamp32PriorVid;
  uint32_t     timestamp32PriorAud;
} CBDATA_FLV_ONDUMP_SAMPLE_T;

static int cbFlv_onDumpScriptTag(FLV_CONTAINER_T *pFlv, FLV_TAG_SCRIPT_T *pTagS, void *pArg) {
  CBDATA_FLV_ONDUMP_SAMPLE_T *pCbData = (CBDATA_FLV_ONDUMP_SAMPLE_T *) pArg;
  unsigned int dumpdatalen;
  unsigned char buf[4096];

  fprintf(pCbData->fp, "script: file:0x%llx, sz:%d (0x%x), ts:%d\n", 
       pTagS->hdr.fileOffset, pTagS->hdr.size32, pTagS->hdr.size32, pTagS->hdr.timestamp32);

  if(pCbData->dumpdatasz > 0) {
    if((dumpdatalen = sizeof(buf)) > pTagS->hdr.size32) {
      dumpdatalen = pTagS->hdr.size32;
    }
    ReadFileStream(pFlv->pStream, buf, dumpdatalen);
    avc_dumpHex(pCbData->fp, buf, dumpdatalen, 1);
  }

  return 0;
}

static int cbFlv_onDumpTagVid(FLV_CONTAINER_T *pFlv, FLV_TAG_VIDEO_T *pTagV, void *pArg) {

  unsigned char buf[128];
  unsigned int dumpdatalen;
  unsigned int idx = 0;
  unsigned int lenread;
  unsigned int streamid32;
  CBDATA_FLV_ONDUMP_SAMPLE_T *pCbData = (CBDATA_FLV_ONDUMP_SAMPLE_T *) pArg;

  memcpy(&streamid32, &pTagV->hdr.streamid, 3);
  streamid32 = htonl(streamid32 << 8);

  fprintf(pCbData->fp, "video[%d]: file:0x%llx, sz:%d (0x%x), ts:%d (%d), sid:%d", pCbData->idxVideo++,
     pTagV->hdr.fileOffset, pTagV->hdr.size32, pTagV->hdr.size32, pTagV->hdr.timestamp32, 
       (pTagV->hdr.timestamp32 -  pCbData->timestamp32PriorVid), streamid32);
  fprintf(pCbData->fp, " [ type:0x%x, hdr:0x%x, ts:%d ]", 
          pTagV->frmtypecodec, pTagV->avc.pkttype, pTagV->avc.comptime32);
  fprintf(pCbData->fp, "\n");

  pCbData->timestamp32PriorVid = pTagV->hdr.timestamp32;

  if(pCbData->dumpdatasz > 0) {

    if((dumpdatalen = pCbData->dumpdatasz) > pTagV->hdr.size32) {
      dumpdatalen = pTagV->hdr.size32;
    }

    while(idx < dumpdatalen) {
      if((lenread = dumpdatalen - idx) > sizeof(buf)) {
        lenread = sizeof(buf);
      } 
      ReadFileStream(pFlv->pStream, buf, lenread);
      avc_dumpHex(pCbData->fp, buf, lenread, 1);
      idx += lenread;
    }

  }

  return 0;
}

static int cbFlv_onDumpTagAud(FLV_CONTAINER_T *pFlv, FLV_TAG_AUDIO_T *pTagA, void *pArg) {

  unsigned int streamid32;
  CBDATA_FLV_ONDUMP_SAMPLE_T *pCbData = (CBDATA_FLV_ONDUMP_SAMPLE_T *) pArg;

  memcpy(&streamid32, &pTagA->hdr.streamid, 3);
  streamid32 = htonl(streamid32 << 8);

  fprintf(pCbData->fp, "audio[%d]: file:0x%llx, sz:%d (0x%x), ts:%d (%d), sid:%d", pCbData->idxAudio++,
     pTagA->hdr.fileOffset, pTagA->hdr.size32, pTagA->hdr.size32, pTagA->hdr.timestamp32, 
  (pTagA->hdr.timestamp32 -  pCbData->timestamp32PriorAud), streamid32);
  fprintf(pCbData->fp, " [ type:0x%x, hdr:0x%x ]", pTagA->frmratesz, pTagA->aac.pkttype);
  fprintf(pCbData->fp, "\n");

  pCbData->timestamp32PriorAud = pTagA->hdr.timestamp32;

  return 0;
}

void flv_dump(FLV_CONTAINER_T *pFlv, int verbose, FILE *fp) {

  CBDATA_FLV_ONDUMP_SAMPLE_T cbData;

  if(!pFlv || !pFlv->pStream || pFlv->pStream->fp == FILEOPS_INVALID_FP || !fp) {
    return;
  }

  memset(&cbData, 0, sizeof(cbData));
  cbData.fp = fp;

  fprintf(fp, "flv ver:%u, aud:%u, vid:%u\n", pFlv->hdr.ver, 
       (FLV_HAVE_AUDIO(pFlv)) ? 1 : 0, (FLV_HAVE_VIDEO(pFlv)) ? 1 : 0);

  if(pFlv->videoTmp.hdr.type != 0) {
    fprintf(fp, "video type: %u\n", (pFlv->videoTmp.frmtypecodec & 0x0f));
  }
  if(pFlv->audioTmp.hdr.type != 0) {
    fprintf(fp, "audio type: %u\n", pFlv->audioTmp.hdr.type);
  }

  if(verbose > 1) {
    reset(pFlv);

    if(verbose > 16) {
      cbData.dumpdatasz = verbose;
    } else if(verbose > VSX_VERBOSITY_HIGH) {
      cbData.dumpdatasz = 16;
    }

    if(flv_parse(pFlv, cbFlv_onDumpTagVid, &cbData, cbFlv_onDumpTagAud, &cbData, 
                       cbFlv_onDumpScriptTag, &cbData, 0, 0) != 0) {
      LOG(X_ERROR("Failed to parse %s"), pFlv->pStream->filename);
      return;
    }
  }

}
#endif // VSX_DUMP_CONTAINER


int flv_readSamples(FLV_CONTAINER_T *pFlv,
                    CB_READ_SAMPLE sampleRdFunc, void *pCbData,
                    uint64_t startHz, uint64_t durationHz, int video) {

  CBDATA_FLV_ONREAD_SAMPLE_T cbData;
  CBFLV_VIDDATA cbVid = NULL;
  CBFLV_AUDDATA cbAud = NULL;

  if(!pFlv || !pFlv->pStream || pFlv->pStream->fp == FILEOPS_INVALID_FP) {
    return -1;
  }

  if(!sampleRdFunc) {
    return 0;
  }
//fprintf(stdout, "read samples\n");
  reset(pFlv);

  if(video) {
    cbVid = cbFlv_onReadVideoSample;
  } else {
    cbAud = cbFlv_onReadAudioSample;
  }

  cbData.pCbData = pCbData;
  cbData.cbReadSample = sampleRdFunc;

//fprintf(stdout, "parsing flv...\n");

  if(flv_parse(pFlv, cbVid, &cbData, cbAud, &cbData, NULL, NULL, startHz, durationHz) != 0) {
    LOG(X_ERROR("Failed to parse %s"), pFlv->pStream->filename);
    return -1;
  }

  return 0;
}

#if defined(VSX_EXTRACT_CONTAINER)

static int cbFlv_onVidExtract(FLV_CONTAINER_T *pFlv, FLV_TAG_VIDEO_T *pVid, void *pArg) {

  if((pVid->frmtypecodec & 0x0f) != FLV_VID_CODEC_AVC) {
    LOG(X_WARNING("avc write called for non avc video type: %u at file:0x%llx"),
                pVid->frmtypecodec, pVid->hdr.fileOffset); 
    return -1;
  }

  if(pVid->avc.pkttype != 1) {
    // Skip avc decoder config record (pkttype = 0)
    // Skip empty end of sequence (pkttype = 2)
    return 0;
  }

//  fprintf(stdout, "cbVid file:0x%x (tag:0x%x) size:%u\n", pFlv->pStream->offset, pVid->hdr.fileOffset, pVid->hdr.size32);

  return avcc_cbWriteSample(pFlv->pStream, pVid->hdr.size32 - 5, pArg, 0, 0); 
}

int flv_extractAvcVid(FLV_CONTAINER_T *pFlv, const char *path, 
                      uint64_t startHz, uint64_t durationHz) {

  int rc;
  FILE_STREAM_T fStreamOut;
  AVCC_CBDATA_WRITE_SAMPLE_T cbSampleData;

  if(!pFlv || !pFlv->pStream || pFlv->pStream->fp == FILEOPS_INVALID_FP) {
    return -1;
  }

  if((pFlv->videoTmp.frmtypecodec & 0xf) != FLV_VID_CODEC_AVC) {
    LOG(X_ERROR("No valid avc content found"));
    return -1;
  }

  if((fStreamOut.fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_CRITICAL("Unable to open file for writing: %s"), path);
    return -1;
  }
  fStreamOut.offset = 0;

  if(pFlv->pStream->offset > 0 && SeekMediaFile(pFlv->pStream, 0, SEEK_SET) < 0) {
    return -1;
  }
  pFlv->pHdrPrev = NULL;

  if((rc = avcc_writeSPSPPS(&fStreamOut, pFlv->vidCfg.pAvcC,
                            pFlv->sampledeltaHz, pFlv->timescaleHz)) < 0) {
    fileops_Close(fStreamOut.fp);
    return -1;
  }
  
  cbSampleData.lenNalSzField = pFlv->vidCfg.pAvcC->lenminusone + 1;
  cbSampleData.pStreamOut = &fStreamOut;

  if(flv_parse(pFlv, cbFlv_onVidExtract, &cbSampleData, NULL, NULL, 
               NULL, NULL, startHz, durationHz) != 0) {
    LOG(X_ERROR("Failed to parse %s"), pFlv->pStream->filename);
    return -1;
  }

  fileops_Close(fStreamOut.fp);

  return 0;
}

static int cbFlv_onAudExtract(FLV_CONTAINER_T *pFlv, FLV_TAG_AUDIO_T *pAud, void *pArg) {

  if(((pFlv->audioTmp.frmratesz >> 4) & 0xf) != FLV_AUD_CODEC_AAC) {
    LOG(X_ERROR("aac write called for non aac audio type: %u at file: 0x%llx"),
                pAud->frmratesz, pAud->hdr.fileOffset);
    return -1;
  }

  if(pAud->aac.pkttype != FLV_AUD_AAC_PKTTYPE_RAW) {
    // Skip aac decoder config record (pkttype = 0)
    return 0;
  }

  //ReadFileStream(pFlv->pStream, b, 4);
  //fprintf(stdout, "len:%u  0x%2.2x 0x%2.2x 0x%2.2x 0x%2.2x\n", pAud->hdr.size32-2,b[0], b[1], b[2], b[3]);

//  fprintf(stdout, "cbAud file:0x%x (tag:0x%x) size:%u\n", pFlv->pStream->offset, pAud->hdr.fileOffset, pAud->hdr.size32);

  return esds_cbWriteSample(pFlv->pStream, pAud->hdr.size32 - 2, pArg, 0, 0); 
//  return 0;
}

int flv_extractAacAud(FLV_CONTAINER_T *pFlv, const char *path, 
                      uint64_t startHz, uint64_t durationHz) {
  ESDS_CBDATA_WRITE_SAMPLE_T cbSampleData;
  unsigned char buf[4096];
  FILE_STREAM_T fStreamOut;

  if(!pFlv || !pFlv->pStream || pFlv->pStream->fp == FILEOPS_INVALID_FP) {
    return -1;
  }

  cbSampleData.bs.buf = buf;
  cbSampleData.bs.sz = sizeof(buf);
  cbSampleData.bs.idx = 0;

  if(((pFlv->audioTmp.frmratesz >> 4) & 0xf) != FLV_AUD_CODEC_AAC) {
    LOG(X_ERROR("No valid aac content found"));
    return -1;
  }

  if(pFlv->audCfg.aacCfg.objTypeIdx == 0) {
    LOG(X_ERROR("aac decoder config not initialized"));
    return -1;
  }

  if((fStreamOut.fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_CRITICAL("Unable to open file for writing: %s"), path);
    return -1;
  }
  fStreamOut.offset = 0;

  if(pFlv->pStream->offset > 0 && SeekMediaFile(pFlv->pStream, 0, SEEK_SET) < 0) {
    return -1;
  }
  pFlv->pHdrPrev = NULL;

  cbSampleData.pStreamOut = &fStreamOut;
  memcpy(&cbSampleData.decoderCfg, &pFlv->audCfg.aacCfg, sizeof(ESDS_DECODER_CFG_T));

  if(flv_parse(pFlv, NULL, NULL, cbFlv_onAudExtract, &cbSampleData, 
               NULL, NULL, startHz, durationHz) != 0) {
    LOG(X_ERROR("Failed to parse %s"), pFlv->pStream->filename);
    return -1;
  }

  fileops_Close(fStreamOut.fp);

  return 0;
}

int flv_extractRaw(FLV_CONTAINER_T *pFlv, const char *outPrfx,
                   float fStart, float fDuration, int overwrite,
                   int extractVid, int extractAud) {

  char *outPath = NULL;
  size_t szPath;
  FILE_HANDLE fp;
  int rc = 0;
  uint64_t startHz = 0;
  uint64_t durationHz = 0;

  if(!pFlv || !pFlv->pStream || pFlv->pStream->fp == FILEOPS_INVALID_FP ||
     !outPrfx) {
    return -1;
  } else if(!extractVid && !extractAud) {
    return -1;
  }

  if((pFlv->sampledeltaHz == 0 || pFlv->timescaleHz == 0) &&
     (fStart != 0 || fDuration != 0)) {
    LOG(X_ERROR("Unable to do partial extract.  No timing found in flv script tag."));
    return -1;
  }

  durationHz = (uint64_t) (fDuration * pFlv->sampledeltaHz);
  startHz = (uint64_t) (fStart * pFlv->timescaleHz);



  szPath = strlen(outPrfx);
  outPath = (char *) avc_calloc(1, szPath + 8);
  memcpy(outPath, outPrfx, szPath);

  LOG(X_DEBUG("Extracting media from %s"), pFlv->pStream->filename);

  if(extractVid) {
    strncpy(&outPath[szPath], ".h264", 7);
    if(!overwrite && (fp = fileops_Open(outPath, O_RDONLY)) != FILEOPS_INVALID_FP) {
      fileops_Close(fp);
      LOG(X_ERROR("File %s already exists.  Will not overwrite."), outPath);
      free(outPath);
      return -1;
    }

    if((rc = flv_extractAvcVid(pFlv, outPath, startHz, durationHz)) == 0) {
      LOG(X_INFO("Created %s"), outPath);
    }

  } 

  if(extractAud) {
    strncpy(&outPath[szPath], ".aac", 7);
    if(!overwrite && (fp = fileops_Open(outPath, O_RDONLY)) != FILEOPS_INVALID_FP) {
      fileops_Close(fp);
      LOG(X_ERROR("File %s already exists.  Will not overwrite."), outPath);
      free(outPath);
      return -1;
    }

    if((rc = flv_extractAacAud(pFlv, outPath, startHz, durationHz)) == 0) {
      LOG(X_INFO("Created %s"), outPath);
    }

  } 


  return rc;
}

const FLV_AMF_T *flv_amf_find(const FLV_AMF_T *pAmf, const char *str) {

  if(!str) {
    return NULL;
  }

  while(pAmf) {
    if(pAmf->key.p && !strncasecmp((const char *) pAmf->key.p, str, strlen(str))) {
      return pAmf;
    }
    pAmf = pAmf->pnext;
  }
  return NULL;
}

const char *flv_amf_get_key_string(const FLV_AMF_T *pAmf) {
  if(pAmf && pAmf->val.type == FLV_AMF_TYPE_STRING && pAmf->val.u.str.p) {
    return (const char *) pAmf->val.u.str.p;
  } else {
    return "";
  }
}

#endif // VSX_EXTRACT_CONTAINER

