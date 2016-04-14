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

static int h264_decodeSPSVUI_hrd(BIT_STREAM_T *pStream, H264_SPS_VUI_T *pVui) {
  int idx;

  if((pVui->hrd_cpb_count = bits_readExpGolomb(pStream) + 1) > 
    sizeof(pVui->hrd_cpb) / sizeof(pVui->hrd_cpb[0])) {
    LOG(X_ERROR("SPS VUI HRD parameter cpb_count %d exceeds %d"),
    pVui->hrd_cpb_count, sizeof(pVui->hrd_cpb) / sizeof(pVui->hrd_cpb[0]));
    return H264_RC_ERR;
  }
  pVui->hrd_bit_rate_scale = bits_read(pStream, 4);
  pVui->hrd_cpb_size_scale = bits_read(pStream, 4);
  for(idx = 0; idx < pVui->hrd_cpb_count; idx++) {
    pVui->hrd_cpb[idx].hrd_bit_rate_val_min1 = bits_readExpGolomb(pStream);
    pVui->hrd_cpb[idx].hrd_cpb_size_val_min1 = bits_readExpGolomb(pStream);
    pVui->hrd_cpb[idx].hrd_cbr_flag = bits_read(pStream, 1);
  }
  pVui->hrd_initial_cpb_removal_delay_len = bits_read(pStream, 5) + 1;
  pVui->hrd_cpb_removal_delay_len = bits_read(pStream, 5) + 1;
  pVui->hrd_dpb_output_delay_len = bits_read(pStream, 5) + 1;
  pVui->hrd_time_offset_len = bits_read(pStream, 5);

  return H264_RC_OK;
}

static int h264_encodeSPSVUI_hrd(BIT_STREAM_T *pStream, H264_SPS_VUI_T *pVui) {
  int idx;

  bits_writeExpGolomb(pStream, pVui->hrd_cpb_count - 1);
  bits_write(pStream, pVui->hrd_bit_rate_scale, 4);
  bits_write(pStream, pVui->hrd_cpb_size_scale, 4);
  for(idx = 0; idx < pVui->hrd_cpb_count; idx++) {
    bits_writeExpGolomb(pStream, pVui->hrd_cpb[idx].hrd_bit_rate_val_min1);
    bits_writeExpGolomb(pStream, pVui->hrd_cpb[idx].hrd_cpb_size_val_min1);
    bits_write(pStream, pVui->hrd_cpb[idx].hrd_cbr_flag, 1);
  }
  bits_write(pStream, pVui->hrd_initial_cpb_removal_delay_len-1, 5);
  bits_write(pStream, pVui->hrd_cpb_removal_delay_len-1, 5);
  bits_write(pStream, pVui->hrd_dpb_output_delay_len-1, 5);
  bits_write(pStream, pVui->hrd_time_offset_len, 5);

  return H264_RC_OK;
}

static int h264_decodeVUI(BIT_STREAM_T *pStream, H264_SPS_VUI_T *pVui) {

  if((pVui->aspect_ratio_info_present = bits_read(pStream, 1))) {
    if((pVui->aspect_ratio_idc = bits_read(pStream, 8)) == 255) {  // MPEG4-10 Table E-1
      pVui->aspect_ratio_sar_width = bits_read(pStream, 16);
      pVui->aspect_ratio_sar_height = bits_read(pStream, 16);
    }
  }
  if((pVui->overscan_info_present = bits_read(pStream, 1))) {
    pVui->overscan_appropriate = bits_read(pStream, 1);
  }

  if((pVui->video_signal_type_present = bits_read(pStream, 1))) {
    pVui->video_format = bits_read(pStream, 3); 
    pVui->video_full_range = bits_read(pStream, 1);  
    if((pVui->color_desc_present = bits_read(pStream, 1))) {
      pVui->color_primaries = bits_read(pStream, 8);  
      pVui->color_transfer_characteristics = bits_read(pStream, 8); 
      pVui->color_matrix = bits_read(pStream, 8);  
    }
  }
  
  if((pVui->chroma_loc_info_present = bits_read(pStream, 1))) {
    pVui->chroma_sample_top = bits_readExpGolomb(pStream);  
    pVui->chroma_sample_bottom = bits_readExpGolomb(pStream);  
  }

  if((pVui->timing_info_present = bits_read(pStream, 1))) {
    pVui->timing_num_units_in_tick = bits_read(pStream, 32);
    pVui->timing_timescale = bits_read(pStream, 32);
    pVui->timing_fixed_frame_rate = bits_read(pStream, 1);
  }

  if((pVui->nal_hrd_params = bits_read(pStream, 1))) {
    if(h264_decodeSPSVUI_hrd(pStream, pVui) < 0) {
      return H264_RC_ERR;
    }
  }

  if((pVui->vcl_hrd_params = bits_read(pStream, 1))) {
    if(h264_decodeSPSVUI_hrd(pStream, pVui) < 0) {
      return H264_RC_ERR;
    }
  }

  if(pVui->nal_hrd_params || pVui->vcl_hrd_params) {
    pVui->low_delay_hrd_flag = bits_read(pStream, 1);
  }

  pVui->pic_struct_present_flag = bits_read(pStream, 1);

  if((pVui->bitstream_restriction = bits_read(pStream, 1))) {
    pVui->motion_vec_over_pic_boundary = bits_read(pStream, 1);
    pVui->max_bytes_per_pic_denom = bits_readExpGolomb(pStream);
    pVui->max_bits_per_pic_denom = bits_readExpGolomb(pStream);
    pVui->log2_max_mv_len_horiz = bits_readExpGolomb(pStream);
    pVui->log2_max_mv_len_vert = bits_readExpGolomb(pStream);
    pVui->num_reorder_frames = bits_readExpGolomb(pStream);
    pVui->max_dec_frame_buffering = bits_readExpGolomb(pStream);
  } 
 
  return H264_RC_OK;
}

int h264_decodeSPS(BIT_STREAM_T *pStream, H264_NAL_SPS_T *pSps) {
  int idx;
  uint8_t profile_idc;
  uint8_t level_idc;
  uint8_t constraint;
  unsigned int sps_id;

  if(pStream->sz - pStream->byteIdx < 8) {
    LOG(X_ERROR("SPS length to decode below minimum: %d"), pStream->sz - pStream->byteIdx);
    return H264_RC_ERR;
  }

  profile_idc = bits_read(pStream, 8);
  constraint = bits_read(pStream, 8); // 3 bits constraints set , 5 bits reserved
  level_idc = bits_read(pStream, 8);

  if((sps_id = bits_readExpGolomb(pStream)) >= H264_MAX_SPS_CNT) {
    LOG(X_ERROR("SPS id is invalid:%d"), sps_id);
    return H264_RC_ERR;
  }

  memset(pSps, 0, sizeof(H264_NAL_SPS_T));
  pSps->profile_id = profile_idc;
  pSps->constraint = constraint;
  pSps->level_id = level_idc;
  pSps->seq_param_set_id = sps_id;

  if(profile_idc >= H264_PROFILE_HIGH) {

    if((pSps->chroma_format_idc = bits_readExpGolomb(pStream)) == 3) {
      pSps->residual_color_transform = bits_read(pStream, 1);
    } else if(pSps->chroma_format_idc > 3) {
      LOG(X_ERROR("SPS chroma_format out of range: %d"), pSps->chroma_format_idc);
      return H264_RC_ERR;
    }
    pSps->bit_depth_luma = bits_readExpGolomb(pStream) + 8;
    pSps->bit_depth_chroma = bits_readExpGolomb(pStream) + 8;
    pSps->qprime_y_zero_transform_bypass = bits_read(pStream, 1);
    pSps->scaling_matrix_present =  bits_read(pStream, 1);
    if(pSps->scaling_matrix_present) {
      LOG(X_ERROR("SPS scaling matrix decode not implemented for high profiles"));
      return H264_RC_ERR;
    }


  } else {
    pSps->chroma_format_idc = 1;
  }

  if((pSps->log2_max_frame_num = bits_readExpGolomb(pStream) + 4) < 4 || 
      pSps->log2_max_frame_num > 16) {
    LOG(X_ERROR("SPS log2_max_frame_num: %d out of range"), pSps->log2_max_frame_num);
    return H264_RC_ERR;
  }

  if((pSps->pic_order_cnt_type = bits_readExpGolomb(pStream)) == 0) {
    
    if((pSps->log2_max_pic_order_cnt_lsb = bits_readExpGolomb(pStream) + 4) < 4 || 
      pSps->log2_max_pic_order_cnt_lsb > 16) {
      LOG(X_ERROR("SPS log2_max_pic_order_cnt_lsb: %d out of range"), 
          pSps->log2_max_pic_order_cnt_lsb);
      return H264_RC_ERR;
    }

  } else if(pSps->pic_order_cnt_type == 1) {    
    pSps->pic_delta_pic_order = bits_read(pStream, 1);    
    pSps->pic_offset_non_ref_pic = bits_readExpGolomb(pStream); 
    pSps->pic_offset_top_to_bottom = bits_readExpGolomb(pStream);  
    pSps->pic_num_ref_frames_in_pic_order_cnt_cycle = bits_readExpGolomb(pStream);

    if(pSps->pic_num_ref_frames_in_pic_order_cnt_cycle > sizeof(pSps->pic_offset_for_ref_frame) / 
      sizeof(pSps->pic_offset_for_ref_frame[0])) {
      LOG(X_ERROR("SPS num_ref_frames_in_pic_order_cnt_cycle %d greater than %d."), 
        pSps->pic_num_ref_frames_in_pic_order_cnt_cycle,
        sizeof(pSps->pic_offset_for_ref_frame) / sizeof(pSps->pic_offset_for_ref_frame[0]));
      return H264_RC_ERR;
    }

    for(idx = 0; idx < pSps->pic_num_ref_frames_in_pic_order_cnt_cycle; idx++) {
      pSps->pic_offset_for_ref_frame[idx] = bits_readExpGolomb(pStream);
    }
   
  } else if(pSps->pic_order_cnt_type != 2) {
    LOG(X_ERROR("SPS pic_order_cnt_type:%d out of range"), pSps->pic_order_cnt_type);
    return H264_RC_ERR;
  } 
  
  pSps->num_ref_frames = bits_readExpGolomb(pStream);
  pSps->gaps_in_frame_num_val_allowed_flag = bits_read(pStream, 1);
  pSps->pic_width_in_mbs = bits_readExpGolomb(pStream) + 1;
  pSps->pic_height_in_mbs = bits_readExpGolomb(pStream) + 1;
  //fprintf(stderr, "RES:%dx%d\n", pSps->pic_width_in_mbs*16, pSps->pic_height_in_mbs*16);
  if((pSps->frame_mbs_only_flag = bits_read(pStream, 1)) == 0) {
    pSps->mb_adaptive_frame_field_flag = bits_read(pStream, 1);    
  }

  pSps->direct_8x8_inference_flag = bits_read(pStream, 1);
  if((pSps->frame_cropping = bits_read(pStream, 1))) {
    pSps->crop_left = bits_readExpGolomb(pStream);
    pSps->crop_right = bits_readExpGolomb(pStream);

    //
    // Crop should be no bigger than 1mb  after applying multiplier of 2px (or 4px for interlaced)
    //
    if((pSps->crop_top = bits_readExpGolomb(pStream)) > 7) {
      pSps->crop_top = 7;
    } else if(pSps->frame_mbs_only_flag == 0 && pSps->crop_top > 3) {
      pSps->crop_top = 3;
    }

    if((pSps->crop_bottom = bits_readExpGolomb(pStream)) > 7) {
      pSps->crop_bottom = 7;
    } else if(pSps->frame_mbs_only_flag == 0 && pSps->crop_bottom > 3) {
      pSps->crop_bottom = 3;
    }

  } else {
    pSps->crop_left = 0;
    pSps->crop_right = 0;
    pSps->crop_top = 0;
    pSps->crop_bottom = 0;
  }

  if((pSps->vui.params_present_flag = bits_read(pStream, 1))) {
    if(h264_decodeVUI(pStream, &pSps->vui) != 0) {
      return H264_RC_ERR;
    }
  }

  pSps->is_ok = 1;

  return H264_RC_OK;
}

int h264_encodeSPS(BIT_STREAM_T *pStream, H264_NAL_SPS_T *pSps) {

  int idx;

  bits_write(pStream, pSps->profile_id, 8);
  bits_write(pStream, 0, 8);
  bits_write(pStream, pSps->level_id, 8);
  bits_writeExpGolomb(pStream, pSps->seq_param_set_id);

  if(pSps->profile_id >= H264_PROFILE_HIGH) {
    bits_writeExpGolomb(pStream, pSps->chroma_format_idc);
    if(pSps->chroma_format_idc == 3) {
      bits_write(pStream, pSps->residual_color_transform, 1);
    }
    bits_writeExpGolomb(pStream, pSps->bit_depth_luma - 8);
    bits_writeExpGolomb(pStream, pSps->bit_depth_chroma - 8);
    bits_write(pStream, pSps->qprime_y_zero_transform_bypass, 1);
    bits_write(pStream, pSps->scaling_matrix_present, 1);

    if(pSps->scaling_matrix_present) {
      LOG(X_ERROR("SPS scaling matrix encode not implemented for high profiles"));
      return H264_RC_ERR;
    }
  } 

  bits_writeExpGolomb(pStream, pSps->log2_max_frame_num - 4);
  bits_writeExpGolomb(pStream, pSps->pic_order_cnt_type);

  if(pSps->pic_order_cnt_type == 0) {   
    bits_writeExpGolomb(pStream, pSps->log2_max_pic_order_cnt_lsb - 4);
  } else if(pSps->pic_order_cnt_type == 1) {  

    bits_write(pStream, pSps->pic_delta_pic_order, 1);
    bits_writeExpGolomb(pStream, pSps->pic_offset_non_ref_pic);
    bits_writeExpGolomb(pStream, pSps->pic_offset_top_to_bottom);
    bits_writeExpGolomb(pStream, pSps->pic_num_ref_frames_in_pic_order_cnt_cycle);

    for(idx = 0; idx < pSps->pic_num_ref_frames_in_pic_order_cnt_cycle; idx++) {
      bits_writeExpGolomb(pStream, pSps->pic_offset_for_ref_frame[idx]);
    }
   
  } else if(pSps->pic_order_cnt_type != 2) {
    LOG(X_ERROR("SPS contains invalid pic order count type:%d"), pSps->pic_order_cnt_type);
    return H264_RC_ERR;
  } 
  
  bits_writeExpGolomb(pStream, pSps->num_ref_frames);
  bits_write(pStream, pSps->gaps_in_frame_num_val_allowed_flag, 1);
  bits_writeExpGolomb(pStream, pSps->pic_width_in_mbs - 1);
  bits_writeExpGolomb(pStream, pSps->pic_height_in_mbs - 1);
  //TODO: this may be set to 0 for interlaced frames
  bits_write(pStream, pSps->frame_mbs_only_flag, 1);
  if(pSps->frame_mbs_only_flag == 0) {
      bits_write(pStream, pSps->mb_adaptive_frame_field_flag, 1);
  }
  bits_write(pStream, pSps->direct_8x8_inference_flag, 1);
  bits_write(pStream, pSps->frame_cropping, 1);
  if(pSps->frame_cropping) {
    bits_writeExpGolomb(pStream, pSps->crop_left);
    bits_writeExpGolomb(pStream, pSps->crop_right);
    bits_writeExpGolomb(pStream, pSps->crop_top);
    bits_writeExpGolomb(pStream, pSps->crop_bottom);
  }

  // VUI parameters
  bits_write(pStream, pSps->vui.params_present_flag, 1);
  if(pSps->vui.params_present_flag) {

    bits_write(pStream, pSps->vui.aspect_ratio_info_present, 1);
    if(pSps->vui.aspect_ratio_info_present) {
      bits_write(pStream, pSps->vui.aspect_ratio_idc, 8);
      if(pSps->vui.aspect_ratio_idc == 255) {
        bits_write(pStream, pSps->vui.aspect_ratio_sar_width, 16);
        bits_write(pStream, pSps->vui.aspect_ratio_sar_height, 16);
      }
    }

    bits_write(pStream, pSps->vui.overscan_info_present, 1);
    if(pSps->vui.overscan_info_present) {
      bits_write(pStream, pSps->vui.overscan_appropriate, 1);
    }

    bits_write(pStream, pSps->vui.video_signal_type_present, 1);
    if(pSps->vui.video_signal_type_present) {
      bits_write(pStream, pSps->vui.video_format, 3);
      bits_write(pStream, pSps->vui.video_full_range, 1);
      bits_write(pStream, pSps->vui.color_desc_present, 1);
      if(pSps->vui.color_desc_present) {
        bits_write(pStream, pSps->vui.color_primaries, 8);
        bits_write(pStream, pSps->vui.color_transfer_characteristics, 8);
        bits_write(pStream, pSps->vui.color_matrix, 8);
      }
    }

    bits_write(pStream, pSps->vui.chroma_loc_info_present, 1);
    if(pSps->vui.chroma_loc_info_present) {
      bits_writeExpGolomb(pStream, pSps->vui.chroma_sample_top);
      bits_writeExpGolomb(pStream, pSps->vui.chroma_sample_bottom);
    }

    bits_write(pStream, pSps->vui.timing_info_present, 1);
    if(pSps->vui.timing_info_present) {

        bits_write(pStream, pSps->vui.timing_num_units_in_tick, 32);
        bits_write(pStream, pSps->vui.timing_timescale, 32);
        bits_write(pStream, pSps->vui.timing_fixed_frame_rate, 1);
    }

    bits_write(pStream, pSps->vui.nal_hrd_params, 1);
    if(pSps->vui.nal_hrd_params) {
      if(h264_encodeSPSVUI_hrd(pStream, &pSps->vui) != 0) {
        return H264_RC_ERR;
      }
    }
    bits_write(pStream, pSps->vui.vcl_hrd_params, 1);
    if(pSps->vui.vcl_hrd_params) {
      if(h264_encodeSPSVUI_hrd(pStream, &pSps->vui) != 0) {
        return H264_RC_ERR;
      }
    }

    if(pSps->vui.nal_hrd_params || pSps->vui.vcl_hrd_params) {
      bits_write(pStream, pSps->vui.low_delay_hrd_flag, 1);
    }

    bits_write(pStream, pSps->vui.pic_struct_present_flag, 1);
    bits_write(pStream, pSps->vui.bitstream_restriction, 1);
    if(pSps->vui.bitstream_restriction) {
      bits_write(pStream, pSps->vui.motion_vec_over_pic_boundary, 1);
      bits_writeExpGolomb(pStream, pSps->vui.max_bytes_per_pic_denom);
      bits_writeExpGolomb(pStream, pSps->vui.max_bits_per_pic_denom);
      bits_writeExpGolomb(pStream, pSps->vui.log2_max_mv_len_horiz);
      bits_writeExpGolomb(pStream, pSps->vui.log2_max_mv_len_vert);
      bits_writeExpGolomb(pStream, pSps->vui.num_reorder_frames);
      bits_writeExpGolomb(pStream, pSps->vui.max_dec_frame_buffering);
    } 

  } // end of VUI 
 
  H264_STREAM_ENDOF_BYTE(pStream);

  return H264_RC_OK;
}

static int h264_decodeSPS_int(H264_DECODER_CTXT_T *pCtxt, BIT_STREAM_T *pStream) {
  int rc = H264_RC_OK;
  H264_NAL_SPS_T sps;

  if((rc = h264_decodeSPS(pStream, &sps)) == 0) {

    memcpy(&pCtxt->sps[sps.seq_param_set_id], &sps, sizeof(H264_NAL_SPS_T));
    pCtxt->sps_idx = sps.seq_param_set_id;
    pCtxt->flag |= H264_DECODER_CTXT_HAVE_SPS;

  }
  return rc;
}

int h264_decodePPS(BIT_STREAM_T *pStream, H264_NAL_PPS_T *pPps) {
  unsigned int sps_id;
  unsigned int pps_id;
  
  if(pStream->sz - pStream->byteIdx < 3) {
    LOG(X_ERROR("PPS given length to decode below minimum: %d"), 
         pStream->sz - pStream->byteIdx);
    return H264_RC_ERR;
  }

  if((pps_id = bits_readExpGolomb(pStream)) >= H264_MAX_PPS_CNT) {
    LOG(X_ERROR("PPS id is invalid:%d"), pps_id);
    return H264_RC_ERR;
  }
  sps_id = bits_readExpGolomb(pStream);
  memset(pPps, 0, sizeof(H264_NAL_PPS_T));
  pPps->seq_param_set_id = sps_id;
  pPps->pic_param_set_id = pps_id;

  pPps->entropy_coding_mode_flag = bits_read(pStream, 1);  // CABAC
  pPps->pic_order_present_flag = bits_read(pStream, 1);
  if((pPps->num_slice_group_map_units = bits_readExpGolomb(pStream) + 1) > 1) {
    // FMO (flexible macroblock ordering type 1-6)  
    pPps->slice_group_map_type =  bits_readExpGolomb(pStream);
    LOG(X_ERROR("PPS FMO ordering type %d not supported."), pPps->slice_group_map_type);
    return H264_RC_ERR;
  }
  pPps->num_ref_idx_l0_active =  bits_readExpGolomb(pStream) + 1;
  pPps->num_ref_idx_l1_active =  bits_readExpGolomb(pStream) + 1;
  pPps->weighted_pred_flag = bits_read(pStream, 1);
  pPps->weighted_bipred_idx = bits_read(pStream, 2);
  pPps->pic_init_qp =  bits_readExpGolomb(pStream) + 26;   // quantization scales (-26,25)
  pPps->pic_init_qs =  bits_readExpGolomb(pStream) + 26;
  pPps->chroma_qp_index_offset = bits_readExpGolomb(pStream);  // chroma qp offset for deblocking/loop filter (-12, 12)
  pPps->deblocking_filter_control_present_flag = bits_read(pStream, 1); //
  pPps->constrained_intra_pred_flag = bits_read(pStream, 1); // intra preditction
  pPps->redundant_pic_cnt_present_flag = bits_read(pStream, 1);

  pPps->is_ok = 1;


  return H264_RC_OK;
}

static int h264_decodePPS_int(H264_DECODER_CTXT_T *pCtxt, BIT_STREAM_T *pStream) {
  int rc = H264_RC_OK;
  H264_NAL_PPS_T pps;

  if((rc = h264_decodePPS(pStream, &pps)) == 0) {

    if(pps.seq_param_set_id >= H264_MAX_SPS_CNT ||
      !pCtxt->sps[pps.seq_param_set_id].is_ok) {
      LOG(X_ERROR("PPS references unknown SPS:%d"), pps.seq_param_set_id);
      return H264_RC_ERR;
    }

    memcpy(&pCtxt->pps[pps.pic_param_set_id], &pps, sizeof(H264_NAL_PPS_T));
    pCtxt->pps_idx = pps.pic_param_set_id;
    pCtxt->flag |= H264_DECODER_CTXT_HAVE_PPS;

  }
  return rc;
}


//static int h264_decodeSEI_int(H264_DECODER_CTXT_T *pCtxt, BIT_STREAM_T *pStream) {
  //LOG(X_WARNING("SEI slice not handled"));
//  return H264_RC_OK;
//}

H264_RESULT_T h264_decodeSEI(BIT_STREAM_T *pStream) {
  H264_RESULT_T rc = H264_RESULT_DECODED;
  uint8_t type;
  uint8_t i;
  int hasEssential = 0;
  int hasUserData = 0;
  //uint32_t idx0;
  uint32_t len;

  while(pStream->byteIdx + 2 < pStream->sz && rc != H264_RESULT_ERR) {

    len = 0;
    type = bits_read(pStream, 8); 

    do {
      i = bits_read(pStream, 8); 
      len += i;
      if(pStream->byteIdx >= pStream->sz) {
        break;
      }
    } while(i == 0xff);

    switch(type) {
      case H264_SEI_USERDATA_UNREG:
        pStream->byteIdx += len;
        hasUserData = 1;
        break;
      case H264_SEI_BUFFERINGPERIOD:
      case H264_SEI_PIC_TIMING:
      case H264_SEI_RECOVERY_POINT:
        hasEssential = 1; 
        pStream->byteIdx += len;
        break;
      default:
        rc = H264_RESULT_ERR;
        break;
   
    }

  }

  if(rc != H264_RESULT_ERR && !hasEssential && hasUserData) {
    rc = H264_RESULT_SEI_DISCARD;
  }

  return rc;
}

static int h264_decodeSlice(H264_DECODER_CTXT_T *pCtxt, BIT_STREAM_T *pStream, uint8_t nalType) {
  int first_mb_in_slice = 0;
  unsigned int slice_type = 0;
  unsigned int slice_type0 = 0;
  unsigned int pps_id = 0;
  unsigned int frame_idx = 0;
  int idr_pic_id = 0;
  int picture_structure = 0;
  int tmp;
  H264_NAL_SPS_T *pSps;
  H264_NAL_PPS_T *pPps;

  if(pStream->sz - pStream->byteIdx < 4) {
    LOG(X_WARNING("Slice given length to decode below minimum: %d"), pStream->sz - pStream->byteIdx);
    // Fail gracefully to accomodate empty NALs
    return H264_RC_OK;  
  }

  pCtxt->sliceIdx++;

  first_mb_in_slice = bits_readExpGolomb(pStream);
  if((slice_type0 = slice_type = (unsigned int) bits_readExpGolomb(pStream)) > H264_SLICE_TYPE_SI) {
    slice_type -= (H264_SLICE_TYPE_SI + 1);
  }

  if(slice_type > H264_SLICE_TYPE_SI) {
    LOG(X_WARNING("Invalid slice type: %d"), slice_type);
    return H264_RC_ERR;
  } 
  pCtxt->lastSliceType = slice_type;

  if((pps_id = bits_readExpGolomb(pStream)) >= H264_MAX_PPS_CNT ||
    !pCtxt->pps[pps_id].is_ok) {
    LOG(X_WARNING("Slice references unknown pps:%d"), pps_id);
    return -1;
  }

  pPps = &pCtxt->pps[pps_id];
  pSps = &pCtxt->sps[ pPps->seq_param_set_id ];
  if((frame_idx = bits_read(pStream, pSps->log2_max_frame_num)) != pCtxt->frameIdx) {
    pCtxt->frameIdx = frame_idx;
    pCtxt->sliceIdx = 0;
  } 


  if(!pSps->frame_mbs_only_flag && bits_read(pStream, 1)) {  // field pic flag
    picture_structure = 1 + bits_read(pStream, 1);  // pic_order_cnt_lsb
  } else {
    picture_structure = 3;
  }

  if(nalType == NAL_TYPE_IDR) {
    idr_pic_id = bits_readExpGolomb(pStream);
  }

  if(pSps->pic_order_cnt_type == 0) {
    pCtxt->picOrderCnt = bits_read(pStream, pSps->log2_max_pic_order_cnt_lsb);
    if(pPps->pic_order_present_flag == 1 && picture_structure == 3) {
      bits_readExpGolomb(pStream);  // delta_poc_bottom
    }
       
  } else if(pSps->pic_order_cnt_type == 1 && !pSps->pic_delta_pic_order) {
     bits_readExpGolomb(pStream);  // delta_poc[0]
     if(pPps->pic_order_present_flag == 1 &&  picture_structure == 3) {
       bits_readExpGolomb(pStream);  // delta_poc[1]
     }
  }


  if(pPps->redundant_pic_cnt_present_flag) {
    bits_readExpGolomb(pStream); // bits_readExpGolomb(pStream);
  }

  //fprintf(stderr, "slice: first_mb_in_slice:%d, slice_type:%d,%d, sps:%d, pps:%d, fridx:%d, picstruct:%d, idr_pic_id:%d, sps poctype:%d, poc:%d, pps_pic_order_present:%d, pps_redundant_pic_cnt_present:%d\n", first_mb_in_slice, slice_type, slice_type0, pPps->seq_param_set_id, pps_id, frame_idx, picture_structure, idr_pic_id, pSps->pic_order_cnt_type, pCtxt->picOrderCnt, pPps->pic_order_present_flag, pPps->redundant_pic_cnt_present_flag);

  if(slice_type != H264_SLICE_TYPE_I) {
    if(slice_type != H264_SLICE_TYPE_B) {
      bits_read(pStream, 1); // direct_spatial_mv_pred
    }
    if((bits_read(pStream, 1))) {  // num_ref_idx_active_override_flag
      bits_readExpGolomb(pStream); // ref_count[0] + 1
      if(slice_type != H264_SLICE_TYPE_B) {
        bits_readExpGolomb(pStream); // ref_count[1] + 1
      }
    }
    if(pPps->entropy_coding_mode_flag) {
      bits_readExpGolomb(pStream);
    }
  }

  if(slice_type == H264_SLICE_TYPE_SP) {
    bits_read(pStream, 1);  // sp_for_switch_flag
  }
  if(slice_type == H264_SLICE_TYPE_SP || slice_type == H264_SLICE_TYPE_SI) {
    bits_readExpGolomb(pStream);  // slice_qs_delta
  }

  if(pPps->deblocking_filter_control_present_flag) {
    tmp = bits_readExpGolomb(pStream);
    if(tmp < 2) {
      tmp^=1;
    }
    if(tmp) {
      bits_readExpGolomb(pStream);  // slice_alpha_c0_offset
      bits_readExpGolomb(pStream);  // slice_beta_offset

    }

  }
 
  return H264_RC_OK;
}

static int h264_decode_NAL_int(H264_DECODER_CTXT_T *pCtxt, BIT_STREAM_T *pStream) {

  int rc = H264_RC_OK;
  int nalType = pStream->buf[pStream->byteIdx];

  if(nalType & 0x80) {
    LOG(X_ERROR("NAL forbidden bit set!"));
    return -1;
  }
  H264_STREAM_INCR_BYTE(pStream);
  pCtxt->lastSliceType = H264_SLICE_TYPE_UNKNOWN;
                
  switch(nalType & NAL_TYPE_MASK) {

    case NAL_TYPE_SEQ_PARAM:     
      //fprintf(stderr, "SPS:\n");avc_dumpHex(stderr, &pStream->buf[pStream->byteIdx], 32, 1);
      rc = h264_decodeSPS_int(pCtxt, pStream);
      pCtxt->lastSliceType = H264_SLICE_TYPE_SPS;
      H264_STREAM_ENDOF_BYTE(pStream);
      break;

    case NAL_TYPE_PIC_PARAM:
      //fprintf(stderr, "PPS:\n");avc_dumpHex(stderr, &pStream->buf[pStream->byteIdx], 32, 1);

      // Prevent attempting to decode PPS w/o having received SPS context
      if(!(pCtxt->flag & H264_DECODER_CTXT_HAVE_SPS)) {
        return H264_RC_IGNORED;
      }

      rc = h264_decodePPS_int(pCtxt, pStream);
      pCtxt->lastSliceType = H264_SLICE_TYPE_PPS;
      H264_STREAM_ENDOF_BYTE(pStream);
      break;

    case NAL_TYPE_SEI:

      //rc = h264_decodeSEI_int(pCtxt, pStream);
      rc = H264_RC_OK;
      pCtxt->lastSliceType = H264_SLICE_TYPE_SEI;
      H264_STREAM_ENDOF_BYTE(pStream);
      break;

    case NAL_TYPE_SLICE:
    case NAL_TYPE_IDR:

      // Prevent attempting to decode VCL data w/o having received SPS & PPS context
      if(!(pCtxt->flag & (H264_DECODER_CTXT_HAVE_SPS | H264_DECODER_CTXT_HAVE_PPS))) {
        return H264_RC_IGNORED;
      }

      rc = h264_decodeSlice(pCtxt, pStream, (nalType & NAL_TYPE_MASK));
      H264_STREAM_ENDOF_BYTE(pStream);
      break;

    case NAL_TYPE_ACCESS_UNIT_DELIM:
      // quietly ignore
      break;

    default:

      LOG(X_WARNING("No handler for NAL type: %d"), nalType);
      rc = H264_RC_ERR;
      break;

  }
 
  return rc;
}


H264_RESULT_T h264_decode_NALHdr(H264_DECODER_CTXT_T *pCtxt, BIT_STREAM_T *pNal,
                                 unsigned int offsetFromStart) {
  int rc;

  if(!pCtxt || !pNal || !pNal->buf) {
    return H264_RESULT_ERR;
  }
  if(pNal->byteIdx >= pNal->sz) {
    return H264_RESULT_CONTINUATION; //??
  }   

  //TODO:  need to go through bytestream to find 0x 00 00 03, and take out
  // 0x3 since it was put into H264B formatter to prevent conflict w/ start code

  // Not interested in entire NAL payload beyond start of header
  if(offsetFromStart > 0) {
    return H264_RESULT_CONTINUATION;
  }

  if((rc = h264_decode_NAL_int(pCtxt, pNal)) == H264_RC_OK) {
    return H264_RESULT_DECODED;
  } else if(rc == H264_RC_IGNORED) {
    return H264_RESULT_IGNORED;
  } else {
    return H264_RESULT_ERR;
  }
  
}

int h264_isNALVcl(unsigned char hdr) {

  if((hdr & NAL_TYPE_MASK) <= NAL_TYPE_SEI) {
    return 1;
  } else {
    return 0;
  }
}

int h264_getNALType(unsigned char hdr) {
  return hdr & NAL_TYPE_MASK;
}

int h264_getVUITiming(H264_NAL_SPS_T *pSps, 
                      unsigned int *pClockHz, unsigned int *pFrameDeltaHz) {

  if(!pSps->vui.timing_info_present) {
    return -1;
  }

  if(pClockHz) {
    *pClockHz = pSps->vui.timing_timescale / 2;
  }
  if(pFrameDeltaHz) {
    *pFrameDeltaHz = pSps->vui.timing_num_units_in_tick;
  }

  return 0;
}

int h264_getVUITimingFromSps(AVC_DECODER_CFG_BLOB_T *pSpsBlob,
                             unsigned int *pClockHz, unsigned int *pFrameDeltaHz) {
  int rc = -1;
  H264_RESULT_T res; 
  H264_DECODER_CTXT_T ctxt;
  BIT_STREAM_T bs;

  if(!pSpsBlob) {
    return -1;
  }

  memset(&ctxt, 0, sizeof(ctxt));
  memset(&bs, 0, sizeof(bs));
  bs.buf = pSpsBlob->data;
  bs.sz = pSpsBlob->len;

  if((res = h264_decode_NALHdr(&ctxt, &bs, 0)) >= H264_RESULT_DECODED &&
    (rc = h264_getVUITiming(&ctxt.sps[ctxt.sps_idx], pClockHz, pFrameDeltaHz)) == 0) {
  } 
  return rc;
}   
                      

int h264_getCroppedDimensions(H264_NAL_SPS_T *pSps, 
                              unsigned int *pwcropped, unsigned int *phcropped,
                              unsigned int *pworig, unsigned int *phorig) {
  int rc = 0;
  int w, h;
  int crop_vert_mult = 2;

  if(!pSps) {
    return -1;
  }

  w = pSps->pic_width_in_mbs * 16;
  if(pworig) {
    *pworig = w;
  }
  h = pSps->pic_height_in_mbs * 16;

 if(pSps->frame_mbs_only_flag == 0) {
    // Interlaced frame
    h *= (2 - pSps->frame_mbs_only_flag);
    crop_vert_mult = 4;
  }

  if(phorig) {
    *phorig = h;
  }

  if(pSps->frame_cropping) {
    if(pSps->crop_bottom != 0) {
      h -= crop_vert_mult * pSps->crop_bottom;
    }
    if(pSps->crop_top != 0) {
      h -= crop_vert_mult * pSps->crop_top;
    }
    if(pSps->crop_left != 0) {
      w -= 2 * pSps->crop_left;
    }
    if(pSps->crop_right != 0) {
      w -= 2 * pSps->crop_right;
    }
    rc = 1;
  }

  if(pwcropped) {
    *pwcropped = w < 0 ? 0 : w;
  }
  if(phcropped) {
    *phcropped = h < 0 ? 0 : h;
  }

  return rc;
}
