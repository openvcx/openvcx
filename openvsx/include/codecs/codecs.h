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


#ifndef __CODECS__H__
#define __CODECS__H__

#if defined(WIN32)
#include "unixcompat.h"
#include "pthread_compat.h"
#else
#include "wincompat.h"
#include <unistd.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/time.h>
#endif // WIN32

#include "vsxconfig.h"

#include "codecs/esds.h"
#include "codecs/aac.h"
#include "codecs/ac3.h"
#include "codecs/amr.h"
#include "codecs/avcc.h"
#include "codecs/h264.h"
#include "codecs/h264avc.h"
#include "codecs/h264_analyze.h"
#include "codecs/mp3.h"
#include "codecs/mpeg2.h"
#include "codecs/mpg4v.h"
#include "codecs/h263.h"
#include "codecs/vorbis.h"
#include "codecs/vp8.h"

#endif // __CODECS_H__
