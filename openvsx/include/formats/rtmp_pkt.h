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


enum RTMP_ONSTATUS_TYPE {
  RTMP_ONSTATUS_TYPE_PUBNOTIFY,
  RTMP_ONSTATUS_TYPE_RESET,
  RTMP_ONSTATUS_TYPE_START,
  RTMP_ONSTATUS_TYPE_PUBLISH
};

typedef struct RTMP_CLIENT_PARAMS {
  const char                  *app;
  const char                  *flashVer;
  const char                  *swfUrl;
  const char                  *tcUrl;
  const char                  *pageUrl;
  const char                  *playElem;
} RTMP_CLIENT_PARAMS_T;


int rtmp_cbReadDataNet(void *pArg, unsigned char *pData, unsigned int len);
int rtmp_parse_readpkt_full(RTMP_CTXT_T *pRtmp, int needpacket);
int rtmp_send_chunkdata(RTMP_CTXT_T *pRtmp, const uint8_t rtmpStreamIdx,
                        const unsigned char *pData, unsigned int lenData,
                        unsigned int chunkStartIdx);

int rtmp_create_hdr(unsigned char *buf, uint8_t streamIdx,
                            uint32_t ts, uint32_t sz, uint8_t contentType,
                            uint32_t dest, int hdrlen);
int rtmp_create_vidstart(RTMP_CTXT_T *pRtmp);
int rtmp_create_audstart(RTMP_CTXT_T *pRtmp);
int rtmp_create_serverbw(RTMP_CTXT_T *pRtmp, unsigned int bw);
int rtmp_create_clientbw(RTMP_CTXT_T *pRtmp, unsigned int bw);
int rtmp_create_ping(RTMP_CTXT_T *pRtmp, uint16_t type, uint32_t arg);
int rtmp_create_chunksz(RTMP_CTXT_T *pRtmp, unsigned int sz);
int rtmp_create_result_invoke(RTMP_CTXT_T *pRtmp);
int rtmp_create_result(RTMP_CTXT_T *pRtmp);
int rtmp_create_onstatus(RTMP_CTXT_T *pRtmp, enum RTMP_ONSTATUS_TYPE type);
int rtmp_create_notify(RTMP_CTXT_T *pRtmp);
int rtmp_create_notify_netstart(RTMP_CTXT_T *pRtmp);
int rtmp_create_onmeta(RTMP_CTXT_T *pRtmp);
int rtmp_create_connect(RTMP_CTXT_T *pRtmp, RTMP_CLIENT_PARAMS_T *pClient);
int rtmp_create_createStream(RTMP_CTXT_T *pRtmp);
int rtmp_create_play(RTMP_CTXT_T *pRtmp, RTMP_CLIENT_PARAMS_T *pClient);
int rtmp_create_onfcpublish(RTMP_CTXT_T *pRtmp);



#endif // __RTMP_PKT_H__
