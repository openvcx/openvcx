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
#include <pthread.h>
#include "logutil.h"
#include "xcodeconfig.h"
#include "xcoder_pip.h"
#include "xcoder_x264.h"
#include "xcoder.h"
#include "ixcode.h"
#include "dlfcn.h"
#include "x264.h"



typedef void (* X264_ENCODER_CLOSE)(x264_t *);
typedef x264_t *(* X264_ENCODER_OPEN)(x264_param_t *);
typedef int (* X264_ENCODER_RECONFIG)(x264_t *, x264_param_t *);
typedef void (* X264_PARAM_DEFAULT)(x264_param_t *);
typedef void (* X264_ENCODER_PARAMETERS)(x264_t *, x264_param_t *);
typedef char *(* X264_PARAM2STRING)(x264_param_t *, int);
typedef int (* X264_ENCODER_HEADERS)(x264_t *, x264_nal_t **pp_nal, int *pi_nal);
typedef void (* X264_PICTURE_INIT)(x264_picture_t *pic);
typedef int (* X264_ENCODER_ENCODE)(x264_t *, x264_nal_t **pp_nal, int *pi_nal, x264_picture_t *pic_in, 
                                    x264_picture_t *pic_out);
typedef int (*X264_ENCODER_DELAYED_FRAMES)(x264_t * );


typedef struct X264_AVCODEC_CTXT {
    x264_param_t    params;
    x264_t         *enc;
    x264_picture_t  pic;
    uint8_t        *sei;
    uint8_t        *extradata;
    int             extradata_size;
    int             sei_size;

    void                    *dlx264;
    X264_ENCODER_CLOSE       fClose;
    X264_ENCODER_OPEN        fOpen;
    X264_ENCODER_RECONFIG    fReconfig;
    X264_ENCODER_ENCODE      fEncode;
    X264_PARAM_DEFAULT       fParamDefault;
    X264_ENCODER_PARAMETERS  fEncoderParameters; 
    X264_PARAM2STRING        fParam2String; 
    X264_ENCODER_HEADERS     fEncoderHeaders;
    X264_PICTURE_INIT        fPictureInit;
    X264_ENCODER_DELAYED_FRAMES fDelayedFrames;
} X264_AVCODEC_CTXT_T;


extern char *x264_param2string( x264_param_t *p, int b_res);


static void xcoder_x264_log(void *p, int level, const char *fmt, va_list args) {

    //if (level < 0 || level > X264_LOG_DEBUG)
    if (level < 0 || level >= X264_LOG_INFO)
        return;

    vfprintf(stderr, fmt, args);
}

void xcoder_x264_close(CODEC_WRAP_CTXT_T *pCtxt) {
  X264_AVCODEC_CTXT_T *pXCtx = NULL;

  if(!pCtxt || (!(pXCtx = (X264_AVCODEC_CTXT_T *) pCtxt->pctxt))) {
    return;
  }

  if(pXCtx->enc) {
    pXCtx->fClose(pXCtx->enc);
    pXCtx->enc = NULL;
  }

  if(pXCtx->sei) {
    av_free(pXCtx->sei);
    pXCtx->sei = NULL;
  }

  if(pXCtx->extradata) {
    av_free(pXCtx->extradata);
    pXCtx->extradata = NULL;
  }

#if defined(XCODE_HAVE_X264_DLOPEN) && (XCODE_HAVE_X264_DLOPEN > 0)
  if(pXCtx->dlx264) {
    dlclose(pXCtx->dlx264);
    pXCtx->dlx264 = NULL;
  }
#endif // (XCODE_HAVE_X264_DLOPEN) && (XCODE_HAVE_X264_DLOPEN > 0)

  free(pCtxt->pctxt);
  pCtxt->pctxt = NULL;

}


static void get_gop(const IXCODE_VIDEO_CTXT_T *pXcode, unsigned int outidx, int *keyint_min, int *keyint_max) {
  float gop_ms = 4.3;

  *keyint_min = 10;
  *keyint_max = 120;

  if(pXcode->out[outidx].cfgGopMaxMs > 0) {
    gop_ms = (double) pXcode->out[outidx].cfgGopMaxMs / 1000.0f;
    *keyint_max = gop_ms * ((double)pXcode->resOutClockHz / pXcode->resOutFrameDeltaHz);
  }
  if(pXcode->out[outidx].cfgGopMinMs > 0) {
    gop_ms = (double) pXcode->out[outidx].cfgGopMinMs / 1000.0f;
    *keyint_min = gop_ms * ((double)pXcode->resOutClockHz / pXcode->resOutFrameDeltaHz);
  } if(pXcode->out[outidx].cfgGopMinMs == -1) {
    *keyint_max = X264_KEYINT_MAX_INFINITE;
  }

  if(*keyint_max < *keyint_min) {
    *keyint_max = *keyint_min;
  } else if(*keyint_min > *keyint_max) {
    *keyint_min = *keyint_max;
  }
}

static void set_rc(const IXCODE_VIDEO_CTXT_T *pXcode, unsigned int outidx, x264_param_t *params) {

  params->rc.i_rc_method = X264_RC_ABR;
  params->rc.i_bitrate = pXcode->out[outidx].cfgBitRateOut / 1000;

  if(pXcode->out[outidx].cfgQMax >= 10 && pXcode->out[outidx].cfgQMax <= 51) {
    params->rc.i_qp_max = pXcode->out[outidx].cfgQMax; 
  } else {
    params->rc.i_qp_max = 51;
  }

  if(pXcode->out[outidx].cfgQMin >= 10 && pXcode->out[outidx].cfgQMin <= 51) {
    params->rc.i_qp_min = pXcode->out[outidx].cfgQMin;
  } else {
    params->rc.i_qp_min = 10;
  }

  if(pXcode->out[outidx].cfgQDiff != 0) {
    params->rc.i_qp_step = pXcode->out[outidx].cfgQDiff;
  }

  if(pXcode->out[outidx].cfgVBVBufSize > 1000) {
    params->rc.i_vbv_buffer_size = pXcode->out[outidx].cfgVBVBufSize / 1000;
    if(pXcode->out[outidx].cfgVBVInitial > 0) {
      params->rc.f_vbv_buffer_init = pXcode->out[outidx].cfgVBVInitial;
    }
  } else {
    params->rc.i_vbv_buffer_size = 0;
  }
  if(pXcode->out[outidx].cfgVBVMaxRate > 1000) {
    params->rc.i_vbv_max_bitrate = pXcode->out[outidx].cfgVBVMaxRate / 1000;
  } else {
    params->rc.i_vbv_max_bitrate = 0;
  }

  if(pXcode->out[outidx].cfgRateCtrl == RATE_CTRL_TYPE_CRF) {
    params->rc.i_rc_method  = X264_RC_CRF;
    params->rc.f_rf_constant = pXcode->out[outidx].cfgQ;
    params->rc.f_rf_constant_max = pXcode->out[outidx].cfgQMax;
  } else if (pXcode->out[outidx].cfgRateCtrl == RATE_CTRL_TYPE_CQP) {
    params->rc.i_rc_method = X264_RC_CQP;
    params->rc.i_qp_constant = pXcode->out[outidx].cfgQ;
  }

}

static int init_x264params(CODEC_WRAP_CTXT_T *pCtxt, unsigned int outidx) {

  const IXCODE_VIDEO_CTXT_T *pX = (IXCODE_VIDEO_CTXT_T *) pCtxt->pXcode;
  const IXCODE_VIDEO_OUT_T *pXcode = &pX->out[outidx];
  X264_AVCODEC_CTXT_T *pXCtx = (X264_AVCODEC_CTXT_T *) pCtxt->pctxt;
  int profile = pXcode->cfgProfile;

  //fprintf(stderr, "X264 FAST:%d, PROFILE:%d\n",pXcode->cfgFast , pXcode->cfgProfile);
  switch(profile) {
    case H264_PROFILE_BASELINE:
    case H264_PROFILE_MAIN:
    case H264_PROFILE_HIGH:
      break;
    default:
      profile = H264_PROFILE_HIGH;
  }

  pXCtx->fParamDefault(&pXCtx->params);

  pXCtx->params.pf_log = xcoder_x264_log;
  pXCtx->params.p_log_private = NULL;
  pXCtx->params.i_log_level = X264_LOG_DEBUG;
  pXCtx->params.i_csp = X264_CSP_I420;

  pXCtx->params.i_level_idc = -1;
  pXCtx->params.i_width = pCtxt->pAVCtxt->out[outidx].dim_enc.width;
  pXCtx->params.i_height = pCtxt->pAVCtxt->out[outidx].dim_enc.height;
  pXCtx->params.vui.i_sar_width = 1;
  pXCtx->params.vui.i_sar_height = 1;
  pXCtx->params.i_fps_num = pXCtx->params.i_timebase_den = pX->resOutClockHz;
  pXCtx->params.i_fps_den = pXCtx->params.i_timebase_num = pX->resOutFrameDeltaHz;

  if(pXcode->resLookaheadmin1 > 0) {
    pXCtx->params.rc.i_lookahead = pXcode->resLookaheadmin1 - 1;
  }

  if(pXcode->cfgThreads > 0) {
    pXCtx->params.i_threads = pXcode->cfgThreads;
  } else {
      // default value.  Rough optimal val be around (# cores * ~2)
    pXCtx->params.i_threads = 8;
    //pXCtx->params.i_threads = X264_THREADS_AUTO;
  }

  if(pXcode->cfgMaxSliceSz > 0) {
    pXCtx->params.i_slice_max_size = pXcode->cfgMaxSliceSz;
  }

  // preset "flags=+loop"
  pXCtx->params.b_deblocking_filter = 1;
  pXCtx->params.i_deblocking_filter_alphac0 = 0;
  pXCtx->params.i_deblocking_filter_beta = 0;
  //p->analyse.intra, p->analyse.inter
  //p->analyse.i_luma_deadzone[0], p->analyse.i_luma_deadzone[1]

  if(pXcode->cfgFast < ENCODER_QUALITY_LOW) {
    // prset "cmp=+chroma"  FF_CMP_CHROMA // use chroma plane for m.e.
    pXCtx->params.analyse.b_chroma_me = 1;
  }

  if(profile == H264_PROFILE_BASELINE || pXcode->cfgFast >= ENCODER_QUALITY_MED) {
    // "coder=0" (CAVLC)
    pXCtx->params.b_cabac = 0;
  } else {
    // preset "coder=1" (CABAC)
    pXCtx->params.b_cabac = 1;
  }

  // preset "partitions=+parti8x8+parti4x4+partp8x8+partb8x8
  if(pXcode->cfgFast == ENCODER_QUALITY_BEST) {
    pXCtx->params.analyse.inter |= (X264_ANALYSE_I8x8 | X264_ANALYSE_I4x4 | 
                                    X264_ANALYSE_PSUB16x16 | X264_ANALYSE_PSUB8x8 |
                                    X264_ANALYSE_BSUB16x16);
  } else {
    pXCtx->params.analyse.inter = 0;
  }

  // preset "directpred="
  pXCtx->params.analyse.i_direct_mv_pred = X264_DIRECT_PRED_AUTO;

  // preset "me_method="
  if(pXcode->cfgFast >= ENCODER_QUALITY_LOW) {
    pXCtx->params.analyse.i_me_method = X264_ME_DIA; // ME_EPZS;
  } else if(pXcode->cfgFast >= ENCODER_QUALITY_MED) {
    pXCtx->params.analyse.i_me_method = X264_ME_HEX; //ME_HEX; 
  } else if(pXcode->cfgFast == ENCODER_QUALITY_HIGH) {
    pXCtx->params.analyse.i_me_method = X264_ME_HEX; // ME_HEX;
  } else if(pXcode->cfgFast == ENCODER_QUALITY_BEST) {
    pXCtx->params.analyse.i_me_method = X264_ME_ESA; // ME_FULL;  
  }

/*
  fps test:
  esa=full 81,74  ME_FULL (ESA)
  epzs 89,85  ME_EPZS (DIA)
  hex 88,85 ME_HEX
  umh  87, 86  ME_UMH
  tesa 72-65  ME_TESA
  full 80-74
*/


  //
  // preset "subq="
  //  0. fullpel only
  //  1. QPel SAD 1 iteration
  //  2. QPel SATD 2 iterations
  //  3. HPel on MB then QPel
  //  4. Always QPel
  //  5. Multi QPel + bi-directional motion estimation
  //  6. RD on I/P frames
  //  7. RD on all frames
  //  8. RD refinement on I/P frames
  //  9. RD refinement on all frames
  //  10. QP-RD (requires --trellis=2, --aq-mode > 0)
  //  11. Full RD [1][2] j
  //

  if(pXcode->cfgFast >= ENCODER_QUALITY_LOW) {
    pXCtx->params.analyse.i_subpel_refine = 0;
    pXCtx->params.analyse.i_direct_mv_pred = X264_DIRECT_PRED_SPATIAL;
  } else if(pXcode->cfgFast >= ENCODER_QUALITY_MED) {
    pXCtx->params.analyse.i_subpel_refine = 2;
  } else if(pXcode->cfgFast == ENCODER_QUALITY_HIGH) {
    pXCtx->params.analyse.i_subpel_refine = 4;
  } else {
    pXCtx->params.analyse.i_subpel_refine = 6;
  }

  // preset "me_range="
  pXCtx->params.analyse.i_me_range = 16;

  get_gop(pCtxt->pXcode, outidx, &pXCtx->params.i_keyint_min, &pXCtx->params.i_keyint_max);

  // preset "sc_threshold="
  if(pXcode->cfgSceneCut != 0) {
    pXCtx->params.i_scenecut_threshold = pXcode->cfgSceneCut;
  } else {
    pXCtx->params.i_scenecut_threshold = 40;
  }

  if(pXcode->cfgIQRatio > 0) {
    pXCtx->params.rc.f_ip_factor = 1 / fabs(pXcode->cfgIQRatio);
  } else {
    pXCtx->params.rc.f_ip_factor = 1 / fabs(.71f);
  }

  if(pXcode->cfgBQRatio > 0) {
    pXCtx->params.rc.f_pb_factor = pXcode->cfgBQRatio;
  } else {
    pXCtx->params.rc.f_pb_factor = 1.250f;
  }

  if(pXcode->cfgQCompress>= 0) {
    pXCtx->params.rc.f_qcompress = pXcode->cfgQCompress;
  } else {
    pXCtx->params.rc.f_qcompress = .6f;
  }

  set_rc(pCtxt->pXcode, outidx, &pXCtx->params);


  if(profile >= H264_PROFILE_MAIN) {
    // preset "b_strategy="
    pXCtx->params.i_bframe_adaptive = 1;

    // preset "max_b_frames="
    pXCtx->params.i_bframe = 3;
  } else {
    pXCtx->params.i_bframe = 0;
  }

  // preset "refs="
  if(pXcode->cfgFast >= ENCODER_QUALITY_LOW) {
    pXCtx->params.i_frame_reference = 1;
  } else if(pXcode->cfgFast > ENCODER_QUALITY_BEST) {
    pXCtx->params.i_frame_reference = 2;
  } else {
    pXCtx->params.i_frame_reference = 3;
  }

  // preset "flags2="
  pXCtx->params.analyse.b_fast_pskip = 1;

  //pXCtx->params.analyse.i_noise_reduction = 0;

  if(pXcode->cfgMBTree >= 0) {
    pXCtx->params.rc.b_mb_tree = pXcode->cfgMBTree;
  } else if(pXcode->resLookaheadmin1 != 1) {
    pXCtx->params.rc.b_mb_tree = 1; // This makes I-frame encodes super slow if lookahead >= 10 or so, but it improves the quality a bit
  } else {
    pXCtx->params.rc.b_mb_tree = 0;
  }

  pXCtx->params.analyse.b_ssim = 0;
  pXCtx->params.b_intra_refresh = 0;
  pXCtx->params.analyse.b_psy = 0;
  pXCtx->params.b_aud = 0;

  if(profile >= H264_PROFILE_MAIN && pXcode->cfgFast == ENCODER_QUALITY_BEST) {
    pXCtx->params.analyse.b_mixed_references = 1;
    pXCtx->params.i_bframe_pyramid = X264_B_PYRAMID_NORMAL;
  } else {
    pXCtx->params.analyse.b_mixed_references = 0;
    pXCtx->params.i_bframe_pyramid = X264_B_PYRAMID_NONE;
  }

  if(profile == H264_PROFILE_HIGH && pXcode->cfgFast <= ENCODER_QUALITY_HIGH) {
    pXCtx->params.analyse.b_weighted_bipred = 1;
    pXCtx->params.analyse.b_transform_8x8 = 1;
    pXCtx->params.analyse.b_fast_pskip = 0;
  } else {
    pXCtx->params.analyse.b_weighted_bipred = 0;
    pXCtx->params.analyse.b_transform_8x8 = 0;
    pXCtx->params.analyse.b_fast_pskip = 1;
  }


  if(profile >= H264_PROFILE_MAIN) {
    // preset "wpredp="
    pXCtx->params.analyse.i_weighted_pred = 2;
  } else {
    pXCtx->params.analyse.i_weighted_pred = 0;
  }

  pXCtx->params.b_repeat_headers = 0; // CODEC_FLAG_GLOBAL_HEADER;
  pXCtx->params.analyse.f_psy_trellis = 0;
  pXCtx->params.analyse.i_trellis = 0;

  LOG(X_DEBUG("H.264%s rate-ctrl: %s %.1f, fast:%d, I:%.3f B:%.3f, gop:[%d - %d], Qrange %d - %d, "
              "VBV size: %dKb, max: %dKb/s, init:%.2f, I scene threshold:%d, slice-max:%d, threads:%d,%s, qcomp:%.2f"),
              (profile == H264_PROFILE_BASELINE ? " baseline" : profile == H264_PROFILE_MAIN ? " main" : 
               profile == H264_PROFILE_HIGH ? " high" : ""),
    pXcode->cfgRateCtrl == RATE_CTRL_TYPE_CQP ? "cqp" :
    (pXcode->cfgRateCtrl == RATE_CTRL_TYPE_CRF ? "crf" : "btrt"),
    ((pXcode->cfgRateCtrl == RATE_CTRL_TYPE_CQP ||
      pXcode->cfgRateCtrl == RATE_CTRL_TYPE_CRF) ? (float)pXcode->cfgQ : pXcode->cfgBitRateOut/1000) ,
    pXcode->cfgFast -1, pXcode->cfgIQRatio, pXcode->cfgBQRatio, pXCtx->params.i_keyint_min, pXCtx->params.i_keyint_max,
    pXCtx->params.rc.i_qp_min,  pXCtx->params.rc.i_qp_max,
    pXCtx->params.rc.i_vbv_buffer_size, pXCtx->params.rc.i_vbv_max_bitrate, pXCtx->params.rc.f_vbv_buffer_init,
    pXCtx->params.i_scenecut_threshold, pXcode->cfgMaxSliceSz, pXCtx->params.i_threads,
    pXCtx->params.rc.b_mb_tree ? " mbtree" : "", pXCtx->params.rc.f_qcompress);


  //TODO: should be able to call x264_param_default_preset( x264_param_t *param, const char *preset, const char *tune )

  //fprintf(stderr, "i_b_frame_adaptive:%d, i_bframe:%d, i_frame_ref:%d, subpel_refine:%d, b_fast_pskip:%d, b_mb_tree:%d, b_frame_pyramid:%d, i_weighted_pred:%d, i_trellis:%d, b_intra_refresh:%d\n",  pXCtx->params.i_bframe_adaptive,  pXCtx->params.i_bframe,  pXCtx->params.i_frame_reference, pXCtx->params.analyse.i_subpel_refine, pXCtx->params.analyse.b_fast_pskip, pXCtx->params.rc.b_mb_tree, pXCtx->params.i_bframe_pyramid, pXCtx->params.analyse.i_weighted_pred, pXCtx->params.analyse.i_trellis, pXCtx->params.b_intra_refresh);

  //fprintf(stderr, "f_ip_factor:%.3f, f_pb_factor:%.3f, psnr:%d, threads:%d, b_interlaced:%d, i_lookahead:%d, i_sync_lookahead:%d, analyse.inter:0x%x, intra: 0x%x, i_me_method:%d, deblock:%d,%d,%d, i_direct_mv_pred:%d, me_range:%d\n", pXCtx->params.rc.f_ip_factor, pXCtx->params.rc.f_pb_factor, pXCtx->params.analyse.b_psnr, pXCtx->params.i_threads, pXCtx->params.b_interlaced, pXCtx->params.rc.i_lookahead, pXCtx->params.i_sync_lookahead, pXCtx->params.analyse.inter, pXCtx->params.analyse.intra, pXCtx->params.analyse.i_me_method, pXCtx->params.b_deblocking_filter, pXCtx->params.i_deblocking_filter_alphac0, pXCtx->params.i_deblocking_filter_beta, pXCtx->params.analyse.i_direct_mv_pred,  pXCtx->params.analyse.i_me_range);

#if defined(TESTME)
sdfsd
  pXCtx->params.analyse.b_psnr = 0;
  pXCtx->params.i_keyint_min = 0;
  pXCtx->params.i_keyint_max = 250;
  //pXCtx->params.rc.i_rc_method = X264_RC_CQP;
  //pXCtx->params.rc.i_qp_constant = 26;
  //pXCtx->params.rc.i_rc_method   = X264_RC_CRF;
  //pXCtx->params.rc.f_rf_constant = 0;
  pXCtx->params.i_bframe = 4;
  pXCtx->params.analyse.i_me_method = X264_ME_HEX;
  //= X264_PART_I4X4 | X264_PART_I8X8 | X264_PART_P8X8 | X264_PART_P4X4 | X264_PART_B8X8;
  pXCtx->params.analyse.inter |= X264_ANALYSE_I4x4;
  pXCtx->params.analyse.inter |= X264_ANALYSE_I8x8;
  pXCtx->params.analyse.inter |= X264_ANALYSE_PSUB16x16;
  pXCtx->params.analyse.inter |= X264_ANALYSE_PSUB8x8;
  pXCtx->params.analyse.inter |= X264_ANALYSE_PSUB16x16;
#endif //(TESTME)

  return 0;
}


static int xcoder_x264_reinit(CODEC_WRAP_CTXT_T *pCtxt, unsigned int outidx) {

  X264_AVCODEC_CTXT_T *pXCtx = NULL;
  IXCODE_VIDEO_CTXT_T *pXcode = NULL;
  x264_param_t params;
  int full_reset = 1;

  if(!(pXCtx = ((X264_AVCODEC_CTXT_T *) pCtxt->pctxt)) || !pXCtx->enc) {
    return -1;
  }

  pXcode = (IXCODE_VIDEO_CTXT_T *) pCtxt->pXcode;

  // TODO: this should be more sophisticated...
  if(pXcode->out[outidx].cfgRateCtrl == RATE_CTRL_TYPE_CRF ||
     pXcode->out[outidx].cfgRateCtrl == RATE_CTRL_TYPE_CQP) {
    full_reset = 0;
  }

  if(full_reset) {

    LOG(X_DEBUG("Doing full h264 re-init"));
    pXCtx->fClose(pXCtx->enc);

    //LOG(X_DEBUG("fps %d/%d config:%d/%d"), pXCtx->params.i_fps_num, pXCtx->params.i_fps_den, pXcode->resOutClockHz, pXcode->resOutFrameDeltaHz);
    pXCtx->params.i_fps_num = pXCtx->params.i_timebase_den = pXcode->resOutClockHz;
    pXCtx->params.i_fps_den = pXCtx->params.i_timebase_num = pXcode->resOutFrameDeltaHz;

    get_gop(pCtxt->pXcode, outidx, &pXCtx->params.i_keyint_min, &pXCtx->params.i_keyint_max);

    set_rc(pCtxt->pXcode, outidx, &pXCtx->params);

    if(!(pXCtx->enc = pXCtx->fOpen(&pXCtx->params))) {
      LOG(X_ERROR("x264_encoder_open failed"));
      return -1;
    }

  } else {

    memset(&params, 0, sizeof(params));
    pXCtx->fEncoderParameters(pXCtx->enc, &params);

    params.rc.i_bitrate = pXcode->out[outidx].cfgBitRateOut / 1000;
    if(pXcode->out[outidx].cfgVBVBufSize > 1000) {
      params.rc.i_vbv_buffer_size = pXcode->out[outidx].cfgVBVBufSize / 1000;
      //if(pXcode->out[outidx].cfgVBVInitial > 0) {
      //  params->rc.f_vbv_buffer_init = pXcode->out[outidx].cfgVBVInitial;
      //}
    } else {
      params.rc.i_vbv_buffer_size = params.rc.i_bitrate;
    }
    if(pXcode->out[outidx].cfgVBVMaxRate > 1000) {
      params.rc.i_vbv_max_bitrate = pXcode->out[outidx].cfgVBVMaxRate / 1000;
    } else {
      params.rc.i_vbv_max_bitrate = params.rc.i_bitrate;
    }
    if(pXcode->out[outidx].cfgVBVMinRate > 0) {
      // no vbv min rate 
    }
    if(pXcode->out[outidx].cfgVBVAggressivity > 0) {
      // n/a
    }

    if(pXcode->out[outidx].cfgRateCtrl == RATE_CTRL_TYPE_CRF) {
      params.rc.i_rc_method  = X264_RC_CRF;
      params.rc.f_rf_constant = pXcode->out[outidx].cfgQ;
      params.rc.f_rf_constant_max = pXcode->out[outidx].cfgQMax;
    } else if (pXcode->out[outidx].cfgRateCtrl == RATE_CTRL_TYPE_CQP) {
      params.rc.i_rc_method = X264_RC_CQP;
      params.rc.i_qp_constant = pXcode->out[outidx].cfgQ;
    }

    params.rc.i_qp_max = pXcode->out[outidx].cfgQMax;
 
    get_gop(pXcode, outidx, &params.i_keyint_min, &params.i_keyint_max);

    LOG(X_DEBUG("Doing h264 reconfig"));
    if(pXCtx->fReconfig(pXCtx->enc, &params) != 0) {
      return -1;
    }

    pXCtx->fEncoderParameters(pXCtx->enc, &params);

  }

  return 0;
}

static int encode_nals(X264_AVCODEC_CTXT_T *pXCtx, uint8_t *buf, int size,
                       x264_nal_t *nals, int nnal, int skip_sei)
{
    uint8_t *p = buf;
    int i;

    /* Write the SEI as part of the first frame. */
    if (pXCtx->sei_size > 0 && nnal > 0) {
        if (pXCtx->sei_size > size) {
            return -1;
        }
        memcpy(p, pXCtx->sei, pXCtx->sei_size);
        p += pXCtx->sei_size;
        pXCtx->sei_size = 0;
        av_freep(&pXCtx->sei);
    }

    for (i = 0; i < nnal; i++){
        /* Don't put the SEI in extradata. */
        if (skip_sei && nals[i].i_type == NAL_SEI) {
            pXCtx->sei_size = nals[i].i_payload;
            pXCtx->sei      = av_malloc(pXCtx->sei_size);
            memcpy(pXCtx->sei, nals[i].p_payload, nals[i].i_payload);
            continue;
        }
        if (nals[i].i_payload > (size - (p - buf))) {
            // return only complete nals which fit in buf
            LOG(X_WARNING("xcoder_x264_encode nal buffer size %d too small for %d"), size, nals[i].i_payload);
            break;
        }
        memcpy(p, nals[i].p_payload, nals[i].i_payload);
        p += nals[i].i_payload;
    }

    return p - buf;
}

static int load_dynamic(X264_AVCODEC_CTXT_T *pXCtx) {
  int rc = 0;

#if defined(XCODE_HAVE_X264_DLOPEN) && (XCODE_HAVE_X264_DLOPEN > 0)

  const char *dlpath = NULL;

  if( !((dlpath = LIBX264_PATH) && (pXCtx->dlx264 = dlopen(dlpath, RTLD_LOCAL | RTLD_LAZY))) &&
      !((dlpath = LIBX264_NAME) && (pXCtx->dlx264 = dlopen(dlpath, RTLD_LOCAL | RTLD_LAZY)))) {
    LOG(X_ERROR("Unable to dlopen x264 encoder library: %s"), dlerror());
    return -1;
  }

  //LOG(X_DEBUG("Dynamically loaded encoder: %s"), dlpath);

  if(!(pXCtx->fClose = dlsym(pXCtx->dlx264, "x264_encoder_close"))) {
    LOG(X_ERROR("Unable to dlsym x264 function: %s"), dlerror());
    return -1;
  }

  if(!(pXCtx->fOpen = dlsym(pXCtx->dlx264, "x264_encoder_open_120"))) {
    LOG(X_ERROR("Unable to dlsym x264 function: %s"), dlerror());
    return -1;
  }

  if(!(pXCtx->fReconfig = dlsym(pXCtx->dlx264, "x264_encoder_reconfig"))) {
    LOG(X_ERROR("Unable to dlsym x264 function: %s"), dlerror());
    return -1;
  }

  if(!(pXCtx->fEncode = dlsym(pXCtx->dlx264, "x264_encoder_encode"))) {
    LOG(X_ERROR("Unable to dlsym x264 function: %s"), dlerror());
    return -1;
  }

  if(!(pXCtx->fParamDefault = dlsym(pXCtx->dlx264, "x264_param_default"))) {
    LOG(X_ERROR("Unable to dlsym x264 function: %s"), dlerror());
    return -1;
  }

  if(!(pXCtx->fParam2String = dlsym(pXCtx->dlx264, "x264_param2string"))) {
    LOG(X_ERROR("Unable to dlsym x264 function: %s"), dlerror());
    return -1;
  }

  if(!(pXCtx->fEncoderParameters = dlsym(pXCtx->dlx264, "x264_encoder_parameters"))) {
    LOG(X_ERROR("Unable to dlsym x264 function: %s"), dlerror());
    return -1;
  }

  if(!(pXCtx->fEncoderHeaders = dlsym(pXCtx->dlx264, "x264_encoder_headers"))) {
    LOG(X_ERROR("Unable to dlsym x264 function: %s"), dlerror());
    return -1;
  }

  if(!(pXCtx->fPictureInit = dlsym(pXCtx->dlx264, "x264_picture_init"))) {
    LOG(X_ERROR("Unable to dlsym x264 function: %s"), dlerror());
    return -1;
  }

  if(!(pXCtx->fDelayedFrames = dlsym(pXCtx->dlx264, "x264_encoder_delayed_frames"))) {
    LOG(X_ERROR("Unable to dlsym x264 function: %s"), dlerror());
    return -1;
  }

#else

  pXCtx->fOpen = x264_encoder_open;
  pXCtx->fClose = x264_encoder_close;
  pXCtx->fParam2String = x264_param2string;
  pXCtx->fEncode = x264_encoder_encode;
  pXCtx->fReconfig = x264_encoder_reconfig;
  pXCtx->fEncoderParameters = x264_encoder_parameters;
  pXCtx->fParamDefault = x264_param_default;
  pXCtx->fEncoderHeaders = x264_encoder_headers;
  pXCtx->fPictureInit = x264_picture_init;
  pXCtx->fDelayedFrames = x264_encoder_delayed_frames;

#endif // (XCODE_HAVE_X264_DLOPEN) && (XCODE_HAVE_X264_DLOPEN > 0)

  return rc;
}

int xcoder_x264_init(CODEC_WRAP_CTXT_T *pCtxt, unsigned int outidx) {
  int rc = 0;
  X264_AVCODEC_CTXT_T *pXCtx = NULL;
  IXCODE_VIDEO_CTXT_T *pXcode = NULL;

  if(!pCtxt || !pCtxt->pAVCtxt || !pCtxt->pXcode) {
    return -1;
  }

  pXcode = (IXCODE_VIDEO_CTXT_T *) pCtxt->pXcode;

  if(pXcode->common.cfgFileTypeOut != XC_CODEC_TYPE_H264) {
    return -1; 
  }

  if(((X264_AVCODEC_CTXT_T *) pCtxt->pctxt)) {
    // Already been initialized
    return xcoder_x264_reinit(pCtxt, outidx);
  }

  if(!(pXCtx = (X264_AVCODEC_CTXT_T *) pCtxt->pctxt)) {
    pCtxt->pctxt = calloc(1, sizeof(X264_AVCODEC_CTXT_T));
  }

  if(!(pXCtx = (X264_AVCODEC_CTXT_T *) pCtxt->pctxt)) {
    return -1;
  }

  if(pCtxt->pAVCtxt->out[outidx].dim_enc.width <= 0 || pCtxt->pAVCtxt->out[outidx].dim_enc.height <= 0) {
    LOG(X_ERROR("Encoder output dimensions not set"));
    return -1;
  }

  if(load_dynamic(pXCtx) < 0) {
    return -1;
  }

  //pCtxt->pAVCtxt->out[outidx].dim_enc.width = pXcode->out[outidx].resOutH;
  //pCtxt->pAVCtxt->out[outidx].dim_enc.height = pXcode->out[outidx].resOutV;

  pCtxt->pAVCtxt->out[outidx].dim_enc.pix_fmt = PIX_FMT_YUV420P;
    
  if(init_x264params(pCtxt, outidx) != 0) {
    return -1;
  }

#if 0
  char *s = pXCtx->fParam2String(&pXCtx->params, 1);
  if(s) {
    s[999] = '\0';
    fprintf(stderr, "x264 config:\n%s\n", s);
    free(s);
  }
#endif // 0

  if(!(pXCtx->enc = pXCtx->fOpen(&pXCtx->params))) {
    LOG(X_ERROR("x264_encoder_open failed"));
    return -1;
  }

  if(1) {
    x264_nal_t *nal;
    int nnal, sz;

    sz = pXCtx->fEncoderHeaders(pXCtx->enc, &nal, &nnal);

    pXCtx->extradata = av_malloc(sz);
    if((pXCtx->extradata_size = encode_nals(pXCtx, pXCtx->extradata, sz, nal, nnal, 1)) + 4 >
      sizeof(pXcode->out[outidx].hdr)) {
      LOG(X_ERROR("video encoder[%d] extra data size %d > %d"), outidx,
          pXCtx->extradata_size + 4, sizeof(pXcode->out[outidx].hdr));
      return -1;
    }

    pXcode->out[outidx].hdrLen = MIN(pXCtx->extradata_size, sizeof(pXcode->out[outidx].hdr));
    memcpy(pXcode->out[outidx].hdr, pXCtx->extradata, pXcode->out[outidx].hdrLen);

  }

  pXcode->out[outidx].qTot = 0;
  pXcode->out[outidx].qSamples = 0;

  return rc;
}


int xcoder_x264_encode(CODEC_WRAP_CTXT_T *pCtxt, 
                       unsigned int outidx,
                       const struct AVFrame *rawin, 
                       unsigned char *out, 
                       unsigned int outlen) {

  IXCODE_VIDEO_CTXT_T *pXcode = NULL;
  X264_AVCODEC_CTXT_T *pXCtx = NULL;
  int lenOut = 0;
  x264_nal_t *nal;
  int nnal;
  int i;
  x264_picture_t pic_out;
  int dts;

  if(!pCtxt || !(pXCtx = (X264_AVCODEC_CTXT_T *) pCtxt->pctxt) || !pXCtx->enc) {
    return -1;
  }

  pXcode = (IXCODE_VIDEO_CTXT_T *) pCtxt->pXcode;

  pXCtx->fPictureInit(&pXCtx->pic);

  pXCtx->pic.img.i_csp   = X264_CSP_I420;
  pXCtx->pic.img.i_plane = 3;

  if(rawin) {
    for (i = 0; i < pXCtx->pic.img.i_plane; i++) {
      pXCtx->pic.img.plane[i]    = rawin->data[i];
      pXCtx->pic.img.i_stride[i] = rawin->linesize[i];
    }

    pXCtx->pic.i_pts  = rawin->pts;

    if(pXcode->out[outidx].cfgForceIDR) {
      pXCtx->pic.i_type = X264_TYPE_KEYFRAME;
      pXcode->out[outidx].cfgForceIDR = 0;
    } else {
      pXCtx->pic.i_type = X264_TYPE_AUTO;
    }
    if(pXCtx->params.b_tff != rawin->top_field_first) {
      pXCtx->params.b_tff = rawin->top_field_first;
      pXCtx->fReconfig(pXCtx->enc, &pXCtx->params);
    }

/*
    //TODO: should be 1,1
    if (pXCtx->params.vui.i_sar_height != ctx->sample_aspect_ratio.den
         || x4->params.vui.i_sar_width != ctx->sample_aspect_ratio.num) {
            x4->params.vui.i_sar_height = ctx->sample_aspect_ratio.den;
            x4->params.vui.i_sar_width = ctx->sample_aspect_ratio.num;
            //x264_encoder_reconfig(x4->enc, &x4->params);
            pXCtx->fReconfig(x4->enc, &x4->params);
        }
    }
*/

  do {

    lenOut = outlen;
    if(pXCtx->fEncode(pXCtx->enc, &nal, &nnal, rawin ? &pXCtx->pic: NULL, &pic_out) < 0) {
      LOG(X_ERROR("x264_encoder_encode failed"));
      return -1;
    }

     if((lenOut = encode_nals(pXCtx, out, lenOut, nal, nnal, 0)) < 0) {
        return -1;
     }

  } while (!lenOut && !rawin && pXCtx->fDelayedFrames(pXCtx->enc));

  switch (pic_out.i_type) {
    case X264_TYPE_IDR  :
    case X264_TYPE_I:
      pXcode->out[outidx].frameType = FRAME_TYPE_I;
      break;
    case X264_TYPE_P:
      pXcode->out[outidx].frameType = FRAME_TYPE_P;
      break;
    case X264_TYPE_B:
    case X264_TYPE_BREF:
      pXcode->out[outidx].frameType = FRAME_TYPE_B;
      break;
    default:
      pXcode->out[outidx].frameType = FRAME_TYPE_UNKNOWN;
      break;
  }

  //x4->out_pic.key_frame = pic_out.b_keyframe;
  if(lenOut > 0)
    pXcode->out[outidx].frameQ = pic_out.i_qpplus1 - 1;
    pXcode->out[outidx].qTot += pXcode->out[outidx].frameQ;
    pXcode->out[outidx].qSamples++;
  }
  pXcode->out[outidx].pts = pic_out.i_pts;

  if((dts = (int) (pic_out.i_pts - pic_out.i_dts)) != 0) {
    pXcode->out[outidx].dts = - dts;
  } else {
    pXcode->out[outidx].dts = 0;
  }

  //fprintf(stderr, "ENCODER threads:%d\n",  pXCtx->params.i_threads);
  //fprintf(stderr, "x264 frtype:%d, pts:%lld, dts:%lld\n", pXcode->out[outidx].frameType, pXcode->out[outidx].pts, pXcode->out[outidx].dts);

  return lenOut;
}


