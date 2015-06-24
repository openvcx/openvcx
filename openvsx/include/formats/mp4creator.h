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


#ifndef __MP4_CREATOR_H__
#define __MP4_CREATOR_H__

#include "formats/mp4.h"
#include "formats/mp4track.h"
#include "codecs/h264avc.h"
#include "codecs/aac.h"


#if defined(VSX_CREATE_CONTAINER)
int mp4_create(const char *pathOut,
              FILE_STREAM_T *pStreamInAudio,
              FILE_STREAM_T *pStreamInVideo,
              double fpsAudio,
              double fpsVideo,
              enum MEDIA_FILE_TYPE audioType,
              enum MEDIA_FILE_TYPE videoType,
              const char *avsyncin,
              int esdsObjTypeIdxPlus1);
#endif // VSX_CREATE_CONTAINER


#endif // ___MP4_CREATOR_H__
