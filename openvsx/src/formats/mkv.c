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
#include "formats/mkv_dict.h"


static int mkv_parse_recur(MKV_PARSE_CTXT_T *pCtxt, const EBML_SYNTAX_T *psyntax, void *pData);
static int mkv_parse_id(MKV_PARSE_CTXT_T *pCtxt, const EBML_SYNTAX_T *psyntax, uint32_t id,
                         void *pData);

static int mkv_check_level_end(MKV_PARSE_CTXT_T *pCtxt) {
  EBML_LEVEL_T *pLevel;

  if(pCtxt->level <= 0) {
    return 0;
  }

  pLevel = &pCtxt->levels[pCtxt->level - 1];

  //fprintf(stderr, "CHECKING LEVEL END level:%d 0x%llx/0x%llx, id:0x%x\n", pCtxt->level, pCtxt->mbs.offset, pLevel->start + pLevel->length, pCtxt->id);

  if((pCtxt->mbs.offset - pLevel->start) >= pLevel->length || pCtxt->id) {
//fprintf(stderr, "LEVEL %d ended\n", pCtxt->level);
    pCtxt->level--;
    return 1;
  }

  return 0;
}

static int mkv_mark_level_start(MKV_PARSE_CTXT_T *pCtxt, uint64_t length) {

  if(pCtxt->level >= EBML_MAX_LEVELS) {
    LOG(X_ERROR("EBML Max sub type nesting level reached %d"), pCtxt->level);
    return -1;
  }

  pCtxt->levels[pCtxt->level].start = pCtxt->mbs.offset;
  pCtxt->levels[pCtxt->level].length = length;
  pCtxt->level++;

  return 0;
}

static int mkv_init_defaults(MKV_PARSE_CTXT_T *pCtxt, const EBML_SYNTAX_T *psyntax, void *pData) {
  int rc = 0;
  unsigned int idx = 0;

  if(!psyntax) {
    //LOG(X_ERROR("EBML Missing syntax definition at offset 0x%llx"), pCtxt->mbs.offset);
    return -1;
  }

  while(psyntax[idx].id > EBML_TYPE_NONE) {
    switch(psyntax[idx].type) {

      case EBML_TYPE_UINT64:
         *(uint64_t *)((char *)pData + psyntax[idx].offset) = psyntax[idx].dflt.ui64;
         VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - INIT INT64 %lld (%lld) at 0x%x + %d"), *(uint64_t *)((char *)pData + psyntax[idx].offset), psyntax[idx].dflt.ui64, pData, psyntax[idx].offset));
        break;

      case EBML_TYPE_UINT32:
         *(uint32_t *)((char *)pData + psyntax[idx].offset) = (uint32_t) psyntax[idx].dflt.ui64;
         VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - INIT INT %d (%d) at 0x%x + %d"), *(uint32_t *)((char *)pData + psyntax[idx].offset), (uint32_t) psyntax[idx].dflt.ui64, pData, psyntax[idx].offset));
        break;

      case EBML_TYPE_DOUBLE:
         *(double *)((char *)pData + psyntax[idx].offset) = psyntax[idx].dflt.dbl;
         VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - DOUBLE %f (%f) at 0x%x + %d"), *(double   *)((char *)pData + psyntax[idx].offset), psyntax[idx].dflt.dbl, pData, psyntax[idx].offset));
        break;

      case EBML_TYPE_UTF8:
        if(psyntax[idx].strsz > 0 && psyntax[idx].dflt.p) {
          strncpy(pData + psyntax[idx].offset, psyntax[idx].dflt.p, psyntax[idx].strsz - 1);
          VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - INIT string '%s' ('%s') sz:%d at0x%x + %d"), pData + psyntax[idx].offset, psyntax[idx].dflt.p, psyntax[idx].strsz, pData + psyntax[idx].offset));
        }
        break;

      default:
        break;

    }
    idx++;
  }


  return rc;
}

static int mkv_parse_elem(MKV_PARSE_CTXT_T *pCtxt, const EBML_SYNTAX_T *psyntax, void *pDataParent) {
  int rc = 0;
  uint32_t id;
  void *pData;
  unsigned int sz;
  unsigned int szread;
  uint64_t length = 0;
  EBML_ARR_T *pArr;
  EBML_BLOB_T *pBlob;
  EBML_BLOB_PARTIAL_T *pBlobPartial;
  void *p;
  unsigned char *v_str;
  uint64_t v_ui64;
  void *v_bin;

  id = pCtxt->id;
  pData = (unsigned char *) pDataParent + psyntax->offset;

  if(psyntax->dynsize > 0 && psyntax->type != EBML_TYPE_SUBTYPES && psyntax->type != EBML_TYPE_PASS) {
    LOG(X_ERROR("Invalid parser definition"));
    return -1;
  }

  //
  // The dynsize field denotes the size of each element in the dynamically allocated array
  //
  if(psyntax->dynsize > 0) {
    pArr = (EBML_ARR_T *) pData;
    if(pArr->nondynalloc) {
      LOG(X_ERROR("EBML array cannot be resized"));
      return -1;
    }
    sz = pArr->count  * psyntax->dynsize;
//fprintf(stderr, "REALLOC id:0x%x TO %d\n", id, sz + psyntax->dynsize);
    if(!(p = avc_realloc(pArr->parr, sz + psyntax->dynsize))) {
      return -1;
    }
    pArr->parr = p;
    memset(&((unsigned char *)pArr->parr)[sz], 0, psyntax->dynsize);
//fprintf(stderr, "CNT:%d dynsize:%d, realloc returned 0x%x\n", pArr->count, psyntax->dynsize, pArr->parr);
    pData = &((unsigned char *) pArr->parr)[sz];
    pArr->count++;
  }

  //if(id == MKV_ID_VOID) { fprintf(stderr, "VOID syntax.id:%d level[%d] file:0x%llx\n", psyntax->id, pCtxt->level, pCtxt->mbs.offset); }

  if(psyntax->type != EBML_TYPE_PASS && psyntax->type != EBML_TYPE_END) {

    //
    // Reset the current id
    //
    pCtxt->id = 0;

    //
    // Get the field length in bytes 
    //
    if((rc = ebml_parse_get_length(&pCtxt->mbs, &length)) < 0) {
      return rc;
    }
//fprintf(stderr, "mkv_parse_elem ebml_parse_get_length:%lld {0x%llx} (len:%d), level[%d], syntax->type:%d, file: 0x%llx\n", length, length, rc, pCtxt->level, psyntax->type, pCtxt->mbs.offset);

    if(psyntax->type >= EBML_TYPE_LAST ||
       (s_ebml_type_max_lengths[psyntax->type] > 0 && length > s_ebml_type_max_lengths[psyntax->type])) {
      LOG(X_ERROR("EBML invalid element length: 0x%llx > 0x%llx, id: 0x%x, type: %d at offset 0x%llx"), 
                  length, s_ebml_type_max_lengths[psyntax->type], id, psyntax->type, pCtxt->mbs.offset);
      return -1;
    }
  }

  VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - parse_elem level[%d], type:%d, id:0x%x, length:%lld (0x%llx), file: 0x%llx, offset:%d, pchild:0x%x, dynsize:%d, pData:0x%x (parent:0x%x)"), pCtxt->level, psyntax->type, id, length, length, pCtxt->mbs.offset, psyntax->offset, psyntax->pchild, psyntax->dynsize, pData, pDataParent) );

  switch(psyntax->type) {

    case EBML_TYPE_UINT64:  
      if((rc = ebml_parse_get_uint(&pCtxt->mbs, length, pData)) < 0) {
        return -1;
      }
      VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - GOT UINT64:%lld (len:%d), file: 0x%llx"), *((uint64_t *) pData), rc, pCtxt->mbs.offset) );
      break;

    case EBML_TYPE_UINT32:  
      if((rc = ebml_parse_get_uint(&pCtxt->mbs, length, &v_ui64)) < 0) {
        return -1;
      }
      *((uint32_t *)pData) = (uint32_t) v_ui64;
      if(length > sizeof(uint32_t)) {
        LOG(X_WARNING("id: 0x%x int64 value truncated from %lld to %ld at offset 0x%llx"), 
            id, v_ui64, *((uint32_t *) pData), pCtxt->mbs.offset);
      }
      VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - GOT UINT32:%ld (len:%d), file:0x%llx"), *((uint32_t *) pData), rc, pCtxt->mbs.offset) );
      break;

    case EBML_TYPE_DOUBLE:  
      if((rc = ebml_parse_get_double(&pCtxt->mbs, length, pData)) < 0) {
        return -1;
      }
      VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - GOT DOUBLE:%f, (len:%d), file:0x%llx"), *((double*) pData), rc, pCtxt->mbs.offset));
      break;

    case EBML_TYPE_UTF8:  

      //TODO: for now these are placed into non-malloced buffers
      if((rc = ebml_parse_get_string(&pCtxt->mbs, length, &v_str)) < 0) {
        return -1;
      }
      if(psyntax->strsz > 0) {
        if(rc >= psyntax->strsz) {
          rc = psyntax->strsz - 1;
        }
        memcpy(pData, v_str, rc);
        ((char *) pData)[rc] = '\0';
      }
      VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - GOT STRING:'%s' (len:%d), file: 0x%llx"), pData, rc, pCtxt->mbs.offset) );
      break;

    case EBML_TYPE_BINARY:  
//fprintf(stderr, "BINARY (type:%d) length:%lld, offset:%llx (fp: 0x%llx)\n", psyntax->type, length, pCtxt->mbs.offset, pCtxt->mbs.pfs->offset);

      pBlob = (EBML_BLOB_T *) pData;
      if(pBlob->nondynalloc) {
        LOG(X_ERROR("EBML blob cannot be allocated"));
        return -1;
      }
      if(pBlob->p) {
        avc_free(&pBlob->p);
      }
      pBlob->size = 0;
      pBlob->fileOffset = pCtxt->mbs.offset;

      if(length > EBML_MAX_BLOB_SZ) {
        LOG(X_ERROR("Binary object id: 0x%x, size %lld exceeds maximum %d at offset 0x%llx"), 
               id, length, EBML_MAX_BLOB_SZ, pCtxt->mbs.offset);
        rc = -1;
        break;
      } else if(length <= 0) {
        break;
      } else if(!(pBlob->p = avc_calloc(1, length))) {
        rc = -1;
        break;
      }

      pBlob->size = length;
      sz = 0;

      while(sz < length) {
        if((szread = length - sz) > pCtxt->mbs.bufsz / 2) {
          szread = pCtxt->mbs.bufsz / 2;
        }
        VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - GOT BINARY %d %d/%lld"), szread, sz, length) );
        if((rc = ebml_parse_get_binary(&pCtxt->mbs, szread, &v_bin)) <= 0) {
          return -1;
        }

        memcpy(&((unsigned char *) pBlob->p)[sz], v_bin, rc);
        VSX_DEBUG_MKV( LOGHEX_DEBUG(&((unsigned char *) pBlob->p)[sz], MIN(rc, 16)) );
        sz += rc;
      }

      break;

    case EBML_TYPE_BINARY_PARTIAL:  
      VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - GOT BINARY PARTIAL (type:%d) length:%lld, offset:%llx (fp: 0x%llx)"), psyntax->type, length, pCtxt->mbs.offset, pCtxt->mbs.pfs->offset) );

      pBlobPartial = (EBML_BLOB_PARTIAL_T *) pData;
      pBlobPartial->size = length;
      pBlobPartial->fileOffset = pCtxt->mbs.offset;

      if((rc = ebml_parse_get_binary(&pCtxt->mbs, MIN(EBML_BLOB_PARTIAL_DATA_SZ, length), &v_bin)) <= 0) {
        return -1;
      }

      memcpy(pBlobPartial->buf, v_bin, rc);
      sz = length - rc;
      rc = ebml_parse_skip_bytes(&pCtxt->mbs, sz);

//avc_dumpHex(stderr, pBlobPartial->buf, EBML_BLOB_PARTIAL_DATA_SZ, 1);
      break;

    case EBML_TYPE_SUBTYPES:

      if(mkv_mark_level_start(pCtxt, length) < 0) {
        return -1;
      }

      mkv_init_defaults(pCtxt, psyntax->pchild, pData);

      VSX_DEBUG_MKV( LOG(X_DEBUG( "MKV - _____subtypes start... ID: 0x%x LEVEL[%d] file:0x%llx"), id, pCtxt->level, pCtxt->mbs.offset) );

      while(mkv_check_level_end(pCtxt) == 0) {
        if(!psyntax->pchild) {
          LOG(X_ERROR("EBML Missing nested syntax definition for id: 0x%x at level %d offset 0x%llx"), 
             id, pCtxt->level, pCtxt->mbs.offset);
          rc = -1;
          break;
        } else if((rc = mkv_parse_recur(pCtxt, psyntax->pchild, pData)) < 0) {
          break;
        }
      }

      VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - _____subtypes end... ID:0x%x LEVEL[%d] pData:0x%x"), id, pCtxt->level, pData) );

      break;
    case EBML_TYPE_PASS:  
      VSX_DEBUG_MKV( LOG(X_DEBUG("PASS calling parse_id id:0x%x"), id) );
      return mkv_parse_id(pCtxt, psyntax->pchild, id, pData);

    case EBML_TYPE_NONE:

      LOG(X_WARNING("EBML skipping %lld bytes id:0x%x offset 0x%llx"), 
           length, id, pCtxt->mbs.offset);

      rc = ebml_parse_skip_bytes(&pCtxt->mbs, length);

      break;

    case EBML_TYPE_END:

      VSX_DEBUG_MKV( LOG(X_DEBUG("EBML_TYPE_END %d bytes:"), length) );
      rc = 1;
      break;

    default:

      LOG(X_ERROR("EBML unhandled id: 0x%x at level %d with syntax type 0x%x offset 0x%llx"), 
          id, pCtxt->level,  psyntax->type, pCtxt->mbs.offset);
      return -1;

  }

  return rc;
}

static int mkv_parse_id(MKV_PARSE_CTXT_T *pCtxt, const EBML_SYNTAX_T *psyntax, uint32_t id,
                         void *pData) {
  int rc;
  unsigned int idx = 0;

  while(psyntax[idx].id > EBML_TYPE_NONE) {
    if(id == psyntax[idx].id) {
        break;
    }
    idx++;
  }

  if (!psyntax[idx].id && id == MKV_ID_CLUSTER &&
       pCtxt->level > 0 && pCtxt->levels[pCtxt->level - 1].length == EBML_MAX_NUMBER) {
    return 0;  // end of cluster with undefined length
  }

  if(!psyntax[idx].id && id != MKV_ID_VOID && id != MKV_ID_CRC32) {
    LOG(X_WARNING("EBML unhandled id: 0x%x at level %d offset 0x%llx"), id, pCtxt->level, pCtxt->mbs.offset);
  }


 // TODO: check for end of cluster

  rc = mkv_parse_elem(pCtxt, &psyntax[idx], pData);
  return rc;
}


static int mkv_parse_recur(MKV_PARSE_CTXT_T *pCtxt, const EBML_SYNTAX_T *psyntax, void *pData) {
  int rc = 0;
  //uint64_t val;

  if(!psyntax)  {
    LOG(X_ERROR("EBML Missing syntax definition at offset 0x%llx"), pCtxt->mbs.offset);
  } else if(!pCtxt || !pData) {
    return -1;
  } else if(pCtxt->mbs.pfs && pCtxt->mbs.offset >= pCtxt->mbs.pfs->size) {
    LOG(X_ERROR("EBML reached EOF %s 0x%llx"), pCtxt->mbs.pfs->filename, pCtxt->mbs.pfs->size);
    return -1;
  }

  //
  // Get the current context id
  //
  if(!pCtxt->id) {

    if((rc = ebml_parse_get_id(&pCtxt->mbs, &pCtxt->id)) < 0) {
      return rc;
    }

    VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - mkv_parse_recur ebml_parse_get_id:0x%x, (len:%d) level[%d], syntax->type:%d, file: 0x%llx"), pCtxt->id, rc, pCtxt->level, psyntax->type, pCtxt->mbs.offset) );
  }

  rc = mkv_parse_id(pCtxt, psyntax, pCtxt->id, pData);

  VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - parse_recur done rc:%d, file now at 0x%llx / 0x%llx"), rc, pCtxt->mbs.offset, pCtxt->mbs.pfs->offset) );
  return rc;
}

static MKV_TRACK_T *find_track(const MKV_SEGMENT_T *pSegment, uint64_t number) {
  MKV_TRACK_T *pTrack;
  unsigned int idx;

  for(idx = 0; idx < pSegment->tracks.arrTracks.count; idx++) {
    pTrack = &((MKV_TRACK_T *) pSegment->tracks.arrTracks.parr)[idx];
    if(pTrack->number == number) {
      return pTrack;
    }
  }

  return NULL;
}

static int mkv_parse_block(MKV_PARSE_CTXT_T *pCtxt, const MKV_SEGMENT_T *pSegment,
                           MKV_BLOCK_T *pBlockOut, uint64_t clusterTimeCode) {
  int rc = 0;
  //int keyframe = pBlockOut->pBg->non_simple ? !pBlockOut->pBg->reference : -1;
  uint64_t ui64;
  uint64_t duration;
  EBML_BYTE_STREAM_T mbs;
  int16_t tm;
  int flags;

  pBlockOut->keyframe = 0 ? !pBlockOut->pBg->reference : -1;

  mbs.bs.buf = pBlockOut->pBg->simpleBlock.buf; 
  mbs.bs.idx = 0;
  mbs.bs.sz = MIN(pBlockOut->pBg->simpleBlock.size, EBML_BLOB_PARTIAL_DATA_SZ);
  mbs.bufsz = mbs.bs.sz;
  mbs.pfs = NULL;
  mbs.offset = 0;

  if((rc = ebml_parse_get_num(&mbs, MIN(pBlockOut->pBg->simpleBlock.size, 8), &ui64)) < 0) {
    LOG(X_ERROR("EBML block insufficient length [%d]/%d"), mbs.bs.idx, mbs.bs.sz);
    return rc;
  }

  if(!(pBlockOut->pTrack = find_track(pSegment, ui64))) {
    LOG(X_ERROR("EBML block references invalid track number %lld"), ui64);
    return -1;
  }

  if((duration = pBlockOut->pBg->duration) == 0) {
    duration = pBlockOut->pTrack->defaultDuration / pSegment->info.timeCodeScale;
  }

  if(mbs.bs.idx + 3 > mbs.bs.sz) {
    LOG(X_ERROR("EBML block insufficient length [%d]/%d"), mbs.bs.idx, mbs.bs.sz);
    return -1;
  }

  tm = htons(*((uint16_t *) &mbs.bs.buf[mbs.bs.idx]));
  mbs.bs.idx += 2;
  flags = mbs.bs.buf[mbs.bs.idx++];
  pBlockOut->bsIdx = mbs.bs.idx;

  if((pBlockOut->pTrack->type & MKV_TRACK_TYPE_VIDEO) && pBlockOut->keyframe == -1) {
    pBlockOut->keyframe = (flags & 0x80) ? 1 : 0;
  } else {
    pBlockOut->keyframe = 0;
  }

  if(tm >= 0 || clusterTimeCode >= -1 * tm) {
    pBlockOut->timeCode = clusterTimeCode + tm;
  } else {
    pBlockOut->timeCode = tm;
  }


  if((flags & 0x06)) {
    LOG(X_ERROR("EBML block lacing not handled"));
    return -1;
  }

  //TODO: handle block encoding

  VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - parse_block reference:%lld, duration:%lld (%lld), size:%d, file:0x%llx, trk:%lld (%d), rc:%d, tm:%lld (block:%d, cluster:%lld), flag:0x%x, key:%d, bs.idx:%d"), pBlockOut->pBg->reference, duration, pBlockOut->pBg->duration, pBlockOut->pBg->simpleBlock.size, pBlockOut->pBg->simpleBlock.fileOffset, ui64, pBlockOut->pTrack->type, rc, pBlockOut->timeCode, tm, clusterTimeCode, flags, pBlockOut->keyframe, mbs.bs.idx) );

  //avc_dumpHex(stderr, pBlockOut->pBg->simpleBlock.buf , 16, 1);
//    switch ((flags & 0x06) >> 1) {



  return rc;
}

static int mkv_parse_cluster(MKV_PARSE_CTXT_T *pCtxt, const MKV_SEGMENT_T *pSegment) {
  int rc = 0;                        
  //unsigned int idx;
  //MKV_CLUSTER_T cluster;
//  FILE_OFFSET_T fileOffset = pCtxt->mbs.offset;
  //MKV_BLOCKGROUP_T *pBlockGroup;

  memset(&pCtxt->cluster, 0, sizeof(pCtxt->cluster));

  VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - _____parse cluster id:0x%x, file: 0x%llx (0x%llx)"), pCtxt->id, pCtxt->mbs.offset, pCtxt->mbs.pfs->offset) );

 // if(pCtxt->id) {
    //
    // Rewind to position before context id
    //
  //  fileOffset -= 4;
  //}

  if(pCtxt->lastFileOffset != pCtxt->mbs.pfs->offset) {
    if(SeekMediaFile(pCtxt->mbs.pfs, pCtxt->lastFileOffset, SEEK_SET) < 0) {
      return -1;
    }
  }

  //
  // Parse the entire cluster
  //
  if((rc = mkv_parse_recur(pCtxt, s_mkv_dict_clusters, &pCtxt->cluster)) < 0) {
    return rc;
  }

  pCtxt->lastFileOffset = pCtxt->mbs.pfs->offset;

  VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - parse cluster id:0x%x, timeCode:%lld, size:%d, file now at 0x%llx (0x%llx)"), pCtxt->id, pCtxt->cluster.timeCode, pCtxt->cluster.arrBlocks.count, pCtxt->mbs.offset, pCtxt->lastFileOffset) );

  //mkv_dump_cluster(stderr, &cluster);

  //ebml_free_data(s_mkv_dict_clusters, &pCtxt->cluster);

  return rc;
}


static void mkv_dump_track(FILE *fp, const MKV_TRACK_T *pTrack) {
  fprintf(fp, "---------------------\n");
  fprintf(fp, "number:%d\n", pTrack->number);
  fprintf(fp, "type:%d\n", pTrack->type);
  fprintf(fp, "uid:%lld\n", pTrack->uid);
  fprintf(fp, "flagLacing:%d\n", pTrack->flagLacing);
  fprintf(fp, "defaultDuration:%lld\n", pTrack->defaultDuration);
  fprintf(fp, "timeCodeScale:%f\n", pTrack->timeCodeScale);
  fprintf(fp, "language:%s\n", pTrack->language);
  fprintf(fp, "codecId:%s\n", pTrack->codecId);
  fprintf(fp, "XC_CODEC_TYPE:%s\n", codecType_getCodecDescrStr(pTrack->codecType));
  fprintf(fp, "codecPrivate size:%d\n", pTrack->codecPrivate.size);
  if(pTrack->codecPrivate.size > 0) {
    avc_dumpHex(stderr, pTrack->codecPrivate.p, MIN(pTrack->codecPrivate.size, 256), 1);
  }
  //avc_dumpHex(stderr, pTrack->codecPrivate.p, pTrack->codecPrivate.size, 1);

  fprintf(fp, "vid.pixelWidth:%d\n", pTrack->video.pixelWidth);
  fprintf(fp, "vid.pixelHeight:%d\n", pTrack->video.pixelHeight);
  fprintf(fp, "vid.displayWidth:%d\n", pTrack->video.displayWidth);
  fprintf(fp, "vid.displayHeight:%d\n", pTrack->video.displayHeight);
  fprintf(fp, "vid.displayUnit:%d\n", pTrack->video.displayUnit);

  fprintf(fp, "aud.samplingFreq:%f\n", pTrack->audio.samplingFreq);
  fprintf(fp, "aud.samplingOutFreq:%f\n", pTrack->audio.samplingOutFreq);
  fprintf(fp, "aud.channels:%d\n", pTrack->audio.channels);
  fprintf(fp, "aud.bitDepth:%d\n", pTrack->audio.bitDepth);

}

static void mkv_dump_tag(FILE *fp, const MKV_TAG_T *pTag) {

  fprintf(fp, "---------------------\n");
  fprintf(fp, "tagTarget trackUid:%lld\n", pTag->tagTarget.trackUid);
  fprintf(fp, "tagTarget chapterUid:%lld\n", pTag->tagTarget.chapterUid);
  fprintf(fp, "tagTarget attachUid:%lld\n", pTag->tagTarget.attachUid);
  fprintf(fp, "tagTarget typeVal:%lld\n", pTag->tagTarget.typeVal);
  fprintf(fp, "tagTarget type:'%s'\n", pTag->tagTarget.type);

  fprintf(fp, "simpleTag name:'%s'\n", pTag->simpleTag.name);
  fprintf(fp, "simpleTag str:'%s'\n", pTag->simpleTag.str);
  fprintf(fp, "simpleTag language:'%s'\n", pTag->simpleTag.language);

}

/*
static void mkv_dump_block(FILE *fp, const MKV_BLOCKGROUP_T *pBlock) {
  fprintf(fp, "---------------------\n");
  fprintf(fp, "blob size:%u\n", pBlock->simpleBlock.size);
}

static void mkv_dump_cluster(FILE *fp, const MKV_CLUSTER_T *pCluster) {
  unsigned int idx;

  fprintf(fp, "---------------------\n");
  fprintf(fp, "cluster.timeCode:%lld\n", pCluster->timeCode);
  fprintf(fp, "arrBlocks.count:%u, %lu\n", pCluster->arrBlocks.count, sizeof(MKV_BLOCKGROUP_T));
  for(idx = 0; idx < pCluster->arrBlocks.count; idx++) {
    mkv_dump_block(fp, &((MKV_BLOCKGROUP_T *) pCluster->arrBlocks.parr)[idx]);
  }
}
*/

void mkv_dump(FILE *fp, const MKV_CONTAINER_T *pMkv) {
  MKV_SEEKENTRY_T *pSeekEntry;
  unsigned int idx;

  if(!fp || !pMkv) {
    return;
  }

  fprintf(fp, "--- header ---\n");
  fprintf(fp, "version:%d (0x%x)\n", pMkv->hdr.version, pMkv->hdr.version);
  fprintf(fp, "readVersion:%d\n", pMkv->hdr.readVersion);
  fprintf(fp, "maxIdLength:%d\n", pMkv->hdr.maxIdLength);
  fprintf(fp, "maxSizeLength:%d\n", pMkv->hdr.maxSizeLength);
  fprintf(fp, "docVersion:%d\n", pMkv->hdr.docVersion);
  fprintf(fp, "docReadVersion:%d\n", pMkv->hdr.docReadVersion);
  fprintf(fp, "docType:%s\n", pMkv->hdr.docType);

  fprintf(fp, "--- segment info ---\n");
  fprintf(fp, "timeCodeScale:%lld\n", pMkv->seg.info.timeCodeScale);
  fprintf(fp, "duration:%f\n", pMkv->seg.info.duration);
  fprintf(fp, "title:'%s'\n", pMkv->seg.info.title);
  fprintf(fp, "muxingApp:'%s'\n", pMkv->seg.info.muxingApp);
  fprintf(fp, "writingApp:'%s'\n", pMkv->seg.info.writingApp);


  fprintf(fp, "arrSeekEntries count:%u, %u\n", pMkv->seg.seekHead.arrSeekEntries.count, 
                                               (unsigned int) sizeof(MKV_SEEKENTRY_T));
  for(idx = 0; idx < pMkv->seg.seekHead.arrSeekEntries.count; idx++) {
    pSeekEntry = &((MKV_SEEKENTRY_T *) pMkv->seg.seekHead.arrSeekEntries.parr)[idx]; 
    fprintf(fp, "seekEntry[%u]: id:0x%llx pos:0x%llx\n", idx, pSeekEntry->id, pSeekEntry->pos);
  }

  fprintf(fp, "arrTags count:%u, %u\n", pMkv->seg.tags.arrTags.count, (unsigned int) sizeof(MKV_TAG_T));
  for(idx = 0; idx < pMkv->seg.tags.arrTags.count; idx++) {
    mkv_dump_tag(fp, &((MKV_TAG_T *) pMkv->seg.tags.arrTags.parr)[idx]); 
  }

  fprintf(fp, "arrTracks count:%u, %u\n", pMkv->seg.tracks.arrTracks.count, (unsigned int) sizeof(MKV_TRACK_T));
  for(idx = 0; idx < pMkv->seg.tracks.arrTracks.count; idx++) {
    mkv_dump_track(fp, &((MKV_TRACK_T *) pMkv->seg.tracks.arrTracks.parr)[idx]); 
  }


}

static const MKV_CODEC_TYPE_T s_mkv_codecs[]={
    { MKV_CODEC_ID_AAC, XC_CODEC_TYPE_AAC},
    { MKV_CODEC_ID_H264, XC_CODEC_TYPE_H264 },
    { MKV_CODEC_ID_VORBIS, XC_CODEC_TYPE_VORBIS},
    { MKV_CODEC_ID_VP8, XC_CODEC_TYPE_VP8},
    { NULL, XC_CODEC_TYPE_UNKNOWN },
};


XC_CODEC_TYPE_T mkv_getCodecType(const char *strCodec) { 
  unsigned int idx = 0;
  const MKV_CODEC_TYPE_T *pCodecType = s_mkv_codecs;

  while(pCodecType[idx].str) {
    if(!strcasecmp(strCodec, pCodecType[idx].str)) {
      return pCodecType[idx].type;
    } 
    idx++;
  }
  
  return XC_CODEC_TYPE_UNKNOWN;
}

const char *mkv_getCodecStr(XC_CODEC_TYPE_T codecType) { 
  unsigned int idx = 0;
  const MKV_CODEC_TYPE_T *pCodecType = s_mkv_codecs;

  while(pCodecType[idx].str) {
    if(codecType == pCodecType[idx].type) {
      return pCodecType[idx].str;
    } 
    idx++;
  }
  
  return "";
}

static int mkv_parse_file(MKV_CONTAINER_T *pMkv) {
  int rc = 0;
  MKV_TRACK_T *pTrack;
  unsigned int idx;

  if(!pMkv || !pMkv->pStream) {
    return - 1;
  }

  memset(&pMkv->hdr, 0, sizeof(pMkv->hdr));
  memset(&pMkv->seg, 0, sizeof(pMkv->seg));

  //
  // Init the parser byte stream
  //
  memset(&pMkv->parser, 0, sizeof(pMkv->parser));
  pMkv->parser.mbs.pfs = pMkv->pStream;
  pMkv->parser.mbs.bs.buf = pMkv->parser.buf;
  pMkv->parser.mbs.bufsz = sizeof(pMkv->parser.buf);

  //
  // Parse the mkv header
  //
  if(rc >= 0 && (rc = mkv_parse_recur(&pMkv->parser, s_mkv_dict_header, &pMkv->hdr)) < 0) {
    ebml_free_data(s_mkv_dict_header, &pMkv->hdr);
    return rc;
  }

  //
  // Parse the mkv segments
  //
  if(rc >= 0 && (rc = mkv_parse_recur(&pMkv->parser, s_mkv_dict_segments, &pMkv->seg)) < 0) {
    ebml_free_data(s_mkv_dict_header, &pMkv->hdr);
    ebml_free_data(s_mkv_dict_segments, &pMkv->seg);
    return rc;
  }

  //
  // Store the data offset prior to reading any clusters
  //
  pMkv->parser.lastFileOffset = pMkv->parser.mbs.pfs->offset;
  pMkv->parser.firstClusterOffset = pMkv->parser.mbs.offset;

  if(rc >= 0) {
    if(pMkv->seg.info.timeCodeScale == 0) {
      pMkv->seg.info.timeCodeScale = 1000000;
    }
    //if(pMkv->seg.info.duration) {
     //pMkv->seg.info.duration = matroska->duration * pMkv->seg.info.timeCodeScale
     //                             * 1000 / AV_TIME_BASE;
    //}
  }

  //
  // Set the video and audio track
  //
  for(idx = 0; idx < pMkv->seg.tracks.arrTracks.count; idx++) {
    pTrack = &((MKV_TRACK_T *) pMkv->seg.tracks.arrTracks.parr)[idx];
    if(!pMkv->vid.pTrack && pTrack->type == MKV_TRACK_TYPE_VIDEO) {
      pMkv->vid.pTrack = pTrack;
      pMkv->vid.pTrack->codecType = mkv_getCodecType(pMkv->vid.pTrack->codecId);
    } else if(!pMkv->aud.pTrack && pTrack->type == MKV_TRACK_TYPE_AUDIO) {
      pMkv->aud.pTrack = pTrack;
      pMkv->aud.pTrack->codecType = mkv_getCodecType(pMkv->aud.pTrack->codecId);
    }
  }

  if(pMkv->vid.pTrack) {
    switch(pMkv->vid.pTrack->codecType) {
      case XC_CODEC_TYPE_H264:
        if(pMkv->vid.u.pCfgH264) {
          avcc_freeCfg(pMkv->vid.u.pCfgH264);
        } else {
          pMkv->vid.u.pCfgH264 = (AVC_DECODER_CFG_T *) avc_calloc(1, sizeof(AVC_DECODER_CFG_T));
        }
        if(avcc_initCfg(pMkv->vid.pTrack->codecPrivate.p, pMkv->vid.pTrack->codecPrivate.size,  pMkv->vid.u.pCfgH264)) {
          return -1;
        }
        break;
      case XC_CODEC_TYPE_VP8:
      default:
        break;
    }
    //TODO: set pMkv->vid.descr
  }

  if(pMkv->aud.pTrack) {
    switch(pMkv->aud.pTrack->codecType) {

      case XC_CODEC_TYPE_AAC:
        if(esds_decodeDecoderSp(pMkv->aud.pTrack->codecPrivate.p, pMkv->aud.pTrack->codecPrivate.size,  &pMkv->aud.u.aacCfg)) {
          return -1;
        }
        break;

      case XC_CODEC_TYPE_VORBIS:
        break;
        pMkv->aud.u.pVorbisCfg = &pMkv->aud.pTrack->codecPrivate;
      default:
        break;
    }

    //TODO: set pMkv->aud.descr
  }

  //fprintf(stderr, "level:%d, id:0x%x\n", pMkv->parser.level, pMkv->parser.id);

  return rc;
}

int mkv_next_block(MKV_CONTAINER_T *pMkv, MKV_BLOCK_T *pBlockOut) {
  int rc = 0;
  MKV_BLOCKGROUP_T *pBlockGroup;

  if(!pMkv || !pMkv->pStream || !pBlockOut) {
    return -1;
  }

  VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - NEXT BLOCK pStream:0x%llx, mbs.offset:%llx, first cluster:%llx"), pMkv->pStream->offset, pMkv->parser.mbs.offset, pMkv->parser.firstClusterOffset) );

  while(rc >= 0) {

    //
    // Check if we finished iterating the blocks of any prior cluster
    //
    if(pMkv->parser.blockIdx >= pMkv->parser.cluster.arrBlocks.count) {

      //
      // Check for EOF
      //
      if(pMkv->parser.mbs.pfs && pMkv->parser.mbs.offset >= pMkv->parser.mbs.pfs->size) {
        break;
      }
  
      //
      // remove any memory allocated for the previous cluster
      //
      if(pMkv->parser.haveCluster) {
        ebml_free_data(s_mkv_dict_clusters, &pMkv->parser.cluster);
      }

      pMkv->parser.haveCluster = 1;
      pMkv->parser.blockIdx = 0;

      //
      // Parse the entire next cluster
      //
      if((rc = mkv_parse_cluster(&pMkv->parser, &pMkv->seg)) < 0) {
        break;
      }

      if(pMkv->parser.cluster.arrBlocks.count <= 0) {
        continue;
      }

    }

    if(!pMkv->parser.cluster.arrBlocks.parr) {
      break;
    }
    pBlockGroup = &((MKV_BLOCKGROUP_T *) pMkv->parser.cluster.arrBlocks.parr)[pMkv->parser.blockIdx++];

    //
    // Parse the next block obtained within the cluster
    //
    if(pBlockGroup->simpleBlock.size > 0) {
      VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - cluster timeCode:%lld block [%d]/%d, data size:%d"), pMkv->parser.cluster.timeCode, pMkv->parser.blockIdx, pMkv->parser.cluster.arrBlocks.count, pBlockGroup->simpleBlock.size) );

      pBlockOut->pBg = pBlockGroup;

      if((rc = mkv_parse_block(&pMkv->parser, &pMkv->seg, pBlockOut, pMkv->parser.cluster.timeCode)) < 0) {
        break;
      }

      return 0;

    }

  }

  return -1;
}

static int mkv_queue_block(MKV_BLOCKLIST_T *pBlockList, const MKV_BLOCK_T *pBlock) {
  MKV_BLOCK_QUEUED_T *pBlockTmp;

  if(!pBlockList->parrBlocks) {
    if(!(pBlockList->parrBlocks = avc_calloc(MKV_BLOCKLIST_SIZE, sizeof(MKV_BLOCK_QUEUED_T)))) {
      return -1;
    }
    pBlockList->sz = MKV_BLOCKLIST_SIZE;
    pBlockList->idxNewest = 0;
    pBlockList->idxOldest = 0;
  }

  VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - QUEING (tid:%d) block size: %d, to %d/%d, newest:%d"), PTHREAD_SELF_TOINT(pthread_self()), MKV_BLOCK_SIZE(pBlock),pBlockList->idxOldest, pBlockList->sz, pBlockList->idxNewest) );

  //
  // Be careful with shallow copy here, preserving any dangling pointers
  //
  memcpy(&pBlockList->parrBlocks[pBlockList->idxNewest].block, pBlock, sizeof(MKV_BLOCK_T));
  memcpy(&pBlockList->parrBlocks[pBlockList->idxNewest].bg, pBlock->pBg, sizeof(MKV_BLOCKGROUP_T));
  pBlockList->parrBlocks[pBlockList->idxNewest].block.pBg = &pBlockList->parrBlocks[pBlockList->idxNewest].bg;

  if(++pBlockList->idxNewest >= pBlockList->sz) {
    pBlockList->idxNewest = 0;
  }
  if(pBlockList->idxNewest == pBlockList->idxOldest) {
    pBlockTmp = &pBlockList->parrBlocks[pBlockList->idxOldest];

    //if(pBlockList->haveRead) {
      LOG(X_ERROR("MKV skipping read block type %d, [%d]/%d, timecode:%lld"), 
          pBlockTmp->block.pTrack ? pBlockTmp->block.pTrack->type : -1, 
          pBlockList->idxOldest, pBlockList->sz, pBlockTmp->block.timeCode);
    //}

    if(++pBlockList->idxOldest >= pBlockList->sz) {
      pBlockList->idxOldest = 0;
    }
  }

  return 0;
}

static int mkv_load_queued_block(MKV_BLOCKLIST_T *pBlockList, MKV_BLOCK_T *pBlockOut) {

  VSX_DEBUG_MKV( LOG(X_DEBUG("READ (tid:%d) queued block size: %d, from %d/%d, newest:%d"), PTHREAD_SELF_TOINT(pthread_self()), MKV_BLOCK_SIZE(&pBlockList->parrBlocks[pBlockList->idxOldest].block),pBlockList->idxOldest, pBlockList->sz, pBlockList->idxNewest) );

  //memcpy(&pBlockList->lastBlock, &pBlockList->parrBlocks[pBlockList->idxOldest], sizeof(MKV_BLOCK_QUEUED_T));
  memcpy(pBlockOut, &pBlockList->parrBlocks[pBlockList->idxOldest].block, sizeof(MKV_BLOCK_T));
  memcpy(&pBlockList->lastBlock.bg, &pBlockList->parrBlocks[pBlockList->idxOldest].bg, sizeof(MKV_BLOCKGROUP_T));
  pBlockOut->pBg = &pBlockList->lastBlock.bg;

  if(++pBlockList->idxOldest >= pBlockList->sz) {
    pBlockList->idxOldest = 0;
  }


  return 0;
}

int mkv_next_block_type(MKV_CONTAINER_T *pMkv, MKV_BLOCK_T *pBlockOut, MKV_TRACK_TYPE_T type) {
  int rc;

  VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - mkv_next_block_type %d"), type) );

  if(type != MKV_TRACK_TYPE_VIDEO && type != MKV_TRACK_TYPE_AUDIO) {
    return -1;
  }

  do {

    //
    // Check for any queued blocks matching the desired track, which may
    // have been loaded by prior requests for blocks matching another track
    //
    if(type == MKV_TRACK_TYPE_VIDEO && pMkv->blocksVid.parrBlocks && 
       pMkv->blocksVid.idxOldest != pMkv->blocksVid.idxNewest) {

      return mkv_load_queued_block(&pMkv->blocksVid, pBlockOut);

    } else if(type == MKV_TRACK_TYPE_AUDIO && pMkv->blocksAud.parrBlocks && 
       pMkv->blocksAud.idxOldest != pMkv->blocksAud.idxNewest) {

      return mkv_load_queued_block(&pMkv->blocksAud, pBlockOut);
    }

    //
    // Get the next available block, which could be from any track
    //
    if((rc = mkv_next_block(pMkv, pBlockOut)) < 0) {
      return rc;
    }

    VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - mkv_next_block got type:%d"), pBlockOut->pTrack ? pBlockOut->pTrack->type : -1) );

    if(pBlockOut->pTrack) {

      //
      // The last block loaded from the cluster matches the requested track  
      //
      if(pBlockOut->pTrack->type == type) {
        VSX_DEBUG_MKV( LOG(X_DEBUG("MKV _ GOT matching block from cluster")) );

        if(type == MKV_TRACK_TYPE_VIDEO) {
          pMkv->blocksVid.haveRead = 1;
        } else if(type == MKV_TRACK_TYPE_AUDIO) {
          pMkv->blocksAud.haveRead = 1;
        }
        
        return 0;
      } else if(pBlockOut->pTrack->type == MKV_TRACK_TYPE_VIDEO) {

        VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - QUEING video block into %d/%d, oldest:%d"), pMkv->blocksVid.idxNewest, pMkv->blocksVid.sz, pMkv->blocksVid.idxOldest) );
        if((rc = mkv_queue_block(&pMkv->blocksVid, pBlockOut)) < 0) {
          return rc;
        }

      } else if(pBlockOut->pTrack->type == MKV_TRACK_TYPE_AUDIO) {

        VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - QUEING audio block into %d/%d, oldest:%d"), pMkv->blocksAud.idxNewest, pMkv->blocksAud.sz, pMkv->blocksAud.idxOldest) );
        if((rc = mkv_queue_block(&pMkv->blocksAud, pBlockOut)) < 0) {
          return rc;
        }

      }

    }

  } while(1);

  return rc;
}

int mkv_loadFrame(MKV_CONTAINER_T *pMkv, MP4_MDAT_CONTENT_NODE_T *pFrame,
                  MKV_TRACK_TYPE_T mkvTrackType, unsigned int clockHz, unsigned int *pLastFrameId) {
  int rc = 0;
  MKV_BLOCK_T block;

  if(!pMkv || !pMkv->pStream || !pFrame) {
    return -1;
  } else if(clockHz == 0 && (mkvTrackType ==  MKV_TRACK_TYPE_VIDEO || mkvTrackType == MKV_TRACK_TYPE_AUDIO)) {
    LOG(X_ERROR("MKV %s track does not have clock rate set!"), ((mkvTrackType ==  MKV_TRACK_TYPE_VIDEO) ? "video" : (mkvTrackType ==  MKV_TRACK_TYPE_AUDIO) ? "audio" : "unknown"));
    return -1;
  }

  if(mkv_next_block_type(pMkv, &block, mkvTrackType) < 0) {
    return -1;
  }

  if(SeekMediaFile(pMkv->pStream, MKV_BLOCK_FOFFSET(&block), SEEK_SET) != 0) {
    return -1;
  }

  if(block.keyframe) {
    pFrame->flags = MP4_MDAT_CONTENT_FLAG_SYNCSAMPLE;
  } else {
    pFrame->flags = 0;
  }
  pFrame->sizeRd = MKV_BLOCK_SIZE(&block);
  pFrame->sizeWr = pFrame->sizeRd;
  if(pLastFrameId) {
    pFrame->frameId = (*pLastFrameId)++;
  } else {
    pFrame->frameId++;
  }
  pFrame->fileOffset = MKV_BLOCK_FOFFSET(&block);
  //TODO: this 1000 should be ((seg.info.timeCodeScale / 1000000) * 1000);
  pFrame->playOffsetHz = block.timeCode * clockHz / 1000.0f;
  pFrame->playDurationHz = 0;
  pFrame->pStreamIn = pMkv->pStream;

  VSX_DEBUG_MKV( LOG(X_DEBUG("MKV - mkv_loadFrame trackType:%d, size:%d, fileoffset:%lld, timecode:%lld, clockHz:%dHz, offsetHz:%lldHz"), mkvTrackType, MKV_BLOCK_SIZE(&block), MKV_BLOCK_FOFFSET(&block), block.timeCode, clockHz, pFrame->playOffsetHz) );
  return rc;
}

int mkv_reset(MKV_CONTAINER_T *pMkv) {
  int rc = 0;

  if(!pMkv) {
    return -1;
  }

  if(pMkv->parser.haveCluster) {
    ebml_free_data(s_mkv_dict_clusters, &pMkv->parser.cluster);
    pMkv->parser.haveCluster = 0;
  }

  // Free any queued blocks
  if(pMkv->blocksVid.parrBlocks) {
    avc_free((void **) &pMkv->blocksVid.parrBlocks);
  }
  pMkv->blocksVid.sz = 0;
  if(pMkv->blocksAud.parrBlocks) {
    avc_free((void **) &pMkv->blocksAud.parrBlocks);
  }
  pMkv->blocksAud.sz = 0;


  pMkv->parser.lastFileOffset = pMkv->parser.firstClusterOffset;
  pMkv->parser.mbs.bs.idx = 0;
  pMkv->parser.mbs.bs.sz = 0;
  pMkv->parser.mbs.offset = pMkv->parser.firstClusterOffset;

  pMkv->parser.blockIdx = 0;
  memset(&pMkv->parser.cluster, 0, sizeof(pMkv->parser.cluster));
  pMkv->parser.level = 0;
  pMkv->parser.id = MKV_ID_CLUSTER;

  if(SeekMediaFile(pMkv->parser.mbs.pfs, pMkv->parser.lastFileOffset, SEEK_SET) < 0) {
    return rc;
  }

  return rc;
}

#if 0
static int testdbl() {

  double d = 13.52;
  double d2;
  unsigned char buf[8];
  unsigned char *puc;
  uint32_t ui32;
  uint64_t ui64;

  //memcpy(buf, &d, 8);

  ui64 = ((uint64_t) htonl(*((uint32_t *) &d ) )) << 32;
  ui64 |= ((uint64_t)  htonl( *((uint32_t *) &((unsigned char *) &d)[4])));

  memcpy(buf, &ui64, 8);

  //memcpy(buf, &d, 8);

  puc = &buf[0];
  ui32 = htonl( *((uint32_t *) puc));
  ui64 = ((uint64_t) ui32) << 32;

  puc = &buf[4];
  ui32 = htonl( *((uint32_t *) puc));
  ui64 |= ui32;

  d2 = *((double *) &ui64);

  fprintf(stderr, "d2 is %f\n", d2);
  avc_dumpHex(stderr, &d, 8, 1);
  avc_dumpHex(stderr, buf, 8, 1);
  avc_dumpHex(stderr, &d2, 8, 1);
  return 0;
}

static char *tester() {
  FILE_STREAM_T fs;
  EBML_BYTE_STREAM_T mbs;
  unsigned int mark;
  unsigned int hdrszlen = 8;
  unsigned int idx;
  unsigned char buf[1024];
  char tmp[128];
  unsigned char audioPriv[] = { 0x13, 0x10, 0x56, 0x35, 0x9d, 0x48, 0x00 };
  unsigned char videoPriv[] = 
      { 0x01, 0x4d, 0x40, 0x33, 0xff, 0xe1, 0x00, 0x17, 0x67, 0x4d, 0x40, 0x33, 0x9a, 0x73, 0x80, 0xa0,
        0x08, 0xb4, 0x20, 0x00, 0x32, 0xe9, 0xe0, 0x09, 0x89, 0x68, 0x11, 0xe3, 0x06, 0x23, 0xc0, 0x01,
        0x00, 0x04, 0x68, 0xee, 0x3c, 0x80 };
  int rc = 0;
  MKV_HEADER_T hdr;
  MKV_SEGMENT_T seg;
  MKV_TRACK_T tracks[2];
  //MKV_TAG_T tags[3];

//return testdbl();

  memset(&fs, 0,sizeof(fs));

  if((fs.fp = fileops_Open("testout.mkv", O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    return NULL;
  }

  memset(buf, 0, sizeof(buf));
  memset(&mbs, 0, sizeof(mbs));
  //mbs.pfs = &fs;
  mbs.bs.buf = buf;
  mbs.bs.sz = sizeof(buf);
  mbs.bufsz = mbs.bs.sz; 

  memset(&hdr, 0, sizeof(hdr));
  hdr.version = EBML_VERSION;
  hdr.readVersion = EBML_VERSION;
  hdr.maxIdLength = EBML_MAX_ID_LENGTH;
  hdr.maxSizeLength = EBML_MAX_SIZE_LENGTH;
  hdr.docVersion = EBML_VERSION; 
  hdr.docReadVersion = EBML_VERSION;
  strcpy(hdr.docType, "matroska");

  rc = mkv_write_header(&hdr, &mbs);

  memset(&seg, 0, sizeof(seg));
  seg.info.timeCodeScale = 1000000; // in ns (1 ms : all timecodes in the segment are expressed in ms)
  seg.info.duration = 0;
  strncpy(seg.info.title, "", sizeof(seg.info.title) - 1);
  strncpy(seg.info.muxingApp, vsxlib_get_appnamestr(tmp, sizeof(tmp)), sizeof(seg.info.muxingApp) - 1);
  strncpy(seg.info.writingApp, vsxlib_get_appnamestr(tmp, sizeof(tmp)), sizeof(seg.info.writingApp) - 1);
  for(idx = 0; idx < sizeof(seg.info.segUid) / sizeof(uint16_t); idx++) {
    *((uint16_t *) &seg.info.segUid[idx]) = random() % RAND_MAX;
  }

  memset(tracks, 0, sizeof(tracks));
  tracks[0].number = 1;
  tracks[0].type = MKV_TRACK_TYPE_VIDEO;
  tracks[0].uid = 1;
  tracks[0].defaultDuration = 0;
  tracks[0].timeCodeScale = 1;
  strncpy(tracks[0].language, "und", sizeof(tracks[0].language) - 1);
  strncpy(tracks[0].codecId, mkv_getCodecStr(XC_CODEC_TYPE_H264), sizeof(tracks[0].codecId) - 1);
  tracks[0].codecPrivate.nondynalloc = 1;
  tracks[0].codecPrivate.p = videoPriv;
  tracks[0].codecPrivate.size = sizeof(videoPriv);
  tracks[0].video.pixelWidth = 1280;
  tracks[0].video.pixelHeight = 544;

  tracks[1].number = 2;
  tracks[1].type = MKV_TRACK_TYPE_AUDIO;
  tracks[1].uid = 2;
  tracks[1].defaultDuration = 0;
  tracks[1].timeCodeScale = 1;
  strncpy(tracks[1].language, "und", sizeof(tracks[1].language) - 1);
  strncpy(tracks[1].codecId, mkv_getCodecStr(XC_CODEC_TYPE_AAC), sizeof(tracks[1].codecId) - 1);
  tracks[1].codecPrivate.nondynalloc = 1;
  tracks[1].codecPrivate.p = audioPriv;
  tracks[1].codecPrivate.size = sizeof(audioPriv);
  tracks[1].audio.samplingFreq = 24000;
  tracks[1].audio.samplingOutFreq = 48000;
  tracks[1].audio.channels = 2;

  seg.tracks.arrTracks.parr = tracks;
  seg.tracks.arrTracks.count = 2;
  seg.tracks.arrTracks.nondynalloc = 1;

  //memset(tags, 0, sizeof(tags));
  //tags[0].tagTarget.trackUid = 0;
  //tags[0].tagTarget.chapterUid = 0;
  //tags[0].tagTarget.attachUid = 0;

  rc = mkv_write_segment(&seg, &mbs);

  fprintf(stderr, "mkv_write_header rc:%d\n", rc);
  mbs.pfs = &fs;
  ebml_write_flush(&mbs);

  //avc_dumpHex(stderr, mbs.bs.buf, mbs.bs.idx, 1);

  CloseMediaFile(&fs);

  return NULL;
}
#endif // 0

MKV_CONTAINER_T *mkv_open(const char *path) {
  FILE_STREAM_T *pStreamIn = NULL;
  MKV_CONTAINER_T *pMkv = NULL;
  int rc = 0;

//return tester();

  //fprintf(stderr, "MKV '%s'\n", path);

  if((pStreamIn = (FILE_STREAM_T *) avc_calloc(1, sizeof(FILE_STREAM_T))) == NULL) {
    return NULL;

  }
  if(OpenMediaReadOnly(pStreamIn, path) < 0) {
    free(pStreamIn);
    return NULL;
  }

  if((pMkv = (MKV_CONTAINER_T *) avc_calloc(1, sizeof(MKV_CONTAINER_T))) == NULL) {
    free(pStreamIn);
    return NULL;
  }

  pMkv->pStream = pStreamIn;

  if((rc = mkv_parse_file(pMkv)) < 0) {
    mkv_close(pMkv);
    pMkv = NULL;
  }

  return pMkv;
}

void mkv_close(MKV_CONTAINER_T *pMkv) {
  if(pMkv) {

    if(pMkv->vid.pTrack) {
      switch(pMkv->vid.pTrack->codecType) {
        case XC_CODEC_TYPE_H264:
          if(pMkv->vid.u.pCfgH264) {
            avcc_freeCfg(pMkv->vid.u.pCfgH264);
            avc_free((void **) &pMkv->vid.u.pCfgH264);
          }
          break;
        case XC_CODEC_TYPE_VP8:
        default:
          break;
      }
    }

    if(pMkv->aud.pTrack) {
      switch(pMkv->aud.pTrack->codecType) {
        case XC_CODEC_TYPE_AAC:
          break;
        default:
          break;
      }
    }

    ebml_free_data(s_mkv_dict_header, &pMkv->hdr);
    ebml_free_data(s_mkv_dict_segments, &pMkv->seg);

    if(pMkv->parser.haveCluster) {
      ebml_free_data(s_mkv_dict_clusters, &pMkv->parser.cluster);
      pMkv->parser.haveCluster = 0;
    }

    if(pMkv->pStream) {
      CloseMediaFile(pMkv->pStream);
      avc_free((void **) &pMkv->pStream);
    }

    // Free any queued blocks
    if(pMkv->blocksVid.parrBlocks) {
      avc_free((void **) &pMkv->blocksVid.parrBlocks);
    }
    pMkv->blocksVid.sz = 0;
    if(pMkv->blocksAud.parrBlocks) {
      avc_free((void **) &pMkv->blocksAud.parrBlocks);
    }
    pMkv->blocksAud.sz = 0;

    free(pMkv);
  }
}

MEDIA_FILE_TYPE_T mkv_isvalidFile(FILE_STREAM_T *pFile) {
  MEDIA_FILE_TYPE_T fileType = MEDIA_FILE_TYPE_UNKNOWN;
  int rc;
  EBML_BYTE_STREAM_T bs;
  uint32_t id = 0;
  unsigned char buf[64];

  if(!pFile) {
    return fileType;
  }


  if((rc = ReadFileStreamNoEof(pFile, buf, sizeof(buf))) < 0) {
    return fileType;
  }

  memset(&bs, 0, sizeof(bs));
  bs.bs.buf = buf;
  bs.bs.sz = rc;
  //bs.pfs = pFile;
  bs.nolog = 1;
  bs.bufsz = bs.bs.sz;

  if((rc = ebml_parse_get_id(&bs, &id)) < 0 || id != MKV_ID_HEADER) {
    return fileType;
  }

  if(avc_binstrstr(buf, bs.bs.sz, (unsigned char *) MKV_DOCTYPE_WEBM, 
                   strlen(MKV_DOCTYPE_WEBM))) {
    return MEDIA_FILE_TYPE_WEBM;
  } else if(avc_binstrstr(buf, bs.bs.sz, (unsigned char *) MKV_DOCTYPE_MATROSKA, 
                          strlen(MKV_DOCTYPE_MATROSKA))) {
    return MEDIA_FILE_TYPE_MKV;
  }
  return fileType;
}

double mkv_getfps(const MKV_CONTAINER_T *pMkv) {
  double durationMs;
  double fps;

  if(!pMkv || !pMkv->vid.pTrack) {
    return -1.0f;
  }

  if(pMkv->seg.info.timeCodeScale == 0) {
    durationMs = 33.333f;  
  } else {
    durationMs = (double) pMkv->vid.pTrack->defaultDuration / pMkv->seg.info.timeCodeScale;
  }

  fps = 1000.0f / durationMs;

  return fps;
}
