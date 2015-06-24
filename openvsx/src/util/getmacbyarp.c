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





#include "vsx_common.h"

#ifdef WIN32

#else  // WIN32

#include <sys/socket.h>

#endif // WIN32

#include "pktgen.h"
#include "util/getmacbyarp.h"


typedef struct ARP_PKT_HDR {
  unsigned short          hwType;
  unsigned short          protoType;
  unsigned char           macLen;
  unsigned char           protoLen;
  unsigned short          opt;
  unsigned char           senderMac[ETH_ALEN];
  unsigned char           senderIpv4[IPV4_ADDR_LEN];
  unsigned char           targetMac[ETH_ALEN];
  unsigned char           targetIpv4[IPV4_ADDR_LEN];

  // Create padding for word alignment
  unsigned short          padding;
} ARP_PKT_HDR_T;

typedef struct ARP_PKT {
  struct ether_header     *pEth;
  ARP_PKT_HDR_T           *pHdr;
  unsigned int             len;
  unsigned char            data[1024];
} ARP_PKT_T;

// Expected size of ARP packet body
#define ARP_PKT_HDR_SZ        28


typedef struct MAC_ENTRY {
  long                    tmDiscovered;
  uint32_t                ipv4;
  unsigned char           mac[ETH_ALEN];
} MAC_ENTRY_T;

// The hash table of media streams using the key of type COLLECT_STREAM_KEY
//static tchash_table_t              *g_macsHash = NULL;

static pthread_mutex_t              g_mtxMacs;
static int                          g_mtxInit;
static unsigned int                 g_macExpirationSec;

static MAC_ENTRY_T 
g_macList[10];

static MAC_ENTRY_T *findmac(uint32_t ipv4) {
  unsigned int idx;

  for(idx = 0; idx < sizeof(g_macList) / sizeof(g_macList[0]); idx++) {
    if(g_macList[idx].ipv4 == ipv4) {
      return &g_macList[idx];
    }
  }
  return NULL;
}

static MAC_ENTRY_T *storemac(const MAC_ENTRY_T *pMac) {
  unsigned int idx;

  for(idx = 0; idx < sizeof(g_macList) / sizeof(g_macList[0]); idx++) {
    if(g_macList[idx].ipv4 == 0) {
      memcpy(&g_macList[idx], pMac, sizeof(MAC_ENTRY_T));
      return &g_macList[idx];
    }
  }
  return NULL;
}

static int clearmac(const MAC_ENTRY_T *pMac) {
  unsigned int idx;

  for(idx = 0; idx < sizeof(g_macList) / sizeof(g_macList[0]); idx++) {
    if(&g_macList[idx] == pMac) {
      memset(&g_macList[idx], 0, sizeof(MAC_ENTRY_T));
      return 0;
    }
  }
  return -1;
}

#ifdef DISABLE_PCAP

int arpmac_GetMac(long ipv4, unsigned char mac[],  unsigned short vlan) {

  LOG(X_ERROR("arpmac_GetMac not implemented when not linking with pcap"));
  return -1;
}

#else // DISABLE_PCAP

static int createArpPkt(ARP_PKT_T *pArpPkt, unsigned int ipv4, unsigned short vlan) {

  unsigned int localIpv4 = pktgen_GetLocalIp();
  unsigned short *pus = NULL;
  pArpPkt->len = 14;

  pArpPkt->pEth = (struct ether_header *) pArpPkt->data;

  memset(pArpPkt->pEth->ether_dhost, 0xff, ETH_ALEN);
  memcpy(pArpPkt->pEth->ether_shost, pktgen_GetLocalMac(), ETH_ALEN);

  if(vlan != 0) {
    pArpPkt->pEth->ether_type = htons(ETHERTYPE_VLAN);
    pus = (unsigned short *) ((unsigned char *)pArpPkt->pEth + 14);
    *pus = htons(vlan);
    pus = (unsigned short *) ((unsigned char *)pArpPkt->pEth + 16);
    *pus = htons(ETHERTYPE_ARP);
    pArpPkt->pHdr = (ARP_PKT_HDR_T *) (((unsigned char *)pArpPkt->data) + 18);
    pArpPkt->len += 4;
  } else {
    pArpPkt->pEth->ether_type = htons(ETHERTYPE_ARP);
    pArpPkt->pHdr = (ARP_PKT_HDR_T *) (((unsigned char *)pArpPkt->data) + 14);
  }


  pArpPkt->pHdr->hwType = htons(ARPHRD_ETHER);
  pArpPkt->pHdr->protoType = htons(ETHERTYPE_IP);
  pArpPkt->pHdr->macLen = ETH_ALEN;
  pArpPkt->pHdr->protoLen = IPV4_ADDR_LEN;
  pArpPkt->pHdr->opt = htons(ARPOP_REQUEST);
  memcpy(pArpPkt->pHdr->senderMac, pktgen_GetLocalMac(), ETH_ALEN);
  memcpy(pArpPkt->pHdr->senderIpv4, &localIpv4, IPV4_ADDR_LEN);
  memset(pArpPkt->pHdr->targetMac, 0x00, ETH_ALEN);
  memcpy(pArpPkt->pHdr->targetIpv4, &ipv4, IPV4_ADDR_LEN);

  pArpPkt->len += 28;

  return 0;
}


static int getMacByArp(unsigned int ipv4, unsigned char mac[], unsigned short vlan) {
	char errbuf[PCAP_ERRBUF_SIZE];
  ARP_PKT_T arpPkt;
  ARP_PKT_HDR_T *pRespPktArp = NULL;
  pcap_t *fpPcap = NULL;
  struct pcap_pkthdr *pPktHdr = NULL;
  int rc;
  struct timeval tvStart, tvNow;
  int waitMs = 3000;
  unsigned short *pus = NULL;
  unsigned char *packet = NULL;
  int haveResponse = 0;
#ifndef WIN32
  struct pcap_pkthdr pktHdr;
  pPktHdr = &pktHdr;
#endif // WIN32


  if(createArpPkt(&arpPkt, ipv4, vlan) != 0) {
    return -1;
  }

  LOG(X_DEBUG("Looking up mac for %s vlan: %u"), inet_ntoa(*((struct in_addr *) &ipv4)), vlan);

  if ((fpPcap = pcap_open_live(pktgen_GetInterface(),		// name of the device
							                  65535,			// portion of the packet to capture. It doesn't matter in this case 
							                  1,				// promiscuous mode (nonzero means promiscuous)
							                  1000,			// read timeout
							                  errbuf			
							                  )) == NULL) {
	  LOG(X_ERROR("Unable to open the adapter. '%s' is not supported by WinPcap\n"), pktgen_GetInterface());
	  return -1;
  }

#ifdef WIN32
  if (pcap_sendpacket(fpPcap,	(unsigned char *) &arpPkt.data, arpPkt.len) != 0) {
#else // WIN32
  if(pktgen_SendPacket(pktgen_GetInterface(), (unsigned char *) &arpPkt.data, arpPkt.len) != 0) {
#endif // WIN32

		LOG(X_ERROR("Error sending the packet on %s"), pktgen_GetInterface());
    pcap_close(fpPcap);	
		return -1;
	}

  gettimeofday(&tvStart, NULL);

#ifdef WIN32
  while((rc = pcap_next_ex(fpPcap, &pPktHdr, &packet)) >= 0) {
#else // WIN32
    while((packet = (unsigned char *) pcap_next(fpPcap, &pktHdr))) {
      rc = 1;
#endif // WIN32

    if(rc > 0 && 
       pPktHdr->len >= (14 + ARP_PKT_HDR_SZ) && 
       pPktHdr->caplen >= (14 + ARP_PKT_HDR_SZ) &&
       !memcmp(packet, arpPkt.pEth->ether_shost, ETH_ALEN)) {
    
      pus = (unsigned short *) &((struct ether_header *) packet)->ether_type;

      if(ntohs(*pus) == ETHERTYPE_VLAN) {
        pus = (unsigned short *) (((unsigned char *) pus) + 4);
      } 

      pRespPktArp = (ARP_PKT_HDR_T *) (((unsigned char *) pus) + 2);

      if(ntohs(*pus) == ETHERTYPE_ARP &&
         pRespPktArp->opt == htons(ARPOP_REPLY) &&
         !memcmp(pRespPktArp->targetMac, arpPkt.pHdr->senderMac, ETH_ALEN) &&
         !memcmp(pRespPktArp->targetIpv4, arpPkt.pHdr->senderIpv4, IPV4_ADDR_LEN)) {

        memcpy(mac, pRespPktArp->senderMac, ETH_ALEN);
        haveResponse = 1;

        if(vlan == 0 || 
          (htons(((struct ether_header *) packet)->ether_type) == ETHERTYPE_VLAN &&
          (htons(vlan & 0x0fff)) == 
          htons(ntohs(*((unsigned short *)(((unsigned char *) packet) + 14))) & 0x0fff)   )) {
          //
          // Only break out at this point if the response is on the same VLAN as the request
          //
          break;
        } 
      }
    }

    gettimeofday(&tvNow, NULL);
    if( ((tvNow.tv_sec - tvStart.tv_sec) * 1000) + ((tvNow.tv_usec - tvStart.tv_usec) / 1000) > waitMs) {

      if(haveResponse == 1) {
        LOG(X_WARNING("Mac response received on incorrect VLAN"));
      } else {
        LOG(X_ERROR("Mac query for %s timed out"), inet_ntoa(*((struct in_addr *)&arpPkt.pHdr->targetIpv4)));
        haveResponse = -1;
      }
      break;
    }

  }

  pcap_close(fpPcap);	

  if(haveResponse == 1) {

    LOG(X_DEBUG("%s is at %02x:%02x:%02x:%02x:%02x:%02x"), 
      inet_ntoa(*((struct in_addr *) &ipv4)),
      mac[0], mac[1], mac[2], mac[3], mac[4], mac[5], mac[6]);

    return 0;

  } else if(haveResponse == 0) {
    LOG(X_ERROR("Unable to retrieve MAC for %s"), inet_ntoa(*((struct in_addr *)&ipv4)));
  }

  return -1;
}



int arpmac_GetMac(long ipv4, unsigned char mac[], unsigned short vlan) {

  MAC_ENTRY_T *pMacEntry = NULL;
  MAC_ENTRY_T macEntry;
  //void *pEntry = NULL;
  struct timeval tvNow;
  int rc;

  if(mac == NULL) {
    return -1;
  }

  gettimeofday(&tvNow, NULL);

  //
  // Try to lookup the mac in the hash table
  //
  pthread_mutex_lock(&g_mtxMacs);

  if((pMacEntry = findmac(ipv4))) {
    memcpy(&macEntry, pMacEntry, sizeof(MAC_ENTRY_T));
  }
 
 //if((rc = tchash_find(g_macsHash, &ipv4, sizeof(IPV4_ADDR_LEN), &pEntry)) == 0) {
   //memcpy(&macEntry, pEntry, sizeof(MAC_ENTRY_T));
   //memcpy(mac, ((MAC_ENTRY_T *)pEntry)->mac, ETH_ALEN);
 //}

 pthread_mutex_unlock(&g_mtxMacs);

  if(!pMacEntry ||
     macEntry.tmDiscovered + (long) g_macExpirationSec < tvNow.tv_sec) {

    //
    // Arp for the mac
    //
    if((rc = getMacByArp(ipv4, macEntry.mac, vlan)) != 0) {

      // 
      // If the arp failed and the mac previously existed, delete it from the hash table
      //
      if(pMacEntry) {
        pthread_mutex_lock(&g_mtxMacs);
        clearmac(pMacEntry);
        pthread_mutex_unlock(&g_mtxMacs);
      }
    
      return -1;
    }


    //
    // Update the hash table
    // We search the hashtable for the key again because another thread
    // may have deleted the key while we were in getMacByArp above
    //
    pthread_mutex_lock(&g_mtxMacs);

    if((pMacEntry = findmac(ipv4)) == NULL) {
    //if(tchash_find(g_macsHash, &ipv4, sizeof(IPV4_ADDR_LEN), &pMacEntry) != 0) {

      //if((pMacEntry = (MAC_ENTRY_T *) calloc(1, sizeof(MAC_ENTRY_T))) == NULL) {
      //  LOG(X_ERROR("Failed to allocate %d bytes"), sizeof(MAC_ENTRY_T));
      //  pthread_mutex_unlock(&g_mtxMacs);
      //  return -1;
      //}
      macEntry.ipv4 = ipv4;
      //pMacEntry->ipv4 = ipv4;
    

      //if(1 != tchash_search(g_macsHash, &pMacEntry->ipv4, sizeof(IPV4_ADDR_LEN), pMacEntry, &pEntry)) {
      if((pMacEntry = storemac(&macEntry)) == NULL) {
        pthread_mutex_unlock(&g_mtxMacs);
        LOG(X_ERROR("failed to add mac entry"));
        return -1;
      }

    } 

    pMacEntry->tmDiscovered = tvNow.tv_sec;

    pthread_mutex_unlock(&g_mtxMacs);
  }

  memcpy(mac, pMacEntry->mac, ETH_ALEN);

  return 0;
}

#endif // DISABLE_PCAP

int arpmac_Init(unsigned int expirationSec) {

  if(!g_mtxInit && pthread_mutex_init(&g_mtxMacs, NULL) != 0) {
    LOG(X_CRITICAL("pthread_mutex_init failed"));
    return -1;
  }
  g_mtxInit = 1;

  //if((g_macsHash = tchash_new(32, 0, TCHASH_NOCOPY)) == NULL) {
  //  LOG(X_CRITICAL("tchash_new failed for g_macsHash"));
  //  return -1;
  //}

  g_macExpirationSec = expirationSec;

  return 0;
}
