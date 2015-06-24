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


#ifndef __DEFAULTS_H__
#define __DEFAULTS_H__

#include "ixcode_pip.h"
#include "xcodeconfig.h"


#define MAX_PARTICIPANTS            (MAX_PIPS + 1)

#define MIXER_MAX_SAMPLE_RATE       48000
#define MIXER_SAMPLE_RATE           16000
#define MIXER_SOURCE_SAMPLE_RATE    MIXER_SAMPLE_RATE

#define MIXER_LATE_THRESHOLD_MS     80

#define MIXER_CONFIG_PATH           "etc/mixer.conf"

#define RTP_PAYLOAD_MTU_SILK        1400

#define MIXER_CHUNKSZ_MS            20
#define MIXER_OUTPUT_CHUNKSZ_MS     MIXER_CHUNKSZ_MS
#define MIXER_PREPROC_CHUNKSZ_MS    MIXER_CHUNKSZ_MS

#endif //__DEFAULTS_H__
