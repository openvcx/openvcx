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


#ifndef __AC3_H__
#define __AC3_H__

#include "vsxconfig.h"


typedef struct AC3_DESCR {
  unsigned int               sampleRateHz;
  unsigned int               numChannels;
  unsigned int               frameSize;
  //int                        bsid;    // bit stream id
  //int                        bsmode;  // bit stream mode
  int                        acmode;  // audio coding mode
} AC3_DESCR_T;

int ac3_find_starthdr(const unsigned char *pbuf, unsigned int len);
int ac3_find_nexthdr(const unsigned char *pbuf, unsigned int len);
int ac3_parse_hdr(const unsigned char *pbuf, unsigned int len, AC3_DESCR_T *pAc3);





#endif // __AC3_H__
