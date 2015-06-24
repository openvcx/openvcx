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


#ifndef __H264_ANALYZE_H__
#define __H264_ANALYZE_H__

#include "vsxconfig.h"
#include "formats/mp4boxes.h"
#include "formats/flv.h"
#include "util/fileutil.h"

#if defined(VSX_HAVE_VIDANALYZE)

int h264_analyze_flv(FLV_CONTAINER_T *pFlv, FILE *fpOut, double fps, int verbosity);
int h264_analyze_mp4(MP4_CONTAINER_T *pMp4, FILE *fpOut, double fps, int verbosity);
int h264_analyze_annexB(FILE_STREAM_T *pStreamH264, FILE *fpOut, double fps, int verbosity);

#endif // VSX_HAVE_VIDANALYZE

#endif // __H264_ANALYZE_H__
