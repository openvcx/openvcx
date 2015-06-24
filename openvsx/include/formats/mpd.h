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


#ifndef __EXT_MPD_H__
#define __EXT_MPD_H__

#include "unixcompat.h"

#define DASH_MPD_SEGTEMPLATE_PRFX            "segtemplate"
#define DASH_MPD_EXT                         ".mpd"
#define DASH_MPD_SEGTEMPLATE_NAME            DASH_MPD_SEGTEMPLATE_PRFX DASH_MPD_EXT
#define DASH_MPD_TS_SEGTEMPLATE_NAME         "ts"DASH_MPD_SEGTEMPLATE_PRFX DASH_MPD_EXT
#define DASH_MPD_DEFAULT_NAME                DASH_MPD_SEGTEMPLATE_PRFX DASH_MPD_EXT 
#define DASH_MPD_TYPE_DEFAULT                DASH_MPD_TYPE_MOOF_SEGMENT_TEMPLATE_TIME
#define DASH_DEFAULT_NAME_PRFX               "out"
//#define DASH_DEFAULT_SUFFIX_MP4              "mp4"
#define DASH_DEFAULT_SUFFIX_M4S              "m4s"

#define DASH_NUM_MEDIA_FILES_DEFAULT 3
#define DASH_NUM_MEDIA_FILES_MIN 1
#define DASH_NUM_MEDIA_FILES_MAX 9999

#define DASH_MP4ADAPTATION_TAG_VID         "v"
#define DASH_MP4ADAPTATION_TAG_AUD         "a"
#define DASH_MP4ADAPTATION_TAG_COMBINED    "x"
#define DASH_MP4ADAPTATION_NAME(isvid)  ((isvid) > 0 ? DASH_MP4ADAPTATION_TAG_VID : \
                                  (isvid) == 0 ? DASH_MP4ADAPTATION_TAG_AUD  : DASH_MP4ADAPTATION_TAG_COMBINED)

#define DASH_MPD_MAX_ADAPTATIONS  2

#define DASH_MPD_TYPE_SEGMENT_TEMPLATE_NUMBER_STR     "Number"
#define DASH_MPD_TYPE_SEGMENT_TEMPLATE_TIME_STR       "Time"
#define DASH_MPD_TYPE_DEFAULT_STR                     DASH_MPD_TYPE_SEGMENT_TEMPLATE_TIME_STR

typedef int (*CB_MPD_FORMAT_PATH_PREFIX)(char *, unsigned int, int, const char *, const char *);

typedef struct DASH_INIT_CTXT {
  int                     enableDash;
  unsigned int            indexcount;
  const char              outdir[VSX_MAX_PATH_LEN];
  const char             *outdir_ts;
  const char             *outfileprefix;
  const char             *outfilesuffix;
  const char             *uriprefix;
  int                     nodelete_expired;
  DASH_MPD_TYPE_T         dash_mpd_type;
} DASH_INIT_CTXT_T;

typedef struct MPD_MEDIA_FILE {
  const char             *mediaDir;
  const char             *mediaUniqIdstr;
  //const char             *padaptationTag; // 'v' or 'a' video/audio designation
  uint32_t                timescale;
  uint64_t                duration;
  uint64_t                durationPreceeding;
} MPD_MEDIA_FILE_T;


typedef struct MPD_SEGMENT_ENTRY {
  int                     valid; 
  MPD_MEDIA_FILE_T        media;
  unsigned int            index;
} MPD_SEGMENT_ENTRY_T;

typedef struct MPD_SEGMENT_LIST {
  MPD_SEGMENT_ENTRY_T      *pSegments;
  unsigned int              indexInSegments;
  unsigned int              curIdx;
  unsigned int              indexCount;  // Number of media files ot include in a playlist
} MPD_SEGMENT_LIST_T;

typedef struct MPD_TYPE {
  DASH_MPD_TYPE_T       type;
} MPD_TYPE_T;

typedef struct MPD_CREATE_CTXT {
  int                       active;
  CB_MPD_FORMAT_PATH_PREFIX cbFormatPathPrefix;
  const CODEC_AV_CTXT_T    *pAvCtxt;
  MPD_TYPE_T                mpdOutputTypes[2];
  int                       mixAudVid;
  int                       useInitMp4;
  unsigned int              outidxTot;
  const char               *outdir;
  DASH_INIT_CTXT_T          init;
  MPD_SEGMENT_LIST_T        segs[DASH_MPD_MAX_ADAPTATIONS];      // historical list
} MPD_CREATE_CTXT_T;



struct MP4_CREATE_STATE_MOOF;
struct HTTPLIVE_DATA;

typedef struct MPD_ADAPTATION {
  int                     isvid;
  int                     isaud;
  MPD_MEDIA_FILE_T       *pMedia;
  const char             *padaptationTag; // 'v' or 'a' video/audio designation
  struct MPD_ADAPTATION  *pnext;
} MPD_ADAPTATION_T;

/*
typedef struct MPD_UDPATE_CTXT {
  int                 no_init_state;
  int                 did_update;
  unsigned int        mediaSequenceIndex;
  int                 mediaSequenceMin; 
  char                uniqIdBuf[128];
  MPD_MEDIA_FILE_T    media;
} MPD_UPDATE_CTXT_T;
*/

int mpd_on_new_mediafile(MPD_CREATE_CTXT_T *pCtxt, const MPD_ADAPTATION_T *pAdaptationList, int outidx, 
                         unsigned int mediaSequenceIndex, int mediaSequenceMin, int duplicateIter);

int mpd_init(MPD_CREATE_CTXT_T *pCtxt, const DASH_INIT_CTXT_T *pDashInitCtxt,
             const CODEC_AV_CTXT_T *pAvCtxt);

int mpd_init_from_httplive(MPD_CREATE_CTXT_T *pCtxt, const struct HTTPLIVE_DATA *pHttplive,
                           const char *outdir);

int mpd_close(MPD_CREATE_CTXT_T *pCtxt, int deleteFiles, int dealloc);

int mpd_format_path_prefix(char *buf, unsigned int sz, int outidx, const char *prefix, 
                           const char *adaptationname);
int mpd_format_path(char *buf, unsigned int sz, CB_MPD_FORMAT_PATH_PREFIX cbFormatPathPrefix,
                    int outidx, const char *prefix, const char *strIndex,  const char *suffix, 
                    const char *adaptationname);
const char *mpd_path_get_adaptationtag(const char *path);
DASH_MPD_TYPE_T dash_mpd_str_to_type(const char *str);

#endif //__EXT_MPD_H__
