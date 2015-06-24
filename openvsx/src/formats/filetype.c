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


const char *codecType_getCodecDescrStr(XC_CODEC_TYPE_T codecType) {
  
  switch(codecType) {

    case XC_CODEC_TYPE_H262:
      return "MPEG-2";
    case XC_CODEC_TYPE_H263:
      return "H.263";
    case XC_CODEC_TYPE_H263_PLUS:
      return "H.263+";
    case XC_CODEC_TYPE_MPEG4V:
      return "MPEG-4";
    case XC_CODEC_TYPE_H264:
      return "H.264 / AVC";
    case XC_CODEC_TYPE_VP6:
      return "On2 VP-6";
    case XC_CODEC_TYPE_VP6F:
      return "On2 VP-6 (Flash)";
    case XC_CODEC_TYPE_VP8:
      return "On2 VP-8";
    case XC_CODEC_TYPE_VID_GENERIC:
      return "Video";
    case XC_CODEC_TYPE_VID_CONFERENCE:
      return "Video Conference";
    case XC_CODEC_TYPE_AUD_CONFERENCE:
      return "Audio Conference";

    case XC_CODEC_TYPE_AAC:
      return "AAC";
    case XC_CODEC_TYPE_AC3:
      return "A52 / AC3";
    case XC_CODEC_TYPE_MPEGA_L2:
      return "MPEG-2 L2 Audio";
    case XC_CODEC_TYPE_MPEGA_L3:
      return "MPEG-2 L3 Audio";
    case XC_CODEC_TYPE_AMRNB:
      return "AMR-NB";
    case XC_CODEC_TYPE_VORBIS:
      return "Vorbis";
    case XC_CODEC_TYPE_SILK:
      return "SILK";
    case XC_CODEC_TYPE_OPUS:
      return "OPUS";
    case XC_CODEC_TYPE_G711_MULAW:
      return "G.711 ulaw";
    case XC_CODEC_TYPE_G711_ALAW:
      return "G.711 alaw";
    case XC_CODEC_TYPE_TELEPHONE_EVENT:
      return "telephone-event";
    case XC_CODEC_TYPE_AUD_GENERIC:
      return "Audio";

    case MEDIA_FILE_TYPE_MPEG2:
      return "mpeg2-ps";
    case MEDIA_FILE_TYPE_MP2TS:
      return "mpeg2-ts";
    case MEDIA_FILE_TYPE_MP2TS_BDAV:
      return "mpeg2-ts bdav";
    case MEDIA_FILE_TYPE_MP4:
      return "mp4";
    case MEDIA_FILE_TYPE_FLV:
      return "flv";
    case MEDIA_FILE_TYPE_MKV:
      return "matroska";
    case MEDIA_FILE_TYPE_WEBM:
      return "webm";
    case MEDIA_FILE_TYPE_RTMPDUMP:
      return "rtmp";
    case MEDIA_FILE_TYPE_RAWDUMP:
      return "raw";
    case MEDIA_FILE_TYPE_M3U:
      return "m3u";
    case MEDIA_FILE_TYPE_SDP:
      return "sdp";
    case MEDIA_FILE_TYPE_META:
      return "metafile";
    case MEDIA_FILE_TYPE_DASHMPD:
      return "dash";

    case MEDIA_FILE_TYPE_PNG:
      return "png";
    case MEDIA_FILE_TYPE_BMP:
      return "bmp";
    case MEDIA_FILE_TYPE_JPG:
      return "jpg";

    case XC_CODEC_TYPE_RAWV_BGRA32:
      return "bgra(32)";
    case XC_CODEC_TYPE_RAWV_RGBA32:
      return "rgba(32)";
    case XC_CODEC_TYPE_RAWV_BGR24:
      return "bgr(24)";
    case XC_CODEC_TYPE_RAWV_RGB24:
      return "rgb(24)";
    case XC_CODEC_TYPE_RAWV_BGR565:
      return "bgr565 (16)";
    case XC_CODEC_TYPE_RAWV_RGB565:
      return "rgb565 (16)";
    case XC_CODEC_TYPE_RAWV_YUV420P:
      return "yuv420p";
    case XC_CODEC_TYPE_RAWV_NV12:
      return "yuv420sp (Y,UVUV)";
    case XC_CODEC_TYPE_RAWV_NV21:
      return "yuv420sp (Y,VUVU)";

    case XC_CODEC_TYPE_RAWA_PCM16LE:
      return "signed 16bit PCM LE";
    case XC_CODEC_TYPE_RAWA_PCMULAW:
      return "PCM mulaw";
    case XC_CODEC_TYPE_RAWA_PCMALAW:
      return "PCM alaw";

    default:
      return "";
  } 

}

unsigned int codecType_getBitsPerPx(XC_CODEC_TYPE_T codecType) {

  switch(codecType) {
    case XC_CODEC_TYPE_RAWV_BGRA32:
    case XC_CODEC_TYPE_RAWV_RGBA32:
      return 32;
    case XC_CODEC_TYPE_RAWV_BGR24:
    case XC_CODEC_TYPE_RAWV_RGB24:
      return 24;
    case XC_CODEC_TYPE_RAWV_BGR565:
    case XC_CODEC_TYPE_RAWV_RGB565:
      return 16;
    case XC_CODEC_TYPE_RAWV_YUV420P:
    case XC_CODEC_TYPE_RAWV_NV12:
    case XC_CODEC_TYPE_RAWV_NV21:
    case XC_CODEC_TYPE_VID_CONFERENCE: // Default to YUV420
      return 12;
    case XC_CODEC_TYPE_RAWV_YUVA420P:
      return 20;
    default:
      return 0;
  }
}

unsigned int codecType_getBytesPerSample(XC_CODEC_TYPE_T codecType) {

  switch(codecType) {
   case XC_CODEC_TYPE_RAWA_PCM16LE:
     return 2;
   case XC_CODEC_TYPE_G711_MULAW:
   case XC_CODEC_TYPE_G711_ALAW:  
   case XC_CODEC_TYPE_RAWA_PCMULAW: 
   case XC_CODEC_TYPE_RAWA_PCMALAW: 
     return 1;
   default:
     return 0;
  }
}

int codecType_getRtpPT(XC_CODEC_TYPE_T codecType, int *payloadTypesMin1) {

  int portIdx = -1;

  if(payloadTypesMin1) {
    if(codecType == MEDIA_FILE_TYPE_MP2TS || codectype_isVid(codecType)) {
      portIdx = STREAM_IDX_VID;
    } else if(codectype_isAud(codecType)) {
      portIdx = STREAM_IDX_AUD;
    } 
    if(portIdx >= 0) {
      if(payloadTypesMin1[portIdx] > 0) {
        return payloadTypesMin1[portIdx] -1;
      }
    }
  }

  switch(codecType) {
    case XC_CODEC_TYPE_H264:
      return RTP_PAYLOAD_TYPE_DYNAMIC + 16;
    case XC_CODEC_TYPE_MPEG4V:
    case XC_CODEC_TYPE_H263_PLUS:
    case XC_CODEC_TYPE_VP8:
      return RTP_PAYLOAD_TYPE_DYNAMIC + 1;
    case XC_CODEC_TYPE_H263:
      return RTP_PAYLOAD_TYPE_H263;
    case XC_CODEC_TYPE_AAC:
    case XC_CODEC_TYPE_AMRNB:
    case XC_CODEC_TYPE_SILK:
    case XC_CODEC_TYPE_OPUS:
      return RTP_PAYLOAD_TYPE_DYNAMIC;
    case XC_CODEC_TYPE_G711_MULAW:
      return RTP_PAYLOAD_TYPE_PCMU;
    case XC_CODEC_TYPE_G711_ALAW:
      return RTP_PAYLOAD_TYPE_PCMA;
    case MEDIA_FILE_TYPE_MP2TS:
      return RTP_PAYLOAD_TYPE_MP2T;
    default:
      return -1;
  }

}

static MEDIA_FILE_TYPE_T check(FILE_STREAM_T *pStream) {
  H264_STREAM_CHUNK_T h264Stream;
  BIT_STREAM_T *pBitStream = &h264Stream.bs;
  uint32_t ui32;
  MEDIA_FILE_TYPE_T fileType;
  unsigned char buf2[64];
  uint32_t bufaligned[16];
  unsigned char *buf = ((unsigned char *) bufaligned);
  int rc;

  if(ReadFileStream(pStream, buf, 4) < 0) {
    return MEDIA_FILE_TYPE_UNKNOWN;
  }

  if(buf[0] == 'F' && buf[1] == 'L' && buf[2] == 'V') {
    return MEDIA_FILE_TYPE_FLV;
  } else if((buf[0] == 0x89 && buf[1] == 'P' && buf[2] == 'N' && buf[3] == 'G') ||
            (buf[0] == 0x8a && buf[1] == 'M' && buf[2] == 'N' && buf[3] == 'G')) {
    return MEDIA_FILE_TYPE_PNG;
  } else if(buf[0] == 'B' && buf[1] == 'M') {
    return MEDIA_FILE_TYPE_BMP;
  }

  ui32 = htonl(*bufaligned);

  if(ui32 == MPEG2_HDR32_PACK || ui32 == MPEG2_HDR32_SEQ) {
     return MEDIA_FILE_TYPE_MPEG2;
  }

  if(ui32 == MPEG4_HDR32_VOS || ui32 == MPEG4_HDR32_GOP || ui32 == MPEG4_HDR32_VOP) {
    return MEDIA_FILE_TYPE_MP4V;
  }

  memset(&h264Stream, 0, sizeof(h264Stream));
  h264Stream.bs.sz = 4;
  h264Stream.bs.buf = buf;
  h264Stream.bs.bitIdx = 0;
  h264Stream.bs.byteIdx = 0;
  h264Stream.startCodeMatch = 0;

  if((rc = h264_findStartCode(&h264Stream, 0, NULL)) == 3 || rc == 4) {
    return MEDIA_FILE_TYPE_H264b;
  }

  if(ReadFileStream(pStream, &buf[4], 4) < 0) {
    return MEDIA_FILE_TYPE_UNKNOWN;
  }
  pBitStream->bitIdx = 0;
  pBitStream->byteIdx = 0;
  pBitStream->sz = 7;
  if(bits_read(pBitStream, 12) == 0xfff) {
    bits_read(pBitStream, 10);
    if(bits_read(pBitStream, 1) == 0) { // read private (0) bit
      return MEDIA_FILE_TYPE_AAC_ADTS;
    }
  }

  // TODO: check valid mp4 file
  ui32 = *((uint32_t *) &buf[4]);
  if(ui32 == *((uint32_t *) "ftyp") ||
     ui32 == *((uint32_t *) "styp") ||
     ui32 == *((uint32_t *) "mdat") ||
     ui32 == *((uint32_t *) "moov")) {
    return MEDIA_FILE_TYPE_MP4;
  }

  if(buf[0] == MP2TS_SYNCBYTE_VAL) {
    if(SeekMediaFile(pStream, MP2TS_LEN, SEEK_SET) == 0 && 
       ReadFileStream(pStream, buf2, 4) >= 0 &&
       buf2[0] == MP2TS_SYNCBYTE_VAL) {
      return MEDIA_FILE_TYPE_MP2TS; 
    }
  } else if(buf[4] == MP2TS_SYNCBYTE_VAL) {
    if(SeekMediaFile(pStream, MP2TS_BDAV_LEN + 4, SEEK_SET) == 0 && 
       ReadFileStream(pStream, buf2, 4) >= 0 &&
       buf2[0] == MP2TS_SYNCBYTE_VAL) {
      return MEDIA_FILE_TYPE_MP2TS_BDAV; 
    }
  }

  if(SeekMediaFile(pStream, 0, SEEK_SET) == 0 && 
     (fileType = mkv_isvalidFile(pStream)) != MEDIA_FILE_TYPE_UNKNOWN) {
    return fileType;
  }  

  if(SeekMediaFile(pStream, 0, SEEK_SET) == 0 && sdp_isvalidFile(pStream)) {
    return MEDIA_FILE_TYPE_SDP;
  }

  if(SeekMediaFile(pStream, 0, SEEK_SET) == 0 && metafile_isvalidFile(pStream->filename)) {
    return MEDIA_FILE_TYPE_META;
  }

  if(SeekMediaFile(pStream, 0, SEEK_SET) == 0 && m3u_isvalidFile(pStream->filename, 0)) {
    return MEDIA_FILE_TYPE_M3U;
  }

  if(SeekMediaFile(pStream, 0, SEEK_SET) == 0 && amr_isvalidFile(pStream)) {
    return MEDIA_FILE_TYPE_AMRNB;
  }  

  return MEDIA_FILE_TYPE_UNKNOWN;
}

enum MEDIA_FILE_TYPE filetype_check(FILE_STREAM_T *pStream) {
  enum MEDIA_FILE_TYPE mediaType;
  FILE_OFFSET_T offset;

  if(!pStream) {
    return MEDIA_FILE_TYPE_UNKNOWN;
  }

  offset = pStream->offset;

  if(pStream->offset != 0) {
    if(fileops_Fseek(pStream->fp, 0, SEEK_SET) != 0) {
      LOG(X_ERROR("Unable to seek to start of file '%s'."), pStream->filename);
      return MEDIA_FILE_TYPE_UNKNOWN;
    }
    pStream->offset = 0;
  }

  mediaType = check(pStream);

  if(pStream->offset != offset) {
    if(fileops_Fseek(pStream->fp, offset, SEEK_SET) != 0) {
      LOG(X_ERROR("Unable to seek in file '%s'."), pStream->filename);
      return MEDIA_FILE_TYPE_UNKNOWN;
    }
    pStream->offset = offset;
  }

  return mediaType;
}


static int get_mp2ts_vidaudprop(const MP2TS_CONTAINER_T *pMp2ts, 
                                MEDIA_DESCRIPTION_T *pMedia) {
  unsigned int idx = 0;
  MP2TS_PID_DESCR_T *pPidDescr = NULL;

  if(!pMp2ts || !pMp2ts->pmtChanges) {
    return -1;
  }

  for(idx = 0; 
      idx < sizeof(pMp2ts->pmtChanges->pidDescr) / sizeof(pMp2ts->pmtChanges->pidDescr[0]);
      idx++) {

    pPidDescr = &pMp2ts->pmtChanges->pidDescr[idx];
    //if(pPidDescr->pid.streamtype == MP2PES_STREAM_VIDEO_H262 ||
    //   pPidDescr->pid.streamtype == MP2PES_STREAM_VIDEO_H264) {
    if(codectype_isVid(mp2_getCodecType(pPidDescr->pid.streamtype))) {

      pMedia->haveVid = 1;
      memcpy(&pMedia->vid.generic, &pPidDescr->u.vidProp, sizeof(pMedia->vid.generic));
      strncpy(pMedia->vid.codec, codecType_getCodecDescrStr(pPidDescr->u.vidProp.mediaType),
              sizeof(pMedia->vid.codec) - 1);

    //} else if(pPidDescr->pid.streamtype == MP2PES_STREAM_AUDIO_AC3 ||
    //          pPidDescr->pid.streamtype == MP2PES_STREAM_AUDIO_AACADTS) {
    } else if(codectype_isAud(mp2_getCodecType(pPidDescr->pid.streamtype))) {

      pMedia->haveAud = 1;
      memcpy(&pMedia->aud.generic, &pPidDescr->u.audProp, sizeof(pMedia->aud.generic));
      strncpy(pMedia->aud.codec, codecType_getCodecDescrStr(pPidDescr->u.audProp.mediaType),
              sizeof(pMedia->vid.codec) - 1);
    }

    if(pMedia->haveVid && pMedia->haveAud) {
      break;
    }
  }

  if(pMedia->haveVid || pMedia->haveAud) {
    return 0;
  } else {
    return -1;
  }
}

int filetype_getdescr(FILE_STREAM_T *pFileStream, 
                      MEDIA_DESCRIPTION_T *pMedia,
                      int fullintrospect) {
  unsigned int idx;
  BOX_T *pBox = NULL;
  BOX_AVC1_T *pBoxAvc1 = NULL;
  BOX_AVCC_T *pBoxAvcC = NULL;
  BOX_ESDS_T *pBoxEsds = NULL;
  BOX_MP4A_T *pBoxMp4a = NULL;
  MP4_TRAK_T mp4Trak;
  MP4_CONTAINER_T *pMp4 = NULL;
  FLV_CONTAINER_T *pFlv = NULL;
  MP2TS_CONTAINER_T *pMp2ts = NULL;
  MKV_CONTAINER_T *pMkv = NULL;
  MKV_TRACK_T *pMkvTrack;
  uint32_t frameDeltaHz;
  //struct stat st;
  int rc = 0;

  if(!pFileStream || pFileStream->fp == FILEOPS_INVALID_FP || !pMedia) {
    return -1;
  }

  if((pMedia->type = filetype_check(pFileStream))
                                           == MEDIA_FILE_TYPE_UNKNOWN) {
    LOG(X_ERROR("Unrecognized file format '%s'"), pFileStream->filename);
    return -1;
  }

  if(!fullintrospect) {
    return 0;
  }

  switch(pMedia->type) {
    case MEDIA_FILE_TYPE_MP4:

      if((pMp4 = mp4_open(pFileStream->filename, 0)) == NULL) {
        return -1;
      }
      pMedia->durationSec = mp4_getDurationMvhd(pMp4);
      pMedia->totSz = pMp4->pStream->size;

      if((pBox = mp4_findBoxChild(pMp4->proot, *((uint32_t *) "mdat")))) {
        pMedia->dataOffset = pBox->fileOffset;
      }

      //
      // Check if the mp4 is a fragmented (moof) file w/o any initializer segment
      // and if-so, assume that it contains both audio & video media data.
      //
       if(!mp4_findBoxChild(pMp4->proot, *((uint32_t *) "moov")) &&
          mp4_findBoxChild(pMp4->proot, *((uint32_t *) "moof"))) {

        pMedia->haveVid = 1;
        pMedia->haveAud = 1;

      } else {

        if((pBoxAvc1 = (BOX_AVC1_T *) 
                      mp4_loadTrack(pMp4->proot, &mp4Trak, *((uint32_t *) "avc1"), 1))) {
          pMedia->haveVid = 1;
          pMedia->vid.generic.resH = htons(pBoxAvc1->width);
          pMedia->vid.generic.resV = htons(pBoxAvc1->height);

          if((pBoxAvcC = (BOX_AVCC_T *) mp4_findBoxChild((BOX_T *) pBoxAvc1, 
                                                        *((uint32_t *) "avcC")))) {
            pMedia->vid.generic.profile = pBoxAvcC->avcc.avcprofile;
            pMedia->vid.generic.level = pBoxAvcC->avcc.avclevel;
          }

          frameDeltaHz = mp4_getSttsSampleDelta(&mp4Trak.pStts->list);

          if(mp4Trak.pMdhd->timescale == 0 || frameDeltaHz == 0) {
            pMedia->vid.generic.fps = 0;
          } else {
            pMedia->vid.generic.fps = (float) mp4Trak.pMdhd->timescale /  frameDeltaHz;
          }

          strncpy(pMedia->vid.codec, codecType_getCodecDescrStr(MEDIA_FILE_TYPE_H264b), 
                sizeof(pMedia->vid.codec) - 1);
        }

        if((pBoxMp4a = (BOX_MP4A_T *) mp4_loadTrack(pMp4->proot, &mp4Trak, *((uint32_t *) "mp4a"), 1))) {
          pMedia->haveAud = 1;
          pMedia->aud.generic.clockHz = htonl(pBoxMp4a->timescale)>>16;
          pMedia->aud.generic.channels = htons(pBoxMp4a->channelcnt);

          if((pBoxEsds = (BOX_ESDS_T *) 
                      mp4_findBoxChild((BOX_T *) pBoxMp4a, *((uint32_t *) "esds")))) {
            strncpy(pMedia->aud.codec, codecType_getCodecDescrStr(MEDIA_FILE_TYPE_AAC_ADTS),
                  sizeof(pMedia->aud.codec) - 1);
          }

        }

      } // end of if(pMp4->haveMoof ...

      mp4_close(pMp4);
      break;

    case MEDIA_FILE_TYPE_FLV:

      if((pFlv = flv_open(pFileStream->filename)) == NULL) {
        return -1;
      }

      if(pFlv->timescaleHz > 0) {
        pMedia->durationSec = (double) pFlv->durationTotHz / pFlv->timescaleHz;
      }

      pMedia->totSz = pFlv->pStream->size;
      //TODO: hardcoded dataOffset for http_resp_sendmedia throttling
      pMedia->dataOffset = 4096;

      if(pFlv->vidCfg.pAvcC) {

        pMedia->haveVid = 1;
        pMedia->vid.generic.resH = pFlv->vidCfg.vidDescr.resH;
        pMedia->vid.generic.resV = pFlv->vidCfg.vidDescr.resV;
        pMedia->vid.generic.profile = pFlv->vidCfg.pAvcC->avcprofile;
        pMedia->vid.generic.level = pFlv->vidCfg.pAvcC->avclevel;

        if(pFlv->timescaleHz == 0 || pFlv->sampledeltaHz == 0) {
          pMedia->vid.generic.fps = 0;
        } else {
          pMedia->vid.generic.fps = (float) pFlv->timescaleHz /  pFlv->sampledeltaHz;
        }

        strncpy(pMedia->vid.codec, codecType_getCodecDescrStr(MEDIA_FILE_TYPE_H264b),
                sizeof(pMedia->vid.codec) - 1);

      }

      if(pFlv->audCfg.audDescr.clockHz != 0) {
        pMedia->haveAud = 1;
        pMedia->aud.generic.clockHz = pFlv->audCfg.audDescr.clockHz;
        pMedia->aud.generic.channels = pFlv->audCfg.audDescr.channels;
        strncpy(pMedia->aud.codec, codecType_getCodecDescrStr(pMedia->aud.generic.mediaType),
                sizeof(pMedia->aud.codec) - 1);

      }

      flv_close(pFlv);
      break;

    case MEDIA_FILE_TYPE_WEBM:
    case MEDIA_FILE_TYPE_MKV:

      if((pMkv = mkv_open(pFileStream->filename)) == NULL) {
        return -1;
      }

      pMedia->durationSec = (double) pMkv->seg.info.duration / 1000.0f;
      if(pMedia->durationSec >= MKV_SEGINFO_DURATION_DEFAULT / 1000.0f) {
        pMedia->durationSec = 0; // Duration is actually not known
      }
      pMedia->totSz = pFileStream->size;
      pMedia->dataOffset = pMkv->parser.mbs.offset;

      for(idx = 0; idx < pMkv->seg.tracks.arrTracks.count; idx++) {
        if((pMkvTrack = &((MKV_TRACK_T *) pMkv->seg.tracks.arrTracks.parr)[idx])) {
          if(pMkvTrack->type == MKV_TRACK_TYPE_VIDEO) {
            pMedia->haveVid = 1;
            pMedia->vid.generic.resH = pMkvTrack->video.pixelWidth;
            pMedia->vid.generic.resV = pMkvTrack->video.pixelHeight;
            strncpy(pMedia->vid.codec, codecType_getCodecDescrStr(pMkvTrack->codecType),
                sizeof(pMedia->vid.codec) - 1);
          } else if(pMkvTrack->type == MKV_TRACK_TYPE_VIDEO) {
            pMedia->haveAud = 1;
            pMedia->aud.generic.clockHz = pMkvTrack->audio.samplingFreq;
            pMedia->aud.generic.channels = pMkvTrack->audio.channels;
            strncpy(pMedia->aud.codec, codecType_getCodecDescrStr(pMkvTrack->codecType),
                sizeof(pMedia->aud.codec) - 1);
          }
        }

      }

      mkv_close(pMkv);

      break;

    case MEDIA_FILE_TYPE_MP2TS:
    case MEDIA_FILE_TYPE_MP2TS_BDAV:

      pMedia->dataOffset = 0;
      pMedia->durationSec = 0;
      pMedia->totSz = pFileStream->size;

      if((pMp2ts = mp2ts_openfrommeta(pFileStream->filename)) == NULL) {
        // Read no more than the first 3000 frames
        // to probe vid & aud state
        pMp2ts = mp2ts_open(pFileStream->filename, NULL,3000, 0);
      }

      get_mp2ts_vidaudprop(pMp2ts, pMedia);

      if(pMp2ts) {
        pMedia->durationSec = (double) pMp2ts->durationHz / 90000.0f;
        mp2ts_close(pMp2ts);
      }

      break;

    case MEDIA_FILE_TYPE_H264b:
      pMedia->totSz = pFileStream->size;
      pMedia->durationSec = 0;
      pMedia->dataOffset = 0;
      pMedia->haveVid = 1;
      strncpy(pMedia->vid.codec, codecType_getCodecDescrStr(pMedia->type), 
                sizeof(pMedia->vid.codec) - 1);
      break;

    case MEDIA_FILE_TYPE_AAC_ADTS:
      pMedia->totSz = pFileStream->size;
      pMedia->durationSec = 0;
      pMedia->dataOffset = 0;
      pMedia->haveAud = 1;
      strncpy(pMedia->aud.codec, codecType_getCodecDescrStr(pMedia->type), 
                sizeof(pMedia->aud.codec) - 1);
      break;

    case MEDIA_FILE_TYPE_H262:
    case MEDIA_FILE_TYPE_AC3:
    case MEDIA_FILE_TYPE_MPEG2:
    case MEDIA_FILE_TYPE_M3U:

      pMedia->totSz = pFileStream->size;
      break;

    case MEDIA_FILE_TYPE_SDP:
      pMedia->totSz = pFileStream->size;
      rc = 0;
      break;
    default:
      LOG(X_WARNING("Unsupported media type %s"), pFileStream->filename);
      rc = -1;
      break;
  }

  return rc;
}

int codectype_isVid(XC_CODEC_TYPE_T codecType) {
  switch(codecType) {
    case XC_CODEC_TYPE_H264:
    case XC_CODEC_TYPE_H263:
    case XC_CODEC_TYPE_H263_PLUS:
    case XC_CODEC_TYPE_H262:
    case XC_CODEC_TYPE_MPEG4V:
    case XC_CODEC_TYPE_VP6:
    case XC_CODEC_TYPE_VP6F:
    case XC_CODEC_TYPE_VP8:
    case XC_CODEC_TYPE_VID_GENERIC:
    case XC_CODEC_TYPE_VID_CONFERENCE:
      return 1;
    default:
      return 0;
  }
  return 0;
}

int codectype_isAud(XC_CODEC_TYPE_T codecType) {
  switch(codecType) {
    case XC_CODEC_TYPE_AAC:
    case XC_CODEC_TYPE_AC3:
    case XC_CODEC_TYPE_MPEGA_L2:
    case XC_CODEC_TYPE_MPEGA_L3:
    case XC_CODEC_TYPE_VORBIS:
    case XC_CODEC_TYPE_SILK:
    case XC_CODEC_TYPE_OPUS:
    case XC_CODEC_TYPE_AMRNB:
    case XC_CODEC_TYPE_G711_MULAW:
    case XC_CODEC_TYPE_G711_ALAW:
    case XC_CODEC_TYPE_TELEPHONE_EVENT:
    case XC_CODEC_TYPE_AUD_GENERIC:
    case XC_CODEC_TYPE_AUD_CONFERENCE:
      return 1;
    default:
      return 0;
  }
  return 0;
}
