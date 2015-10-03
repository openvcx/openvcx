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


#ifndef __RTMP_PARSE_H__
#define __RTMP_PARSE_H__

#include <stdio.h>
#include "fileutil.h"
#include "unixcompat.h"
#include "codecfmt.h"


#define RTMP_PORT_DEFAULT                1935


//#define DEBUG_RTMP_READ 1

#define RTMP_STREAM_IDX_CTRL          0x02
#define RTMP_STREAM_IDX_INVOKE        0x03
#define RTMP_STREAM_IDX_NOTIFY        0x04
#define RTMP_STREAM_IDX_5             0x05
#define RTMP_STREAM_IDX_6             0x06
#define RTMP_STREAM_IDX_7             0x07
#define RTMP_STREAM_IDX_8             0x08

#define RTMP_STREAM_IDX_MAX           RTMP_STREAM_IDX_8

#define RTMP_STREAM_INDEXES           (RTMP_STREAM_IDX_MAX - RTMP_STREAM_IDX_CTRL + 1)

#define RTMP_CONTENT_TYPE_CHUNKSZ     0x01
#define RTMP_CONTENT_TYPE_BYTESREAD   0x03
#define RTMP_CONTENT_TYPE_PING        0x04
#define RTMP_CONTENT_TYPE_SERVERBW    0x05
#define RTMP_CONTENT_TYPE_CLIENTBW    0x06
#define RTMP_CONTENT_TYPE_AUDDATA     0x08  // FLV_TAG_AUDIODATA
#define RTMP_CONTENT_TYPE_VIDDATA     0x09  // FLV_TAG_VIDEODATA
#define RTMP_CONTENT_TYPE_MSG         0x11
#define RTMP_CONTENT_TYPE_NOTIFY      0x12
#define RTMP_CONTENT_TYPE_INVOKE      0x14
#define RTMP_CONTENT_TYPE_FLV         0x16

#define RTMP_CHUNK_SZ_DEFAULT         128
#define RTMP_CHUNK_SZ_OUT             4096
#define RTMP_HANDSHAKE_HDR            0x03
#define RTMP_HANDSHAKE_SZ             1536
#define RTMP_HANDSHAKE_TMT_MS         6000
#define RTMP_RCV_TMT_MS               15000
#define RTMP_RCV_IDLE_TMT_MS          50000

#define RTMP_NETCONNECT_REJECTED     "NetConnection.Connect.Rejected"
#define RTMP_NETCONNECT_SUCCESS      "NetConnection.Connect.Success"
#define RTMP_NETSTREAM_DATA_START    "NetStream.Data.Start"
#define RTMP_NETSTREAM_PLAY_START    "NetStream.Play.Start"
#define RTMP_NETSTREAM_PLAY_RESET    "NetStream.Play.Reset"
#define RTMP_NETSTREAM_PUBLISH_START "NetStream.Publish.Start"

typedef enum RTMP_CLIENTBW_LIMIT_TYPE {
  RTMP_CLIENTBW_LIMIT_TYPE_HARD             = 0,
  RTMP_CLIENTBW_LIMIT_TYPE_SOFT             = 1,
  RTMP_CLIENTBW_LIMIT_TYPE_DYNAMIC          = 2
} RTMP_CLIENTBW_LIMIT_TYPE_T;


typedef int (* RTMP_CB_READDATA)(void *, unsigned char *, unsigned int);

typedef struct RTMP_HDR {
  uint8_t                 id;            // 1 byte
  uint32_t                ts;            // 3 bytes
  uint32_t                szPkt;         // 3 bytes
  uint8_t                 contentType;   // 1 byte
  uint32_t                dest;          // 4 bytes

  unsigned char           buf[16];
  unsigned int            szHdr;
} RTMP_HDR_T;

typedef struct RTMP_PKT {
  RTMP_HDR_T              hdr;
  unsigned int            idxInHdr;
  unsigned int            idxInChunk;
  unsigned int            idxInPkt;
  unsigned int            szHdr0;
  unsigned int            tsAbsolute;
} RTMP_PKT_T;



typedef enum RTMP_STATE {
  RTMP_STATE_INVALID             = 0,
  RTMP_STATE_ERROR               = 1,
  RTMP_STATE_CLI_DELETESTREAM    = 2,
  RTMP_STATE_DISCONNECTED        = 3,
  RTMP_STATE_CLI_START           = 4,
  RTMP_STATE_CLI_HANDSHAKEDONE   = 5,
  RTMP_STATE_CLI_HAVESERVERBW    = 6,
  RTMP_STATE_CLI_CONNECT         = 7,
  RTMP_STATE_CLI_FCPUBLISH       = 8,
  RTMP_STATE_CLI_CREATESTREAM    = 9,
  RTMP_STATE_CLI_RELEASESTREAM   = 10,
  RTMP_STATE_CLI_PUBLISH         = 11,
  RTMP_STATE_CLI_PLAY            = 12,
  RTMP_STATE_CLI_PAUSE           = 13,
  RTMP_STATE_CLI_ONMETA          = 14,
  RTMP_STATE_CLI_DONE            = 15
} RTMP_STATE_T;

typedef enum RTMP_METHOD {
  RTMP_METHOD_UNKNOWN            = 0,
  RTMP_METHOD_ERROR              = 1,
  RTMP_METHOD_RESULT             = 2,
  RTMP_METHOD_CONNECT            = 3,
  RTMP_METHOD_PLAY               = 4,
  RTMP_METHOD_CREATESTREAM       = 5,
  RTMP_METHOD_DELETESTREAM       = 6,
  RTMP_METHOD_CLOSESTREAM        = 7,
  RTMP_METHOD_RELEASESTREAM      = 8,
  RTMP_METHOD_PAUSERAW           = 9,
  RTMP_METHOD_PUBLISH            = 10,
  RTMP_METHOD_UNPUBLISH          = 11,
  RTMP_METHOD_FCPUBLISH          = 12,
  RTMP_METHOD_FCUNPUBLISH        = 13,
  RTMP_METHOD_SAMPLEACCESS       = 14,
  RTMP_METHOD_SETDATAFRAME       = 15,
  RTMP_METHOD_SERVERBW           = 16,
  RTMP_METHOD_CLIENTBW           = 17,
  RTMP_METHOD_PING               = 18,
  RTMP_METHOD_CHUNKSZ            = 19,
  RTMP_METHOD_BYTESREAD          = 20,
  RTMP_METHOD_ONMETADATA         = 21,
  RTMP_METHOD_ONSTATUS           = 22,
  RTMP_METHOD_ONFCPUBLISH        = 23,
  RTMP_METHOD_ONFCSUBSCRIBE      = 24,
} RTMP_METHOD_T;

#define RTMP_PARAM_LEN_MAX          128

typedef struct RTMP_CONNECT_PARAMS {
  char                     app[RTMP_PARAM_LEN_MAX];
  char                     tcUrl[RTMP_PARAM_LEN_MAX];
  char                     play[RTMP_PARAM_LEN_MAX];
  double                   capabilities;
  double                   objEncoding;
  double                   createStreamCode;
} RTMP_CONNECT_PARAMS_T;

typedef struct RTMP_PUBLISH_PARAMS {
  char                     fcpublishurl[RTMP_PARAM_LEN_MAX];
  char                     publishurl[RTMP_PARAM_LEN_MAX];
  char                     publishurl2[RTMP_PARAM_LEN_MAX];
} RTMP_PUBLISH_PARAMS_T;


typedef struct RTMP_CTXT {
  BYTE_STREAM_T            in;
  BYTE_STREAM_T            out;
  RTMP_CB_READDATA         cbRead; // cb for reading packet contents from source
  void                    *pCbData;
  SOCKET_DESCR_T          *pSd;
  RTMP_PKT_T               pkt[RTMP_STREAM_INDEXES];
  unsigned int             streamIdx;
  unsigned int             streamIdxPrev;
  unsigned int             chunkSzIn; 
  unsigned int             chunkSzOut; // TODO: should this be stream index specific?
  RTMP_CONNECT_PARAMS_T    connect;
  RTMP_PUBLISH_PARAMS_T    publish;
  uint8_t                  contentTypeLastInvoke;
  unsigned int             rcvTmtMs;
  struct timeval           tvLastRd;

  //TODO: seperate server output stream context from parse ctxt
  int                      isclient;
  enum RTMP_STATE          state;
  enum RTMP_METHOD         methodParsed;
  double                   advCapabilities; 
  double                   advObjEncoding; 
  unsigned int             serverbw;
  unsigned int             bytesRead;
  struct timeval           tmLastPing;
  CODEC_AV_CTXT_T          av;
  int                      novid;
  int                      noaud;

  unsigned int             requestOutIdx; // xcode outidx

} RTMP_CTXT_T;

int rtmp_parse_init(RTMP_CTXT_T *pRtmp, unsigned int insz, unsigned int outsz);
void rtmp_parse_close(RTMP_CTXT_T *pRtmp);
int rtmp_parse_reset(RTMP_CTXT_T *pRtmp);
int rtmp_parse_readpkt(RTMP_CTXT_T *pRtmp);
int rtmp_parse_invoke(RTMP_CTXT_T *pRtmp, FLV_AMF_T *pAmf);

const FLV_AMF_T *rtmp_amf_find(const FLV_AMF_T *pAmf, const char *str);
const char *rtmp_amf_get_key_string(const FLV_AMF_T *pAmf);

int rtmp_handshake_srv(RTMP_CTXT_T *pRtmp);
int rtmp_handshake_cli(RTMP_CTXT_T *pRtmp, int rtmpfp9);



#endif // __RTMP_PARSE_H__
