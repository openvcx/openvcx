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


#ifndef __STREAM_RTSP_H__
#define __STREAM_RTSP_H__

#include "stream_rtp.h"
#include "stream_rtsptypes.h"

typedef struct RTSP_HTTP_SESSION_COOKIE {
  struct sockaddr_storage    sa;
  char                       sessionCookie[RTSP_HTTP_SESSION_COOKIE_MAX];
} RTSP_HTTP_SESSION_COOKIE_T;

#define RTSP_HTTP_POST_BUFFER_SZ            1536


typedef struct RTSP_HTTP_SESSION {
  int                            inuse;
  //SOCKET                         socketPost;
  NETIO_SOCK_T                   netsocketPost;
  int                            expired;
  RTSP_HTTP_SESSION_COOKIE_T     cookie;
  struct timeval                 tvCreate;
  struct timeval                 tvLastMsg;

  unsigned int                   requestSz;
  unsigned int                   requestIdxWr;
  char                           *pRequestBufWr;  // base64 decoded msg written by POST thread
  char                           *pRequestBufRd;  // msg referenced by GET thread
  pthread_mutex_t                mtx;

  PKTQUEUE_COND_T                cond;

  struct RTSP_HTTP_SESSION      *pnext;
} RTSP_HTTP_SESSION_T;

#define RTSP_FORCE_TCP_UA_LIST_MAX     10

typedef struct STREAM_RTSP_UALIST {
  const char                   *str;
  unsigned int                  count;
  char                         *arr[RTSP_FORCE_TCP_UA_LIST_MAX];
} STREAM_RTSP_UALIST_T;

typedef struct STREAM_RTSP_SESSIONS {
  RTSP_SESSION_T                 *psessions;
  const unsigned int              max; 
  STREAM_RTP_MULTI_T             *pRtpMultis[IXCODE_VIDEO_OUT_MAX];
  int                             runMonitor;
  unsigned int                    numActive;
  pthread_mutex_t                 mtx;

  // config
  const unsigned int             *psessionTmtSec;
  const int                      *prefreshtimeoutviartcp;

  int                             staticLocalPort;
  RTSP_TRANSPORT_SECURITY_TYPE_T  rtspsecuritytype;
  RTSP_TRANSPORT_TYPE_T           rtsptranstype;
  SOCKET                          sockStaticLocalPort;

  STREAM_RTSP_UALIST_T            rtspForceTcpUAList;

  //
  // for RTSP over HTTP
  //
  unsigned int                    numRtspGetSessions;
  RTSP_HTTP_SESSION_T             *pRtspGetSessionsBuf;
  RTSP_HTTP_SESSION_T             *pRtspGetSessionsHead;
  RTSP_HTTP_SESSION_T             *pRtspGetSessionsTail;

} STREAM_RTSP_SESSIONS_T;

typedef struct STREAM_RTSP_ANNOUNCE_T {
  int                           isannounced;
  PKTQUEUE_COND_T               cond;
  int                           connectretrycntminone;   // http fetch retry control
                                                         // 0 - off, 1 - forever, 
                                                         // 2 - once, 3 twice, etc... 
  pthread_mutex_t               mtx;
  RTSP_CLIENT_SESSION_T        *pSession;
} STREAM_RTSP_ANNOUNCE_T;

int rtspsrv_init(STREAM_RTSP_SESSIONS_T *pRtsp);
void rtspsrv_close(STREAM_RTSP_SESSIONS_T *pRtsp);
RTSP_SESSION_T *rtspsrv_findSession(STREAM_RTSP_SESSIONS_T *pRtsp,
                                           const char *sessionId);
RTSP_SESSION_T *rtspsrv_newSession(STREAM_RTSP_SESSIONS_T *pRtsp, int rtpmux, int rtcpmux);
int rtspsrv_closeSession(STREAM_RTSP_SESSIONS_T *pRtsp, const char sessionId[]);
int rtspsrv_playSessionTrack(STREAM_RTSP_SESSIONS_T *pRtsp, const char sessionId[],
                             int trackId, int lock);

RTSP_HTTP_SESSION_T *rtspsrv_newHttpSession(STREAM_RTSP_SESSIONS_T *pRtsp, const char *sessionCookie,
                                               const struct sockaddr *psa);
int rtspsrv_deleteHttpSession(STREAM_RTSP_SESSIONS_T *pRtsp, const char *sessionCookie,
                              const struct sockaddr *psa);
RTSP_HTTP_SESSION_T *rtspsrv_findHttpSession(STREAM_RTSP_SESSIONS_T *pRtsp, const char *sessionCookie,
                                             const struct sockaddr *psa, int lock);
void rtspsrv_initSession(const STREAM_RTSP_SESSIONS_T *pRtsp, RTSP_SESSION_T *pSes);


#endif // __STREAM_RTSP_H__

