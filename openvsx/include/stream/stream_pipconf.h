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

#ifndef __STREAM_PIPCONF_H__
#define __STREAM_PIPCONF_H__

#include "vsxlib_int.h"

#define PIP_KEY_PAD_TOP                "padTop"
#define PIP_KEY_PAD_BOTTOM             "padBottom"
#define PIP_KEY_PAD_LEFT               "padLeft"
#define PIP_KEY_PAD_RIGHT              "padRight"
#define PIP_KEY_PAD_ASPECT_RATIO       "padAspect"
#define PIP_KEY_PAD_RGB                "padRGB"
#define PIP_KEY_CROP_TOP               "cropTop"
#define PIP_KEY_CROP_BOTTOM            "cropBottom"
#define PIP_KEY_CROP_LEFT              "cropLeft"
#define PIP_KEY_CROP_RIGHT             "cropRight"
#define PIP_KEY_POSX_RIGHT             "xRight"
#define PIP_KEY_POSX                   "x"
#define PIP_KEY_POSY_BOTTOM            "yBottom"
#define PIP_KEY_POSY                   "y"
#define PIP_KEY_POSX1_RIGHT            "x1Right"
#define PIP_KEY_POSX1                  "x1"
#define PIP_KEY_POSY1_BOTTOM           "y1Bottom"
#define PIP_KEY_POSY1                  "y1"
#define PIP_KEY_PIX_PER_FRAME          "ppf"
#define PIP_KEY_ALPHA_MAX0             "alphamax0"
#define PIP_KEY_ALPHA_MAX1             "alphamax1"
#define PIP_KEY_ALPHA_PER_FRAME        "apf"
#define PIP_KEY_FRAME_DURATION         "frames"
#define PIP_KEY_ALPHA_MAX              "alphamax"
#define PIP_KEY_ALPHA_MIN              "alphamin"
#define PIP_KEY_ZORDER                 "zorder"
#define PIP_KEY_XCODE                  "xcode"
#define PIP_KEY_NOVIDEO                "novideo"
#define PIP_KEY_NOAUDIO                "noaudio"

#define PIP_MOTION_VEC_MAX             100



IXCODE_PIP_MOTION_T *pip_read_conf(PIP_CFG_T *pipCfg, char input[VSX_MAX_PATH_LEN],
                                   char pipXcodestr[KEYVAL_MAXLEN]);


#endif // __STREAM_PIPCONF_H__
