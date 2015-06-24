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


int mkv_write_header(const MKV_HEADER_T *pMkvHdr, EBML_BYTE_STREAM_T *mbs) {
  int rc = 0;
  unsigned int pos;
  const unsigned int hdrszlen = 8;
  EBML_BYTE_STREAM_T mbstmp;
  unsigned int mark;

  if(!pMkvHdr || !mbs || !mbs->bs.buf) {
    return -1;
  }

  pos = mbs->bs.idx;

  if(rc >= 0) {
    rc = ebml_write_id(mbs,  MKV_ID_HEADER);
  }

  mark = mbs->bs.idx;
  mbs->bs.idx += hdrszlen;

  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_EBMLVERSION, pMkvHdr->version);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_EBMLREADVERSION, pMkvHdr->readVersion);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_EBMLMAXIDLENGTH, pMkvHdr->maxIdLength);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_EBMLMAXSIZELENGTH, pMkvHdr->maxSizeLength);
  }
  if(rc >= 0) {
    rc = ebml_write_string(mbs,  MKV_ID_DOCTYPE, pMkvHdr->docType);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_DOCTYPEVERSION, pMkvHdr->docVersion);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_DOCTYPEREADVERSION, pMkvHdr->docReadVersion);
  }

  //
  //
  // Fill in the MKV_ID_HEADER level length
  if(rc >= 0) {
    memcpy(&mbstmp, mbs, sizeof(mbstmp));
    mbstmp.pfs = NULL;
    mbstmp.bs.idx = mark;
    mbstmp.bs.sz = mbstmp.bs.idx + hdrszlen;
    rc = ebml_write_num(&mbstmp, mbs->bs.idx - mark - hdrszlen, hdrszlen);
  }

  if(rc >= 0) {
    return (mbs->bs.idx - pos);
  } else {
    return rc;
  }
}

static int mkv_write_track_audio(const MKV_TRACK_AUD_T *pTrackA, EBML_BYTE_STREAM_T *mbs) {
  int rc = 0;
  const unsigned int hdrszlen = 8;
  EBML_BYTE_STREAM_T mbstmp;
  unsigned int pos;
  unsigned int mark;

  pos = mbs->bs.idx;

  if(rc >= 0) {
    rc = ebml_write_id(mbs,  MKV_ID_TRACKAUDIO);
  }

  mark = mbs->bs.idx;
  mbs->bs.idx += hdrszlen;

  if(rc >= 0) {
    rc = ebml_write_double(mbs,  MKV_ID_AUDIOSAMPLINGFREQ, pTrackA->samplingFreq);
  }
  if(rc >= 0) {
    rc = ebml_write_double(mbs,  MKV_ID_AUDIOOUTSAMPLINGFREQ, pTrackA->samplingOutFreq);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_AUDIOCHANNELS, pTrackA->channels);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_AUDIOBITDEPTH, pTrackA->bitDepth);
  }

  //
  // Fill in the id level length
  //
  if(rc >= 0) {
    memcpy(&mbstmp, mbs, sizeof(mbstmp));
    mbstmp.pfs = NULL;
    mbstmp.bs.idx = mark;
    mbstmp.bs.sz = mbstmp.bs.idx + hdrszlen;
    rc = ebml_write_num(&mbstmp, mbs->bs.idx - mark - hdrszlen, hdrszlen);
  }

  if(rc >= 0) {
    return (mbs->bs.idx - pos);
  }

  return rc;
}

static int mkv_write_track_video(const MKV_TRACK_VID_T *pTrackV, EBML_BYTE_STREAM_T *mbs) {
  int rc = 0;
  const unsigned int hdrszlen = 8;
  EBML_BYTE_STREAM_T mbstmp;
  unsigned int pos;
  unsigned int mark;

  pos = mbs->bs.idx;

  if(rc >= 0) {
    rc = ebml_write_id(mbs,  MKV_ID_TRACKVIDEO);
  }

  mark = mbs->bs.idx;
  mbs->bs.idx += hdrszlen;

  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_VIDEOPIXELWIDTH, pTrackV->pixelWidth);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_VIDEOPIXELHEIGHT, pTrackV->pixelHeight);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_VIDEODISPLAYWIDTH, pTrackV->displayWidth);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_VIDEODISPLAYHEIGHT, pTrackV->displayHeight);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_VIDEODISPLAYUNIT, pTrackV->displayUnit);
  }

  //
  // Fill in the id level length
  //
  if(rc >= 0) {
    memcpy(&mbstmp, mbs, sizeof(mbstmp));
    mbstmp.pfs = NULL;
    mbstmp.bs.idx = mark;
    mbstmp.bs.sz = mbstmp.bs.idx + hdrszlen;
    rc = ebml_write_num(&mbstmp, mbs->bs.idx - mark - hdrszlen, hdrszlen);
  }

  if(rc >= 0) {
    return (mbs->bs.idx - pos);
  }

  return rc;
}

static int mkv_write_track(const MKV_TRACK_T *pTrack, EBML_BYTE_STREAM_T *mbs) {
  int rc = 0;
  const unsigned int hdrszlen = 8;
  EBML_BYTE_STREAM_T mbstmp;
  unsigned int pos;
  unsigned int mark;

  pos = mbs->bs.idx;

  if(rc >= 0) {
    rc = ebml_write_id(mbs,  MKV_ID_TRACKENTRY);
  }

  mark = mbs->bs.idx;
  mbs->bs.idx += hdrszlen;

  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_TRACKNUMBER, pTrack->number);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_TRACKTYPE, pTrack->type);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_TRACKUID, pTrack->uid);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_TRACKFLAGLACING, pTrack->flagLacing);
  }
  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_TRACKDEFAULTDURATION, pTrack->defaultDuration);
  }
  if(rc >= 0) {
    rc = ebml_write_double(mbs,  MKV_ID_TRACKTIMECODESCALE, pTrack->timeCodeScale);
  }
  if(rc >= 0) {
    rc = ebml_write_string(mbs,  MKV_ID_TRACKLANGUAGE, pTrack->language);
  }
  if(rc >= 0) {
    rc = ebml_write_string(mbs,  MKV_ID_CODECID, pTrack->codecId);
  }

  if(pTrack->type == MKV_TRACK_TYPE_VIDEO && rc >= 0) {
    rc = mkv_write_track_video(&pTrack->video, mbs);
  } else if(pTrack->type == MKV_TRACK_TYPE_AUDIO && rc >= 0) {
    rc = mkv_write_track_audio(&pTrack->audio, mbs);
  }

  if(rc >= 0) {
    rc = ebml_write_binary(mbs,  MKV_ID_CODECPRIVATE, pTrack->codecPrivate.p, pTrack->codecPrivate.size);
  }

  //
  // Fill in the id level length
  //
  if(rc >= 0) {
    memcpy(&mbstmp, mbs, sizeof(mbstmp));
    mbstmp.pfs = NULL;
    mbstmp.bs.idx = mark;
    mbstmp.bs.sz = mbstmp.bs.idx + hdrszlen;
    rc = ebml_write_num(&mbstmp, mbs->bs.idx - mark - hdrszlen, hdrszlen);
  }

  if(rc >= 0) {
    return (mbs->bs.idx - pos);
  }

  return rc;
}

static int mkv_write_tracks(const MKV_TRACKS_T *pTracks, EBML_BYTE_STREAM_T *mbs) {
  int rc = 0;
  const unsigned int hdrszlen = 8;
  EBML_BYTE_STREAM_T mbstmp;
  unsigned int pos;
  unsigned int mark;
  unsigned int idx;

  pos = mbs->bs.idx; 

  if(rc >= 0) {
    rc = ebml_write_id(mbs,  MKV_ID_TRACKS);
  }

  mark = mbs->bs.idx;
  mbs->bs.idx += hdrszlen;

  for(idx = 0; idx < pTracks->arrTracks.count; idx++) {
    if(rc >= 0) {
      rc = mkv_write_track(&((MKV_TRACK_T *) pTracks->arrTracks.parr)[idx], mbs);
    }
  }

  //
  // Fill in the id level length
  //
  if(rc >= 0) {
    memcpy(&mbstmp, mbs, sizeof(mbstmp));
    mbstmp.pfs = NULL;
    mbstmp.bs.idx = mark;
    mbstmp.bs.sz = mbstmp.bs.idx + hdrszlen;
    rc = ebml_write_num(&mbstmp, mbs->bs.idx - mark - hdrszlen, hdrszlen);
  }

  if(rc >= 0) {
    return (mbs->bs.idx - pos);
  } 

  return rc;
}

static int mkv_write_segment_info(const MKV_SEGINFO_T *pInfo, EBML_BYTE_STREAM_T *mbs) {
  int rc = 0;
  const unsigned int hdrszlen = 8;
  EBML_BYTE_STREAM_T mbstmp;
  unsigned int pos;
  unsigned int mark;

  pos = mbs->bs.idx;

  if(rc >= 0) {
    rc = ebml_write_id(mbs,  MKV_ID_INFO);
  }

  mark = mbs->bs.idx;
  mbs->bs.idx += hdrszlen;

  if(rc >= 0) {
    rc = ebml_write_uint(mbs,  MKV_ID_TIMECODESCALE, pInfo->timeCodeScale);
  }

  if(rc >= 0) {
    rc = ebml_write_double(mbs,  MKV_ID_DURATION, pInfo->duration);
  }

  if(rc >= 0) {
    rc = ebml_write_string(mbs,  MKV_ID_TITLE, pInfo->title);
  }

  if(rc >= 0) {
    rc = ebml_write_string(mbs,  MKV_ID_MUXINGAPP, pInfo->muxingApp);
  }

  if(rc >= 0) {
    rc = ebml_write_string(mbs,  MKV_ID_WRITINGAPP, pInfo->writingApp);
  }

  if(rc >= 0) {
    rc = ebml_write_binary(mbs,  MKV_ID_SEGMENTUID, pInfo->segUid, sizeof(pInfo->segUid));
  }

  //
  // Fill in the id level length
  //
  if(rc >= 0) {
    memcpy(&mbstmp, mbs, sizeof(mbstmp));
    mbstmp.pfs = NULL;
    mbstmp.bs.idx = mark;
    mbstmp.bs.sz = mbstmp.bs.idx + hdrszlen;
    rc = ebml_write_num(&mbstmp, mbs->bs.idx - mark - hdrszlen, hdrszlen);
  }

  if(rc >= 0) {
    return (mbs->bs.idx - pos);
  } 

  return rc;
}

int mkv_write_segment(const MKV_SEGMENT_T *pSeg, EBML_BYTE_STREAM_T *mbs) {
  int rc = 0;
  const unsigned int hdrszlen = 8;
  EBML_BYTE_STREAM_T mbstmp;
  unsigned int pos;
  unsigned int mark;

  if(!pSeg || !mbs || !mbs->bs.buf) {
    return -1;
  }

  pos = mbs->bs.idx; 

  if(rc >= 0) {
    rc = ebml_write_id(mbs,  MKV_ID_SEGMENT);
  }

  mark = mbs->bs.idx;
  mbs->bs.idx += hdrszlen;

  if(rc >= 0) {
    rc = mkv_write_segment_info(&pSeg->info, mbs);
  }

  if(rc >= 0) {
    rc = mkv_write_tracks(&pSeg->tracks, mbs);
  }

  //
  // Fill in the id level length
  //
  if(rc >= 0) {
    memcpy(&mbstmp, mbs, sizeof(mbstmp));
    mbstmp.pfs = NULL;
    mbstmp.bs.idx = mark;
    mbstmp.bs.sz = mbstmp.bs.idx + hdrszlen;
    //rc = ebml_write_num(&mbstmp, mbs->bs.idx - mark - hdrszlen, hdrszlen);
    rc = ebml_write_num(&mbstmp, EBML_MAX_NUMBER, 8);
  }

  if(rc >= 0) {
    return (mbs->bs.idx - pos);
  } 

  return rc;
}

#if 0
static char *testerw() {
  FILE_STREAM_T fs;
  EBML_BYTE_STREAM_T mbs;
  unsigned int mark;
  unsigned int hdrszlen = 8;
  unsigned int idx;
  unsigned char buf[1024];
  char tmp[128];
  unsigned char audioPriv[] = { 0x13, 0x10, 0x56, 0x35, 0x9d, 0x48, 0x00 };
  unsigned char videoPriv[] = 
      { 0x01, 0x4d, 0x40, 0x33, 0xff, 0xe1, 0x00, 0x17, 0x67, 0x4d, 0x40, 0x33, 0x9a, 0x73, 0x80, 0xa0,
        0x08, 0xb4, 0x20, 0x00, 0x32, 0xe9, 0xe0, 0x09, 0x89, 0x68, 0x11, 0xe3, 0x06, 0x23, 0xc0, 0x01,
        0x00, 0x04, 0x68, 0xee, 0x3c, 0x80 };
  int rc = 0;
  MKV_HEADER_T hdr;
  MKV_SEGMENT_T seg;
  MKV_TRACK_T tracks[2];
  //MKV_TAG_T tags[3];

//return testdbl();

  memset(&fs, 0,sizeof(fs));

  if((fs.fp = fileops_Open("testout.mkv", O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    return NULL;
  }

  memset(buf, 0, sizeof(buf));
  memset(&mbs, 0, sizeof(mbs));
  //mbs.pfs = &fs;
  mbs.bs.buf = buf;
  mbs.bs.sz = sizeof(buf);
  mbs.bufsz = mbs.bs.sz; 

  memset(&hdr, 0, sizeof(hdr));
  hdr.version = EBML_VERSION;
  hdr.readVersion = EBML_VERSION;
  hdr.maxIdLength = EBML_MAX_ID_LENGTH;
  hdr.maxSizeLength = EBML_MAX_SIZE_LENGTH;
  hdr.docVersion = EBML_VERSION; 
  hdr.docReadVersion = EBML_VERSION;
  strcpy(hdr.docType, "matroska");

  rc = mkv_write_header(&hdr, &mbs);

  memset(&seg, 0, sizeof(seg));
  seg.info.timeCodeScale = 1000000; // in ns (1 ms : all timecodes in the segment are expressed in ms)
  seg.info.duration = 0;
  strncpy(seg.info.title, "", sizeof(seg.info.title) - 1);
  strncpy(seg.info.muxingApp, vsxlib_get_appnamestr(tmp, sizeof(tmp)), sizeof(seg.info.muxingApp) - 1);
  strncpy(seg.info.writingApp, vsxlib_get_appnamestr(tmp, sizeof(tmp)), sizeof(seg.info.writingApp) - 1);
  for(idx = 0; idx < sizeof(seg.info.segUid) / sizeof(uint16_t); idx++) {
    *((uint16_t *) &seg.info.segUid[idx]) = random() % RAND_MAX;
  }

  memset(tracks, 0, sizeof(tracks));
  tracks[0].number = 1;
  tracks[0].type = MKV_TRACK_TYPE_VIDEO;
  tracks[0].uid = 1;
  tracks[0].defaultDuration = 0;
  tracks[0].timeCodeScale = 1;
  strncpy(tracks[0].language, "und", sizeof(tracks[0].language) - 1);
  strncpy(tracks[0].codecId, mkv_getCodecStr(XC_CODEC_TYPE_H264), sizeof(tracks[0].codecId) - 1);
  tracks[0].codecPrivate.nondynalloc = 1;
  tracks[0].codecPrivate.p = videoPriv;
  tracks[0].codecPrivate.size = sizeof(videoPriv);
  tracks[0].video.pixelWidth = 1280;
  tracks[0].video.pixelHeight = 544;

  tracks[1].number = 2;
  tracks[1].type = MKV_TRACK_TYPE_AUDIO;
  tracks[1].uid = 2;
  tracks[1].defaultDuration = 0;
  tracks[1].timeCodeScale = 1;
  strncpy(tracks[1].language, "und", sizeof(tracks[1].language) - 1);
  strncpy(tracks[1].codecId, mkv_getCodecStr(XC_CODEC_TYPE_AAC), sizeof(tracks[1].codecId) - 1);
  tracks[1].codecPrivate.nondynalloc = 1;
  tracks[1].codecPrivate.p = audioPriv;
  tracks[1].codecPrivate.size = sizeof(audioPriv);
  tracks[1].audio.samplingFreq = 24000;
  tracks[1].audio.samplingOutFreq = 48000;
  tracks[1].audio.channels = 2;

  seg.tracks.arrTracks.parr = tracks;
  seg.tracks.arrTracks.count = 2;
  seg.tracks.arrTracks.nondynalloc = 1;

  //memset(tags, 0, sizeof(tags));
  //tags[0].tagTarget.trackUid = 0;
  //tags[0].tagTarget.chapterUid = 0;
  //tags[0].tagTarget.attachUid = 0;

  rc = mkv_write_segment(&seg, &mbs);

  fprintf(stderr, "mkv_write_header rc:%d\n", rc);
  mbs.pfs = &fs;
  ebml_write_flush(&mbs);

  //avc_dumpHex(stderr, mbs.bs.buf, mbs.bs.idx, 1);

  CloseMediaFile(&fs);

  return NULL;
}
#endif // 0
