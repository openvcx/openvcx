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

#ifndef __IMAGE_GENERIC_H__
#define __IMAGE_GENERIC_H__

#include "vsxconfig.h"
#include "formats/filetypes.h"
#include "util/bits.h"


#define IMAGE_MAX_SIZE      9830400

typedef struct IMAGE_GENERIC_DESCR {
  MEDIA_FILE_TYPE_T           mediaType;
  BYTE_STREAM_T               bs;
  unsigned int                width;
  unsigned int                height;
} IMAGE_GENERIC_DESCR_T;


int image_open(const char *path, IMAGE_GENERIC_DESCR_T *pImg, 
               enum MEDIA_FILE_TYPE mediaType);
int image_close(IMAGE_GENERIC_DESCR_T *pImg);




#endif // __IMAGE_GENERIC_H__
