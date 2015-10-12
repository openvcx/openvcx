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

#if defined(VSX_HAVE_STREAMER)

//#define TEST_KILL 1
#if defined(TEST_KILL)
extern int g_proc_exit;
extern pthread_cond_t *g_proc_exit_cond;
#include <signal.h>
#endif // TEST_KILL

int streamer_run_tofile(STREAM_XMIT_NODE_T *pStreamIn, STREAM_XMIT_NODE_T *pStreamOut, const char *path) {
  FILE_STREAM_T fileOut;

  if(pStreamIn->pNext) {
    LOG(X_ERROR("Cannot not stream multiple  streams to one file"));
    return -1;
  }

  memset(&fileOut, 0, sizeof(fileOut));
  if(pStreamOut->poverwritefile && ! *pStreamOut->poverwritefile) {
    if((fileOut.fp = fileops_Open(path, O_RDONLY)) != FILEOPS_INVALID_FP) {
      fileops_Close(fileOut.fp);
      LOG(X_ERROR("File %s already exists.  Will not overwrite."), path);
      return -1;
    }
  }

  if((fileOut.fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open file for writing: %s"), path);
    return -1;
  }
  LOG(X_DEBUG("Writing to local file: %s"), path);

  // TODO: disable loop setting

  pStreamOut->pXmitCbs->pCbRecordData = &fileOut;

  while(*pStreamIn->prunning == STREAMER_STATE_RUNNING && !g_proc_exit) {

    if(pStreamIn->cbCanSend(pStreamIn->pCbData) < 0) {
      // TODO: check error condition vs eof
      //fprintf(stderr, "no more pkts...\n");
      break;
    }

    if(pStreamIn->cbPreparePkt(pStreamIn->pCbData) < 0) {
      // TODO: check error condition vs eof
      //fprintf(stderr, "no more pkts...\n");
      break;
    }

  }

  return 0;
}

typedef struct NET_PROGRESS {
  unsigned int numTotal;
  unsigned int numCompleted;
  unsigned int numError;
} NET_PROGRESS_T;

/*
#define DTLS_HANDSHAKE_RTP_TIMEOUT_MS            20000
#define DTLS_HANDSHAKE_RTCP_ADDITIONAL_MS         1000
#define DTLS_HANDSHAKE_RTCP_ADDITIONAL_GIVEUP_MS  5000
*/

#if defined(VSX_HAVE_SSL_DTLS)

static void setDtlsTimeouts(DTLS_TIMEOUT_CFG_T *pdtlsTimeouts, const STREAM_XMIT_NODE_T *pStream) {

  memset(pdtlsTimeouts, 0, sizeof(DTLS_TIMEOUT_CFG_T));

  if(pStream->pdtlsTimeouts) {
    memcpy(pdtlsTimeouts, pStream->pdtlsTimeouts, sizeof(DTLS_TIMEOUT_CFG_T));
  }

  if(pdtlsTimeouts->handshakeRtpTimeoutMs == 0) {
    pdtlsTimeouts->handshakeRtpTimeoutMs = DTLS_HANDSHAKE_RTP_TIMEOUT_MS;
  }

  if(pdtlsTimeouts->handshakeRtcpAdditionalMs == 0) {
    pdtlsTimeouts->handshakeRtcpAdditionalMs = DTLS_HANDSHAKE_RTCP_ADDITIONAL_MS;
  }

  if(pdtlsTimeouts->handshakeRtcpAdditionalGiveupMs == 0) {
    pdtlsTimeouts->handshakeRtcpAdditionalGiveupMs = MAX(pdtlsTimeouts->handshakeRtcpAdditionalMs, 
                                                        DTLS_HANDSHAKE_RTCP_ADDITIONAL_GIVEUP_MS);
  }
}


static void do_dtls_handshake(NETIO_SOCK_T *pnetsock, const struct sockaddr *psaDst,
                              NET_PROGRESS_T *pProgress) {

  char tmp[128];
  pProgress->numTotal++;

  //LOG(X_DEBUG("flags_dtls_server:%d"), pnetsock->flags & NETIO_FLAG_SSL_DTLS_SERVER);
  if(pnetsock->ssl.state != SSL_SOCK_STATE_ERROR &&
     pnetsock->ssl.state != SSL_SOCK_STATE_HANDSHAKE_COMPLETED &&
    !(pnetsock->flags & NETIO_FLAG_SSL_DTLS_SERVER)) {

    VSX_DEBUG_DTLS( LOG(X_DEBUG("DTLS - %u.%u Calling dtls_netsock_handshake %s:%d state:%d"), 
                          timer_GetTime()/TIME_VAL_US, timer_GetTime()%TIME_VAL_US, 
      FORMAT_NETADDR(*psaDst, tmp, sizeof(tmp)), ntohs(PINET_PORT(psaDst)), pnetsock->ssl.state) ); 
    dtls_netsock_handshake(pnetsock, psaDst);
  }

  if(pnetsock->ssl.state == SSL_SOCK_STATE_ERROR) {
    pProgress->numError++;
  } else if(pnetsock->ssl.state == SSL_SOCK_STATE_HANDSHAKE_COMPLETED) {

    //
    // If this is a DTLS-SRTP socket make sure that the output srtp key has been set (post handshake completion)
    // For a capture/streamer shared socket this may happen post capture-side handshake completion
    //
    if(!(pnetsock->flags & NETIO_FLAG_SRTP) ||
        (pnetsock->flags & NETIO_FLAG_SSL_DTLS_SRTP_OUTKEY)) {
      pProgress->numCompleted++;
    }
  }

}

static int do_dtls_handshakes(STREAM_XMIT_NODE_T *pStream, 
                              TIME_VAL tmstart,
                              TIME_VAL *ptmstartRtpHandshakeDone,
                              NET_PROGRESS_T *progRtp, 
                              NET_PROGRESS_T *progRtcp,
                              const DTLS_TIMEOUT_CFG_T *pdtlsTimeouts) {
  int idxDest, idxDestPrior;
  STREAM_RTP_DEST_T *pDest = NULL;
  STREAM_RTP_DEST_T *pDestPrior = NULL;
  STREAM_RTP_MULTI_T *pRtpMulti = NULL;
  STREAM_RTP_MULTI_T *pRtpMultiPrior = NULL;
  int sameRtpSock, sameRtcpSock;
  TIME_VAL tmnow;

  //LOG(X_DEBUG("do_dtls_handshakes"));
  if(progRtp) {
    memset(progRtp, 0, sizeof(NET_PROGRESS_T));
  }
  if(progRtcp) {
    memset(progRtcp, 0, sizeof(NET_PROGRESS_T));
  }

  pRtpMulti = pStream->pRtpMulti;
  while(pRtpMulti) {

    if(g_proc_exit) {
      return -1;
    } else if(*pStream->prunning != STREAMER_STATE_RUNNING) {
      continue; 
    }

    //
    // Check if the RTP socket is pending creation by the capture_socket  thread
    //
    if(progRtp && pRtpMulti->pStreamerCfg->sharedCtxt.state == STREAMER_SHARED_STATE_PENDING_CREATE) {
      progRtp->numTotal++;
      continue;
    }

    for(idxDest = 0; idxDest < pRtpMulti->numDests; idxDest++) {

      if(!(pDest = &pRtpMulti->pdests[idxDest]) || !pDest->isactive) {
        continue;
      }

      sameRtpSock = sameRtcpSock = 0;

      if(pRtpMultiPrior) {
        //
        // Check RTP video/audio multiplexing over the same port
        //
        for(idxDestPrior = 0; idxDestPrior < pRtpMultiPrior->numDests; idxDestPrior++) {

          if(!(pDestPrior = &pRtpMultiPrior->pdests[idxDestPrior]) || !pDestPrior->isactive) {
            continue;
          }

          if(INET_PORT(pDestPrior->saDsts) == INET_PORT(pDest->saDsts)) {
            sameRtpSock = 1;
          }
          if(INET_PORT(pDestPrior->saDstsRtcp) == INET_PORT(pDest->saDstsRtcp)) {
            sameRtcpSock = 1;
          }
        }
      }


      //if(pDest->pRtpMulti->pStreamerCfg->xcode.vid.pip.active) 
      //LOG(X_DEBUG("SAMERTPSOCK:%D, SAMERTCPSOCK:%d"), sameRtpSock, sameRtcpSock);

      if(progRtp && (STREAM_RTP_PNETIOSOCK(*pDest)->flags & NETIO_FLAG_SSL_DTLS) && !sameRtpSock &&
         STREAM_RTP_FD(*pDest) != INVALID_SOCKET) {
        
        VSX_DEBUG_DTLS( 
         LOG(X_DEBUG("DTLS - calling do_dtls_handshake RTP pnetsock.state: %d, flags:0x%x, af_family: %d"), 
           STREAM_RTP_PNETIOSOCK(*pDest)->ssl.state, STREAM_RTP_PNETIOSOCK(*pDest)->flags, 
           pDest->saDsts.ss_family ));

        do_dtls_handshake(STREAM_RTP_PNETIOSOCK(*pDest), (const struct sockaddr *) &pDest->saDsts, progRtp);
      }

      if(progRtcp && STREAM_RTP_FD(*pDest) != STREAM_RTCP_FD(*pDest) &&
        (STREAM_RTCP_PNETIOSOCK(*pDest)->flags & NETIO_FLAG_SSL_DTLS) && !sameRtcpSock &&
         STREAM_RTCP_FD(*pDest) != INVALID_SOCKET) {
       
        VSX_DEBUG_DTLS( 
          LOG(X_DEBUG("DTLS - calling do_dtls_handshake RCTP pnetsock.state: %d, flags:0x%x, af_family: %d"), 
             STREAM_RTCP_PNETIOSOCK(*pDest)->ssl.state, STREAM_RTCP_PNETIOSOCK(*pDest)->flags,
             pDest->saDstsRtcp.ss_family));

        do_dtls_handshake(STREAM_RTCP_PNETIOSOCK(*pDest), (const struct sockaddr *) &pDest->saDstsRtcp, progRtcp);
      }

    } // end of for(idxDest..

    pRtpMultiPrior = pRtpMulti;
    pRtpMulti = pRtpMulti->pnext;
  } // end of while(pRtpMulti...

  //LOG(X_DEBUG("streamer_do_dtls_handshake done rtp-numDtls:%d, rtp-numDtlsCompleted:%d, rtcp-numDtls:%d, rtcp-numDtlsCompleted:%d"), progRtp ? progRtp->numTotal : -1, progRtp ? progRtp->numCompleted : -1, progRtcp->numTotal, progRtcp->numCompleted);

  if(!progRtp) {
    return 0;
  } if(progRtp->numError > 0) {
    return -1;
  } else if(progRtp->numCompleted == progRtp->numTotal) {

    if(*ptmstartRtpHandshakeDone == 0) {
      *ptmstartRtpHandshakeDone = timer_GetTime();
    }

    if(progRtcp->numTotal - progRtcp->numCompleted > 0 &&
      ((tmnow = timer_GetTime()) - *ptmstartRtpHandshakeDone) / TIME_VAL_MS < pdtlsTimeouts->handshakeRtcpAdditionalMs) {
      return progRtcp->numTotal - progRtcp->numCompleted;
    } else {
      return 0;
    }

  } else {

    if(((tmnow = timer_GetTime()) - tmstart) / TIME_VAL_MS > pdtlsTimeouts->handshakeRtpTimeoutMs) {
      LOG(X_ERROR("Aborting DTLS RTP handshake(s) after %lld ms.  %d/%d completed"), 
          (tmnow - tmstart) / TIME_VAL_MS, progRtp->numCompleted, progRtp->numTotal);
      return -1;
    } else {
      return progRtp->numTotal - progRtp->numCompleted;
    }

  }

}

#endif // (VSX_HAVE_SSL_DTLS)

#if defined(VSX_HAVE_TURN)
static int do_turn_relays(STREAM_XMIT_NODE_T *pStream, NET_PROGRESS_T *progressTurn) {
  //int rc = 0;
  STREAM_RTP_MULTI_T *pRtpMulti = NULL;
  int idxDest;
  STREAM_RTP_DEST_T *pDest = NULL;
  NETIO_SOCK_T *pnetsock;

  if(progressTurn) {
    memset(progressTurn, 0, sizeof(NET_PROGRESS_T));
  }

  pRtpMulti = pStream->pRtpMulti;
  while(pRtpMulti) {

    if(g_proc_exit) {
      return -1;
    } else if(*pStream->prunning != STREAMER_STATE_RUNNING) {
      continue;
    }

    for(idxDest = 0; idxDest < pRtpMulti->numDests; idxDest++) {

      if(!(pDest = &pRtpMulti->pdests[idxDest]) || !pDest->isactive) {
        continue;
      }
      //LOG(X_DEBUG("DO_TURN_RELAYS.... idxDest:%d, pnetsock: 0x%x"), idxDest, STREAM_RTP_PNETIOSOCK(*pDest));
      if(progressTurn) {
        pnetsock = STREAM_RTP_PNETIOSOCK(*pDest);
        if(pnetsock->turn.use_turn_indication_in || pnetsock->turn.use_turn_indication_out) {
          progressTurn->numTotal++;
          if(pnetsock->turn.have_error) {
            progressTurn->numError++;
          } else if(turn_can_send_data(&pnetsock->turn)) {
            progressTurn->numCompleted++;
          }
        }
      }

    }

    pRtpMulti = pRtpMulti->pnext;
  } // end of while(pRtpMulti...

  if(!progressTurn) {
    return 0;
  } if(progressTurn->numError > 0) {
    return -1;
  } else if(progressTurn->numCompleted == progressTurn->numTotal) {
    return progressTurn->numTotal - progressTurn->numCompleted;
  } else {
    return progressTurn->numTotal - progressTurn->numCompleted;
  }

}
#endif // (VSX_HAVE_TURN)

int streamer_run_tonet(STREAM_XMIT_NODE_T *pList, double durationSec) {
  STREAM_XMIT_NODE_T *pStream;
  int triedXmit;
  TIME_VAL tmstartOutput = 0;
  TIME_VAL tmstart, tmnow;
  int haveTmstartOutput = 0;
  int rc;

#if defined(VSX_HAVE_TURN)
  int pendingTurnRelays = 1;
  int pendingTurnRelays0;
  NET_PROGRESS_T progTurn;
  memset(&progTurn, 0, sizeof(progTurn));
#else // (VSX_HAVE_TURN)
  int pendingTurnRelays = 0;
#endif // (VSX_HAVE_TURN)

#if defined(VSX_HAVE_SSL_DTLS)
  TIME_VAL tmstartRtpHandshakeDone = 0;
  int giveupRtcpHandshakes = 0;
  int pendingDtlsRtpHandshakes = 1;
  NET_PROGRESS_T progRtp;
  NET_PROGRESS_T progRtcp;
  DTLS_TIMEOUT_CFG_T dtlsTimeouts;

  setDtlsTimeouts(&dtlsTimeouts, pList);
  memset(&progRtp, 0, sizeof(progRtp));
  memset(&progRtcp, 0, sizeof(progRtcp));
#else
  int pendingDtlsRtpHandshakes = 0;
#endif // (VSX_HAVE_SSL_DTLS)


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

  tmstart = tmnow = timer_GetTime();

  while(*pList->prunning == STREAMER_STATE_RUNNING && !g_proc_exit) {

    pStream = pList;
    triedXmit = 0;

    while(pStream) {

#if defined(VSX_HAVE_TURN)

      pendingTurnRelays0 = pendingTurnRelays;

      if(pendingTurnRelays > 0 && (pendingTurnRelays = do_turn_relays(pStream, &progTurn)) < 0) {
        LOG(X_ERROR("Giving up waiting for output transmission after TURN relay setup failure"));
        return -1; 
      }

      if(pendingTurnRelays == 0 && pendingTurnRelays0 != pendingTurnRelays && progTurn.numTotal > 0) {
        LOG(X_INFO("Starting output transmission after creating TURN relay(s)"));
      }

      //LOG(X_DEBUG("pendingTurnRelays:%d, %d/%d, numerror:%d"), pendingTurnRelays, progTurn.numCompleted, progTurn.numTotal, progTurn.numError); 
#endif // (VSX_HAVE_TURN)

#if defined(VSX_HAVE_SSL_DTLS)
      //if(pStream->pRtpMulti->pStreamerCfg->xcode.vid.pip.active) 
      //LOG(X_DEBUG("loop... pendingDtlsRtpHandshakes:%d, rtp:%d - %d, rtcp:%d - %d, giveupRtcpHandshakes:%d, tm:%lld ms"), pendingDtlsRtpHandshakes, progRtp.numTotal,  progRtp.numCompleted, progRtcp.numTotal,  progRtcp.numCompleted, giveupRtcpHandshakes, (timer_GetTime() - tmstart) / TIME_VAL_MS);

      //LOG(X_DEBUG("streamer2: pendingTurnRelays;%d, completed:%d/%d, pendingDtlsRtpHandshakes:%d"), pendingTurnRelays, progTurn.numCompleted, progTurn.numTotal, pendingDtlsRtpHandshakes);

      if(pendingTurnRelays == 0 && pendingDtlsRtpHandshakes > 0 && 
         (pendingDtlsRtpHandshakes = do_dtls_handshakes(pStream, tmstart, &tmstartRtpHandshakeDone, 
                                                     &progRtp, &progRtcp, &dtlsTimeouts)) < 0) {
        return -1;
      } else if(pendingDtlsRtpHandshakes == 0 && pendingTurnRelays == 0) {

        if(!haveTmstartOutput) {
          tmstartOutput = timer_GetTime();
          haveTmstartOutput = 1;
        }

        if(progRtcp.numTotal - progRtcp.numCompleted > 0 && !giveupRtcpHandshakes) {

          if(tmstartRtpHandshakeDone == 0) {
            tmstartRtpHandshakeDone = timer_GetTime();
          }

          if(((tmnow = timer_GetTime()) - tmstartRtpHandshakeDone) / TIME_VAL_MS > 
             dtlsTimeouts.handshakeRtcpAdditionalGiveupMs) {
            LOG(X_ERROR("Aborting DTLS RTCP handshaking after %lld ms"), (tmnow - tmstart) / TIME_VAL_MS);
            giveupRtcpHandshakes = 1;
          } else {
            do_dtls_handshakes(pStream, tmstart, &tmstartRtpHandshakeDone, NULL, &progRtcp, &dtlsTimeouts);
          }  

          //LOG(X_DEBUG("loop... calling do_dtls_handshakes Rtcp.numDtls:%d - Rtcp.numDtlsCompleted"), progRtcp.numTotal, progRtcp.numCompleted);
        }
#endif // (VSX_HAVE_SSL_DTLS)

        //if(pStream->pRtpMulti->pStreamerCfg->xcode.vid.pip.active) 
        //LOG(X_DEBUG("Calling cbCanSend"));
        if((rc = pStream->cbCanSend(pStream->pCbData)) > 0) {

          if((rc = pStream->cbPreparePkt(pStream->pCbData)) < 0) {
            return -1;
          } else if(rc > 0) {
            triedXmit = 1;
          }

        } else if(rc == -2) {
          // all programs have ended
          return 0;
        } else if(rc < 0) {
          return -1;
        } else {
          //fprintf(stderr, "streamer cansend rc:%d\n", rc);
        }   

#if defined(VSX_HAVE_SSL_DTLS)
      }
#endif // (VSX_HAVE_SSL_DTLS)

      pStream = pStream->pNext;
    } // while(pStream)

    if(triedXmit == 0) {

#ifdef WIN32

      //sl1 = timer_GetTime();

      //Sleep(1) may sleep past its short bedtime on win32, even from a thread w/ SCHED_RR
      //However, sleep(0) uses alot more cpu slices
      //TODO: make this a WaitForSingleObject for any registered pktqueue writers
      
      if(pList->pSleepQ) {
        //TODO: this does not return if a pkt has been queued and no subsequent pkt arrives
        pktqueue_waitforunreaddata(pList->pSleepQ);
      } else {
        Sleep(1);
      }

    } else {
      // On windows Sleep 0 relinquishes execution for any waiting threads
      Sleep(0);

#else // WIN32


      VSX_DEBUG2(tmnow = timer_GetTime());

      //fprintf(stderr, "usleep...\n");
      if(pendingDtlsRtpHandshakes > 0 || pendingTurnRelays > 0) {
        usleep(10000);
      } else {
        usleep(1000);
      }
      //fprintf(stderr, "usleep done...\n");

      countIterations = 0;

      VSX_DEBUG_STREAMAV( LOG(X_DEBUGVV("STREAM - stream_readframes slept for %lld ns"), timer_GetTime() - tmnow));

      //fprintf(stderr, "%lld --woke up from streamer2 zzz...\n", timer_GetTime() / TIME_VAL_MS);
    } else {
      //fprintf(stderr, "stream_readframes not sleeping count:%d tried:%d\n", countIterations, triedXmit);

      //fprintf(stderr, "no usleep...%d\n", countIterations);
      if(countIterations++ > 10000) {
        // During continuous xmit, sleep to prevent unresponsive system
        usleep(1);
        countIterations = 0;
      } 
#endif // WIN32

    } 

    tmnow = timer_GetTime();

#if defined(TEST_KILL)
//static int raiseSig=0; if(!raiseSig && haveTmstartOutput && tmnow > tmstartOutput + (4* TIME_VAL_US)) { raise(SIGINT); }
static int raiseSig=0; if(!raiseSig && haveTmstartOutput && tmnow > tmstartOutput + (19* TIME_VAL_US)) { g_proc_exit=1;if(g_proc_exit_cond){pthread_cond_broadcast(g_proc_exit_cond);} }
#endif // TEST_KILL

    if(haveTmstartOutput && durationSec > 0 && tmnow > tmstartOutput + (durationSec * TIME_VAL_US)) {
      LOG(X_DEBUG("Stream duration %.1f sec limit reached"), durationSec);
      //*pList->prunning = STREAMER_STATE_FINISHED;
      return 0;
    }

#if defined(VSX_HAVE_LICENSE)
    // Check if stream time is limited
    if(!(pList->pLic->capabilities & LIC_CAP_STREAM_TIME_UNLIMITED)) {

      if(haveTmstartOutput && tmnow > tmstartOutput + (STREAM_LIMIT_SEC * TIME_VAL_US)) {
        LOG(X_INFO("Stream time limited.  Stopping stream transmission after %d sec"), 
                 (int) (tmnow - tmstartOutput) / TIME_VAL_US);
        *pList->prunning = STREAMER_STATE_FINISHED;
        if(!(g_proc_exit_flags & PROC_EXIT_FLAG_NO_EXIT_ON_STREAM_TIME_LIMITED)) {
          g_proc_exit = 1;
        }
      }
    }
#endif // VSX_HAVE_LICENSE

/*
LOG(X_DEBUG("pList: 0x%x, pip.active:%d, overlay.havePip:%d"), pList, pList->pRtpMulti->pStreamerCfg->xcode.vid.pip.active,  pList->pRtpMulti->pStreamerCfg->xcode.vid.overlay.havePip);
if(pList->pRtpMulti->pStreamerCfg->xcode.vid.pip.active && haveTmstartOutput && tmnow > tmstartOutput + (5 * TIME_VAL_US)) {
  *pList->prunning = STREAMER_STATE_FINISHED;
  if(!(g_proc_exit_flags & PROC_EXIT_FLAG_NO_EXIT_ON_STREAM_TIME_LIMITED)) {
    g_proc_exit = 1;
  }
}
*/

#if defined(LITE_VERSION)
    if(haveTmstartOutput && tmnow > tmstartOutput + (STREAM_LIMIT_LITE_SEC * TIME_VAL_US)) {
      LOG(X_INFO("Stream time limited.  Stopping stream transmission after %d sec"),
               (int) (tmnow - tmstartOutput) / TIME_VAL_US);
      *pList->prunning = STREAMER_STATE_FINISHED;
      if(!(g_proc_exit_flags & PROC_EXIT_FLAG_NO_EXIT_ON_STREAM_TIME_LIMITED)) {
        g_proc_exit = 1;
      }
    }
#endif // (LITE_VERSION)

  }

  return 0;
}

#endif // VSX_HAVE_STREAMER
