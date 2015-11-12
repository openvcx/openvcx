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


#ifndef __SYSUTIL_H__
#define __SYSUTIL_H__

#define SYS_MIN_INTERVAL_MS           5500
#define SYS_CPU_MIN_INTERVAL_MS       SYS_MIN_INTERVAL_MS
#define SYS_MEM_MIN_INTERVAL_MS       SYS_MIN_INTERVAL_MS

typedef struct CPU_SNAPSHOT {
  TIME_VAL              tvsnapshot;
  int                   numcpu;
  long double           tmuser;
  long double           tmnice;
  long double           tmsys;
  long double           tmidle;
  long double           tmtot;
} CPU_SNAPSHOT_T;

typedef struct MEM_SNAPSHOT {
  TIME_VAL           tvsnapshot;
  int64_t            memTotal;
  int64_t            memFree;
  int64_t            buffers;
  int64_t            cached;
  int64_t            memUsed;
} MEM_SNAPSHOT_T;

typedef struct CPU_USAGE_PERCENT {
  float                 percentuser;
  float                 percentnice;
  float                 percentsys;
  float                 percentidle;
} CPU_USAGE_PERCENT_T;

typedef struct SYS_USAGE_CTXT {
  CPU_SNAPSHOT_T         cpusnapshots[2];
  MEM_SNAPSHOT_T         memsnapshot;

  CPU_USAGE_PERCENT_T  *pcurC;
  MEM_SNAPSHOT_T       *pcurM;
} SYS_USAGE_CTXT_T;

int sys_cpu_usage(SYS_USAGE_CTXT_T *pCtxt);
int sys_mem_usage(SYS_USAGE_CTXT_T *pCtxt);
int sys_getcpus();

#endif // __SYSUTIL_H__
