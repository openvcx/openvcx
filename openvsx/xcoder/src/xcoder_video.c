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


#ifdef WIN32

#include <windows.h>
#include "unixcompat.h"

#else // WIN32

#include <unistd.h>

#endif // WIN32

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <pthread.h>
#include "logutil.h"
#include "ixcode.h"
#include "xcoder_libavcodec.h"
#include "xcoder.h"
#include "xcoder_filter.h"
#include "libswscale/swscale.h"
#include "vsxlib.h"

//#include "x264.h"
//#include "../../ffmpeg-0.10.2_mac/libswscale/swscale_internal.h"
// PIX_FMT_YUV420P = 0
// PIX_FMT_YUVA420P = 35

#if defined(XCODE_HAVE_X264)
#include "xcoder_x264.h"
#endif // (XCODE_HAVE_X264)

#if defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)
#include "xcoder_pip.h"
#endif // XCODE_HAVE_PIP

#if defined(XCODE_PROFILE_VID)
  struct timeval gtv[14];
#endif // (XCODE_PROFILE_VID)

#if defined(TESTME)
#define le2me_32(x) (x)

static const uint32_t crc_table[1][257] = {
    {
        0x00000000, 0xB71DC104, 0x6E3B8209, 0xD926430D, 0xDC760413, 0x6B6BC517,
        0xB24D861A, 0x0550471E, 0xB8ED0826, 0x0FF0C922, 0xD6D68A2F, 0x61CB4B2B,
        0x649B0C35, 0xD386CD31, 0x0AA08E3C, 0xBDBD4F38, 0x70DB114C, 0xC7C6D048,
        0x1EE09345, 0xA9FD5241, 0xACAD155F, 0x1BB0D45B, 0xC2969756, 0x758B5652,
        0xC836196A, 0x7F2BD86E, 0xA60D9B63, 0x11105A67, 0x14401D79, 0xA35DDC7D,
        0x7A7B9F70, 0xCD665E74, 0xE0B62398, 0x57ABE29C, 0x8E8DA191, 0x39906095,
        0x3CC0278B, 0x8BDDE68F, 0x52FBA582, 0xE5E66486, 0x585B2BBE, 0xEF46EABA,
        0x3660A9B7, 0x817D68B3, 0x842D2FAD, 0x3330EEA9, 0xEA16ADA4, 0x5D0B6CA0,
        0x906D32D4, 0x2770F3D0, 0xFE56B0DD, 0x494B71D9, 0x4C1B36C7, 0xFB06F7C3,
        0x2220B4CE, 0x953D75CA, 0x28803AF2, 0x9F9DFBF6, 0x46BBB8FB, 0xF1A679FF,
        0xF4F63EE1, 0x43EBFFE5, 0x9ACDBCE8, 0x2DD07DEC, 0x77708634, 0xC06D4730,
        0x194B043D, 0xAE56C539, 0xAB068227, 0x1C1B4323, 0xC53D002E, 0x7220C12A,
        0xCF9D8E12, 0x78804F16, 0xA1A60C1B, 0x16BBCD1F, 0x13EB8A01, 0xA4F64B05,
        0x7DD00808, 0xCACDC90C, 0x07AB9778, 0xB0B6567C, 0x69901571, 0xDE8DD475,
        0xDBDD936B, 0x6CC0526F, 0xB5E61162, 0x02FBD066, 0xBF469F5E, 0x085B5E5A,
        0xD17D1D57, 0x6660DC53, 0x63309B4D, 0xD42D5A49, 0x0D0B1944, 0xBA16D840,
        0x97C6A5AC, 0x20DB64A8, 0xF9FD27A5, 0x4EE0E6A1, 0x4BB0A1BF, 0xFCAD60BB,
        0x258B23B6, 0x9296E2B2, 0x2F2BAD8A, 0x98366C8E, 0x41102F83, 0xF60DEE87,
        0xF35DA999, 0x4440689D, 0x9D662B90, 0x2A7BEA94, 0xE71DB4E0, 0x500075E4,
        0x892636E9, 0x3E3BF7ED, 0x3B6BB0F3, 0x8C7671F7, 0x555032FA, 0xE24DF3FE,
        0x5FF0BCC6, 0xE8ED7DC2, 0x31CB3ECF, 0x86D6FFCB, 0x8386B8D5, 0x349B79D1,
        0xEDBD3ADC, 0x5AA0FBD8, 0xEEE00C69, 0x59FDCD6D, 0x80DB8E60, 0x37C64F64,
        0x3296087A, 0x858BC97E, 0x5CAD8A73, 0xEBB04B77, 0x560D044F, 0xE110C54B,
        0x38368646, 0x8F2B4742, 0x8A7B005C, 0x3D66C158, 0xE4408255, 0x535D4351,
        0x9E3B1D25, 0x2926DC21, 0xF0009F2C, 0x471D5E28, 0x424D1936, 0xF550D832,
        0x2C769B3F, 0x9B6B5A3B, 0x26D61503, 0x91CBD407, 0x48ED970A, 0xFFF0560E,
        0xFAA01110, 0x4DBDD014, 0x949B9319, 0x2386521D, 0x0E562FF1, 0xB94BEEF5,
        0x606DADF8, 0xD7706CFC, 0xD2202BE2, 0x653DEAE6, 0xBC1BA9EB, 0x0B0668EF,
        0xB6BB27D7, 0x01A6E6D3, 0xD880A5DE, 0x6F9D64DA, 0x6ACD23C4, 0xDDD0E2C0,
        0x04F6A1CD, 0xB3EB60C9, 0x7E8D3EBD, 0xC990FFB9, 0x10B6BCB4, 0xA7AB7DB0,
        0xA2FB3AAE, 0x15E6FBAA, 0xCCC0B8A7, 0x7BDD79A3, 0xC660369B, 0x717DF79F,
        0xA85BB492, 0x1F467596, 0x1A163288, 0xAD0BF38C, 0x742DB081, 0xC3307185,
        0x99908A5D, 0x2E8D4B59, 0xF7AB0854, 0x40B6C950, 0x45E68E4E, 0xF2FB4F4A,
        0x2BDD0C47, 0x9CC0CD43, 0x217D827B, 0x9660437F, 0x4F460072, 0xF85BC176,
        0xFD0B8668, 0x4A16476C, 0x93300461, 0x242DC565, 0xE94B9B11, 0x5E565A15,
        0x87701918, 0x306DD81C, 0x353D9F02, 0x82205E06, 0x5B061D0B, 0xEC1BDC0F,
        0x51A69337, 0xE6BB5233, 0x3F9D113E, 0x8880D03A, 0x8DD09724, 0x3ACD5620,
        0xE3EB152D, 0x54F6D429, 0x7926A9C5, 0xCE3B68C1, 0x171D2BCC, 0xA000EAC8,
        0xA550ADD6, 0x124D6CD2, 0xCB6B2FDF, 0x7C76EEDB, 0xC1CBA1E3, 0x76D660E7,
        0xAFF023EA, 0x18EDE2EE, 0x1DBDA5F0, 0xAAA064F4, 0x738627F9, 0xC49BE6FD,
        0x09FDB889, 0xBEE0798D, 0x67C63A80, 0xD0DBFB84, 0xD58BBC9A, 0x62967D9E,
        0xBBB03E93, 0x0CADFF97, 0xB110B0AF, 0x060D71AB, 0xDF2B32A6, 0x6836F3A2,
        0x6D66B4BC, 0xDA7B75B8, 0x035D36B5, 0xB440F7B1, 0x00000001
    }
};
static uint32_t crc_buf(const uint32_t *ctx,
                        uint32_t crc,
                        const uint8_t *pin,
                        size_t len) {

  const uint8_t *pend = pin + len;

  if(!ctx[256]) {
    while(pin < pend - 3){
      crc ^= le2me_32(*(const uint32_t*) pin);
      pin+=4;
      crc =  ctx[768 + (crc & 0xff)] ^ctx[512 + ((crc >> 8 ) &0xff)]
            ^ctx[256 + ((crc >> 16) & 0xff)] ^ctx[((crc >> 24) )];
    }
  }

  while(pin < pend) {
    crc = ctx[ ((uint8_t)crc) ^ *pin++ ] ^ (crc >> 8);
  }

  return crc;
}


uint32_t test_crc(const unsigned char *buf, unsigned int len) {
  return crc_buf(crc_table[0], -1, buf, len);
}

#endif // (TESTME)


extern void av_log_set_level(int);

pthread_mutex_t g_xcode_mtx = PTHREAD_MUTEX_INITIALIZER;

#if defined(__APPLE__)
// Needed when linking with libcommonutil.a or anything which references VSX_DEBUG_...
int g_debug_flags;
#endif // __APPLE__

#define OUT_DIMENSIONS(pXcode, av, pipframeidx)  ((pXcode)->pip.active ? &((av).dim_pips[pipframeidx]) : &((av).dim_enc)) 


#define FILTER_ENABLED(out) ((out).unsharp.common.active || \
                              (out).denoise.common.active || \
                              (out).color.common.active || \
                              (out).testFilter.common.active)


void xcode_fill_yuv420p(unsigned char *data[4], int linesize[4], unsigned char *p, unsigned int ht) {     

  //
  // yuv420p
  //

  data[0] = p;
  data[1] = data[0] + (linesize[0] * ht);
  data[2] = data[1] + (linesize[1] * ht/2);
  if(linesize[2] > 0 && linesize[3] > 0) {
    data[3] = data[2] + (linesize[2] * ht/2);
  } else {
    data[3] = NULL;
  }
}

void ixcode_init_common(IXCODE_COMMON_CTXT_T *pCommon) {
  pCommon->inpts90Khz = 0;
  pCommon->outpts90Khz = 0;
  pCommon->decodeInIdx = 0;
  pCommon->decodeOutIdx = 0;
  pCommon->encodeInIdx = 0;
  pCommon->encodeOutIdx = 0;
}

static int init_pip_dimensions(IXCODE_VIDEO_CTXT_T *pXcode, unsigned int scaleIdx, unsigned int frameidx) {
  unsigned int idx;
  int rc = 0;
  unsigned int pip_pic_sz_orig;
  IXCODE_AVCTXT_T *pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;

  pip_pic_sz_orig = pAvCtx->out[0].dim_pips[frameidx].pic_sz;

  for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {

    pAvCtx->out[idx].dim_pips[frameidx].width = PIP_PLACEMENT_WIDTH(pXcode->pip.pos[frameidx]);
    pAvCtx->out[idx].dim_pips[frameidx].height = PIP_PLACEMENT_HEIGHT(pXcode->pip.pos[frameidx]);
    pAvCtx->out[idx].dim_pips[frameidx].pix_fmt = PIX_FMT_YUVA420P;

    PIPXLOG("INIT_PIP_DIMENSIONS CALLING AVPICTURE_ALLOC pip_init dim_pips[%d] out[%d], scale[%d] %dx%d", frameidx, idx, scaleIdx,  pAvCtx->out[idx].dim_pips[frameidx].width, pAvCtx->out[idx].dim_pips[frameidx].height);

    pAvCtx->out[idx].dim_pips[frameidx].pic_sz = avpicture_get_size(pAvCtx->out[idx].dim_pips[frameidx].pix_fmt,
                                                     pAvCtx->out[idx].dim_pips[frameidx].width,
                                                     pAvCtx->out[idx].dim_pips[frameidx].height);

    if(pXcode->pip.doEncode && pAvCtx->out[idx].encWrap.u.v.fInit) {
      pAvCtx->out[idx].dim_enc.pix_fmt = PIX_FMT_YUV420P;
      pAvCtx->out[idx].dim_enc.pic_sz = avpicture_get_size(pAvCtx->out[idx].dim_enc.pix_fmt,
                                                           pAvCtx->out[idx].dim_enc.width,
                                                           pAvCtx->out[idx].dim_enc.height);
    }

#if (XCODE_FILTER_ON)
    // 
    // Force any filter buffers to reset since the output dimensions have changed 
    //
    if(FILTER_ENABLED(pXcode->out[idx]) && pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufFilter) {
      av_free(pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufFilter);
      pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufFilter = NULL;
    }
#endif // XCODE_FILTER_ON

  }

  pXcode->pip.pPrivData = pAvCtx;

  pthread_mutex_lock(&pXcode->overlay.mtx);

  if(!pAvCtx->pframepipmtx) {
    pAvCtx->pframepipmtx = (pthread_mutex_t *) calloc(1, sizeof(pthread_mutex_t));
    pthread_mutex_init(pAvCtx->pframepipmtx, NULL);
  }

  if(pAvCtx->piprawDataBufs[frameidx] && pip_pic_sz_orig != pAvCtx->out[0].dim_pips[frameidx].pic_sz) {
    av_free(pAvCtx->piprawDataBufs[frameidx]);
    pAvCtx->piprawDataBufs[frameidx] = NULL;
  }
  
  if(!pAvCtx->piprawDataBufs[frameidx] && !(pAvCtx->piprawDataBufs[frameidx] = av_malloc(pAvCtx->out[0].dim_pips[frameidx].pic_sz))) {
    rc = -1;
  }

  pthread_mutex_unlock(&pXcode->overlay.mtx);

  return rc;
}

static int check_dimensions(IXCODE_VIDEO_CTXT_T *pXcode, unsigned int idx, unsigned int scaleIdx, unsigned int pipframeidx) {
  IXCODE_AVCTXT_T *pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;
  IXCODE_VIDEO_CROP_T *pCrop = &pXcode->out[idx].crop;
  IXCODE_VIDEO_CTXT_T *pPipX = pXcode->overlay.p_pipsx[0];
  int forcescaler = 0;

  if(pXcode->overlay.havePip && pPipX && pPipX->pip.pMotion && 
    pPipX->pip.pMotion->active && pPipX->pip.pMotion->pmCur) {
    pCrop = &pPipX->pip.pMotion->pmCur->crop;
  }

  if(pXcode->pip.active && 
     (pAvCtx->out[idx].dim_pips[pipframeidx].width != PIP_PLACEMENT_WIDTH(pXcode->pip.pos[pipframeidx]) ||
     (pAvCtx->out[idx].dim_pips[pipframeidx].height != PIP_PLACEMENT_HEIGHT(pXcode->pip.pos[pipframeidx])))) {

    if(init_pip_dimensions(pXcode, scaleIdx, pipframeidx) < 0) {
      return -1;
    }
    //fprintf(stderr, "XCODE... tid;0x%x init_pip_dimensions done, forcescaler=1\n", pthread_self());
    forcescaler = 1; 
  } 

#if defined(XCODE_FILTER_ROTATE) && (XCODE_FILTER_ROTATE > 0)
  if(!forcescaler && !pAvCtx->out[idx].scale[scaleIdx].pswsctx && pXcode->out[idx].active && !pXcode->out[idx].passthru &&
     pXcode->out[idx].rotate.common.active && pXcode->out[idx].rotate.degrees != 0) {
    forcescaler = 1;
  }
#endif // XCODE_FILTER_ROTATE) && (XCODE_FILTER_ROTATE > 0)

  if(OUT_DIMENSIONS(pXcode, pAvCtx->out[idx], pipframeidx)->width > 0 &&
     OUT_DIMENSIONS(pXcode, pAvCtx->out[idx], pipframeidx)->height > 0 && 
    (pCrop->padTop + pCrop->padBottom > OUT_DIMENSIONS(pXcode, pAvCtx->out[idx], pipframeidx)->width ||
     pCrop->padLeft + pCrop->padRight > OUT_DIMENSIONS(pXcode, pAvCtx->out[idx], pipframeidx)->height)) {
    return -1;
  }

  if(!forcescaler && (pCrop->padTop != pAvCtx->out[idx].scale[scaleIdx].padTop || 
      pCrop->padBottom != pAvCtx->out[idx].scale[scaleIdx].padBottom ||
      pCrop->padLeft != pAvCtx->out[idx].scale[scaleIdx].padLeft || 
      pCrop->padRight != pAvCtx->out[idx].scale[scaleIdx].padRight ||
      pCrop->cropTop != pAvCtx->out[idx].scale[scaleIdx].cropTop || 
      pCrop->cropBottom != pAvCtx->out[idx].scale[scaleIdx].cropBottom ||
      pCrop->cropLeft !=pAvCtx->out[idx].scale[scaleIdx].cropLeft || 
      pCrop->cropRight != pAvCtx->out[idx].scale[scaleIdx].cropRight)) {

      //fprintf(stderr, "XCODE outidx[%d] INIT_DIMENSIONS pad:%d,%d,%d,%d crop:%d,%d,%d,%d\n", idx, pCrop->padTop, pCrop->padBottom, pCrop->padLeft, pCrop->padRight, pCrop->cropTop, pCrop->cropBottom, pCrop->cropLeft, pCrop->cropRight);

    //fprintf(stderr, "XCODE... tid:0x%x, crop/pad on, forcescaler=1\n", pthread_self());
    forcescaler = 1;
  }

  //
  // Reset the scaler context
  //
  if(forcescaler && pAvCtx->out[idx].scale[scaleIdx].pswsctx) {
    sws_freeContext(pAvCtx->out[idx].scale[scaleIdx].pswsctx);
    pAvCtx->out[idx].scale[scaleIdx].pswsctx = NULL;
    //fprintf(stderr, "XCODE... tid:0x%x, pswsctx set to NULL\n", pthread_self());
  }

  pAvCtx->out[idx].scale[scaleIdx].cropTop = pCrop->cropTop;
  pAvCtx->out[idx].scale[scaleIdx].cropBottom = pCrop->cropBottom;
  pAvCtx->out[idx].scale[scaleIdx].cropLeft = pCrop->cropLeft;
  pAvCtx->out[idx].scale[scaleIdx].cropRight = pCrop->cropRight;

  pAvCtx->out[idx].scale[scaleIdx].padTop = pCrop->padTop;
  pAvCtx->out[idx].scale[scaleIdx].padBottom = pCrop->padBottom;
  pAvCtx->out[idx].scale[scaleIdx].padLeft = pCrop->padLeft;
  pAvCtx->out[idx].scale[scaleIdx].padRight = pCrop->padRight;
  memcpy(pAvCtx->out[idx].scale[scaleIdx].padRGB, pCrop->padRGB, 4);

  //fprintf(stderr, "XCODE PAD l/r: %dx%d top/bot: %dx%d output:%dx%d\n", pAvCtx->out[idx].scale[scaleIdx].padLeft, pAvCtx->out[idx].scale[scaleIdx].padRight, pAvCtx->out[idx].scale[scaleIdx].padTop, pAvCtx->out[idx].scale[scaleIdx].padBottom, pAvCtx->out[idx].enc_width, pAvCtx->out[idx].enc_height);

  if(!pXcode->pip.active &&
     (pXcode->overlay.havePip || pXcode->usewatermark ||
     (pAvCtx->out[idx].scale[scaleIdx].padTop > 0 || pAvCtx->out[idx].scale[scaleIdx].padBottom > 0 ||
      pAvCtx->out[idx].scale[scaleIdx].padLeft > 0 || pAvCtx->out[idx].scale[scaleIdx].padRight > 0 ||
      pAvCtx->out[idx].scale[scaleIdx].cropTop > 0 || pAvCtx->out[idx].scale[scaleIdx].cropBottom > 0 ||
      pAvCtx->out[idx].scale[scaleIdx].cropLeft > 0 || pAvCtx->out[idx].scale[scaleIdx].cropRight > 0))) {

      // should be set if linesize[0] != width
    pAvCtx->out[idx].scale[scaleIdx].forcescaler = 1;
  } else {
    pAvCtx->out[idx].scale[scaleIdx].forcescaler = forcescaler;
  }
  //fprintf(stderr, "XCODE... tid:0x%x, done forcescaler[%d]:%d\n", pthread_self(), idx, pAvCtx->out[idx].forcescaler);

  return pAvCtx->out[idx].scale[scaleIdx].forcescaler;
}

int ixcode_init_vid(IXCODE_VIDEO_CTXT_T *pXcode) {

  IXCODE_AVCTXT_T *pAvCtx = NULL;
  CODEC_WRAP_VIDEO_ENCODER_T enc;
  CODEC_WRAP_VIDEO_DECODER_T dec;
  unsigned int idx;
  int rc = 0;

  if(!pXcode) {
    return -1;
  }

#if defined(__APPLE__)
  //TODO: logger global (extern or static) is multiply defined 
  // in multiple .so
  if(pXcode->common.pLogCtxt) {
    logger_setContext(pXcode->common.pLogCtxt);
  } else if(pXcode->common.cfgVerbosity > 1) {
    logger_SetLevel(S_DEBUG);
    logger_AddStderr(S_DEBUG, 0);
  } else {
    logger_SetLevel(S_INFO);
    logger_AddStderr(S_INFO, 0);
  }
#endif // __APPLE__

  memset(&enc, 0, sizeof(enc));
  enc.fInit = xcoder_libavcodec_video_init_encoder;
  enc.fClose = xcoder_libavcodec_video_close_encoder;
  enc.fEncode = xcoder_libavcodec_video_encode;
  enc.ctxt.pXcode = pXcode;

  memset(&dec, 0, sizeof(dec));
  dec.fInit = xcoder_libavcodec_video_init_decoder;
  dec.fClose = xcoder_libavcodec_video_close_decoder;
  dec.fDecode = xcoder_libavcodec_video_decode;
  dec.ctxt.pXcode = pXcode;

  //
  // Override any default decoder function mappings
  //
  switch(pXcode->common.cfgFileTypeIn) {
    case XC_CODEC_TYPE_H262:
      break;
    case XC_CODEC_TYPE_MPEG4V:
      break;
    case XC_CODEC_TYPE_H263:
    case XC_CODEC_TYPE_H263_PLUS:
      break;
    case XC_CODEC_TYPE_H264:
      break;

#if defined(XCODE_HAVE_VP6)
    case XC_CODEC_TYPE_VP6:
      break;
#endif // XCODE_HAVE_VP6

#if defined(XCODE_HAVE_VP6F)
    case XC_CODEC_TYPE_VP6F:
      break;
#endif // XCODE_HAVE_VP6F

#if defined(XCODE_HAVE_VP8)
    case XC_CODEC_TYPE_VP8:
      break;
#endif // XCODE_HAVE_VP8

#if defined(XCODE_HAVE_PNG)
    case XC_CODEC_TYPE_PNG:
      break;
#endif  // XCODE_HAVE_PNG

    case XC_CODEC_TYPE_BMP:
      break;
    case XC_CODEC_TYPE_RAWV_BGRA32:
    case XC_CODEC_TYPE_RAWV_RGBA32:
    case XC_CODEC_TYPE_RAWV_BGR24:
    case XC_CODEC_TYPE_RAWV_RGB24:
    case XC_CODEC_TYPE_RAWV_BGR565:
    case XC_CODEC_TYPE_RAWV_RGB565:
    case XC_CODEC_TYPE_RAWV_YUV420P:
    case XC_CODEC_TYPE_RAWV_YUVA420P:
    case XC_CODEC_TYPE_RAWV_NV21:
    case XC_CODEC_TYPE_RAWV_NV12:
    case XC_CODEC_TYPE_VID_CONFERENCE:

      dec.fDecode = NULL;
      break;
    default:
      LOG(X_ERROR("Unsupported xcoder input video codec %d"), pXcode->common.cfgFileTypeIn);
      return -1;
  }

  //
  // Override any default encoder function mappings
  //
  switch(pXcode->common.cfgFileTypeOut) {

#if defined(XCODE_HAVE_X264)

    case XC_CODEC_TYPE_H264:

#if !defined(XCODE_HAVE_X264_AVCODEC) || (XCODE_HAVE_X264_AVCODEC == 0)

        //
        // Access libx264 directly without going through libavcodec
        //
        enc.fInit = xcoder_x264_init;
        enc.fClose = xcoder_x264_close;
        enc.fEncode = xcoder_x264_encode;

#endif // (XCODE_HAVE_X264_AVCODEC) || (XCODE_HAVE_X264_AVCODEC == 0)

      break;

#endif // XCODE_HAVE_X264

    case XC_CODEC_TYPE_MPEG4V:
      break;
    case XC_CODEC_TYPE_H263:
      break;
    case XC_CODEC_TYPE_H263_PLUS:
      break;

#if defined(XCODE_HAVE_VP8)
    case XC_CODEC_TYPE_VP8:
      break;
#endif // XCODE_HAVE_VP8

    case XC_CODEC_TYPE_RAWV_YUVA420P:
    case XC_CODEC_TYPE_RAWV_YUV420P:
    case XC_CODEC_TYPE_RAWV_BGRA32:
    case XC_CODEC_TYPE_RAWV_RGBA32:
    case XC_CODEC_TYPE_RAWV_BGR24:
    case XC_CODEC_TYPE_RAWV_RGB24:
    case XC_CODEC_TYPE_RAWV_BGR565:
    case XC_CODEC_TYPE_RAWV_RGB565:
      enc.fEncode = NULL;
      break;
    default:
      LOG(X_ERROR("Unsupported xcoder output video codec %d"), pXcode->common.cfgFileTypeOut);
      return -1;
  }

  pthread_mutex_lock(&g_xcode_mtx);

  //
  // Perform any encoder update
  //
  if((pAvCtx = pXcode->common.pPrivData)) {
    for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
      if(rc == 0 && pAvCtx->out[idx].encWrap.u.v.fInit) {
        LOG(X_INFO("Updating video encoder[%d] configuration"), idx);
        if(pAvCtx->out[idx].encWrap.u.v.fInit(&pAvCtx->out[idx].encWrap.u.v.ctxt, idx) < 0) {
          LOG(X_ERROR("Failed to update video encoder[%d]"), idx);
          rc = -1;
          break;
        }  
      } 
    }
    pthread_mutex_unlock(&g_xcode_mtx);
    return rc;
  }

  //fprintf(stderr, "XCODE_INIT tid:0x%x, pip.active:%d, overlay.havePip:%d, cfgFileTypeOut:%d\n", pthread_self(), pXcode->pip.active, pXcode->overlay.havePip, pXcode->common.cfgFileTypeOut);

  ixcode_init_common(&pXcode->common);

  pAvCtx = pXcode->common.pPrivData = calloc(1, sizeof(IXCODE_AVCTXT_T));

  if(pXcode->pip.active && !pXcode->pip.doEncode) {
    //enc.fClose = NULL;
    enc.fEncode = NULL;
    enc.fInit = NULL;
  } 

  if(enc.fEncode && (pXcode->resOutClockHz == 0 || pXcode->resOutFrameDeltaHz == 0)) {
    LOG(X_ERROR("Video transcoding output frame rate not set"));
    return -1;
  }

  enc.ctxt.pAVCtxt = pAvCtx;
  dec.ctxt.pAVCtxt = pAvCtx;
  memcpy(&pAvCtx->decWrap.u.v, &dec, sizeof(CODEC_WRAP_VIDEO_DECODER_T));
  for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
    memcpy(&pAvCtx->out[idx].encWrap.u.v, &enc, sizeof(CODEC_WRAP_VIDEO_ENCODER_T));
  }

  //
  // Disable any output index encoder(s)
  //
  for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {

    if(rc != 0 || !pXcode->out[idx].active || pXcode->out[idx].passthru) {

      if((idx == 0 && !pXcode->out[idx].active) || 
         (idx == 1 && pXcode->out[idx].active && !pXcode->out[idx-1].passthru)) {
        rc = -1;
        break;
      }
      pAvCtx->out[idx].encWrap.u.v.fInit = NULL;
      pAvCtx->out[idx].encWrap.u.v.fEncode = NULL;
      continue;
    }

    //
    // Set any encoder output resolution here before calling fInit
    //
    if(pXcode->pip.active && pXcode->pip.doEncode) {
      if(pXcode->out[idx].cfgOutPipH > 0 && pXcode->out[idx].cfgOutPipV > 0) {
        pAvCtx->out[idx].dim_enc.width = pXcode->out[idx].cfgOutPipH;
        pAvCtx->out[idx].dim_enc.height = pXcode->out[idx].cfgOutPipV;
      } else {
        pAvCtx->out[idx].dim_enc.width = pXcode->pip.pXOverlay->out[idx].resOutH;
        pAvCtx->out[idx].dim_enc.height = pXcode->pip.pXOverlay->out[idx].resOutV;
      }
    } else {
      pAvCtx->out[idx].dim_enc.width = pXcode->out[idx].resOutH;
      pAvCtx->out[idx].dim_enc.height = pXcode->out[idx].resOutV;
    }
    //LOG(X_DEBUG("DIM_ENC[%d].%dx%d (cfgOutPip:%dx%d), (resOut:%dx%d)"), idx, pAvCtx->out[idx].dim_enc.width, pAvCtx->out[idx].dim_enc.height, pXcode->out[idx].cfgOutPipH, pXcode->out[idx].cfgOutPipV, pXcode->out[idx].resOutH, pXcode->out[idx].resOutV);

  } // end of for


  //
  // Init and open the decoder
  //
  if(rc == 0 && dec.fInit && dec.fInit(&pAvCtx->decWrap.u.v.ctxt, 0) < 0) {
    LOG(X_ERROR("Failed to open video decoder"));
    rc = -1;
  }

  //
  // Init and open the encoder 
  //
  for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {

    //fprintf(stderr, "xcode[%d] active:%d, passthru:%d, fInit:0x%x\n", idx, pXcode->out[idx].active, pXcode->out[idx].passthru, pAvCtx->out[idx].encWrap.u.v.fInit);

    if(rc == 0 && pAvCtx->out[idx].encWrap.u.v.fInit && 
                  pAvCtx->out[idx].encWrap.u.v.fInit(&pAvCtx->out[idx].encWrap.u.v.ctxt, idx) < 0) {
      LOG(X_ERROR("Failed to open video encoder[%d]"), idx);
      rc = -1;
    }

    pXcode->out[idx].qTot = 0;
    pXcode->out[idx].qSamples = 0;

  }

  //TODO: set resOutH, resOutV, pix_fmt, if this is a pip and


  //
  // Initialize the scaler
  //
  if(rc == 0) {

    for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {

      if(pXcode->out[idx].cfgScaler > 0) {
 
        // scalers from fastest to slowest

        if(pXcode->out[idx].cfgScaler == 1) {
          pAvCtx->out[idx].scale[0].scalertype = SWS_FAST_BILINEAR; 
        } else if(pXcode->out[idx].cfgScaler == 2) {
          pAvCtx->out[idx].scale[0].scalertype = SWS_BILINEAR;
        } else if(pXcode->out[idx].cfgScaler == 4) {
          pAvCtx->out[idx].scale[0].scalertype = SWS_GAUSS;
        } else if(pXcode->out[idx].cfgScaler == 5) {
          pAvCtx->out[idx].scale[0].scalertype = SWS_SINC;
        } else { 
          // cfgScaler == 3
          pAvCtx->out[idx].scale[0].scalertype = SWS_BICUBIC;
        }
      } else if(pXcode->out[idx].cfgFast >= ENCODER_QUALITY_MED) {
          pAvCtx->out[idx].scale[0].scalertype = SWS_FAST_BILINEAR; 
      } else if(pXcode->out[idx].cfgFast >= ENCODER_QUALITY_HIGH) {
         pAvCtx->out[idx].scale[0].scalertype = SWS_BILINEAR; 
      } else {
        pAvCtx->out[idx].scale[0].scalertype = SWS_BICUBIC;
      } 
      pAvCtx->out[idx].scale[1].scalertype = pAvCtx->out[idx].scale[0].scalertype;
      pAvCtx->out[idx].scale[2].scalertype = pAvCtx->out[idx].scale[0].scalertype;

    } // end of for
  }

  if(rc == 0) {

    //
    // If this instance is a PIP, Perform any PIP specific initialization 
    //
    if(pXcode->pip.active) {

      init_pip_dimensions(pXcode, 0, 0);

      LOG(X_DEBUG("Initialized PIP %d x %d at x:%d, y:%d"), 
           pAvCtx->out[0].dim_pips[0].width, pAvCtx->out[0].dim_pips[0].height, pXcode->pip.pos[0].posx, pXcode->pip.pos[0].posy);

    }

  }

  pthread_mutex_unlock(&g_xcode_mtx);

  if(rc != 0) {
    LOG(X_ERROR("Failed to init transcode video context"));
    ixcode_close_vid(pXcode);
  }

  return rc;
}

void ixcode_close_vid(IXCODE_VIDEO_CTXT_T *pXcodeV) {
  IXCODE_AVCTXT_T *pAvCtx = NULL;
  unsigned int idx, scaleIdx;

  if(!pXcodeV) {
    return;
  }

  pthread_mutex_lock(&g_xcode_mtx);

  if((pAvCtx = pXcodeV->common.pPrivData)) {

    if(pAvCtx->decWrap.u.v.fClose) {
      pAvCtx->decWrap.u.v.fClose(&pAvCtx->decWrap.u.v.ctxt);
    }

    for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {
      if(pAvCtx->out[idx].encWrap.u.v.fClose) {
        pAvCtx->out[idx].encWrap.u.v.fClose(&pAvCtx->out[idx].encWrap.u.v.ctxt);
      }
    }

    for(idx = 0; idx < IXCODE_VIDEO_OUT_MAX; idx++) {

      for(scaleIdx = 0; scaleIdx < SCALER_COUNT_MAX; scaleIdx++) {
        if(pAvCtx->out[idx].scale[scaleIdx].pswsctx) {
          sws_freeContext(pAvCtx->out[idx].scale[scaleIdx].pswsctx);
          pAvCtx->out[idx].scale[scaleIdx].pswsctx = NULL;
        }
        pAvCtx->out[idx].scale[scaleIdx].forcescaler = 0; 

        if(pAvCtx->out[idx].scale[scaleIdx].pframeScaled) {
          avpicture_free((AVPicture *)pAvCtx->out[idx].scale[scaleIdx].pframeScaled);
          av_free(pAvCtx->out[idx].scale[scaleIdx].pframeScaled);
          pAvCtx->out[idx].scale[scaleIdx].pframeScaled = NULL;
        }

        if(pAvCtx->out[idx].scale[scaleIdx].pframeRotate) {
          avpicture_free((AVPicture *)pAvCtx->out[idx].scale[scaleIdx].pframeRotate);
          av_free(pAvCtx->out[idx].scale[scaleIdx].pframeRotate);
          pAvCtx->out[idx].scale[scaleIdx].pframeRotate = NULL;
        }

        if(pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufPad) {
          av_free(pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufPad);
          pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufPad = NULL;
        }

        if(pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufCrop) {
          av_free(pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufCrop);
          pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufCrop = NULL;
        }

        if(pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufFilter) {
          av_free(pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufFilter);
          pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufFilter = NULL;
        }

      }

      if(pAvCtx->out[idx].pPipBufEncoderIn) {
        av_free(pAvCtx->out[idx].pPipBufEncoderIn);
        pAvCtx->out[idx].pPipBufEncoderIn = NULL;
      }

#if defined(XCODE_FILTER_COLOR) && (XCODE_FILTER_COLOR > 0)
      if(pAvCtx->out[idx].pFilterColorCtx) {
        filter_color_free(pAvCtx->out[idx].pFilterColorCtx);
        pAvCtx->out[idx].pFilterColorCtx = NULL;
      }
#endif // (XCODE_FILTER_COLOR) && (XCODE_FILTER_COLOR > 0)

#if defined(XCODE_FILTER_UNSHARP) && (XCODE_FILTER_UNSHARP > 0)
      if(pAvCtx->out[idx].pFilterUnsharpCtx) {
        filter_unsharp_free(pAvCtx->out[idx].pFilterUnsharpCtx);
        pAvCtx->out[idx].pFilterUnsharpCtx = NULL;
      }
#endif // (XCODE_FILTER_UNSHARP) && (XCODE_FILTER_UNSHARP > 0)

#if defined(XCODE_FILTER_DENOISE) && (XCODE_FILTER_DENOISE > 0)
      if(pAvCtx->out[idx].pFilterDenoiseCtx) {
        filter_denoise_free(pAvCtx->out[idx].pFilterDenoiseCtx);
        pAvCtx->out[idx].pFilterDenoiseCtx = NULL;
      }
#endif //(XCODE_FILTER_DENOISE) && (XCODE_FILTER_DENOISE > 0)

#if defined(XCODE_FILTER_ROTATE) && (XCODE_FILTER_ROTATE > 0)
      if(pAvCtx->out[idx].pFilterRotateCtx) {
        filter_rotate_free(pAvCtx->out[idx].pFilterRotateCtx);
        pAvCtx->out[idx].pFilterRotateCtx = NULL;
      }
#endif // XCODE_FILTER_ROTATE

#if defined(XCODE_FILTER_TEST) && (XCODE_FILTER_TEST > 0)
      if(pAvCtx->out[idx].pFilterTestCtx) {
        filter_test_free(pAvCtx->out[idx].pFilterTestCtx);
        pAvCtx->out[idx].pFilterTestCtx = NULL;
      }
#endif // (XCODE_FILTER_TEST && (XCODE_FILTER_TEST > 0)

    } //end of for

    for(idx = 0; idx < PIP_ADD_MAX; idx++) {
      if(pAvCtx->piprawDataBufs[idx]) {
        av_free(pAvCtx->piprawDataBufs[idx]);
        pAvCtx->piprawDataBufs[idx] = NULL;
      }
    }

    if(pAvCtx->pframepipmtx) {
      pthread_mutex_lock(&pXcodeV->overlay.mtx);

      pthread_mutex_destroy(pAvCtx->pframepipmtx);
      free(pAvCtx->pframepipmtx);
      pAvCtx->pframepipmtx = NULL;

      pthread_mutex_unlock(&pXcodeV->overlay.mtx);
    }
    pAvCtx->piphavepos0 = 0;

    //if(pXcodeV->pip.rawDataBuf) {
    //  av_free(pXcodeV->pip.rawDataBuf);
    //  pXcodeV->pip.rawDataBuf = NULL;
    //}

    free(pAvCtx);
    pXcodeV->common.pPrivData = NULL;

  }

  pthread_mutex_unlock(&g_xcode_mtx);

}

static int check_overlay(const IXCODE_VIDEO_CTXT_T *pXcode, int flag) {
  unsigned int idx;

  if(pXcode->overlay.havePip) {
    for(idx = 0; idx < MAX_PIPS; idx++) {
    //for(idx = 0; idx < (sizeof(pXcode->overlay.p_pipsx) /
    //                    sizeof(pXcode->overlay.p_pipsx[0])); idx++) {
      if(pXcode->overlay.p_pipsx[idx] && (pXcode->overlay.p_pipsx[idx]->pip.flags & flag)) {
        return 1;
      }
    }
  }

  return 0;
}

#if 0
static int dumpyuv(const AVFrame *pframe, int w, int h) {
  int fd;
  static int g_fdidx;
  int line;
  char buf[64];

  sprintf(buf, "tmpout/frame%d.yuv", g_fdidx++);

  if((fd = open(buf, O_CREAT | O_RDWR, 0666)) < 0) {
    return -1;
  }

  fprintf(stderr, "DUMPYUV %dx%d linesize:%d,%d,%d,%d  0x%x, 0x%x, 0x%x, 0x%x (%d,%d,%d) \n", w, h, pframe->linesize[0], pframe->linesize[1], pframe->linesize[2], pframe->linesize[3], pframe->data[0], pframe->data[1], pframe->data[2], pframe->data[3], pframe->data[1] - pframe->data[0], pframe->data[2] - pframe->data[1], pframe->data[3] - pframe->data[2]);
 //fprintf(stderr, "test:%d\n", (h+ (1 << 1) - 1) >> 1);

  for(line = 0; line < h; line++) {
    write(fd, pframe->data[0] + (line * pframe->linesize[0]), w);
  }
  for(line = 0; line < h/2; line++) {
    write(fd, pframe->data[1] + (line * pframe->linesize[1]), w/2);
  }
  for(line = 0; line < h/2; line++) {
    write(fd, pframe->data[2] + (line * pframe->linesize[2]), w/2);
  }

  close(fd);

  return 0;
}
#endif // 0

static void swap_fill(IXCODE_AVCTXT_OUT_T *pOutCtx, AVFrame *pframeout, 
                      const int linesizes[3], unsigned char **ppScaleDataBuf, int pix_fmt, unsigned int scaleIdx) {
  unsigned char *p;

  p = pframeout->data[0];
  pframeout->data[0] = pOutCtx->scale[scaleIdx].pframeScaled->data[0] = *ppScaleDataBuf;
  *ppScaleDataBuf = p;

  pframeout->linesize[0] = linesizes[0];
  pframeout->linesize[1] = linesizes[1];
  pframeout->linesize[2] = linesizes[2];

  pframeout->data[1] = pframeout->data[0] + (pframeout->linesize[0] * pOutCtx->dim_enc.height);
  pframeout->data[2] = pframeout->data[1] + (pframeout->linesize[1] * pOutCtx->dim_enc.height/2);

  pOutCtx->scale[scaleIdx].pframeScaled->data[1] = pOutCtx->scale[scaleIdx].pframeScaled->data[0] +
                  (pOutCtx->scale[scaleIdx].pframeScaled->linesize[0] * pOutCtx->dim_enc.height);
  pOutCtx->scale[scaleIdx].pframeScaled->data[2] = pOutCtx->scale[scaleIdx].pframeScaled->data[1] +
                  (pOutCtx->scale[scaleIdx].pframeScaled->linesize[1] * pOutCtx->dim_enc.height/2);

  //avpicture_fill((AVPicture *) pframeout, pframeout->data[0], pix_fmt,
  //               pOutCtx->enc_width, pOutCtx->enc_height);

  //avpicture_fill((AVPicture *) pOutCtx->pframeScaled, pOutCtx->pframeScaled->data[0], pix_fmt,
  //               pOutCtx->enc_width, pOutCtx->enc_height);

}


#if (XCODE_FILTER_ON)

static int runfilter(const FILTER_COMMON_T *pFilter, 
                     IXCODE_AVCTXT_OUT_T *pOutCtx, 
                     IXCODE_VIDEO_OUT_T *pOutXcode,
                     void **ppFilterCtx, 
                     FUNC_FILTER_INIT filterInit, 
                     FUNC_FILTER_EXEC filterExec,
                     AVFrame *pinFrame,
                     unsigned int scaleIdx) {

  AVFrame outFrame;
  AVPicture pic;
  int sz;

  if(!pFilter->active) {
    return 0;
  }

  if(!pOutCtx->scale[scaleIdx].pScaleDataBufFilter &&
     (sz = avpicture_fill(&pic, NULL, pOutCtx->dim_enc.pix_fmt,
                            pOutCtx->dim_enc.width,
                            pOutCtx->dim_enc.height)) > 0) {
    pOutCtx->scale[scaleIdx].pScaleDataBufFilter = (unsigned char *) av_malloc(sz);
  }

  if(!pOutCtx->scale[scaleIdx].pScaleDataBufFilter) {
    return IXCODE_RC_ERROR;
  }

  if(!(*ppFilterCtx)) {
    if(!(*ppFilterCtx = filterInit(pOutCtx->dim_enc.width, (void *) pFilter))) {
      return IXCODE_RC_ERROR;
    }
  }


  outFrame = *pinFrame;
  xcode_fill_yuv420p(outFrame.data, outFrame.linesize, pOutCtx->scale[scaleIdx].pScaleDataBufFilter, 
                     pOutCtx->dim_enc.height);

  //fprintf(stderr, "doing filter out[%d]\n", outidx);
  filterExec(*ppFilterCtx, (const unsigned char **) pinFrame->data,  
             pinFrame->linesize, outFrame.data, outFrame.linesize,
            pOutCtx->dim_enc.width, pOutCtx->dim_enc.height);

  swap_fill(pOutCtx, pinFrame, outFrame.linesize, &pOutCtx->scale[scaleIdx].pScaleDataBufFilter, 
            pOutCtx->dim_enc.pix_fmt, scaleIdx);

  return 0;
}

#if defined(XCODE_FILTER_ROTATE) && (XCODE_FILTER_ROTATE > 0)
static int rotateFilter(IXCODE_VIDEO_CTXT_T *pXcode, unsigned int idx, AVFrame *pframeout, 
                        unsigned int scaleIdx) {
  int rc = 0;  
  IXCODE_AVCTXT_T *pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;

#if defined(XCODE_PROFILE_VID)
  gettimeofday(&gtv[12], NULL);
#endif // XCODE_PROFILE_VID

  if(!pAvCtx->out[idx].pFilterRotateCtx &&
     !(pAvCtx->out[idx].pFilterRotateCtx = filter_rotate_init(pAvCtx->out[idx].dim_enc.width, 
                                                           &pXcode->out[idx].rotate.common))) {
    rc = IXCODE_RC_ERROR;
  }

  if(rc == 0 && (rc = filter_rotate(pAvCtx->out[idx].pFilterRotateCtx,
                 (const unsigned char **) pAvCtx->out[idx].scale[scaleIdx].pframeRotate->data,
                 pAvCtx->out[idx].scale[scaleIdx].pframeRotate->linesize,
                 pframeout->data, 
                 pframeout->linesize, 
                 pAvCtx->out[idx].dim_enc.width - pAvCtx->out[idx].scale[scaleIdx].padLeft - 
                                                  pAvCtx->out[idx].scale[scaleIdx].padRight,
                 pAvCtx->out[idx].dim_enc.height - pAvCtx->out[idx].scale[scaleIdx].padTop - 
                                                   pAvCtx->out[idx].scale[scaleIdx].padBottom)) < 0) {
    LOG(X_ERROR("Image rotation failed"));
  }

#if defined(XCODE_PROFILE_VID)
  gettimeofday(&gtv[13], NULL);
  pAvCtx->prof.num_vrotate++;
#endif // XCODE_PROFILE_VID

  return rc;
}
#endif // (XCODE_FILTER_ROTATE) && (XCODE_FILTER_ROTATE > 0)

static void runfilters(IXCODE_VIDEO_CTXT_T *pXcode, AVFrame *pframesout[IXCODE_VIDEO_OUT_MAX], 
                       unsigned int scaleIdx) {
  unsigned int outidx;
  //int sz;
  //AVPicture pic;
  IXCODE_AVCTXT_T *pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;

#if defined(XCODE_PROFILE_VID)
      gettimeofday(&gtv[10], NULL); 
#endif // XCODE_PROFILE_VID


      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        if(!pXcode->out[outidx].active || pXcode->out[outidx].passthru || 
           !FILTER_ENABLED(pXcode->out[outidx])) {
          continue;
        }

#if defined(XCODE_FILTER_COLOR) && (XCODE_FILTER_COLOR > 0)

        if(pXcode->out[outidx].color.common.active) {
          runfilter(&pXcode->out[outidx].color.common, 
                    &pAvCtx->out[outidx],
                    &pXcode->out[outidx],
                    &pAvCtx->out[outidx].pFilterColorCtx,
                    filter_color_init,
                    filter_color,
                    pframesout[outidx],
                    scaleIdx);
        }

#endif // (XCODE_FILTER_COLOR) && (XCODE_FILTER_COLOR > 0)

#if defined(XCODE_FILTER_UNSHARP) && (XCODE_FILTER_UNSHARP > 0)

        if(pXcode->out[outidx].unsharp.common.active) {
          runfilter(&pXcode->out[outidx].unsharp.common, 
                    &pAvCtx->out[outidx],
                    &pXcode->out[outidx],
                    &pAvCtx->out[outidx].pFilterUnsharpCtx,
                    filter_unsharp_init,
                    filter_unsharp,
                    pframesout[outidx],
                    scaleIdx);
        }

#endif // (XCODE_FILTER_UNSHARP) && (XCODE_FILTER_UNSHARP > 0)

#if defined(XCODE_FILTER_DENOISE) && (XCODE_FILTER_DENOISE > 0)

        if(pXcode->out[outidx].denoise.common.active) {
          runfilter(&pXcode->out[outidx].denoise.common,
                    &pAvCtx->out[outidx],
                    &pXcode->out[outidx],
                    &pAvCtx->out[outidx].pFilterDenoiseCtx,
                    filter_denoise_init,
                    filter_denoise,
                    pframesout[outidx],
                    scaleIdx);
        }

#endif // (XCODE_FILTER_DENOISE) && (XCODE_FILTER_DENOISE > 0)

#if defined(XCODE_FILTER_TEST) && (XCODE_FILTER_TEST > 0)

        if(pXcode->out[outidx].testFilter.common.active) {
          runfilter(&pXcode->out[outidx].testFilter.common,
                    &pAvCtx->out[outidx],
                    &pXcode->out[outidx],
                    &pAvCtx->out[outidx].pFilterTestCtx,
                    filter_test_init,
                    filter_test,
                    pframesout[outidx],
                    scaleIdx);
        }

#endif // (XCODE_FILTER_TEST) && (XCODE_FILTER_TEST > 0)

      }

//fprintf(stderr, "encoder in: 0x%x, filterbuf:0x%x\n", pframesout[0]->data[0], pAvCtx->out[0].pScaleDataBufFilter);

#if defined(XCODE_PROFILE_VID)
      gettimeofday(&gtv[11], NULL); 
      pAvCtx->prof.num_vfilters++;
#endif // XCODE_PROFILE_VID

}
#endif // XCODE_FILTER_ON


static enum IXCODE_RC init_scaler(IXCODE_VIDEO_CTXT_T *pXcode, 
                                  unsigned int idx, 
                                  unsigned int scaleIdx,
                                  const VID_DIMENSIONS_T *pDimensionsOut, 
                                  const VID_DIMENSIONS_T *pDimensionsIn) {
  int sz = 0;
  int width, height;
  AVPicture pic;
  
  IXCODE_AVCTXT_T *pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;

  //fprintf(stderr, "XCODDE outidx[%d] INIT_SCALER pAvCtx:0x%x\n", idx, pAvCtx);

  if(pAvCtx->out[idx].scale[scaleIdx].pswsctx) {
    sws_freeContext(pAvCtx->out[idx].scale[scaleIdx].pswsctx);
    pAvCtx->out[idx].scale[scaleIdx].pswsctx = NULL;
  }

  if(pXcode->out[idx].active && !pXcode->out[idx].passthru &&
     pXcode->out[idx].rotate.common.active) {

    if(IXCODE_FILTER_ROTATE_90(pXcode->out[idx])) {
      width = pDimensionsOut->height - pAvCtx->out[idx].scale[scaleIdx].padTop - 
                                       pAvCtx->out[idx].scale[scaleIdx].padBottom;
      height = pDimensionsOut->width - pAvCtx->out[idx].scale[scaleIdx].padLeft - 
                                       pAvCtx->out[idx].scale[scaleIdx].padRight;
    } else {
      width = pDimensionsOut->width - pAvCtx->out[idx].scale[scaleIdx].padLeft - 
                                      pAvCtx->out[idx].scale[scaleIdx].padRight;
      height = pDimensionsOut->height - pAvCtx->out[idx].scale[scaleIdx].padTop - 
                                        pAvCtx->out[idx].scale[scaleIdx].padBottom;
    }

    if(pAvCtx->out[idx].scale[scaleIdx].pframeRotate) {
      avpicture_free((AVPicture *)pAvCtx->out[idx].scale[scaleIdx].pframeRotate);
      av_free(pAvCtx->out[idx].scale[scaleIdx].pframeRotate);
      pAvCtx->out[idx].scale[scaleIdx].pframeRotate = NULL;
    }

    //LOG(X_DEBUG("CALLING AVPICTURE_ALLOC rotate out[%d] scale[%d] pix_fmt:%d, %dx%d"), idx, scaleIdx, pDimensionsOut->pix_fmt, width, height);

    if((pAvCtx->out[idx].scale[scaleIdx].pframeRotate = avcodec_alloc_frame()) == NULL ||
      avpicture_alloc((AVPicture *)pAvCtx->out[idx].scale[scaleIdx].pframeRotate,
                    pDimensionsOut->pix_fmt, width, height) < 0) {
      return IXCODE_RC_ERROR_SCALE;
    }

  } else {
    width = pDimensionsOut->width - pAvCtx->out[idx].scale[scaleIdx].padLeft - 
                                    pAvCtx->out[idx].scale[scaleIdx].padRight;
    height = pDimensionsOut->height - pAvCtx->out[idx].scale[scaleIdx].padTop - 
                                      pAvCtx->out[idx].scale[scaleIdx].padBottom;
  }

  PIPXLOG("CALLING SWS_GETCONTEXT out[%d] scale[%d] %dx%d -> %dx%d", idx, scaleIdx, pDimensionsIn->width - pAvCtx->out[idx].scale[scaleIdx].cropLeft - pAvCtx->out[idx].scale[scaleIdx].cropRight, pDimensionsIn->height - pAvCtx->out[idx].scale[scaleIdx].cropTop - pAvCtx->out[idx].scale[scaleIdx].cropBottom, width, height);

  if((pAvCtx->out[idx].scale[scaleIdx].pswsctx = sws_getContext(
       pDimensionsIn->width - pAvCtx->out[idx].scale[scaleIdx].cropLeft - 
                              pAvCtx->out[idx].scale[scaleIdx].cropRight,
       pDimensionsIn->height - pAvCtx->out[idx].scale[scaleIdx].cropTop - 
                               pAvCtx->out[idx].scale[scaleIdx].cropBottom,
                                       pDimensionsIn->pix_fmt,
                                       width,
                                       height,
                                       pDimensionsOut->pix_fmt,
                                       pAvCtx->out[idx].scale[scaleIdx].scalertype, 
                                       NULL, NULL, NULL)) == NULL) {
    LOG(X_ERROR("Failed to init out[%d].scaler[%d] frame resize context"), idx, scaleIdx);
    return IXCODE_RC_ERROR_SCALE;
  }

  if(pAvCtx->out[idx].scale[scaleIdx].pframeScaled) {
    avpicture_free((AVPicture *)pAvCtx->out[idx].scale[scaleIdx].pframeScaled);
    av_free(pAvCtx->out[idx].scale[scaleIdx].pframeScaled);
    pAvCtx->out[idx].scale[scaleIdx].pframeScaled = NULL;
  }

  //LOG(X_DEBUG("CALLING AVPICTURE_ALLOC pframeScaled out[%d] scale[%d] pix_fmt:%d, %dx%d"), idx, scaleIdx, pDimensionsOut->pix_fmt, pDimensionsOut->width, pDimensionsOut->height);

  if((pAvCtx->out[idx].scale[scaleIdx].pframeScaled = avcodec_alloc_frame()) == NULL ||
    avpicture_alloc((AVPicture *)pAvCtx->out[idx].scale[scaleIdx].pframeScaled,
                    pDimensionsOut->pix_fmt, pDimensionsOut->width, pDimensionsOut->height) < 0) {
    return IXCODE_RC_ERROR_SCALE;
  }
  //fprintf(stderr, "PFRAMESCALED 0x%x, lsz:%d, 0x%x, lsz:%d\n", pAvCtx->out[idx].pframeScaled->data[0], pAvCtx->out[idx].pframeScaled->linesize[0], pAvCtx->out[idx].pframeScaled->data[1], pAvCtx->out[idx].pframeScaled->linesize[1]);
  if(pAvCtx->out[idx].scale[scaleIdx].padLeft > 0 || pAvCtx->out[idx].scale[scaleIdx].padRight > 0 ||
     pAvCtx->out[idx].scale[scaleIdx].padTop > 0 || pAvCtx->out[idx].scale[scaleIdx].padBottom > 0) {

    if(avpicture_fill((AVPicture *) pAvCtx->out[idx].scale[scaleIdx].pframeScaled, 
            pAvCtx->out[idx].scale[scaleIdx].pframeScaled->data[0], pDimensionsOut->pix_fmt, 
            pDimensionsOut->width - (pAvCtx->out[idx].scale[scaleIdx].padLeft + 
                                     pAvCtx->out[idx].scale[scaleIdx].padRight),
            pDimensionsOut->height - (pAvCtx->out[idx].scale[scaleIdx].padTop + 
                                      pAvCtx->out[idx].scale[scaleIdx].padBottom)) < 0) {
      return IXCODE_RC_ERROR_SCALE;
    }

    if((sz = avpicture_fill(&pic, NULL, pDimensionsOut->pix_fmt, pDimensionsOut->width, 
                            pDimensionsOut->height)) > 0) {
      if(pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufPad) {
        av_free(pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufPad);
      }
      pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufPad = (unsigned char *) av_malloc(sz);
 
    }
  }

  if(pAvCtx->out[idx].scale[scaleIdx].cropLeft > 0 || pAvCtx->out[idx].scale[scaleIdx].cropRight > 0 || 
     pAvCtx->out[idx].scale[scaleIdx].cropTop > 0 || pAvCtx->out[idx].scale[scaleIdx].cropBottom > 0) {

    if((sz = avpicture_fill(&pic, NULL, pDimensionsIn->pix_fmt, 
                 pDimensionsIn->width - pAvCtx->out[idx].scale[scaleIdx].cropLeft - 
                                        pAvCtx->out[idx].scale[scaleIdx].cropRight,
                 pDimensionsIn->height - pAvCtx->out[idx].scale[scaleIdx].cropTop - 
                                         pAvCtx->out[idx].scale[scaleIdx].cropBottom)) > 0) {

      if(pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufCrop) {
        av_free(pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufCrop);
      }
      pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufCrop = (unsigned char *) av_malloc(sz);
    }
  }

  if(pAvCtx->decIsImage >  1) {
    pAvCtx->decIsImage = 1;
  }

  LOG(X_INFO("Resizing picture %dx%d(%d) -> %dx%d(%d) (type:%d)"),
           pDimensionsIn->width, pDimensionsIn->height, pDimensionsIn->pix_fmt,
           pDimensionsOut->width, pDimensionsOut->height, pDimensionsOut->pix_fmt, 
           pAvCtx->out[idx].scale[scaleIdx].scalertype);

  LOG(X_DEBUG("out[%d].scaler[%d] (crop / pad) config %dx%d(%d) -> %dx%d(%d)"), idx, scaleIdx,
              pDimensionsIn->width - pAvCtx->out[idx].scale[scaleIdx].cropLeft - 
                                     pAvCtx->out[idx].scale[scaleIdx].cropRight, 
              pDimensionsIn->height - pAvCtx->out[idx].scale[scaleIdx].cropTop - 
                                      pAvCtx->out[idx].scale[scaleIdx].cropBottom, 
              pDimensionsIn->pix_fmt, 
              pDimensionsOut->width - pAvCtx->out[idx].scale[scaleIdx].padLeft - 
                                      pAvCtx->out[idx].scale[scaleIdx].padRight, 
              pDimensionsOut->height - pAvCtx->out[idx].scale[scaleIdx].padTop - 
                                       pAvCtx->out[idx].scale[scaleIdx].padBottom, 
              pDimensionsOut->pix_fmt);
  //LOG(X_DEBUG("idx[%d].scale[%d].pswsctx->srcFormat:%d"), idx, scaleIdx,  pAvCtx->out[idx].scale[scaleIdx].pswsctx->srcFormat);

  return IXCODE_RC_OK;
}

static enum IXCODE_RC scale_frame(IXCODE_VIDEO_CTXT_T *pXcode, 
                                  const AVFrame *pframein, 
                                  AVFrame *pframeout, 
                                  unsigned int idx, 
                                  unsigned int scaleIdx,
                                  const VID_DIMENSIONS_T *pDimensionsOut, 
                                  const VID_DIMENSIONS_T *pDimensionsIn) {

  AVFrame tmpFrame;
  IXCODE_AVCTXT_T *pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;
  int dec_height = pDimensionsIn->height;

  tmpFrame = *pframein;

#if defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)

  //
  // Do any cropping
  //
  if(pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufCrop && 
     (pAvCtx->out[idx].scale[scaleIdx].cropTop > 0 || pAvCtx->out[idx].scale[scaleIdx].cropBottom > 0 ||
      pAvCtx->out[idx].scale[scaleIdx].cropLeft > 0 || pAvCtx->out[idx].scale[scaleIdx].cropRight > 0)) {

    tmpFrame.data[0] = pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufCrop;
    
    pip_resize_canvas(0, (const unsigned char **) pframein->data,  
                   pframein->linesize, tmpFrame.data, tmpFrame.linesize,
                   pDimensionsIn->width, 
                   pAvCtx->out[idx].scale[scaleIdx].cropLeft, 
                   pAvCtx->out[idx].scale[scaleIdx].cropRight,
                   pDimensionsIn->height, 
                   pAvCtx->out[idx].scale[scaleIdx].cropTop, 
                   pAvCtx->out[idx].scale[scaleIdx].cropBottom, NULL);

    dec_height = pDimensionsIn->height - pAvCtx->out[idx].scale[scaleIdx].cropTop - 
                                         pAvCtx->out[idx].scale[scaleIdx].cropBottom;

  //if(pXcode->common.encodeInIdx%10 == 0) dumpyuv(&tmpFrame, pAvCtx->dec_width , pAvCtx->dec_height - pAvCtx->scale[scaleIdx].cropTop - pAvCtx->scale[scaleIdx].cropBottom);
  }
#endif // XCODE_HAVE_PIP


  PIPXLOG("SWS_SCALE out[%d] scale[%d] %dx%d -> %dx%d, idx[%d].scale[%d].pswsctx->srcFormat:%d, dstFormat:%d, lsin:%d,%d,%d,%d, lsout:%d,%d,%d,%d", idx, scaleIdx, pDimensionsIn->width - pAvCtx->out[idx].scale[scaleIdx].cropLeft - pAvCtx->out[idx].scale[scaleIdx].cropRight, pDimensionsIn->height - pAvCtx->out[idx].scale[scaleIdx].cropTop - pAvCtx->out[idx].scale[scaleIdx].cropBottom, pDimensionsOut->width, pDimensionsOut->height, idx, scaleIdx,  pAvCtx->out[idx].scale[scaleIdx].pswsctx->srcFormat, pAvCtx->out[idx].scale[scaleIdx].pswsctx->dstFormat, tmpFrame.linesize[0], tmpFrame.linesize[1], tmpFrame.linesize[2], tmpFrame.linesize[3], pframeout->linesize[0], pframeout->linesize[1], pframeout->linesize[2], pframeout->linesize[3]);

  if(sws_scale(pAvCtx->out[idx].scale[scaleIdx].pswsctx,
          (const uint8_t **) tmpFrame.data,
                             tmpFrame.linesize,
                             0,
                             dec_height,
     pAvCtx->out[idx].scale[scaleIdx].pframeRotate ? pAvCtx->out[idx].scale[scaleIdx].pframeRotate->data : 
                                                    pframeout->data,
     pAvCtx->out[idx].scale[scaleIdx].pframeRotate ? pAvCtx->out[idx].scale[scaleIdx].pframeRotate->linesize : 
                                                     pframeout->linesize) < 0) {
    LOG(X_ERROR("Failed to resize frame"));
    return IXCODE_RC_ERROR_SCALE;
  }

#if defined(XCODE_FILTER_ROTATE) && (XCODE_FILTER_ROTATE > 0)

  if(pAvCtx->out[idx].scale[scaleIdx].pframeRotate && pXcode->out[idx].rotate.common.active &&
     pXcode->out[idx].active && !pXcode->out[idx].passthru) {
    if(rotateFilter(pXcode, idx, pframeout, scaleIdx)) {
      return IXCODE_RC_ERROR_SCALE;
    }
  }

#endif // (XCODE_FILTER_ROTATE) && (XCODE_FILTER_ROTATE> 0)

  //unsigned char rgb[4] = { 0x00, 0x00, 0xff, 0x00 }; pip_fill_rect(pframeout->data, pframeout->linesize, pAvCtx->out[idx].enc_width, pAvCtx->out[idx].enc_height, 300, 200, 200, 300, rgb);

#if defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)

  //
  // Do any border padding
  //
  if(pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufPad && 
     (pAvCtx->out[idx].scale[scaleIdx].padTop > 0 || pAvCtx->out[idx].scale[scaleIdx].padBottom > 0 ||
      pAvCtx->out[idx].scale[scaleIdx].padLeft > 0 || pAvCtx->out[idx].scale[scaleIdx].padRight > 0)) {

    tmpFrame = *pframeout;
    tmpFrame.data[0] = pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufPad;
    // pip_resize_canvas only needs data[0] to be set.

    //LOG(X_DEBUG("CALLING PIP_RESIZE_C out[%d].scaleIdx[%d] GROW TO %d (pad l:%d, r:%d, t:%d, b:%d) ls:%d"), idx, scaleIdx, pAvCtx->out[idx].dim_enc.height - (pAvCtx->out[idx].scale[scaleIdx].padTop + pAvCtx->out[idx].scale[scaleIdx].padBottom),pAvCtx->out[idx].scale[scaleIdx].padLeft, pAvCtx->out[idx].scale[scaleIdx].padRight, pAvCtx->out[idx].scale[scaleIdx].padTop, pAvCtx->out[idx].scale[scaleIdx].padBottom, tmpFrame.linesize[0]);

    pip_resize_canvas(1, (const unsigned char **) pframeout->data,  
                   pframeout->linesize, tmpFrame.data, tmpFrame.linesize,
                   pDimensionsOut->width - (pAvCtx->out[idx].scale[scaleIdx].padLeft + 
                                            pAvCtx->out[idx].scale[scaleIdx].padRight), 
                   pAvCtx->out[idx].scale[scaleIdx].padLeft, pAvCtx->out[idx].scale[scaleIdx].padRight,
                   pDimensionsOut->height - (pAvCtx->out[idx].scale[scaleIdx].padTop + 
                                             pAvCtx->out[idx].scale[scaleIdx].padBottom), 
                   pAvCtx->out[idx].scale[scaleIdx].padTop, pAvCtx->out[idx].scale[scaleIdx].padBottom, 
                   pAvCtx->out[idx].scale[scaleIdx].padRGB);

    //fprintf(stderr, "SWAP FILL pip:%d, out_pix_fmt:%d\n", pXcode->pip.active, out_pix_fmt);
    swap_fill(&pAvCtx->out[idx], pframeout, tmpFrame.linesize, &pAvCtx->out[idx].scale[scaleIdx].pScaleDataBufPad,
              pDimensionsOut->pix_fmt, scaleIdx);

  }
#endif //XCODE_HAVE_PIP


    //fprintf(stderr, "before unsharp data[0]: 0x%x, pScaleDataBufFilter:0x%x, ls3:%d, pix_fmt:%d\n", pframeout->data[0], pAvCtx->out[idx].pScaleDataBufFilter, pframeout->linesize[3], pAvCtx->out[idx].enc_pix_fmt);

  //testfilter_yuv420(pframeout->data, pframeout->linesize, pAvCtx->out[idx].enc_width, pAvCtx->out[idx].enc_height);
  //testfilter2_nv21(pframeout->data, pframeout->linesize, pAvCtx->out[idx].enc_width, pAvCtx->out[idx].enc_height);

  return IXCODE_RC_OK;
}

static int ixcode_video_scale(IXCODE_VIDEO_CTXT_T *pXcode, 
                              AVFrame *pframesout[IXCODE_VIDEO_OUT_MAX],
                              AVFrame *pframein,
                              int dupFrame,
                              IXCODE_OUTBUF_T *pout,
                              int lenOutRes[IXCODE_VIDEO_OUT_MAX],
                              const VID_DIMENSIONS_T *pDimensionsIn[IXCODE_VIDEO_OUT_MAX],
                              unsigned int scaleIdx,
                              int is_pip) {
  int rc = 0;
  unsigned int outidx;
  IXCODE_AVCTXT_T *pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;
  VID_DIMENSIONS_T *pDimensionsOut;
  unsigned int pipframeidx = 0;

#if defined(XCODE_PROFILE_VID)
   gettimeofday(&gtv[4], NULL); 
#endif // XCODE_PROFILE_VID

  if(is_pip && scaleIdx >= 2) {
    // This is the nth PIP output unique resolution 
    pipframeidx = scaleIdx - 1;
  }

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(!pXcode->out[outidx].active || pXcode->out[outidx].passthru) {
      continue;
    }

    pDimensionsOut = is_pip ?  &pAvCtx->out[outidx].dim_pips[pipframeidx] : &pAvCtx->out[outidx].dim_enc;

    if(pAvCtx->out[outidx].scale[scaleIdx].pframeScaled) {
      *pframesout[outidx] = *(pAvCtx->out[outidx].scale[scaleIdx].pframeScaled);
    } else {
      *pframesout[outidx] = *pframein;
    }

    if(!pAvCtx->out[outidx].encWrap.u.v.fEncode && (pout && lenOutRes)) {

      if(pXcode->pip.active) {


      } else if(pout->lenbuf < pAvCtx->out[outidx].dim_enc.pic_sz ||
       //TODO: this depends ont the scaler to fill the contents of pout->buf 
        avpicture_fill((AVPicture *) pframesout[outidx], pout->buf, pDimensionsOut->pix_fmt, 
                       pDimensionsOut->width, pDimensionsOut->height) < 0) {
        LOG(X_ERROR("Output video frame length size %d below required %d"), 
                    pout->lenbuf, pAvCtx->out[outidx].dim_enc.pic_sz);
        return IXCODE_RC_ERROR_ENCODE;
      }

      lenOutRes[outidx] = pDimensionsOut->pic_sz;

    } 

    pframesout[outidx]->pict_type = 0;
    pframesout[outidx]->pts = pXcode->common.encodeInIdx;

    if(0&&pAvCtx->decIsImage > 1) {
      //fprintf(stderr, "AVOIDING SCALER... decodeInIdx:%d\n", pXcode->common.decodeInIdx);
      //TODO: if this is a main overlay image (1 decode)... need to restore it if going from pip>1 -> pip=0

    } else if(!dupFrame && pAvCtx->out[outidx].scale[scaleIdx].pswsctx) {
      if(pipframeidx == 0 && pAvCtx->decIsImage == 1) {
        pAvCtx->decIsImage++;
      }
      PIPXLOG("SCALE_FRAME pip.active:%d, outidx[%d].scale[%d].pframeScaled:0x%x decodeInIdx:%d -> %dx%d", pXcode->pip.active, outidx, scaleIdx, pAvCtx->out[outidx].scale[scaleIdx].pframeScaled, pXcode->common.decodeInIdx, pDimensionsOut->width, pDimensionsOut->height);
      if((rc = scale_frame(pXcode, pframein, pframesout[outidx], outidx, scaleIdx,
                           pDimensionsOut, pDimensionsIn[outidx])) != IXCODE_RC_OK) {
        return rc;
      }

    }

  } // end of for(outidx = 0 ...

#if defined(XCODE_PROFILE_VID)
      gettimeofday(&gtv[5], NULL); 
      pAvCtx->prof.num_vscales++;
#endif // XCODE_PROFILE_VID

  return 0;
}

static int ixcode_video_decode(IXCODE_VIDEO_CTXT_T *pXcode,
                               unsigned char *bufIn,
                               unsigned int lenIn,
                               int *pdupFrame,
                               AVFrame **ppframein) {

  int lenRaw = 0;
  IXCODE_AVCTXT_T *pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;

  pAvCtx->framedecidx = pAvCtx->framedecidxok == 0 ? 1 : 0;
  *ppframein = &pAvCtx->framedec[pAvCtx->framedecidx];

  if(lenIn == 0 || !bufIn) {
    if(pAvCtx->havePrevVidFrame) {
      *pdupFrame = 1;
      *ppframein = &pAvCtx->framedec[pAvCtx->framedecidxok];
      pAvCtx->framedecidx = pAvCtx->framedecidxok;
    } else {
      LOG(X_WARNING("Duplicate frame requested but no prior frame available")); 
      //return IXCODE_RC_OK;
      return IXCODE_RC_ERROR;
    }
  }

  if(!(*pdupFrame)) {

    avcodec_get_frame_defaults(*ppframein);

    pXcode->common.decodeInIdx++;

#if defined(XCODE_PROFILE_VID)
    gettimeofday(&gtv[2], NULL);
#endif // XCODE_PROFILE_VID

    if(pAvCtx->decWrap.u.v.fDecode) {

      if(pXcode->common.cfgNoDecode) {
        *ppframein = &pAvCtx->framedec[pAvCtx->framedecidxok];
        lenRaw = 1;
      } else if(pAvCtx->decIsImage && pXcode->common.decodeInIdx > 1) {
        *ppframein = &pAvCtx->framedec[pAvCtx->framedecidxok];
        //*ppframein = &pAvCtx->framedec[1];
        lenRaw = 1;
        //fprintf(stderr, "ppframe decodeInIdx:%d, framedecidxok:%d, framedecidx:%d\n", pXcode->common.decodeInIdx, pAvCtx->framedecidxok, pAvCtx->framedecidx);
      } else if((lenRaw = pAvCtx->decWrap.u.v.fDecode(&pAvCtx->decWrap.u.v.ctxt,
                                               0,
                                               *ppframein,
                                               bufIn,
                                               lenIn)) < 0) {

        LOG(X_ERROR("Failed to decode video frame length %d"), lenIn);
        return IXCODE_RC_ERROR_DECODE;
      }
      //fprintf(stderr, "lenRaw:%d, lenIn:%d, ppframein %d,%d,%d, bufIn:%d\n", lenRaw, lenIn, (int)(*ppframein)->data[0]%16, (int)(*ppframein)->data[1]%16, (int)(*ppframein)->data[2]%16, (int)bufIn%16);

    } else {

      if(lenIn < pAvCtx->dim_in.pic_sz) {
        LOG(X_ERROR("Input video frame length size %d below required %d"), lenIn, pAvCtx->dim_in.pic_sz);
        return IXCODE_RC_ERROR_DECODE;
      } else if(avpicture_fill((AVPicture *) *ppframein, 
                               bufIn, 
                               pAvCtx->dim_in.pix_fmt, 
                               pAvCtx->dim_in.width, 
                               pAvCtx->dim_in.height) < 0) {
        LOG(X_ERROR("avpicture_fill failed")); 
        return IXCODE_RC_ERROR_DECODE;
      }
      pAvCtx->havePrevVidFrame = 1;
      lenRaw=1;

      //TODO: put this out of xcode, so its only done once
      if(pXcode->common.cfgFileTypeIn == XC_CODEC_TYPE_VID_CONFERENCE) {
        unsigned char rgb[4] = { 0x00, 0x00, 0x00, 0x00 };     
        //unsigned char rgb[4] = { 0x40, 0x40, 0x40, 0x00 };     
        pip_fill_rect((*ppframein)->data, (*ppframein)->linesize,
                       pAvCtx->dim_in.width, pAvCtx->dim_in.height, 0, 0, 
                       pAvCtx->dim_in.width, pAvCtx->dim_in.height, rgb);
      }

    }

#if defined(XCODE_PROFILE_VID)
    gettimeofday(&gtv[3], NULL);
    pAvCtx->prof.num_vdecodes++;
#endif // XCODE_PROFILE_VID

    if((pAvCtx->havePrevVidFrame && lenRaw > 0)) {

      pXcode->common.decodeOutIdx++;

  //fprintf(stderr, "DEC_PIX_FMT:%d, %dx%d, lenRaw:%d\n",pAvCtx->dec_pix_fmt, pAvCtx->dec_width, pAvCtx->dec_height, lenRaw);
  //if(pXcode->common.decodeOutIdx%10 == 0) dumpyuv(*ppframein, pAvCtx->dec_width, pAvCtx->dec_height);

#if defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)
      //
      // Overlay any PIP image(s) onto the decoded frame prior to scaling
      //
      if(pXcode->overlay.havePip &&
         check_overlay(pXcode, PIP_FLAGS_INSERT_BEFORE_SCALE)) {

#if defined(XCODE_PROFILE_VID)
        gettimeofday(&gtv[8], NULL);
#endif // XCODE_PROFILE_VID

        PIPXLOG("CREATE_OVERLAY (before)[%d] 0x%x", 0, pXcode);

        pip_create_overlay(pXcode, 
                           (*ppframein)->data, 
                           (*ppframein)->linesize, 
                           0,
                           pAvCtx->dim_in.width,
                           pAvCtx->dim_in.height,
                           1);

#if defined(XCODE_PROFILE_VID)
        gettimeofday(&gtv[9], NULL);
        pAvCtx->prof.num_blend++;
#endif // XCODE_PROFILE_VID

      }
#endif // XCODE_HAVE_PIP

    }

  }

  return lenRaw;
}


static int ixcode_video_encode(IXCODE_VIDEO_CTXT_T *pXcode,
                               int lenOutRes[IXCODE_VIDEO_OUT_MAX],
                               AVFrame *pframesout[IXCODE_VIDEO_OUT_MAX],
                               IXCODE_OUTBUF_T *pout) {

  unsigned int outidx;
  unsigned int bufOutIdx = 0;
  IXCODE_AVCTXT_T *pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;

  //fprintf(stderr, "encode_video[%d]\n", pXcode->common.encodeOutIdx);

#if defined(XCODE_PROFILE_VID)
  gettimeofday(&gtv[6], NULL);
#endif // XCODE_PROFILE_VID

  pXcode->common.encodeInIdx++;

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(!pXcode->out[outidx].active) {
      continue;
    } else if(pXcode->out[outidx].passthru) {

      lenOutRes[outidx] = pout->lens[outidx];
      // TODO: temp hardcode of FRAMESZ_VIDQUEUE_PREBUF_BYTES
      bufOutIdx += lenOutRes[outidx] + (128 + 16 - (lenOutRes[outidx] % 16));

    } else if(pAvCtx->out[outidx].encWrap.u.v.fEncode) {

      //if(pXcode->pip.active) LOG(X_DEBUG("xcode fEncode pip.active:%d, cfgFileTypeOut:%d, doing encode... Kbs:%d, vbvMax:%d, gopMaxMs:%d, resLookaheadMin1:%d, %dx%d (%dx%d)"), pXcode->pip.active, pXcode->common.cfgFileTypeOut, pXcode->out[0].cfgBitRateOut, pXcode->out[0].cfgVBVMaxRate, pXcode->out[0].cfgGopMaxMs, pXcode->out[0].resLookaheadmin1, pXcode->out[0].resOutH, pXcode->out[outidx].resOutV, pXcode->pip.pos[0].pwidth ? *pXcode->pip.pos[0].pwidth : -1, pXcode->pip.pos[0].pheight ? *pXcode->pip.pos[0].pheight : -1);
      if(bufOutIdx >= pout->lenbuf) {
        LOG(X_ERROR("xcoder encode[%d] buffer[%d] exceeds %d"), outidx, bufOutIdx, pout->lenbuf);
        lenOutRes[outidx] = -1;
        break;
      } else {

      //LOG(X_DEBUG("fEncode outidx:%d, linesize:%d,%d,%d,%d"), outidx, pframesout[outidx]->linesize[0], pframesout[outidx]->linesize[1], pframesout[outidx]->linesize[2], pframesout[outidx]->linesize[3]);

        //fprintf(stderr, "encode_video data[0]:0x%x -> 0x%x\n", pframesout[outidx]->data[0], &pout->buf[bufOutIdx]);
        lenOutRes[outidx] = pAvCtx->out[outidx].encWrap.u.v.fEncode(
                                                     &pAvCtx->out[outidx].encWrap.u.v.ctxt,
                                                     outidx,
                                                     pframesout[outidx],
                                                     &pout->buf[bufOutIdx], 
                                                     pout->lenbuf - bufOutIdx);
        //static int g_encodeidx; fprintf(stderr, "ENCODE_VIDEO [%d] [outidx:%d] len-out:%d, frameType:%s, in:0x%x out:0x%x { [bufOutIdx:%d]: 0x%x 0x%x 0x%x 0x%x }\n", g_encodeidx++, outidx, lenOutRes[outidx], pXcode->out[outidx].frameType == FRAME_TYPE_I ? "I\n\n\n" : pXcode->out[outidx].frameType == FRAME_TYPE_P ? "P" : pXcode->out[outidx].frameType == FRAME_TYPE_B ? "B" : "", (int64_t)pframesout[outidx]%16, (int64_t)(&pout->buf[bufOutIdx])%16, bufOutIdx, pout->buf[bufOutIdx], pout->buf[bufOutIdx+1],pout->buf[bufOutIdx+2],pout->buf[bufOutIdx+3]);

//lenOutRes[outidx] = 0;
#if defined(TESTME)
  static FILE *s_fp=NULL;
  if(lenOutRes[outidx]>0) {
    if(!s_fp) { s_fp = fopen("testencode.log", "w"); }
    uint32_t crc_rc = test_crc(&pout->buf[bufOutIdx], lenOutRes[outidx]);
    fprintf(s_fp, "OUT[%d].len:%d crc:0x%x\n", pXcode->common.encodeOutIdx, lenOutRes[outidx], crc_rc);
    fflush(s_fp);
  }
#endif //(TESTME)

      }

      pout->poffsets[outidx] = &pout->buf[bufOutIdx];
      pout->lens[outidx] = lenOutRes[outidx];
      if(lenOutRes[outidx] > 0) {
        // TODO: temp hardcode of FRAMESZ_VIDQUEUE_PREBUF_BYTES
        bufOutIdx += lenOutRes[outidx] + (128 + 16 - (lenOutRes[outidx] % 16));
      }

    }

  } // end of for(outidx ...

#if defined(XCODE_PROFILE_VID)
  gettimeofday(&gtv[7], NULL); 
  pAvCtx->prof.num_vencodes++;
#endif // XCODE_PROFILE_VID

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(!pXcode->out[outidx].active || pXcode->out[outidx].passthru) {
      continue;
    }

    if(lenOutRes[outidx] > 0) {

      if(outidx == 0 || (outidx == 1 && pXcode->out[0].passthru)) {
        pXcode->common.encodeOutIdx++;
      }

    } else if(lenOutRes[outidx] < 0) {
      LOG(X_ERROR("encode error lenOut:%d"), lenOutRes[outidx]);
      lenOutRes[outidx] = IXCODE_RC_ERROR_ENCODE;
    }

  } // end of for(outidx ... 

  return lenOutRes[0];
}

static enum IXCODE_RC encode_shared_pip_out(IXCODE_VIDEO_CTXT_T *pXcode) {

  IXCODE_AVCTXT_T *pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;
  IXCODE_AVCTXT_T *pAvCtxOverlay = NULL;
  enum IXCODE_RC rc = IXCODE_RC_OK;
  AVFrame *pframesout[IXCODE_VIDEO_OUT_MAX];
  const VID_DIMENSIONS_T *pDimensionsIn[IXCODE_VIDEO_OUT_MAX];
  unsigned int outidx;
  int width, height;
  unsigned int sz;
  AVFrame frameIn;
  AVFrame *pframeIn;

  if(pXcode->overlay.havePip) {

    pthread_mutex_lock(&pXcode->pip.pXOverlay->overlay.mtx);

    if(!(pAvCtxOverlay = (IXCODE_AVCTXT_T *) pXcode->pip.pXOverlay->common.pPrivData)) {
      LOG(X_ERROR("PIP context not available"));
      pthread_mutex_unlock(&pXcode->pip.pXOverlay->overlay.mtx);
      return -1;
    }
    pframeIn = &pAvCtxOverlay->out[0].scale[0].frameenc;
    width = pAvCtxOverlay->out[0].dim_enc.width;
    height = pAvCtxOverlay->out[0].dim_enc.height;
    frameIn = *pframeIn;
    frameIn.linesize[3] = 0;

    pthread_mutex_unlock(&pXcode->pip.pXOverlay->overlay.mtx);

    if(!pAvCtx->out[0].pPipBufEncoderIn) {
      //sz = avpicture_get_size(PIX_FMT_YUVA420P, pAvCtxOverlay->out[0].dim_enc.width,
      sz = avpicture_get_size(pAvCtxOverlay->out[0].dim_enc.pix_fmt, width, height);
      if(!(pAvCtx->out[0].pPipBufEncoderIn = av_malloc(sz))) {
        return IXCODE_RC_ERROR;
      }
    }

    xcode_fill_yuv420p(frameIn.data, frameIn.linesize, pAvCtx->out[0].pPipBufEncoderIn, height);

    //pthread_mutex_lock(&pXcode->overlay.mtx);

#if 1

    int numToBlend = 0;
    //pthread_mutex_unlock(&pXcode->overlay.mtx);

    if(1||width != pAvCtx->out[0].dim_enc.width || height != pAvCtx->out[0].dim_enc.height ||
       numToBlend != pAvCtx->out[0].scale[1].numBlendPrior) {

      //unsigned char rgb[4] = { 0xff, 0xff, 0xff, 0x00 };
      unsigned char rgb[4] = { 0x00, 0x00, 0x00, 0x00 };
      pip_fill_rect(frameIn.data, frameIn.linesize, width, height, 0, 0, width, height, rgb);
    }
    pAvCtx->out[0].scale[1].numBlendPrior = numToBlend;

    //pthread_mutex_lock(&pXcode->overlay.mtx);

#endif // 0

    //LOG(X_DEBUG("Calling CREATE OVERLAY from encode_shared_pip_out 0x%x"), pXcode);

   //if(pAvCtx->out[0].scale[1].pswsctx) LOG(X_DEBUG("CALLING CREATE OV... idx[%d].scale[%d].pswsctx->srcFormat:%d, dstFormat:%d"), 0, 1,  pAvCtx->out[0].scale[1].pswsctx->srcFormat, pAvCtx->out[0].scale[1].pswsctx->dstFormat);
    PIPXLOG("CREATE_OVERLAY (pip.doEncode)  idx[%d].scale[%d].pswsctx->srcFormat:%d, dstFormat:%d, idx[%d].scale[%d].pswsctx->srcFormat:%d, dstFormat:%d", 0, 0,  pAvCtx->out[0].scale[0].pswsctx ? pAvCtx->out[0].scale[0].pswsctx->srcFormat : -99, pAvCtx->out[0].scale[0].pswsctx ? pAvCtx->out[0].scale[0].pswsctx->dstFormat : -99, 0, 1,  pAvCtx->out[0].scale[1].pswsctx ? pAvCtx->out[0].scale[1].pswsctx->srcFormat : -99, pAvCtx->out[0].scale[1].pswsctx ? pAvCtx->out[0].scale[1].pswsctx->dstFormat : -99);

    pip_create_overlay(pXcode, 
                            frameIn.data, 
                            frameIn.linesize, 
                            0,
                            width,
                            height,
                            1);
   //if(pAvCtx->out[0].scale[1].pswsctx) LOG(X_DEBUG("FINISHED CREATE OV... idx[%d].scale[%d].pswsctx->srcFormat:%d, dstFormat:%d"), 0, 1,  pAvCtx->out[0].scale[1].pswsctx->srcFormat, pAvCtx->out[0].scale[1].pswsctx->dstFormat);

    //pthread_mutex_unlock(&pXcode->overlay.mtx);

  } else {

    pthread_mutex_lock(&pXcode->pip.pXOverlay->overlay.mtx);

    pAvCtxOverlay = (IXCODE_AVCTXT_T *) pXcode->pip.pXOverlay->common.pPrivData;
    pframeIn = &pAvCtxOverlay->out[0].scale[0].frameenc;

    if(!pAvCtx->out[0].pPipBufEncoderIn) {
      sz = avpicture_get_size(pAvCtxOverlay->out[0].dim_enc.pix_fmt, pAvCtxOverlay->out[0].dim_enc.width, 
                              pAvCtxOverlay->out[0].dim_enc.height);
      if(!(pAvCtx->out[0].pPipBufEncoderIn = av_malloc(sz))) {
        pthread_mutex_unlock(&pXcode->pip.pXOverlay->overlay.mtx);
        return IXCODE_RC_ERROR;
      }
    }

    frameIn = *pframeIn;
    xcode_fill_yuv420p(frameIn.data, frameIn.linesize, pAvCtx->out[0].pPipBufEncoderIn, 
                       pAvCtxOverlay->out[0].dim_enc.height);
    memcpy(frameIn.data[0], pframeIn->data[0], frameIn.linesize[0] * pAvCtxOverlay->out[0].dim_enc.height);
    memcpy(frameIn.data[1], pframeIn->data[1], frameIn.linesize[1] * pAvCtxOverlay->out[0].dim_enc.height/2);
    memcpy(frameIn.data[2], pframeIn->data[2], frameIn.linesize[2] * pAvCtxOverlay->out[0].dim_enc.height/2);

  //fprintf(stderr, "ENCODE_PIP_OUT linesize:%d,%d, data:0x%x overlay:%dx%d, pip:%dx%d, pip.enc:%dx%d\n", pframeIn->linesize[0], pframeIn->linesize[1], pframeIn->data[0], pAvCtxOverlay->out[0].dim_enc.width, pAvCtxOverlay->out[0].dim_enc.height, pAvCtx->out[0].dim_pips[0].width, pAvCtx->out[0].dim_pips[0].height, pAvCtx->out[0].dim_enc.width, pAvCtx->out[0].dim_enc.height);

  //avpicture_get_size(PIX_FMT_YUVA420P, pAvCtx->out[idx].dim_pips[0].width, pAvCtx->out[idx].dim_pips[0].height);

    pthread_mutex_unlock(&pXcode->pip.pXOverlay->overlay.mtx);

  }

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    pframesout[outidx] = &pAvCtx->out[outidx].scale[1].frameenc;

    if(!pXcode->out[outidx].active || pXcode->out[outidx].passthru) {
      continue;
    }

    pDimensionsIn[outidx]  = &pAvCtxOverlay->out[outidx].dim_enc;

    //
    // Init the dedicated PIP encoder overlay resolution -> output encode resolution scaler
    //
    if(pAvCtx->out[outidx].scale[1].pswsctx == NULL) {

//LOG(X_DEBUG("PIP_OUT calling init_scaler outidx[%d], enc:%dx%d"), outidx,  pAvCtx->out[outidx].dim_enc.width, pAvCtx->out[outidx].dim_enc.height);
      if((rc = init_scaler(pXcode, outidx, 1, &pAvCtx->out[outidx].dim_enc, 
                           pDimensionsIn[outidx])) != IXCODE_RC_OK) {
        return rc;
      }
    }

  } // end of if(pXcode->out[outidx].active

   //LOG(X_DEBUG("CALLING VIDEO_SCALE... idx[%d].scale[%d].pswsctx->srcFormat:%d, dstFormat:%d"), 0, 1,  pAvCtx->out[0].scale[1].pswsctx->srcFormat, pAvCtx->out[0].scale[1].pswsctx->dstFormat);

  if(ixcode_video_scale(pXcode, pframesout, &frameIn, 0, NULL, NULL, pDimensionsIn, 1, 0) < 0) {
    return IXCODE_RC_ERROR_ENCODE;
  }

  return rc;
}

enum IXCODE_RC ixcode_frame_vid(IXCODE_VIDEO_CTXT_T *pXcode, 
                                unsigned char *bufIn, 
                                unsigned int lenIn,
                                IXCODE_OUTBUF_T *pout) {

  IXCODE_AVCTXT_T *pAvCtx;
  enum IXCODE_RC rc = IXCODE_RC_OK;
  int lenRaw = 0;
  int lenOutRes[IXCODE_VIDEO_OUT_MAX];
  int dupFrame = 0;
  int doEncode = 0;
  int doPipAdd = 0;
  unsigned int outidx;
  const VID_DIMENSIONS_T *pDimensionsIn[IXCODE_VIDEO_OUT_MAX];
  const VID_DIMENSIONS_T *pDimensionsInTmp[IXCODE_VIDEO_OUT_MAX];
  AVFrame *pframein = NULL;
  AVFrame *pframesout[IXCODE_VIDEO_OUT_MAX];
  AVFrame *pframesoutTmp[IXCODE_VIDEO_OUT_MAX];
  AVFrame *pframesPipIn[PIP_ADD_MAX];
  AVFrame *pframesPipOut[PIP_ADD_MAX];

#if defined(XCODE_PROFILE_VID)
  memset(gtv, 0, sizeof(gtv));
  gettimeofday(&gtv[0], NULL);
#endif // XCODE_PROFILE_VID

  if(!pXcode || !pXcode->common.pPrivData || !pout || !pout->buf) {
    return IXCODE_RC_ERROR;
  }

  //
  // A pass-thru null encoding is always placed into the 0th output index
  //
  if(pXcode->out[0].passthru) {
    pout->poffsets[1] = NULL;
    pout->lens[1] = 0;
  } else {
    pout->poffsets[0] = NULL;
    pout->lens[0] = 0;
  }

  pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;
  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
    lenOutRes[outidx] = 0;
    pframesout[outidx] = &pAvCtx->out[outidx].scale[0].frameenc;
    pframesoutTmp[outidx] = &pAvCtx->out[outidx].scale[2].frameenc;
  }

  //
  // Decode the encoded frame
  //
  if((lenRaw = ixcode_video_decode(pXcode, bufIn, lenIn, &dupFrame, &pframein)) < 0) {
    return (enum IXCODE_RC) lenRaw;
  }

#if 0
  if(pXcode->pip.active && pXcode->pip.pXcodeA && lenRaw > 0 && pAvCtx->havePrevVidFrame) {
    unsigned char rgb[4] = { 0xff, 0x00, 0x00, 0x00 };     
    if(pXcode->pip.pXcodeA->vadLatest) rgb[0] = 0x00; rgb[1] = 0xff;
    pip_fill_rect(pframein->data, pframein->linesize,
        pAvCtx->dim_in.width, pAvCtx->dim_in.height, pAvCtx->dim_in.width/2, pAvCtx->dim_in.height/2, 40, 40, rgb);
  }
#endif // 0

  //fprintf(stderr, "XCODER FRAME prev:%d lenraw:%d\n", pAvCtx->havePrevVidFrame, lenRaw);

  if(dupFrame || (pAvCtx->havePrevVidFrame && lenRaw > 0)) {

    //if(pXcode->pip.active) LOG(X_DEBUG("xcode lenIn:%d, dupFrame:%d, inpts:%.3f, outpts:%.3f   %s"), lenIn, dupFrame, ((double)pXcode->common.inpts90Khz/90000), ((double)pXcode->common.outpts90Khz/90000), (dupFrame || (pXcode->common.inpts90Khz + 150 >= pXcode->common.outpts90Khz)) ? "YES" : "NO");

    if(dupFrame || pXcode->common.inpts90Khz + 150 >= pXcode->common.outpts90Khz) {
      doEncode = 1;
    } else if(pXcode->pip.active) {
      doPipAdd = 1;
    }

    if(doEncode || doPipAdd) {

      //
      // Initialize any scaler(s)
      //
      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        pDimensionsIn[outidx] = &pAvCtx->dim_in; 
        pDimensionsInTmp[outidx] = &pAvCtx->dim_in; 

        if(!pXcode->out[outidx].active || pXcode->out[outidx].passthru) {
          continue;
        }

        check_dimensions(pXcode, outidx, 0, 0);

        //fprintf(stderr, "SHOULD I CALL INIT_SCALER tid:0x%x pXcode:0x%x outidx[%d], pip.active:%d wid:%d->%d, ht:%d->%d, pix_fmt:%d->%d, force:%d\n", pthread_self(), pXcode, outidx, pXcode->pip.active, pAvCtx->dim_in.width, OUT_DIMENSIONS(pXcode, pAvCtx->out[outidx])->width, pAvCtx->dim_in.height, OUT_DIMENSIONS(pXcode, pAvCtx->out[outidx])->height, pAvCtx->dim_in.pix_fmt, OUT_DIMENSIONS(pXcode, pAvCtx->out[outidx])->pix_fmt, pAvCtx->out[outidx].scale[0].forcescaler);


        if(pAvCtx->out[outidx].scale[0].pswsctx == NULL &&
          (pAvCtx->out[outidx].scale[0].forcescaler == 1 ||
           pAvCtx->dim_in.width != OUT_DIMENSIONS(pXcode, pAvCtx->out[outidx], 0)->width ||
           pAvCtx->dim_in.height != OUT_DIMENSIONS(pXcode, pAvCtx->out[outidx], 0)->height ||
           pAvCtx->dim_in.pix_fmt != OUT_DIMENSIONS(pXcode, pAvCtx->out[outidx], 0)->pix_fmt)) {

        //fprintf(stderr, "XCODE... CALLING INIT_SCALERtid:0x%x pXcode:0x%x outidx[%d], pip.active:%d wid:%d->%d, ht:%d->%d, pix_fmt:%d->%d, force:%d\n", pthread_self(), pXcode, outidx, pXcode->pip.active, pAvCtx->dim_in.width, OUT_DIMENSIONS(pXcode, pAvCtx->out[outidx])->width, pAvCtx->dim_in.height, OUT_DIMENSIONS(pXcode, pAvCtx->out[outidx])->height, pAvCtx->dim_in.pix_fmt, OUT_DIMENSIONS(pXcode, pAvCtx->out[outidx])->pix_fmt, pAvCtx->out[outidx].scale[0].forcescaler);

          if((rc = init_scaler(pXcode, outidx, 0, OUT_DIMENSIONS(pXcode, pAvCtx->out[outidx], 0),
                               &pAvCtx->dim_in)) != IXCODE_RC_OK) {
            return rc;
          }

        } // end of if(pXcode->out[outidx].active

      } // end of for(outidx

      //
      // Resize the raw image
      //
      if(ixcode_video_scale(pXcode, pframesout, pframein, dupFrame, pout, lenOutRes, 
                            pDimensionsIn, 0, pXcode->pip.active) < 0) {
        return IXCODE_RC_ERROR_ENCODE;
      }

      //fprintf(stderr, "encode_video pts[%d] bit_rate:%d\n", frameTmp.pts, pAvCtx->codecEnc.u.v.pavctx->bit_rate);

#if defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)

      //
      // pixel-draw any watermark symbol 
      //
      if(!dupFrame && pXcode->usewatermark && !pXcode->pip.active) {

        for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

          if(!pXcode->out[outidx].active || pXcode->out[outidx].passthru) {
            continue;
          }

          pip_mark(pframesout[outidx]->data, pframesout[outidx]->linesize, 
                 pAvCtx->out[outidx].dim_enc.height ?  pAvCtx->out[outidx].dim_enc.height : pAvCtx->dim_in.height);
        }
      }

#endif // XCODE_HAVE_PIP

      if(!dupFrame) {

//fprintf(stderr, "havePip:%d flags:%d 0x%x\n", pXcode->overlay.havePip, pXcode->overlay.flags, &pXcode->overlay);
        if(pXcode->pip.active) {

#if defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)

          for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

            if(!pXcode->out[outidx].active) {
            //if(pXcode->out[outidx].passthru || (outidx > 1 && !pXcode->out[outidx - 1].passthru)) {
              continue;
            }

            pAvCtx->out[outidx].framespip[0] = *pframesout[outidx];
            pframesPipIn[1] = NULL;
            pframesPipOut[1] = NULL;

#if 1
            //
            // Check if a pipframeidx > 0 has been set with a resolution (from the layout manager) which needs
            // its own scaler instance here
            //
            if(PIP_PLACEMENT_WIDTH(pXcode->pip.pos[1]) > 0  && PIP_PLACEMENT_HEIGHT(pXcode->pip.pos[1]) > 0) {

              if(pAvCtx->out[outidx].dim_pips[1].width != PIP_PLACEMENT_WIDTH(pXcode->pip.pos[1]) ||
                 pAvCtx->out[outidx].dim_pips[1].height != PIP_PLACEMENT_HEIGHT(pXcode->pip.pos[1])) {

                PIPXLOG("NEED TO ADD PIP[1]...%dx%d", PIP_PLACEMENT_WIDTH(pXcode->pip.pos[1]), PIP_PLACEMENT_HEIGHT(pXcode->pip.pos[1]));
                if(check_dimensions(pXcode, outidx, 2, 1) > 0 && 
                 (init_scaler(pXcode, outidx, 2, OUT_DIMENSIONS(pXcode, pAvCtx->out[outidx], 1), &pAvCtx->dim_in)) != IXCODE_RC_OK) {
                  return IXCODE_RC_ERROR;
                }
              }

              PIPXLOG("PIP2 position... %dx%d calling scaler ", PIP_PLACEMENT_WIDTH(pXcode->pip.pos[1]), PIP_PLACEMENT_HEIGHT(pXcode->pip.pos[1]));
              if(ixcode_video_scale(pXcode, pframesoutTmp, pframein, 0, NULL, NULL, pDimensionsIn, 2, 1) < 0) {
                return IXCODE_RC_ERROR_ENCODE;
              }
              pframesPipIn[1] = pframesoutTmp[outidx];
              pframesPipOut[1] = pAvCtx->out[outidx].scale[2].pframeScaled; 

            }
#endif // 1

            //LOG(X_DEBUG("PIP_ADD start outidx[%d] passthru:%d, 0x%x, ls:%d, %d"), outidx, pXcode->out[outidx].passthru, pAvCtx, pAvCtx->out[outidx].framespip[0].linesize[0], pframesout[outidx]->linesize[0]);
            pframesPipIn[0] = pframesout[outidx];
            pframesPipOut[0] = pAvCtx->out[outidx].scale[0].pframeScaled; 
            if(pip_add(pXcode, outidx, pframesPipIn, pframesPipOut) != 0) {
              return IXCODE_RC_ERROR;
            } 
            //fprintf(stderr, "PIP_ADD done outidx[%d] passthru:%d, 0x%x, ls:%d, %d\n", outidx, pXcode->out[outidx].passthru, pAvCtx, pAvCtx->out[outidx].framespip[0].linesize[0], pframesout[outidx]->linesize[0]);

          } // end of for(outidx ...

          if(doEncode && pXcode->pip.doEncode) {

            if((rc = encode_shared_pip_out(pXcode)) < 0) {
              return rc;
            }
            for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
              pframesout[outidx] = &pAvCtx->out[outidx].scale[1].frameenc;
            }
          }

#endif // XCODE_HAVE_PIP

        } else if(doEncode && pXcode->overlay.havePip && !check_overlay(pXcode, PIP_FLAGS_INSERT_BEFORE_SCALE)) {

#if defined(XCODE_PROFILE_VID)
          gettimeofday(&gtv[8], NULL);
#endif // XCODE_PROFILE_VID

#if defined(XCODE_HAVE_PIP) && (XCODE_HAVE_PIP > 0)
          for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

            if(!pXcode->out[outidx].active || pXcode->out[outidx].passthru) {
              continue;
            }

            //if((!pXcode->out[0].passthru && outidx > 0) || outidx > 1) break; //TODO: pip outidx > 0 is broken in the blend func
            PIPXLOG("CREATE_OVERLAY[%d] 0x%x (post sws) data:0x%x, 0x%x, 0x%x, 0x%x, linesize:%d,%d,%d,%d", outidx, pXcode, pframesout[outidx]->data[0], pframesout[outidx]->data[1], pframesout[outidx]->data[2], pframesout[outidx]->data[3], pframesout[outidx]->linesize[0], pframesout[outidx]->linesize[1], pframesout[outidx]->linesize[2], pframesout[outidx]->linesize[3]);

            pip_create_overlay(pXcode, 
                               pframesout[outidx]->data, 
                               pframesout[outidx]->linesize, 
                               outidx,
                               pAvCtx->out[outidx].dim_enc.width,
                               pAvCtx->out[outidx].dim_enc.height,
                               1);

          }
#endif // XCODE_HAVE_PIP

#if defined(XCODE_PROFILE_VID)
          gettimeofday(&gtv[9], NULL); 
          pAvCtx->prof.num_blend++;
#endif // XCODE_PROFILE_VID

        }
      }

    } // end of if(dupFrame || pXcode->common.inpts90Khz ... 


    if(doEncode) {

#if (XCODE_FILTER_ON)

      //
      // Run any image processing filters
      //
      runfilters(pXcode, pframesout, 0);

#endif // XCODE_FILTER_ON

      //
      // Encode the video frame
      //
      if(ixcode_video_encode(pXcode, lenOutRes, pframesout, pout) < 0) {
      
      }

    } // end of if(doEncode...

  } // end of if(!dupFrame

#if defined(XCODE_PROFILE_VID)
  gettimeofday(&gtv[1], NULL); 
  pAvCtx->prof.num_v++;
  pAvCtx->prof.tottime_vdecodes += (uint64_t) ((gtv[3].tv_sec-gtv[2].tv_sec)*1000000)+
                                 ((gtv[3].tv_usec-gtv[2].tv_usec));
  pAvCtx->prof.tottime_vscales += (uint64_t)  ((gtv[5].tv_sec-gtv[4].tv_sec)*1000000)+
                                 ((gtv[5].tv_usec-gtv[4].tv_usec));
  pAvCtx->prof.tottime_vfilters += (uint64_t)  ((gtv[11].tv_sec-gtv[10].tv_sec)*1000000)+
                                 ((gtv[11].tv_usec-gtv[10].tv_usec));
  pAvCtx->prof.tottime_vrotate += (uint64_t)  ((gtv[13].tv_sec-gtv[12].tv_sec)*1000000)+
                                 ((gtv[13].tv_usec-gtv[12].tv_usec));
  pAvCtx->prof.tottime_vencodes += (uint64_t) ((gtv[7].tv_sec-gtv[6].tv_sec)*1000000)+
                                 ((gtv[7].tv_usec-gtv[6].tv_usec));
  pAvCtx->prof.tottime_blend +=    (uint64_t) ((gtv[9].tv_sec-gtv[8].tv_sec)*1000000)+
                                 ((gtv[9].tv_usec-gtv[8].tv_usec));
  pAvCtx->prof.tottime_v +=       (uint64_t)  ((gtv[1].tv_sec-gtv[0].tv_sec)*1000000)+
                                 ((gtv[1].tv_usec-gtv[0].tv_usec));

  LOG(X_DEBUG("xcode_vid done in %ld(%.1f) (dec:%ld(%.1f), scale:%ld(%.1f) filter:%ld(%.1f) rotate:%ld(%.1f) ovl:%ld(%.1f), enc:%ld(%.1f)) (len:%d)"), 

     (uint32_t)((gtv[1].tv_sec-gtv[0].tv_sec)*1000000)+((gtv[1].tv_usec-gtv[0].tv_usec)),
     (float)pAvCtx->prof.tottime_v / pAvCtx->prof.num_v/1000.0f,

     ((gtv[3].tv_sec-gtv[2].tv_sec)*1000000)+((gtv[3].tv_usec-gtv[2].tv_usec)),
     pAvCtx->prof.num_vdecodes > 0 ? (float)pAvCtx->prof.tottime_vdecodes / pAvCtx->prof.num_vdecodes/1000.0f : 0,
     ((gtv[5].tv_sec-gtv[4].tv_sec)*1000000)+((gtv[5].tv_usec-gtv[4].tv_usec)),
     pAvCtx->prof.num_vscales > 0 ? (float)pAvCtx->prof.tottime_vscales / pAvCtx->prof.num_vscales/1000.0f : 0,

     ((gtv[11].tv_sec-gtv[10].tv_sec)*1000000)+((gtv[11].tv_usec-gtv[10].tv_usec)),
     pAvCtx->prof.num_vfilters > 0 ? (float)pAvCtx->prof.tottime_vfilters / pAvCtx->prof.num_vfilters/1000.0f : 0,

     ((gtv[13].tv_sec-gtv[12].tv_sec)*1000000)+((gtv[13].tv_usec-gtv[12].tv_usec)),
     pAvCtx->prof.num_vrotate > 0 ? (float)pAvCtx->prof.tottime_vrotate / pAvCtx->prof.num_vrotate /1000.0f : 0,

     ((gtv[9].tv_sec-gtv[8].tv_sec)*1000000)+((gtv[9].tv_usec-gtv[8].tv_usec)), 
     pAvCtx->prof.num_blend > 0 ? (float)pAvCtx->prof.tottime_blend / pAvCtx->prof.num_blend/1000.0f : 0,

     ((gtv[7].tv_sec-gtv[6].tv_sec)*1000000)+((gtv[7].tv_usec-gtv[6].tv_usec)), 
     pAvCtx->prof.num_vencodes > 0 ? (float)pAvCtx->prof.tottime_vencodes / pAvCtx->prof.num_vencodes/1000.0f : 0,
     lenOutRes);
#endif // XCODE_PROFILE_VID

  return (enum IXCODE_RC) lenOutRes[0];
}

