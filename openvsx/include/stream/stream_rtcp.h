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


#ifndef __STREAM_RTCP_H__
#define __STREAM_RTCP_H__

#define RTCP_HDR_LEN 8

#define RTCP_RR_LEN (RTCP_HDR_LEN + 24)
#define RTCP_FIR_LEN (RTCP_HDR_LEN + 8)
#define RTCP_PLI_LEN (RTCP_HDR_LEN + 4)
#define RTCP_NACK_LEN (RTCP_HDR_LEN + 8)
#define RTCP_RR_COUNT(hdr)   ((hdr) & 0x1f)
#define RTCP_FB_TYPE RTCP_RR_COUNT

typedef struct RTCP_PKT_HDR {
  uint8_t                      hdr;
  uint8_t                      pt;
  uint16_t                     len;
  uint32_t                     ssrc;  // SSRC of the sender of this RTCP packet
} RTCP_PKT_HDR_T;


typedef struct RTCP_PKT_SR {
  RTCP_PKT_HDR_T               hdr;
  uint32_t                     ntp_msw;
  uint32_t                     ntp_lsw;
  uint32_t                     rtpts;
  uint32_t                     pktcnt;
  uint32_t                     octetcnt;
} RTCP_PKT_SR_T;

typedef struct RTCP_PKT_RR {
  RTCP_PKT_HDR_T               hdr;
  uint32_t                     ssrc_mediasrc;
  uint8_t                      fraclost;
  uint8_t                      cumulativelost[3];
  uint16_t                     seqcycles;
  uint16_t                     seqhighest;
  uint32_t                     jitter;
  uint32_t                     lsr;
  uint32_t                     dlsr;
} RTCP_PKT_RR_T;

#define RTCP_SDES_END           0
#define RTCP_SDES_CNAME         1
#define RTCP_SDES_NAME          2
#define RTCP_SDES_EMAIL         3
#define RTCP_SDES_PHONE         4
#define RTCP_SDES_LOC           5
#define RTCP_SDES_TOOL          6
#define RTCP_SDES_NOTE          7
#define RTCP_SDES_PRIV          8

typedef struct RTCP_PKT_SDES_ITEM {
  uint8_t                      type;
  uint8_t                      len;
  uint8_t                      txt[128];
} RTCP_PKT_SDES_ITEM_T;

typedef struct RTCP_PKT_SDES {
  RTCP_PKT_HDR_T               hdr;
  RTCP_PKT_SDES_ITEM_T         item;
} RTCP_PKT_SDES_T;


//
// Payload Specific FB (RFC 4585)
// Feedback Control Information
// Generic NACK
//
typedef struct RTCP_PKT_RTPFB_NACK {
  RTCP_PKT_HDR_T               hdr;
  uint32_t                     ssrc_mediasrc;
  uint16_t                     pid;   // packet id / RTP seq no of lost packet
  uint16_t                     blp;   // bitmask of following lost packets
} RTCP_PKT_RTPFB_NACK_T;

//
// Payload Specific FB (RFC 5104)
// Feedback Control Information
// Full Intra-Refresh
//
typedef struct RTCP_PKT_PSFB_FIR {
  RTCP_PKT_HDR_T               hdr;
  uint32_t                     ssrc_mediasrc;
  uint32_t                     ssrc;        // seems redundant 
  uint8_t                      seqno;
  uint8_t                      reserved[3];
} RTCP_PKT_PSFB_FIR_T;

typedef struct RTCP_PKT_PSFB_PLI {
  RTCP_PKT_HDR_T               hdr;
  uint32_t                     ssrc_mediasrc;
} RTCP_PKT_PSFB_PLI_T;

typedef struct STREAM_RTCP_SR {
  RTCP_PKT_SR_T                sr;
  RTCP_PKT_SDES_T              sdes;

  uint32_t                     pktcnt;
  uint32_t                     octetcnt;
  float                        ftsoffsetdelay;
  uint16_t                     seqhighest;       // last seq # at xmitter
  uint16_t                     pad;

  TIME_VAL                     tmLastSrSent;
  TIME_VAL                     tmLastRrRcvd;
  RTCP_PKT_RR_T                lastRrRcvd;

  //
  // Obtained from remote's RRs
  //
  uint32_t                     reportedLost;
  uint32_t                     reportedLostTot;
  uint32_t                     reportedSeqNum;
  uint64_t                     reportedSeqNumTot;
  uint32_t                     prevcumulativelost;
  uint16_t                     prevseqhighest;
  uint16_t                     pad2;

  //
  // Obtained from remote's Feedback
  //
  uint8_t                      lastRcvdFir;
  uint8_t                      lastRcvdFirSeqno; 

} STREAM_RTCP_SR_T;

#define RTCP_NACK_PKTS_MAX     4

typedef struct STREAM_RTCP_RR {
  RTCP_PKT_RR_T                rr;
  RTCP_PKT_SDES_T              sdes;

  // Used for constructing feedback packet
  RTCP_PKT_PSFB_FIR_T          fb_fir;
  RTCP_PKT_PSFB_PLI_T          fb_pli;
  RTCP_PKT_RTPFB_NACK_T        fbs_nack[RTCP_NACK_PKTS_MAX];
  unsigned int                 countNackPkts;
  uint64_t                     rembBitrateBps;

  uint32_t                     pktcnt;
  uint32_t                     pktdropcnt;
  uint32_t                     pktdroptot;
  int                          seqRolls;

  TIME_VAL                     tmLastSrRcvd;
  RTCP_PKT_SR_T                lastSrRcvd;

  struct timeval               tvPriorPkt;
  uint32_t                     tsPriorPkt; // RTP ts
  double                       rfc3550jitter;
  uint32_t                     clockHz;
  int64_t                      deviationMs0; // RTP playout time deviation from start
  int64_t                      deviationMs0Mark; 

} STREAM_RTCP_RR_T;

void rtcp_init_sdes(RTCP_PKT_SDES_T *pSdes, uint32_t ssrc);


#endif // __STREAM_RTCP_H__
