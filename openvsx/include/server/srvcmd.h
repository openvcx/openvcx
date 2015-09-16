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


#ifndef __SERVER_CMD_H__
#define __SERVER_CMD_H__

#include "unixcompat.h"

#ifndef WIN32

#include <arpa/inet.h>

#endif // WIN32

#include "capture/capture.h"
#include "formats/httplive.h"
#include "formats/m3u.h"
#include "server/server.h"
#include "util/pool.h"
#include "stream/stream_outfmt.h"


//
// pStreamerCfg0 should be set in streamer context
// pStreamerCfg1 should be set in srvcmd context
// pStreamerCfg1 can be null in mgr/web-portal context
//
#define GET_STREAMER_FROM_CONN(pc)  ((pc)->pStreamerCfg0 ? (pc)->pStreamerCfg0 : (pc)->pStreamerCfg1)

enum SRV_CMD {
  SRV_CMD_NONE            = 0,
  SRV_CMD_PLAY_FILE       = 1,
  SRV_CMD_PLAY_JUMPFILE   = 2,
  SRV_CMD_SKIP_FILE       = 3,
  SRV_CMD_PLAY_LIVE       = 4,
  SRV_CMD_PLAY_SETPROP    = 5,
  SRV_CMD_RECORD          = 6,
  SRV_CMD_RECORD_STOP     = 7,
  SRV_CMD_RECORD_PAUSE    = 8,
  SRV_CMD_PLAY_STOP       = 9,
  SRV_CMD_PAUSE           = 10,
  SRV_CMD_STATUS          = 11
};

typedef struct SRV_CTRL_NEXT {
  enum SRV_CMD            cmd;
  char                    filepath[VSX_MAX_PATH_LEN];
  char                    playlistpath[VSX_MAX_PATH_LEN];
  int                     isplaylist;
  double                  fps;
  int                     reset_rtp_seq;
  PLAYLIST_M3U_T         *pDirPlaylist; // virtual playlist of current dir contents
  CAPTURE_LOCAL_DESCR_T   captureCfg;
  STREAMER_CFG_T          streamerCfg;
} SRV_CTRL_NEXT_T;

typedef struct SRV_CTRL_RECORD {
  CAPTURE_STREAM_T        stream;
  CAPTURE_CBDATA_T        streamsOut;
} SRV_CTRL_RECORD_T;

enum URL_CAPABILITY {
  URL_CAP_LIST         =  1 << 1,
  URL_CAP_PROP         =  1 << 2,
  URL_CAP_ROOTHTML     =  1 << 3,
  URL_CAP_POST         =  1 << 4,
  URL_CAP_IMG          =  1 << 5,
  URL_CAP_TMN          =  1 << 6,
  URL_CAP_FILEMEDIA    =  1 << 7,
  URL_CAP_TSLIVE       =  1 << 8,
  URL_CAP_TSHTTPLIVE   =  1 << 9,  
  URL_CAP_MOOFLIVE     =  1 << 10,  
  URL_CAP_RTMPLIVE     =  1 << 11, 
  URL_CAP_RTSPLIVE     =  1 << 12, 
  URL_CAP_FLVLIVE      =  1 << 13, 
  URL_CAP_MKVLIVE      =  1 << 14, 
  URL_CAP_CMD          =  1 << 15,
  URL_CAP_LIVE         =  1 << 16,   
  URL_CAP_STATUS       =  1 << 17,
  URL_CAP_PIP          =  1 << 18,
  URL_CAP_DIRLIST      =  1 << 19,
  URL_CAP_CONFIG       =  1 << 20 
};

typedef struct SRV_CFG {
  const char                      *livepwd;      // live / httplive password
  const char                      *uipwd;           // UI password
  const char                      *pchannelchgr;
  const char                      *propFilePath;
  MEDIADB_DESCR_T                 *pMediaDb; 
  SESSION_CACHE_T                 *pSessionCache;
  HTTPLIVE_DATA_T                 *pHttpLiveDatas[IXCODE_VIDEO_OUT_MAX];
  MOOFSRV_CTXT_T                  *pMoofCtxts[IXCODE_VIDEO_OUT_MAX];
  STREAM_RTSP_SESSIONS_T          *pRtspSessions;
  const struct SRV_LISTENER_CFG   *pListenRtmp;
  const struct SRV_LISTENER_CFG   *pListenRtsp;
  const struct SRV_LISTENER_CFG   *pListenHttp;
  float                            throttlerate;
  float                            throttleprebuf;
  int                              enable_symlink;
  int                              disable_root_dirlist;
} SRV_CFG_T;

typedef struct SRV_CTRL {
  SRV_CTRL_NEXT_T            curCfg;
  SRV_CTRL_NEXT_T            nextCfg;
  SRV_CTRL_RECORD_T          recordCfg;
  CAPTURE_PKT_ACTION_DESCR_T noopAction;
  int                        startNext;
  int                        isRunning;
  pthread_mutex_t            mtx;
  char                       lasterrstr[256];
  int                        lasterrcode;
  SRV_CFG_T                 *pCfg;
  //uint16_t                *prtp_seq_at_end;
} SRV_CTRL_T;

//
// SRV_CTRL_T state
//
// active 
// curCfg.streamerCfg.running >= 0
//
// play file
// looping  curCfg.streamerCfg.loop | curCfg.playlist[0] != 0 -> loop_pl
// xmitpaused curCfg.streamerCfg.pauseRequested
//
// play live
// xmitpaused curCfg.streamerCfg.action.do_stream
// recording curCfg.streamerCfg.action.cbRecord | cbRecordData
// recordpaused curCfg.streamerCfg.action.do_record_post
//
// quit curCfg.streamerCfg.quitRequested


typedef struct CLIENT_CONN {
  POOL_ELEMENT_T             pool;
  SRV_CTRL_T                *ppSrvCtrl[2]; //ppSrvCtrl may be NULL if called in context
                                       // outside of srv_start_conf, such as cmdline
  STREAMER_CFG_T            *pStreamerCfg0; // shortcut to ppSrvCtrl[0]->curCfg.streamerCfg
  STREAMER_CFG_T            *pStreamerCfg1; // shortcut to ppSrvCtrl[1]->curCfg.streamerCfg
  pthread_mutex_t           *pMtx0;
  pthread_mutex_t           *pMtx1;
  pthread_t                  ptd;
  pthread_attr_t             attr;
  SOCKET_DESCR_T             sd;
  //const struct sockaddr    *psrvsaListen; // server tcp listener port

  // Digest auth connection specific state
  //char nonce[AUTH_NONCE_MAXLEN];
  //char opaque[AUTH_OPAQUE_MAXLEN];
  
  HTTP_REQ_T                 httpReq;
  struct SRV_LISTENER_CFG   *pListenCfg;
  SRV_CFG_T                 *pCfg;
  void                      *pUserData; // Used to pass CAP_ASYNC_DESCR_T for capture_rtmp
} CLIENT_CONN_T;

enum IDX_SRV_CTRL_PLAY {
  IDX_SRV_CTRL_PLAY_FILE           = 0,
  IDX_SRV_CTRL_PLAY_LIVE           = 1
};


typedef struct SRV_CTRL_PROC_DATA {
  SRV_CTRL_T       *ppSrvCtrl[2];
  unsigned int      idx;
} SRV_CTRL_PROC_DATA_T;

void srv_cmd_proc(void *parg);
void srv_ctrl_proc(void *parg);
void srv_lock_conn_mutexes(CLIENT_CONN_T *pConn, int lock);







#endif // __SERVER_CMD_H__
