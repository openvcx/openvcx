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

#ifndef __CODEC_WRAP_H__
#define __CODEC_WRAP_H__


struct AVFrame;

typedef struct CODEC_WRAP_CTXT {
  void                       *pctxt;   // codec implementation private context
  struct IXCODE_AVCTXT       *pAVCtxt;
  void                       *pXcode; // IXCODE_AUDIO_CTXT_T or IXCODE_VIDEO_CTXT_T
} CODEC_WRAP_CTXT_T;

typedef void (* XCODER_AUDIO_CLOSE)(CODEC_WRAP_CTXT_T *);
typedef int (* XCODER_AUDIO_INIT)(CODEC_WRAP_CTXT_T *);
typedef int (* XCODER_AUDIO_ENCODE)(CODEC_WRAP_CTXT_T *, unsigned char *, unsigned int, 
                                    const int16_t *, int64_t *pts);
typedef int (* XCODER_AUDIO_DECODE)(CODEC_WRAP_CTXT_T *, const unsigned char *, unsigned int, 
                                    int16_t *, unsigned int);

typedef void (* XCODER_VIDEO_CLOSE)(CODEC_WRAP_CTXT_T *);
typedef int (* XCODER_VIDEO_INIT)(CODEC_WRAP_CTXT_T *, unsigned int);
typedef int (* XCODER_VIDEO_ENCODE)(CODEC_WRAP_CTXT_T *, unsigned int, const struct AVFrame *, 
                                    unsigned char *, unsigned int);
typedef int (* XCODER_VIDEO_DECODE)(CODEC_WRAP_CTXT_T *, unsigned int, struct AVFrame *, 
                                    const unsigned char *, unsigned int);

typedef struct CODEC_WRAP_AUDIO_DECODER {
  CODEC_WRAP_CTXT_T          ctxt;
  XCODER_AUDIO_INIT          fInit;
  XCODER_AUDIO_CLOSE         fClose;
  XCODER_AUDIO_DECODE        fDecode;
} CODEC_WRAP_AUDIO_DECODER_T;

typedef struct CODEC_WRAP_AUDIO_ENCODER {
  CODEC_WRAP_CTXT_T          ctxt;
  XCODER_AUDIO_INIT          fInit;
  XCODER_AUDIO_CLOSE         fClose;
  XCODER_AUDIO_ENCODE        fEncode;
} CODEC_WRAP_AUDIO_ENCODER_T;

typedef struct CODEC_WRAP_VIDEO_DECODER {
  CODEC_WRAP_CTXT_T          ctxt;
  XCODER_VIDEO_INIT          fInit;
  XCODER_VIDEO_CLOSE         fClose;
  XCODER_VIDEO_DECODE        fDecode;
} CODEC_WRAP_VIDEO_DECODER_T;

typedef struct CODEC_WRAP_VIDEO_ENCODER {
  CODEC_WRAP_CTXT_T          ctxt;
  XCODER_VIDEO_INIT          fInit;
  XCODER_VIDEO_CLOSE         fClose;
  XCODER_VIDEO_ENCODE        fEncode;
} CODEC_WRAP_VIDEO_ENCODER_T;

typedef struct CODEC_WRAP_ENCODER {
  union {
    CODEC_WRAP_AUDIO_ENCODER_T       a;
    CODEC_WRAP_VIDEO_ENCODER_T       v;
  } u;
} CODEC_WRAP_ENCODER_T;

typedef struct CODEC_WRAP_DECODER {
  union {
    CODEC_WRAP_AUDIO_DECODER_T       a;
    CODEC_WRAP_VIDEO_DECODER_T       v;
  } u;
} CODEC_WRAP_DECODER_T;


#endif // __CODEC_WRAP_H__
