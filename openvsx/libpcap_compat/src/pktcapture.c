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

#include <stdio.h>
#include "unixcompat.h"
#include "pthread_compat.h"

#if !defined(WIN32) || defined(__MINGW32__)

#include <unistd.h>
#include <string.h>
#include <sys/types.h>

#endif // WIN32

#include "pcap_compat.h"
#include "logutil.h"
#include "pktcapture.h"
#include "pktcommon.h"


#ifdef DISABLE_PCAP

int capture_Start(PCAP_CAPTURE_PROPERTIES_T *p) {
  return -1;
}
int capture_Stop(PCAP_CAPTURE_PROPERTIES_T *p) {
  return -1;
}
int capture_SetInterface(PCAP_CAPTURE_PROPERTIES_T *p, const char *iface) {

  return -1;
}
int capture_SetInputfile(PCAP_CAPTURE_PROPERTIES_T *p, const char *file) {
  return -1;
}
int capture_SetFilter(PCAP_CAPTURE_PROPERTIES_T *p, const char *filterStr) {
  return -1;
}
struct pcap_stat *capture_GetStats(PCAP_CAPTURE_PROPERTIES_T *p) {
  return NULL;
}
const char *capture_GetInterface(PCAP_CAPTURE_PROPERTIES_T *p) {
  return "";
}
void *capture_GetPcap(PCAP_CAPTURE_PROPERTIES_T *p) {
  return NULL;
}
int capture_IsRunning(PCAP_CAPTURE_PROPERTIES_T *p) {
  return 0;
}


#else // DISABLE_PCAP

//static PCAP_CAPTURE_PROPERTIES_T g_pcapProps;

static int capture_close(PCAP_CAPTURE_PROPERTIES_T *p) {

  if(p->pcap_fp) {
    pcap_close((pcap_t *) p->pcap_fp);
    p->pcap_fp = NULL;
    return 0;
  }
  return -1;
}


static int capture_setfilter(PCAP_CAPTURE_PROPERTIES_T *p) {

  //
  // Set pcap filter
  //
  if(p->filter[0] != '\0') {

    if(sizeof(p->bpfProg) < sizeof(struct bpf_program)) {
      LOG(X_ERROR("Invalid capture structure system size"));
      return -1;
    }

    if(pcap_compile((pcap_t *) p->pcap_fp, 
                               (struct bpf_program *) p->bpfProg,
                               p->filter,
                               1,
                               0xffffffff) == -1) {

      LOG(X_ERROR("Unable to compile filter '%s' for '%s'"), p->filter, p->device);
      return -1;
    }

    if(pcap_setfilter((pcap_t *) p->pcap_fp, (struct bpf_program *) p->bpfProg) != 0) {
      LOG(X_ERROR("Unable to set filter '%s' for '%s'"), p->filter, p->device);
      return -1;
    }

  }
  return 0;
}

static int capture_init(PCAP_CAPTURE_PROPERTIES_T *p) {

  char errbuf[PCAP_ERRBUF_SIZE];

  if(p->open_dev) {

    if ((p->pcap_fp = pcap_open_live(p->device,	
                  0xffff, // size
                  p->promiscuous,
                  1000,	 // read timeout
                  errbuf // error buffer
        )) == NULL) {
      LOG(X_CRITICAL("Unable to open network interface. '%s'"), p->device);
      return -1;
    }

    if(capture_setfilter(p) != 0) {
      capture_close(p);
      return -1;
    }

    LOG(X_INFO("Listening on interface '%s' %s %s"), 
         p->device, pkt_FindDevDescr(p->device), (p->promiscuous ? "(promiscuous)" : ""));

  } else {

    if((p->pcap_fp = pcap_open_offline(p->device, errbuf)) == NULL) {
      LOG(X_CRITICAL("Unable to open the file. '%s'"), p->device);		  
      return -1;
    }

    if(capture_setfilter(p) != 0) {
      capture_close(p);
      return -1;
    }

    LOG(X_INFO("Reading from capture file '%s'"), p->device);      

  }
  
#ifdef WIN32
  LOG(X_DEBUG("%s"), pcap_lib_version());
#else // WIN32
  LOG(X_DEBUG("pcap version: %u.%u"), pcap_major_version((pcap_t *) p->pcap_fp),
                                    pcap_minor_version((pcap_t *) p->pcap_fp));
#endif // WIN32

	return 0;
}



static void *captureProc(void *arg) {

  PCAP_CAPTURE_PROPERTIES_T *p = (PCAP_CAPTURE_PROPERTIES_T *) arg;
  struct pcap_pkthdr *pPkthdr = NULL;
  unsigned char *packet = NULL;
  unsigned int pktCnt = 0u;
  struct timeval tv0;
  long int msOffset = 0;
#ifndef WIN32
  struct pcap_pkthdr pktHdr;
  pPkthdr = &pktHdr;
#endif // WIN32

  LOG(X_DEBUG("Capture thread starting"));

  if(capture_init(p) != 0) {
    p->open_dev_rc = -1;
    p->is_running = 0;
    return NULL;
  }

  p->open_dev_rc = 1;

  if(p->open_dev) {

    pcap_loop((pcap_t *) p->pcap_fp, 0, p->pktHandler, p->userCbData);

  } else {

#ifdef WIN32
    while(pcap_next_ex((pcap_t *) p->pcap_fp, &pPkthdr, (const u_char **) &packet) >= 0) {
#else // WIN32

    while((packet = (unsigned char *) pcap_next((pcap_t *) p->pcap_fp, &pktHdr))) {
#endif // WIN32

      //
      // When reading from a file
      // Adjust the packet time to start at the current system time
      //
      if(pktCnt++ == 0) {
        gettimeofday(&tv0, NULL);
        msOffset = ((tv0.tv_sec - pPkthdr->ts.tv_sec) * 1000) +
                   ((tv0.tv_usec - pPkthdr->ts.tv_usec) / 1000);
      }

      pPkthdr->ts.tv_sec += (msOffset / 1000);
      pPkthdr->ts.tv_usec += ((msOffset % 1000) * 1000);
      if(pPkthdr->ts.tv_usec > 999999) {
        pPkthdr->ts.tv_sec ++;
        pPkthdr->ts.tv_usec -= 1000000;
      }

      p->pktHandler(p->userCbData, pPkthdr, packet);
    }

  }

  capture_close(p);
  LOG(X_DEBUG("Capture thread ending after reading %u packets"), pktCnt);
  p->is_running = 0;

  return NULL;
}


const char *capture_GetInterface(PCAP_CAPTURE_PROPERTIES_T *p) {
  return p->device;
}

int capture_SetInterface(PCAP_CAPTURE_PROPERTIES_T *p, const char *iface) {

  if(!iface || strlen(iface) > sizeof(p->device) -1) {
    return -1;
  }

  strcpy(p->device, iface);
  p->open_dev = 1;

  return 0;
}

int capture_SetFilter(PCAP_CAPTURE_PROPERTIES_T *p, const char *filterStr) {

  LOG(X_DEBUG("Setting pcap filter '%s'"), filterStr ? filterStr : "");

  if(filterStr == NULL) {
    p->filter[0] = '\0';
  } else if(strlen(filterStr) >= sizeof(p->filter)) {
    return -1;
  } else {
    strcpy(p->filter, filterStr);
  }

  return 0;
}

int capture_SetInputfile(PCAP_CAPTURE_PROPERTIES_T *p, const char *file) {

  if(capture_SetInterface(p, file) < 0) {
    return -1;
  }
  p->open_dev = 0;

  return 0;
}

struct pcap_stat *capture_GetStats(PCAP_CAPTURE_PROPERTIES_T *p) {
  static struct pcap_stat stat;

  // Check if we're reading from a file, instead of a live capture
  if(p->open_dev == 0) {
    stat.ps_recv = 0;
    stat.ps_drop = 0;
    return &stat;
  }

  if(p->pcap_fp == NULL) {
    return NULL;
  }

  if(pcap_stats((pcap_t *) p->pcap_fp, &stat) != 0) {
    return NULL;
  }

  return &stat;
}

int capture_Start(PCAP_CAPTURE_PROPERTIES_T *p) {
  struct sched_param param;

  if(p == NULL || p->pktHandler == NULL) {
    LOG(X_ERROR("Packet handler not defined"));
    return -1;
  }

  if(p->device[0] == '\0') {
    LOG(X_ERROR("No interface set"));
    return -1;
  }
  
  if(p->pktHandler == NULL) {
    LOG(X_ERROR("packet capture packet handler not set"));
    return -1;
  }

  // initialize capture thread
  pthread_attr_init(&p->capture_attr);
  pthread_attr_setdetachstate(&p->capture_attr, PTHREAD_CREATE_DETACHED);
  pthread_attr_setschedpolicy(&p->capture_attr, SCHED_RR);
  param.sched_priority = sched_get_priority_max(SCHED_RR);
  pthread_attr_setschedparam(&p->capture_attr, &param);

  //memcpy(&g_pcapProps.g_capProps, pCapPropsArg, sizeof(CAPTURE_PROPERTIES_T));
  p->open_dev_rc = 0;
  p->is_running = 1;

  if(pthread_create(&p->capture_thread, 
                    &p->capture_attr, 
                    captureProc, 
                    p) != 0) {
    LOG(X_ERROR("Cannot create capture thread"));
    return -1;
  }

  do {
    if(p->open_dev_rc < 0) {
      p->is_running = 0;
      return -1;
    } else if(p->open_dev_rc > 0) {
      return 0;
    }
    usleep(100000);
  } while(1);

  return 0;
}

int capture_Stop(PCAP_CAPTURE_PROPERTIES_T *p) {

#ifdef WIN32
  pcap_breakloop((pcap_t *) p->pcap_fp);
#endif // WIN32

  return 0;
}

void *capture_GetPcap(PCAP_CAPTURE_PROPERTIES_T *p) {
  return p->pcap_fp;
}    

int capture_IsRunning(PCAP_CAPTURE_PROPERTIES_T *p) {
  return p->is_running;
}

#endif // DISABLE_PCAP
