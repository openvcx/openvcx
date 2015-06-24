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


#ifndef __H264_H__
#define __H264_H__

#include "vsxconfig.h"
#include "bits.h"

#define NAL_TYPE_MASK                   0x1f
#define NAL_NRI_MASK                    0x60

#define NAL_NRI_NONE                    0x00
#define NAL_NRI_LOW                     0x20
#define NAL_NRI_MED                     0x40
#define NAL_NRI_HI                      0x60


#define NAL_TYPE_NONSPECIFIC            0
#define NAL_TYPE_SLICE                  1
#define NAL_TYPE_PARTITION_2            2
#define NAL_TYPE_PARTITION_3            3
#define NAL_TYPE_PARTITION_4            4
#define NAL_TYPE_IDR                    5
#define NAL_TYPE_SEI                    6
#define NAL_TYPE_SEQ_PARAM              7
#define NAL_TYPE_PIC_PARAM              8
#define NAL_TYPE_ACCESS_UNIT_DELIM      9
#define NAL_TYPE_END_OF_SEQ             10
#define NAL_TYPE_END_OF_STREAM          11
#define NAL_TYPE_FILTER_DATA            12
#define NAL_TYPE_RESERVED_13            13
#define NAL_TYPE_RESERVED_14            14
#define NAL_TYPE_RESERVED_15            15
#define NAL_TYPE_RESERVED_16            16
#define NAL_TYPE_RESERVED_17            17
#define NAL_TYPE_RESERVED_18            18
#define NAL_TYPE_RESERVED_19            19
#define NAL_TYPE_RESERVED_20            20
#define NAL_TYPE_RESERVED_21            21
#define NAL_TYPE_RESERVED_22            22
#define NAL_TYPE_RESERVED_23            23
#define NAL_TYPE_STAP_A                 24    // Single-time aggregation packet 
#define NAL_TYPE_STAP_B                 25    // Single-time aggregation packet     5.7.1
#define NAL_TYPE_MTAP_16                26    // Multi-time aggregation packet      5.7.2
#define NAL_TYPE_MTAP_32                27    // Multi-time aggregation packet      5.7.2
#define NAL_TYPE_FRAG_A                 28    // Fragmentation unit                 5.8
#define NAL_TYPE_FRAG_B                 29    // Fragmentation unit                 5.8


#define H264_PROFILE_BASELINE           0x42
//#define H264_PROFILE_EXT                0x4d
//#define H264_PROFILE_MAIN               0x58
#define H264_PROFILE_MAIN               0x4d
#define H264_PROFILE_EXT                0x58
#define H264_PROFILE_HIGH               0x64 // YUV4:2:0 8bits/sample
#define H264_PROFILE_HIGH_10            0x6e // YUV4:2:0 10bits/sample
#define H264_PROFILE_HIGH_422           0x7a // YUV4:2:2 10bits/sample
#define H264_PROFILE_HIGH_444           0x90 // YUV4:4:4 12bits/sample
#define H264_PROFILE_HIGH_444_PRED      0xf4 // YUV4:4:4 12bits/sample 
#define H264_PROFILE_CAVLC_444          0x2c


#define H264_MAX_SPS_CNT          32
#define H264_MAX_PPS_CNT          256

#define H264_AUD_I                (0)
#define H264_AUD_I_P              (1 << 5)
#define H264_AUD_I_P_B            (2 << 5)
#define H264_AUD_SI               (3 << 5)
#define H264_AUD_SI_SP            (4 << 5) 
#define H264_AUD_I_SI             (5 << 5)
#define H264_AUD_I_SI_P_SP        (6 << 5)
#define H264_AUD_I_SI_P_SP_B      (7 << 5)


#define H264_SEI_BUFFERINGPERIOD        0
#define H264_SEI_PIC_TIMING             1
#define H264_SEI_USERDATA_UNREG         5
#define H264_SEI_RECOVERY_POINT         6

typedef struct H264_SPS_HRD_CPB {
  int hrd_bit_rate_val_min1;
  int hrd_cpb_size_val_min1;
  int hrd_cbr_flag;
} H264_SPS_HRD_CPB_T;

typedef struct H264_SPS_VUI {

  // video usability info
  int params_present_flag;
  int aspect_ratio_info_present;
  int aspect_ratio_idc;
  int aspect_ratio_sar_width;
  int aspect_ratio_sar_height;
  int overscan_info_present;
  int overscan_appropriate;
  int video_signal_type_present;
  int video_format;
  int video_full_range;
  int color_desc_present;
  int color_primaries;
  int color_transfer_characteristics;
  int color_matrix;
  int chroma_loc_info_present;
  int chroma_sample_top;
  int chroma_sample_bottom;
  int timing_info_present;
  int timing_num_units_in_tick;
  int timing_timescale;
  int timing_fixed_frame_rate;
  int nal_hrd_params;
  int vcl_hrd_params;
  int low_delay_hrd_flag;
  int hrd_cpb_count;
  int hrd_bit_rate_scale;
  int hrd_cpb_size_scale;
  H264_SPS_HRD_CPB_T hrd_cpb[32];
  int hrd_initial_cpb_removal_delay_len;
  int hrd_cpb_removal_delay_len;
  int hrd_dpb_output_delay_len;
  int hrd_time_offset_len;
  int pic_struct_present_flag;
  int bitstream_restriction;
  int motion_vec_over_pic_boundary;
  int max_bytes_per_pic_denom;
  int max_bits_per_pic_denom;
  int log2_max_mv_len_horiz;
  int log2_max_mv_len_vert;
  int num_reorder_frames;
  int max_dec_frame_buffering;

} H264_SPS_VUI_T;

typedef struct H264_NAL_SPS {

  int is_ok;

  uint8_t profile_id;
  uint8_t constraint;
  uint8_t level_id;
  int seq_param_set_id;
  int chroma_format_idc;
  int log2_max_frame_num;
  int pic_order_cnt_type;
  int pic_delta_pic_order;
  int pic_offset_non_ref_pic;
  int pic_offset_top_to_bottom;
  int pic_num_ref_frames_in_pic_order_cnt_cycle; 
  int pic_offset_for_ref_frame[32];
  int log2_max_pic_order_cnt_lsb;             
  int num_ref_frames;
  int gaps_in_frame_num_val_allowed_flag;
  int pic_width_in_mbs;
  int pic_height_in_mbs;
  int frame_mbs_only_flag;
  int mb_adaptive_frame_field_flag;
  int direct_8x8_inference_flag;
  int frame_cropping;
  int crop_left;
  int crop_right;
  int crop_top;
  int crop_bottom;
  H264_SPS_VUI_T vui;

  // Profile > 100
  int residual_color_transform;
  int bit_depth_chroma;
  int bit_depth_luma;
  int qprime_y_zero_transform_bypass;
  int scaling_matrix_present;

} H264_NAL_SPS_T;


typedef struct H264_NAL_PPS {

  int is_ok;

  int pic_param_set_id;
  unsigned int seq_param_set_id;
  int entropy_coding_mode_flag;   // CABAC
  int pic_order_present_flag;
  int num_slice_group_map_units;
  int slice_group_map_type;
  int num_ref_idx_l0_active;
  int num_ref_idx_l1_active;
  int weighted_pred_flag;
  int weighted_bipred_idx;
  int pic_init_qp;
  int pic_init_qs;
  int chroma_qp_index_offset;
  int deblocking_filter_control_present_flag;
  int constrained_intra_pred_flag;
  int redundant_pic_cnt_present_flag;

  // optional 8x8 transform follows

} H264_NAL_PPS_T;
  

typedef STREAM_CHUNK_T H264_STREAM_CHUNK_T;

// 0,5 P slice
// 1,6 B slice
// 2,7 I slice
// 3,8 SP slice
// 4,9 SI slice

enum H264_SLICE_TYPE {
  H264_SLICE_TYPE_P        =  0,
  H264_SLICE_TYPE_B        =  1,
  H264_SLICE_TYPE_I        =  2,
  H264_SLICE_TYPE_SP       =  3,
  H264_SLICE_TYPE_SI       =  4,
  H264_SLICE_TYPE_SPS      =  5,
  H264_SLICE_TYPE_PPS      =  6,
  H264_SLICE_TYPE_SEI      =  7,
  H264_SLICE_TYPE_UNKNOWN  =  8,
};

#define H264_DECODER_CTXT_HAVE_SPS      0x01
#define H264_DECODER_CTXT_HAVE_PPS      0x02
//#define H264_DECODER_CTXT_HAVE_IDR      0x04

#define H264_RC_ERR           -1
#define H264_RC_OK             0
#define H264_RC_IGNORED        1

typedef enum H264_RESULT {
  // anything lower than this is considered an error
  H264_RESULT_ERR             = 10,
  H264_RESULT_DECODED         = 11,
  H264_RESULT_IGNORED         = 12,
  H264_RESULT_CONTINUATION    = 13,
  H264_RESULT_SEI_DISCARD     = 14
} H264_RESULT_T;

typedef struct H264_DECODER_CTXT {

  H264_NAL_SPS_T sps[H264_MAX_SPS_CNT];
  H264_NAL_PPS_T pps[H264_MAX_PPS_CNT];

  unsigned int sps_idx;
  unsigned int pps_idx;
  int flag;

  unsigned int frameIdx; // from IDR
  unsigned int picOrderCnt;
  unsigned int sliceIdx; // in frame
  enum H264_SLICE_TYPE lastSliceType;

} H264_DECODER_CTXT_T;

#define H264_STREAM_INCR_BYTE(p) p->byteIdx++
#define H264_STREAM_ENDOF_BYTE(p) if(p->bitIdx > 0) {p->byteIdx++;} p->bitIdx = 0;

#define H264_INIT_BITPARSER(_bsst, _bufst, _lenst) \
                              memset(&_bsst, 0, sizeof(_bsst)); \
                              _bsst.buf = (unsigned char *) _bufst; \
                              _bsst.sz = _lenst; \
                              _bsst.emulPrevSeqLen = 3; \
                              _bsst.emulPrevSeq[2] = 0x03;

int h264_getCroppedDimensions(H264_NAL_SPS_T *pSps,
                              unsigned int *pwcropped, unsigned int *phcropped,
                              unsigned int *pworig, unsigned int *phorig);
int h264_getVUITiming(H264_NAL_SPS_T *pSps,
                      unsigned int *pClockHz, unsigned int *pFrameDeltaHz);
int h264_getNALType(unsigned char hdr);
int h264_isNALVcl(unsigned char hdr);
H264_RESULT_T h264_decode_NALHdr(H264_DECODER_CTXT_T *pCtxt, BIT_STREAM_T *pNal,
                                 unsigned int offsetFromStart);
H264_RESULT_T h264_decodeSEI(BIT_STREAM_T *pStream);
int h264_decodeSPS(BIT_STREAM_T *pStream, H264_NAL_SPS_T *pSps);
int h264_decodePPS(BIT_STREAM_T *pStream, H264_NAL_PPS_T *pPps);
int h264_encodeSPS(BIT_STREAM_T *pStream, H264_NAL_SPS_T *pSps);



#endif // __H264_H__
