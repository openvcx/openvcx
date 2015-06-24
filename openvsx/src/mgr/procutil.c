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
#include "mgr/procutil.h"

#include <signal.h>


int procutil_readpid(const char *path) {
  FILE_HANDLE fp;
  char buf[64];
  int pid = 0;

  if((fp = fileops_Open(path, O_RDONLY )) == FILEOPS_INVALID_FP) {
    return 0;
  }

  if(fileops_fgets(buf, sizeof(buf), fp)) {
    pid = atoi(buf);
  }

  fileops_Close(fp);

  return pid;
}

int procutil_isrunning(int pid) {

#if defined(__linux__)
  struct stat st;
  char path[64];

  snprintf(path, sizeof(path), "/proc/%d", pid);

  if(fileops_stat(path, &st) == 0) {
    return 1;
  }

  return 0;

#elif defined(__APPLE__)

  int rc;
  int fd;
  char cmd[256];
#define PID_FILE "tmp/pid"

  snprintf(cmd, sizeof(cmd), "ps cu | awk \"{print \\$2}\" | grep '^%d$' > "PID_FILE,
           pid);

  if((rc = system(cmd)) < 0) {
    LOG(X_ERROR("Failed to run '%s'"), cmd);
    return -1;
  } else {
    if((fd = open("tmp/pid", O_RDONLY)) < 0) {
      LOG(X_ERROR("Failed to open "PID_FILE));
      return -1;
    }
    if((rc = read(fd, cmd, sizeof(cmd))) > 0) {
      rc = 1;
    } else {
      rc = 0;
    }
    close(fd);
    fileops_DeleteFile(PID_FILE);

  }

  return rc;

#elif defined(WIN32)

  return -1;

#endif // WIN32

  return -1;
}

static int wait_forend(int pid, unsigned int msmax) {
  int ms = 0;
  struct timeval tv0, tv1;

  gettimeofday(&tv0, NULL);

  while(ms < msmax) {

    if(!procutil_isrunning(pid)) {
      return 0;
    }
    gettimeofday(&tv1, NULL);
    ms = TIME_TV_DIFF_MS(tv1, tv0);
    usleep(50000);
  }

  return -1;
}

int procutil_kill_block(int pid) {
  int rc;

  kill(pid, SIGINT); 

  if((rc = wait_forend(pid, 2000)) == 0) {
    return rc;
  }

  kill(pid, SIGTERM); 

  if((rc = wait_forend(pid, 2000)) == 0) {
    return rc;
  }

  kill(pid, SIGKILL); 

  if((rc = wait_forend(pid, 2000)) == 0) {
    return rc;
  }

  return -1;
}

