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

#ifndef __THROUGHPUT_STATS_H__
#define __THROUGHPUT_STATS_H__


#include "util/burstmeter.h"

#define THROUGHPUT_STATS_BURSTRATES_MAX    2

typedef struct THROUGHPUT_STATS_METER {
  uint64_t                    bytes;
  uint64_t                    slots;
} THROUGHPUT_STATS_METER_T;

typedef struct THROUGHPUT_STATS_TM {
 uint64_t                     tm;
} THROUGHPUT_STATS_TM_T;

typedef struct THROUGHPUT_STATS {
  THROUGHPUT_STATS_METER_T        written;
  THROUGHPUT_STATS_METER_T        read;
  THROUGHPUT_STATS_METER_T        skipped;        // overwritten bytes / slots
  THROUGHPUT_STATS_TM_T           tmLastWr;
  THROUGHPUT_STATS_TM_T           tmLastRd;
  BURSTMETER_SAMPLE_SET_T     bitratesWr[THROUGHPUT_STATS_BURSTRATES_MAX];
  BURSTMETER_SAMPLE_SET_T     bitratesRd[THROUGHPUT_STATS_BURSTRATES_MAX];
} THROUGHPUT_STATS_T;



#endif // __THROUGHPUT_STATS_H__
