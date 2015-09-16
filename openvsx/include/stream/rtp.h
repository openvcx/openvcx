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


#ifndef __RTP_H__
#define __RTP_H__

#ifdef WIN32
#include "unixcompat.h"
#else
#include <netinet/in.h>
#endif // WIN32

//
//   The RTP header has the following format:
//
//    0                   1                   2                   3
//    0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1 2 3 4 5 6 7 8 9 0 1
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |V=2|P|X|  CC   |M|     PT      |       sequence number         |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |                           timestamp                           |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//   |           synchronization source (SSRC) identifier            |
//   +=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+=+
//   |            contributing source (CSRC) identifiers             |
//   |                             ....                              |
//   +-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+-+
//


#define UDP_HEADER_LEN                                8
#define RTP_HEADER_LEN                                12

#define RTP_VERSION                                   2
#define RTP8_VERSION                                  0x80 
#define RTP_VERSION_BITSHIFT                          6

#define RTP_PT_MASK                                   0x7f
#define RTP_PT_MARKER_MASK                            0x80

#define MAX_IP_STR_LEN                                48




//
// RTP payload types
//
#define RTP_PAYLOAD_TYPE_PCMU         0     // ITU-T G.711 PCMU
#define RTP_PAYLOAD_TYPE_1016         1     // USA Federal Standard FS-1016
#define RTP_PAYLOAD_TYPE_G721         2     // ITU-T G.721 (G.726)
#define RTP_PAYLOAD_TYPE_GSM          3     // GSM 06.10   (RPE-LTP)
#define RTP_PAYLOAD_TYPE_G723         4     // ITU-T G.723 (G.723.1)
#define RTP_PAYLOAD_TYPE_DVI4_8       5     // DVI4 8Khz
#define RTP_PAYLOAD_TYPE_DVI4_16      6     // DVI4 16Khz
#define RTP_PAYLOAD_TYPE_LPC          7     // Experimental linear predictive encoding from Xerox PARC
#define RTP_PAYLOAD_TYPE_PCMA         8     // ITU-T G.711 pCMA
#define RTP_PAYLOAD_TYPE_G722         9     // ITU-T G.722
#define RTP_PAYLOAD_TYPE_L16_STEREO   10    // 16 bit uncompressed audio, stereo
#define RTP_PAYLOAD_TYPE_L16_MONO     11    // 16 bit uncompressed audio, mono
#define RTP_PAYLOAD_TYPE_QCEPT        12    // Qualcomm PureVoice CODEC  CELP RFC 2658
#define RTP_PAYLOAD_TYPE_QCELP        RTP_PAYLOAD_TYPE_QCEPT     
#define RTP_PAYLOAD_TYPE_CN           13    // Comfort Noise
#define RTP_PAYLOAD_TYPE_MPA          14    // MPEG-1/2 Audio    RFC 2550
#define RTP_PAYLOAD_TYPE_G728         15    // ITU-T G.728
#define RTP_PAYLOAD_TYPE_DVI4_11025   16    // DVI4 11025 Hz
#define RTP_PAYLOAD_TYPE_DVI4_22050   17    // DVI4 22050 Hz
#define RTP_PAYLOAD_TYPE_G729         18    // ITU-T G.729
#define RTP_PAYLOAD_TYPE_CELLB        25    // Sun CellB video encoding
#define RTP_PAYLOAD_TYPE_JPG          26    // JPEG compressed video RFC 2435
#define RTP_PAYLOAD_TYPE_NV           28    // 'nv' program
#define RTP_PAYLOAD_TYPE_H261         31    // ITU-T H.261  RFC 2032
#define RTP_PAYLOAD_TYPE_MPV          32    // MPEG-1/2 Video  RFC 2250
#define RTP_PAYLOAD_TYPE_MP2T         33    // MPEG-2 transport stream RFC 2250
#define RTP_PAYLOAD_TYPE_H263         34    // ITU-T H.263

#define RTP_PAYLOAD_TYPE_DYNAMIC      96

//
// assumed dynamic / reserved payload types
//
//#define RTP_PAYLOAD_TYPE_CN2          19
//#define RTP_PAYLOAD_TYPE_DTMF_100     100   // X-NSE
//#define RTP_PAYLOAD_TYPE_DTMF_73      73
//#define RTP_PAYLOAD_TYPE_DTMF_121     121      
//#define RTP_PAYLOAD_TYPE_DTMF_RELAY   122
#define RTP_PAYLOAD_TYPE_INVALID      255


#define RTPHDR32_SEQ_NUM_MASK           0x0000ffff
#define RTPHDR32_EXT_MASK               0x10000000
#define RTPHDR32_CSRC_MASK              0x0f000000
#define RTPHDR32_TYPE_MASK              0x007f0000
#define RTPHDR32_VERSION_MASK           0xc0000000
#define RTPHDR32_MARKED_MASK            0x00800000

#define RTPHDR8_VERSION_MASK            0xc0
#define RTPHDR8_VERSION_PADDING_MASK    0xe0

#define RTCP_HDR_VALID(hdr8)         (((hdr8) & 0xe0)  == 0x80)

// Payload Type
#define RTCP_PT_SR                     200
#define RTCP_PT_RR                     201
#define RTCP_PT_SDES                   202
#define RTCP_PT_BYE                    203
#define RTCP_PT_APP                    204
#define RTCP_PT_RTPFB                  205 // Transport layer specific FB (RFC 4585)
#define RTCP_PT_PSFB                   206 // Payload Specific FB (RFC 5104)

//
// Feedback Control Information payload-specific types
//
#define RTCP_FCI_RTPFB_NACK             1 // Generick NACK (RFC 4585 6.2.1)

#define RTCP_FCI_PSFB_PLI               1 // Picture Loss Indication (RFC 4585)
#define RTCP_FCI_PSFB_SLI               2 // Slice Lost Indication (RFC 4585)
#define RTCP_FCI_PSFB_RPSI              3 // Reference Picture Selection Indication (RFC 4585)
#define RTCP_FCI_PSFB_FIR               4 // Full Intra Refresh (RFC 5104)
#define RTCP_FCI_PSFB_TSTR              5 // Temporal-Spatial Trade-off Request (RFC 5104)
#define RTCP_FCI_PSFB_TSTN              6 // Temporal-Spatial Trade-off Notification (RFC 5104)
#define RTCP_FCI_PSFB_VBCM              7 // Video Back Channel Message (RFC 5104)
#define RTCP_FCI_PSFB_APP              15 // Application layer FB (RFC 4585)

struct rtphdr {
   unsigned char    header;     
   unsigned char    pt;             
   uint16_t         sequence_num;        
   uint32_t         timeStamp;            
   uint32_t         ssrc;                 
};

typedef struct {
   uint32_t header;      // rtp header
   uint32_t timeStamp;      // timestamp
   uint32_t ssrc;           // synchronization source
   //unsigned int csrc[1];        // optional CSRC list
} rtp_t;



#endif // __RTP_H__
