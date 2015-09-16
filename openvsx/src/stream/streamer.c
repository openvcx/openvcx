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

//
// TODO: this legacy code needs to be replaced by streamer2.c in any invocations
//

#if defined(VSX_HAVE_STREAMER)

static int sendRtcpSr(STREAM_XMIT_NODE_T *pStream, unsigned int idxDest) {
  int rc = 0;
  //char tmp[128];
  unsigned char buf[256];
  int len = 0;

  if(!pStream->pXmitAction->do_output_rtphdr) {
    return 0;
  }

  if((len = stream_rtcp_createsr(pStream->pRtpMulti, idxDest, buf, sizeof(buf))) < 0) {
    LOG(X_ERROR("Unable to create rtcp sr for destination[%d]"), idxDest);
    return -1;
  }

  //fprintf(stderr, "sendto rtcp sr [dest:%d] %s:%d %d pkt:%u,%u ts:%u\n", idxDest, inet_ntoa(pStream->saDstsRtcp[idxDest].sin_addr), ntohs(pStream->saDstsRtcp[idxDest].sin_port), len, pStream->pRtpMulti[idxDest].rtcp.sr.pktcnt,pStream->pRtpMulti[idxDest].rtcp.sr.octetcnt,htonl(pStream->pRtpMulti[idxDest].rtcp.sr.rtpts));

  if((rc = netio_sendto(STREAM_RTCP_PNETIOSOCK(pStream->pRtpMulti->pdests[idxDest]), 
           (struct sockaddr *) &pStream->pRtpMulti->pdests[idxDest].saDstsRtcp, buf, len, "(rtcp)")) != len) {
    return -1;
  }


  return rc;
}

static int sendPktUdpRtp(STREAM_XMIT_NODE_T *pStream, unsigned int idxDest,
                      const unsigned char *pData, unsigned int len) {
  int rc = 0;
  //char tmp[128];

  if(!pStream->pXmitAction->do_output_rtphdr) {
    if(len >= RTP_HEADER_LEN) {
      pData += RTP_HEADER_LEN;
      len -= RTP_HEADER_LEN;
    } else {
      len = 0;
    }
  }

  if(len == 0) {
    LOG(X_WARNING("sendto called with 0 length"));
    return 0;
  }

  pStream->pRtpMulti->pdests[idxDest].rtcp.sr.rtpts = pStream->pRtpMulti->pRtp->timeStamp;
  pStream->pRtpMulti->pdests[idxDest].rtcp.pktcnt++;
  pStream->pRtpMulti->pdests[idxDest].rtcp.octetcnt += len ;

  //fprintf(stderr, "sendto [dest:%d] %s:%d %d bytes seq:%u ts:%u ssrc:0x%x\n", idxDest, inet_ntoa(pStream->saDsts[idxDest].sin_addr), ntohs(pStream->saDsts[idxDest].sin_port), len, htons(pStream->pRtpMulti[idxDest].m_pRtp->sequence_num), htonl(pStream->pRtpMulti[idxDest].m_pRtp->timeStamp), pStream->pRtpMulti[idxDest].m_pRtp->ssrc);

  if((rc = netio_sendto(STREAM_RTP_PNETIOSOCK(pStream->pRtpMulti->pdests[idxDest]),
                  (struct sockaddr *) &pStream->pRtpMulti->pdests[idxDest].saDsts,
                  pData, len, NULL)) != len) {
    return -1;
  }


  return rc;
}

/*
int stream_tofile(STREAM_XMIT_NODE_T *pStream, const char *path) {
  FILE_STREAM_T fileOut;
  uint64_t bytesWritten = 0;
  int rc;

  if(pStream->pNext) {
    LOG(X_ERROR("Will not stream multiples streams to one file"));
    return -1;
  }

  memset(&fileOut, 0, sizeof(fileOut));
  if((fileOut.fp = fileops_Open(path, O_RDONLY)) != FILEOPS_INVALID_FP) {
    fileops_Close(fileOut.fp);
    LOG(X_ERROR("File %s already exists.  Will not overwrite."), path);
    return -1;
  } else if((fileOut.fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open file for writing: %s"), path);
    return -1;
  }
  LOG(X_DEBUG("Writing to local file: %s"), path);

  // TODO: disable loop setting

  do {

    // avoid calling (pStream->cbCanSend(pStream->pCbData)) 
     //pStream->pRtpMulti->m_pktsSent++;
     //if(pStream->pRtpMulti->m_nextFrame > pStream->pRtpMulti->m_framesSent) {
     //  pStream->pRtpMulti->m_framesSent++;
     //}
    if(pStream->cbCanSend(pStream->pCbData) < 0) {
      // TODO: check error condition vs eof
      fprintf(stderr, "no more pkts...\n");
      break;
    }

    //fprintf(stderr, "payload len:%d\n", pStream->pRtpMulti->m_payloadLen);

    if(pStream->pRtpMulti->payloadLen > 0) {
      if((rc = WriteFileStream(&fileOut, pStream->pRtpMulti->pPayload, 
            pStream->pRtpMulti->payloadLen)) != pStream->pRtpMulti->payloadLen) {
        if(rc > 0) {
          bytesWritten += rc;
        }
        return -1;
      }

      bytesWritten += rc;
      pStream->pRtpMulti->payloadLen = 0;
    }
 
    if(pStream->cbPreparePkt(pStream->pCbData) < 0) {
      // TODO: check error condition vs eof
      fprintf(stderr, "no more pkts...\n");
      break;
    }

  } while(1);

  LOG(X_DEBUG("Wrote %llu bytes to: %s"), bytesWritten, path);

  return 0;
}
*/

int stream_udp(STREAM_XMIT_NODE_T *pList, double durationSec) {
  STREAM_XMIT_NODE_T *pStream;
  COLLECT_STREAM_PKTDATA_T collectPkt;
  int triedXmit;
  unsigned int idxDest;
  TIME_VAL tmstart, tm1, tmnow;
  TIME_VAL tmprevbwdescr;
  int rc;
  int sz;
  int szPkt;
  unsigned int szData;
  const unsigned char *pData;
  unsigned int szDataPayload;
  const unsigned char *pDataPayload;
  uint64_t totBytes = 0;
  unsigned int totPkts = 0;
  unsigned int bytes = 0;
  unsigned int pkts = 0;
  unsigned int bytes2 = 0;
  unsigned int pkts2 = 0;
  //char dstStr[64];
#ifndef WIN32
  unsigned int countIterations = 0;
#endif // WIN32

  if(!pList 
#if defined(VSX_HAVE_LICENSE)
   || !pList->pLic
#endif // VSX_HAVE_LICENSE
     ) {
    return -1;
  }

  //snprintf(dstStr, sizeof(dstStr), "%s:%d", inet_ntoa(pList->pRtpMulti->pdests[0].saDsts.sin_addr), 
  //            ntohs(pList->pRtpMulti->pdests[0].saDsts.sin_port));

  tmstart = tmnow = timer_GetTime();
  tm1 = tmstart;
  tmprevbwdescr = tmstart;


  while(*pList->prunning == STREAMER_STATE_RUNNING && !g_proc_exit) {

    pStream = pList;
    triedXmit = 0;

    while(pStream) {

      if((rc = pStream->cbCanSend(pStream->pCbData)) > 0) {

        pData = stream_rtp_data(pStream->pRtpMulti);
        szData = stream_rtp_datalen(pStream->pRtpMulti);

        if(szData >= RTP_HEADER_LEN) {
          pDataPayload = pData + RTP_HEADER_LEN;
          szDataPayload = szData - RTP_HEADER_LEN;
        } else {
          pDataPayload = NULL;
          szDataPayload = 0;
        }

        if(pStream->pXmitAction->do_stream) {
          triedXmit = 1;

          totPkts++;
          pkts++;
          pkts2++;

          if(pStream->pXmitAction->do_output) {

            szPkt = 0;
            for(idxDest = 0; idxDest < pStream->pRtpMulti->numDests; idxDest++) {
  
              if(pStream->pXmitDestRc[idxDest] != 0) {
                continue;
              }
              if(pStream->rawSend) {
                if(pktgen_Queue(pData, szData) != 0) {
                  pStream->pXmitDestRc[idxDest] = -1; 
                  sz = -1;
                } else {
                  sz = szData;
                }
              } else {

                //
                // Check and send an rtcp sender report
                //
                if(*pStream->pfrtcp_sr_intervalsec > 0 &&
                  (tmnow - pStream->pRtpMulti->pdests[idxDest].tmLastRtcpSr) / TIME_VAL_MS  > 
                         *pStream->pfrtcp_sr_intervalsec * 1000) {
                  sendRtcpSr(pStream, idxDest);
                  pStream->pRtpMulti->pdests[idxDest].tmLastRtcpSr = tmnow;
                } 

                if((sz = sendPktUdpRtp(pStream, idxDest, pData, szData)) < 0) {
                  pStream->pXmitDestRc[idxDest] = sz; 
                }
              }
              if(szPkt == 0 && sz > 0) {
                szPkt = sz; 
              }
            } // end of for

            // Exit if there are no good transmitters in the list
            if(pStream->pXmitAction->do_output && szPkt == 0) {
              return -1;
            }

          } else {    // if do_output
            szPkt = szData;
          }

          if(!pStream->pXmitAction->do_output_rtphdr && szPkt > RTP_HEADER_LEN) {
            szPkt -= RTP_HEADER_LEN;
          }

          totBytes += szPkt;
          bytes += szPkt;

          //
          // Add the packet data to any outbound avclive subscribers
          //
          // TODO: do not hardcode szData > RTP_HEADER_LEN rtp hdr len
          if(pStream->pLiveQ && pStream->pLiveQ->numActive > 0 && szDataPayload > 0) {
            pthread_mutex_lock(&pStream->pLiveQ->mtx);
            for(sz = 0; (unsigned int) sz < pStream->pLiveQ->max; sz++) {
              if(pStream->pLiveQ->pQs[sz]) {
                pktqueue_addpkt(pStream->pLiveQ->pQs[sz], pDataPayload, szDataPayload, NULL, 0);
              }
            }
            pthread_mutex_unlock(&pStream->pLiveQ->mtx);
          }
          bytes2 += szPkt;

        } else {

          // preserve rtp sequence number during 'live pause'
          //pStream->pRtp->m_pRtp->sequence_num = 
          //  htons(htons(pStream->pRtp->m_pRtp->sequence_num) - 1);

          //During 'live pause', update the last seq #
          //if(pStream->pXmitAction->prior_do_stream && pStream->prtp_sequence_at_end) {
          //  *pStream->prtp_sequence_at_end = pStream->pRtp->m_pRtp->sequence_num;
          //}
          //fprintf(stderr, "not streaming\n");

        }

        //
        // Record output stream
        //
        if(pStream->pXmitAction->do_record_post && pStream->pXmitCbs->cbRecord && 
           pStream->pXmitCbs->pCbRecordData) {

          memset(&collectPkt, 0, sizeof(collectPkt));
          collectPkt.payload.pData = (unsigned char *) pDataPayload;
          PKTCAPLEN(collectPkt.payload) = szDataPayload;
          if((sz = pStream->pXmitCbs->cbRecord(pStream->pXmitCbs->pCbRecordData, &collectPkt)) < 0) {
            return -1;
          }
          if(triedXmit == 0) {
            triedXmit = 1;
          }
        }

        //
        // Call post processing function, such as http live streaming
        // callback to segment and package output ts files 
        //
        if(pStream->pXmitAction->do_httplive && pStream->pXmitCbs->cbPostProc &&
           pStream->pXmitCbs->pCbPostProcData && pStream->pXmitAction->do_stream) {

          if((sz = pStream->pXmitCbs->cbPostProc(pStream->pXmitCbs->pCbPostProcData, 
                                                pDataPayload, szDataPayload)) < 0) {
            return -1;
          }
          if(triedXmit == 0) {
            triedXmit = 1;
          }
        }


        //if(pStream->pXmitAction->do_stream != pStream->pXmitAction->prior_do_stream) {
        //  pStream->pXmitAction->prior_do_stream = pStream->pXmitAction->do_stream;
        //}


        pStream->pRtpMulti->payloadLen = 0;

        if((rc = pStream->cbPreparePkt(pStream->pCbData)) < 0) {
          return -1;
        }
        //fprintf(stderr, "streamer prepare pkt returned %d\n", rc);

      } else if(rc < 0) {
        LOG(X_DEBUG("Stream ending, sent: %"LL64"u bytes %u pkts"), totBytes, totPkts); 
        return -1;
      } else {
        //fprintf(stderr, "streamer cansend rc:%d\n", rc);
      } 

      pStream = pStream->pNext;
    } // while(pStream)

    pktgen_SendQueued();


    if(triedXmit == 0) {

#ifdef WIN32

      //sl1 = timer_GetTime();

      //Sleep(1) may sleep past its short bedtime on win32, even from a thread w/ SCHED_RR
      //However, sleep(0) uses alot more cpu slices
      //TODO: make this a WaitForSingleObject for any registered pktqueue writers
      

      if(pList->pSleepQ) {
        //TODO: this does not return if a pkt has been queued and no subsequent pkt arrives
        pktqueue_waitforunreaddata(pList->pSleepQ);
        //pthread_cond_wait(pList->pCond, pList->pMtxCond);
      } else {
        Sleep(1);
      }

    } else {
      // On windows Sleep 0 relinquishes execution for any waiting threads
      Sleep(0);

#else // WIN32

      VSX_DEBUG2(tmnow = timer_GetTime())

      usleep(1000);
      countIterations = 0;

      VSX_DEBUGLOG3("stream_udp slept for %lld ns\n", timer_GetTime() - tmnow);

    } else {

      if(countIterations++ > 10000) {
        // During continuous xmit, sleep to prevent unresponsive system
        usleep(1);
        countIterations = 0;
      } 
#endif // WIN32

    } 

    tmnow = timer_GetTime();

    if(pList->pBwDescr && (tmnow / TIME_VAL_US) > (tmprevbwdescr / TIME_VAL_US) + 1) {
      pList->pBwDescr->intervalMs = (float)(tmnow - tmprevbwdescr)/ TIME_VAL_MS;
      pList->pBwDescr->pkts = pkts2;
      pList->pBwDescr->bytes = bytes2;
      TV_FROM_TIMEVAL(pList->pBwDescr->updateTv, tmnow);
      //pList->pBwDescr->updateTv.tv_sec = tmnow / TIME_VAL_US;
      //pList->pBwDescr->updateTv.tv_usec = tmnow % TIME_VAL_US;
      bytes2 = 0;
      pkts2 = 0;
      tmprevbwdescr = tmnow;
    }

    if(durationSec > 0 && tmnow > tmstart + (durationSec * TIME_VAL_US)) {
      LOG(X_DEBUG("Stream duration %.1f sec limit reached"), durationSec);
      *pList->prunning = STREAMER_STATE_FINISHED;
    }

#if defined (VSX_HAVE_LICENSE)
    // Check if stream time is limited
    if(!(pList->pLic->capabilities & LIC_CAP_STREAM_TIME_UNLIMITED)) {
      if(tmnow > tmstart + (STREAM_LIMIT_SEC * TIME_VAL_US)) {
        LOG(X_INFO("Stream time limited.  Stopping stream transmission after %d sec"), 
                 (int) (tmnow - tmstart) / TIME_VAL_US);
        *pList->prunning = STREAMER_STATE_FINISHED;
        if(!(g_proc_exit_flags & PROC_EXIT_FLAG_NO_EXIT_ON_STREAM_TIME_LIMITED)) {
          g_proc_exit = 1;
        }
      }
    }
#endif // VSX_HAVE_LICENSE

#if defined(LITE_VERSION)
    if(tmnow > tmstart + (STREAM_LIMIT_LITE_SEC * TIME_VAL_US)) {
      LOG(X_INFO("Stream time limited.  Stopping stream transmission after %d sec"),
               (int) (tmnow - tmstart) / TIME_VAL_US);
      *pList->prunning = STREAMER_STATE_FINISHED;
      if(!(g_proc_exit_flags & PROC_EXIT_FLAG_NO_EXIT_ON_STREAM_TIME_LIMITED)) {
        g_proc_exit = 1;
      }
    }
#endif // (LITE_VERSION)

/*
    if(0 && pList->verbosity > 1 && tv2.tv_sec > tv1.tv_sec+3) {

      elapsedMs0 = ((tv2.tv_sec - tv0.tv_sec) * 1000) + 
                   ((tv2.tv_usec - tv0.tv_usec) /1000);
      elapsedMs1 = ((tv2.tv_sec - tv1.tv_sec) * 1000) + 
                   ((tv2.tv_usec - tv1.tv_usec) /1000);

      fprintf(stdout, "%u", elapsedMs0/1000);
         
      if(durationSec != 0) {
        fprintf(stdout, "/%.1f", durationSec);
      }

      fprintf(stdout, " sec, %s %.1fKb/s %.1fpkts/s (total: %u pkts, %.1fKB, %.1fKb/s)",
           dstStr,
           (double)(bytes  / 128.0f / ((double)elapsedMs1/1000.0f)),
           (double)(pkts/ ((double)elapsedMs1/1000.0f)),
                         totPkts, (double)totBytes/1024.0f,
                           (double)(totBytes /  128.0f / ((double)elapsedMs0/1000.0f))); 
      fprintf(stdout, "\n");

      bytes = 0;
      pkts = 0;
      tv1.tv_sec = tv2.tv_sec;
      tv1.tv_usec = tv2.tv_usec;
    }
 */  
  }

  return 0;
}

#endif // VSX_HAVE_STREAMER
