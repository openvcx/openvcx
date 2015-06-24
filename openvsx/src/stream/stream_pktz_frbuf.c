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

#if defined(VSX_HAVE_STREAMER)


int stream_pktz_frbuf_init(void *pArg, const void *pInitArgs, unsigned int progIdx) {
  PKTZ_FRBUF_T *pPktzFrbuf = (PKTZ_FRBUF_T *) pArg;
  PKTZ_INIT_PARAMS_FRBUF_T *pInitParams = (PKTZ_INIT_PARAMS_FRBUF_T *) pInitArgs;
  unsigned int sz = 0;
  int rc = 0;

  if(!pPktzFrbuf || !pInitParams) {
    return -1;
  }

  //fprintf(stderr, "pktz_frbuf_init 0x%x codectype:%d progidx:%d (%dHz %dHz)\n", pPktzFrbuf, pInitParams->va.codecType, progIdx, pInitParams->common.clockRateHz, pInitParams->va.clockRateHz);

  pPktzFrbuf->pFrameData = pInitParams->common.pFrameData;
  pPktzFrbuf->consecErrors = 0;
  memcpy(&pPktzFrbuf->va, &pInitParams->va, sizeof(pPktzFrbuf->va));
  pPktzFrbuf->va.isvideo = (progIdx == 0) ? 1 : 0;
  
  if(pPktzFrbuf->va.clockRateHz == 0) {
    pPktzFrbuf->va.clockRateHz = pInitParams->common.clockRateHz;
  }

  if(pPktzFrbuf->va.isvideo) {

    if((sz = pPktzFrbuf->va.u.v.width * pPktzFrbuf->va.u.v.height * 
         codecType_getBitsPerPx(pPktzFrbuf->va.codecType)) <= 0 ||
       !(pPktzFrbuf->pFrBufOut = framebuf_create(sz, 0))) {
      LOG(X_ERROR("framebuffer packetizer[%d] has invalid dimenions %d x %d %dbps"),
        progIdx, pPktzFrbuf->va.u.v.width, pPktzFrbuf->va.u.v.height, 
        codecType_getBitsPerPx(pPktzFrbuf->va.codecType));
      return -1;
    }
    pPktzFrbuf->pFrBufOut->cfg.id = 20; 

  } else {
    
    // 
    // default to 1 sec duration of audio output samples buffer
    //
    sz = pPktzFrbuf->va.u.a.channels * pPktzFrbuf->va.clockRateHz * 2; 

    if(pPktzFrbuf->va.u.a.samplesbufdurationms > 0) {
      sz *= ((float) pPktzFrbuf->va.u.a.samplesbufdurationms / 1000);
    }

    if(sz <= 0 || !(pPktzFrbuf->pFrBufOut = ringbuf_create(sz))) {
      LOG(X_ERROR("framebuffer packetizer[%d] has invalid channels %d or %dHz"),
        progIdx, pPktzFrbuf->va.u.a.channels, pPktzFrbuf->va.clockRateHz);
      return -1;
    }
    pPktzFrbuf->pFrBufOut->cfg.id = 21; 

  }

  LOG(X_DEBUG("Allocated local framebuffer(id:%d) size: %d"), 
      pPktzFrbuf->pFrBufOut->cfg.id, sz);

  if(pInitParams->ppPktzFrBufOut) {
    *pInitParams->ppPktzFrBufOut = pPktzFrbuf->pFrBufOut;
    pPktzFrbuf->ppPktzFrBufOut = pInitParams->ppPktzFrBufOut;
  }

  if(rc < 0) {
     stream_pktz_frbuf_close(pPktzFrbuf); 
  }

  return rc;
}

int stream_pktz_frbuf_reset(void *pArg, int fullReset, unsigned int progIdx) {
  PKTZ_FRBUF_T *pPktzFrbuf = (PKTZ_FRBUF_T *) pArg;

  pPktzFrbuf->consecErrors = 0;

  return 0;
}

int stream_pktz_frbuf_close(void *pArg) {
  PKTZ_FRBUF_T *pPktzFrbuf = (PKTZ_FRBUF_T *) pArg;

  if(pPktzFrbuf == NULL) {
    return -1;
  }

  if(pPktzFrbuf->ppPktzFrBufOut) {
    *pPktzFrbuf->ppPktzFrBufOut = NULL;
  }
  if(pPktzFrbuf->pFrBufOut) {
    pktqueue_destroy(pPktzFrbuf->pFrBufOut);
    pPktzFrbuf->pFrBufOut = NULL;
  }

  return 0;
}

//static FILE_HANDLE delme_a;
//static FILE_HANDLE delme_v;

int stream_pktz_frbuf_addframe(void *pArg, unsigned int progIdx) {
  PKTZ_FRBUF_T *pPktzFrbuf = (PKTZ_FRBUF_T *) pArg;
  int rc = 0;
  PKTQ_EXTRADATA_T xtra;
  //BMP_FILE_T bmp;

  //fprintf(stderr, "pktz_frbuf_addframe[%d] %dHz len:%d pts:%.3f dts:%.3f %dx%d tm:%llu\n", progIdx, pPktzFrbuf->clockRateHz, pPktzFrbuf->pFrameData->len, PTSF(pPktzFrbuf->pFrameData->pts), PTSF(pPktzFrbuf->pFrameData->dts), pPktzFrbuf->width, pPktzFrbuf->height, timer_GetTime());

  xtra.tm.pts = OUTFMT_PTS(pPktzFrbuf->pFrameData);
  xtra.tm.dts = OUTFMT_DTS(pPktzFrbuf->pFrameData);
  //xtra.flags = 0;
  xtra.pQUserData = NULL;

  if((rc = pktqueue_addpkt(pPktzFrbuf->pFrBufOut, 
                           OUTFMT_DATA(pPktzFrbuf->pFrameData),
                           OUTFMT_LEN(pPktzFrbuf->pFrameData), &xtra, 0)) < 0) {

    LOG(X_ERROR("framebuffer packetizer[%d] failed to add frame sz:%d"),
                progIdx, OUTFMT_LEN(pPktzFrbuf->pFrameData));  
  }


/*
  if(progIdx == 1) {
    if(delme_a == 0) delme_a  = fileops_Open("/sdcard/delme.pcm", O_RDWR | O_CREAT);
    rc = fileops_WriteBinary(delme_a, pPktzFrbuf->pFrameData->pData, pPktzFrbuf->pFrameData->len);
    //fprintf(stderr, "pktz_frbuf audio wrote %d\n", rc);
  }
*/


/*
  if(progIdx == 0) {
    delme_v  = fileops_Open("delme.rgb565", O_RDWR | O_CREAT);
    rc = fileops_WriteBinary(delme_v, pPktzFrbuf->pFrameData->pData, pPktzFrbuf->pFrameData->len);
    fprintf(stderr, "pktz_frbuf audio wrote %d\n", rc);
    close(delme_v);
  }
*/


  return rc;
}

#endif // VSX_HAVE_STREAMER
