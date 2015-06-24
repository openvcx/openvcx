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
#include "pktcapture.h"

#if defined(VSX_HAVE_CAPTURE)

static void captureFreePcap(PCAP_CAPTURE_PROPERTIES_T *pCapture) {
  CAPTURE_STATE_T *pState;

  if(pCapture) {

    if(capture_IsRunning(pCapture)) {
      capture_Stop(pCapture);
      usleep(20000);
    }

    if((pState = (CAPTURE_STATE_T *) pCapture->userCbData)) {
      rtp_captureFree(pState);
    }
    free(pCapture);
  }

}

static PCAP_CAPTURE_PROPERTIES_T *captureCreatePcap(unsigned int numStreams,
                                               unsigned int jtBufSzPkts,
                                               unsigned int jtBufPktBufSz,
                                               unsigned int cfgMaxRtpGapWaitTmMs) {

  CAPTURE_STATE_T *pState = NULL;
  PCAP_CAPTURE_PROPERTIES_T *pCapture;

  if((pState = rtp_captureCreate(numStreams, jtBufSzPkts, jtBufSzPkts,
                                 jtBufPktBufSz, cfgMaxRtpGapWaitTmMs, cfgMaxRtpGapWaitTmMs)) == NULL) {
    return NULL;
  }

  if(!(pCapture = (PCAP_CAPTURE_PROPERTIES_T *) calloc(1, sizeof(PCAP_CAPTURE_PROPERTIES_T)))) {
    free(pCapture);
    return NULL;
  }
  
  pCapture->userCbData = pState;
  pCapture->pktHandler = capture_onRawPkt;

  return pCapture;
}

static void capturePrintPcap(PCAP_CAPTURE_PROPERTIES_T *pCapture, FILE *fp) {
  struct pcap_stat *pCapStats = NULL;

  pCapStats = capture_GetStats(pCapture);

  fprintf(fp, "\n(pcap rx:%u drop:%u)", 
          pCapStats->ps_recv, pCapStats->ps_drop);

  rtp_capturePrint((CAPTURE_STATE_T *) pCapture->userCbData, fp);

}

int cap_pcapStart(const char *iface, const char *outDir, int isFile, 
                      const CAPTURE_FILTER_T filters[], unsigned int numFilters,
                      int overwriteOut, int promiscuous) {

  PCAP_CAPTURE_PROPERTIES_T *pCapture;
  CAPTURE_STATE_T *pState;
  CAPTURE_CBDATA_T streamCbData;
  unsigned int jtBufSzInPkts = CAPTURE_RTP_JTBUF_SZ_PKTS;

  capture_initCbData(&streamCbData, outDir, overwriteOut);

  if(numFilters > 0) {
    if(filters[0].transType == CAPTURE_FILTER_TRANSPORT_UDPRAW) {
      jtBufSzInPkts = 0;
    }
  }

  if((pCapture = captureCreatePcap(CAPTURE_DB_MAX_STREAMS_PCAP, 
                                   jtBufSzInPkts, 
                                   RTP_JTBUF_PKT_BUFSZ_DEFAULT,
                                   1000)) == NULL) {
                                   //pCfg->pcommon->cfgMaxRtpGapWaitTmMs)) == NULL) {
    capture_deleteCbData(&streamCbData);
    return -1;
  }

  if(numFilters > 0) {
    pState = (CAPTURE_STATE_T *) pCapture->userCbData;
    pState->pCbUserData = &streamCbData;
    pState->cbOnStreamAdd = capture_cbOnStreamAdd;

    memcpy(&pState->filt.filters[0], &filters[0], sizeof(CAPTURE_FILTER_T));
    pState->filt.numFilters = numFilters;

    if(filters[0].transType == CAPTURE_FILTER_TRANSPORT_TCP) {
 
      //TODO: seems that libpcap layer 4 filters may not be correct for 
      // some localhost ipv6 on linux

      capture_SetFilter(pCapture, "tcp");
    } else {
      capture_SetFilter(pCapture, "udp");
    }

  } else {
    capture_SetFilter(pCapture, "udp");
  }

  if(isFile) {
    capture_SetInputfile(pCapture, iface);
  } else {
    pCapture->promiscuous = promiscuous;
    capture_SetInterface(pCapture, iface);
  }
  //TODO: check filter trans filter for tcp/udp


  if(capture_Start(pCapture) != 0) {
    LOG(X_ERROR("Unable to initialize packet capture."));
    captureFreePcap(pCapture);
    capture_deleteCbData(&streamCbData);
    return -1;
  }

  while(capture_IsRunning(pCapture)) {
    if(isFile) {
      usleep(10000);
    } else {
      usleep(5000000);
      capturePrintPcap(pCapture, stderr);
    }
  }

  rtp_captureFlush(((CAPTURE_STATE_T *)pCapture->userCbData));

  if(numFilters == 0) {
    capturePrintPcap(pCapture, stderr);
  }

  captureFreePcap(pCapture);
  capture_deleteCbData(&streamCbData);

  return 0;

//#endif // DISABLE_PCAP

}



/*
int testpcap(const char *path) {
  FILE_STREAM_T fStreamOut;
  const char *pathOut = "C:\\csco\\ctms\\tmp\\testfromcap.h264";
//char buf[1024];
  PCAP_CAPTURE_PROPERTIES_T *pCapture;


  if((fStreamOut.fp = fileops_Open(pathOut, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_CRITICAL("Unable to open file for writing: %s"), pathOut);
    return -1;
  }
  fStreamOut.offset = 0;

  pCapture = rtp_captureCreate(8, 32, RTP_JTBUF_PKT_BUFSZ_DEFAULT, write_h264net);

  capture_SetInputfile(pCapture, path);
  capture_SetFilter(pCapture, "udp");

  ((CAPTURE_STATE_T *)pCapture->userCbData)->filt.filters[0].ssrcFilter = 0x726de995;
  ((CAPTURE_STATE_T *)pCapture->userCbData)->filt.filters[0].dstIp = inet_addr("172.28.176.162");
  //((CAPTURE_STATE_T *)pCapture->userCbData)->filt.filters[0].dstPort = htons(16406);
  ((CAPTURE_STATE_T *)pCapture->userCbData)->filt.filters[0].payloadType = 112;
  ((CAPTURE_STATE_T *)pCapture->userCbData)->filt.filters[0].havePayloadTypeFilter = 1;
  ((CAPTURE_STATE_T *)pCapture->userCbData)->filt.filters[0].pCbUserData = &fStreamOut;
  ((CAPTURE_STATE_T *)pCapture->userCbData)->numFilters = 1;




  if(capture_Start(pCapture) != 0) {
    LOG(X_ERROR("Unable to initialize packet capture."));
    rtp_captureFree(pCapture);
    CloseMediaFile(&fStreamOut);
    return -1;
  }

  while(capture_IsRunning(pCapture)) {
    sleep(1);
  }

  rtp_captureFlush(((CAPTURE_STATE_T *)pCapture->userCbData));

  rtp_capturePrint(((CAPTURE_STATE_T *)pCapture->userCbData), stderr);

  rtp_captureFree(pCapture);
  //pkt_ListInterfaces(stdout);

  CloseMediaFile(&fStreamOut);
  return 0;
}
*/

#endif // VSX_HAVE_CAPTURE
