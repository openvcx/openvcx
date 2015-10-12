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


#ifndef __PROC_DB_H__
#define __PROC_DB_H__

#include "unixcompat.h"
#include "formats/filetype.h"

#define SYS_PROC_NAME_MAX              HTTP_URL_LEN
#define SYS_PROC_INSTANCE_ID_LEN       16

#define PROCDB_EXPIRE_IDLE_SEC 30


#define PORT_RANGE_START              50000
#define PORT_COUNT                    1000


enum SYS_PROC_FLAG {
  SYS_PROC_FLAG_PENDING           = 0x01,
  SYS_PROC_FLAG_STARTED           = 0x02,
  SYS_PROC_FLAG_RUNNING           = 0x04,
  SYS_PROC_FLAG_ERROR             = 0x08
};

typedef struct SYS_PROC {
  struct SYS_PROC            *pnext;
  int                         pid;
  int                         startPort;
  int                         flags;
  struct timeval              tmStart;
  struct timeval              tmLastAccess;
  struct timeval              tmLastPoll;
  struct timeval              tmLastPollActive;
  int                         numActive;    // active clients repored in '/status'
  int                         isXcoded;
  char                        instanceId[SYS_PROC_INSTANCE_ID_LEN + 1];
  char                        tokenId[META_FILE_TOKEN_LEN]; 
  char                        name[SYS_PROC_NAME_MAX];      // resource name
  char                        id[META_FILE_IDSTR_MAX];
  int                         profileId;
  MEDIA_DESCRIPTION_T         mediaDescr;
  unsigned int                mbbps;       // macroblocks per second
} SYS_PROC_T;

typedef struct SYS_PROCLIST {
  SYS_PROC_T           *procs;
  int                   runMonitor;
  unsigned int          pollIntervalMs;
  pthread_mutex_t       mtx;
  unsigned int          maxInstances;
  unsigned int          activeInstances;
  unsigned int          maxXcodeInstances;
  unsigned int          activeXcodeInstances;
  unsigned int          mbbpsTot;       // macroblocks per second
  unsigned int          maxMbbpsTot;
  unsigned int          procexpiresec;
  uint16_t              minStartPort;
  uint16_t              maxStartPort;
  uint16_t              priorStartPort;
} SYS_PROCLIST_T;



SYS_PROC_T *procdb_findName(SYS_PROCLIST_T *pProcs, const char *name, 
                            const char *id, int lock);
int procdb_create_instanceId(char *buf);
SYS_PROC_T *procdb_findInstanceId(SYS_PROCLIST_T *pProcs, const char *instanceId, int lock);
int procdb_delete(SYS_PROCLIST_T *pProcs, const char *name, const char *id, int lock);
SYS_PROC_T *procdb_setup(SYS_PROCLIST_T *pProcs, const char *virtPath, const char *id, 
                         const MEDIA_DESCRIPTION_T *pMediaDescr, const char *pXcodeStr, 
                         const char *pInstanceId, const char *pTokenId, int lock);
int procdb_start(SYS_PROCLIST_T *pProcs, SYS_PROC_T *pProc, const char *filePath,
                 const STREAM_DEVICE_T *pDev, const char *launchpath, 
                 const char *xcodestr, const char *incapturestr, 
                 const char *userpass, int methodBits, int ssl);
int procdb_create(SYS_PROCLIST_T *pProcs);
void procdb_destroy(SYS_PROCLIST_T *pProcs);
unsigned int procdb_getMbbps(const MEDIA_DESCRIPTION_T *pMediaDescr);



#endif // __PROC_DB_H__
