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


#ifndef __STREAM_RTMP_H__
#define __STREAM_RTMP_H__

typedef struct RTMP_REQ_CTXT {
  struct STREAMER_CFG     *pStreamerCfg;
  RTMP_CTXT_CLIENT_T      *pClient;
  RTMP_CLIENT_CFG_T       *pCfg;
} RTMP_REQ_CTXT_T;

typedef struct STREAM_RTMP_PUBLISH {
  int                           ispublished;
  PKTQUEUE_COND_T               cond;
  int                           connectretrycntminone;   // http fetch retry control
                                                         // 0 - off, 1 - forever, 
                                                         // 2 - once, 3 twice, etc... 
  pthread_mutex_t               mtx;
  RTMP_CLIENT_SESSION_T        *pSession;
  RTMP_CLIENT_CFG_T             cfg;
} STREAM_RTMP_PUBLISH_T;

int stream_rtmp_publish_start(struct STREAMER_CFG *pStreamerCfg, int wait_for_setup);
int stream_rtmp_close(struct STREAMER_CFG *pStreamerCfg);

#endif // __STREAM_RTMP_H__

