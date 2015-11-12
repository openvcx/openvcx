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
#include "util/sysutil_cpus.h"


int sys_getcpus() {
  return getcpus();
}

static int cpu_snapshot(CPU_SNAPSHOT_T *pS) {
  int rc = -1;

#if defined(__linux__)
  FILE_HANDLE fp;
  char buf[1024];
  const char *path = "/proc/stat";
  const char *p;
  int have_cpu = 0;

  if(!pS) {
    return -1;
  }

  memset(pS, 0, sizeof(CPU_SNAPSHOT_T));

  if((fp = fileops_Open(path, O_RDONLY)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open %s for reading. "ERRNO_FMT_STR), path, ERRNO_FMT_ARGS);
    return -1;
  }

  while(fileops_fgets(buf, sizeof(buf) -1, fp)) {
    if(!have_cpu && !strncmp(buf, "cpu ", 4)) {
      p = &buf[4];
      MOVE_WHILE_SPACE(p);
      sscanf(p, "%Lf %Lf %Lf %Lf", &pS->tmuser, &pS->tmnice, &pS->tmsys, &pS->tmidle);
      pS->tmtot = pS->tmuser + pS->tmnice + pS->tmsys + pS->tmidle;
      have_cpu = 1;
    } else if(!strncmp(buf, "cpu", 3)) {
      pS->numcpu++;
    }
  }

  fileops_Close(fp);

  if(have_cpu > 0) {
    rc = 0;
  }

#else // (__linux__)
  LOG(X_ERROR("CPU snapshot not implemented on this architecure"));
#endif // (__linux__)

  return rc;
}

static float get_percentage(long double val, const CPU_SNAPSHOT_T *pdiff) {

  if(pdiff->tmtot == 0) {
    return 0.0f;
  }
  return  (float) 100.0f * val  / (float) pdiff->tmtot;
}

int sys_cpu_usage(SYS_USAGE_CTXT_T *pCtxt) {
  int rc = 0;
  CPU_SNAPSHOT_T *pSnapshots[2];
  CPU_SNAPSHOT_T diff;
  TIME_VAL tmnow;

  if(!pCtxt || !pCtxt->pcurC) {
    return -1;
  }

  if(pCtxt->cpusnapshots[0].tvsnapshot == 0) {
    pSnapshots[0] = &pCtxt->cpusnapshots[0];
    pSnapshots[1]  = &pCtxt->cpusnapshots[1];
  } else {
    pSnapshots[1] = &pCtxt->cpusnapshots[0];
    pSnapshots[0]  = &pCtxt->cpusnapshots[1];
  }

  tmnow = timer_GetTime();

  //
  // Return if it has not yet been SYS_CPU_MIN_INTERVAL_MS since the last snapshot
  //
  if(pSnapshots[1]->tvsnapshot != 0 && 
     ((tmnow - pSnapshots[1]->tvsnapshot) / TIME_VAL_MS) < SYS_CPU_MIN_INTERVAL_MS) {
    LOG(X_DEBUGV("CPU snapshots not ready"));
    return 0;
  }
 
  //
  // Take a running snapshot of the cpu counters
  //
  if((rc = cpu_snapshot(pSnapshots[0])) < 0) {
    return rc;
  } 

  pSnapshots[0]->tvsnapshot = tmnow;

  if(pSnapshots[1]->tvsnapshot == 0) {
    //
    // We've only been called once and don't have at least 2 snapshots
    //
    return 0;
  }

  //
  // Make sure the counters have been updated since the last snapshot
  //
  if((diff.tmtot = pSnapshots[0]->tmtot - pSnapshots[1]->tmtot) == 0) {
    LOG(X_ERROR("CPU tmtot 0 (%Lf - %Lf)"), pSnapshots[0]->tmtot, pSnapshots[1]->tmtot);
    return 0;
  }

  diff.tmuser = pSnapshots[0]->tmuser - pSnapshots[1]->tmuser;
  diff.tmnice = pSnapshots[0]->tmnice - pSnapshots[1]->tmnice;
  diff.tmsys = pSnapshots[0]->tmsys - pSnapshots[1]->tmsys;
  diff.tmidle = pSnapshots[0]->tmidle - pSnapshots[1]->tmidle;

  pCtxt->pcurC->percentuser = get_percentage(diff.tmuser, &diff);
  pCtxt->pcurC->percentnice = get_percentage(diff.tmnice, &diff);
  pCtxt->pcurC->percentsys = get_percentage(diff.tmsys, &diff);
  pCtxt->pcurC->percentidle = get_percentage(diff.tmidle, &diff);

  pSnapshots[1]->tvsnapshot = 0;

  return 1;
}

static int mem_snapshot(MEM_SNAPSHOT_T *pM, TIME_VAL tmnow) {
  int rc = -1;
 
#if defined(__linux__)
  FILE_HANDLE fp;
  char buf[1024];
  char *p;
  const char *path = "/proc/meminfo";
  int haveCached = 0;
  int haveBuffers = 0;
  int haveFree = 0;

  if(!pM) {
    return -1;
  }

  memset(pM, 0, sizeof(MEM_SNAPSHOT_T));

  if((fp = fileops_Open(path, O_RDONLY)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open %s for reading. "ERRNO_FMT_STR), path, ERRNO_FMT_ARGS);
    return -1;
  }

  while(fileops_fgets(buf, sizeof(buf) -1, fp)) {

    if(!strncasecmp(buf, "MemTotal:", 9)) {
      p = &buf[9];
      MOVE_WHILE_SPACE(p);
      pM->memTotal = strutil_read_numeric(p, 0, 0, 0);
    } else if(!strncasecmp(buf, "MemFree:", 8)) {
      p = &buf[8];
      MOVE_WHILE_SPACE(p);
      pM->memFree = strutil_read_numeric(p, 0, 0, 0);
      haveFree = 1;
    } else if(!strncasecmp(buf, "Buffers:", 8)) {
      p = &buf[8];
      MOVE_WHILE_SPACE(p);
      pM->buffers = strutil_read_numeric(p, 0, 0, 0);
      haveBuffers = 1;
    } else if(!strncasecmp(buf, "Cached:", 7)) {
      p = &buf[7];
      MOVE_WHILE_SPACE(p);
      pM->cached = strutil_read_numeric(p, 0, 0, 0);
      haveCached = 1;
    }

    if(pM->memTotal > 0 && haveFree  && haveBuffers && haveCached) {
      pM->memUsed = pM->memTotal - pM->memFree;
      pM->tvsnapshot = tmnow;
      rc = 0;
      break;
    }
  }

  fileops_Close(fp);

#else // (__linux__)
  LOG(X_ERROR("Memory snapshot not implemented on this architecure"));
#endif // (__linux__)

  return rc;
}

int sys_mem_usage(SYS_USAGE_CTXT_T *pCtxt) {
  int rc = 0;
  TIME_VAL tmnow;

  if(!pCtxt || !pCtxt->pcurM) {
    return -1;
  }

  tmnow = timer_GetTime();

  //
  // Return if it has not yet been SYS_MEM_MIN_INTERVAL_MS since the last snapshot
  //
  if(pCtxt->memsnapshot.tvsnapshot != 0 && 
     ((tmnow - pCtxt->memsnapshot.tvsnapshot) / TIME_VAL_MS) < SYS_MEM_MIN_INTERVAL_MS) {
    LOG(X_DEBUGV("Memory snapshot not ready"));
    return 0;
  }
 
  //
  // Take a running snapshot of the cpu counters
  //
  if((rc = mem_snapshot(&pCtxt->memsnapshot, tmnow)) < 0)  {
    return rc;
  }

  memcpy(pCtxt->pcurM, &pCtxt->memsnapshot, sizeof(MEM_SNAPSHOT_T));

  return 1;
}
