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


#ifndef __CAPTURE_RTP_H__
#define __CAPTURE_RTP_H__

#include "unixcompat.h"
#if defined(__MINGW32__) 
#include <winsock2.h>
#elif !defined(WIN32)
#include <netinet/in.h>
#endif // WIN32

#include <stdio.h>
#include "pcap_compat.h"
#include "pkttypes.h"
#include "stream/rtp.h"
#include "stream/stream_rtcp.h"
#include "capture.h"


#define RTP_STREAM_IDLE_EXPIRE_MS              3000

#define RTP_MIN_PORT                           1024

#define UINT32_ROLL_VAL               0xffffffff
#define UINT32_ROLL_MIN               0x3fffffff
#define UINT32_ROLL_MAX               0xbfffffff
#define UINT16_ROLL_VAL               0xffff
#define UINT16_ROLL_MIN               0x3fff
#define UINT16_ROLL_MAX               0xbfff

#define DID_UINT16_ROLL(seq, prev)     \
                     ((seq < UINT16_ROLL_MIN && prev > UINT16_ROLL_MAX) ? 1 : 0)

#define DID_UINT32_ROLL(seq, prev)     \
                     ((seq < UINT32_ROLL_MIN && prev > UINT32_ROLL_MAX) ? 1 : 0)

#define RTP_SEQ_DELTA(seq, prev) (DID_UINT16_ROLL(seq, prev) ? (0xffff-prev + seq + 1) : (seq - prev))

#define RTP_TS_DELTA(ts, prev) (DID_UINT32_ROLL(ts, prev) ? (0xffffffff-prev + ts + 1) : (ts - prev))


void rtp_captureFree(CAPTURE_STATE_T *pState);
CAPTURE_STATE_T *rtp_captureCreate(unsigned int numStreams, 
                                   unsigned int jtBufSzPktsVid,
                                   unsigned int jtBufSzPktsAud,
                                   unsigned int jtBufPktBufSz,
                                   unsigned int cfgMaxVidRtpGapWaitTmMs,
                                   unsigned int cfgMaxAudRtpGapWaitTmMs);

int rtp_captureFlush(CAPTURE_STATE_T *pState);
void rtp_capturePrint(const CAPTURE_STATE_T *pState, FILE *fp);
void rtp_capturePrintLog(const CAPTURE_STATE_T *pState, int logLevel);
int capture_delete_stream(CAPTURE_STATE_T *pState, CAPTURE_STREAM_T *pStream);


int capture_decodeRtp(const struct ether_header *peth,
                              const struct ip *pip,
                              const struct udphdr *pudp,
                              const rtp_t *rtp,
                              const struct pcap_pkthdr *pkthdr,
                              COLLECT_STREAM_PKT_T *pkt);

int capture_decodeUdpRaw(const struct ether_header *peth,
                         const struct ip *pip,
                         const struct udphdr *pudp,
                         const struct pcap_pkthdr *pkthdr,
                         COLLECT_STREAM_PKT_T *pkt);


const CAPTURE_STREAM_T *capture_processRtp(CAPTURE_STATE_T *pState, 
                                     const COLLECT_STREAM_PKT_T *pPkt,
                                     const CAPTURE_FILTER_T *pFilter);
int capture_processUdpRaw(CAPTURE_STATE_T *pState, 
                          const COLLECT_STREAM_PKT_T *pPkt,
                          const CAPTURE_FILTER_T *pFilter);
int capture_processTcpRaw(CAPTURE_STATE_T *pState,
                          const COLLECT_STREAM_PKT_T *pPkt,
                          const CAPTURE_FILTER_T *pFilter);

//int capture_rtcp_create(STREAM_RTCP_RR_T *pRtcp, int do_rr, int do_fir, int do_pli, int do_nack,
//                        unsigned char *pBuf, unsigned int lenbuf);
//int capture_rtcp_set_local_ssrc(STREAM_RTCP_RR_T *pRtcp, uint32_t ssrc);
//CAPTURE_STREAM_T *capture_rtcp_lookup(const CAPTURE_STATE_T *pState, 
//                                      const COLLECT_STREAM_KEY_T *pKey);
int capture_check_rtp_bye(const CAPTURE_STATE_T *pState);



#endif //__CAPTURE_RTP_H__
