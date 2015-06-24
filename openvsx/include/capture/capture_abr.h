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


#ifndef __CAPTURE_ABR_H__
#define __CAPTURE_ABR_H__

#include "util/burstmeter.h"

#define CAPTURE_STATS_ABR_RANGE_MS 5000

struct CAPTURE_STREAM;

typedef struct CAPTURE_ABR {
  int forceAdjustment;
  TIME_VAL tvLastAdjustment;
  TIME_VAL tvLastAdjustmentDown;
  TIME_VAL tvLastAdjustmentUp;
  double bwPayloadBpsTargetLast;
  double bwPayloadBpsTargetMax;
  double bwPayloadBpsTargetMin;
  BURSTMETER_SAMPLE_SET_T  payloadBitrate;
} CAPTURE_ABR_T;


int capture_abr_notifyBitrate(const struct CAPTURE_STREAM *pStream, double *pbwPayloadBpsCur);
int capture_abr_init(CAPTURE_ABR_T *pAbr);
void capture_abr_close(CAPTURE_ABR_T *pAbr);

#endif // __CAPTURE_ABR_H__
