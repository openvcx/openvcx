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


#ifndef __STREAM_NET_AV_H__
#define __STREAM_NET_AV_H__

#include "stream_net_pes.h"

typedef STREAM_PES_DATA_T  STREAM_AV_DATA_T;

typedef struct STREAM_NET_AV {

  STREAM_AV_DATA_T                vid;
  STREAM_AV_DATA_T                aud;
  XC_CODEC_TYPE_T                 vidCodecType;
  XC_CODEC_TYPE_T                 audCodecType;
  MP2_PES_PROG_T                  progs[2];
  STREAM_XMIT_ACTION_T           *pXmitAction;
} STREAM_NET_AV_T;




void stream_net_av_init(STREAM_NET_T *pNet);


#endif // __STREAM_NET_AV_H__
