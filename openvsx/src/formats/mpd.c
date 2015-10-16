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

#define MPD_XMLNS               "urn:mpeg:dash:schema:mpd:2011"
#define MPD_XMLNS_XSI           "http://www.w3.org/2001/XMLSchema-instance"
#define MPD_XMLNS_XLINK         "http://www.w3.org/1999/xlink"
#define MPD_XSI_SCHEMALOCATION  MPD_XMLNS" http://standards.iso.org/ittf/PubliclyAvailableStandards/MPEG-DASH_schema_files/DASH-MPD.xsd"
#define MPD_PROFILE_LIVE_MP4    "urn:mpeg:dash:profile:isoff-live:2011"
#define MPD_PROFILE_FULL_MP4    "urn:mpeg:dash:profile:full:2011"
#define MPD_PROFILE_FULL_TS     "urn:mpeg:dash:profile:mp2t-main:2011"

#define MPD_REPRESENTATION_ID_STR      "RepresentationID"

#define MPD_EOL "\r\n"
#define MPD_WRITE(s, arg...)  if((rc = snprintf(&buf[idx], szbuf - idx, s, ##arg)) > 0) { idx += rc; }
#define MPD_WRITE_RET(s, arg...)  if((rc = snprintf(&buf[idx], szbuf - idx, s, ##arg)) > 0) { idx += rc; } \
                                   else { return -1; }

typedef enum DASH_INITIALIZER_TYPE {
  DASH_INITIALIZER_TYPE_NONE       = 0,
  DASH_INITIALIZER_TYPE_ELEMENT    = 1,
  DASH_INITIALIZER_TYPE_ATTRIB     = 2,
} DASH_INITIALIZER_TYPE_T;

typedef struct REPRESENTATION_PROPS {
  unsigned int      repId;
  int               width;
  int               height;
  unsigned int      audioClockHz;
  char             *innerTag;
  char              codecInitV[48]; 
  char              codecInitA[48]; 
  float             rateBps;
} REPRESENTATION_PROPS_T;


static const char *mpd_type_to_filename(DASH_MPD_TYPE_T mpdType);

const char *mpd_path_get_adaptationtag(const char *path) {
  const char *p;

  if(!(p = strutil_getFileExtension(path))) {
    return NULL;
  }
  p--;

  while(p > path && *(p + 1) != '_') {
    p--; 
  }

  return p;
}

int mpd_format_path_prefix(char *buf, 
                           unsigned int szbuf, 
                           int outidx, 
                           const char *prefix, 
                           const char *adaptationtag) {
  int rc;
  char repIdStr[32];

  if(outidx >= 0) {
    //
    // [outidx+1][out][a|v]_$[Number|Time]$.m4s
    //
    snprintf(repIdStr, sizeof(repIdStr), "%d", outidx + 1);
  } else {
    //
    // $RepresentationID$[out][a|v]_$[Number|Time]$.m4s
    //
    snprintf(repIdStr, sizeof(repIdStr), "$" MPD_REPRESENTATION_ID_STR "$");
  }

  rc = snprintf(buf, szbuf, "%s%s%s_", repIdStr, prefix, adaptationtag ? adaptationtag : "");

  return rc;
}

int mpd_format_path(char *buf, 
                    unsigned int szbuf, 
                    CB_MPD_FORMAT_PATH_PREFIX cbFormatPathPrefix,
                    int outidx, 
                    const char *prefix, 
                    const char *strIndex, 
                    const char *suffix, 
                    const char *adaptationtag) {
  size_t szret = 0;
  int rc = 0;  

  if(!cbFormatPathPrefix) {
    return -1;
  }

  if((szret = cbFormatPathPrefix(buf, szbuf, outidx, prefix, adaptationtag)) < 0) {
    return szret;
  }

  if((rc = snprintf(&buf[szret], szbuf - szret, "%s.%s", strIndex, suffix)) > 0) {
    szret += rc; 
  } else {
    szret = rc;
  }

  return szret;
}

static int writepath(const MPD_CREATE_CTXT_T *pCtxt, 
                     int outidx,
                     const char *txt, 
                     char *buf, 
                     unsigned int szbuf, 
                     const char *adaptationtag) {
  int rc = 0;
  const char *uriprfxdelimeter = "";
  char tmp[VSX_MAX_PATH_LEN];
  char tokenstr[16 + META_FILE_TOKEN_LEN];

  if(pCtxt->init.uriprefix && (rc = strlen(pCtxt->init.uriprefix)) > 0 &&
     pCtxt->init.uriprefix[rc - 1] != '/') {
    uriprfxdelimeter = "/";
  }

  tokenstr[0] = '\0';
  if(srv_write_authtoken(tokenstr, sizeof(tokenstr), pCtxt->init.pAuthTokenId, NULL, 1) < 0 || 
     (rc = mpd_format_path(tmp, sizeof(tmp), pCtxt->cbFormatPathPrefix, outidx,
                           pCtxt->init.outfileprefix, txt, pCtxt->init.outfilesuffix, adaptationtag)) < 0 ||
    (rc = snprintf(buf, szbuf, "%s%s%s%s%s",
             (pCtxt->init.uriprefix ? pCtxt->init.uriprefix : ""), uriprfxdelimeter, tmp,
             tokenstr[0] != '\0' ? "?" : "", tokenstr)) < 0) {
    return rc;
  }

  return rc;
}

static int write_init_path(const MPD_CREATE_CTXT_T *pCtxt, 
                           int outidx, 
                           char *buf, 
                           unsigned int szbuf, 
                           const char *adaptationtag) {

  return writepath(pCtxt, outidx, "init", buf, szbuf, adaptationtag);
}

static int write_mpd_path(const MPD_CREATE_CTXT_T *pCtxt, 
                          int outidx, 
                          DASH_MPD_TYPE_T mpdType,
                          char *buf, 
                          unsigned int szbuf) {

  int rc = 0;
  char stroutidx[32];
  char mpdfilename[VSX_MAX_PATH_LEN];

  if(outidx >= 0) {
    snprintf(stroutidx, sizeof(stroutidx), "%d", outidx + 1);
  } else {
    stroutidx[0] = '\0';
  }

  snprintf(mpdfilename, sizeof(mpdfilename), "%s%s", stroutidx, mpd_type_to_filename(mpdType));

  mediadb_prepend_dir(pCtxt->init.outdir, mpdfilename, buf, szbuf);

  return rc;
}

static float getMediaRateBps(const MPD_CREATE_CTXT_T *pCtxt, 
                             unsigned int outidx, 
                             const MPD_ADAPTATION_T *pAdaptation) {
  FILE_OFFSET_T size = 0;
  struct stat st;
  float durationSec = 0;
  float rateBps = 0;
  char tmp[256];
  char mediaPath[VSX_MAX_PATH_LEN];

  mpd_format_path(tmp, sizeof(tmp), mpd_format_path_prefix, outidx,
                    pCtxt->init.outfileprefix, pAdaptation->pMedia->mediaUniqIdstr, pCtxt->init.outfilesuffix,
                    pAdaptation->padaptationTag);
  mediadb_prepend_dir(pAdaptation->pMedia->mediaDir, tmp, mediaPath, sizeof(mediaPath));

 if(mediaPath[0] != '\0' && fileops_stat(mediaPath, &st) == 0) {
    size = (FILE_OFFSET_T) st.st_size;
  }

  //LOG(X_DEBUG("GETMEDIARATEBPS mediaPath: '%s', size: %lld"), mediaPath, size);

  if(pAdaptation->pMedia->timescale > 0) {
    durationSec = (float) ((double) pAdaptation->pMedia->duration / pAdaptation->pMedia->timescale);
  }

  if(durationSec > 0) {
    rateBps = 8 * size / durationSec;
  }

  return rateBps;
}

static int write_tag_segmententry(uint32_t timescale, 
                                  uint64_t duration, 
                                  uint64_t durationPreceeding, 
                                  char *buf, 
                                  unsigned int szbuf) {
  int rc = 0;
  unsigned int idx = 0;

  MPD_WRITE_RET("<S t=\"%llu\" d=\"%llu\"/>"MPD_EOL, durationPreceeding, duration);

  return idx > 0 ? idx : rc;
}

static int is_representation_same(const REPRESENTATION_PROPS_T *pRep1, 
                                  const REPRESENTATION_PROPS_T *pRep2) {

  if(pRep1->width == pRep2->width && pRep1->height == pRep2->height &&
     pRep1->audioClockHz == pRep2->audioClockHz &&
     pRep1->rateBps == pRep2->rateBps &&
     !strcmp(pRep1->codecInitV, pRep2->codecInitV) && !strcmp(pRep1->codecInitA, pRep2->codecInitA)) {
    return 1;
  }

  return 0;
}

static int create_representation(const MPD_CREATE_CTXT_T *pCtxt, 
                                 const MPD_ADAPTATION_T *pAdaptation,
                                 unsigned int outidx, 
                                 REPRESENTATION_PROPS_T *pRep) {
  int rc = 0;

  pRep->rateBps = getMediaRateBps(pCtxt, outidx, pAdaptation);
  pRep->repId = outidx + 1;

  if(pAdaptation->isvid) {
    pRep->width = pCtxt->pAvCtxt->vid.pStreamerCfg->status.vidProps[outidx].resH;
    pRep->height = pCtxt->pAvCtxt->vid.pStreamerCfg->status.vidProps[outidx].resV;

    if(pCtxt->pAvCtxt->vid.codecType == XC_CODEC_TYPE_H264) {
      snprintf(pRep->codecInitV, sizeof(pRep->codecInitV), "avc1.%.2x%.2x%.2x",
              pCtxt->pAvCtxt->vid.codecCtxt.h264.avcc.avcprofile,
              pCtxt->pAvCtxt->vid.codecCtxt.h264.avcc.profilecompatibility,
              pCtxt->pAvCtxt->vid.codecCtxt.h264.avcc.avclevel);
    }
  } 
  if(pAdaptation->isaud) {
    if(pCtxt->pAvCtxt->aud.codecType == XC_CODEC_TYPE_AAC) {
      snprintf(pRep->codecInitA, sizeof(pRep->codecInitA), "mp4a.%.1x.%.1x",
         ESDS_OBJTYPE_MP4A, pCtxt->pAvCtxt->aud.codecCtxt.aac.adtsFrame.sp.objTypeIdx);
    }
    pRep->audioClockHz = pAdaptation->pMedia->timescale;

  }

  //pRep->innerTag = "<AudioChannelConfiguration schemeIdUri=\"urn:mpeg:dash:23003:3:audio_channel_configuration:2011\" value=\"2\"/>";

  return rc;
}

static int write_tag_representation(const MPD_CREATE_CTXT_T *pCtxt, 
                                    const REPRESENTATION_PROPS_T *pRep,
                                    char *buf, unsigned int szbuf) {

  int rc = 0;
  unsigned int idx = 0;

  MPD_WRITE_RET("<Representation id=\"%d\"", pRep->repId);

  if(pRep->width > 0) {
    MPD_WRITE_RET(" width=\"%d\"", pRep->width);
  }
  if(pRep->height > 0) {
    MPD_WRITE_RET(" height=\"%d\"", pRep->height);
  }

  if(pRep->codecInitV[0] != '\0' || pRep->codecInitA[0] != '\0') {
    MPD_WRITE_RET(" codecs=\"%s%s%s\"", pRep->codecInitV[0] ? pRep->codecInitV : "",
           (pRep->codecInitV[0] && pRep->codecInitA[0]) ? "," : "", pRep->codecInitA[0] ? pRep->codecInitA: "");
  }

  if(pRep->audioClockHz > 0) {
    MPD_WRITE_RET(" audioSamplingRate=\"%d\"", pRep->audioClockHz);
  }

  if(pRep->rateBps > 0) {
    MPD_WRITE_RET(" bandwidth=\"%u\"", (unsigned int) pRep->rateBps);
  }

  if(pRep->innerTag) {
    MPD_WRITE_RET(">"MPD_EOL"%s"MPD_EOL"</Representation>"MPD_EOL, pRep->innerTag); 
  } else {
    MPD_WRITE_RET("/>"MPD_EOL); // End of Representation tag
  }


  return idx;
}

static int write_representations(const MPD_CREATE_CTXT_T *pCtxt, 
                                 const MPD_ADAPTATION_T *pAdaptation,
                                 int outidx, 
                                 char *buf, unsigned int szbuf) {

  int rc = 0;
  unsigned int idx = 0; 
  REPRESENTATION_PROPS_T *pRepPrev = NULL;
  REPRESENTATION_PROPS_T reps[IXCODE_VIDEO_OUT_MAX];

  memset(reps, 0, sizeof(reps));

  if(outidx >= 0) {

    //
    // We're writing an MPD with just one representation
    //
    if((rc = create_representation(pCtxt, pAdaptation, outidx, &reps[outidx])) < 0) {
      return rc;
    }

    if((rc = write_tag_representation(pCtxt, &reps[outidx], &buf[idx], szbuf - idx)) < 0) {
      return rc;
    }
    idx += rc;

  } else {

    //
    // We're writing an MPD with multiple representations 
    //
    for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

      if(outidx > 0 && !pCtxt->pAvCtxt->vid.pStreamerCfg->xcode.vid.out[outidx].active) {
        continue;
      }

      if((rc = create_representation(pCtxt, pAdaptation, outidx, &reps[outidx])) < 0) {
        return rc;
      }

      if(!pRepPrev || !is_representation_same(&reps[outidx], pRepPrev)) {

        if((rc = write_tag_representation(pCtxt, &reps[outidx], &buf[idx], szbuf - idx)) < 0) {
          return rc;
        }
        idx += rc;
      }
      pRepPrev =  &reps[outidx];

    } // end for

  }

  return idx;
}

static int write_tag_segmenttemplate_start(const MPD_MEDIA_FILE_T *pMediaDescr,
                                           const char *mediaUrl, 
                                           const char *initMediaUrl, int startNumber,
                                           char *buf, unsigned int szbuf) {   
  int rc = 0;
  unsigned int idx = 0;

  MPD_WRITE_RET("<SegmentTemplate");
  if(pMediaDescr->timescale > 0 && pMediaDescr->duration > 0) {
    MPD_WRITE_RET(" timescale=\"%u\"" , pMediaDescr->timescale);
  }

  MPD_WRITE_RET(" media=\"%s\"", mediaUrl);

  if(startNumber >= 0) {
    MPD_WRITE_RET(" startNumber=\"%d\"" , startNumber);
  }

  if(initMediaUrl && initMediaUrl[0] != '\0') {
    MPD_WRITE_RET(" initialization=\"%s\"", initMediaUrl);
  }

  MPD_WRITE_RET(">"MPD_EOL); // End of SegmentTemplate tag

  return idx > 0 ? idx : rc;
}

static int write_segmenttemplate_bytime(MPD_CREATE_CTXT_T *pCtxt, 
                                        const MPD_ADAPTATION_T *pAdaptation,
                                        const MPD_SEGMENT_LIST_T *pSegs,
                                        const char *initMediaUrl,
                                        char *buf, unsigned int szbuf) {
  int rc = 0;
  unsigned int idx = 0;
  unsigned int indexInSegments;
  unsigned int indexSegment = 0;
  char mediaUrl[VSX_MAX_PATH_LEN];

  if((rc = writepath(pCtxt, -1, "$Time$", mediaUrl, sizeof(mediaUrl), pAdaptation->padaptationTag)) < 0) {
    return rc;
  }

  if((rc = write_tag_segmenttemplate_start(pAdaptation->pMedia, mediaUrl,
                                           initMediaUrl, -1, &buf[idx], szbuf - idx)) < 0) {
    return rc;
  }
  idx += rc;

  if(pSegs->indexInSegments > 0) {
    indexInSegments = pSegs->indexInSegments - 1;
  } else {
    indexInSegments = pSegs->indexCount - 1;
  }

  MPD_WRITE_RET("<SegmentTimeline>"MPD_EOL);

  for(indexSegment = 0; indexSegment < pSegs->indexCount; indexSegment++) {

    if(++indexInSegments >= pSegs->indexCount) {
      indexInSegments = 0;
    }

    if(!pSegs->pSegments[indexInSegments].valid) {
      continue;
    }

    if((rc = write_tag_segmententry(pSegs->pSegments[indexInSegments].media.timescale,
                                    pSegs->pSegments[indexInSegments].media.duration,
                                    pSegs->pSegments[indexInSegments].media.durationPreceeding,
                                    &buf[idx], szbuf - idx)) < 0) {
      return rc;
    }
    idx += rc;

  } // end of for(indexSegment...

  MPD_WRITE_RET("</SegmentTimeline>"MPD_EOL);

  MPD_WRITE_RET("</SegmentTemplate>"MPD_EOL);

  return idx > 0 ? idx : rc;
}


static int write_segmenttemplate_bynumber(MPD_CREATE_CTXT_T *pCtxt, 
                                        const MPD_ADAPTATION_T *pAdaptation,
                                        const MPD_SEGMENT_LIST_T *pSegs,
                                        const char *initMediaUrl,
                                        unsigned int startNumber,
                                        char *buf, unsigned int szbuf) {
  int rc = 0;
  unsigned int idx = 0;
  char mediaUrl[VSX_MAX_PATH_LEN];

  if((rc = writepath(pCtxt, -1, "$Number$", mediaUrl, sizeof(mediaUrl), pAdaptation->padaptationTag)) < 0) {
    return rc;
  }

  if((rc = write_tag_segmenttemplate_start(pAdaptation->pMedia, mediaUrl, initMediaUrl, 
                                           startNumber, &buf[idx], szbuf - idx)) < 0) {
      return rc;
  }
  idx += rc;

  MPD_WRITE_RET("<SegmentTimeline>"MPD_EOL);

  if((rc = write_tag_segmententry(pAdaptation->pMedia->timescale,
                                  pAdaptation->pMedia->duration,
                                  pAdaptation->pMedia->durationPreceeding,
                                  &buf[idx], szbuf - idx)) < 0) {
    return rc;
  }
  idx += rc;
  MPD_WRITE("</SegmentTimeline>"MPD_EOL);

  MPD_WRITE_RET("</SegmentTemplate>"MPD_EOL);

  return idx > 0 ? idx : rc;
}

static int write_tag_segmenttemplate(DASH_MPD_TYPE_T mpdType,
                                     MPD_CREATE_CTXT_T *pCtxt, 
                                     const MPD_ADAPTATION_T *pAdaptation,
                                     const MPD_SEGMENT_LIST_T *pSegs,
                                     enum DASH_INITIALIZER_TYPE writeInitType,
                                     unsigned int startNumber,
                                     char *buf, unsigned int szbuf) {

  int rc = 0;
  char initMediaUrl[VSX_MAX_PATH_LEN];

  initMediaUrl[0] = '\0';

  //
  // Create the path of the initializer element of this adaptation
  //
  if(writeInitType > DASH_INITIALIZER_TYPE_NONE) {
    if((rc = write_init_path(pCtxt, -1, initMediaUrl, sizeof(initMediaUrl), 
                             pAdaptation->padaptationTag)) < 0) {
      return rc;
    }
  }

  if(mpdType == DASH_MPD_TYPE_MOOF_SEGMENT_TEMPLATE_TIME || 
     mpdType == DASH_MPD_TYPE_TS_SEGMENT_TEMPLATE_TIME) {

    rc = write_segmenttemplate_bytime(pCtxt, pAdaptation, pSegs, initMediaUrl, buf, szbuf);

  } else if(mpdType == DASH_MPD_TYPE_MOOF_SEGMENT_TEMPLATE_NUMBER || 
            mpdType == DASH_MPD_TYPE_TS_SEGMENT_TEMPLATE_NUMBER) {

    rc = write_segmenttemplate_bynumber(pCtxt, pAdaptation, pSegs, initMediaUrl, startNumber, buf, szbuf);

  } else {
    return -1;
  }

  return rc;
}

static int write_tag_adaptationset_start(MPD_CREATE_CTXT_T *pCtxt, 
                                         unsigned int adaptationId, 
                                         const char *mimeType,
                                         int width, int height,
                                         int startWithSAPType,
                                         char *buf, unsigned int szbuf) {


  int rc = 0;
  unsigned int idx = 0;

  MPD_WRITE("<AdaptationSet id=\"%d\"", adaptationId);

  if(mimeType) {
    MPD_WRITE(" mimeType=\"%s\"", mimeType);
  }
  if(width > 0 && height > 0) {
    MPD_WRITE(" width=\"%d\" height=\"%d\"", width, height);
  }

  MPD_WRITE(" startWithSAP=\"%d\"", startWithSAPType);

  MPD_WRITE(">"MPD_EOL);

  return idx;
}

static int mpd_adaptationset_create(enum DASH_MPD_TYPE mpdType,
                                    const MPD_ADAPTATION_T *pAdaptation,
                                    const MPD_SEGMENT_LIST_T *pSegs,
                                    MPD_CREATE_CTXT_T *pCtxt,
                                    int outidx,
                                    unsigned int adaptationId,
                                    unsigned int mediaSequenceIndex, 
                                    char *buf, 
                                    size_t szbuf) {
  int rc = 0;
  unsigned int idx = 0;
  int startWithSAPType;
  char mimeType[128];

  //
  // SAP types (1-6) are defined in ISO/IEC 14496-12 Annex I
  //
  startWithSAPType = 1;
  if(pAdaptation->isvid && pAdaptation->isaud) {
    snprintf(mimeType, sizeof(mimeType), CONTENT_TYPE_MP4);
  } else if(pAdaptation->isvid) {
    snprintf(mimeType, sizeof(mimeType), CONTENT_TYPE_M4V);
  } else if(pAdaptation->isaud) {
    snprintf(mimeType, sizeof(mimeType), CONTENT_TYPE_M4A);
  }

  if((rc = write_tag_adaptationset_start(pCtxt, adaptationId, mimeType, 0, 0, 
                                         startWithSAPType, &buf[idx], szbuf - idx)) < 0) {
    return rc;
  }
  idx += rc;

  if(pAdaptation->isvid && pCtxt->pAvCtxt->vid.codecType != XC_CODEC_TYPE_UNKNOWN) {
    MPD_WRITE_RET("<ContentComponent id=\"1\" contentType=\"video\"/>"MPD_EOL);
  }

  if(pAdaptation->isaud && pCtxt->pAvCtxt->aud.codecType != XC_CODEC_TYPE_UNKNOWN) {
    MPD_WRITE_RET("<ContentComponent id=\"2\" contentType=\"audio\"/>"MPD_EOL);
  }

  if(mpdType == DASH_MPD_TYPE_MOOF_SEGMENT_TEMPLATE_NUMBER ||
     mpdType == DASH_MPD_TYPE_MOOF_SEGMENT_TEMPLATE_TIME ||
     mpdType == DASH_MPD_TYPE_TS_SEGMENT_TEMPLATE_NUMBER ||
     mpdType == DASH_MPD_TYPE_TS_SEGMENT_TEMPLATE_TIME) {

    if((rc = write_tag_segmenttemplate(mpdType,
                         pCtxt,
                         pAdaptation,
                         pSegs, 
                         pCtxt->useInitMp4 ? DASH_INITIALIZER_TYPE_ATTRIB : DASH_INITIALIZER_TYPE_NONE,
                         mediaSequenceIndex, &buf[idx], szbuf - idx)) < 0) {
      return rc;
    }
    idx += rc;

  } else {
    return -1;
  }

  if((rc = write_representations(pCtxt, pAdaptation, outidx, &buf[idx], szbuf - idx)) < 0) {
    return rc;
  }
  idx += rc;

  MPD_WRITE("</AdaptationSet>"MPD_EOL);

  return idx;
}

static float get_segment_total_duration_sec(const MPD_SEGMENT_LIST_T *pSegs, uint32_t timescale) {
  unsigned int indexSegment;
  unsigned int indexInSegments;
  uint64_t durationTotHz = 0;
  float durationTotSec = 0;

  if(pSegs->indexInSegments > 0) {
    indexInSegments = pSegs->indexInSegments - 1;
  } else {
    indexInSegments = pSegs->indexCount - 1;
  }

  for(indexSegment = 0; indexSegment < pSegs->indexCount; indexSegment++) {

    if(++indexInSegments >= pSegs->indexCount) {
      indexInSegments = 0;
    }

    if(!pSegs->pSegments[indexInSegments].valid) {
      continue;
    }

    durationTotHz += pSegs->pSegments[indexInSegments].media.duration;

  }

  durationTotSec = (float) durationTotHz / timescale;

  return durationTotSec;
}

static int mpd_doc_create(enum DASH_MPD_TYPE mpdType,
                          const MPD_ADAPTATION_T *pAdaptationList,
                          MPD_CREATE_CTXT_T *pCtxt, 
                          int outidx, 
                          unsigned int mediaSequenceIndex, 
                          char *buf, 
                          size_t szbuf) {
  int idx = 0;
  int rc = 0;
  time_t tmnow;
  struct tm *ptm;
  unsigned int periodId = 1;
  float durationSec = 0;
  float durationTotSec = 0;
  int startWithSAPType;
  float minBufferTime = 0;
  float minimumUpdatePeriod = 0;
  float maxSegmentDuration = 0;
  float timeShiftBufferDepth = 0;
  unsigned int adaptationId = 0;
  unsigned int segIdx = 0;
  const MPD_ADAPTATION_T *pAdaptationCur;
  const MPD_SEGMENT_LIST_T *pSegs = NULL;
  char tmbuf[2][64]; 
  char codecStrVid[32];
  char codecStrAud[32];

  codecStrVid[0] = codecStrAud[0] = '\0';

  if(pAdaptationList->pMedia->timescale > 0) {
    durationSec = (float) ((double) pAdaptationList->pMedia->duration / pAdaptationList->pMedia->timescale);
  }

  durationTotSec = get_segment_total_duration_sec(&pCtxt->segs[0], pAdaptationList->pMedia->timescale);

  minBufferTime = MIN(durationSec * .5, 3.0f); 
  //
  // SAP types (1-6) are defined in ISO/IEC 14496-12 Annex I
  //
  startWithSAPType = 1;

  if((minimumUpdatePeriod = (.7f * durationSec)) < 3.0f) {
    minimumUpdatePeriod = 3.0f;
  }
  maxSegmentDuration = durationSec + 2; 
  timeShiftBufferDepth = durationTotSec;

  memset(tmbuf, 0, sizeof(tmbuf));
  tmnow = time(NULL);
  ptm = gmtime(&tmnow);
  rc = snprintf(tmbuf[0], sizeof(tmbuf[0]), "%d-%02d-%02dT%02d:%02d:%02dZ",
           ptm->tm_year + 1900,
           ptm->tm_mon + 1,
           ptm->tm_mday,
           ptm->tm_hour, ptm->tm_min, ptm->tm_sec);
  memcpy(&tmbuf[1], tmbuf[0], sizeof(tmbuf[1]));


  MPD_WRITE("<?xml version=\"1.0\" encoding=\"UTF-8\"?>"MPD_EOL);

  MPD_WRITE("<MPD"
            " xmlns:xsi=\""MPD_XMLNS_XSI"\""
            " xmlns=\""MPD_XMLNS"\""
            " xmlns:xlink=\""MPD_XMLNS_XLINK"\""
            " xsi:schemaLocation=\""MPD_XSI_SCHEMALOCATION"\""
            //" profiles=\""MPD_PROFILE_LIVE_MP4"\""
            " profiles=\""MPD_PROFILE_LIVE_MP4",urn:com:dashif:dash264\""
            " type=\"dynamic\""
            " minBufferTime=\"PT%.1fS\""
            " minimumUpdatePeriod=\"PT%.1fS\""
            //" mediaPresentationDuration=\"PT0H1M59.91S\""
            , minBufferTime, minimumUpdatePeriod);

  if(tmbuf[0] != '\0') {
    MPD_WRITE(" availabilityStartTime=\"%s\"", tmbuf[0]);
  }
  if(tmbuf[1] != '\0') {
    MPD_WRITE(" publishTime=\"%s\"", tmbuf[1]);
  }
  if(timeShiftBufferDepth > 0) {
    MPD_WRITE(" timeShiftBufferDepth=\"PT%.1fS\"", timeShiftBufferDepth);
  }
  if(maxSegmentDuration > 0) {
    MPD_WRITE(" maxSegmentDuration=\"PT%.1fS\"", maxSegmentDuration);
  }

  MPD_WRITE(">"MPD_EOL);

  MPD_WRITE("<ProgramInformation" ">"MPD_EOL);
            //" moreInformationURL=\""VSX_URL"\""
  MPD_WRITE("<Title>%s</Title>"MPD_EOL, "Media Presentation Description for live broadcast");
  MPD_WRITE("</ProgramInformation>"MPD_EOL);

  MPD_WRITE("<Period"
            " id=\"%d\""
            " start=\"PT0S\""
            //" duration=\"PT0H1M59.91S\""
            ">"MPD_EOL, periodId);

  pAdaptationCur = pAdaptationList;
  while(pAdaptationCur) {
    if(segIdx < DASH_MPD_MAX_ADAPTATIONS) {
      pSegs = &pCtxt->segs[segIdx++];
    }

    if((rc = mpd_adaptationset_create(mpdType, pAdaptationCur, pSegs, pCtxt, outidx, 
                                      ++adaptationId, mediaSequenceIndex, &buf[idx], szbuf - idx)) < 0) {
      return rc;
    }
    idx += rc;

    pAdaptationCur = pAdaptationCur->pnext;
  }


  MPD_WRITE("</Period>"MPD_EOL);

  MPD_WRITE("</MPD>"MPD_EOL);

  if(rc >= 0) {
    rc = idx;
  }

  return rc;
}

static void purge_segments(MPD_CREATE_CTXT_T *pCtxt, const MPD_ADAPTATION_T *pAdaptationList, 
                           unsigned int outidx, int mediaSequenceMin) {
  int rc;
  const MPD_ADAPTATION_T *pAdaptationCur = NULL;
  char buf[VSX_MAX_PATH_LEN];

  //VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - mpd purge_segments... ")));

    if(!pCtxt->init.nodelete_expired) {


      pAdaptationCur = pAdaptationList;
      while(pAdaptationCur) {

        if((rc = mpd_format_path_prefix(buf, sizeof(buf), outidx, pCtxt->init.outfileprefix, 
                                        pAdaptationCur->padaptationTag)) > 0) {

          //VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - mpd purge_segmnents '%s'"), buf));
      
      //fprintf(stderr, "TRY 3... HTTP_PURGE_SEG mediaSeq:%d, mediaSeqMin:%d, keep:%d, outdir:'%s', prfx:'%s'\n", mediaSequenceIndex, mediaSeqMin, keepIdx, pCtxt->outdir, buf);
          http_purge_segments(pCtxt->outdir, buf, "."DASH_DEFAULT_SUFFIX_M4S, mediaSequenceMin, 0);
        }
        pAdaptationCur = pAdaptationCur->pnext;
      }
    }
}

static int init_adaptation_segmentlist(MPD_SEGMENT_LIST_T *pSegs, 
                                       const MPD_ADAPTATION_T *pAdaptation,
                                       unsigned int curIdx) {
  int rc = 0;

  if(pSegs->pSegments) {

    pSegs->pSegments[pSegs->indexInSegments].index = curIdx;
    pSegs->pSegments[pSegs->indexInSegments].valid = pAdaptation ? 1 : 0;
    if(pAdaptation) {

      if((pSegs->pSegments[pSegs->indexInSegments].media.timescale = pAdaptation->pMedia->timescale) == 0) {
        return -1;
      } else if(!pAdaptation->pMedia->mediaUniqIdstr || pAdaptation->pMedia->mediaUniqIdstr[0] == '\0') {
        return -1;
      }

      memcpy(&pSegs->pSegments[pSegs->indexInSegments].media, pAdaptation->pMedia,
             sizeof(pSegs->pSegments[pSegs->indexInSegments].media));
    }
  }
  if(++pSegs->indexInSegments >= pSegs->indexCount) {
    pSegs->indexInSegments = 0;
  }
  pSegs->curIdx = curIdx;

  return rc;
}

static int create_and_write_mpd(MPD_CREATE_CTXT_T *pCtxt, 
                                const MPD_ADAPTATION_T *pAdaptationList,
                                DASH_MPD_TYPE_T mpdType, 
                                int outidx,
                                int idxmax,
                                unsigned int mediaSequenceIndex,
                                int mediaSequenceMin) {
  int rc = 0;
  int idxbuf = 0;
  FILE_HANDLE fp;
  char buf[4096];
  char mpdpath[VSX_MAX_PATH_LEN];

  //
  // Create the .mpd xml file contents
  //
  if((idxbuf = mpd_doc_create(mpdType, pAdaptationList, pCtxt, 
                              outidx, mediaSequenceIndex, buf, sizeof(buf))) <= 0) {
    return -1;
  }

  //
  // Write the .mpd xml file to disk 
  //
  if((rc = write_mpd_path(pCtxt, outidx, mpdType, mpdpath, sizeof(mpdpath))) < 0) {
    return rc;
  }

  if((fp = fileops_Open(mpdpath, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Failed to open DASH mpd '%s' for writing"), mpdpath);
    return -1;
  }

  if((rc = fileops_WriteBinary(fp, (unsigned char *) buf, idxbuf)) < 0) {
    LOG(X_ERROR("Failed to write %d to DASH mpd '%s'"), idxbuf, mpdpath);
  }

  fileops_Close(fp);

  if(rc >= 0) {
    LOG(X_DEBUG("Updated DASH playlist '%s' (%d - %d)"), mpdpath, mediaSequenceMin, idxmax);
  } else {
    LOG(X_ERROR("Failed to update DASH playlist '%s' (%d - %d)"), mpdpath, mediaSequenceMin, idxmax);
  }

  return rc;
}

int mpd_on_new_mediafile(MPD_CREATE_CTXT_T *pCtxt, 
                         const MPD_ADAPTATION_T *pAdaptationList, 
                         int outidx,
                         unsigned int mediaSequenceIndex,
                         int mediaSequenceMin,
                         int duplicateIter) {

  int rc = 0;
  int idxmax = 0;
  int keepIdx;
  unsigned int idx;
  unsigned int outFileTypeIdx;
  const MPD_ADAPTATION_T *pAdaptationCur = NULL;

  if(!pCtxt || !pAdaptationList || !pAdaptationList->pMedia) {
    return -1;
  }

  VSX_DEBUG_DASH( LOG(X_DEBUG("DASH - mpd_on_new_mediafile '%s', outidx:%d"), 
                               pAdaptationList->pMedia->mediaUniqIdstr, outidx));

  idxmax = mediaSequenceIndex;
  keepIdx = pCtxt->segs[0].indexCount + 1;

  //
  // Purge any media segments which are to be expired
  //
  if(mediaSequenceIndex > keepIdx + 2) {

    if(mediaSequenceMin < 0) {
      mediaSequenceMin = mediaSequenceIndex - keepIdx - 2;
    }

    if(outidx >= 0) {
      purge_segments(pCtxt, pAdaptationList, outidx, mediaSequenceMin);
    }

  }

  if(!duplicateIter) {
    //
    // Store relevant info about the new entry in the MPD_SEGEMENT_ENTRY_T struct
    //
    pAdaptationCur = pAdaptationList;
    for(idx = 0; idx < DASH_MPD_MAX_ADAPTATIONS; idx++) {

      if(init_adaptation_segmentlist(&pCtxt->segs[idx], pAdaptationCur, mediaSequenceIndex) < 0) {
        return -1;
      }

      if(!(pAdaptationCur = pAdaptationCur->pnext)) {
        break;
      }
    }
  }

  //
  // Iterate throuth each type of .mpd we are to create
  //
  for(outFileTypeIdx = 0; outFileTypeIdx < sizeof(pCtxt->mpdOutputTypes) / sizeof(pCtxt->mpdOutputTypes[0]); 
      outFileTypeIdx++) {

    if(pCtxt->mpdOutputTypes[outFileTypeIdx].type == DASH_MPD_TYPE_INVALID) {
      continue;
    }

    //
    // Write a mpd for adaptive bitrate streaming, with one or more video representations
    //
    if(outidx < 0) {

      if((rc = create_and_write_mpd(pCtxt, pAdaptationList, pCtxt->mpdOutputTypes[outFileTypeIdx].type, -1,
                                    idxmax, mediaSequenceIndex, mediaSequenceMin)) < 0) {
      }
    }

    //
    // Write an output index specific mpd, with just one video representation
    //
    //if(pCtxt->outidxTot > 1) {
     else {
      if((rc = create_and_write_mpd(pCtxt, pAdaptationList, pCtxt->mpdOutputTypes[outFileTypeIdx].type, outidx,
                                    idxmax, mediaSequenceIndex, mediaSequenceMin)) < 0) {
      }
    }

  } // end of for

  return rc;
}

int mpd_init_from_httplive(MPD_CREATE_CTXT_T *pCtxt, const HTTPLIVE_DATA_T *pHttplive, const char *outdir) {
  int rc = 0;
  DASH_INIT_CTXT_T dashInit;

  if(!pCtxt || !pHttplive || !pCtxt->pAvCtxt || !outdir) {
    return -1;
  }
  
  memset(&dashInit, 0, sizeof(dashInit));
  dashInit.enableDash = 1;
  dashInit.indexcount = pHttplive->indexCount;
  strncpy((char *) dashInit.outdir, outdir, sizeof(dashInit.outdir) - 1);
  dashInit.outdir_ts = pHttplive->dir;
  dashInit.outfileprefix = pHttplive->fileprefix;
  dashInit.uriprefix = pHttplive->uriprefix;
  dashInit.nodelete_expired = 1;

  if((rc = mpd_init(pCtxt, &dashInit, pCtxt->pAvCtxt)) >= 0) {
    pCtxt->init.outfilesuffix = HTTPLIVE_TS_NAME_EXT;
    pCtxt->cbFormatPathPrefix = httplive_format_path_prefix;
    pCtxt->mpdOutputTypes[0].type = DASH_MPD_TYPE_TS_SEGMENT_TEMPLATE_NUMBER;
    pCtxt->mpdOutputTypes[1].type = DASH_MPD_TYPE_INVALID;
  }

  //fprintf(stderr, "MPD OUTFILEPREFIX:'%s' '%s', suffix:'%s', outidx:%d, outdir:'%s', outdir_ts:'%s'\n", pHttplive->fileprefix, pCtxt->init.outfileprefix, pCtxt->init.outfilesuffix, pHttplive->outidx, pCtxt->init.outdir, pCtxt->init.outdir_ts);

  return rc;
}

int mpd_init(MPD_CREATE_CTXT_T *pCtxt, 
             const DASH_INIT_CTXT_T *pDashInitCtxt,
             const CODEC_AV_CTXT_T *pAvCtxt) {

  unsigned int indexCount;
  unsigned int idx;
  int rc = 0;

  if(!pCtxt || !pDashInitCtxt) {
    return -1;
  }

  memcpy(&pCtxt->init, pDashInitCtxt, sizeof(pCtxt->init));

  if(!pCtxt->init.outfileprefix || pCtxt->init.outfileprefix[0] == '\0') {
    pCtxt->init.outfileprefix = DASH_DEFAULT_NAME_PRFX;
  }

  pCtxt->active = 1;
  pCtxt->cbFormatPathPrefix = mpd_format_path_prefix;
  pCtxt->pAvCtxt = pAvCtxt;
  pCtxt->outdir = pDashInitCtxt->outdir;
  if(!pCtxt->init.outfilesuffix) {
    pCtxt->init.outfilesuffix = DASH_DEFAULT_SUFFIX_M4S;
  }
  pCtxt->mpdOutputTypes[0].type = pDashInitCtxt->dash_mpd_type;
  pCtxt->mpdOutputTypes[1].type = DASH_MPD_TYPE_INVALID;

  if((indexCount = pCtxt->init.indexcount) == 0) {
    indexCount = DASH_NUM_MEDIA_FILES_DEFAULT;
  } else if(indexCount < DASH_NUM_MEDIA_FILES_MIN) {
    indexCount = DASH_NUM_MEDIA_FILES_MIN;
  } else if(indexCount > DASH_NUM_MEDIA_FILES_MAX) {
    indexCount = DASH_NUM_MEDIA_FILES_MAX;
  }

  for(idx = 0; idx < DASH_MPD_MAX_ADAPTATIONS; idx++) {
    if(!(pCtxt->segs[idx].pSegments = 
                    (MPD_SEGMENT_ENTRY_T *) avc_calloc(indexCount, sizeof(MPD_SEGMENT_ENTRY_T)))) {
      return -1;
    }
    pCtxt->segs[idx].curIdx = 0;
    pCtxt->segs[idx].indexInSegments = 0;
    pCtxt->segs[idx].indexCount = indexCount;
  }

  pCtxt->outidxTot = 1;
  //
  // Count the number of active outputs
  //
  for(idx = 1; idx < IXCODE_VIDEO_OUT_MAX; idx ++) {
    if(pCtxt->pAvCtxt->vid.pStreamerCfg->xcode.vid.out[idx].active) {
      pCtxt->outidxTot++;
    }
  }

  return rc;
}

int mpd_close(MPD_CREATE_CTXT_T *pCtxt, int deleteFiles, int dealloc) {
  int rc = 0;
  unsigned int idx;
  char mpdpath[VSX_MAX_PATH_LEN];
  int outidx;
  struct stat st;

  if(!pCtxt) {
    return -1;
  }

  if(deleteFiles) {
    //
    // Remove playlist .mpd files
    //
    for(idx = 0; idx < sizeof(pCtxt->mpdOutputTypes) / sizeof(pCtxt->mpdOutputTypes[0]); idx++) {

      for(outidx = -1; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

        write_mpd_path(pCtxt, outidx, pCtxt->mpdOutputTypes[idx].type, mpdpath, sizeof(mpdpath));

        if(fileops_stat(mpdpath, &st) == 0 && (rc = fileops_DeleteFile(mpdpath)) != 0) {
          LOG(X_ERROR("Failed to delete '%s'"), mpdpath);
        }
      }

    }
  }

  if(dealloc) {
    for(idx = 0; idx < DASH_MPD_MAX_ADAPTATIONS; idx++) {
      if(pCtxt->segs[idx].pSegments) {
        avc_free((void **) &pCtxt->segs[idx].pSegments);
      }
    }
  }

  return rc;
}


DASH_MPD_TYPE_T dash_mpd_str_to_type(const char *str) {

  if(str) {
    if(!strcasecmp(str, DASH_MPD_TYPE_SEGMENT_TEMPLATE_NUMBER_STR)) {
      return DASH_MPD_TYPE_MOOF_SEGMENT_TEMPLATE_NUMBER;
    } else if(!strcasecmp(str, DASH_MPD_TYPE_SEGMENT_TEMPLATE_TIME_STR)) {
      return DASH_MPD_TYPE_MOOF_SEGMENT_TEMPLATE_TIME;
    }

  }
  return DASH_MPD_TYPE_INVALID;

}

static const char *mpd_type_to_filename(DASH_MPD_TYPE_T mpdType) {

  switch(mpdType) {
    case DASH_MPD_TYPE_TS_SEGMENT_TEMPLATE_NUMBER:
    case DASH_MPD_TYPE_TS_SEGMENT_TEMPLATE_TIME:
      return DASH_MPD_TS_SEGTEMPLATE_NAME;
    case DASH_MPD_TYPE_MOOF_SEGMENT_TEMPLATE_NUMBER:
    case DASH_MPD_TYPE_MOOF_SEGMENT_TEMPLATE_TIME:
      return DASH_MPD_SEGTEMPLATE_NAME;
    default:
      return DASH_MPD_DEFAULT_NAME;
   }

}


/*
int mpd_on_new_ts(unsigned int mediaSequenceIndex, int mediaSequenceMin, unsigned int outidx,
                  const char *mediaPath, MPD_CREATE_CTXT_T *pCtxt,
                  uint32_t timescale, uint64_t duration, uint64_t durationPreceeding) {

  int rc = 0;
  MPD_ADAPTATION_T adaptationSet;
  memset(&adaptationSet, 0, sizeof(adaptationSet));

  adaptationSet.isvid = 1;
  adaptationSet.isaud = 1;
  adaptationSet.mediaPath = mediaPath;
  adaptationSetkk

  //if(mediaSequenceIndex <= 0) {
  //  return 0;
  //}

  //fprintf(stderr, "MPD_ON_NEW_TS mediaSequenceIndex:%d, mediaSequenceMin:%d, outidx[%d], '%s', scale:%uHz, duration:%lluHz, durationPreceeding:%lluHz\n", mediaSequenceIndex, mediaSequenceMin, outidx, mediaPath, timescale, duration, durationPreceeding);
  //fprintf(stderr, "TS MPD UPDATE... vid.codecType:%d, profile:%d, %d, %d\n", pCtxt->pAvCtxt->vid.codecType, pCtxt->pAvCtxt->vid.codecCtxt.h264.avcc.avcprofile, pCtxt->pAvCtxt->vid.codecCtxt.h264.avcc.profilecompatibility, pCtxt->pAvCtxt->vid.codecCtxt.h264.avcc.avclevel);

  rc = mpd_on_new_mediafile(mediaSequenceIndex, mediaSequenceMin, 
                            outidx, mediaPath, pCtxt, timescale, duration, durationPreceeding);

  return rc;
}
*/

