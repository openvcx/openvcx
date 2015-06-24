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
#include <math.h>


void h264_printSps(FILE *fpOut, const H264_NAL_SPS_T *pSps) {

  fprintf(fpOut, "SPS:\n");
  fprintf(fpOut, "profile: %s (0x%x)\n", 
    h264_getPrintableProfileStr(pSps->profile_id), pSps->profile_id);
  fprintf(fpOut, "constraint bits: %u %u %u %u\n",
    (pSps->constraint & 0x80) >> 7, (pSps->constraint & 0x40) >> 6, 
    (pSps->constraint & 0x20) >> 5, (pSps->constraint & 0x10) >> 4);
  fprintf(fpOut, "level: %u\n", pSps->level_id);
  fprintf(fpOut, "sps id: %u\n", pSps->seq_param_set_id);
  fprintf(fpOut, "chroma format id: %u\n", pSps->chroma_format_idc);
  if(pSps->profile_id >= H264_PROFILE_HIGH) {
    fprintf(fpOut, "residual color transform: %u\n", pSps->residual_color_transform);
    fprintf(fpOut, "bit depth luma (minus 8): %u\n", pSps->bit_depth_luma);
    fprintf(fpOut, "bit depth chroma (minus 8): %u\n", pSps->bit_depth_chroma);
    fprintf(fpOut, "qprime y zero tranform bypass: %u\n", pSps->qprime_y_zero_transform_bypass);
    fprintf(fpOut, "scaling matrix present: %u\n", pSps->scaling_matrix_present);
  }
  fprintf(fpOut, "log2 max frame num (minus 4): %u\n", pSps->log2_max_frame_num);
  fprintf(fpOut, "pic order count type: %u\n", pSps->pic_order_cnt_type);
  if(pSps->pic_order_cnt_type == 0) {
    fprintf(fpOut, "max pic order count type lsb (minus 4): %u\n", pSps->log2_max_pic_order_cnt_lsb);
  } else if(pSps->pic_order_cnt_type == 1) {
    fprintf(fpOut, "delta pic order: %u\n", pSps->pic_delta_pic_order);
    fprintf(fpOut, "offset non ref pic: %u\n", pSps->pic_offset_non_ref_pic);
    fprintf(fpOut, "offset top to bottom: %u\n", pSps->pic_offset_top_to_bottom);
    fprintf(fpOut, "num ref frames in pic order count cycle: %u\n", pSps->pic_num_ref_frames_in_pic_order_cnt_cycle);
  }
  fprintf(fpOut, "num ref frames: %u\n", pSps->num_ref_frames);
  fprintf(fpOut, "gaps in frame num allowed: %u\n", pSps->gaps_in_frame_num_val_allowed_flag);
  fprintf(fpOut, "pic width in mb (minus 1): %u\n", pSps->pic_width_in_mbs);
  fprintf(fpOut, "pic height in mb (minus 1): %u\n", pSps->pic_height_in_mbs);
  fprintf(fpOut, "frame mbs only: %u\n", pSps->frame_mbs_only_flag);
  if(pSps->frame_mbs_only_flag == 0) {
    fprintf(fpOut, "mb adaptive frame field: %u\n", pSps->mb_adaptive_frame_field_flag);
  }
  fprintf(fpOut, "direct 8x8 inference: %u\n", pSps->direct_8x8_inference_flag);
  fprintf(fpOut, "frame cropping: %u\n", pSps->frame_cropping);
  if(pSps->frame_cropping) {
    fprintf(fpOut, "left: %u, right: %u, top: %u, bottom: %u\n",
      pSps->crop_left, pSps->crop_right, pSps->crop_top, pSps->crop_bottom);
  }
  fprintf(fpOut, "vui parameters present: %u\n", pSps->vui.params_present_flag);
  if(pSps->vui.params_present_flag) {
    fprintf(fpOut, "vui aspect ratio info present: %u\n", pSps->vui.aspect_ratio_info_present);
    if(pSps->vui.aspect_ratio_info_present) {
      fprintf(fpOut, "vui aspect ratio idc: %u\n", pSps->vui.aspect_ratio_idc);
      if(pSps->vui.aspect_ratio_idc == 255) {
        fprintf(fpOut, "vui aspect ratio sar width: %u\n", pSps->vui.aspect_ratio_sar_width);
        fprintf(fpOut, "vui aspect ratio sar height: %u\n", pSps->vui.aspect_ratio_sar_height);
      }
    }
    fprintf(fpOut, "vui overscan info present: %u\n", pSps->vui.overscan_info_present);
    if(pSps->vui.overscan_info_present) {
      fprintf(fpOut, "vui overscan appropriate: %u\n", pSps->vui.overscan_appropriate);
    }
    fprintf(fpOut, "vui video signal type present: %u\n", pSps->vui.video_signal_type_present);
    if(pSps->vui.video_signal_type_present) {
      fprintf(fpOut, "vui video signal format: %u\n", pSps->vui.video_format);
      fprintf(fpOut, "vui video signal full range: %u\n", pSps->vui.video_full_range);
      fprintf(fpOut, "vui video signal color description present: %u\n", pSps->vui.color_desc_present);
      if(pSps->vui.color_desc_present) {
        fprintf(fpOut, "vui video signal color primaries: %u\n", pSps->vui.color_primaries);
        fprintf(fpOut, "vui video signal color transfer characteristics: %u\n", pSps->vui.color_transfer_characteristics);
        fprintf(fpOut, "vui video signal color matrix: %u\n", pSps->vui.color_matrix);
      }
    }
    fprintf(fpOut, "vui chroma location info present: %u\n", pSps->vui.chroma_loc_info_present);
    if(pSps->vui.chroma_loc_info_present) {
      fprintf(fpOut, "vui chroma sample top: %u\n", pSps->vui.chroma_sample_top); 
      fprintf(fpOut, "vui chroma sample bottom: %u\n", pSps->vui.chroma_sample_bottom); 
    }
    fprintf(fpOut, "vui timing info present: %u\n", pSps->vui.timing_info_present);
    if(pSps->vui.timing_info_present) {
      fprintf(fpOut, "vui timing info num units in tick: %u\n", pSps->vui.timing_num_units_in_tick);
      fprintf(fpOut, "vui timing info scale: %u\n", pSps->vui.timing_timescale);
      fprintf(fpOut, "vui timing info fixed frame rate: %u\n", pSps->vui.timing_fixed_frame_rate);
    }
    fprintf(fpOut, "vui nal hrd params present: %u\n", pSps->vui.nal_hrd_params);
    fprintf(fpOut, "vui vcl hrd params present: %u\n", pSps->vui.vcl_hrd_params);
    fprintf(fpOut, "vui pic struct present: %u\n", pSps->vui.pic_struct_present_flag);
    fprintf(fpOut, "vui bitstream restriction: %u\n", pSps->vui.bitstream_restriction);
    if(pSps->vui.bitstream_restriction) {
      fprintf(fpOut, "vui bitstream motion vector over pic boundary: %u\n", 
        pSps->vui.motion_vec_over_pic_boundary);
      fprintf(fpOut, "vui bitstream max bytes per pic denom: %u\n", 
        pSps->vui.max_bytes_per_pic_denom);
      fprintf(fpOut, "vui bitstream max bits per pic denom: %u\n", 
        pSps->vui.max_bits_per_pic_denom);
      fprintf(fpOut, "vui bitstream log2 max motion vector len horiz: %u\n", 
        pSps->vui.log2_max_mv_len_horiz);
      fprintf(fpOut, "vui bitstream log2 max motion vector len vert: %u\n",
        pSps->vui.log2_max_mv_len_vert);
      fprintf(fpOut, "vui bitstream num reorder frames: %u\n",
        pSps->vui.num_reorder_frames);
      fprintf(fpOut, "vui bitstream max decoder frame buffering: %u\n",
        pSps->vui.max_dec_frame_buffering);

    }
  }
}

void h264_printPps(FILE *fpOut, const H264_NAL_PPS_T *pPps) {

  fprintf(fpOut, "PPS:\n");
  fprintf(fpOut, "pps id: %u\n", pPps->pic_param_set_id);
  fprintf(fpOut, "sps id: %u\n", pPps->seq_param_set_id);
  fprintf(fpOut, "entropy coding mode: %u\n", pPps->entropy_coding_mode_flag);
  fprintf(fpOut, "pic order present: %u\n", pPps->pic_order_present_flag);
  fprintf(fpOut, "num slice groups (minus 1): %u\n", pPps->num_slice_group_map_units);
  if(pPps->num_slice_group_map_units > 1) {
    fprintf(fpOut, "slice groups map type (FMO ordering): %u\n", pPps->slice_group_map_type);
  }
  fprintf(fpOut, "num ref idx l0 active (minus 1): %u\n", pPps->num_ref_idx_l0_active);
  fprintf(fpOut, "num ref idx l1 active (minus 1): %u\n", pPps->num_ref_idx_l1_active);
  fprintf(fpOut, "weighted prediction: %u\n", pPps->weighted_pred_flag);
  fprintf(fpOut, "weighted biprediction idx: %u\n", pPps->weighted_bipred_idx);
  fprintf(fpOut, "pic init qp (minus 26): %u\n", pPps->pic_init_qp);
  fprintf(fpOut, "pic init qs (minus 26): %u\n", pPps->pic_init_qs);
  fprintf(fpOut, "chroma qp index offset: %d\n", pPps->chroma_qp_index_offset);
  fprintf(fpOut, "deblocking filter control %u\n", pPps->deblocking_filter_control_present_flag);
  fprintf(fpOut, "constrained intra prediction: %u\n", pPps->constrained_intra_pred_flag);
  fprintf(fpOut, "redundant pic count present: %u\n", pPps->redundant_pic_cnt_present_flag);

}

#if defined(VSX_HAVE_VIDANALYZE)

static void printH264Info(FILE *fpOut,
                     const H264_AVC_DESCR_T *pH264,
                     const H264_NAL_SPS_T *pSps, 
                     const H264_NAL_PPS_T *pPps,
                     int verbosity) {

  const char *chromaFormat[] = {"Gray", "YUV420", "YUV422", "YUV444"};

  fprintf(fpOut, "H.264 %s profile (0x%x) level %u\n", 
          h264_getPrintableProfileStr(pSps->profile_id), 
          pSps->profile_id, pSps->level_id);

  fprintf(fpOut, "%ux%u %s%s%s poc:%d ref_fr:%d qp:%d qs:%d %s%s %s\n",
    pH264->frameWidthPx, pH264->frameHeightPx,
    (pPps->entropy_coding_mode_flag ? "CABAC" : "CAVLC"),
    (pPps->num_slice_group_map_units-1 ? " FMO" : ""),
    (pPps->weighted_pred_flag ? " weighted-pred" : ""),
    pSps->pic_order_cnt_type,
    pSps->num_ref_frames,
    pPps->pic_init_qp, 
    pPps->pic_init_qs,
    (pSps->frame_mbs_only_flag ? "frame_mbs_only" : ""),
    (pSps->mb_adaptive_frame_field_flag ? " mb_adaptive_frame_field" : ""),
    chromaFormat[pSps->chroma_format_idc]
    );
}

static const char *getNalSliceTypeDescr(uint8_t sliceType) {
  switch(sliceType) {
      case H264_SLICE_TYPE_I:
        return "  I";
      case H264_SLICE_TYPE_SI:
        return " SI";
      case H264_SLICE_TYPE_P:
        return "  P";
      case H264_SLICE_TYPE_SP:
        return " SP";
      case H264_SLICE_TYPE_B:
        return "  B";
      case H264_SLICE_TYPE_SEI:
        return "SEI";
  }
  return "";
}

static int checkFrameForSlice(const H264_NAL_T *pStartSlice,
                              const uint8_t sliceTypes[],
                              size_t szTypes) {
  unsigned int idx = 0;
  uint32_t frameId = 0;

  if(pStartSlice) {
    frameId = pStartSlice->content.frameId; 
  }

  while(pStartSlice && pStartSlice->content.frameId == frameId)  {

    for(idx = 0; idx < szTypes; idx++) {
      if(pStartSlice->sliceType == sliceTypes[idx]) {
        return 1;
      }
    }
    pStartSlice = (H264_NAL_T *) pStartSlice->content.pnext;
  }
  return 0;
}

static int analyze_h264_nals(const H264_AVC_DESCR_T *pH264, const double fps,
                             const char *pathInH264, FILE *fpOut, int verbosity) {
  BURSTMETER_DECODER_SET_T bitrates[2];
  H264_NAL_T *pNalsVcl;
  //unsigned int idx;
  unsigned int clockHz;
  unsigned int frameDeltaHz;
  uint32_t lastFrameId = 0;
  uint32_t frameId0 = 0;
  long long totBytes = 0;
  long long szIFrames = 0;
  long long szPFrames = 0;
  long long szBFrames = 0;
  long numIFrames = 0;
  long numPFrames = 0;
  long numBFrames = 0;
  long long *pSzFrames = NULL;
  double durationTotSec = 0.0;
  double totGOPszFrames = 0.0;
  unsigned int numGOPs = 0;
  unsigned int numFramesInGOP = 0;
  unsigned int numFramesInGOPMax = 0;
  const H264_NAL_PPS_T *pPps = NULL;
  const H264_NAL_SPS_T *pSps = NULL;
  char deltaBuf[32];
  const uint8_t sliceTypesI[] = { H264_SLICE_TYPE_I, H264_SLICE_TYPE_SI };
  const uint8_t sliceTypesP[] = { H264_SLICE_TYPE_P, H264_SLICE_TYPE_SP };
  const uint8_t sliceTypesB[] = { H264_SLICE_TYPE_B };

  deltaBuf[0] = '\0';

  if(fps != 0.0f) {
    vid_convert_fps(fps, &clockHz, &frameDeltaHz);
  } else {
    clockHz = pH264->clockHz;
    frameDeltaHz = pH264->frameDeltaHz;
  }
  
  if(clockHz == 0 || frameDeltaHz == 0) {
    LOG(X_ERROR("Frame rate not found in SPS VUI.  Specify frame rate with '--fps='"));
    return -1;
  }

  memset(bitrates, 0, sizeof(bitrates));
  bitrates[0].clockHz = bitrates[1].clockHz = clockHz;
  bitrates[0].frameDeltaHz = bitrates[1].frameDeltaHz = frameDeltaHz;
  if(burstmeter_init(&bitrates[0].set, 20, 1000) < 0 || burstmeter_init(&bitrates[1].set, 20, 300) < 0) {
    return -1;
  }
  //bitrate.extraMeters[0].rangeMs = 300;
  //bitrate.extraMeters[0].periodMs = bitrate.set.meter.periodMs;
  //bitrate.numExtraMeters = 1;
  
  pNalsVcl = (H264_NAL_T *) h264_cbGetNextAnnexBSample((H264_AVC_DESCR_T *) pH264, 0, 0);

  if(pNalsVcl) {
    frameId0 = lastFrameId = pNalsVcl->content.frameId;

    if(checkFrameForSlice(pNalsVcl, sliceTypesI, 2)) {
      numGOPs++;
      numFramesInGOP++;
      numIFrames++;
       pSzFrames = &szIFrames;
    } else if(checkFrameForSlice(pNalsVcl, sliceTypesB, 1)) {
      numBFrames++;
      pSzFrames = &szBFrames;
    } else if(checkFrameForSlice(pNalsVcl, sliceTypesP, 2)) {
      numPFrames++;
      pSzFrames = &szPFrames;
    } else {
      pSzFrames = NULL;
    }
  }

  pPps = &pH264->ctxt.pps[pH264->ctxt.pps_idx];
  pSps = &pH264->ctxt.sps[pH264->ctxt.sps_idx];
  
  while(pNalsVcl) {

    if(pNalsVcl->content.frameId != lastFrameId) {

      if(checkFrameForSlice(pNalsVcl, sliceTypesI, 2)) {

          numGOPs++;
          totGOPszFrames  += numFramesInGOP;
          if(numFramesInGOP > numFramesInGOPMax) {
            numFramesInGOPMax = numFramesInGOP;
          }
          numFramesInGOP = 0;
      }

      numFramesInGOP++;

      if(checkFrameForSlice(pNalsVcl, sliceTypesI, 2)) {
        numIFrames++;
        pSzFrames = &szIFrames;
      } else if(checkFrameForSlice(pNalsVcl, sliceTypesB, 1)) {
        numBFrames++;
        pSzFrames = &szBFrames;
      } else if(checkFrameForSlice(pNalsVcl, sliceTypesP, 2)) {
        numPFrames++;
        pSzFrames = &szPFrames;
      } else {
        pSzFrames = NULL;
      }

      lastFrameId = pNalsVcl->content.frameId;
    }

    if(verbosity > 1) {
      if(pNalsVcl == pH264->pNalsVcl) {
        h264_printSps(fpOut, pSps);
        fprintf(fpOut, "\n");
        h264_printPps(fpOut, pPps);
        fprintf(fpOut, "\n");
      }


      if(pH264->haveBSlices) {
        snprintf(deltaBuf, sizeof(deltaBuf),",(%s%d)", (pNalsVcl->content.decodeOffset>0?"+":""),
                                                        pNalsVcl->content.decodeOffset);
      }
      fprintf(fpOut, "[fr:%u (idx:%u,poc:%u)%s %uHz,%"LL64"uHz] %s file:%"LL64"u(0x%"LL64"x) len:%u\n", 
                     pNalsVcl->content.frameId,
                     pNalsVcl->frameIdx,
                     pNalsVcl->picOrderCnt,
                     deltaBuf,
                     pNalsVcl->content.playDurationHz,
                     pNalsVcl->content.playOffsetHz,
                     getNalSliceTypeDescr(pNalsVcl->sliceType),
                     pNalsVcl->content.fileOffset,
                     pNalsVcl->content.fileOffset,
                     pNalsVcl->content.sizeRd);
    }

    if(pSzFrames) {
      *pSzFrames += pNalsVcl->content.sizeRd;
    }

    if(burstmeter_AddDecoderSample(&bitrates[0], (unsigned int) pNalsVcl->content.sizeRd, 
                                  (unsigned int) pNalsVcl->content.frameId) != 0 ||
      burstmeter_AddDecoderSample(&bitrates[1], (unsigned int) pNalsVcl->content.sizeRd, 
                                  (unsigned int) pNalsVcl->content.frameId) != 0) {
      LOG(X_ERROR("Failed to add bitrate sample"));
      break;
    }

    totBytes += pNalsVcl->content.sizeRd;

    pNalsVcl = (H264_NAL_T *) h264_cbGetNextAnnexBSample((H264_AVC_DESCR_T *) pH264, 1, 0);
  }

  totGOPszFrames  += numFramesInGOP;

  if(pH264->pNalsLastVcl ) {
    durationTotSec = (double) ((pH264->pNalsLastVcl->content.frameId - frameId0) 
                          * frameDeltaHz) / clockHz;
  }


  if(verbosity > 1) {
    fprintf(fpOut, "\n");
  }
  fprintf(fpOut,"%s size: %.1fKB, duration: %s\n", 
                pathInH264, (double) totBytes/1024.0f, avc_getPrintableDuration(
                 (uint64_t) 
                (pH264->pNalsLastVcl ? pH264->pNalsLastVcl->content.frameId : 0) 
                 * frameDeltaHz, clockHz));

  if(clockHz != 0) {
    fprintf(fpOut, "%u Hz, tick %u Hz (%.4f fps)\n",
            clockHz, frameDeltaHz, ((double)clockHz / frameDeltaHz));
  }

  printH264Info(fpOut, pH264, pSps, pPps, verbosity);

  fprintf(fpOut, "slices:%u vcl:%u decode errors:%u\n",
        pH264->numNalsTot, pH264->numNalsVcl, pH264->numNalsErr);

  fprintf(fpOut, "I:%"LL64"u, P:%"LL64"u, B:%"LL64"u, SI:%"LL64"u, SP:%"LL64"u, SPS:%"LL64"u, PPS:%"LL64"u, SEI:%"LL64"u, unknown:%"LL64"u\n",
    pH264->sliceCounter[H264_SLICE_TYPE_I],
    pH264->sliceCounter[H264_SLICE_TYPE_P],
    pH264->sliceCounter[H264_SLICE_TYPE_B],
    pH264->sliceCounter[H264_SLICE_TYPE_SI],
    pH264->sliceCounter[H264_SLICE_TYPE_SP],
    pH264->sliceCounter[H264_SLICE_TYPE_SPS],
    pH264->sliceCounter[H264_SLICE_TYPE_PPS],
    pH264->sliceCounter[H264_SLICE_TYPE_SEI],
    pH264->sliceCounter[H264_SLICE_TYPE_UNKNOWN]);

  if(pH264->pNalsLastVcl) {
    fprintf(fpOut, "frames: %u, %.1f slices/fr GOP avg: %.1f, max: %u\n", 
                pH264->pNalsLastVcl->content.frameId - frameId0 + 1,
                (double)pH264->numNalsVcl / (pH264->pNalsLastVcl->content.frameId - frameId0 + 1),
                (numGOPs == 0 ? 0 : (double)totGOPszFrames / numGOPs), numFramesInGOPMax);
  }
  fprintf(fpOut, "I:%lu %.1fKB/fr, P:%lu %.1fKB/fr, B:%lu %.1fKB/fr\n",
          numIFrames, (numIFrames > 0 ? ((double)szIFrames /(numIFrames * 1024.0f)) : 0),
          numPFrames, (numPFrames > 0 ? ((double)szPFrames /(numPFrames * 1024.0f)) : 0),
          numBFrames, (numBFrames > 0 ? ((double)szBFrames /(numBFrames * 1024.0f)) : 0));

  fprintf(fpOut,"bitrate  avg: %.1fKb/s, max(1sec): %.1fKb/s", 
          (double)totBytes / durationTotSec/125.0,
          (bitrates[0].set.meter.max.bytes * (1000.0f / bitrates[0].set.meter.rangeMs) / 125.0f));

  //for(idx = 0; idx < bitrate.numExtraMeters; idx++) {
  //  fprintf(fpOut, ", (%ums): %.1fKb/s\n",
  //    bitrate.extraMeters[idx].rangeMs,
  //    (bitrate.extraMeters[idx].max.bytes * (1000.0f / bitrate.extraMeters[idx].rangeMs) / 125.0f));
  //}
  fprintf(fpOut, ", (%ums): %.1fKb/s\n",
    bitrates[1].set.meter.rangeMs,
    (bitrates[1].set.meter.max.bytes * (1000.0f / bitrates[1].set.meter.rangeMs) / 125.0f));
  fprintf(fpOut, "\n");

  burstmeter_close(&bitrates[0].set);
  burstmeter_close(&bitrates[1].set);

  return 0;
}

int h264_analyze_flv(FLV_CONTAINER_T *pFlv, FILE *fpOut, double fps, int verbosity) {
  H264_AVC_DESCR_T h264;
  int rc = 0;

  memset(&h264, 0, sizeof(h264));

  if(h264_createNalListFromFlv(&h264, pFlv, fps) != 0) {
    return -1;
  }

  if(fps == 0 && (h264.clockHz == 0 || h264.frameDeltaHz == 0)) {    

    fprintf(stderr, "TODO: get fps from flv\n");
    return -1;
  }

  rc = analyze_h264_nals(&h264, fps, pFlv->pStream->filename, fpOut, verbosity);

  h264_free(&h264);

  return rc;
}


int h264_analyze_mp4(MP4_CONTAINER_T *pMp4, FILE *fpOut, double fps, int verbosity) {
  H264_AVC_DESCR_T h264;
  MP4_TRAK_AVCC_T mp4BoxSet;
  int rc = 0;

  memset(&h264, 0, sizeof(h264));

  //if(h264_createFromMp4(&h264, pMp4, 0) != 0) {
  if(h264_createNalListFromMp4(&h264, pMp4, NULL, 0) != 0) {
    return -1;
  }

  if(fps == 0 && (h264.clockHz == 0 || h264.frameDeltaHz == 0)) {
    if(mp4_initAvcCTrack(pMp4, &mp4BoxSet) < 0) {
      return -1;
    } else if(mp4BoxSet.tk.pStts->list.pEntries[0].sampledelta == 0 ||
              mp4BoxSet.tk.pMdhd->timescale == 0) {
      LOG(X_ERROR("Unable to extract timing information from avcC track"));
      return -1;
    }
    h264.clockHz = mp4BoxSet.tk.pMdhd->timescale;
    h264.frameDeltaHz = mp4BoxSet.tk.pStts->list.pEntries[0].sampledelta;
  }

  rc = analyze_h264_nals(&h264, fps, pMp4->pStream->cbGetName(pMp4->pStream), fpOut, verbosity);

  h264_free(&h264);

  return rc;
}

int h264_analyze_annexB(FILE_STREAM_T *pStreamH264, FILE *fpOut, double fps, int verbosity) {
  H264_AVC_DESCR_T h264;
  int rc = 0;

  memset(&h264, 0, sizeof(h264));

  if(h264_createNalListFromAnnexB(&h264, pStreamH264, 0, fps) != 0) {
    return -1;
  }

  rc = analyze_h264_nals(&h264, fps, pStreamH264->filename, fpOut, verbosity);
   
  h264_free(&h264);

  return rc;
}

#endif // VSX_HAVE_VIDANALYZE
