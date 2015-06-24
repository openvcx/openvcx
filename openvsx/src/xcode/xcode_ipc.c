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
#include "xcode/xcode_ipc.h"

#ifdef ENABLE_XCODE

#ifdef XCODE_IPC

#include <sys/stat.h>
#include <sys/types.h>

#if !defined(WIN32)
#include <sys/ipc.h>
#include <sys/shm.h>
#include <sys/sem.h>
#endif // WIN32



#if defined(WIN32)

XCODE_IPC_DESCR_T *xcode_ipc_open(unsigned int sz, key_t key) {
  XCODE_IPC_DESCR_T *pIpc = NULL;

  pIpc = avc_calloc(1, sizeof(XCODE_IPC_DESCR_T));
  pIpc->sz = sz;

  if((pIpc->hFile = OpenFileMappingA(FILE_MAP_ALL_ACCESS, FALSE, 
                                    XCODE_FILE_MAPPING_NAME)) == NULL) {

    LOG(X_ERROR("OpenFileMapping %s failed (error:%d)"),  
                XCODE_FILE_MAPPING_NAME, GetLastError());
    xcode_ipc_close(pIpc);
    return NULL;
  }

  if((pIpc->pmem = MapViewOfFile(pIpc->hFile, FILE_MAP_ALL_ACCESS, 0, 0, pIpc->sz)) == NULL) {
    LOG(X_ERROR("MapViewOfFile %s failed (error:%d)"),  
                XCODE_FILE_MAPPING_NAME, GetLastError());
    xcode_ipc_close(pIpc);
    return NULL;
  }

  if((pIpc->hSemSrv = OpenSemaphoreA(SYNCHRONIZE | SEMAPHORE_MODIFY_STATE, 
                                    FALSE, XCODE_SEM_SRV_NAME)) == NULL) {
    LOG(X_ERROR("OpenSemaphore %s failed (error:%d)"), XCODE_SEM_SRV_NAME, GetLastError());
    xcode_ipc_close(pIpc);
    return NULL;
  }

  if((pIpc->hSemCli = OpenSemaphoreA(SYNCHRONIZE | SEMAPHORE_MODIFY_STATE, 
                                    FALSE, XCODE_SEM_CLI_NAME)) == NULL) {
    LOG(X_ERROR("OpenSemaphore %s failed (error:%d)"), XCODE_SEM_CLI_NAME, GetLastError());
    xcode_ipc_close(pIpc);
    return NULL;
  }

  pIpc->pmem->hdr.cmd = XCODE_IPC_CMD_NONE;

  return pIpc;
}

void xcode_ipc_close(XCODE_IPC_DESCR_T *pIpc) {

  if(pIpc) {

    if(pIpc->pmem) {
      UnmapViewOfFile(pIpc->pmem);
      pIpc->pmem = NULL;
    }

    if(pIpc->hFile) {
      CloseHandle(pIpc->hFile);
      pIpc->hFile = NULL;
    }

    if(pIpc->hSemSrv) {
      CloseHandle(pIpc->hSemSrv);
      pIpc->hSemSrv = NULL;
    }

    if(pIpc->hSemCli) {
      CloseHandle(pIpc->hSemCli);
      pIpc->hSemCli = NULL;
    }

    free(pIpc);
  }

}

#else // WIN32

XCODE_IPC_DESCR_T *xcode_ipc_open(unsigned int sz, key_t key) {
  XCODE_IPC_DESCR_T *pIpc = NULL;
  struct shmid_ds buf;
  int shmid;
  //int rc;
#ifdef __linux__
  int flags = 0x180;
#else
  int flags = SEM_R | SEM_A;
#endif // __linux__

  buf.shm_segsz = 0;
  if((shmid = shmget(key, 0, flags)) != -1) {
    // get the segment size of any existing shm
    if(shmctl(shmid, IPC_STAT, &buf) != 0) {
      return NULL;
    }
  }

  if(buf.shm_segsz == 0) {
    LOG(X_ERROR("xcode shared mem not yet created by writer"));
    return NULL;
  }

  if(buf.shm_segsz < sz) {
    LOG(X_ERROR("xcode shared mem size %d below minimum %d"), buf.shm_segsz, sz);
    return NULL;
  }

  pIpc = avc_calloc(1, sizeof(XCODE_IPC_DESCR_T));
  pIpc->key = key;
  pIpc->sz = sz;

  if((pIpc->shmid = shmget(pIpc->key, pIpc->sz, flags)) == -1) {
    LOG(X_ERROR("xcode shmget failed for key: 0x%x flags: 0x%x size: %u"), 
                pIpc->key, flags, pIpc->sz);
    free(pIpc);
    return NULL;
  }

  if(shmctl(pIpc->shmid, IPC_STAT, &buf) != 0) {
    LOG(X_ERROR("xcode shmctl failed for shmid %d"), pIpc->shmid);
    free(pIpc);
    return NULL;
  }

  if(buf.shm_nattch > 1) {
    LOG(X_ERROR("xcode shared mem already has %d attached"), buf.shm_nattch);
    free(pIpc);
    return NULL;
  }

  if((pIpc->pmem = shmat(pIpc->shmid, 0, 0)) == (void *) -1) {
    LOG(X_ERROR("xcode shmat failed"));
    free(pIpc);
    return NULL;
  }

  if((pIpc->sem_srv = sem_open(XCODE_IPC_SEM_NAME_SRV, 0,
                        S_IRWXU | S_IRWXG | S_IRWXO , 0)) == SEM_FAILED) {
    LOG(X_ERROR("xcode sem_open server failed with "ERRNO_FMT_STR), ERRNO_FMT_ARGS);
    pIpc->sem_srv = NULL;
    return NULL;
  }

  if((pIpc->sem_cli = sem_open(XCODE_IPC_SEM_NAME_CLI, 0,
                        S_IRWXU | S_IRWXG | S_IRWXO , 0)) == SEM_FAILED) {
    LOG(X_ERROR("xcode sem_open client failed with "ERRNO_FMT_STR), ERRNO_FMT_ARGS);
    pIpc->sem_cli = NULL;
    return NULL;
  }

  pIpc->pmem->hdr.cmd = XCODE_IPC_CMD_NONE;

  //fprintf(stderr, "init ipc ok\n");

  return pIpc;
}

void xcode_ipc_close(XCODE_IPC_DESCR_T *pIpc) {

  if(pIpc) {
    if(pIpc->shmid > 0) {
      shmdt(pIpc->pmem); 
      pIpc->shmid = 0;
    }

    if(pIpc->sem_srv) {
      if(sem_close(pIpc->sem_srv) != 0) {
        LOG(X_ERROR("sem_close srv failed"));
      }
      pIpc->sem_srv = NULL;
    }

    if(pIpc->sem_cli) {
      if(sem_close(pIpc->sem_cli) != 0) {
        LOG(X_ERROR("sem_close cli failed"));
      }
      pIpc->sem_cli = NULL;
    }

    free(pIpc);
    //fprintf(stderr, "close ipc ok\n");
  }

}

#endif // WIN32

static int xcode_call_ipc(XCODE_IPC_DESCR_T *pIpc,
                          enum XCODE_IPC_CMD cmd) {
  int rc = 0;

  pIpc->pmem->hdr.cmd = cmd;

  //fprintf(stderr, "ipc call cmd: %d\n", cmd);

#if defined(WIN32)
  if(ReleaseSemaphore(pIpc->hSemSrv, 1, NULL) == 0) {
    LOG(X_ERROR("ReleaseSemaphore failed with error: %d"), GetLastError());
#else
  if(sem_post(pIpc->sem_srv) != 0) {
    LOG(X_ERROR("sem_post failed"));
#endif // WIN32
    rc = -1;
  }

#if defined(WIN32)
    if((rc = WaitForSingleObject(pIpc->hSemCli, INFINITE)) == WAIT_FAILED) {
      LOG(X_ERROR("WaitForSingleObject failed (error: %d)"), GetLastError());
#else // WIN32
  if(sem_wait(pIpc->sem_cli) != 0) {
    LOG(X_ERROR("sem_wait failed with "ERRNO_FMT_STR), ERRNO_FMT_ARGS);
#endif // WIN32
    rc = -1;
  }

  if(rc == 0) {
    rc = pIpc->pmem->hdr.cmdrc;
  }  else {
    LOG(X_ERROR("ipc call cmd: %d executed with rc: %d"), cmd, rc);
  }

  return rc;
}


int xcode_init_vid(IXCODE_VIDEO_CTXT_T *pIn) {
  IXCODE_CTXT_T *pXcode;
  IXCODE_VIDEO_CTXT_T *pXcodeV;
  int rc;

  if(!pIn->common.pIpc) {
    return -1;
  }

  pXcode = &((XCODE_IPC_DESCR_T *) pIn->common.pIpc)->pmem->hdr.ctxt;
  pXcodeV = &pXcode->vid;

  pXcodeV->common.cfgFileTypeIn = pIn->common.cfgFileTypeIn;
  pXcodeV->common.cfgFileTypeOut = pIn->common.cfgFileTypeOut;
  pXcodeV->common.cfgVerbosity = pIn->common.cfgVerbosity;
  pXcodeV->common.cfgFlags = pIn->common.cfgFlags;
  pXcodeV->inH = pIn->inH;
  pXcodeV->inV = pIn->inV;
  pXcodeV->out[0].resOutH = pIn->out[0].resOutH;
  pXcodeV->out[0].resOutV = pIn->out[0].resOutV;
  memcpy(&pXcodeV->out[0].crop, &pIn->out[0].crop, sizeof(pXcodeV->out[0].crop));
  pXcodeV->resOutClockHz = pIn->resOutClockHz;
  pXcodeV->resOutFrameDeltaHz = pIn->resOutFrameDeltaHz;
  pXcodeV->out[0].resLookaheadmin1 = pIn->out[0].resLookaheadmin1;
  pXcodeV->out[0].rate.cfgBitRateOut = pIn->out[0].rate.cfgBitRateOut;
  pXcodeV->out[0].rate.cfgBitRateTolOut = pIn->out[0].rate.cfgBitRateTolOut;
  pXcodeV->out[0].cfgMaxSliceSz = pIn->out[0].cfgMaxSliceSz;
  pXcodeV->out[0].cfgThreads = pIn->out[0].cfgThreads;
  pXcodeV->out[0].cfgFast = pIn->out[0].cfgFast;
  pXcodeV->out[0].cfgProfile = pIn->out[0].cfgProfile;
  pXcodeV->out[0].cfgRateCtrl = pIn->out[0].cfgRateCtrl;
  pXcodeV->out[0].cfgQ = pIn->out[0].cfgQ;
  pXcodeV->out[0].cfgQMax = pIn->out[0].cfgQMax;
  pXcodeV->out[0].cfgVBVBufSize = pIn->out[0].cfgVBVBufSize;
  pXcodeV->out[0].cfgVBVMaxRate = pIn->out[0].cfgVBVMaxRate;
  pXcodeV->out[0].cfgVBVMinRate = pIn->out[0].cfgVBVMinRate;
  pXcodeV->out[0].cfgIQRatio = pIn->out[0].cfgIQRatio;
  pXcodeV->out[0].cfgBQRatio = pIn->out[0].cfgBQRatio;
  pXcodeV->out[0].cfgGopMaxMs = pIn->out[0].cfgGopMaxMs;
  pXcodeV->out[0].cfgForceIDR = pIn->out[0].cfgForceIDR;
  pXcodeV->out[0].cfgGopMinMs = pIn->out[0].cfgGopMinMs;
  pXcodeV->out[0].cfgScaler = pIn->out[0].cfgScaler;
  pXcodeV->out[0].cfgSceneCut = pIn->out[0].cfgSceneCut;
  pXcodeV->inClockHz = pIn->inClockHz;
  pXcodeV->inFrameDeltaHz = pIn->inFrameDeltaHz;
  pXcodeV->usewatermark = pIn->usewatermark;
  if((pXcodeV->extradatasize = pIn->extradatasize) > sizeof(pXcodeV->extradata)) {
    return -1;
  }
  memcpy(pXcodeV->extradata, pIn->extradata, pIn->extradatasize);
  memcpy(&pXcodeV->pip, &pIn->pip, sizeof(pXcodeV->pip));

  if((rc = xcode_call_ipc(pIn->common.pIpc, XCODE_IPC_CMD_INIT_VID)) == 0) {
    pIn->common.inpts90Khz = pXcodeV->common.inpts90Khz; 
    pIn->common.outpts90Khz = pXcodeV->common.outpts90Khz; 
    pIn->common.decodeInIdx = pXcodeV->common.decodeInIdx; 
    pIn->common.decodeOutIdx = pXcodeV->common.decodeOutIdx; 
    pIn->common.encodeInIdx = pXcodeV->common.encodeInIdx; 
    pIn->common.encodeOutIdx = pXcodeV->common.encodeOutIdx; 
    //pIn->out[0].qTot = pXcodeV->out[0].qTot;
    //pIn->out[0].qSamples = pXcodeV->out[0].qSamples;
    if((pIn->out[0].hdrLen = pXcodeV->out[0].hdrLen) > sizeof(pIn->out[0].hdr)) {
      pIn->out[0].hdrLen = sizeof(pIn->out[0].hdr);
    }
    memcpy(pIn->out[0].hdr, pXcodeV->out[0].hdr, pIn->out[0].hdrLen);
  }

  return rc;
}
int xcode_init_aud(IXCODE_AUDIO_CTXT_T *pIn) {
  IXCODE_CTXT_T *pXcode;
  IXCODE_AUDIO_CTXT_T *pXcodeA;
  int rc;

  if(!pIn->common.pIpc) {
    return -1;
  }

  pXcode = &((XCODE_IPC_DESCR_T *) pIn->common.pIpc)->pmem->hdr.ctxt;
  pXcodeA = &pXcode->aud;

  pXcodeA->common.cfgFileTypeIn = pIn->common.cfgFileTypeIn;
  pXcodeA->common.cfgFileTypeOut = pIn->common.cfgFileTypeOut;
  pXcodeA->cfgBitRateOut = pIn->cfgBitRateOut;
  pXcodeA->common.cfgVerbosity = pIn->common.cfgVerbosity;
  pXcodeA->cfgSampleRateIn = pIn->cfgSampleRateIn;
  pXcodeA->cfgSampleRateOut = pIn->cfgSampleRateOut;
  pXcodeA->cfgChannelsOut = pIn->cfgChannelsOut;
  pXcodeA->cfgChannelsIn = pIn->cfgChannelsIn;
  pXcodeA->cfgDecoderSamplesInFrame = pIn->cfgDecoderSamplesInFrame;
  pXcodeA->cfgVolumeAdjustment = pIn->cfgVolumeAdjustment;

  if((rc = xcode_call_ipc(pIn->common.pIpc, XCODE_IPC_CMD_INIT_AUD)) == 0) {
    pIn->common.inpts90Khz = pXcodeA->common.inpts90Khz; 
    pIn->common.outpts90Khz = pXcodeA->common.outpts90Khz; 
    pIn->common.decodeInIdx = pXcodeA->common.decodeInIdx; 
    pIn->common.decodeOutIdx = pXcodeA->common.decodeOutIdx; 
    pIn->common.encodeInIdx = pXcodeA->common.encodeInIdx; 
    pIn->common.encodeOutIdx = pXcodeA->common.encodeOutIdx; 
    pIn->encoderSamplesInFrame = pXcodeA->encoderSamplesInFrame;
    //if((pIn->hdrLen = pXcodeA->hdrLen) > sizeof(pIn->hdr)) {
    //  pIn->hdrLen = sizeof(pIn->hdr);
    //}
    //memcpy(pIn->hdr, pXcodeA->hdr, pIn->hdrLen);
    memcpy(pIn->compoundLens, pXcodeA->compoundLens, sizeof(pIn->compoundLens));
  }

  return rc;
}
void xcode_close_vid(IXCODE_VIDEO_CTXT_T *pIn) {

  IXCODE_CTXT_T *pXcode;
  IXCODE_VIDEO_CTXT_T *pXcodeV;
  int rc;

  if(!pIn->common.pIpc) {
    return;
  }

  pXcode = &((XCODE_IPC_DESCR_T *) pIn->common.pIpc)->pmem->hdr.ctxt;
  pXcodeV = &pXcode->vid;

  if((rc = xcode_call_ipc(pIn->common.pIpc, XCODE_IPC_CMD_CLOSE_VID)) == 0) {
    pIn->out[0].hdrLen = pXcodeV->out[0].hdrLen;
  }

  return;
}

void xcode_close_aud(IXCODE_AUDIO_CTXT_T *pIn) {

  if(!pIn->common.pIpc) {
    return;
  }

  xcode_call_ipc(pIn->common.pIpc, XCODE_IPC_CMD_CLOSE_AUD);

  return;
}

int xcode_frame_vid(IXCODE_VIDEO_CTXT_T *pIn, unsigned char *bufIn, unsigned int lenIn,
                    IXCODE_OUTBUF_T *pout) {
  XCODE_IPC_MEM_T *pMem;
  IXCODE_VIDEO_CTXT_T *pXcodeV;
  unsigned int lenCopied;
  int rc;
  unsigned char *bufOut = pout->buf;
  unsigned int lenOut = pout->lenbuf;

  if(!pIn->common.pIpc) {
    return -1;
  }

  if(bufIn && bufIn != bufOut) {
    LOG(X_ERROR("buffer output does not match input")); 
    return -1;
  }

  pMem = ((XCODE_IPC_DESCR_T *) pIn->common.pIpc)->pmem;
  pXcodeV = &pMem->hdr.ctxt.vid;

  pXcodeV->common.inpts90Khz = pIn->common.inpts90Khz;
  pXcodeV->common.outpts90Khz = pIn->common.outpts90Khz;
  pXcodeV->common.decodeInIdx = pIn->common.decodeInIdx;
  pXcodeV->common.decodeOutIdx = pIn->common.decodeOutIdx;
  pXcodeV->common.encodeInIdx = pIn->common.encodeInIdx;
  pXcodeV->common.encodeOutIdx = pIn->common.encodeOutIdx;
  pXcodeV->out[0].cfgForceIDR = pIn->out[0].cfgForceIDR;
  pXcodeV->out[0].qTot = pIn->out[0].qTot;
  pXcodeV->out[0].qSamples = pIn->out[0].qSamples;
  pXcodeV->out[0].pts = pIn->out[0].pts;
  pXcodeV->out[0].dts = pIn->out[0].dts;

  if((lenCopied = lenIn) > XCODE_IPC_MEM_DATASZ(pMem)) {
    lenCopied = XCODE_IPC_MEM_DATASZ(pMem);
    LOG(X_WARNING("Video frame length to be decoded truncated from %d to %d"), lenIn, lenCopied);
  }
  if(bufIn) {
    memcpy(XCODE_IPC_MEM_DATA(pMem), bufIn, lenCopied);
  }
  pMem->hdr.cmdrc = (enum IXCODE_RC) lenCopied;
  pMem->hdr.offsetOut = 0;

  if((rc = xcode_call_ipc(pIn->common.pIpc, XCODE_IPC_CMD_ENCODE_VID)) != -1) {
    pIn->common.inpts90Khz = pXcodeV->common.inpts90Khz;
    pIn->common.outpts90Khz = pXcodeV->common.outpts90Khz;
    pIn->common.decodeInIdx = pXcodeV->common.decodeInIdx;
    pIn->common.decodeOutIdx = pXcodeV->common.decodeOutIdx;
    pIn->common.encodeInIdx = pXcodeV->common.encodeInIdx;
    pIn->common.encodeOutIdx = pXcodeV->common.encodeOutIdx;
    pIn->out[0].qTot = pXcodeV->out[0].qTot;
    pIn->out[0].qSamples = pXcodeV->out[0].qSamples;
    pIn->out[0].frameType = pXcodeV->out[0].frameType;
    pIn->out[0].pts = pXcodeV->out[0].pts;
    pIn->out[0].dts = pXcodeV->out[0].dts;

    if((lenCopied = rc) > XCODE_IPC_MEM_DATASZ(pMem)) {
      lenCopied = XCODE_IPC_MEM_DATASZ(pMem);
      LOG(X_WARNING("Encoded video frame length truncated from %d to %d"), rc, lenCopied);
      rc = lenCopied;
    } else if(lenCopied > lenOut) {
      lenCopied = lenOut;
      LOG(X_WARNING("Encoded video frame length truncated from %d to %d"), rc, lenCopied);
      rc = lenCopied;
    }
    memcpy(bufOut, XCODE_IPC_MEM_DATA(pMem), lenCopied);
  }

  return rc;
}
int xcode_frame_aud(IXCODE_AUDIO_CTXT_T *pIn, unsigned char *bufIn, unsigned int lenIn,
                    unsigned char *bufOut, unsigned int lenOut) {
  XCODE_IPC_MEM_T *pMem;
  IXCODE_AUDIO_CTXT_T *pXcodeA;
  int rc, rcorig;

  if(!pIn->common.pIpc) {
    return -1;
  }

  pMem = ((XCODE_IPC_DESCR_T *) pIn->common.pIpc)->pmem;
  pXcodeA = &pMem->hdr.ctxt.aud;

  if(lenIn + lenOut > XCODE_IPC_MEM_DATASZ(pMem)) {
    LOG(X_ERROR("shared mem buffer size %d too small for %d + %d"),
                XCODE_IPC_MEM_DATASZ(pMem), lenIn, lenOut); 
    return -1;
  } 

  pXcodeA->common.inpts90Khz = pIn->common.inpts90Khz;
  pXcodeA->common.outpts90Khz = pIn->common.outpts90Khz;
  pXcodeA->common.decodeInIdx = pIn->common.decodeInIdx;
  pXcodeA->common.decodeOutIdx = pIn->common.decodeOutIdx;
  pXcodeA->common.encodeInIdx = pIn->common.encodeInIdx;
  pXcodeA->common.encodeOutIdx = pIn->common.encodeOutIdx;
  pXcodeA->pts = pIn->pts;

  // The following may not have been set during init
  //pXcodeA->cfgChannelsIn = pIn->cfgChannelsIn;
  //pXcodeA->cfgSampleRateIn = pIn->cfgSampleRateIn;

  if(bufIn) {
    memcpy(XCODE_IPC_MEM_DATA(pMem), bufIn, lenIn);
  }
  pMem->hdr.cmdrc = lenIn;
  pMem->hdr.offsetOut = lenIn;

  if((rc = rcorig = xcode_call_ipc(pIn->common.pIpc, XCODE_IPC_CMD_ENCODE_AUD)) != -1) {
    pIn->common.inpts90Khz = pXcodeA->common.inpts90Khz;
    pIn->common.outpts90Khz = pXcodeA->common.outpts90Khz;
    pIn->common.decodeInIdx = pXcodeA->common.decodeInIdx;
    pIn->common.decodeOutIdx = pXcodeA->common.decodeOutIdx;
    pIn->common.encodeInIdx = pXcodeA->common.encodeInIdx;
    pIn->common.encodeOutIdx = pXcodeA->common.encodeOutIdx;
    pIn->pts = pXcodeA->pts;

    if(rcorig > (int) (XCODE_IPC_MEM_DATASZ(pMem) - lenIn)) {
      rc = XCODE_IPC_MEM_DATASZ(pMem) - lenIn;
      LOG(X_WARNING("Encoded audio frame length truncated from %d to %d"), rcorig, rc);
    } else if(rc > (int) lenOut) {
      rc = lenOut;
      LOG(X_WARNING("Encoded audio frame length truncated from %d to %d"), rcorig, rc);
    }
    memcpy(bufOut, XCODE_IPC_MEM_DATA(pMem) + lenIn, rc);
  }

  return rc;
}

#else // XCODE_IPC

int xcode_init_vid(IXCODE_VIDEO_CTXT_T *pXcode) {
  return ixcode_init_vid(pXcode); 
}

int xcode_init_aud(IXCODE_AUDIO_CTXT_T *pXcode) {
  return ixcode_init_aud(pXcode); 
}

void xcode_close_vid(IXCODE_VIDEO_CTXT_T *pXcode) {
  ixcode_close_vid(pXcode);
}

void xcode_close_aud(IXCODE_AUDIO_CTXT_T *pXcode) {
  ixcode_close_aud(pXcode);
}

int xcode_frame_vid(IXCODE_VIDEO_CTXT_T *pXcode, unsigned char *bufIn, unsigned int lenIn,
                    IXCODE_OUTBUF_T *pout) {
                    //unsigned char *bufOut, unsigned int lenOut,
                    //unsigned char *bufsOut[], unsigned int lensOut[]) {

  //return ixcode_frame_vid(pXcode, bufIn, lenIn, bufOut, lenOut, bufsOut, lensOut);
  return ixcode_frame_vid(pXcode, bufIn, lenIn, pout);
}

int xcode_frame_aud(IXCODE_AUDIO_CTXT_T *pXcode, unsigned char *bufIn, unsigned int lenIn,
                    unsigned char *bufOut, unsigned int lenOut) {
  return ixcode_frame_aud(pXcode, bufIn, lenIn, bufOut, lenOut);
}

#endif // XCODE_IPC


#else // ENABLE_XCODE

int xcode_init_vid(IXCODE_VIDEO_CTXT_T *pXcode) {
  return -1;
}

int xcode_init_aud(IXCODE_AUDIO_CTXT_T *pXcode) {
  return -1;
}

void xcode_close_vid(IXCODE_VIDEO_CTXT_T *pXcode) {
}

void xcode_close_aud(IXCODE_AUDIO_CTXT_T *pXcode) {
}

int xcode_frame_vid(IXCODE_VIDEO_CTXT_T *pXocde, unsigned char *bufIn, unsigned int lenIn,
                    IXCODE_OUTBUF_T *pout) {
  return -1;
}

int xcode_frame_aud(IXCODE_AUDIO_CTXT_T *pXocde, unsigned char *bufIn, unsigned int lenIn,
                      unsigned char *bufOut, unsigned int lenOut) {
  return -1;
}

#endif // ENABLE_XCODE
