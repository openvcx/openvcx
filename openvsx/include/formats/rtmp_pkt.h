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


#ifndef __RTMP_PKT_H__
#define __RTMP_PKT_H__

//#include "formats/rtmp_auth.h"

enum RTMP_ONSTATUS_TYPE {
  RTMP_ONSTATUS_TYPE_PUBNOTIFY,
  RTMP_ONSTATUS_TYPE_RESET,
  RTMP_ONSTATUS_TYPE_PLAYSTART,
  RTMP_ONSTATUS_TYPE_PUBLISH,
  RTMP_ONSTATUS_TYPE_DATASTART,
};

typedef struct RTMP_CLIENT_CFG {
  int                           rtmpfp9;
  const char                   *prtmppageurl;
  const char                   *prtmpswfurl;
} RTMP_CLIENT_CFG_T;


typedef struct RTMP_CLIENT_PARAMS {
  const char                  *flashVer;
  const char                  *playElem;
  RTMP_CLIENT_CFG_T            cfg;
} RTMP_CLIENT_PARAMS_T;

struct RTMP_CTXT;

int rtmp_cbReadDataNet(void *pArg, unsigned char *pData, unsigned int len);
int rtmp_parse_readpkt_full(struct RTMP_CTXT *pRtmp, int needpacket, struct FLV_AMF *pAmfList);
int rtmp_send_chunkdata(struct RTMP_CTXT *pRtmp, const uint8_t rtmpStreamIdx,
                        const unsigned char *pData, unsigned int lenData,
                        unsigned int chunkStartIdx);

int rtmp_create_hdr(unsigned char *buf, uint8_t streamIdx,
                            uint32_t ts, uint32_t sz, uint8_t contentType,
                            uint32_t dest, int hdrlen);
int rtmp_create_vidstart(struct RTMP_CTXT *pRtmp);
int rtmp_create_audstart(struct RTMP_CTXT *pRtmp);
int rtmp_create_serverbw(struct RTMP_CTXT *pRtmp, unsigned int bw);
int rtmp_create_clientbw(struct RTMP_CTXT *pRtmp, unsigned int bw);
int rtmp_create_ping(struct RTMP_CTXT *pRtmp, uint16_t type, uint32_t arg);
int rtmp_create_chunksz(struct RTMP_CTXT *pRtmp);
int rtmp_create_result_invoke(struct RTMP_CTXT *pRtmp);
int rtmp_create_result(struct RTMP_CTXT *pRtmp);
int rtmp_create_error(struct RTMP_CTXT *pRtmp, double value, const char *code, const char *descr);
int rtmp_create_onstatus(struct RTMP_CTXT *pRtmp, enum RTMP_ONSTATUS_TYPE type);
int rtmp_create_notify(struct RTMP_CTXT *pRtmp);
int rtmp_create_notify_netstart(struct RTMP_CTXT *pRtmp);
int rtmp_create_onmeta(struct RTMP_CTXT *pRtmp);
int rtmp_create_setDataFrame(struct RTMP_CTXT *pRtmp);
int rtmp_create_connect(struct RTMP_CTXT *pRtmp, const struct RTMP_CLIENT_PARAMS *pClient);
int rtmp_create_fcpublish(struct RTMP_CTXT *pRtmp, const struct RTMP_CLIENT_PARAMS *pClient);
int rtmp_create_publish(struct RTMP_CTXT *pRtmp, const struct RTMP_CLIENT_PARAMS *pClient);
int rtmp_create_createStream(struct RTMP_CTXT *pRtmp, int contentType);
int rtmp_create_releaseStream(struct RTMP_CTXT *pRtmp, const struct RTMP_CLIENT_PARAMS *pClient);
int rtmp_create_play(struct RTMP_CTXT *pRtmp, const struct RTMP_CLIENT_PARAMS *pClient);
int rtmp_create_close(struct RTMP_CTXT *pRtmp);
int rtmp_create_onfcpublish(struct RTMP_CTXT *pRtmp);
int rtmp_send(struct RTMP_CTXT *pRtmp, const char *descr);
int rtmp_sendbuf(struct RTMP_CTXT *pRtmp, const unsigned char *buf, unsigned int sz, const char *descr);



#endif // __RTMP_PKT_H__
