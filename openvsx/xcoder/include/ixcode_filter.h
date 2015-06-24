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

#ifndef __IXCODE_FILTER_H__
#define __IXCODE_FILTER_H__

typedef void * (* FUNC_FILTER_INIT) (int, void *);
typedef void (* FUNC_FILTER_FREE) (void *);
typedef int (* FUNC_FILTER_EXEC) (void *, const unsigned char *[4], const int *, unsigned char *[4],  \
                                  const int *, unsigned int, unsigned int);

typedef struct FILTER_COMMON {
  int                          active;
} FILTER_COMMON_T;

typedef struct FILTER_UNSHARP_PLANE {
  unsigned int                 sizeX;
  unsigned int                 sizeY;
  float                        strength;;
} FILTER_UNSHARP_PLANE_T;

typedef struct IXCODE_FILTER_UNSHARP {
  FILTER_COMMON_T              common;
  FILTER_UNSHARP_PLANE_T       luma;
  FILTER_UNSHARP_PLANE_T       chroma;
} IXCODE_FILTER_UNSHARP_T;


typedef struct IXCODE_FILTER_DENOISE { 
  FILTER_COMMON_T              common;
  double                       lumaSpacial;
  double                       lumaTemporal;
  double                       chromaSpacial;
  double                       chromaTemporal;
} IXCODE_FILTER_DENOISE_T;


#define IXCODE_COLOR_CONTRAST_DEFAULT        1.0f
#define IXCODE_COLOR_BRIGHTNESS_DEFAULT      1.0f
#define IXCODE_COLOR_HUE_DEFAULT             0.0f
#define IXCODE_COLOR_SATURATION_DEFAULT      1.0f
#define IXCODE_COLOR_GAMMA_DEFAULT           1.0f

typedef struct IXCODE_FILTER_COLOR {
  FILTER_COMMON_T             common;
  float                       fContrast;
  float                       fBrightness;
  float                       fHue;
  float                       fSaturation;
  float                       fGamma;
} IXCODE_FILTER_COLOR_T;


typedef struct IXCODE_FILTER_TEST {
  FILTER_COMMON_T             common;
  int                         val;
} IXCODE_FILTER_TEST_T;


#define IXCODE_FILTER_ROTATE_90(x) \
          (!(x).passthru && (x).rotate.common.active && \
          ((x).rotate.degrees == 90 || (x).rotate.degrees == -90 || \
           (x).rotate.degrees == 270 || (x).rotate.degrees == -270))

typedef struct IXCODE_FILTER_ROTATE {
  FILTER_COMMON_T             common;
  int                         degrees;
} IXCODE_FILTER_ROTATE_T;

#endif // __IXCODE_FILTER_H__
