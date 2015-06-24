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


void vorbis_free(VORBIS_DESCR_T *pVorbis) {

  //if(pVorbis->codecPrivData) {
  //  avc_free((void **) &pVorbis->codecPrivData);
  //}
  //pVorbis->codecPrivLen = 0;

}

int vorbis_loadFramesFromMkv(VORBIS_DESCR_T *pVorbis, MKV_CONTAINER_T *pMkv) {
  int rc = 0;
  //double fpsmkv;

  if(!pMkv->aud.pTrack) {
    return -1;
  }

  //if((fpsmkv = mkv_getfps(pMkv)) <= 0) {
  //  LOG(X_ERROR("Unable to extract audio timing information from mkv"));
  //  return -1;
  //}

  //vid_convert_fps(fpsmkv, &pVorbis->clockHz, &pVorbis->frameDeltaHz);


  //TODO: get clock from codecPriv

  pVorbis->lastFrameId = 0;
  pVorbis->numSamples = 0;
  pVorbis->pMkv = pMkv;
  pVorbis->codecPrivData = pMkv->aud.pTrack->codecPrivate.p;
  pVorbis->codecPrivLen = pMkv->aud.pTrack->codecPrivate.size;

  if(pVorbis->codecPrivData && pVorbis->codecPrivLen > 0) {

    if((rc = vorbis_parse_hdr(pVorbis->codecPrivData, pVorbis->codecPrivLen, pVorbis)) < 0) {
      return rc;
    }
  }

  if(pVorbis->clockHz == 0) {
    pVorbis->clockHz = pMkv->aud.pTrack->audio.samplingFreq;
  } else if((uint32_t) pMkv->aud.pTrack->audio.samplingFreq > 0 && (uint32_t) pMkv->aud.pTrack->audio.samplingFreq != pVorbis->clockHz) {
    LOG(X_WARNING("Vorbis container clock: %.1fHz differs from codec-private-data clock: %dHz"), pMkv->aud.pTrack->audio.samplingFreq, pVorbis->clockHz);
  }

  LOG(X_DEBUG("Vorbis channels: %d, clock: %dHz, block-size: %d,%d, frame-size:%d"), pVorbis->channels, pVorbis->clockHz, pVorbis->blocksize[0], pVorbis->blocksize[1], pVorbis->framesize);

  return rc;
}

MP4_MDAT_CONTENT_NODE_T *vorbis_cbGetNextSample(void *pArg, int idx, int advanceFp) {
  VORBIS_DESCR_T *pVorbis = (VORBIS_DESCR_T *) pArg;

  if(!pVorbis) {
    return NULL;
  }

  //TODO: respect idx for looping

  if(mkv_loadFrame(pVorbis->pMkv, &pVorbis->lastFrame, MKV_TRACK_TYPE_AUDIO, 
                   pVorbis->clockHz, &pVorbis->lastFrameId) < 0) {
    return NULL;
  }

//fprintf(stderr, "GOT MKV VORBIS block sz:%d, %lldHz (clock:%dHz) \n", pVorbis->lastFrame.sizeRd, pVorbis->lastFrame.playOffsetHz, pVorbis->clockHz);

  return &pVorbis->lastFrame;
}

int vorbis_parse_hdr(const unsigned char *pbuf, unsigned int len, VORBIS_DESCR_T *pVorbis) {
  int rc = 0;
  unsigned int idx = 0;
  int headers_len[3];
  const unsigned char *headers[3];
  unsigned int blocksz[2];
 
  if(!pVorbis || !pbuf || len <= 3) {
    return -1;
  }

  //fprintf(stderr, "VORBIS PARSE HDR len:%d\n", len);
  //avc_dumpHex(stderr, pbuf, MIN(len, 64), 1);

  if(pbuf[idx++] != 0x02) {
    return -1;
  }

  memset(headers_len, 0, sizeof(headers_len));
  while((headers_len[0] += pbuf[idx++]) == 0xff && idx < len);
  while((headers_len[1] += pbuf[idx++]) == 0xff && idx < len);
  headers_len[2] = len - headers_len[0] - headers_len[1] - idx;

  if(headers_len[0] < 30 || headers_len[1] <= 1 || headers_len[2] <= 1) {
    return -1;
  }
  headers[0] = &pbuf[idx];
  headers[1] = headers[0] + headers_len[0];
  headers[2] = headers[1] + headers_len[1];

  //
  // ID header
  //
  idx = 0;
  if(headers[0][idx++] != 0x01) {
    return -1;
  } else if(headers[0][idx++] != 'v' ||  headers[0][idx++] != 'o' || headers[0][idx++] != 'r' ||
     headers[0][idx++] != 'b' ||  headers[0][idx++] != 'i' || headers[0][idx++] != 's') {
    return -1;
  }

  if(*((const uint32_t *) &(headers[0])[idx]) != 0) { // version 
    return -1;
  }
  idx += 4;
  
  if((pVorbis->channels = headers[0][idx++]) <= 0) {
    return -1;
  }
  if((pVorbis->clockHz = *((const uint32_t *) &(headers[0])[idx])) <= 0) {
    return -1;
  }
  idx += 4;

  pVorbis->bitrate_max = *((const uint32_t *) &(headers[0])[idx]);
  idx += 4;
  pVorbis->bitrate_avg = *((const uint32_t *) &(headers[0])[idx]);
  idx += 4;
  pVorbis->bitrate_min = *((const uint32_t *) &(headers[0])[idx]);
  idx += 4;

  blocksz[0] = (headers[0][idx] >> 4);
  blocksz[1] = (headers[0][idx++] & 0x0f);
  pVorbis->blocksize[0] = 1 << blocksz[0];
  pVorbis->blocksize[1] = 1 << blocksz[1];

  if((headers[0][idx++] & 0x80) != 0) {
    return -1;
  }

  pVorbis->framesize = MIN(pVorbis->blocksize[0], pVorbis->blocksize[1]) >> 2;

  //avc_dumpHex(stderr, headers[2], MIN(32, headers_len[2]), 1);
  //
  // Setup header
  //
  idx = 0;
  if(headers[2][idx++] != 0x05) {
    return -1;
  }

  //fprintf(stderr, "HEADERS_LEN:%d,%d,%d, idx:%d,chan:%d, clock:%d, blksz:%d,%d, frsz:%d\n", headers_len[0], headers_len[1], headers_len[2], idx, pVorbis->channels, pVorbis->clockHz, pVorbis->blocksize[0], pVorbis->blocksize[1], pVorbis->framesize);

  return rc;
}
