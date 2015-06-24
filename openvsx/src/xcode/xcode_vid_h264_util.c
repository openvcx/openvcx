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


static int testh264_handlespspps(XCODE_H264_OUT_T *pXh264,
                                 BIT_STREAM_T *pStream,
                                 SPSPPS_RAW_T *pspspps) {
  H264_STREAM_CHUNK_T bs;
  H264_STREAM_CHUNK_T bstmp;
  unsigned char *pBuf;
  unsigned int lenBuf;
  int pos_sps = 0;
  int pos_pps = 0;
  int sps_len = 0;
  int pps_len = 0;
  unsigned int ui;

  memset(&bs, 0, sizeof(bs));
  pBuf = &pStream->buf[pStream->byteIdx];
  lenBuf = pStream->sz - pStream->byteIdx;
  bs.bs.buf = pBuf;
  bs.bs.sz = lenBuf;

  //LOG(X_DEBUG("testh264_handlespspps 0x%x 0x%x 0x%x 0x%x 0x%x, lenBuf:%d"), pBuf[0], pBuf[1], pBuf[2], pBuf[3], pBuf[4], lenBuf); LOGHEX_DEBUG(pBuf, MIN(64, lenBuf));

  if((pos_sps = h264_findStartCode(&bs, 0, NULL)) >= H264_STARTCODE_LEN) {
    pos_sps -= H264_STARTCODE_LEN;

    memset(&bs, 0, sizeof(bs));
    bs.bs.buf = (unsigned char *) pBuf + pos_sps + H264_STARTCODE_LEN;
    bs.bs.sz = lenBuf - (bs.bs.buf - pBuf);

    if((sps_len = h264_findStartCode(&bs, 0, NULL)) >= H264_STARTCODE_LEN) {
      pos_pps = pos_sps + sps_len;
      //
      // find end of pps
      //
      memset(&bs, 0, sizeof(bs));
      bs.bs.buf = (unsigned char *) pBuf + pos_pps + H264_STARTCODE_LEN;
      bs.bs.sz = lenBuf - (bs.bs.buf - pBuf);
      memcpy(&bstmp, &bs, sizeof(bstmp));
      //LOG(X_DEBUG("sps_len:%d, pos_pps[%d], finding end of pps"), sps_len, pos_pps);
      if((pps_len = h264_findStartCode(&bs, 0, NULL)) >= H264_STARTCODE_LEN) {
        //LOG(X_DEBUG("End is.. pps_len:%d"), pps_len);
      //} else if(pps_len != -1) {
      } else {
        //memcpy(&bs, &bstmp, sizeof(bs));
        //LOG(X_DEBUG("sps_len:%d, pos_pps[%d], pps_len:%d, finding end of pps again w/ short option at 0x%x 0x%x len:%d, matchidx:%d"), sps_len, pos_pps, pps_len, bs.bs.buf[0], bs.bs.buf[1], bs.bs.sz, bs.startCodeMatch);

        // Check for 0x00 0x00 0x01
        if((pps_len = h264_findStartCode(&bs, 1, NULL)) >= H264_STARTCODE_LEN) {
          //LOG(X_DEBUG("SHORT pps_len:%d"), pps_len);
          pps_len = pps_len + 4 - 3; // minus short tail code + prev start code
        //} else if(pps_len != -1) {
        } else {

          if(pps_len > 32) {
            LOG(X_WARNING("Failed to find end of H.264 PPS (%d) pps_len:%d"), pps_len, lenBuf - pos_pps);
            LOGHEX_DEBUG(bstmp.bs.buf, 32);
          }
          pps_len = lenBuf - pos_pps;
        }

      }

      if(sps_len < 0) {
        sps_len = 0;
      }
      if(pps_len < 0) {
        pps_len = 0;
      }

      //avc_dumpHex(stderr, pBuf, 48, 1);
      //LOG(X_DEBUG("DECODE len:%d sps[%d len:%d] pps[%d len:%d]"), lenBuf, pos_sps, sps_len, pos_pps, pps_len);

      //
      // Decode expected encoder output SPS, PPS -
      // which is expected to be bundled w/ each received  I frame
      //
      if(sps_len > 0) {
        xcode_h264_decodenalhdr(pXh264, &pBuf[pos_sps], sps_len);
      } 

      if(pps_len > 0) {
        xcode_h264_decodenalhdr(pXh264, &pBuf[pos_pps], pps_len);
      }

      if(sps_len <= 0 || pps_len <= 0) {
        return -1;
      }

      pStream->byteIdx += sps_len + pps_len;


      //
      // Retain raw output SPS, PPS
      //
      if(pspspps) {

        pspspps->sps_len = sps_len - H264_STARTCODE_LEN;
        if((ui = pspspps->sps_len) > AVCC_SPS_MAX_LEN) {
          ui = AVCC_SPS_MAX_LEN;
        }
        memcpy(pspspps->sps_buf, &pBuf[pos_sps + H264_STARTCODE_LEN], ui);
        pspspps->sps = pspspps->sps_buf;

        pspspps->pps_len = pps_len - H264_STARTCODE_LEN;
        if((ui = pspspps->pps_len) > AVCC_SPS_MAX_LEN) {
          ui = AVCC_SPS_MAX_LEN;
        }
        memcpy(pspspps->pps_buf, &pBuf[pos_pps + H264_STARTCODE_LEN], ui);
        pspspps->pps = pspspps->pps_buf;

      }

      pXh264->pocDelta = 2;

      return 0;
    }

  }

  return -1;
}


int xcode_h264_decodenalhdr(XCODE_H264_OUT_T *pXh264,
                                 const unsigned char *pNal,
                                 unsigned int len) {
  BIT_STREAM_T bs;
  H264_RESULT_T rch264;  int rc = 0;
  int startHdrLen;
  int isvcl;

  if(len < H264_STARTCODE_LEN + 1) {
    return -1;
  }

  //LOG(X_DEBUG("xcode_h264_decodenalhdr 0x%x 0x%x 0x%x 0x%x 0x%x, len:%d"), pNal[0], pNal[1], pNal[2], pNal[3], pNal[4], len);

  H264_INIT_BITPARSER(bs, pNal, len);

  if((startHdrLen = xcode_h264_skipannexbhdr(&bs)) < 0) {
    LOG(X_ERROR("No H.264 AnnexB start header preceeding NAL"));
    return -1;
  }

  isvcl = (h264_getNALType(bs.buf[bs.byteIdx]) < NAL_TYPE_SEI) ? 1 : 0;

  if(isvcl) {
    pXh264->expectedPoc += pXh264->pocDelta;
  }

  //fprintf(stderr, "testh264_decodenalhdr 0x%x 0x%x 0x%x 0x%x 0x%x VCL:%d expected:%d starthdrlen:%d\n", pNal[0], pNal[1], pNal[2], pNal[3], pNal[4], isvcl, pXh264->expectedPoc, startHdrLen);

  //
  // Decode the NAL slice header
  //
  if((rch264 = h264_decode_NALHdr(&pXh264->h264, &bs, 0)) != H264_RESULT_DECODED) {
    LOG(X_ERROR("Failed to decode xcoder NAL 0x%x len:%d rc:%d"), pNal[4], len-4, rch264);
    //avc_dumpHex(stderr, bs.buf, (bs.sz - bs.byteIdx) > 32 ? 32 : bs.sz - bs.byteIdx, 1);
    rc = -1;
  }  else {

    //fprintf(stderr, "h264 poc:%d expected:%d dlta:%d nal len:%d nal:%d\n", pXh264->h264.picOrderCnt,pXh264->expectedPoc, pXh264->pocDelta, len, (pNal[0] & NAL_TYPE_MASK));

    if(isvcl) {
      // current x264 version is returning 3 byte start code 0x00 0x00 0x01 0x06 0x05...
      //for start of stream  1st frame is SEI, but its misinterpreted as I frame
      // so hence this 503 may be invalid since its not really an i frame ??

      // x264 seems to return 503 for the first I frame
      if(pXh264->h264.picOrderCnt >= 500) {
        LOG(X_WARNING("Ignoring Possibly Bogus H.264 slice POC %d"), pXh264->h264.picOrderCnt);
        pXh264->h264.picOrderCnt = 0;
      }

      if(pXh264->h264.picOrderCnt == 0) {
        pXh264->expectedPoc = pXh264->h264.picOrderCnt;
      }
    }

  }

  return rc;
}


int xcode_h264_skipannexbhdr(BIT_STREAM_T *bs) {

  //
  // Check for either 0x 00 00 01,  0x 00 00 00 01 annexB header start
  //
  if(*((uint16_t *) &bs->buf[bs->byteIdx]) == 0) {
    bs->byteIdx += 2;
    if(bs->buf[bs->byteIdx] == 0 && bs->buf[bs->byteIdx + 1] == 0x01) {
      bs->byteIdx += 2; 
      return 4;
    } else if(bs->buf[bs->byteIdx] == 0x01) {
      bs->byteIdx++;
      return 3; 
    }
  }

  return -1;
}

int xcode_h264_find_spspps(XCODE_H264_OUT_T *pH264out, const unsigned char *pData,
                           unsigned int len,  SPSPPS_RAW_T *pSpspps) {
  int rc = 0;
  BIT_STREAM_T bitstream;
  H264_STREAM_CHUNK_T bs;
  H264_STREAM_CHUNK_T bstmp;
  int startHdrLen = 0;

  if(!pData || len < 4) {
    return -1;
  }

  memset(&bs, 0, sizeof(bs));
  bs.bs.buf = (unsigned char *) pData;
  bs.bs.sz = len;

  //LOG(X_DEBUG("xcode_h264_find_spspps 0x%x 0x%x 0x%x 0x%x 0x%x, len:%d, byteIdx:%d,  startHdrLen%d"), pData[0], pData[1], pData[2], pData[3], pData[4], len, bs.bs.byteIdx, startHdrLen);

  if((startHdrLen = xcode_h264_skipannexbhdr(&bs.bs)) < 0) {
    return -1;
  }

  // 
  // Skip past any NAL AUD
  //
  if(bs.bs.buf[bs.bs.byteIdx] == NAL_TYPE_ACCESS_UNIT_DELIM) {
    bs.bs.byteIdx += 2;
    if((startHdrLen = xcode_h264_skipannexbhdr(&bs.bs)) < 0) {
      return -1;
    }
  }

  if(h264_getNALType(bs.bs.buf[bs.bs.byteIdx]) == NAL_TYPE_SEI) {

    //fprintf(stderr, "SEI byteIdx:%d,  startHdrLen%d\n", bs.bs.byteIdx, startHdrLen);
    memcpy(&bitstream, &bs.bs, sizeof(bitstream));
    h264_decodeSEI(&bitstream);

    if((rc = h264_findStartCode(&bs, 0, NULL)) >= H264_STARTCODE_LEN) {
      bs.bs.byteIdx += rc;
    }
    //fprintf(stderr, "SEI done byteIdx:%d,  startHdrLen%d\n", bs.bs.byteIdx, startHdrLen);
  }

  //LOG(X_DEBUG("checking for SPS... byteIdx:%d,  startHdrLen%d"), bs.bs.byteIdx, startHdrLen);
  if(h264_getNALType(bs.bs.buf[bs.bs.byteIdx]) == NAL_TYPE_SEQ_PARAM) {
    //LOG(X_DEBUG("SPS byteIdx:%d,  startHdrLen%d"), bs.bs.byteIdx, startHdrLen);
    bs.bs.byteIdx -= startHdrLen;
    memcpy(&bstmp, &bs, sizeof(bstmp));
    if(testh264_handlespspps(pH264out, &bs.bs, pSpspps) < 0) {
      LOG(X_ERROR("handlespspps failed for frame size %d/%d, sps_len:%d, pps_len:%d"), bs.bs.byteIdx, bs.bs.sz, pSpspps->sps_len, pSpspps->pps_len);
      LOGHEX_DEBUG(&bstmp.bs.buf[bstmp.bs.byteIdx], MIN(64, bstmp.bs.sz - bstmp.bs.byteIdx));
    }

    if((startHdrLen = xcode_h264_skipannexbhdr(&bs.bs)) < 0) {
      return -1;
    }

  }

  if(h264_getNALType(bs.bs.buf[bs.bs.byteIdx]) == NAL_TYPE_SEI) {

    memcpy(&bitstream, &bs.bs, sizeof(bitstream));
    h264_decodeSEI(&bitstream);

    if((rc = h264_findStartCode(&bs, 0, NULL)) >= H264_STARTCODE_LEN) {
      bs.bs.byteIdx += rc;
    }

  }

//fprintf(stderr, "ENDING AT BYTE[%d] 0x%x\n", bs.bs.byteIdx, bs.bs.buf[bs.bs.byteIdx]); avc_dumpHex(stderr, pData, MIN(len,96), 1);
  if(h264_getNALType(bs.bs.buf[bs.bs.byteIdx]) == NAL_TYPE_IDR) {
    return bs.bs.byteIdx;
  } else {

    //h264_decode_NALHdr(H264_DECODER_CTXT_T *pCtxt, BIT_STREAM_T *pNal, 0) {

    return 0;
  }

}


