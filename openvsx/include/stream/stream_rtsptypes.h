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


#ifndef __STREAM_RTSP_TYPES_H__
#define __STREAM_RTSP_TYPES_H__

#include "formats/rtsp_parse.h"

#define RTSP_SESSION_ID_LEN                16
#define RTSP_SESSION_ID_MAX                64
#define RTSP_HTTP_SESSION_COOKIE_MAX       RTSP_SESSION_ID_MAX 
#define RTSP_SESSION_IDLE_SEC_SPEC         60 
#define RTSP_SESSION_IDLE_SEC              30 

#define RTSP_TRACKID_VID           1
#define RTSP_TRACKID_AUD           2


typedef enum RTSP_PLAY_STATE {
  RTSP_PLAY_STATE_NONE           = 0,
  RTSP_PLAY_STATE_PLAY           = 1,
  RTSP_PLAY_STATE_PAUSED         = 2
} RTSP_PLAY_STATE_T;

typedef enum RTSP_SESSION_TYPE {
  RTSP_SESSION_TYPE_UDP          = 0x01,
  RTSP_SESSION_TYPE_INTERLEAVED  = 0x02,
  RTSP_SESSION_TYPE_HTTP         = 0x04
} RTSP_SESSION_TYPE_T;

#define RTSP_SESSION_FLAG_EXPIRED        0x01
#define RTSP_SESSION_FLAG_PAUSED         0x02


typedef struct  RTSP_SESSION_PROG {
  uint8_t                       issetup;
  uint8_t                       isplaying;
  uint8_t                       multicast;
  char                          transStr[47];
  int16_t                       numports; 
  uint16_t                      ports[2];   // remote rtp, rtcp ports
  uint16_t                      localports[2]; // local bind rtp, rtcp ports
  struct in_addr                dstIp;
  STREAM_RTP_DEST_T            *pDest;
  int                           haveKeyFrame;
  unsigned int                  nonKeyFrames;
  char                          url[RTSP_URL_LEN];
} RTSP_SESSION_PROG_T;

typedef struct RTSP_SESSION {
  int                           inuse;
  int                           flag;
  RTSP_SESSION_TYPE_T           sessionType;
  struct timeval                tvlastupdate;
  unsigned int                  expireSec;
  int                           refreshtimeoutviartcp;
  char                          sessionid[RTSP_SESSION_ID_MAX];
  RTSP_SESSION_PROG_T           vid;
  RTSP_SESSION_PROG_T           aud;

  STREAMER_LIVEQ_T             *pLiveQ2;
  unsigned int                  liveQIdx; 
  LIVEQ_CB_CTXT_T               liveQCtxt;

  unsigned int                  requestOutIdx; // xcode outidx

  //
  // RTSP interleaved over HTTP
  //
  //char                          sessionCookie[RTSP_SESSION_ID_MAX];
  //struct RTSP_SESSION          *pComplement;

} RTSP_SESSION_T;





#endif // __STREAM_RTSP_TYPES_H__

