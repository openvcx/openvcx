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

#include "mgr/procdb.h"
#include "mgr/srvmgr.h"
#include "mgr/procutil.h"

#define PROCUTIL_PID_DIR               "tmp/"


//TODO: move this to http files
int http_getpage(const char *addr, uint16_t port, const char *uri,
                 char *buf, unsigned int szbuf, unsigned int tmtms) {
  int rc = 0;
  NETIO_SOCK_T netsock;
  struct sockaddr_storage sa;
  HTTP_PARSE_CTXT_T hdrCtxt;
  HTTP_RESP_T httpResp;
  unsigned int contentLen = 0;
  const char *p;
  unsigned int consumed = 0;
  struct timeval tv0, tv1;
  char hdr[1024];

  if(!addr || !buf || szbuf <= 0) {
    return -1;
  }
  if(!uri) {
    uri = "/";
  }

  //TODO: does not resolve host!
  memset(&sa, 0, sizeof(sa));
  if((rc = net_getaddress(addr, &sa)) < 0) {
    return rc;
  }
  INET_PORT(sa) = htons(port);

  //TODO: this will not work for ipv6 socket with current function prototype
  memset(&netsock, 0, sizeof(netsock));
  if((NETIOSOCK_FD(netsock) = net_opensocket(SOCK_STREAM, 0, 0, NULL)) == INVALID_SOCKET) {
    return -1;
  }

  if((rc = net_connect(NETIOSOCK_FD(netsock), (const struct sockaddr *) &sa)) != 0) {
    net_closesocket(&NETIOSOCK_FD(netsock));
    return rc;
  }

  gettimeofday(&tv0, NULL);

  memset(&hdrCtxt, 0, sizeof(hdrCtxt));
  memset(&httpResp, 0, sizeof(httpResp));
  hdrCtxt.pnetsock = &netsock;
  hdrCtxt.pbuf = hdr;
  hdrCtxt.szbuf = sizeof(hdr);
  hdrCtxt.tmtms = tmtms;

  VSX_DEBUG_MGR(LOG(X_DEBUG("MGR - Sending local status command: '%s' to: %d"), uri, htons(INET_PORT(sa))));

  if((httpcli_gethdrs(&hdrCtxt, &httpResp, (const struct sockaddr *) &sa, uri, NULL, 0, 0, NULL, NULL)) < 0) {
    return -1;
  }

  if(rc >= 0 && (p = conf_find_keyval(httpResp.hdrPairs, HTTP_HDR_CONTENT_LEN))) {
    contentLen = atoi(p);  
  } 

  if(rc >= 0) {
    if(contentLen <= 0) {
      LOG(X_ERROR("Content-Length not found in response"));
      rc = -1;
    } else if(contentLen > szbuf) {
      LOG(X_ERROR("Input buffer size too small %d / %d"), szbuf, contentLen);
      rc = -1;
    } else if(hdrCtxt.idxbuf > hdrCtxt.hdrslen) {
      if((consumed = hdrCtxt.idxbuf - hdrCtxt.hdrslen) < szbuf) {
        memcpy(buf, &hdr[hdrCtxt.hdrslen], consumed);
      } else {
        LOG(X_ERROR("Input buffer size too small %d / %d"), szbuf, contentLen);
        rc = -1;
      }
    }
  }

  if(rc >= 0 && net_setsocknonblock(NETIOSOCK_FD(netsock), 1) < 0) {
    rc = -1;
  } 

    //fprintf(stderr, "GET PAGE OK idx:%d hdrs:%d conentlen:%d\n", hdrCtxt.idxbuf, hdrCtxt.hdrslen, contentLen);

  while(rc >= 0 && consumed < contentLen) {
    if((rc = netio_recvnb(&netsock, (unsigned char *) &buf[consumed], 
                          contentLen - consumed, 500)) > 0) {
      consumed += rc;
    } 

    gettimeofday(&tv1, NULL);
    if(tmtms > 0 && consumed < contentLen && TIME_TV_DIFF_MS(tv1, tv0) > tmtms) {
      LOG(X_WARNING("HTTP %s:%d%s timeout %d ms exceeded"), FORMAT_NETADDR(sa, hdr, sizeof(hdr)), 
           ntohs(INET_PORT(sa)), uri, tmtms);
      break;
      rc = -1;
    }
  }

  if(contentLen > 0 && consumed >= contentLen) {
    rc = consumed;
  } else {
    rc = -1;
  }

  netio_closesocket(&netsock);

  return rc;
}

static int get_pid(SYS_PROC_T *pProc) {
  char path[VSX_MAX_PATH_LEN];
  snprintf(path, sizeof(path), "%s%s.pid", PROCUTIL_PID_DIR, pProc->instanceId);
  return procutil_readpid(path);
}

static void procdb_dump(SYS_PROCLIST_T *pProcs) {
  SYS_PROC_T *p;
  pthread_mutex_lock(&pProcs->mtx);
  p = pProcs->procs;
  while(p) {
    fprintf(stderr, "name:'%s', id:'%s' instance:%s, flags:0x%x\n", p->name, p->id, p->instanceId, p->flags);
    p = p->pnext;
  }
  pthread_mutex_unlock(&pProcs->mtx);
}

static int check_status(SYS_PROC_T *pProc) {
  int rc;
  char *p, *p2;
  KEYVAL_PAIR_T kv;
  char buf[1024];
  int numActiveRtmp = -1;
  int numActiveRtsp = -1;
  int numActiveRtspInterleaved = -1;
  int numActiveTsLive = -1;
  int numActiveFlvLive = -1;
  int numActiveMkvLive = -1;
  const char *host = "127.0.0.1";

  pProc->numActive = 0;

  if((rc = http_getpage(host, MGR_GET_PORT_STATUS(pProc->startPort), VSX_STATUS_URL, buf, sizeof(buf), 2000)) < 0) {
    LOG(X_ERROR("Failed to load "VSX_STATUS_URL" for %s:%d"), host, MGR_GET_PORT_STATUS(pProc->startPort));
    return rc;
  }

  p = buf;
  while(p - buf < rc) {
    p2 = p;
    while(p2 - buf < rc && *p2 != '&') {
      p2++;
    }
    //fprintf(stderr, "--%c%c%c len:%d, rc:%d\n", p[0], p[1], p[2], p2 - p, rc);
    if(conf_parse_keyval(&kv, p, p2 - p, '=', 0) == 2) {
      if(!strncasecmp(kv.key, "rtmp", 4)) {
        numActiveRtmp = atoi(kv.val);
      } else if(!strncasecmp(kv.key, "rtspi", 5)) {
        numActiveRtspInterleaved = atoi(kv.val);
      } else if(!strncasecmp(kv.key, "rtsp", 4)) {
        numActiveRtsp = atoi(kv.val);
      } else if(!strncasecmp(kv.key, "tslive", 6)) {
        numActiveTsLive = atoi(kv.val);
      } else if(!strncasecmp(kv.key, "flvlive", 7)) {
        numActiveFlvLive = atoi(kv.val);
      } else if(!strncasecmp(kv.key, "mkvlive", 7)) {
        numActiveMkvLive = atoi(kv.val);
      } else if(!strncasecmp(kv.key, "outputRate", 10)) {
       // output stream bps data rate 
      }

      //fprintf(stderr, "key:'%s' val:'%s'\n", kv.key , kv.val);
    }
    p = p2;
    while(p - buf < rc && *p == '&') {
      p++;
    }
  }


  if(numActiveRtmp > 0) {
    pProc->numActive += numActiveRtmp;
  }
  if(numActiveRtsp > 0) {
    pProc->numActive += numActiveRtsp;
  }
  if(numActiveTsLive > 0) {
    pProc->numActive += numActiveTsLive;
  }
  if(numActiveFlvLive > 0) {
    pProc->numActive += numActiveFlvLive;
  }
  if(numActiveMkvLive > 0) {
    pProc->numActive += numActiveMkvLive;
  }
 
  VSX_DEBUG_MGR( LOG(X_DEBUG("Status for %s (%s, profile:%s) %s:%d rtmp:%d rtsp:%d (interleaved:%d) ts:%d flv: %d mkv:%d"
                " (rc:%d '%s')"), 
     pProc->name, pProc->instanceId, pProc->id, host, MGR_GET_PORT_STATUS(pProc->startPort), 
     numActiveRtmp, numActiveRtsp, numActiveRtspInterleaved, numActiveTsLive, numActiveFlvLive, 
     numActiveMkvLive, rc, buf););

  return rc;
}

static void procdb_monitor_proc(void *pArg) {
  SYS_PROCLIST_T *pProcs = (SYS_PROCLIST_T *) pArg;
  SYS_PROC_T *pProc, *pProcTmp;
  SYS_PROC_T *pProcPrev;
  int remove;
  int rc;
  char logstr[128];
  struct timeval *ptvlatest;
  struct timeval tvnow;

  while(pProcs->runMonitor) {

    //procdb_dump(pProcs);

    gettimeofday(&tvnow, NULL);
    pProcPrev = NULL;
    pProc = pProcs->procs;

    while(pProc) {

      remove = 0;
      if((pProc->flags & SYS_PROC_FLAG_RUNNING)) {

        if((rc = procutil_isrunning(pProc->pid)) < 0) {
          remove = 1;
          snprintf(logstr, sizeof(logstr), "Unable to determine process state");
        } else if(rc == 0) {
          remove = 1;
          snprintf(logstr, sizeof(logstr), "Process has ended");
        } else {

          //
          // Poll the child process to see how many active sessions are being serviced
          //
          if(TIME_TV_DIFF_MS(tvnow, pProc->tmLastPoll) > pProcs->pollIntervalMs) {
//fprintf(stderr, "checkign status\n");
            check_status(pProc);
//fprintf(stderr, "checkign status done numActive:%d\n", pProc->numActive);
            memcpy(&pProc->tmLastPoll, &tvnow, sizeof(pProc->tmLastPoll));
            if(pProc->numActive > 0) {
              memcpy(&pProc->tmLastPollActive, &tvnow, sizeof(pProc->tmLastPollActive));
            }
          }

          if(pProc->tmLastAccess.tv_sec > pProc->tmLastPollActive.tv_sec) {
//fprintf(stderr, "last access is higher\n");
            ptvlatest = &pProc->tmLastAccess; 
          } else {
//fprintf(stderr, "last poll is higher\n");
            ptvlatest = &pProc->tmLastPollActive; 
          }
          
//fprintf(stderr, "comparing %d\n", tvnow.tv_sec - ptvlatest->tv_sec);
          if(tvnow.tv_sec > ptvlatest->tv_sec + pProcs->procexpiresec) {
            snprintf(logstr, sizeof(logstr), "Expiring idle process");
            remove = 1;
          }
        }

      }
        
      if(remove) {

        LOG(X_DEBUG("%s '%s' instance id: %s, id:'%s', pid:%d, flags:0x%x (active: %d/%d, xcode: %d/%d)"), 
            logstr, pProc->name, pProc->instanceId, pProc->id, pProc->pid, pProc->flags, 
          pProcs->activeInstances, pProcs->maxInstances, pProcs->activeXcodeInstances, 
          pProcs->maxXcodeInstances);

        if(procutil_kill_block(pProc->pid) < 0) {
          LOG(X_ERROR("Unable to kill process '%s' instance id: %s, pid:%d"), 
              pProc->name, pProc->instanceId, pProc->pid);
        }

        pthread_mutex_lock(&pProcs->mtx); 
        if(pProcPrev) {
          pProcPrev->pnext = pProc->pnext;
        }
        if(pProcs->procs == pProc) {
          pProcs->procs = pProc->pnext;
        }

        if(pProcs->activeInstances > 0) {
          pProcs->activeInstances--;
        }
        if(pProc->isXcoded && pProcs->activeXcodeInstances > 0) {
          pProcs->activeXcodeInstances--;
        }
        if(pProc->mbbps > 0 && pProcs->mbbpsTot > pProc->mbbps) {
          pProcs->mbbpsTot -= pProc->mbbps;
        }

        pProcTmp = pProc->pnext;
        free(pProc);
        pProc = pProcTmp;
        pthread_mutex_unlock(&pProcs->mtx); 

      } else { 
        pProcPrev = pProc;
        pProc = pProc->pnext;
      }

    }  // end of while(pProc

    usleep(1000000);

  } // end of while(runMonitor

}

int procdb_create(SYS_PROCLIST_T *pProcs) {
  pthread_t ptdMonitor;
  pthread_attr_t attrMonitor;

  if(!pProcs) {
    return -1;
  }

  pthread_mutex_init(&pProcs->mtx, NULL);
  pProcs->runMonitor = 1;
  pthread_attr_init(&attrMonitor);
  pthread_attr_setdetachstate(&attrMonitor, PTHREAD_CREATE_DETACHED);

  if(pProcs->minStartPort == 0) {
    pProcs->minStartPort = PORT_RANGE_START;
  }
  if(pProcs->maxStartPort == 0) {
    pProcs->maxStartPort = pProcs->minStartPort + PORT_COUNT;
  }
  if(pProcs->maxStartPort <= pProcs->minStartPort) {
    LOG(X_ERROR("Invalid port range %d - %d"), pProcs->minStartPort, pProcs->maxStartPort);
    return -1;
  }
  LOG(X_DEBUG("Using Port range %d - %d"), pProcs->minStartPort, pProcs->maxStartPort);

  pProcs->priorStartPort = pProcs->maxStartPort; 
  pProcs->pollIntervalMs = 5000;

  if(pthread_create(&ptdMonitor,
                    &attrMonitor,
                    (void *) procdb_monitor_proc,
                    (void *) pProcs) != 0) {
    LOG(X_ERROR("Unable to create process monitor thread"));
    pthread_mutex_destroy(&pProcs->mtx);
    pProcs->runMonitor = 0;
    return -1;
  }

  return 0;
}

void procdb_destroy(SYS_PROCLIST_T *pProcs) {
  SYS_PROC_T *pProc, *pProcNext;

  if(!pProcs) {
    return;
  }

  pProc = pProcs->procs;
  while(pProc) {
    pProcNext = pProc->pnext;
    free(pProc);
    pProc = pProcNext;
  }

  pthread_mutex_destroy(&pProcs->mtx);

}

int procdb_delete(SYS_PROCLIST_T *pProcs, const char *name, const char *id, int lock) {
  int rc = -1;
  SYS_PROC_T *pProc = NULL;
  SYS_PROC_T *pProcPrev = NULL;

  if(!pProcs || !name) {
    return -1;
  }

  if(!id) {
    id = "";
  }

  if(lock) {
    pthread_mutex_lock(&pProcs->mtx); 
  }

  pProc = pProcs->procs;

  while(pProc) {

   if(!strcasecmp(pProc->name, name) && !strcasecmp(pProc->id, id)) {
   
      if(pProcPrev) {
        pProcPrev->pnext = pProc->pnext;
      }
      if(pProcs->procs == pProc) {
        pProcs->procs = pProc->pnext;
      }

      LOG(X_DEBUG("Removing process '%s' pid:%d flags:0x%x"), 
          pProc->name, pProc->pid, pProc->flags);

      if(pProcs->activeInstances > 0) {
        pProcs->activeInstances--;
      }
      if(pProc->isXcoded && pProcs->activeXcodeInstances > 0) {
        pProcs->activeXcodeInstances--;
      }

      free(pProc);
      rc = 0;
      break;

    } else { 
      pProcPrev = pProc;
      pProc = pProc->pnext;
    }
  }

  if(lock) {
    pthread_mutex_unlock(&pProcs->mtx); 
  }

  if(rc != 0) {
    LOG(X_ERROR("Failed to remove process '%s'"), pProc->name);
  }

  return rc;
}

SYS_PROC_T *procdb_findName(SYS_PROCLIST_T *pProcs, 
                            const char *name, 
                            const char *id, 
                            int lock) {
  SYS_PROC_T *pProc = NULL;

  if(!pProcs || !name) {
    return NULL;
  }

  if(!id) {
    id = "";
  }

  if(lock) {
    pthread_mutex_lock(&pProcs->mtx); 
  }

  pProc = pProcs->procs;

  while(pProc) {
    if(!strcasecmp(pProc->name, name) && !strcasecmp(pProc->id, id)) {
      break;
    }
    pProc = pProc->pnext; 
  }

  if(lock) {
    pthread_mutex_unlock(&pProcs->mtx); 
  }

  return pProc;
}

SYS_PROC_T *procdb_findInstanceId(SYS_PROCLIST_T *pProcs, 
                                  const char *instanceId, 
                                  int lock) {
  SYS_PROC_T *pProc = NULL;

  if(!pProcs || !instanceId) {
    return NULL;
  }

  if(lock) {
    pthread_mutex_lock(&pProcs->mtx); 
  }

  pProc = pProcs->procs;

  while(pProc) {
    if(!strncasecmp(pProc->instanceId, instanceId, SYS_PROC_INSTANCE_ID_LEN)) {
      break;
    }
    pProc = pProc->pnext; 
  }

  if(lock) {
    pthread_mutex_unlock(&pProcs->mtx); 
  }

  return pProc;
}

int procdb_create_instanceId(char *buf) {
  unsigned int idx;
  unsigned char tmp[SYS_PROC_INSTANCE_ID_LEN / 2];

  for(idx = 0; idx < sizeof(tmp) - 1; idx += 2) {
    *((uint16_t *) &tmp[idx]) = random() % RAND_MAX;
  }

  hex_encode(tmp, sizeof(tmp), buf, SYS_PROC_INSTANCE_ID_LEN + 1);

  return SYS_PROC_INSTANCE_ID_LEN;
}

unsigned int procdb_getMbbps(const MEDIA_DESCRIPTION_T *pMediaDescr) {
  unsigned int mbbps;
 
  if(!pMediaDescr) {
    return 0;
  }

  mbbps = (pMediaDescr->vid.generic.resH * pMediaDescr->vid.generic.resV)  / 256;

  if((pMediaDescr->vid.generic.resH * pMediaDescr->vid.generic.resV) % 256 != 0) {
    mbbps++;
  }

  return mbbps;
}

SYS_PROC_T *procdb_setup(SYS_PROCLIST_T *pProcs, 
                         const char *virtPath, 
                         const char *pId,  
                         const MEDIA_DESCRIPTION_T *pMediaDescr, 
                         const char *pXcodeStr, 
                         const char *pInstanceId,
                         const char *pTokenId,
                         int lock) {

  SYS_PROC_T *pProc = NULL;

  if(!pProcs || !virtPath || virtPath[0] == '\0') {
    return NULL;
  }

  if(!(pProc = (SYS_PROC_T *) avc_calloc(1, sizeof(SYS_PROC_T)))) {
    return NULL;
  }

  strncpy(pProc->name, virtPath, SYS_PROC_NAME_MAX - 1);
  pProc->name[SYS_PROC_NAME_MAX - 1] = '\0';
  if(pId) {
    strncpy(pProc->id, pId, META_FILE_IDSTR_MAX - 1);
    pProc->id[META_FILE_IDSTR_MAX - 1] = '\0';
  }

  if(++pProcs->priorStartPort > pProcs->maxStartPort) {
    pProcs->priorStartPort = pProcs->minStartPort; 
  }

  pProc->startPort = pProcs->priorStartPort;
  gettimeofday(&pProc->tmStart, NULL);
  memcpy(&pProc->tmLastAccess, &pProc->tmStart, sizeof(pProc->tmLastAccess));
  memcpy(&pProc->tmLastPoll, &pProc->tmStart, sizeof(pProc->tmLastPoll));
  memcpy(&pProc->tmLastPollActive, &pProc->tmStart, sizeof(pProc->tmLastPollActive));
  pProc->flags = SYS_PROC_FLAG_PENDING;
  if(pMediaDescr) {
    memcpy(&pProc->mediaDescr, pMediaDescr, sizeof(pProc->mediaDescr));
    if(pXcodeStr) { 
      pProc->mbbps = procdb_getMbbps(pMediaDescr);
    }
  }

  if(pTokenId && pTokenId[0] != '\0') {
    strncpy(pProc->tokenId, pTokenId, sizeof(pProc->tokenId));
  }

  if(pInstanceId && pInstanceId[0] != '\0') {
    strncpy(pProc->instanceId, pInstanceId, sizeof(pProc->instanceId));
  } else {
    procdb_create_instanceId(pProc->instanceId);
  }

  // for the time being, each process uses 3 unique tcp ports
  pProcs->priorStartPort += MGR_PORT_ALLOC_COUNT;

  //
  // Add to list
  //
  if(lock) {
    pthread_mutex_lock(&pProcs->mtx); 
  }

  if(pXcodeStr) {
    pProc->isXcoded = 1;
    pProcs->activeXcodeInstances++;
  }
  pProcs->activeInstances++;

  if(pProcs->procs) {
    pProc->pnext = pProcs->procs;
  }
  pProcs->procs = pProc;
  if(lock) {
    pthread_mutex_unlock(&pProcs->mtx); 
  }

  return pProc;
}

static int write_listener(int *p_lastPort, 
                          int port, 
                          int ssl, 
                          const char *name,
                          const char *protocol,
                          const char *userpass, 
                          const char *listeneraddr, 
                          int comma, 
                          char *buf, 
                          unsigned int szbuf) {
  
  unsigned int idxbuf = 0;
  int rc;
  char tmpbuf[1024];

  //
  // '=' [http] [s] :// [ username:password@ ] [ listener-address ] [ : ] [ listener-port ]
  //
  snprintf(tmpbuf, sizeof(tmpbuf), "=%s%s://%s%s%s%s%d",
           protocol, ssl ? "s" : "", userpass ? userpass: "", userpass ? "@" : "", 
           listeneraddr ? listeneraddr : "", listeneraddr ? ":" : "", port);

  //
  // ',' [stream method name] [tmpbuf]
  //
  if((rc = snprintf(&buf[idxbuf], szbuf - idxbuf, "%s%s%s",
           comma ? "," : "", name, (!p_lastPort || port != *p_lastPort) ? tmpbuf : "")) > 0) {
    idxbuf += rc;
  }
  if(p_lastPort) {
    *p_lastPort = port;
  }

  return idxbuf;
}

static char *get_methods_str(int methodBits, 
                             int ssl,
                             char *buf, 
                             unsigned int szbuf, 
                             int startPort, 
                             const char *userpass) {
  unsigned int idxMethod;
  unsigned int idxbuf = 0;
  int lastHttpPort = -1;
  int lastPort = -1;
  int rc;

  if(userpass && userpass[0] == '\0') {
    userpass = NULL;
  }

  if(methodBits == 0) {
    methodBits = (1 << STREAM_METHOD_HTTPLIVE) |
                 (1 << STREAM_METHOD_FLVLIVE) |
                 (1 << STREAM_METHOD_MKVLIVE) |
                 (1 << STREAM_METHOD_TSLIVE) |
                 (1 << STREAM_METHOD_RTSP) |
                 (1 << STREAM_METHOD_RTMP);
  }

  buf[0] = '\0';

#define HTTP_PROTO_STR "http"
#define RTMP_PROTO_STR "rtmp"
#define RTSP_PROTO_STR "rtsp"

  for(idxMethod = 0; idxMethod < STREAM_METHOD_MAX; idxMethod++) {

    switch(methodBits & (1 << idxMethod)) {
      case (1 << STREAM_METHOD_DASH):
        if((rc = write_listener(&lastPort, MGR_GET_PORT_HTTP(startPort), ssl, "dash", HTTP_PROTO_STR,
                        userpass, NULL, idxbuf > 0 ? 1 : 0, &buf[idxbuf], szbuf - idxbuf)) > 0) {
          idxbuf += rc;
        }
        lastHttpPort = lastPort;
        break;
      case (1 << STREAM_METHOD_HTTPLIVE):
        if((rc = write_listener(&lastPort, MGR_GET_PORT_HTTP(startPort), ssl, "httplive", HTTP_PROTO_STR, 
                        userpass, NULL, idxbuf > 0 ? 1 : 0, &buf[idxbuf], szbuf - idxbuf)) > 0) {
          idxbuf += rc;
        }
        lastHttpPort = lastPort;
        break;
      case (1 << STREAM_METHOD_FLVLIVE):
        if((rc = write_listener(&lastPort, MGR_GET_PORT_HTTP(startPort), ssl, "flvlive", HTTP_PROTO_STR, 
                        userpass, NULL, idxbuf > 0 ? 1 : 0, &buf[idxbuf], szbuf - idxbuf)) > 0) {
          idxbuf += rc;
        }
        lastHttpPort = lastPort;
        break;
      case (1 << STREAM_METHOD_MKVLIVE):
        if((rc = write_listener(&lastPort, MGR_GET_PORT_HTTP(startPort), ssl, "mkvlive", HTTP_PROTO_STR, 
                        userpass, NULL, idxbuf > 0 ? 1 : 0, &buf[idxbuf], szbuf - idxbuf)) > 0) {
          idxbuf += rc;
        }
        lastHttpPort = lastPort;
        break;
      case (1 << STREAM_METHOD_TSLIVE):
        if((rc = write_listener(&lastPort, MGR_GET_PORT_HTTP(startPort), ssl, "tslive", HTTP_PROTO_STR, 
                        userpass, NULL, idxbuf > 0 ? 1 : 0, &buf[idxbuf], szbuf - idxbuf)) > 0) {
          idxbuf += rc;
        }
        lastHttpPort = lastPort;
        break;
      case (1 << STREAM_METHOD_RTMP):
        if((rc = write_listener(&lastPort, MGR_GET_PORT_RTMP(startPort), ssl, "rtmp", RTMP_PROTO_STR, 
                            userpass, NULL, idxbuf > 0 ? 1 : 0, &buf[idxbuf], szbuf - idxbuf)) > 0) {
          idxbuf += rc;
        }
        break;
      case (1 << STREAM_METHOD_RTSP):
        if((rc = write_listener(&lastPort, MGR_GET_PORT_RTSP(startPort), ssl, "rtsp", RTSP_PROTO_STR, 
                            userpass, NULL, idxbuf > 0 ? 1 : 0, &buf[idxbuf], szbuf - idxbuf)) > 0) {
          idxbuf += rc;
        }
        break;

      default:
        break;

    } // end of switch

  } // end of for(idxMethod...

  if(idxbuf > 0) {
    //
    // If we are using a streaming output method then include the '/live' URL which also allows for loading
    // various rsrc/*.js and img/ files required by rsrc/xxx_embed.html files
    //
    if((rc = write_listener(&lastHttpPort, MGR_GET_PORT_HTTP(startPort), ssl, "live", HTTP_PROTO_STR, 
                        userpass, NULL, idxbuf > 0 ? 1 : 0, &buf[idxbuf], szbuf - idxbuf)) > 0) {
      idxbuf += rc;
    }
  }

  //
  // Include a /status URL listener bound to localhost without any credentials
  //
  if((rc = write_listener(NULL, MGR_GET_PORT_STATUS(startPort), 0, "status", HTTP_PROTO_STR, 
                      NULL, "127.0.0.1", idxbuf > 0 ? 1 : 0, &buf[idxbuf], szbuf - idxbuf)) > 0) {
    idxbuf += rc;
  }

  return buf;
}

int procdb_start(SYS_PROCLIST_T *pProcs, 
                 SYS_PROC_T *pProc, 
                 const char *filePath,
                 const STREAM_DEVICE_T *pDev, 
                 const char *launchpath,
                 const char *xcodestr, 
                 const char *incapturestr, 
                 const char *userpass, 
                 int methodBits,
                 int ssl) {
  int rc = -1;
  int pid;
  struct timeval tv0, tv1;
  char methodsbuf[1024];
  char cmd[512];

  if(!pProcs || !pProc || !filePath || !pDev || !launchpath) {
    return -1;
  }

  get_methods_str(methodBits, ssl, methodsbuf, sizeof(methodsbuf), pProc->startPort, userpass);

  snprintf(cmd, sizeof(cmd), " %s %s \"%s\" %s \"%s\" \"%s\" \"%s\" \"%s\" &",
          launchpath, pProc->instanceId, filePath, 
          pDev->name, methodsbuf, xcodestr ? xcodestr : "",
          incapturestr ? incapturestr: "", pProc->tokenId);

  LOG(X_DEBUG("Starting '%s'"), cmd);

  if((rc = system(cmd)) < 0) {

    LOG(X_ERROR("Failed to start process '%s'"), cmd);

    if(procdb_delete(pProcs, pProc->name, pProc->id, 1) < 0) {
      pProc->flags |= SYS_PROC_FLAG_ERROR;
    }

    rc = -1;

  } else {

    pProc->flags = SYS_PROC_FLAG_STARTED;

    gettimeofday(&tv0, NULL);
    do {
      usleep(200000);
      pid = get_pid(pProc);
      gettimeofday(&tv1, NULL);
      if(tv1.tv_sec > tv0.tv_sec + 3) {
        LOG(X_ERROR("Timeout while waiting for pid file %s for %s"), 
              pProc->instanceId, pProc->name);
        break;
      }
    } while(pid == 0);

    if(pid != 0) {
      pProc->pid = pid;
      gettimeofday(&pProc->tmLastAccess, NULL);
      pProcs->mbbpsTot += pProc->mbbps;
      LOG(X_DEBUG("Process started '%s' instance id: %s, pid: %d, port: %d"), 
                  pProc->name, pProc->instanceId, pProc->pid, pProc->startPort);
      pProc->flags = SYS_PROC_FLAG_RUNNING;
      rc = 0;
    } else {
      pProc->flags |= SYS_PROC_FLAG_ERROR;
      procdb_delete(pProcs, pProc->name, pProc->id, 1);
      rc = -1;
    }

  }

  return rc;
}
