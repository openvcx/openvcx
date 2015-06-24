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


#ifndef __XCODE_PARSE_H__
#define __XCODE_PARSE_H__

#include "xcode_types.h"


int xcode_parse_configstr(const char *pstr, IXCODE_CTXT_T *pXcode, int encoderOpts, int dump);
int xcode_parse_configstr_vid(const char *pstr, IXCODE_VIDEO_CTXT_T *pXcode, int encoderOpts, int dump);
int xcode_parse_configstr_aud(const char *pstr, IXCODE_AUDIO_CTXT_T *pXcode, int encoderOpts, int dump);
int xcode_parse_xcfg(STREAM_XCODE_DATA_T *pData, int dump);
void xcode_dump_conf_video(IXCODE_VIDEO_CTXT_T *pXcode, unsigned int outidx, int rc, int logLevel);
void xcode_dump_conf_video_abr(IXCODE_VIDEO_CTXT_T *pXcode, unsigned int outidx, int logLevel);
void xcode_dump_conf_audio(IXCODE_AUDIO_CTXT_T *pXcode, unsigned int outidx, int rc, int logLevel);



#endif // __XCODE_ALLH__
