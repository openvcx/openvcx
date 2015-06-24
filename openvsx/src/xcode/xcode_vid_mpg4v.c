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

int xcode_mpg4v_find_seqhdrs(const unsigned char *pData, unsigned int len, MPG4V_SEQ_HDRS_T *pHdrs) {
  int rc = 0;
  int idxStartCode;
  unsigned char *p;
  STREAM_CHUNK_T stream;
  MPG4V_DESCR_T mpg4v;

  if(!pData || !pHdrs) {
    return -1;
  }

  memset(&mpg4v, 0, sizeof(mpg4v));

  memset(&stream, 0, sizeof(stream));
  stream.bs.buf = (unsigned char *) pData;
  stream.bs.sz = len;

  if(stream.bs.sz < 4) {
    return 0;
  } else if(mpg2_findStartCode(&stream) != 3) {
    LOG(X_ERROR("Ouput mpeg4v frame does not contain valid start code"));
    return -1;
  }

  stream.bs.sz -= 3;
  stream.bs.byteIdx += 3;

  //avc_dumpHex(stderr, pData, MIN(128, len), 1);

  do {

    //avc_dumpHex(stderr, &stream.bs.buf[stream.bs.byteIdx], 8, 0);

    p =  &stream.bs.buf[stream.bs.byteIdx - 3];
    if(mpg4v_decodeObj(&stream, &mpg4v) < 0) {
      break;
    }

    if((idxStartCode = mpg2_findStartCode(&stream)) <= 0) {
      break;
    }

    if(mpg4v.ctxt.flag == MPG4V_DECODER_FLAG_HDR) {
      mpg4v_appendStartCodes(&mpg4v, p, idxStartCode);
    }

    //fprintf(stderr, "START CODE:0x%x idxSC:%d\n", mpg4v.ctxt.startCode, idxStartCode);

    stream.bs.byteIdx += idxStartCode;

  } while(stream.bs.byteIdx < stream.bs.sz);

  if(mpg4v.seqHdrs.hdrsLen > 0) {
    rc = mpg4v.seqHdrs.hdrsLen;
    memcpy(pHdrs, &mpg4v.seqHdrs, sizeof(MPG4V_SEQ_HDRS_T));
  }
  //fprintf(stderr, "HEADERS:\n"); avc_dumpHex(stderr, mpg4v.seqHdrs.hdrsBuf, MIN(256, mpg4v.seqHdrs.hdrsLen), 1);

  return rc;
} 




int xcode_vid_mpg4v_check(STREAM_XCODE_DATA_T *pData, unsigned int outidx) {
  int rc = 0;
  //int irc;
  int idxStartCode;
  int64_t pts_adjustment;
  STREAM_CHUNK_T stream;
  IXCODE_VIDEO_CTXT_T *pXcode = &pData->piXcode->vid;
  STREAM_XCODE_VID_UDATA_T *pUData = (STREAM_XCODE_VID_UDATA_T *)pXcode->pUserData;

//fprintf(stderr, "MPG4V out result len:%d headers len:%d\n", pData->curFrame.lenData, pXcode->hdrLen);
//avc_dumpHex(stderr, pData->curFrame.pData, pData->curFrame.lenData > 64 ? 64 : pData->curFrame.lenData, 1);
//avc_dumpHex(stderr, pXcode->hdr, pXcode->hdrLen, 1);

  memset(&stream, 0, sizeof(stream));
  stream.bs.buf = (unsigned char *) pData->curFrame.xout.outbuf.poffsets[outidx];
  stream.bs.sz = pData->curFrame.xout.outbuf.lens[outidx];

  if(stream.bs.sz < 4) {
    return 0;
  } else if(mpg2_findStartCode(&stream) != 3) {
    LOG(X_ERROR("Ouput mpeg4v frame does not contain valid start code"));
    return -1;
  }

  stream.bs.sz -= 3;
  stream.bs.byteIdx += 3;

  pData->curFrame.xout.keyframes[outidx] = 0;

  do {

    //avc_dumpHex(stderr, &stream.bs.buf[stream.bs.byteIdx], 8, 0);

    if(mpg4v_decodeObj(&stream, &pUData->out[outidx].uout.xmpg4v.mpg4v) < 0) {
      break;
    }
    //TODO: look and extract sequence headers from the bit stream
    //fprintf(stderr, "MPEG-4 headers.... %d\n", pUData->uout.xmpg4v.mpg4v.seqHdrs.hdrsLen);
    if(pUData->out[outidx].uout.xmpg4v.mpg4v.ctxt.lastFrameType == MPG4V_FRAME_TYPE_I) {
      pData->curFrame.xout.keyframes[outidx] = 1;
    }

    //
    // Don't waste checking each byte
    //
    if(pUData->out[outidx].uout.xmpg4v.mpg4v.ctxt.startCode == MPEG4_HDR8_VOP) {
      break;
    }

    if((idxStartCode = mpg2_findStartCode(&stream)) <= 0) {
      break;
    }

    stream.bs.byteIdx += idxStartCode;

  } while(stream.bs.byteIdx < stream.bs.sz);


  if(pXcode->out[outidx].dts != 0 && 
     pXcode->resOutFrameDeltaHz > 0 && pXcode->resOutClockHz > 0) {
    pts_adjustment = (int64_t) ( (90000.0f * (float)pXcode->resOutFrameDeltaHz /
            //pXcode->resOutClockHz)* (pXcode->out[outidx].dts / pXcode->resOutFrameDeltaHz));
            pXcode->resOutClockHz)* pXcode->out[outidx].dts );

    pData->curFrame.pkt.xtra.tm.pts -= pts_adjustment;
  }
  //fprintf(stderr, "xcode_vid_mpg4[%d] xcode pts:%lld,%lld, pts:%.3f,%.3f adj:%.3f clock%d/%d\n", outidx, pXcode->out[outidx].pts, pXcode->out[outidx].dts, PTSF(pData->curFrame.pkt.xtra.tm.pts), PTSF(pData->curFrame.pkt.xtra.tm.dts), PTSF(pts_adjustment), pXcode->resOutClockHz, pXcode->resOutFrameDeltaHz);


  return rc;
}


