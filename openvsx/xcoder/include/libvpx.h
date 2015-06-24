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

#ifndef __LIBVPX_EXT_H__
#define __LIBVPX_EXT_H__

#ifdef ENABLE_XCODE

#include "libavcodec/avcodec.h"
#include "vpx/vpx_encoder.h"
#include "vpx/vp8cx.h"

struct IXCODE_VIDEO_OUT;

typedef struct _VP8Context {
#if LIBAVCODEC_VERSION_MAJOR >= 53
    AVClass        *class;
#endif
    struct IXCODE_VIDEO_OUT *pXcodeOut;
    struct vpx_codec_ctx encoder;
    struct vpx_codec_enc_cfg enccfg;
    struct vpx_image rawimg;
    struct vpx_fixed_buf twopass_stats;
    int deadline; //i.e., RT/GOOD/BEST
    struct FrameListData *coded_frame_list;

    int cpu_used;
    /**
     * VP8 specific flags, see VP8F_* below.
     */
    int flags;
#define VP8F_ERROR_RESILIENT 0x00000001 ///< Enable measures appropriate for streaming over lossy links
#define VP8F_AUTO_ALT_REF    0x00000002 ///< Enable automatic alternate reference frame generation

    int auto_alt_ref;

    int arnr_max_frames;
    int arnr_strength;
    int arnr_type;

    int lag_in_frames;
    int error_resilient;
    int crf;
    int is_init;

} VP8Context;


#endif // ENABLE_XCODE

#endif // __LIBVPX_EXT_H__
