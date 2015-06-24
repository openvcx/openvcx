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


#ifndef __FILE_TYPE_H__
#define __FILE_TYPE_H__

#include "util/fileutil.h"
#include "formats/filetypes.h"

typedef struct VIDEO_DESCRIPTION_GENERIC {
  uint16_t                       resH;
  uint16_t                       resV;
  int                            profile;
  int                            level;
  float                          fps;
  enum MEDIA_FILE_TYPE           mediaType; 
} VIDEO_DESCRIPTION_GENERIC_T;

typedef struct MEDIA_VID_DESCRIPTION {
  VIDEO_DESCRIPTION_GENERIC_T    generic;
  char                           codec[32];
  char                           extra[32];
} MEDIA_VID_DESCRIPTION_T;

typedef struct AUDIO_DESCRIPTION_GENERIC {
  unsigned int                   clockHz;
  uint16_t                       channels;
  uint16_t                       pad;
  enum MEDIA_FILE_TYPE           mediaType; 
} AUDIO_DESCRIPTION_GENERIC_T;

typedef struct MEDIA_AUD_DESCRIPTION {
  AUDIO_DESCRIPTION_GENERIC_T    generic;
  char                           codec[32];
  char                           extra[32];
} MEDIA_AUD_DESCRIPTION_T;

typedef struct MEDIA_DESCRIPTION {
  enum MEDIA_FILE_TYPE           type;
  FILE_OFFSET_T                  totSz;
  double                         durationSec;
  FILE_OFFSET_T                  dataOffset;
  uint16_t                       haveVid;
  uint16_t                       haveAud;
  MEDIA_VID_DESCRIPTION_T        vid;
  MEDIA_AUD_DESCRIPTION_T        aud;
} MEDIA_DESCRIPTION_T;


enum MEDIA_FILE_TYPE filetype_check(FILE_STREAM_T *pStream);
int filetype_getdescr(FILE_STREAM_T *pFileStream, MEDIA_DESCRIPTION_T *pMedia, 
                      int fullintrospect);
const char *codecType_getCodecDescrStr(XC_CODEC_TYPE_T codecType);
unsigned int codecType_getBitsPerPx(XC_CODEC_TYPE_T codecType);
unsigned int codecType_getBytesPerSample(XC_CODEC_TYPE_T codecType);
int codecType_getRtpPT(XC_CODEC_TYPE_T codecType, int *payloadTypesMin1);

int codectype_isVid(XC_CODEC_TYPE_T codecType);
int codectype_isAud(XC_CODEC_TYPE_T codecType);

#endif // __FILE_TYPE_H__
