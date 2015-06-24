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



// THIS FILE HAS BEEN DEPRECATED IN FAVOR OF xcoder_x264.c utilizing libx264.so with dlopen

#ifdef ENABLE_XCODE


#include "xcodeconfig.h"

#if defined(XCODE_HAVE_X264_AVCODEC) && (XCODE_HAVE_X264_AVCODEC > 0)

#include "libx264.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

extern char *x264_param2string( x264_param_t *p, int b_res );

static void X264_log(void *p, int level, const char *fmt, va_list args)
{
    static const int level_map[] = {
        [X264_LOG_ERROR]   = AV_LOG_ERROR,
        [X264_LOG_WARNING] = AV_LOG_WARNING,
        [X264_LOG_INFO]    = AV_LOG_INFO,
        [X264_LOG_DEBUG]   = AV_LOG_DEBUG
    };

    if (level < 0 || level > X264_LOG_DEBUG)
    //if (level < 0 || level >= X264_LOG_INFO)
        return;

    av_vlog(p, level_map[level], fmt, args);
}

#if X264_BUILD >= 76
static int encode_nals(AVCodecContext *ctx, uint8_t *buf, int size,
                       x264_nal_t *nals, int nnal, int skip_sei)
{
    X264Context *x4 = ctx->priv_data;
    uint8_t *p = buf;
    int i;

    //av_log(ctx, AV_LOG_ERROR, "encode_nals size:%d, nnal:%d, skip_sei:%d, sei_size:%d, nal[0].i_payload:%d\n", size, nnal, skip_sei, x4->sei_size, nnal > 0 ? nals[0].i_payload : -99);
    //if(nnal>0) av_log(ctx, AV_LOG_ERROR,"0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", ((unsigned char *)nals[0].p_payload)[0], ((unsigned char *)nals[0].p_payload)[1], ((unsigned char *)nals[0].p_payload)[2],((unsigned char *)nals[0].p_payload)[3],((unsigned char *)nals[0].p_payload)[4],((unsigned char *)nals[0].p_payload)[5]);

    /* Write the SEI as part of the first frame. */
    if (x4->sei_size > 0 && nnal > 0) {
        if (x4->sei_size > size) {
            av_log(ctx, AV_LOG_ERROR, "Error: nal buffer is too small\n");
            return -1;
        }

        //if(x4->sei[2] != 0x00) {  // The SEI has only 3 annexB header bytes instead of 4
        //    *p = 0x00;
        //    p++;
        //} 
        memcpy(p, x4->sei, x4->sei_size);
        //av_log(ctx, AV_LOG_ERROR,"copying sei size:%d into index[%d], 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", x4->sei_size, p - buf, p[0], p[1], p[2], p[3], p[4], p[5]);
        p += x4->sei_size;


        x4->sei_size = 0;
        av_freep(&x4->sei);
    }

    for (i = 0; i < nnal; i++){
        /* Don't put the SEI in extradata. */
        if (skip_sei && nals[i].i_type == NAL_SEI) {
            x4->sei_size = nals[i].i_payload;
            x4->sei      = av_malloc(x4->sei_size);
            memcpy(x4->sei, nals[i].p_payload, nals[i].i_payload);
            continue;
        }
        if (nals[i].i_payload > (size - (p - buf))) {
            // return only complete nals which fit in buf
            av_log(ctx, AV_LOG_ERROR, "Error: nal buffer is too small\n");
            break;
        }
        memcpy(p, nals[i].p_payload, nals[i].i_payload);
//av_log(ctx, AV_LOG_ERROR,"copying nal size:%d into index[%d], 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x\n", nals[i].i_payload, (p - buf),p[0], p[1], p[2], p[3], p[4], p[5]);
        p += nals[i].i_payload;
    }

    return p - buf;
}
#else
static int encode_nals(AVCodecContext *ctx, uint8_t *buf, int size, x264_nal_t *nals, int nnal, int skip_sei)
{
    X264Context *x4 = ctx->priv_data;
    uint8_t *p = buf;
    int i, s;

    /* Write the SEI as part of the first frame. */
    if (x4->sei_size > 0 && nnal > 0) {
        memcpy(p, x4->sei, x4->sei_size);
        p += x4->sei_size;
        x4->sei_size = 0;
    }

    for (i = 0; i < nnal; i++) {
        /* Don't put the SEI in extradata. */
        if (skip_sei && nals[i].i_type == NAL_SEI) {
            x4->sei = av_malloc( 5 + nals[i].i_payload * 4 / 3 );
            if(x264_nal_encode(x4->sei, &x4->sei_size, 1, nals + i) < 0)
                return -1;
            continue;
        }
        s = x264_nal_encode(p, &size, 1, nals + i);
        if (s < 0)
            return -1;
        p += s;
    }

    return p - buf;
}
#endif

static int X264ext_frame(AVCodecContext *ctx, uint8_t *buf,
                      int orig_bufsize, void *data)
{
    X264Context *x4 = ctx->priv_data;
    AVFrame *frame = data;
    x264_nal_t *nal;
    int nnal, i;
    x264_picture_t pic_out;
    int bufsize;

    x264_picture_init( &x4->pic );
    x4->pic.img.i_csp   = X264_CSP_I420;
    x4->pic.img.i_plane = 3;

    if (frame) {
        for (i = 0; i < x4->pic.img.i_plane; i++) {
            x4->pic.img.plane[i]    = frame->data[i];
            x4->pic.img.i_stride[i] = frame->linesize[i];
        }

        x4->pic.i_pts  = frame->pts;
        x4->pic.i_type =
            frame->pict_type == AV_PICTURE_TYPE_I ? X264_TYPE_KEYFRAME :
            frame->pict_type == AV_PICTURE_TYPE_P ? X264_TYPE_P :
            frame->pict_type == AV_PICTURE_TYPE_B ? X264_TYPE_B :
                                            X264_TYPE_AUTO;

        if (x4->params.b_tff != frame->top_field_first) {
            x4->params.b_tff = frame->top_field_first;
            x264_encoder_reconfig(x4->enc, &x4->params);
        }
        if (x4->params.vui.i_sar_height != ctx->sample_aspect_ratio.den
         || x4->params.vui.i_sar_width != ctx->sample_aspect_ratio.num) {
            x4->params.vui.i_sar_height = ctx->sample_aspect_ratio.den;
            x4->params.vui.i_sar_width = ctx->sample_aspect_ratio.num;
            x264_encoder_reconfig(x4->enc, &x4->params);
        }
    }
//static s_gg2;  if(s_gg2++ %10==1) x4->pic.i_type = X264_TYPE_IDR;
    do {
        //fprintf(stderr, "calling encoder_encode intype:%d outtype:%d\n", x4->pic.i_type, pic_out.i_type);
        bufsize = orig_bufsize;
        if (x264_encoder_encode(x4->enc, &nal, &nnal, frame? &x4->pic: NULL, &pic_out) < 0)
            return -1;

         bufsize = encode_nals(ctx, buf, bufsize, nal, nnal, 0);
         if (bufsize < 0)
            return -1;
    } while (!bufsize && !frame && x264_encoder_delayed_frames(x4->enc));

    /* FIXME: libx264 now provides DTS, but AVFrame doesn't have a field for it. */
    x4->out_pic.pts = pic_out.i_pts;

    switch (pic_out.i_type) {
    case X264_TYPE_IDR:
    case X264_TYPE_I:
        x4->out_pic.pict_type = AV_PICTURE_TYPE_I;
        break;
    case X264_TYPE_P:
        x4->out_pic.pict_type = AV_PICTURE_TYPE_P;
        break;
    case X264_TYPE_B:
    case X264_TYPE_BREF:
        x4->out_pic.pict_type = AV_PICTURE_TYPE_B;
        break;
    }

    x4->out_pic.key_frame = pic_out.b_keyframe;
    if (bufsize)
        x4->out_pic.quality = (pic_out.i_qpplus1 - 1) * FF_QP2LAMBDA;

    ctx->coded_frame->pts = pic_out.i_pts;
    ctx->coded_frame->display_picture_number = (int) (pic_out.i_pts - pic_out.i_dts);
    ctx->coded_frame->coded_picture_number = 0;
    //fprintf(stderr, "x264 pts:%lld, dts:%lld\n", pic_out.i_pts, pic_out.i_dts);
    //fprintf(stderr, "encoder_encode intype:%d outtype:%d, bufsize:%d, i_keyint_min:%d, i_keyint_max:%d\n", x4->pic.i_type, pic_out.i_type, bufsize, x4->params.i_keyint_min, x4->params.i_keyint_max);

    return bufsize;
}

static av_cold int X264ext_close(AVCodecContext *avctx)
{
    X264Context *x4 = avctx->priv_data;

    av_freep(&avctx->extradata);
    av_free(x4->sei);

    if (x4->enc) {
        x264_encoder_close(x4->enc);
        x4->enc = NULL;
    }

    return 0;
}

static av_cold int X264ext_reinit(AVCodecContext *avctx)
{
    X264Context *x4 = avctx->priv_data;
    x264_param_t params;

    if(!x4->enc)
        return -1;

    memset(&params, 0, sizeof(params));
    x264_encoder_parameters(x4->enc, &params);

    avctx->rc_buffer_size = avctx->bit_rate;
    avctx->rc_max_rate = avctx->rc_buffer_size;

    params.rc.i_bitrate         = avctx->bit_rate      / 1000;
    params.rc.i_vbv_buffer_size = avctx->rc_buffer_size / 1000;
    params.rc.i_vbv_max_bitrate = avctx->rc_max_rate    / 1000;

    if (avctx->crf) {
        params.rc.i_rc_method   = X264_RC_CRF;
        params.rc.f_rf_constant = avctx->crf;
        params.rc.f_rf_constant_max = avctx->qmax;
    } else if (avctx->cqp > -1) {
        params.rc.i_rc_method   = X264_RC_CQP;
        params.rc.i_qp_constant = avctx->cqp;
    }

    params.rc.i_qp_max          = avctx->qmax;

    params.i_keyint_max         = avctx->gop_size;
    params.i_keyint_min         = avctx->keyint_min;
    if (params.i_keyint_min > params.i_keyint_max)
        params.i_keyint_min = params.i_keyint_max;

    //fprintf(stderr, "RECONFIG BITRATE:%d, method:%d, crf:%f/%f(%f), cqp:%d(%d), qmax:%d(%d)\n", params.rc.i_bitrate, params.rc.i_rc_method, params.rc.f_rf_constant, avctx->crf, params.rc.f_rf_constant_max, params.rc.i_qp_constant, avctx->cqp, params.rc.i_qp_max, avctx->qmax);

    //x264_encoder_intra_refresh(x4->enc);
    if(x264_encoder_reconfig(x4->enc, &params) != 0) 
        return -1;

    x264_encoder_parameters(x4->enc, &params);

    //fprintf(stderr, "RETURN BITRATE:%d, method:%d, crf:%f/%f(%f), cqp:%d(%d), qmax;%d(%d)\n", params.rc.i_bitrate, params.rc.i_rc_method, params.rc.f_rf_constant, params.rc.f_rf_constant_max, avctx->crf, params.rc.i_qp_constant, avctx->cqp, params.rc.i_qp_max, avctx->qmax);

    return 0;
}

#define OPT_STR(opt, param)                                                   \
    do {                                                                      \
        int ret;                                                              \
        if (param && (ret = x264_param_parse(&x4->params, opt, param)) < 0) { \
            if(ret == X264_PARAM_BAD_NAME)                                    \
                av_log(avctx, AV_LOG_ERROR,                                   \
                        "bad option '%s': '%s'\n", opt, param);               \
            else                                                              \
                av_log(avctx, AV_LOG_ERROR,                                   \
                        "bad value for '%s': '%s'\n", opt, param);            \
            return -1;                                                        \
        }                                                                     \
    } while (0)

#define PARSE_X264_OPT(name, var)\
    if (x4->var && x264_param_parse(&x4->params, name, x4->var) < 0) {\
        av_log(avctx, AV_LOG_ERROR, "Error parsing option '%s' with value '%s'.\n", name, x4->var);\
        return AVERROR(EINVAL);\
    }



static av_cold int X264ext_init(AVCodecContext *avctx)
{

    X264Context *x4 = avctx->priv_data;
    int sw,sh;

    if (x4->enc) {
        return X264ext_reinit(avctx);
    }

    x264_param_default(&x4->params);

    //fprintf(stderr, "X264 THREADS:%d lookahead:%d, max_b:%d, has_b:%d(x264:%d)\n", x4->params.i_threads, x4->params.rc.i_lookahead, avctx->max_b_frames, avctx->has_b_frames, x4->params.i_bframe);

    x4->params.b_deblocking_filter         = avctx->flags & CODEC_FLAG_LOOP_FILTER;

    x4->params.rc.f_ip_factor             = 1 / fabs(avctx->i_quant_factor);
    x4->params.rc.f_pb_factor             = avctx->b_quant_factor;
    x4->params.analyse.i_chroma_qp_offset = avctx->chromaoffset;

    if (avctx->level > 0)
        x4->params.i_level_idc = avctx->level;

    x4->params.pf_log               = X264_log;
    x4->params.p_log_private        = avctx;
    x4->params.i_log_level          = X264_LOG_DEBUG;
    x4->params.i_csp                = X264_CSP_I420;

    //OPT_STR("weightp", x4->wpredp);

    if (avctx->bit_rate) {
        x4->params.rc.i_bitrate   = avctx->bit_rate / 1000;
        x4->params.rc.i_rc_method = X264_RC_ABR;
    }
    x4->params.rc.i_vbv_buffer_size = avctx->rc_buffer_size / 1000;
    x4->params.rc.i_vbv_max_bitrate = avctx->rc_max_rate    / 1000;
    x4->params.rc.b_stat_write      = avctx->flags & CODEC_FLAG_PASS1;
    if (avctx->flags & CODEC_FLAG_PASS2) {
        x4->params.rc.b_stat_read = 1;
    } else {
#if FF_API_X264_GLOBAL_OPTS
        if (avctx->crf) {
            x4->params.rc.i_rc_method   = X264_RC_CRF;
            x4->params.rc.f_rf_constant = avctx->crf;
            x4->params.rc.f_rf_constant_max = avctx->crf_max;
        } else if (avctx->cqp > -1) {
            x4->params.rc.i_rc_method   = X264_RC_CQP;
            x4->params.rc.i_qp_constant = avctx->cqp;
        }
#endif

    }

    //OPT_STR("stats", x4->stats);

    //fprintf(stderr, "CONFIG BITRATE:%d, vbv_buffer_size:%d, vbv_max_bitrate:%d, crf:%f, cqp:%d\n", x4->params.rc.i_bitrate, x4->params.rc.i_vbv_buffer_size, x4->params.rc.i_vbv_max_bitrate, avctx->crf, avctx->cqp);

    if (avctx->rc_buffer_size && avctx->rc_initial_buffer_occupancy &&
        (avctx->rc_initial_buffer_occupancy <= avctx->rc_buffer_size)) {
        x4->params.rc.f_vbv_buffer_init =
            (float)avctx->rc_initial_buffer_occupancy / avctx->rc_buffer_size;
    }

/*
    OPT_STR("level", x4->level);

    if(x4->x264opts){
        const char *p= x4->x264opts;
        while(p){
            char param[256]={0}, val[256]={0};
            if(sscanf(p, "%255[^:=]=%255[^:]", param, val) == 1){
                OPT_STR(param, "1");
            }else
                OPT_STR(param, val);
            p= strchr(p, ':');
            p+=!!p;
        }
    }
*/

#if FF_API_X264_GLOBAL_OPTS
    if (avctx->aq_mode >= 0)
        x4->params.rc.i_aq_mode = avctx->aq_mode;
    if (avctx->aq_strength >= 0)
        x4->params.rc.f_aq_strength = avctx->aq_strength;
    if (avctx->psy_rd >= 0)
        x4->params.analyse.f_psy_rd           = avctx->psy_rd;
    if (avctx->psy_trellis >= 0)
        x4->params.analyse.f_psy_trellis      = avctx->psy_trellis;
    if (avctx->rc_lookahead >= 0)
        x4->params.rc.i_lookahead             = avctx->rc_lookahead;
    if (avctx->weighted_p_pred >= 0)
        x4->params.analyse.i_weighted_pred    = avctx->weighted_p_pred;
    if (avctx->bframebias)
        x4->params.i_bframe_bias              = avctx->bframebias;
    if (avctx->deblockalpha)
        x4->params.i_deblocking_filter_alphac0 = avctx->deblockalpha;
    if (avctx->deblockbeta)
        x4->params.i_deblocking_filter_beta    = avctx->deblockbeta;
    if (avctx->complexityblur >= 0)
        x4->params.rc.f_complexity_blur        = avctx->complexityblur;
    if (avctx->directpred >= 0)
        x4->params.analyse.i_direct_mv_pred    = avctx->directpred;
    if (avctx->partitions) {
        if (avctx->partitions & X264_PART_I4X4)
            x4->params.analyse.inter |= X264_ANALYSE_I4x4;
        if (avctx->partitions & X264_PART_I8X8)
            x4->params.analyse.inter |= X264_ANALYSE_I8x8;
        if (avctx->partitions & X264_PART_P8X8)
            x4->params.analyse.inter |= X264_ANALYSE_PSUB16x16;
        if (avctx->partitions & X264_PART_P4X4)
            x4->params.analyse.inter |= X264_ANALYSE_PSUB8x8;
        if (avctx->partitions & X264_PART_B8X8)
            x4->params.analyse.inter |= X264_ANALYSE_BSUB16x16;
    }
    if (avctx->flags2) {
        x4->params.analyse.b_ssim = avctx->flags2 & CODEC_FLAG2_SSIM;
        x4->params.b_intra_refresh = avctx->flags2 & CODEC_FLAG2_INTRA_REFRESH;
        x4->params.i_bframe_pyramid = avctx->flags2 & CODEC_FLAG2_BPYRAMID ? X264_B_PYRAMID_NORMAL : X264_B_PYRAMID_NONE;
        x4->params.analyse.b_weighted_bipred  = avctx->flags2 & CODEC_FLAG2_WPRED;
        x4->params.analyse.b_mixed_references = avctx->flags2 & CODEC_FLAG2_MIXED_REFS;
        x4->params.analyse.b_transform_8x8    = avctx->flags2 & CODEC_FLAG2_8X8DCT;
        x4->params.analyse.b_fast_pskip       = avctx->flags2 & CODEC_FLAG2_FASTPSKIP;
        x4->params.b_aud                      = avctx->flags2 & CODEC_FLAG2_AUD;
        x4->params.analyse.b_psy              = avctx->flags2 & CODEC_FLAG2_PSY;
        x4->params.rc.b_mb_tree               = !!(avctx->flags2 & CODEC_FLAG2_MBTREE);
    }
#endif

    if (avctx->me_method == ME_EPZS)
        x4->params.analyse.i_me_method = X264_ME_DIA;
    else if (avctx->me_method == ME_HEX)
        x4->params.analyse.i_me_method = X264_ME_HEX;
    else if (avctx->me_method == ME_UMH)
        x4->params.analyse.i_me_method = X264_ME_UMH;
    else if (avctx->me_method == ME_FULL)
        x4->params.analyse.i_me_method = X264_ME_ESA;
    else if (avctx->me_method == ME_TESA)
        x4->params.analyse.i_me_method = X264_ME_TESA;

    if (avctx->gop_size >= 0)
        x4->params.i_keyint_max         = avctx->gop_size;
    if (avctx->max_b_frames >= 0)
        x4->params.i_bframe             = avctx->max_b_frames;
    if (avctx->scenechange_threshold >= 0)
        x4->params.i_scenecut_threshold = avctx->scenechange_threshold;
    if (avctx->qmin >= 0)
        x4->params.rc.i_qp_min          = avctx->qmin;
    if (avctx->qmax >= 0)
        x4->params.rc.i_qp_max          = avctx->qmax;
    if (avctx->max_qdiff >= 0)
        x4->params.rc.i_qp_step         = avctx->max_qdiff;
    if (avctx->qblur >= 0)
        x4->params.rc.f_qblur           = avctx->qblur;     /* temporally blur quants */
    if (avctx->qcompress >= 0)
        x4->params.rc.f_qcompress       = avctx->qcompress; /* 0.0 => cbr, 1.0 => constant qp */
    if (avctx->refs >= 0)
        x4->params.i_frame_reference    = avctx->refs;
    if (avctx->trellis >= 0)
        x4->params.analyse.i_trellis    = avctx->trellis;
    if (avctx->me_range >= 0)
        x4->params.analyse.i_me_range   = avctx->me_range;
    if (avctx->noise_reduction >= 0)
        x4->params.analyse.i_noise_reduction = avctx->noise_reduction;
    if (avctx->me_subpel_quality >= 0)
        x4->params.analyse.i_subpel_refine   = avctx->me_subpel_quality;
    if (avctx->b_frame_strategy >= 0)
        x4->params.i_bframe_adaptive = avctx->b_frame_strategy;
    if (avctx->keyint_min >= 0)
        x4->params.i_keyint_min = avctx->keyint_min;
    if (avctx->coder_type >= 0)
        x4->params.b_cabac = avctx->coder_type == FF_CODER_TYPE_AC;
    if (avctx->me_cmp >= 0)
        x4->params.analyse.b_chroma_me = avctx->me_cmp & FF_CMP_CHROMA;

    //if (x4->aq_mode >= 0)
    //    x4->params.rc.i_aq_mode = x4->aq_mode;
    //if (x4->aq_strength >= 0)
    //    x4->params.rc.f_aq_strength = x4->aq_strength;
    //PARSE_X264_OPT("psy-rd", psy_rd);
    //PARSE_X264_OPT("deblock", deblock);
    //PARSE_X264_OPT("partitions", partitions);
    //PARSE_X264_OPT("stats", stats);
    //if (x4->psy >= 0)
    //    x4->params.analyse.b_psy  = x4->psy;
    if (x4->lookaheadmin1 > 0)
        x4->params.rc.i_lookahead = x4->lookaheadmin1 - 1;
    //if (x4->weightp >= 0)
    //    x4->params.analyse.i_weighted_pred = x4->weightp;
    //if (x4->weightb >= 0)
    //    x4->params.analyse.b_weighted_bipred = x4->weightb;
    //if (x4->cplxblur >= 0)
    //    x4->params.rc.f_complexity_blur = x4->cplxblur;

    //if (x4->ssim >= 0)
    //    x4->params.analyse.b_ssim = x4->ssim;
    //if (x4->intra_refresh >= 0)
    //    x4->params.b_intra_refresh = x4->intra_refresh;
    //if (x4->b_bias != INT_MIN)
    //    x4->params.i_bframe_bias              = x4->b_bias;
    //if (x4->b_pyramid >= 0)
    //    x4->params.i_bframe_pyramid = x4->b_pyramid;
    //if (x4->mixed_refs >= 0)
    //    x4->params.analyse.b_mixed_references = x4->mixed_refs;
    //if (x4->dct8x8 >= 0)
    //    x4->params.analyse.b_transform_8x8    = x4->dct8x8;
    //if (x4->fast_pskip >= 0)
    //    x4->params.analyse.b_fast_pskip       = x4->fast_pskip;
    //if (x4->aud >= 0)
    //    x4->params.b_aud                      = x4->aud;
    //if (x4->mbtree >= 0)
    //    x4->params.rc.b_mb_tree               = x4->mbtree;
    //if (x4->direct_pred >= 0)
    //    x4->params.analyse.i_direct_mv_pred   = x4->direct_pred;

    if (x4->i_slice_max_size >= 0)
        x4->params.i_slice_max_size =  x4->i_slice_max_size;

    //if (x4->fastfirstpass)
    //    x264_param_apply_fastfirstpass(&x4->params);

    //if (x4->profile)
    //    if (x264_param_apply_profile(&x4->params, x4->profile) < 0) {
    //        av_log(avctx, AV_LOG_ERROR, "Error setting profile %s.\n", x4->profile);
    //        return AVERROR(EINVAL);
    //    }

    x4->params.i_width          = avctx->width;
    x4->params.i_height         = avctx->height;
    av_reduce(&sw, &sh, avctx->sample_aspect_ratio.num, avctx->sample_aspect_ratio.den, 4096);
    x4->params.vui.i_sar_width  = sw;
    x4->params.vui.i_sar_height = sh;
    x4->params.i_fps_num = x4->params.i_timebase_den = avctx->time_base.den;
    x4->params.i_fps_den = x4->params.i_timebase_num = avctx->time_base.num;

    x4->params.analyse.b_psnr = avctx->flags & CODEC_FLAG_PSNR;

    x4->params.i_threads      = avctx->thread_count;

    x4->params.b_interlaced   = avctx->flags & CODEC_FLAG_INTERLACED_DCT;

//    x4->params.b_open_gop     = !(avctx->flags & CODEC_FLAG_CLOSED_GOP);

    x4->params.i_slice_count  = avctx->slices;

    x4->params.vui.b_fullrange = avctx->pix_fmt == PIX_FMT_YUVJ420P;

    if (avctx->flags & CODEC_FLAG_GLOBAL_HEADER)
        x4->params.b_repeat_headers = 0;

    // update AVCodecContext with x264 parameters
    avctx->has_b_frames = x4->params.i_bframe ?
        x4->params.i_bframe_pyramid ? 2 : 1 : 0;
    if (avctx->max_b_frames < 0)
        avctx->max_b_frames = 0;

    //fprintf(stderr, "X264 THREADS:%d lookahead:%d, max_b:%d, has_b:%d(x264:%d)\n", x4->params.i_threads, x4->params.rc.i_lookahead, avctx->max_b_frames, avctx->has_b_frames, x4->params.i_bframe);

    avctx->bit_rate = x4->params.rc.i_bitrate*1000;
#if FF_API_X264_GLOBAL_OPTS
    avctx->crf = x4->params.rc.f_rf_constant;
    //fprintf(stderr, " GLOBAL O\n");
#else
    //fprintf(stderr, "NO GLOBAL O\n");
#endif

#if 0
  char *s = x264_param2string( &x4->params, 1);
  if(s) {
    s[999] = '\0';
    fprintf(stderr, "%s\n", s);
    free(s);
  }
#endif // 0

    x4->enc = x264_encoder_open(&x4->params);
    if (!x4->enc)
        return -1;

    avctx->coded_frame = &x4->out_pic;

    if (avctx->flags & CODEC_FLAG_GLOBAL_HEADER) {
        x264_nal_t *nal;
        int nnal, s, i;

        s = x264_encoder_headers(x4->enc, &nal, &nnal);

        for (i = 0; i < nnal; i++) {
            //if (nal[i].i_type == NAL_SEI) {
            //    av_log(avctx, AV_LOG_INFO, "%s\n", nal[i].p_payload+25);
            //}
        }

        avctx->extradata      = av_malloc(s);
        avctx->extradata_size = encode_nals(avctx, avctx->extradata, s, nal, nnal, 1);
    }

   return 0;
}



#include "libavutil/avutil.h"
#include "libavutil/opt.h"
#include "libavcodec/avcodec.h"

struct AVCodecDefault {
    const uint8_t *key;
    const uint8_t *value;
};

static const AVCodecDefault x264_defaults[] = {
    { "b",                "0" },
    { "bf",               "-1" },
    { "flags2",           "0" },
    { "g",                "-1" },
    { "qmin",             "-1" },
    { "qmax",             "-1" },
    { "qdiff",            "-1" },
    { "qblur",            "-1" },
    { "qcomp",            "-1" },
    { "rc_lookahead",     "-1" },
    { "refs",             "-1" },
    { "sc_threshold",     "-1" },
    { "trellis",          "-1" },
    { "nr",               "-1" },
    { "me_range",         "-1" },
    { "me_method",        "-1" },
    { "subq",             "-1" },
    { "b_strategy",       "-1" },
    { "keyint_min",       "-1" },
    { "coder",            "-1" },
    { "cmp",              "-1" },
    { "threads",          AV_STRINGIFY(X264_THREADS_AUTO) },
    { NULL },
};

#define OFFSET(x) offsetof(X264Context, x)
#define VE AV_OPT_FLAG_VIDEO_PARAM | AV_OPT_FLAG_ENCODING_PARAM
#define offsetof(T, F) ((unsigned int)((char *)&((T *)0)->F))
static const AVOption options[] = {
  { "", "",      OFFSET(i_slice_max_size), AV_OPT_TYPE_INT,    { 1 }, 0, 1, VE},
    { NULL }
};


static const AVClass class = {
    .class_name = "libx264",
    .item_name  = av_default_item_name,
    .option     = options,
    .version    = LIBAVUTIL_VERSION_INT,
};


static const enum PixelFormat pix_fmts_8bit[] = {
    PIX_FMT_YUV420P,
    PIX_FMT_YUVJ420P,
    PIX_FMT_YUV422P,
    PIX_FMT_YUV444P,
    PIX_FMT_NONE
};

static av_cold void X264_init_static(AVCodec *codec)
{
    //if (x264_bit_depth == 8)
        codec->pix_fmts = pix_fmts_8bit;
    //else if (x264_bit_depth == 9)
    //    codec->pix_fmts = pix_fmts_9bit;
    //else if (x264_bit_depth == 10)
    //    codec->pix_fmts = pix_fmts_10bit;
}

AVCodec libx264ext_encoder = {
    .name           = "libx264",
    .type           = AVMEDIA_TYPE_VIDEO,
    .id             = CODEC_ID_H264,
    .priv_data_size = sizeof(X264Context),
    .init           = X264ext_init,
    .encode         = X264ext_frame,
    .close          = X264ext_close,
    .capabilities   = CODEC_CAP_DELAY | CODEC_CAP_AUTO_THREADS,
    .long_name      = NULL,
    .priv_class     = &class,
    .defaults       = x264_defaults,
    .init_static_data = X264_init_static,
};


#endif // (XCODE_HAVE_X264_AVCODEC) && (XCODE_HAVE_X264_AVCODEC > 0)

#endif // ENABLE_XCODE

