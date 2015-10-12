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


#ifndef __HTTP_LIVE_H__
#define __HTTP_LIVE_H__

#ifdef WIN32

#include <windows.h>
#include "unixcompat.h"

#else // WIN32

#include <unistd.h>
#include "wincompat.h"

#endif // WIN32

#define EXT_HTML                            ".html"

#define HTTPLIVE_TS_NAME_PRFX               "out"
#define HTTPLIVE_MULTIBITRATE_NAME_PRFX    "multi"
#define HTTPLIVE_TS_NAME_EXT                "ts"
#define HTTPLIVE_PL_NAME_EXT                ".m3u8"
#define HTTPLIVE_DURATION_DEFAULT           10.0f
#define HTTPLIVE_INDEX_HTML                 "index"EXT_HTML

#define HTTPLIVE_EXTX_MEDIA_SEQUENCE        "EXT-X-MEDIA-SEQUENCE"
#define HTTPLIVE_EXTX_TARGET_DURATION       "EXT-X-TARGETDURATION"
#define HTTPLIVE_EXTX_STREAM_INFO           "EXT-X-STREAM-INF"

#define HTTPLIVE_BITRATE_MULTIPLIER         1.15f

struct STREAMER_CFG;

typedef struct HTTPLIVE_DATA {
  char                    dir[VSX_MAX_PATH_LEN];
  char                    uriprefix[128];
  char                    fileprefix[128];
  float                   duration;
  int                     nodelete;
  FILE_STREAM_T           fs;
  struct timeval          tvNextRoll;
  struct timeval          tvPriorRoll;
  struct timeval          tvRoll0;
  int                     curIdx;
  unsigned int            indexCount;  // Number of .ts files ot include in an .m3u8 playlist
  struct STREAMER_CFG    *pStreamerCfg;

  //
  // During initialization, everything after this point can be given unique values 
  // when using multiple xcode outputs (multi-bitrate)
  //

  int                     count;       // for parallel (multi-bitrate output) count of # of tot streams
                                       // including the current one
  int                     active;
  unsigned int            autoBitrate;        // auto computed bitrate (bps) of this output stream
  unsigned int            publishedBitrate;   // configured published bitrate (bps) of this output stream
  unsigned int            outidx;             // xcode outidx of this instance
  struct MPD_CREATE_CTXT *pMpdMp2tsCtxt;      // ctxt for creating MPEG-DASH .mpd using .ts media files
  struct HTTPLIVE_DATA   *pnext;

} HTTPLIVE_DATA_T;


int httplive_cbOnPkt(void *pUserData, const unsigned char *pPktData, unsigned int len);

int httplive_init(HTTPLIVE_DATA_T *pData);
int httplive_close(HTTPLIVE_DATA_T *pData);
int httplive_getindexhtml(const char *host, const char *dir, const char *prfx, 
                          unsigned char *pBuf, unsigned int *plen);

int http_purge_segments(const char *dirpath,
                        const char *fileprefix,
                        const char *ext,
                        unsigned int idxmin,
                        int purgeNonIdx);
int httplive_format_path_prefix(char *buf, unsigned int sz, int outidx, const char *prefix, 
                                const char *adaptationname);

#endif //__HTTP_LIVE_H__
