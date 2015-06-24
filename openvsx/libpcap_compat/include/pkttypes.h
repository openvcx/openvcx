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

#ifndef __PACKET_TYPES_H__
#define __PACKET_TYPES_H__


#ifndef WIN32
#include "unixcompat.h"
#endif // WIN32

#if !defined(WIN32) && !defined(__MINGW32__) && !defined(ANDROID)
#include <net/ethernet.h>
#endif // __MINGW32__

#if defined(WIN32)
#define __LITTLE_ENDIAN  100
#define __BYTE_ORDER __LITTLE_ENDIAN
#endif // WIN32


//
// net/ethernet.h
//
#ifndef ETHERTYPE_IP
#define ETHERTYPE_IP              0x0800          /* IP */
#endif // ETHERTYPE_IP

#ifndef ETHERTYPE_VLAN
#define ETHERTYPE_VLAN              0x8100        
#endif // ETHERTYPE_VLAN

#ifndef ETHERTYPE_ARP
#define ETHERTYPE_ARP             0x0806 
#endif // ETHERTYPE_ARP

//
// linux/if_arp.h
//
#ifndef ARPHDR_ETHER
#define ARPHRD_ETHER              1 
#endif // ARPHDR_ETHER

#ifndef ARPOP_REQUEST
#define ARPOP_REQUEST             1
#endif // ARPOP_REQUEST

#ifndef ARPOP_REPLY
#define ARPOP_REPLY               2  
#endif // ARPOP_REPLY

//
// linux/if_ether.h
//
#ifndef ETH_FRAME_LEN
#define ETH_FRAME_LEN             1514            /* Max. octets in frame sans FCS */
#endif // ETH_FRAME_LEN

#ifndef ETH_ALEN
#define ETH_ALEN                  6 
#endif // ETH_ALEN


// Custom defines
#ifndef IPV4_ADDR_LEN
#define IPV4_ADDR_LEN             4
#endif // IPV4_ADDR_LEN


#define PKTGEN_DEFAULT_TTL                64


#if defined(WIN32) || defined(ANDROID)
struct ether_header {
  unsigned char  ether_dhost[ETH_ALEN];      /* destination eth addr */
  unsigned char  ether_shost[ETH_ALEN];      /* source ether addr    */
  unsigned short ether_type;                 /* packet type ID field */
};
#endif // __MINGW32__
//#endif // _NET_ETHERNET_H

typedef struct {
  unsigned char llc_dsap;
  unsigned char llc_ssap;
  unsigned char llc_cr;
  unsigned char snap_orgcode[3];
  unsigned short snap_pid;
} llc_t;


struct ip {

#if defined(__APPLE__) || defined(__MINGW32__)

#if BYTE_ORDER==BIG_ENDIAN
    unsigned char ip_v:4;               
    unsigned char ip_hl:4;             
#else // BIG_ENDIAN
    unsigned char ip_hl:4;             
    unsigned char ip_v:4;           
#endif // BIG_ENDIAN

#else // __APPLE__

/*
#if defined(BIG_ENDIAN)
    unsigned char ip_v:4;               
    unsigned char ip_hl:4;             
#else // BIG_ENDIAN
    unsigned char ip_hl:4;              
    unsigned char ip_v:4;        
#endif // BIG_ENDIAN
*/

#if __BYTE_ORDER == __LITTLE_ENDIAN
    unsigned char ip_hl:4;               
    unsigned char ip_v:4;               
#endif
#if __BYTE_ORDER == __BIG_ENDIAN
    unsigned char ip_v:4;                
    unsigned char ip_hl:4;             
#endif


#endif // __APPLE_


    unsigned char ip_tos;                /* type of service */
    uint16_t ip_len;                     /* total length */
    uint16_t ip_id;                      /* identification */
    uint16_t ip_off;                     /* fragment offset field */
#define IP_RF 0x8000                     /* reserved fragment flag */
#define IP_DF 0x4000                     /* dont fragment flag */
#define IP_MF 0x2000                     /* more fragments flag */
#define IP_OFFMASK 0x1fff                /* mask for fragmenting bits */
    unsigned char ip_ttl;                /* time to live */
    unsigned char ip_p;                  /* protocol */
    uint16_t ip_sum;                     /* checksum */
    struct in_addr ip_src, ip_dst;       /* source and dest address */
};




#if defined(__MINGW32__)
struct in6_addr
  {
    union
      {
        uint8_t u6_addr8[16];
        uint16_t u6_addr16[8];
        uint32_t u6_addr32[4];
      } in6_u;
#define s6_addr                 in6_u.u6_addr8
#define s6_addr16               in6_u.u6_addr16
#define s6_addr32               in6_u.u6_addr32
};
#endif // __MINGW32__



struct ip6_hdr {
  union  {
    struct ip6_hdrctl {
      uint32_t ip6_un1_flow;   /* 4 bits version, 8 bits TC, 20 bits flow-ID */
      uint16_t ip6_un1_plen;   /* payload length */
      uint8_t  ip6_un1_nxt;    /* next header */
      uint8_t  ip6_un1_hlim;   /* hop limit */
    } ip6_un1;
    uint8_t ip6_un2_vfc;       /* 4 bits version, top 4 bits tclass */
  } ip6_ctlun;
  struct in6_addr ip6_src;      /* source address */
  struct in6_addr ip6_dst;      /* destination address */
};

char *inet6_ntoa(struct in6_addr in6);

#define IPV6_HDR_SIZE               40
#define IPV4_HDR_SIZE               20

struct udphdr {
  unsigned short     source;
  unsigned short     dest;
  unsigned short     len;
  unsigned short     check;
};

//#define IPPROTO_UDP       0x11

struct tcphdr
  {
    unsigned short source;
    unsigned short dest;
    unsigned int   seq;
    unsigned int   ack_seq;
    unsigned char  doffres; 
    unsigned char  flags;
    //unsigned short res1:4;
    //unsigned short doff:4;
    //unsigned short fin:1;
    //unsigned short syn:1;
    //unsigned short rst:1;
    //unsigned short psh:1;
    //unsigned short ack:1;
    //unsigned short urg:1;
    //unsigned short res2:2;
    unsigned short window;
    unsigned short check;
    unsigned short urg_ptr;
};


#ifndef MAX_RTP_SEQ
#define MAX_RTP_SEQ 65535
#endif // MAX_RTP_SEQ




#endif //__PACKET_TYPES_H__
