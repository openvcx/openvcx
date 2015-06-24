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

#include <windows.h>

#else // WIN32

#include <unistd.h>
#include <sys/types.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <semaphore.h>
#include <sys/shm.h>
#include <sys/sem.h>
#include <signal.h>
#include <pthread.h>
#include <errno.h>

#endif // WIN32

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "logutil.h"
#include "xcode_ipc_srv.h"
#include "version.h"


#define SEM_MAX_ERRORS 10

#define XCODE_BANNER "vsx-xcode " VSX_VERSION

static XCODE_IPC_DESCR_T *g_pXcodeIpc;

void xcode_ipc_close(XCODE_IPC_DESCR_T *pXcodeIpc);

#if defined(WIN32)

int xcode_ipc_init(XCODE_IPC_DESCR_T *pXcodeIpc) {
  int rc = 0;

  if((pXcodeIpc->hFile = CreateFileMapping(INVALID_HANDLE_VALUE,
                                        NULL,
                                        PAGE_READWRITE,
                                        0,
                                        pXcodeIpc->sz,
                                        XCODE_FILE_MAPPING_NAME)) == NULL) {
    LOG(X_ERROR("CreateFileMapping %s failed (error:%d)"), XCODE_FILE_MAPPING_NAME, 
        GetLastError());
    return -1;
  }

  if((pXcodeIpc->pmem = MapViewOfFile(pXcodeIpc->hFile, 
                    FILE_MAP_ALL_ACCESS, 0, 0, pXcodeIpc->sz)) == NULL) {

    LOG(X_ERROR("MapViewOfFile %s failed (error:%d)"), XCODE_FILE_MAPPING_NAME,
                GetLastError());
    xcode_ipc_close(pXcodeIpc);
    return -1;
  }

  if((pXcodeIpc->hSemSrv = CreateSemaphore(NULL, 0, 1, XCODE_SEM_SRV_NAME)) == NULL) {
    LOG(X_ERROR("CreateSemaphore %s failed (error:%d)"), XCODE_SEM_SRV_NAME, GetLastError());
    xcode_ipc_close(pXcodeIpc);
    return -1;
  }

  if((pXcodeIpc->hSemCli = CreateSemaphore(NULL, 0, 1, XCODE_SEM_CLI_NAME)) == NULL) {
    LOG(X_ERROR("CreateSemaphore %s failed (error:%d)"), XCODE_SEM_CLI_NAME, GetLastError());
    xcode_ipc_close(pXcodeIpc);
    return -1;
  }

  pXcodeIpc->pmem->hdr.sz = pXcodeIpc->sz;
  pXcodeIpc->pmem->hdr.cmd = XCODE_IPC_CMD_NONE;

  LOG(X_DEBUG("Initialized shared memory"));

  return rc;
}

void xcode_ipc_close(XCODE_IPC_DESCR_T *pXcodeIpc) {

  if(pXcodeIpc->pmem) {
    UnmapViewOfFile(pXcodeIpc->pmem);
    pXcodeIpc->pmem = NULL;
  }

  if(pXcodeIpc->hFile) {
    CloseHandle(pXcodeIpc->hFile);
    pXcodeIpc->hFile= NULL;
  }

  if(pXcodeIpc->hSemSrv) {
    CloseHandle(pXcodeIpc->hSemSrv);
    pXcodeIpc->hSemSrv = NULL;
  }

  if(pXcodeIpc->hSemCli) {
    CloseHandle(pXcodeIpc->hSemCli);
    pXcodeIpc->hSemCli = NULL;
  }

}

#else // WIN32


void sig_handler(int sig) {

  LOG(X_DEBUG("Deleting shared memory"));
  xcode_ipc_close(g_pXcodeIpc);

  signal(sig, SIG_DFL);

  exit(0);
}

int xcode_ipc_init(XCODE_IPC_DESCR_T *pXcodeIpc) {

  struct shmid_ds buf;
  int shmid;
  int rc = 0;
  int flags = IPC_CREAT;
#ifdef __linux__
  flags |= 0x180;
#else // __linux__
  flags |= SEM_R | SEM_A;
#endif // __linux__


  buf.shm_segsz = 0;
  if((shmid = shmget(pXcodeIpc->key, 0, flags)) != -1) {
    // get the segment size of any existing shm
    if(shmctl(shmid, IPC_STAT, &buf) != 0) {
      LOG(X_ERROR("shmctl IPC_STAT failed with errno: %d"), errno);
      return -1;
    }
  }

  if(buf.shm_segsz > 0 && buf.shm_segsz != pXcodeIpc->sz) {
    if(buf.shm_nattch > 0) {
      LOG(X_ERROR("Unable to resize shared mem %u -> %u while %d reader(s) attached"),
                  buf.shm_segsz, pXcodeIpc->sz, buf.shm_nattch);
      return -1;
    }
    if(shmctl(shmid, IPC_RMID, &buf) != 0) {
      LOG(X_ERROR("Failed to remove shared mem segment"));
      return -1;
    }
  } 

  if((pXcodeIpc->shmid = shmget(pXcodeIpc->key, pXcodeIpc->sz, flags)) == -1) {
    LOG(X_ERROR("shmget failed for key: 0x%x flags: 0x%x size: %u with errno: %d"), 
                pXcodeIpc->key, flags, pXcodeIpc->sz, errno); 
    return -1;
  } 

  if(shmctl(pXcodeIpc->shmid, IPC_STAT, &buf) != 0) {
    LOG(X_ERROR("shmctl IPC_STAT failed for shmid %d errno: %d"), pXcodeIpc->shmid, errno);
    return -1;
  }

  if(buf.shm_nattch >= 1) {
    LOG(X_ERROR("shared mem already has %d attached"), buf.shm_nattch);
    return -1;
  } else {
    // init semaphore to 1
    //semctl(pXcodeIpc->semid, 0, SETVAL, 1);
  }

  if((pXcodeIpc->pmem = shmat(pXcodeIpc->shmid, 0, 0)) == (void *) -1) {
    LOG(X_ERROR("shmat failed"));
    return -1;
  }

  if((flags & IPC_CREAT) || buf.shm_nattch == 0) {

    pXcodeIpc->pmem->hdr.sz = pXcodeIpc->sz;
    pXcodeIpc->pmem->hdr.cmd = XCODE_IPC_CMD_NONE;

    if((pXcodeIpc->sem_srv = sem_open(XCODE_IPC_SEM_NAME_SRV, 0,
                           S_IRWXU | S_IRWXG | S_IRWXO , 0)) == SEM_FAILED) {

      if((pXcodeIpc->sem_srv = sem_open(XCODE_IPC_SEM_NAME_SRV, O_CREAT, 
                          S_IRWXU | S_IRWXG | S_IRWXO , 0)) == SEM_FAILED) {
        LOG(X_ERROR("sem_open server O_CREAT '%s' failed with errno: %d"), 
                       XCODE_IPC_SEM_NAME_SRV, errno);
        pXcodeIpc->sem_srv = NULL;
        return -1;
      }

    } else {
      LOG(X_DEBUG("sem_srv already exists"));
    }

    if((pXcodeIpc->sem_cli = sem_open(XCODE_IPC_SEM_NAME_CLI, 0, 
                          S_IRWXU | S_IRWXG | S_IRWXO , 0)) == SEM_FAILED) {

      if((pXcodeIpc->sem_cli = sem_open(XCODE_IPC_SEM_NAME_CLI, O_CREAT, 
                            S_IRWXU | S_IRWXG | S_IRWXO , 0)) == SEM_FAILED) {
        LOG(X_ERROR("sem_open client failed with errno: %d"), errno);
        pXcodeIpc->sem_cli = NULL;
        return -1;
      }

    } else {
      LOG(X_DEBUG("sem_cli already exists"));
    }

    LOG(X_DEBUG("Initialized shared memory"));

  }

  return rc;
}

void xcode_ipc_close(XCODE_IPC_DESCR_T *pXcodeIpc) {
  struct shmid_ds buf;

  memset(&buf, 0, sizeof(buf));

  if(pXcodeIpc->shmid > 0) {
    if(pXcodeIpc->pmem) {
      shmdt(pXcodeIpc->pmem);
      pXcodeIpc->pmem = NULL;
    }
    shmctl(pXcodeIpc->shmid, IPC_RMID, &buf);
    pXcodeIpc->shmid = -1;
  }

  if(pXcodeIpc->sem_srv) {
    if(sem_close(pXcodeIpc->sem_srv) != 0) {
      LOG(X_ERROR("sem_close failed"));
    }
    pXcodeIpc->sem_srv = NULL;
    sem_unlink(XCODE_IPC_SEM_NAME_SRV);
  }
 
  if(pXcodeIpc->sem_cli) {
    if(sem_close(pXcodeIpc->sem_cli) != 0) {
      LOG(X_ERROR("sem_close failed"));
    }
    pXcodeIpc->sem_cli = NULL;
    sem_unlink(XCODE_IPC_SEM_NAME_CLI);
  }

}

#endif // WIN32


static int xcode_ipc_dispatch(XCODE_IPC_DESCR_T *pXcodeIpc) {
  int errcnt = 0;
  IXCODE_OUTBUF_T outbuf;
#ifdef WIN32
  int rc;
#endif // WIN32

  while(errcnt < SEM_MAX_ERRORS) {

#if defined(WIN32)
    if((rc = WaitForSingleObject(pXcodeIpc->hSemSrv, INFINITE)) == WAIT_FAILED) {
      LOG(X_ERROR("WaitForSingleObject failed (error: %d)"), GetLastError());
#else // WIN32
    if(sem_wait(pXcodeIpc->sem_srv) != 0) {
      LOG(X_ERROR("sem_wait failed with error: %d"), errno);
#endif // WIN32
      errcnt++;
      continue;
    } else if(errcnt > 0) {
      errcnt = 0;
    }

    //fprintf(stderr, "ipc exec cmd: %d ...\n", pXcodeIpc->pmem->hdr.cmd);

    switch(pXcodeIpc->pmem->hdr.cmd) {
      case XCODE_IPC_CMD_INIT_VID:
        pXcodeIpc->pmem->hdr.cmdrc = ixcode_init_vid(&pXcodeIpc->pmem->hdr.ctxt.vid);
        break;
      case XCODE_IPC_CMD_INIT_AUD:
        pXcodeIpc->pmem->hdr.cmdrc = ixcode_init_aud(&pXcodeIpc->pmem->hdr.ctxt.aud);
        break;
      case XCODE_IPC_CMD_CLOSE_VID:
        ixcode_close_vid(&pXcodeIpc->pmem->hdr.ctxt.vid);
        pXcodeIpc->pmem->hdr.cmdrc = 0;
        break;
      case XCODE_IPC_CMD_CLOSE_AUD:
        ixcode_close_aud(&pXcodeIpc->pmem->hdr.ctxt.aud);
        pXcodeIpc->pmem->hdr.cmdrc = 0;
        break;
      case XCODE_IPC_CMD_ENCODE_VID:
        memset(&outbuf, 0, sizeof(outbuf));
        outbuf.buf = XCODE_IPC_MEM_DATA(pXcodeIpc->pmem); 
        outbuf.lenbuf = XCODE_IPC_MEM_DATASZ(pXcodeIpc->pmem);
        pXcodeIpc->pmem->hdr.cmdrc = ixcode_frame_vid(&pXcodeIpc->pmem->hdr.ctxt.vid,
              XCODE_IPC_MEM_DATA(pXcodeIpc->pmem), (unsigned int) pXcodeIpc->pmem->hdr.cmdrc, 
              &outbuf);
        break;
      case XCODE_IPC_CMD_ENCODE_AUD:
        if(pXcodeIpc->pmem->hdr.offsetOut > XCODE_IPC_MEM_DATASZ(pXcodeIpc->pmem)) {
          pXcodeIpc->pmem->hdr.cmdrc = IXCODE_RC_ERROR;
        } else {
          pXcodeIpc->pmem->hdr.cmdrc = ixcode_frame_aud(&pXcodeIpc->pmem->hdr.ctxt.aud,
              XCODE_IPC_MEM_DATA(pXcodeIpc->pmem), (unsigned int) pXcodeIpc->pmem->hdr.cmdrc,
              XCODE_IPC_MEM_DATA(pXcodeIpc->pmem) + pXcodeIpc->pmem->hdr.offsetOut, 
              XCODE_IPC_MEM_DATASZ(pXcodeIpc->pmem) - pXcodeIpc->pmem->hdr.offsetOut);
        }
        break;
      default:
        LOG(X_ERROR("Invalid ipc command: %d"), pXcodeIpc->pmem->hdr.cmd);
        pXcodeIpc->pmem->hdr.cmdrc = IXCODE_RC_ERROR;
    }

    if(pXcodeIpc->pmem->hdr.cmdrc < IXCODE_RC_OK) {
      LOG(X_ERROR("ipc exec cmd: %d rc: %d"), pXcodeIpc->pmem->hdr.cmd, 
                                              pXcodeIpc->pmem->hdr.cmdrc);
    }

    pXcodeIpc->pmem->hdr.cmd = XCODE_IPC_CMD_NONE;

#if defined(WIN32)
    if(ReleaseSemaphore(pXcodeIpc->hSemCli, 1, NULL) == 0) {
      LOG(X_ERROR("sem_post failed with error: %d"), GetLastError());
#else
    if(sem_post(pXcodeIpc->sem_cli) != 0) {
      LOG(X_ERROR("sem_post failed with error: %d"), errno);
#endif // WIN32
    }

  }

  return 0;
}



static void usage(int argc, char *argv[]) {
  fprintf(stdout, "usage: %s \n", argv[0]);
  fprintf(stdout, "Used in conjunction with OpenVSX to transcode video\n\n");
}

int main(int argc, char *argv[]) {
  XCODE_IPC_DESCR_T xcodeIpc;
  int rc;

  fprintf(stdout, "\n"XCODE_BANNER" "ARCH"\n\n", BUILD_INFO_NUM);

  if(argc > 1) {
    usage(argc, argv);
    return 0;
  }

  logger_Init(NULL, NULL, 0, 0, LOG_FLAG_USESTDERR | LOG_FLAG_FLUSHOUTPUT);
  logger_SetFile("log", "vsx-xcode", LOGGER_MAX_FILES, LOGGER_MAX_FILE_SZ,
                      LOG_OUTPUT_DEFAULT | LOG_FLAG_USELOCKING);
  logger_SetLevel(S_DEBUG);
  logger_AddStderr(S_DEBUG, LOG_OUTPUT_DEFAULT_STDERR);

  LOG(X_INFO("vsx-xcode starting"));

  memset(&xcodeIpc, 0, sizeof(xcodeIpc));
  g_pXcodeIpc = &xcodeIpc;

  xcodeIpc.sz = XCODE_IPC_MEM_SZ;

#if !defined(WIN32)
  xcodeIpc.key = XCODE_IPC_SHMKEY;
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  signal(SIGKILL, sig_handler);
#endif // WIN32

  rc = xcode_ipc_init(&xcodeIpc);

  if(rc == 0) {
    xcode_ipc_dispatch(&xcodeIpc);
  }

  xcode_ipc_close(&xcodeIpc);

  return 0;
}
