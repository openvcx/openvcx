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


#ifndef __PACKET_CAPTURE_H__
#define __PACKET_CAPTURE_H__

#include "pktcommon.h"
#include "pthread_compat.h"

typedef void (*CAPTURE_PKT_HANDLER) (unsigned char *pArg, 
                                     const struct pcap_pkthdr *pkthdr, 
                                     const unsigned char *packet) ;
typedef struct PCAP_CAPTURE_PROPERTIES {
  CAPTURE_PKT_HANDLER pktHandler;
  const char *iface;
  int promiscuous;
  void *userCbData;

  void *pcap_fp;
  char device[256];
  char filter[256];
  int open_dev;
  int open_dev_rc; // should be atomic_t
  int is_running; // should be atomic_t
  pthread_t capture_thread;
  pthread_attr_t capture_attr;
#ifndef DISABLE_PCAP
  unsigned char bpfProg[32];
#endif // DISABLE_PCAP
} PCAP_CAPTURE_PROPERTIES_T;


#define capture_ListInterfaces      pkt_ListInterfaces


int capture_Start(PCAP_CAPTURE_PROPERTIES_T *pCapProps);
int capture_Stop(PCAP_CAPTURE_PROPERTIES_T *p);
int capture_SetInterface(PCAP_CAPTURE_PROPERTIES_T *p, const char *iface);
int capture_SetInputfile(PCAP_CAPTURE_PROPERTIES_T *p, const char *file);
int capture_SetFilter(PCAP_CAPTURE_PROPERTIES_T *p, const char *filterStr);
struct pcap_stat *capture_GetStats(PCAP_CAPTURE_PROPERTIES_T *p);
const char *capture_GetInterface(PCAP_CAPTURE_PROPERTIES_T *p);
void *capture_GetPcap(PCAP_CAPTURE_PROPERTIES_T *p);
int capture_IsRunning(PCAP_CAPTURE_PROPERTIES_T *p);



#endif // __PACKET_CAPTURE_H__
