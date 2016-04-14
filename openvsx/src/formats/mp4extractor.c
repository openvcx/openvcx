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

static BOX_T *fillTrack(MP4_TRAK_T *pBoxes, uint32_t stsdType) {
  BOX_T *pBoxStsdType = NULL;

  if((pBoxes->pTkhd = (BOX_TKHD_T *) mp4_findBoxChild(pBoxes->pTrak, 
                                                      *((uint32_t *) "tkhd"))) == NULL) {
    return NULL;
  }

  if((pBoxes->pMdia = mp4_findBoxChild(pBoxes->pTrak, *((uint32_t *) "mdia"))) == NULL) {
    return NULL;
  }
  if((pBoxes->pMdhd = (BOX_MDHD_T *) mp4_findBoxChild(pBoxes->pMdia, 
                                                       *((uint32_t *) "mdhd"))) == NULL) {
    return NULL;
  }
  if((pBoxes->pHdlr = (BOX_HDLR_T *) mp4_findBoxChild(pBoxes->pMdia, 
                                                       *((uint32_t *) "hdlr"))) == NULL) {
    return NULL;
  }
  if((pBoxes->pMinf = mp4_findBoxChild(pBoxes->pMdia, *((uint32_t *) "minf"))) == NULL) {
    return NULL;
  }

  pBoxes->pSmhd = NULL;
  pBoxes->pVmhd = NULL;
  pBoxes->pNmhd = NULL;
  pBoxes->pVmhd = (BOX_VMHD_T *) mp4_findBoxChild(pBoxes->pMinf, *((uint32_t *) "vmhd"));
  pBoxes->pSmhd = (BOX_SMHD_T *) mp4_findBoxChild(pBoxes->pMinf, *((uint32_t *) "smhd"));
  pBoxes->pNmhd = mp4_findBoxChild(pBoxes->pMinf, *((uint32_t *) "nmhd"));

  if(pBoxes->pVmhd == NULL && pBoxes->pSmhd == NULL && pBoxes->pNmhd == NULL) {
    return NULL;
  } 

  if((pBoxes->pDinf = mp4_findBoxChild(pBoxes->pMinf, *((uint32_t *) "dinf"))) == NULL) {
    return NULL;
  }

  if((pBoxes->pStbl = mp4_findBoxChild(pBoxes->pMinf, *((uint32_t *) "stbl"))) == NULL) {
    return NULL;
  }
  if((pBoxes->pStsd = (BOX_STSD_T *) mp4_findBoxChild(pBoxes->pStbl, 
                                                     *((uint32_t *) "stsd"))) == NULL) {
    return NULL;
  }
  if((pBoxes->pStts = (BOX_STTS_T *) mp4_findBoxChild(pBoxes->pStbl, 
                                                     *((uint32_t *) "stts"))) == NULL) {

    //
    // Attempt to look for STTS as child for avc1
    //
    if((pBoxes->pStts = (BOX_STTS_T *) mp4_findBoxInTree(pBoxes->pStbl, 
                                                     *((uint32_t *) "stts"))) == NULL) {
      LOG(X_ERROR("No STTS box found"));
      return NULL;
    }
  }

  if((pBoxes->pStsz = (BOX_STSZ_T *) mp4_findBoxChild(pBoxes->pStbl, 
                                                     *((uint32_t *) "stsz"))) == NULL) {
    return NULL;
  }
  if((pBoxes->pStsc = (BOX_STSC_T *) mp4_findBoxChild(pBoxes->pStbl, 
                                                     *((uint32_t *) "stsc"))) == NULL) {
    return NULL;
  }
  if((pBoxes->pStco = (BOX_STCO_T *) mp4_findBoxChild(pBoxes->pStbl, 
                                                     *((uint32_t *) "stco"))) == NULL) {
    return NULL; 
  }

  if(stsdType != 0) {
    if((pBoxStsdType = mp4_findBoxChild((BOX_T *)pBoxes->pStsd, stsdType)) == NULL) {
      return NULL;
    }
  } else {
    // default to first child for unknown media type
    pBoxStsdType = pBoxes->pStsd->box.child;
  }

  pBoxes->pStsdChild = pBoxStsdType;

  // Optional box
  pBoxes->pStss = (BOX_STSS_T *) mp4_findBoxChild(pBoxes->pStbl, *((uint32_t *) "stss"));

  return pBoxStsdType;
}

static BOX_T *mp4_loadMovieFrag(BOX_T *pPrevMoof, MP4_MOOF_TRAK_T *pMoofTrak, uint32_t trackid) {
  BOX_TRAF_T *pBoxTraf;
  BOX_TFHD_T *pBoxTfhd = NULL;
  int64_t dataOffset = 0;

  if(!pPrevMoof) {
    return NULL;
  }

  //memset(pMoofTrak, 0, sizeof(MP4_MOOF_TRAK_T));
  pMoofTrak->pTfhd = NULL;

  //fprintf(stderr, "loadMovieFrag called for trackid:%d, pTfhd: 0x%x, mdat:0x%x\n", trackid, pMoofTrak->pTfhd, pMoofTrak->pMdat);

  do {

    if(!(pMoofTrak->pMoof = mp4_findBoxNext(pPrevMoof, *((uint32_t *) "moof")))) {
      //LOG(X_ERROR("Unable to find moof box"));
      return NULL;
    }

    if(!(pMoofTrak->pMdat = mp4_findBoxNext(pMoofTrak->pMdat ? pMoofTrak->pMdat : pPrevMoof, 
                                            *((uint32_t *) "mdat")))) {
      LOG(X_ERROR("Unable to find moof mdat box"));
      return NULL;
    }

    if(!(pMoofTrak->pMfhd = (BOX_MFHD_T *) mp4_findBoxChild(pMoofTrak->pMoof, *((uint32_t *) "mfhd")))) {
      LOG(X_ERROR("Unable to find mfhd box"));
      return NULL;
    }

    //
    // TODO: A moof w/ audio + video, may contain multiple traf / tfhd boxes
    // the 'continue' below should first try to look for a next 'traf / tfhd box
    //
    if(!(pBoxTraf = mp4_findBoxNext((BOX_T *) pMoofTrak->pMfhd, *((uint32_t *) "traf")))) {
      LOG(X_ERROR("Unable to find traf box"));
      return NULL;
    }

    if(!(pBoxTfhd = (BOX_TFHD_T *) mp4_findBoxChild(pBoxTraf, *((uint32_t *) "tfhd")))) {
      LOG(X_ERROR("Unable to find tfhd box"));
      return NULL;
    } else if(pBoxTfhd->trackid != trackid) {
      pPrevMoof = pMoofTrak->pMoof;
      //fprintf(stderr, "loadMovieFrag skipping moof trackid:%d\n", pBoxTfhd->trackid);
      continue;
    }
    pMoofTrak->pTfhd = pBoxTfhd;

    if(!(pMoofTrak->pTrun = (BOX_TRUN_T *) mp4_findBoxChild(pBoxTraf, *((uint32_t *) "trun")))) {
      LOG(X_ERROR("Unable to find trun box"));
      return NULL;
    }

    if((pMoofTrak->pTfhd->flags & TFHD_FLAG_BASE_DATA_OFFSET)) {
      dataOffset += pMoofTrak->pTfhd->base_data_offset;
    }
    if((pMoofTrak->pTrun->flags & TRUN_FLAG_DATA_OFFSET)) {
      dataOffset += pMoofTrak->pTrun->data_offset;
    }

    pMoofTrak->fileOffset = pMoofTrak->pMoof->fileOffset + dataOffset;

    //fprintf(stderr, "loadMovieFrag called for trackid:%d fileOffset: 0x%llx (0x%llx + 0x%llx)\n", trackid, pMoofTrak->fileOffset, pMoofTrak->pMoof->fileOffset,  dataOffset);

    if((pMoofTrak->fileOffset != pMoofTrak->pMdat->fileOffsetData)) {
      LOG(X_WARNING("Moof data offset mismatch mdat 0x%"LL64"x + 0x%"LL64"x != 0x%"LL64"x"),
                    pMoofTrak->pMoof->fileOffset, dataOffset, pMoofTrak->pMdat->fileOffsetData);
    }
 
  } while(!pMoofTrak->pTfhd || pMoofTrak->pTfhd->trackid != trackid);

  //fprintf(stderr, "loadMovieFrag returning...\n");

  return pMoofTrak->pMoof;
}

BOX_T *mp4_loadTrack(BOX_T *pRoot, MP4_TRAK_T *pBoxes, uint32_t stsdType, int initMoov) {
  BOX_FTYP_T *pBoxFtyp = NULL;
  BOX_T *pBoxMoov = NULL;
  BOX_MVHD_T *pBoxMvhd = NULL;
  BOX_T *pBoxMdat = NULL;
  BOX_T *pBoxStsdType = NULL;

  BOX_T *pBoxMoof = NULL;
  //BOX_MFHD_T *pBoxMfhd;
  BOX_TRAF_T *pBoxTraf = NULL;
  BOX_TFHD_T *pBoxTfhd = NULL;
  BOX_TRUN_T *pBoxTrun = NULL;

  //fprintf(stderr, "LOAD TRAK %c%c%c%c, initMoov:%d\n", ((unsigned char *)&stsdType)[0], ((unsigned char *)&stsdType)[1], ((unsigned char *)&stsdType)[2], ((unsigned char *)&stsdType)[3], initMoov);

  if((pBoxFtyp = (BOX_FTYP_T *) mp4_findBoxChild(pRoot, *((uint32_t *) "ftyp"))) == NULL &&
     (pBoxFtyp = (BOX_FTYP_T *) mp4_findBoxChild(pRoot, *((uint32_t *) "styp"))) == NULL) {
    LOG(X_ERROR("Unable to find ftyp box"));
    return NULL;
  }

  if(initMoov) {

    memset(pBoxes, 0, sizeof(MP4_TRAK_T));

    if((pBoxMoov = mp4_findBoxChild(pRoot, *((uint32_t *) "moov"))) == NULL) {
      LOG(X_ERROR("Unable to find moov box"));
      return NULL;
    }
  
    if((pBoxMvhd  = (BOX_MVHD_T *) mp4_findBoxChild(pBoxMoov, *((uint32_t *) "mvhd"))) == NULL) {
      LOG(X_ERROR("Unable to find mvhd box"));
      return NULL;
    }

    pBoxMdat = mp4_findBoxChild(pRoot, *((uint32_t *) "mdat"));

    //if((pBoxMdat = mp4_findBoxChild(pRoot, *((uint32_t *) "mdat"))) == NULL) {
    //  LOG(X_ERROR("Unable to find mdat box"));
    //  return NULL;
    //}

    pBoxes->pTrak = mp4_findBoxChild(pBoxMoov, *((uint32_t *) "trak"));

    for(; pBoxes->pTrak != NULL; pBoxes->pTrak = mp4_findBoxNext(pBoxes->pTrak,  
                                                        *((uint32_t *) "trak"))) {
      if((pBoxStsdType = fillTrack(pBoxes, stsdType))) {
        pBoxes->pMdat = pBoxMdat;
        pBoxes->stsdType = stsdType;
        break;
      }

    }

  } else if(pBoxes->pStsdChild) {
    pBoxStsdType = pBoxes->pStsdChild;  
  } // end of initMoov

  //
  // For ISOBMFF (fragment mp4) get the totSampleCnt by traversing all the 'trun' boxes
  //
  if((!initMoov || pBoxStsdType) && (pBoxMoof = mp4_findBoxChild(pRoot, *((uint32_t *) "moof")))) {

    if(!pBoxes->pTkhd) {
      LOG(X_ERROR("No trak header box found"));
      return NULL; 
    }
    memset(&pBoxes->moofTrak, 0, sizeof(pBoxes->moofTrak));

    //fprintf(stderr, "LOAD TRAK ISO BMFF root: 0x%x\n", pRoot);

    pBoxes->moofTrak.pMp4Root = pRoot;
    pBoxes->moofTrak.totSampleCnt = 0;
    pBoxes->moofTrak.totMoofTruns = 0;

    do {
      //if((pBoxMfhd = (BOX_MFHD_T *) mp4_findBoxChild(pBoxMoof, *((uint32_t *) "mfhd")))) {
      //  fprintf(stderr, "mfhd->seq:%d\n", pBoxMfhd->sequence);
      //}
      if((pBoxTraf = (BOX_TRAF_T *) mp4_findBoxChild(pBoxMoof, *((uint32_t *) "traf"))) &&
         (pBoxTfhd = (BOX_TFHD_T *) mp4_findBoxChild(pBoxTraf, *((uint32_t *) "tfhd"))) &&
         pBoxes->pTkhd->trackid ==  pBoxTfhd->trackid &&
         (pBoxTrun = (BOX_TRUN_T *) mp4_findBoxChild(pBoxTraf, *((uint32_t *) "trun")))) {

        pBoxes->moofTrak.totMoofTruns++;
        pBoxes->moofTrak.totSampleCnt += pBoxTrun->sample_count;
      }

    } while((pBoxMoof = mp4_findBoxNext(pBoxMoof, *((uint32_t *) "moof"))));      

    //fprintf(stderr, "MOOFER... %d (%d)\n", pBoxes->moofTrak.totSampleCnt, mp4_getSampleCount(pBoxes));

  }
//fprintf(stdout, "0x%x %c%c%c%c %u\n", pBoxStsdType, ((char *)&stsdType)[0], ((char *)&stsdType)[1], ((char *)&stsdType)[2], ((char *)&stsdType)[3], pBoxes->pMdhd->timescale);

  return pBoxStsdType;
}

unsigned int mp4_getSampleCount(const MP4_TRAK_T *pMp4Trak) {

  if(!pMp4Trak) {
    return 0;
  } else if(pMp4Trak->moofTrak.totMoofTruns > 0) {

    //
    // ISOBMFF fragmented mp4
    //
    return pMp4Trak->moofTrak.totSampleCnt;

  } else if(pMp4Trak->pStts->list.entrycnt == 1) {
    return pMp4Trak->pStts->list.pEntries[0].samplecnt;
  } else {
    return pMp4Trak->pStsz->samplecount;
  }

}

static int getSamplesInChunk(BOX_STSC_T *pStsc, unsigned int idxChunk, unsigned int *pidxStsc) {

  //
  //     1th based first chunk, samples per chunk, 1th based idx in stsd
  // { stsc[0]: 0x1 0x1e 0x1 , stsc[1]: 0xf2 0x3 0x1 }
  //

  if(*pidxStsc >= pStsc->entrycnt) {
    return -1;
  } 

  if(pStsc->pEntries[*pidxStsc].sampledescidx != 1) {
    LOG(X_ERROR("stsc[%d].sampledescidx=%d (!= 1) not implemented!"), 
          *pidxStsc, pStsc->pEntries[*pidxStsc].sampledescidx);
    return -1;
  }

  if(*pidxStsc < pStsc->entrycnt-1) {
    if(idxChunk >= pStsc->pEntries[(*pidxStsc) + 1].firstchunk - 1) {
      (*pidxStsc)++;
    }
  }

  return pStsc->pEntries[*pidxStsc].sampleperchunk;
}

static int getSampleDuration(BOX_STTS_T *pStts, 
                              unsigned int *pidxSampleInStts,
                              unsigned int *pidxStts) {

  uint32_t sampleHz = 0;

  if(pStts->list.entrycnt == 0 || !pStts->list.pEntries) {
    LOG(X_ERROR("Invalid stts entry count: %u"), pStts->list.entrycnt);
    return -1;
  } else if(*pidxStts >= pStts->list.entrycnt) {
    LOG(X_ERROR("Invalid stts index referenced: %u/%u"), (*pidxStts)+1, pStts->list.entrycnt); 
    return -1;
  } else if(*pidxSampleInStts >= pStts->list.pEntries[*pidxStts].samplecnt) {
    LOG(X_ERROR("Invalid stts[%u] entry index referenced: %u/%u"), *pidxStts, 
                (*pidxSampleInStts)+1, pStts->list.pEntries[*pidxStts].samplecnt); 
    return -1;
  }

  sampleHz = pStts->list.pEntries[*pidxStts].sampledelta;
  (*pidxSampleInStts)++;
  if(*pidxSampleInStts >= pStts->list.pEntries[*pidxStts].samplecnt) {
    *pidxSampleInStts = 0;
    (*pidxStts)++;
  }

  return (int) sampleHz;
}

static int getSamplesSize(const BOX_STSZ_T *pStsz, unsigned int idxSample) {

  if(pStsz->samplesize != 0) {
    return pStsz->samplesize;
  } else if(idxSample < pStsz->samplecount) {
    return pStsz->pSamples[idxSample];
  } else {
    return -1;
  }
}

static int isSyncSample(BOX_STSS_T *pStss, unsigned int *pidxStss, unsigned int idxSample) {

  if(!pStss || pStss->entrycnt == 0 || !pStss->pSamples) {
    return 0;
  } else if(*pidxStss >= pStss->entrycnt) {
    //LOG(X_ERROR("Invalid stss index referenced: %u/%u"), (*pidxStss)+1, pStss->entrycnt); 
    return 0;
  } else if(pStss->pSamples[*pidxStss] == idxSample + 1) {
    (*pidxStss)++;
    return 1;
  }
  return 0;
}


int mp4_getNextSyncSampleIdx(MP4_EXTRACT_STATE_INT_T *pState) {

  if(pState->pExtSt->trak.pStss && pState->u.tk.idxStss < pState->pExtSt->trak.pStss->entrycnt) {
    return pState->pExtSt->trak.pStss->pSamples[pState->u.tk.idxStss] - 1;
  }
  return -1;
}

static int readSampleFromISOBMFF(MP4_EXTRACT_STATE_INT_T *pState, 
                                 MP4_MDAT_CONTENT_NODE_T *pContent,
                                 uint32_t *pSampleDurationHz) {
  BOX_T *pBox;
  MP4_MOOF_TRAK_T *pMoofTrak = NULL;
  BOX_TRUN_ENTRY_T *pTrunEntry = NULL;
  unsigned int sampleSize;
  unsigned int sampleDuration;

  //fprintf(stderr, "readSampleFromISOBMFF tkhdid:%d, mdat: 0x%x, sample:%d/%d\n", pState->pExtSt->trak.pTkhd->trackid, pState->pExtSt->trak.pMdat, pState->u.mf.idxSample,  pState->u.mf.countSamples);

  pMoofTrak = &pState->pExtSt->trak.moofTrak;

  if(!pMoofTrak->pMp4Root) {
    return -1;
  }

  //fprintf(stderr, "readSampleFromISOBMFF tkhdid:%d, moof.idxSample:%d/%d, idxMoofTrun:%d/%d, mfhdseq:%u\n", pState->pExtSt->trak.pTkhd->trackid, pState->u.mf.idxSample,  pState->u.mf.countSamples, pState->u.mf.idxMoofTrun, pMoofTrak->totMoofTruns, pState->u.mf.mfhdSequence);

  //
  // Load the next MOOF fragment box
  //
  if(pState->u.mf.idxSample >= pState->u.mf.countSamples) {

    //if(pState->u.mf.mfhdSequence <= 0) {
    if(!pState->u.mf.haveFirstMoof) {
      //fprintf(stderr, "first...\n");

      pState->u.mf.haveFirstMoof = 1;
      pMoofTrak->pMdat = NULL;
      pBox = pMoofTrak->pMp4Root->child;
      pState->u.mf.idxMoofTrun = 0;
      //pState->u.mf.dataOffset = 0;
      pState->u.mf.idxSample = 0;

    } else {
      pBox = pState->u.mf.pMoofCur;

      //fprintf(stderr, "CHECK FOR MORE MOOF TRUNS %d/%d\n", pState->u.mf.idxMoofTrun, pMoofTrak->totMoofTruns);

      //
      // Check if there are anymore fragments in this mp4
      // 
      if(pState->u.mf.idxMoofTrun >= pMoofTrak->totMoofTruns) {
        //
        // No more samples 
        //
        pState->pExtSt->atEndOfTrack = 1;
        return -1;
      }

    }

    pMoofTrak->pMoof = NULL;
    if(!(pBox = mp4_loadMovieFrag(pBox, pMoofTrak, pState->pExtSt->trak.pTkhd->trackid))) {
      if(pMoofTrak->pMoof) {
        LOG(X_ERROR("Failed to load moof"));
      }
      return -1;
    } else if(pMoofTrak->pMfhd->sequence <= 0) {
      LOG(X_ERROR("Moof mfhd sequence should be greater than 0"));
      return -1;
    }

    //memset(&pState->u.mf, 0, sizeof(pState->u.mf));
    pState->u.mf.pMoofCur = (BOX_MOOF_T *) pBox;
    pState->u.mf.mfhdSequence = pMoofTrak->pMfhd->sequence;
    pState->u.mf.idxSample = 0;
    pState->u.mf.countSamples = pMoofTrak->pTrun->sample_count;
    pState->u.mf.idxMoofTrun++;
    pState->u.mf.dataOffset = 0;

  }

  pTrunEntry = &pMoofTrak->pTrun->pSamples[pState->u.mf.idxSample];

  if((pMoofTrak->pTrun->flags & TRUN_FLAG_SAMPLE_SIZE)) {
    sampleSize = pTrunEntry->sample_size;
  } else if(pMoofTrak->pTfhd && (pMoofTrak->pTfhd->flags & TFHD_FLAG_DEFAULT_SAMPLE_SIZE)) {
    sampleSize = pMoofTrak->pTfhd->default_sample_size;
  } else {
    sampleSize = 0;
  }

  if(pSampleDurationHz) {
    if((pMoofTrak->pTrun->flags & TRUN_FLAG_SAMPLE_DURATION)) {
      sampleDuration = pTrunEntry->sample_duration;
    } else if(pMoofTrak->pTfhd && (pMoofTrak->pTfhd->flags & TFHD_FLAG_DEFAULT_SAMPLE_DURATION)) {
      sampleDuration = pMoofTrak->pTfhd->default_sample_duration;
    } else {
      sampleDuration = 0;
    }
    *pSampleDurationHz = sampleDuration;
  }

  //pContent->flags = MP4_MDAT_CONTENT_FLAG_SYNCSAMPLE;
  pContent->flags = 0;
  pContent->sizeRd = sampleSize;
  pContent->fileOffset = pMoofTrak->fileOffset + pState->u.mf.dataOffset;
  pState->u.mf.dataOffset += sampleSize;
  pState->u.mf.idxSample++;

  //fprintf(stderr, "mp4_readSampleFromTrack moof trk:%d, seq:%u, sample[%d/%d], trun[%d/%d],size:%u at 0x%llx(0x%llx+0x%llx), flags:0x%x, duration:%u\n", pMoofTrak->pTfhd->trackid, pState->u.mf.mfhdSequence, pState->u.mf.idxSample, pState->u.mf.countSamples, pState->u.mf.idxMoofTrun, pMoofTrak->totMoofTruns, pContent->sizeRd, pContent->fileOffset, pMoofTrak->fileOffset, pState->u.mf.dataOffset, pContent->flags, sampleDuration);

  return 0;
}

//TODO: create readSampleFromTrack wrapper to try to load next container after end of track

static int mp4_readSampleFromTrack_int(MP4_EXTRACT_STATE_INT_T *pState, 
                                       MP4_MDAT_CONTENT_NODE_T *pContent,
                                       uint32_t *pSampleDurationHz) {
  int sampleSize;
  int sampleHz;

  //fprintf(stderr, "readSampleFromTrack tkhdid:%d, mdat: 0x%x, isobmff:%d, tkhd_id:%d, chunk_idx:%d, file:%llu/%llu %lluHz\n", pState->pExtSt->trak.pTkhd->trackid, pState->pExtSt->trak.pMdat, pState->pExtSt->trak.moofTrak.pMp4Root, pContent->tkhd_id, pContent->chunk_idx, pContent->fileOffset, pContent->pStreamIn ? pContent->pStreamIn->size : 0, pContent->playOffsetHz);
  //fprintf(stderr, "readSampleFromTrack sample[%d], chunk[%d], sampleinchunk[%d]/%d, chunkOffset:%d, stsc[%d]\n", pState->u.tk.idxSample, pState->u.tk.idxChunk, pState->u.tk.idxSampleInChunk, pState->u.tk.samplesInChunk, pState->u.tk.chunkOffset,  pState->u.tk.idxStsc);

  //
  // If this is a fragmented mp4 look at the moof data
  //
  if(pState->pExtSt->trak.moofTrak.pMp4Root) {
    return readSampleFromISOBMFF(pState, pContent, pSampleDurationHz);
  }

  if(!pState->u.tk.isinit ||
     pState->u.tk.idxSampleInChunk >= (unsigned int) pState->u.tk.samplesInChunk) {

    pState->u.tk.chunkOffset = 0;
    pState->u.tk.idxSampleInChunk = 0;

    if(pState->u.tk.isinit) {
      pState->u.tk.idxChunk++;
    }

    if((pState->u.tk.samplesInChunk = getSamplesInChunk(pState->pExtSt->trak.pStsc, 
                                                pState->u.tk.idxChunk, 
                                                &pState->u.tk.idxStsc)) < 0) {

      if(pState->u.tk.idxChunk == 0 && pState->u.tk.idxStsc == 0) {
        pState->pExtSt->atEndOfTrack = 1;
      } else {
        LOG(X_ERROR("Invalid samples in chunks for chunk[%d], stsc[%d]"), 
                    pState->u.tk.idxChunk, pState->u.tk.idxStsc);
      }
      return -1;
    }

  }
  //fprintf(stderr, "IDXCHUNK:%d/%d, IDXSTSC:%d\n", pState->u.tk.idxChunk, pState->pExtSt->trak.pStco->entrycnt, pState->u.tk.idxStsc); 
  if(pState->u.tk.idxChunk >= pState->pExtSt->trak.pStco->entrycnt) {

    //
    // Reached the end of the track
    //
    pState->pExtSt->atEndOfTrack = 1;
    LOG(X_WARNING("sample table chunk sample index end[%d/%d], stsc[%d]"), 
        pState->u.tk.idxChunk, pState->pExtSt->trak.pStco->entrycnt, pState->u.tk.idxStsc);

    return -1;
  }

  if((sampleSize = getSamplesSize(pState->pExtSt->trak.pStsz, pState->u.tk.idxSample)) < 0) {
    LOG(X_ERROR("Invalid sample size for sample[%d], chunk[%d] (sample:%u)"), 
          pState->u.tk.idxSampleInChunk, pState->u.tk.idxChunk, pState->u.tk.idxSample);
    return -1;
  }     

  if((sampleHz = getSampleDuration(pState->pExtSt->trak.pStts, &pState->u.tk.idxSampleInStts, 
                                   &pState->u.tk.idxStts)) < 0) {
    return -1;
  }

  if(isSyncSample(pState->pExtSt->trak.pStss, &pState->u.tk.idxStss, pState->u.tk.idxSample)) {
    pContent->flags = MP4_MDAT_CONTENT_FLAG_SYNCSAMPLE;
  } else {
    pContent->flags = 0;
  }

  pContent->sizeRd = sampleSize;
  pContent->chunk_idx = pState->u.tk.idxChunk; 
  pContent->fileOffset = pState->pExtSt->trak.pStco->pSamples[pState->u.tk.idxChunk] +
                         pState->u.tk.chunkOffset;

//fprintf(stdout, "chunk:%d smpl:%u(%u)%d chk in file:0x%x +0x%x (0x%llx)\n", pState->u.tk.idxChunk, pState->u.tk.idxSampleInChunk,pState->u.tk.idxSample,pContent->flags,pState->pExtSt->trak.pStco->pSamples[pState->u.tk.idxChunk],pState->u.tk.chunkOffset, pContent->fileOffset);

  if(pSampleDurationHz) {
    *pSampleDurationHz = sampleHz;
  }

  pState->u.tk.idxSample++;
  pState->u.tk.idxSampleInChunk++;
  pState->u.tk.chunkOffset += sampleSize;

  if(pState->u.tk.isinit == 0) {
    pState->u.tk.isinit = 1;
  }

  return 0;
}

int mp4_readSampleFromTrack(MP4_EXTRACT_STATE_INT_T *pState,
                             MP4_MDAT_CONTENT_NODE_T *pContent,
                             uint32_t *pSampleDurationHz) {

  int haveNextMp4;
  int rc = 0;

  do {

    haveNextMp4 = 0;

    if((rc = mp4_readSampleFromTrack_int(pState, pContent, pSampleDurationHz)) < 0) {
      if(pState->pExtSt->atEndOfTrack && pState->pExtSt->nextLoader.cbLoadNextMp4) {

        //fprintf(stderr, "CALLING LOAD NEXT MP4 moof.trackid:%d trak.trackid:%d\n", pState->pExtSt->trak.moofTrak.pTfhd? pState->pExtSt->trak.moofTrak.pTfhd->trackid: -1, pState->pExtSt->trak.pTkhd ? pState->pExtSt->trak.pTkhd->trackid : -1);

        //
        // Invoke the callback to prepare the next successive mp4
        //
        if((rc = pState->pExtSt->nextLoader.cbLoadNextMp4(pState)) == 0) {
          haveNextMp4 = 1;
        }

      }
    }

  } while(haveNextMp4);

  return rc;
}

MP4_EXTRACT_STATE_T *mp4_create_extract_state(const MP4_CONTAINER_T *pMp4, const MP4_TRAK_T *pTrak) {
  MP4_EXTRACT_STATE_T *pExtractSt = NULL;

  if(!(pExtractSt = (MP4_EXTRACT_STATE_T *) avc_calloc(1, sizeof(MP4_EXTRACT_STATE_T)))) {
    return NULL;
  }

  memcpy(&pExtractSt->trak, pTrak, sizeof(MP4_TRAK_T));
  pExtractSt->cur.pExtSt = pExtractSt;
  pExtractSt->start.pExtSt = pExtractSt;
  pExtractSt->lastSync.pExtSt = pExtractSt;
  pExtractSt->pStream = pMp4->pStream;

  //
  // Make a copy of the next loader callback context, which is owned by the mp4 container
  //
  if(pMp4->pNextLoader) {
    memcpy(&pExtractSt->nextLoader, pMp4->pNextLoader, sizeof(pExtractSt->nextLoader));
  }

  // preserve the pNextLoader

  return pExtractSt;
}
                                              

static int readSamplesFromISOBMFF(const MP4_TRAK_T *pMp4BoxSet, MP4_FILE_STREAM_T *pStreamIn, 
                            CB_READ_SAMPLE_FSMP4 sampleRdFunc, void *pCbData,
                            uint64_t startHz, uint64_t durationHz) {

  int rc = 0;
  MP4_EXTRACT_STATE_T state;
  MP4_MDAT_CONTENT_NODE_T content;
  uint32_t sampleDurationHz;
  //FILE_OFFSET_T fileOffset;

  memset(&content, 0, sizeof(content));
  memset(&state, 0, sizeof(state));
  memcpy(&state.trak, pMp4BoxSet, sizeof(state.trak));
  state.cur.pExtSt = &state;
  state.start.pExtSt = &state;
  state.pStream = pStreamIn;
  sampleDurationHz = 0;

  do {
    if((rc = readSampleFromISOBMFF(&state.cur, &content, &sampleDurationHz)) >= 0) {
      //fprintf(stderr, "calling sampleRdFunc fileOffset: 0x%llx\n", content.fileOffset);

      if(pStreamIn->cbSeek(pStreamIn, content.fileOffset, SEEK_SET) != 0) {
        return -1;
      }

      if((rc = sampleRdFunc(pStreamIn, content.sizeRd, pCbData, 0, sampleDurationHz)) < 0) {
        return rc;
      }

    }
  } while(rc >= 0);

  return 0;
}

int mp4_readSamplesFromTrack(const MP4_TRAK_T *pMp4BoxSet, MP4_FILE_STREAM_T *pStreamIn, 
                            CB_READ_SAMPLE_FSMP4 sampleRdFunc, void *pCbData,
                            uint64_t startHz, uint64_t durationHz) {
  MP4_EXTRACT_STATE_INT_T state;
  int sampleSize;
  int rc = 0;
  FILE_OFFSET_T fileOffset;
  FILE_OFFSET_T fileOffsetPrev;
  uint64_t totalHz = 0;
  int sampleHz;

  if(!sampleRdFunc || !pMp4BoxSet) {
    return -1;
  }

  //
  // If this is a fragmented mp4 look at the moof data
  //
  if(pMp4BoxSet->moofTrak.pMp4Root) {
    return readSamplesFromISOBMFF(pMp4BoxSet, pStreamIn, sampleRdFunc, pCbData, startHz, durationHz);
  }

  memset(&state, 0, sizeof(state));
//fprintf(stderr, "startHz set to %llu\n", startHz);

  for(state.u.tk.idxChunk = 0; 
      state.u.tk.idxChunk < pMp4BoxSet->pStco->entrycnt; 
      state.u.tk.idxChunk++) {
    fileOffset = pMp4BoxSet->pStco->pSamples[state.u.tk.idxChunk];
    state.u.tk.chunkOffset = 0;

    if(pStreamIn->cbSeek(pStreamIn, fileOffset + state.u.tk.chunkOffset, SEEK_SET) != 0) {
      return -1;
    }

    if((state.u.tk.samplesInChunk = getSamplesInChunk(pMp4BoxSet->pStsc, 
                                                 state.u.tk.idxChunk, &state.u.tk.idxStsc)) < 0) {
      LOG(X_ERROR("Invalid samples in chunks for chunk idx:%d, stsc[%d]"), 
                  state.u.tk.idxChunk, state.u.tk.idxStsc);
      return -1;
    }

    for(state.u.tk.idxSampleInChunk = 0; 
        state.u.tk.idxSampleInChunk < (unsigned int) state.u.tk.samplesInChunk; 
        state.u.tk.idxSampleInChunk++) {
      
      if((sampleSize = getSamplesSize(pMp4BoxSet->pStsz, state.u.tk.idxSample)) < 0) {
        LOG(X_ERROR("Invalid sample size for sample idx[%d], chunk idx[%d]"), 
              state.u.tk.idxSample, state.u.tk.idxChunk);
        return -1;
      }     

      if((sampleHz = getSampleDuration(pMp4BoxSet->pStts, &state.u.tk.idxSampleInStts, 
                                       &state.u.tk.idxStts)) < 0) {
        return -1;
      }
      if(durationHz > 0 && totalHz > startHz + durationHz + sampleHz) {
        return 0;
      }
      //fprintf(stderr, "sz:%d, duration:%d, totalHz:%lld\n", sampleSize, sampleHz, totalHz);
      fileOffsetPrev = pStreamIn->offset;
      if(totalHz >= startHz) {
        if((rc = sampleRdFunc(pStreamIn, sampleSize, pCbData, state.u.tk.idxChunk, sampleHz)) < 0) {
          return rc;
        }
      }

      totalHz += sampleHz;

      if(pStreamIn->offset != fileOffsetPrev + sampleSize) {
        if(pStreamIn->cbSeek(pStreamIn, fileOffsetPrev + sampleSize, SEEK_SET) != 0) {
          return -1;
        }
      }

      state.u.tk.chunkOffset += sampleSize;
      state.u.tk.idxSample++;
    }

    if(pMp4BoxSet->stchk.p_stchk) {
      pMp4BoxSet->stchk.p_stchk[state.u.tk.idxChunk].sz = state.u.tk.chunkOffset;
    }

  }

//fprintf(stderr, "%u %u %llu\n", idxSampleInStts, idxStts, totalHz);
//fprintf(stderr, "%d\n",getSampleDuration(pMp4BoxSet->pStts, &idxSampleInStts, &idxStts));

  return 0;
}


int mp4_initAvcCTrack(const MP4_CONTAINER_T *pMp4, MP4_TRAK_AVCC_T *pMp4BoxSet) {

  if(!pMp4 || !pMp4BoxSet) {
    return -1;
  }

  if(!pMp4->pStream || !pMp4->pStream->cbCheckFd(pMp4->pStream)) {
    return -1;
  } 

  memset(pMp4BoxSet, 0, sizeof(MP4_TRAK_AVCC_T));

  if(pMp4->pStream->cbSeek(pMp4->pStream, 0, SEEK_SET) != 0) {
    return -1;
  }

  //
  // Perform initialization of the pMp4BoxSet->tk structure
  //
  if((pMp4BoxSet->pAvc1 = (BOX_AVC1_T *) mp4_loadTrack(pMp4->proot, &pMp4BoxSet->tk, 
                                  *((uint32_t *) "avc1"), 1)) == NULL) {

    LOG(X_ERROR("No valid avc1 track found in %s"), pMp4->pStream->cbGetName(pMp4->pStream));
    return -1;
  }

  if((pMp4BoxSet->pAvcC = (BOX_AVCC_T *) mp4_findBoxChild(
                        (BOX_T *)pMp4BoxSet->pAvc1, *((uint32_t *) "avcC"))) == NULL) {
    LOG(X_ERROR("No valid avcC track found in %s"), pMp4->pStream->cbGetName(pMp4->pStream));
    return -1;
  }

  if(pMp4BoxSet->pAvcC->avcc.numsps < 1) {
    LOG(X_ERROR("No SPS found in avcC"));
    return -1;
  }

  if(pMp4BoxSet->pAvcC->avcc.numpps < 1) {
    LOG(X_ERROR("No PPS found in avcC"));
    return -1;
  }

  return 0;
}

int mp4_initMp4vTrack(const MP4_CONTAINER_T *pMp4, MP4_TRAK_MP4V_T *pMp4BoxSet) {

  if(!pMp4 || !pMp4BoxSet) {
    return -1;
  }

  if(!pMp4->pStream || !pMp4->pStream->cbCheckFd(pMp4->pStream)) {
    return -1;
  } 

  memset(pMp4BoxSet, 0, sizeof(MP4_TRAK_MP4V_T));

  if(pMp4->pStream->cbSeek(pMp4->pStream, 0, SEEK_SET) != 0) {
    return -1;
  }

  if((pMp4BoxSet->pMp4v = (BOX_MP4V_T *) mp4_loadTrack(pMp4->proot, &pMp4BoxSet->tk, 
                                  *((uint32_t *) "mp4v"), 1)) == NULL) {
    LOG(X_ERROR("No valid mp4v track found in %s"), pMp4->pStream->cbGetName(pMp4->pStream));
    return -1;
  }

  if((pMp4BoxSet->pEsds = (BOX_ESDS_T *) mp4_findBoxChild(
                        (BOX_T *)pMp4BoxSet->pMp4v, *((uint32_t *) "esds"))) == NULL) {
    LOG(X_ERROR("No valid esds track found in %s"), pMp4->pStream->cbGetName(pMp4->pStream));
    return -1;
  }

  return 0;
}

int mp4_initMp4aTrack(const MP4_CONTAINER_T *pMp4, MP4_TRAK_MP4A_T *pMp4BoxSet) {

  BOX_T *pBox;

  if(!pMp4 || !pMp4BoxSet) {
    return -1;
  }

  if(!pMp4->pStream || !pMp4->pStream->cbCheckFd(pMp4->pStream)) {
    return -1;
  } 

  memset(pMp4BoxSet, 0, sizeof(MP4_TRAK_MP4A_T));

  if(pMp4->pStream->cbSeek(pMp4->pStream, 0, SEEK_SET) != 0) {
    return -1;
  }

  if((pMp4BoxSet->pMp4a = (BOX_MP4A_T *) mp4_loadTrack(pMp4->proot, &pMp4BoxSet->tk, 
                                  *((uint32_t *) "mp4a"), 1)) == NULL) {
    LOG(X_ERROR("No valid mp4a track found in %s"), pMp4->pStream->cbGetName(pMp4->pStream));
    return -1;
  }

  if(!(pMp4BoxSet->pEsds = (BOX_ESDS_T *) mp4_findBoxChild(
                        (BOX_T *)pMp4BoxSet->pMp4a, *((uint32_t *) "esds")))) {
    //
    // iPhone 4 recordings can include mp4a -> chan/wave -> frma/mp4a/esds
    //
    if((pBox =  mp4_findBoxChild( (BOX_T *)pMp4BoxSet->pMp4a, *((uint32_t *) "wave")))) {

      pMp4BoxSet->pEsds = (BOX_ESDS_T *) mp4_findBoxChild(pBox, *((uint32_t *) "esds"));
    }
  }
  if(!pMp4BoxSet->pEsds) {
    LOG(X_ERROR("No valid esds (audio) track found in %s"), pMp4->pStream->cbGetName(pMp4->pStream));
    return -1;
  }

  return 0;
}

int mp4_initSamrTrack(const MP4_CONTAINER_T *pMp4, MP4_TRAK_SAMR_T *pMp4BoxSet) {

  if(!pMp4 || !pMp4BoxSet) {
    return -1;
  }

  if(!pMp4->pStream || !pMp4->pStream->cbCheckFd(pMp4->pStream)) {
    return -1;
  } 

  memset(pMp4BoxSet, 0, sizeof(MP4_TRAK_SAMR_T));

  if(pMp4->pStream->cbSeek(pMp4->pStream, 0, SEEK_SET) != 0) {
    return -1;
  }

  if((pMp4BoxSet->pSamr = (BOX_SAMR_T *) mp4_loadTrack(pMp4->proot, &pMp4BoxSet->tk, 
                                  *((uint32_t *) "samr"), 1)) == NULL) {
    LOG(X_ERROR("No valid samr track found in %s"), pMp4->pStream->cbGetName(pMp4->pStream));
    return -1;
  }

  if((pMp4BoxSet->pDamr = (BOX_DAMR_T *) mp4_findBoxChild(
                (BOX_T *)pMp4BoxSet->pSamr, *((uint32_t *) "damr"))) == NULL) {
    LOG(X_ERROR("No valid damr track found in %s"), pMp4->pStream->cbGetName(pMp4->pStream));
    return -1;
  }

  return 0;
}

#if defined(VSX_EXTRACT_CONTAINER)

/*
typedef struct AVCC_CBDATA_WRITE_SAMPLE_FS_WRAPPER {
  AVCC_CBDATA_WRITE_SAMPLE_FS_WRAPPER             avcDescr;

} AVCC_CBDATA_WRITE_SAMPLE_FS_WRAPPER_T;

static int read_sample_fs_wrapper(MP4_FILE_STREAM_T *pStreamIn, uint32_t sampleSize, 
                                  void *pCbData, int chunkIdx, uint32_t sampleDurationHz) {


}
*/

static int avcc_cbWriteSample_mp4wrap(MP4_FILE_STREAM_T *pStreamIn, uint32_t sampleSize,
                       void *pCbData, int chunkIdx, uint32_t sampleDurationHz) {
  return avcc_cbWriteSample((FILE_STREAM_T *) pStreamIn->pCbData, sampleSize, 
                            pCbData, chunkIdx, sampleDurationHz);
}

static int mpg4v_cbWriteSample_mp4wrap(MP4_FILE_STREAM_T *pStreamIn, uint32_t sampleSize,
                       void *pCbData, int chunkIdx, uint32_t sampleDurationHz) {
  return mpg4v_cbWriteSample((FILE_STREAM_T *) pStreamIn->pCbData, sampleSize, 
                            pCbData, chunkIdx, sampleDurationHz);
}

static int esds_cbWriteSample_mp4wrap(MP4_FILE_STREAM_T *pStreamIn, uint32_t sampleSize,
                       void *pCbData, int chunkIdx, uint32_t sampleDurationHz) {
  return esds_cbWriteSample((FILE_STREAM_T *) pStreamIn->pCbData, sampleSize, 
                            pCbData, chunkIdx, sampleDurationHz);
}

static int writeGenericSample(FILE_STREAM_T *pStreamIn, uint32_t sampleSize, 
                              void *pCbData, int chunk_idx, uint32_t sampleDurationHz);

static int writeGenericSample_mp4wrap(MP4_FILE_STREAM_T *pStreamIn, uint32_t sampleSize,
                       void *pCbData, int chunkIdx, uint32_t sampleDurationHz) {
  return writeGenericSample((FILE_STREAM_T *) pStreamIn->pCbData, sampleSize, 
                            pCbData, chunkIdx, sampleDurationHz);
}

static int mp4_extractAvcVid(const MP4_CONTAINER_T *pMp4, const char *path, 
                      float fStart, float fDuration) {
  FILE_STREAM_T fStreamOut;
  MP4_TRAK_AVCC_T mp4BoxSet;
  AVCC_CBDATA_WRITE_SAMPLE_T avcDescr;
  uint64_t startHz = 0;
  uint64_t durationHz = 0;
  uint32_t sampledelta = 0;
  int rc = 0;

  if(!pMp4|| !pMp4->pStream || pMp4->pStream == FILEOPS_INVALID_FP) {
    return -1;
  }

  LOG(X_DEBUG("Extracting h.264 video from %s"), pMp4->pStream->cbGetName(pMp4->pStream));

  if(mp4_initAvcCTrack(pMp4, &mp4BoxSet) < 0) {
    return -1;
  }

  durationHz = (uint64_t) (fDuration * mp4BoxSet.tk.pMdhd->timescale);
  startHz = (uint64_t) (fStart * mp4BoxSet.tk.pMdhd->timescale);

  if((fStreamOut.fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_CRITICAL("Unable to open file for writing: %s"), path);
    return -1;
  }
  fStreamOut.offset = 0;

  if((sampledelta = mp4_getSttsSampleDelta(&mp4BoxSet.tk.pStts->list)) == 0) {
    LOG(X_WARNING("Unable to get video sample timing from stts w/ %d entries"), 
                   mp4BoxSet.tk.pStts->list.entrycnt);
  }

  rc = avcc_writeSPSPPS(&fStreamOut, &mp4BoxSet.pAvcC->avcc,
                         sampledelta, mp4BoxSet.tk.pMdhd->timescale);
    
  
  if(rc >= 0) {
    avcDescr.lenNalSzField = mp4BoxSet.pAvcC->avcc.lenminusone + 1;
    avcDescr.pStreamOut = &fStreamOut;

    if(avcDescr.lenNalSzField <= 0 || avcDescr.lenNalSzField > 4) {
      LOG(X_ERROR("Invalid avcC start frame length field: %u"), avcDescr.lenNalSzField);
      fileops_Close(fStreamOut.fp);
      return -1;
    }

    rc = mp4_readSamplesFromTrack(&mp4BoxSet.tk, pMp4->pStream,  
                   avcc_cbWriteSample_mp4wrap, &avcDescr, startHz, durationHz);
  }

  fileops_Close(fStreamOut.fp);

  return rc;
}

static int mp4_extractMp4Vid(const MP4_CONTAINER_T *pMp4, const char *path, 
                      float fStart, float fDuration) {
  FILE_STREAM_T fStreamOut;
  MP4_TRAK_MP4V_T mp4BoxSet;
  MPG4V_CBDATA_WRITE_SAMPLE_T mpg4vDescr;
  uint64_t startHz = 0;
  uint64_t durationHz = 0;
  uint32_t sampledelta = 0;
  int rc = 0;

  if(!pMp4|| !pMp4->pStream || pMp4->pStream == FILEOPS_INVALID_FP) {
    return -1;
  }

  LOG(X_DEBUG("Extracting mpeg-4 video from %s"), pMp4->pStream->cbGetName(pMp4->pStream));

  if(mp4_initMp4vTrack(pMp4, &mp4BoxSet) < 0) {
    return -1;
  }

  durationHz = (uint64_t) (fDuration * mp4BoxSet.tk.pMdhd->timescale);
  startHz = (uint64_t) (fStart * mp4BoxSet.tk.pMdhd->timescale);

  if((fStreamOut.fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_CRITICAL("Unable to open file for writing: %s"), path);
    return -1;
  }
  fStreamOut.offset = 0;

  if((sampledelta = mp4_getSttsSampleDelta(&mp4BoxSet.tk.pStts->list)) == 0) {
    LOG(X_WARNING("Unable to get video sample timing from stts w/ %d entries"), 
                   mp4BoxSet.tk.pStts->list.entrycnt);
  }

  rc = mpg4v_writeStart(&fStreamOut, mp4BoxSet.pEsds->specific.startcodes,
                        mp4BoxSet.pEsds->specific.type.length);

  if(rc >= 0) {
    mpg4vDescr.pStreamOut = &fStreamOut;

    rc = mp4_readSamplesFromTrack(&mp4BoxSet.tk, pMp4->pStream,  
                   mpg4v_cbWriteSample_mp4wrap, &mpg4vDescr, startHz, durationHz);
  }

  fileops_Close(fStreamOut.fp);

  return rc;
}

static int mp4_extractAacAud(const MP4_CONTAINER_T *pMp4, const char *path, 
                      float fStart, float fDuration) {
  FILE_STREAM_T fStreamOut;
  MP4_TRAK_MP4A_T mp4BoxSet;
  ESDS_CBDATA_WRITE_SAMPLE_T esdsDescr;
  unsigned char buf[4096];
  uint64_t startHz = 0;
  uint64_t durationHz = 0;
  int rc = 0;

  if(!pMp4|| !pMp4->pStream || pMp4->pStream == FILEOPS_INVALID_FP) {
    return -1;
  }

  LOG(X_DEBUG("Extracting aac audio from %s"), pMp4->pStream->cbGetName(pMp4->pStream));

  esdsDescr.bs.buf = buf;
  esdsDescr.bs.sz = sizeof(buf);
  esdsDescr.bs.idx = 0;

  if(mp4_initMp4aTrack(pMp4, &mp4BoxSet) < 0) {
    return -1;
  }

  if(mp4BoxSet.pEsds->config.objtypeid != ESDS_OBJTYPE_MP4A) {
    LOG(X_ERROR("Unsupported mp4a esds object type descriptor: %u"), 
                mp4BoxSet.pEsds->config.objtypeid);
    return -1;
  } else if(esds_decodeDecoderSp(mp4BoxSet.pEsds->specific.startcodes, 
        (mp4BoxSet.pEsds->specific.type.length > sizeof(mp4BoxSet.pEsds->specific.startcodes) ?
        sizeof(mp4BoxSet.pEsds->specific.startcodes) : mp4BoxSet.pEsds->specific.type.length), 
        &esdsDescr.decoderCfg) != 0) {
    LOG(X_ERROR("Failed to decode mp4a esds decoder specific data for type descriptor: %u"), 
                mp4BoxSet.pEsds->config.type.descriptor);
    return -1;
  }

  durationHz = (uint64_t) (fDuration * mp4BoxSet.tk.pMdhd->timescale);
  startHz = (uint64_t) (fStart * mp4BoxSet.tk.pMdhd->timescale);

  if((fStreamOut.fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_CRITICAL("Unable to open file for writing: %s"), path);
    return -1;
  }
  fStreamOut.offset = 0;

  esdsDescr.pStreamOut = &fStreamOut;
  rc = mp4_readSamplesFromTrack(&mp4BoxSet.tk, pMp4->pStream, esds_cbWriteSample_mp4wrap, 
                                &esdsDescr, startHz, durationHz);

  fileops_Close(fStreamOut.fp);

  return rc;
}

typedef struct CBDATA_WRITE_GENERIC_SAMPLE {
  FILE_STREAM_T *pStreamOut;
  unsigned char arena[4096];
} CBDATA_WRITE_GENERIC_SAMPLE_T;

static int writeGenericSample(FILE_STREAM_T *pStreamIn, uint32_t sampleSize, 
                              void *pCbData, int chunk_idx, uint32_t sampleDurationHz) {

  CBDATA_WRITE_GENERIC_SAMPLE_T *pCbDataGeneric = NULL;
  unsigned int idxByteInSample = 0;
  unsigned int lenRead;
  int idxArena = 0;
  int rc = 0;

  if((pCbDataGeneric = (CBDATA_WRITE_GENERIC_SAMPLE_T *) pCbData) == NULL) {
    return -1;
  }

  while(idxByteInSample < sampleSize) {

    if((lenRead = sampleSize - idxByteInSample) > sizeof(pCbDataGeneric->arena) - idxArena) {
      lenRead = sizeof(pCbDataGeneric->arena) - idxArena;
    }

    if(ReadFileStream(pStreamIn, &pCbDataGeneric->arena[idxArena], lenRead) < 0) {
      return -1;
    }

    if((rc = WriteFileStream(pCbDataGeneric->pStreamOut, 
                             pCbDataGeneric->arena, lenRead + idxArena)) < 0) {
      return -1;
    }
    
    idxArena = 0;
    idxByteInSample += lenRead;
   
  }

  return rc;
}
static int extractGenericTrak(const MP4_CONTAINER_T *pMp4, 
                              const MP4_TRAK_T *pMp4BoxSet, 
                              const char *path, 
                              float fStart,
                              float fDuration) {

  FILE_STREAM_T fStreamOut;
  CBDATA_WRITE_GENERIC_SAMPLE_T genericDescr;
  uint64_t startHz = 0;
  uint64_t durationHz = 0;
  int rc = 0;

  durationHz = (uint64_t) (fDuration * pMp4BoxSet->pMdhd->timescale);
  startHz = (uint64_t) (fStart * pMp4BoxSet->pMdhd->timescale);

  if((fStreamOut.fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_CRITICAL("Unable to open file for writing: %s"), path);
    return -1;
  }
  fStreamOut.offset = 0;

  LOG(X_DEBUG("Extracting raw media from %s"), pMp4->pStream->cbGetName(pMp4->pStream));

  genericDescr.pStreamOut = &fStreamOut;
  rc = mp4_readSamplesFromTrack(pMp4BoxSet, pMp4->pStream, writeGenericSample_mp4wrap, &genericDescr,
                                startHz, durationHz);

  fileops_Close(fStreamOut.fp);

  return rc;
}

/*
BOX_T *mp4_findTrackByName(const MP4_CONTAINER_T *pMp4, uint32_t type) {
  BOX_T *pBoxTrak = NULL;
  BOX_STSD_T *pBoxStsd;
  BOX_T *pBox;

  if(!pMp4|| !pMp4->pStream || pMp4->pStream->fp == FILEOPS_INVALID_FP) {
    return NULL;
  }

  if(!(pBoxTrak = mp4_findBoxInTree(pMp4->proot, *((uint32_t *) "moov"))) || 
     !(pBoxTrak = pBoxTrak->child)) {
    LOG(X_ERROR("No tracks found in mp4"));
    return NULL;
  }

  while(pBoxTrak) {

    if(pBoxTrak->type != *((uint32_t *) "trak") ||
       !(pBoxStsd = (BOX_STSD_T *) mp4_findBoxInTree(pBoxTrak, *((uint32_t *) "stsd")))) {

      pBoxTrak = pBoxTrak->pnext;
      continue;
    }

    if((pBox = mp4_findBoxChild((BOX_T *) pBoxStsd, type))) {
      return pBoxTrak;
    }

    pBoxTrak = pBoxTrak->pnext;
  }

  return NULL;
}
*/

int mp4_extractRaw(const MP4_CONTAINER_T *pMp4, const char *outPrfx, 
                   float fStart, float fDuration, int overwrite,
                   int extractVid, int extractAud) {

  BOX_HDLR_T *pBoxHdlr;
  BOX_T *pBoxTrak;
  BOX_T *pBox;
  MP4_TRAK_T mp4Trak;
  char *outPath = NULL;
  size_t szPath;
  FILE_HANDLE fp;
  int rc = 0;

  if(!pMp4|| !pMp4->pStream || !pMp4->pStream->cbCheckFd(pMp4->pStream) ||
     !outPrfx) {
    return -1;
  } else if(!extractVid && !extractAud) {
    return -1;
  }

  if(!(pBoxTrak = mp4_findBoxInTree(pMp4->proot, *((uint32_t *) "moov"))) || 
     !(pBoxTrak = pBoxTrak->child)) {
    LOG(X_ERROR("No tracks found in mp4"));
    return -1;
  }

  while(pBoxTrak) {

    if(pBoxTrak->type != *((uint32_t *) "trak") ||
       !(pBoxHdlr = (BOX_HDLR_T *) mp4_findBoxInTree(pBoxTrak, *((uint32_t *) "hdlr")))) {

      pBoxTrak = pBoxTrak->pnext;
      continue;
    }

    memset(&mp4Trak, 0, sizeof(MP4_TRAK_T));
    mp4Trak.pTrak = pBoxTrak;
    pBox = fillTrack(&mp4Trak, 0);

    if(!pBox) {
      pBoxTrak = pBoxTrak->pnext;
      continue;
    }

    szPath = strlen(outPrfx);
    outPath = (char *) avc_calloc(1, szPath + 8);
    memcpy(outPath, outPrfx, szPath);
    rc = 0;

    if(pBoxHdlr->handlertype == *((uint32_t *) "soun") ||
       pBoxHdlr->handlertype == *((uint32_t *) "sdsm")) {

      if(extractAud) {
        if(mp4_findBoxInTree(pBoxTrak,  *((uint32_t *) "mp4a"))) {
          strncpy(&outPath[szPath], ".aac", 7);
        } else {
          LOG(X_WARNING("Unknown audio track %c%c%c%c written as raw output"), 
                ((unsigned char *)&pBox->type)[0], ((unsigned char *)&pBox->type)[1],
                ((unsigned char *)&pBox->type)[2], ((unsigned char *)&pBox->type)[3]);
          strncpy(&outPath[szPath], ".araw", 7);
        }
      } else {
        pBox = NULL;
      }    

    } else if(pBoxHdlr->handlertype == *((uint32_t *) "vide")) {

      if(extractVid) {
        if(mp4_findBoxInTree(pBoxTrak,  *((uint32_t *) "avc1"))) {
          strncpy(&outPath[szPath], ".h264", 7);
        } else if(mp4_findBoxInTree(pBoxTrak,  *((uint32_t *) "mp4v"))) {
          strncpy(&outPath[szPath], ".mpg4", 7);
        } else {
          LOG(X_WARNING("Unknown video track %c%c%c%c written as raw output"), 
                ((unsigned char *)&pBox->type)[0], ((unsigned char *)&pBox->type)[1],
                ((unsigned char *)&pBox->type)[2], ((unsigned char *)&pBox->type)[3]);
          strncpy(&outPath[szPath], ".vraw", 7);
        }
      } else {
        pBox = NULL;
      }    

    } else {
      pBox = NULL; 
    }

    if(pBox) {

      if(!overwrite && (fp = fileops_Open(outPath, O_RDONLY)) != FILEOPS_INVALID_FP) {
         fileops_Close(fp);
         LOG(X_ERROR("File %s already exists.  Will not overwrite."), outPath);
         free(outPath);
         pBoxTrak = pBoxTrak->pnext;
         continue;
      }

      if(pBox->type == *((uint32_t *) "avc1")) {
        rc = mp4_extractAvcVid(pMp4, outPath, fStart, fDuration);
      } else if(pBox->type == *((uint32_t *) "mp4v")) {
        rc = mp4_extractMp4Vid(pMp4, outPath, fStart, fDuration);
      } else if(pBox->type == *((uint32_t *) "mp4a")) {
        rc = mp4_extractAacAud(pMp4, outPath, fStart, fDuration);
      } else {
        rc = extractGenericTrak(pMp4, &mp4Trak, outPath, fStart, fDuration);
      }

      if(rc == 0) {
        LOG(X_INFO("Created %s"), outPath);
      }
      free(outPath);

    }

    pBoxTrak = pBoxTrak->pnext;
  }


  return rc;
} 

int mp4_dumpStts(const char *pathout, const MP4_TRAK_T *pTrak, const char *filename, const char *trakDescr) {
  unsigned int idx;
  FILE_HANDLE fp;
  char buf[512];
  int rc = 0;

  if(!pathout || !pTrak || !pTrak->pStts || !pTrak->pMdhd) {
    return -1;
  }

  if((fp = fileops_Open(pathout, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open file for writing: %s"), pathout);
    return -1;
  }

  LOG(X_INFO("Created %s"), pathout);

  if(filename) {
    fileops_WriteExt(fp, buf, sizeof(buf), "#av sync for %s\n", filename);
  }
  if(trakDescr) {
    fileops_WriteExt(fp, buf, sizeof(buf), "#track=%s\n", trakDescr);
  }
  fileops_WriteExt(fp, buf, sizeof(buf), "stts=%u\n", pTrak->pStts->list.entrycnt);
  fileops_WriteExt(fp, buf, sizeof(buf), "clock=%u\n", pTrak->pMdhd->timescale);
  for(idx = 0; idx < pTrak->pStts->list.entrycnt; idx++) {
    fileops_WriteExt(fp, buf, sizeof(buf), "count=%u,delta=%u\n",
                    pTrak->pStts->list.pEntries[idx].samplecnt,
                    pTrak->pStts->list.pEntries[idx].sampledelta);
  }

  fileops_Close(fp);

  return rc;
}

#if 0
typedef struct MP4_STBL_CLONE {
  BOX_STTS_T stts;
  BOX_CTTS_T ctts;
  BOX_STSZ_T stsz;
  BOX_STSC_T stsc;
  BOX_STCO_T stco;
  BOX_STSS_T stss;
  int useSyncSample;
} MP4_STBL_CLONE_T;

static int tester(const MP4_TRAK_T *pMp4BoxSet, FILE_STREAM_T *pStreamIn, 
                  uint64_t startHz, MP4_STBL_CLONE_T *pClone) {
  MP4_EXTRACT_STATE_INT_T state;
  MP4_EXTRACT_STATE_INT_T state0;
  MP4_EXTRACT_STATE_INT_T statePrior;
  MP4_EXTRACT_STATE_INT_T *pStatePrior = NULL;
  int sampleSize;
  int rc = 0;
  FILE_OFFSET_T fileOffset;
  FILE_OFFSET_T fileOffsetPrev;
  uint64_t totalHz = 0;
  int sampleHz;
  int syncSample;

  memset(&state, 0, sizeof(state));
  memset(&state0, 0, sizeof(state0));
  memset(&statePrior, 0, sizeof(statePrior));
  pStatePrior = &state;

  for(state.u.tk.idxChunk = 0; 
      state.u.tk.idxChunk < pMp4BoxSet->pStco->entrycnt; 
      state.u.tk.idxChunk++) {

    fileOffset = pMp4BoxSet->pStco->pSamples[state.u.tk.idxChunk];
    state.u.tk.chunkOffset = 0;

    if(SeekMediaFile(pStreamIn, fileOffset + state.u.tk.chunkOffset, SEEK_SET) != 0) {
      return -1;
    }

    if((state.u.tk.samplesInChunk = getSamplesInChunk(pMp4BoxSet->pStsc, 
                                                 state.u.tk.idxChunk, &state.u.tk.idxStsc)) < 0) {
      LOG(X_ERROR("Invalid samples in chunks for chunk idx:%d, stsc[%d]"), 
                  state.u.tk.idxChunk, state.u.tk.idxStsc);
      return -1;
    }

    for(state.u.tk.idxSampleInChunk = 0; 
        state.u.tk.idxSampleInChunk < (unsigned int) state.u.tk.samplesInChunk; 
        state.u.tk.idxSampleInChunk++) {
      
      if((sampleSize = getSamplesSize(pMp4BoxSet->pStsz, state.u.tk.idxSample)) < 0) {
        LOG(X_ERROR("Invalid sample size for sample idx[%d], chunk idx[%d]"), 
              state.u.tk.idxSample, state.u.tk.idxChunk);
        return -1;
      }     

      memcpy(&state0, &state, sizeof(state0));

      if((sampleHz = getSampleDuration(pMp4BoxSet->pStts, &state.u.tk.idxSampleInStts, 
                                       &state.u.tk.idxStts)) < 0) {
        return -1;
      }

      syncSample = isSyncSample(pMp4BoxSet->pStss, &state.u.tk.idxStss, state.u.tk.idxSample);

      fileOffsetPrev = pStreamIn->offset;

fprintf(stdout, "%lldHz %.2f sample[%d], sampleInCh[%d], chunk[%d], stsc[%d], stss[%d], stts[%d/%d] sync:%d\n", totalHz, ((float)totalHz/ pMp4BoxSet->pMdhd->timescale), state.u.tk.idxSample, state.u.tk.idxSampleInChunk, state.u.tk.idxChunk, state.u.tk.idxStsc, state.u.tk.idxStss, state.u.tk.idxSampleInStts, state.u.tk.idxStts, syncSample);

      totalHz += sampleHz;

      if(totalHz >= startHz) {
        memcpy(&pClone->stts, pMp4BoxSet->pStts, sizeof(BOX_STTS_T));
        //TODO: STTS needs a deep copy, skip past idxStts, and decrement samplecnt of [idxStts]
        //pClone->stts.list.entrycnt = pMp4BoxSet->pStts->list.entrycnt - state0.idxSampleInStts;
        //pClone->stts.list.pEntries = &pMp4BoxSet->pStts->list.pEntries[state0.idxSampleInStts];
        //pClone->stts.

        return 0;
      }

      if(pStreamIn->offset != fileOffsetPrev + sampleSize) {
        if(SeekMediaFile(pStreamIn, fileOffsetPrev + sampleSize, SEEK_SET) != 0) {
          return -1;
        }
      }

      state.u.tk.chunkOffset += sampleSize;
      state.u.tk.idxSample++;

      if(pClone->useSyncSample && syncSample) {
        memcpy(&statePrior, &state0, sizeof(statePrior));
        pStatePrior = &statePrior;
      }

    }

    if(pMp4BoxSet->stchk.p_stchk) {
      pMp4BoxSet->stchk.p_stchk[state.u.tk.idxChunk].sz = state.u.tk.chunkOffset;
    }


  }

//fprintf(stderr, "%u %u %llu\n", idxSampleInStts, idxStts, totalHz);
//fprintf(stderr, "%d\n",getSampleDuration(pMp4BoxSet->pStts, &idxSampleInStts, &idxStts));

  return 0;
}

int mp4_createSeeked(const char *path, float fStart) {
  int rc = 0;
  MP4_CONTAINER_T *pMp4;
  BOX_T *pBoxTrak;
  BOX_T *pBox;
  BOX_HDLR_T *pBoxHdlr;
  MP4_TRAK_T mp4TrakTmp;
  MP4_TRAK_T mp4TrakAud;
  MP4_TRAK_T mp4TrakVid;
  MP4_TRAK_T *pMp4TrakVid = NULL;
  MP4_TRAK_T *pMp4TrakAud = NULL;
  MP4_STBL_CLONE_T cloneVid;
  MP4_STBL_CLONE_T cloneAud;
  uint64_t startHz = 0;

  if(!(pMp4 = mp4_open(path, 1))) {
    return -1;
  }

  memset(&cloneVid, 0, sizeof(cloneVid));
  memset(&cloneAud, 0, sizeof(cloneAud));

  if(!(pBoxTrak = mp4_findBoxInTree(pMp4->proot, *((uint32_t *) "moov"))) || 
     !(pBoxTrak = pBoxTrak->child)) {
    LOG(X_ERROR("No tracks found in mp4"));
    mp4_close(pMp4);
    return -1;
  }

  while(pBoxTrak && (!pMp4TrakAud || !pMp4TrakVid)) {

    if(pBoxTrak->type != *((uint32_t *) "trak") ||
       !(pBoxHdlr = (BOX_HDLR_T *) mp4_findBoxInTree(pBoxTrak, *((uint32_t *) "hdlr")))) {

      pBoxTrak = pBoxTrak->pnext;
      continue;
    }


    memset(&mp4TrakTmp, 0, sizeof(MP4_TRAK_T));
    mp4TrakTmp.pTrak = pBoxTrak;
    pBox = fillTrack(&mp4TrakTmp, 0);

    if(!pBox) {
      pBoxTrak = pBoxTrak->pnext;
      continue;
    }

    if(!pMp4TrakAud && (pBoxHdlr->handlertype == *((uint32_t *) "soun") ||
       pBoxHdlr->handlertype == *((uint32_t *) "sdsm"))) {

      memcpy(&mp4TrakAud, &mp4TrakTmp, sizeof(mp4TrakAud));
      pMp4TrakAud = &mp4TrakAud;

    } else if(!pMp4TrakVid && pBoxHdlr->handlertype == *((uint32_t *) "vide")) {

      memcpy(&mp4TrakVid, &mp4TrakTmp, sizeof(mp4TrakVid));
      pMp4TrakVid = &mp4TrakVid;

    }

    pBoxTrak = pBoxTrak->pnext;
  }

fStart=15;

  if(pMp4TrakVid) {
    startHz = (uint64_t) (fStart * pMp4TrakVid->pMdhd->timescale);
    fprintf(stdout, "vid seek %lldHz\n", startHz);
    cloneVid.useSyncSample = 1;
    tester(pMp4TrakVid, pMp4->pStream, startHz, &cloneVid); 
  } 


/*
  if(pMp4TrakAud) {
    startHz = (uint64_t) (fStart * pMp4TrakAud->pMdhd->timescale);
    fprintf(stdout, "aud seek %lldHz\n", startHz);
    tester(pMp4TrakAud, pMp4->pStream, startHz, &cloneAud); 
  }
*/

  mp4_close(pMp4);
  
  return rc;
}
#endif // 0

#endif // VSX_EXTRACT_CONTAINER
