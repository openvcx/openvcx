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


void vp8_free(VP8_DESCR_T *pVp8) {

}

int vp8_loadFramesFromMkv(VP8_DESCR_T *pVp8, MKV_CONTAINER_T *pMkv) {
  int rc = 0;
  double fpsmkv;
  MKV_BLOCK_T block;
  unsigned char buf[32];
  unsigned int idx = 0;

  if((fpsmkv = mkv_getfps(pMkv)) <= 0) {
    LOG(X_ERROR("Unable to extract video timing information from mkv"));
    return -1;
  }

  vid_convert_fps(fpsmkv, &pVp8->clockHz, &pVp8->frameDeltaHz);

  pVp8->lastFrameId = 0;
  pVp8->numSamples = 0;
  pVp8->pMkv = pMkv;

  while((rc = mkv_next_block(pMkv, &block)) >= 0) {

    if(block.pTrack && block.pTrack->type == MKV_TRACK_TYPE_VIDEO && block.keyframe) {

      if(SeekMediaFile(pMkv->pStream, MKV_BLOCK_FOFFSET(&block), SEEK_SET) < 0 || 
         (rc = ReadFileStream(pMkv->pStream, buf, 12)) != 12) {
        rc = -1;
        break;
      }
      if((rc = vp8_parse_hdr(buf, rc, pVp8) >= 0)) {
        //fprintf(stderr, "GOT ID idx:%d %dx%d\n", idx, pVp8->frameWidthPx, pVp8->frameHeightPx);
        break;
      }

    }

    if(idx++ > 1024) {
      LOG(X_WARNING("Mkv probe keyframe not found after %d blocks"), idx);
      break;
    }
  } 

  //
  // Reset the mkv parser contxt to the start of the first clustesr
  //
  if(rc >= 0) {
    rc = mkv_reset(pMkv);
  }

  return rc;
}

MP4_MDAT_CONTENT_NODE_T *vp8_cbGetNextSample(void *pArg, int idx, int advanceFp) {
  VP8_DESCR_T *pVp8 = (VP8_DESCR_T *) pArg;

  if(!pVp8) {
    return NULL;
  }

  //TODO: respect idx for looping

  if(mkv_loadFrame(pVp8->pMkv, &pVp8->lastFrame, MKV_TRACK_TYPE_VIDEO, 
                   pVp8->clockHz, &pVp8->lastFrameId) < 0) {
    return NULL;
  }

//fprintf(stderr, "GOT MKV VP8 block sz:%d, %lldHz (clock:%dHz) \n", pVp8->lastFrame.sizeRd, pVp8->lastFrame.playOffsetHz, pVp8->clockHz);

  return &pVp8->lastFrame;
}

int vp8_parse_hdr(const unsigned char *pbuf, unsigned int len, VP8_DESCR_T *pVp8) {
  int rc = 0;

  if(!pbuf || len < 1 || !pVp8) {
    return -1;
  }

  if(!VP8_ISKEYFRAME(pbuf)) {
    // not a keyframe
    return -1;
  }

  if(len < 9) {
    return -1;
  }

  pVp8->frameWidthPx = *((uint16_t *) &pbuf[6]) & 0x3fff;
  pVp8->frameHeightPx = *((uint16_t *) &pbuf[8]) & 0x3fff;

  return rc;
}
