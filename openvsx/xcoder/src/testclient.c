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


#ifdef WIN32

//#include <windows.h>
//#include "unixcompat.h"

#else // WIN32

#include <unistd.h>
#include <sys/types.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <pthread.h>

#include <semaphore.h>

#endif // WIN32

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "logutil.h"

#include "xcode_ipc.h"

static XCODE_IPC_DESCR_T *g_pXcodeIpc;

static sem_t *g_psem_srv;
static sem_t *g_psem_cli;

void xcode_ipc_close(XCODE_IPC_DESCR_T *pXcodeIpc);

void sig_handler(int sig) {

  xcode_ipc_close(g_pXcodeIpc);

  signal(sig, SIG_DFL);

  exit(0);
}

int xcode_ipc_init(XCODE_IPC_DESCR_T *pXcodeIpc) {

  struct shmid_ds buf;
  int shmid;
  int rc = 0;
  int flags = 0;
#ifdef __linux__
  flags |= 0x180;
#else
  flags |= SEM_R | SEM_A;
#endif // __linux__


  buf.shm_segsz = 0;
  if((shmid = shmget(pXcodeIpc->key, 0, flags)) != -1) {
    // get the segment size of any existing shm
    if(shmctl(shmid, IPC_STAT, &buf) != 0) {
      return -1;
    }
  }

  if(buf.shm_segsz == 0) {
    fprintf(stderr,"Shared mem not yet created by writer\n");
    return -1;
  }

  //if(buf.shm_segsz < sz) {
  //  fprintf(stderr,"shared mem size %d below minimum %d\n", buf.shm_segsz, sz);
  //  return -1;
  //}

  if((pXcodeIpc->shmid = shmget(pXcodeIpc->key, pXcodeIpc->sz, flags)) == -1) {
    fprintf(stderr,"shmget failed for key: 0x%x flags: 0x%x size: %u\n", 
                pXcodeIpc->key, flags, pXcodeIpc->sz); 
    return -1;
  } 

  if(shmctl(pXcodeIpc->shmid, IPC_STAT, &buf) != 0) {
    fprintf(stderr,"shmctl failed for shmid %d\n", pXcodeIpc->shmid);
    xcode_ipc_close(pXcodeIpc);
    return -1;
  }

  //if((pXcodeIpc->semid = semget(pXcodeIpc->key, 1, flags)) == -1) {
  //  fprintf(stderr,"semget failed for flags: 0x%x\n", flags);
  //  xcode_ipc_close(pXcodeIpc);
  //  return -1;
  //}

  if(buf.shm_nattch > 1) {
    fprintf(stderr, "shared mem already has %d attached\n", buf.shm_nattch);
    xcode_ipc_close(pXcodeIpc);
    return -1;
  }

  if((pXcodeIpc->pmem = shmat(pXcodeIpc->shmid, 0, 0)) == (void *) -1) {
    LOG(X_ERROR("shmat failed"));
    xcode_ipc_close(pXcodeIpc);
    return -1;
  }

  if(!(g_psem = sem_open("/tmp/sem_test", 0))) {
    fprintf(stderr, "sem_open failed");
    rc = -1;
  }

  return rc;
}

void xcode_ipc_close(XCODE_IPC_DESCR_T *pXcodeIpc) {
  struct shmid_ds buf;

  memset(&buf, 0, sizeof(buf));

  if(pXcodeIpc->shmid > 0) {
    if(pXcodeIpc->pmem) {
      shmdt(pXcodeIpc->pmem);
    }
    pXcodeIpc->shmid = -1;
  }

  if(g_psem) {
    if(sem_close(g_psem) != 0) {
      fprintf(stderr, "sem_close failed\n");
    }
    g_psem = NULL;
  }


}

static int xcode_ipc_run(XCODE_IPC_DESCR_T *pXcodeIpc) {
  int rc = 0;

  while(1) {

    sleep(2);
    fprintf(stderr, "posting");
    sem_post(g_psem);
    
    sleep(5);
  }

  while(1) {

  
    //fprintf(stderr, "pthread_cond_wait...\n");
    //pthread_cond_wait(&pXcodeIpc->pmem->hdr.cliCond, &pXcodeIpc->pmem->hdr.cliMtxCond);
    //fprintf(stderr, "ipc exec cmd: %d ...\n", pXcodeIpc->pmem->hdr.cmd);

    switch(pXcodeIpc->pmem->hdr.cmd) {
      case XCODE_IPC_CMD_INIT_VID:
        pXcodeIpc->pmem->hdr.cmdrc = pesxcode_init_vid(&pXcodeIpc->pmem->hdr.ctxt.vid);
        break;
      case XCODE_IPC_CMD_INIT_AUD:
        pXcodeIpc->pmem->hdr.cmdrc = pesxcode_init_aud(&pXcodeIpc->pmem->hdr.ctxt.aud);
        break;
      case XCODE_IPC_CMD_CLOSE_VID:
        pesxcode_close_vid(&pXcodeIpc->pmem->hdr.ctxt.vid);
        break;
      case XCODE_IPC_CMD_CLOSE_AUD:
        pesxcode_close_aud(&pXcodeIpc->pmem->hdr.ctxt.aud);
        break;
      case XCODE_IPC_CMD_ENCODE_VID:
        pXcodeIpc->pmem->hdr.cmdrc = pesxcode_frame_vid(&pXcodeIpc->pmem->hdr.ctxt.vid,
              XCODE_IPC_MEM_DATA(pXcodeIpc->pmem), XCODE_IPC_MEM_DATASZ(pXcodeIpc->pmem),
              XCODE_IPC_MEM_DATA(pXcodeIpc->pmem), XCODE_IPC_MEM_DATASZ(pXcodeIpc->pmem));
        break;
      case XCODE_IPC_CMD_ENCODE_AUD:
        pXcodeIpc->pmem->hdr.cmdrc = pesxcode_frame_aud(&pXcodeIpc->pmem->hdr.ctxt.aud,
              XCODE_IPC_MEM_DATA(pXcodeIpc->pmem), XCODE_IPC_MEM_DATASZ(pXcodeIpc->pmem),
              XCODE_IPC_MEM_DATA(pXcodeIpc->pmem), XCODE_IPC_MEM_DATASZ(pXcodeIpc->pmem));
        break;
      default:
        fprintf(stderr, "Invalid ipc command: %d\n", pXcodeIpc->pmem->hdr.cmd);
        pXcodeIpc->pmem->hdr.cmdrc = -1;
    }

    fprintf(stderr, "pthread_cond_broadcast...\n");
    pthread_cond_broadcast(&pXcodeIpc->pmem->hdr.srvCond);

  }

  return rc;
}

int main(int argc, char *argv[]) {
  XCODE_IPC_DESCR_T xcodeIpc;
  int rc;

  logger_Init(NULL, NULL, 0, 0, LOG_FLAG_USESTDERR | LOG_FLAG_FLUSHOUTPUT);

  logger_SetLevel(S_DEBUG);
  logger_AddStderr(S_DEBUG, LOG_OUTPUT_DEFAULT_STDERR);

  LOG(X_INFO("avcxcode starting"));

  memset(&xcodeIpc, 0, sizeof(xcodeIpc));
  xcodeIpc.key = XCODE_IPC_SHMKEY;
  xcodeIpc.sz = 0x38000 + sizeof(XCODE_IPC_MEM_HDR_T);

  g_pXcodeIpc = &xcodeIpc;
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  rc = xcode_ipc_init(&xcodeIpc);

  fprintf(stdout, "rc:%d\n", rc);

  if(rc == 0) {
    xcode_ipc_run(&xcodeIpc);
  }

  xcode_ipc_close(&xcodeIpc);

  return 0;
}
