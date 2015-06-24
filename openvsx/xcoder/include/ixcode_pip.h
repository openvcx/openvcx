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

#ifndef __IXCODE_PIP_H__
#define __IXCODE_PIP_H__

//
// Max number of unique encoder output instances
//
#define IXCODE_VIDEO_OUT_MAX              4 
//#define IXCODE_VIDEO_OUT_MAX              VIDEO_OUT_MAX 

//
// Max number of PIP instances
//
#define MAX_PIPS                          6

//
// Max number of unique PIP resolutions that will feed
// into available overlays
//
#define PIP_ADD_MAX         2

/*
#define PIP_FLAGS_INSERT_AFTER_SCALE      0x00
#define PIP_FLAGS_INSERT_BEFORE_SCALE     0x01
#define PIP_FLAGS_POSITION_BOTTOM         0x02
#define PIP_FLAGS_POSITION_RIGHT          0x04
#define PIP_FLAGS_CLOSE_ON_END            0x08
*/

#define PIP_FLAGS_MOTION_X0_RIGHT         0x01
#define PIP_FLAGS_MOTION_X1_RIGHT         0x02
#define PIP_FLAGS_MOTION_Y0_BOTTOM        0x04
#define PIP_FLAGS_MOTION_Y1_BOTTOM        0x08

#include <pthread.h>

typedef struct IXCODE_VIDEO_CROP {

  //
  // cropping is applied to input picture before padding
  //
  // cropped_width =  resOutH - cropLeft + cropRight
  //

  unsigned int                 cropTop;
  unsigned int                 cropBottom;
  unsigned int                 cropLeft;
  unsigned int                 cropRight;

  //
  // padding causes shrinkage of input picture so that
  // shrunk_width = resOutH - padLeft - padRight
  //
  unsigned int                 padTopCfg;
  unsigned int                 padBottomCfg;
  unsigned int                 padLeftCfg;
  unsigned int                 padRightCfg;
  unsigned int                 padTop;
  unsigned int                 padBottom;
  unsigned int                 padLeft;
  unsigned int                 padRight;

  int                          padAspectR;
  unsigned char                padRGB[4];

} IXCODE_VIDEO_CROP_T;

typedef struct IXCODE_PIP_MOTION_VEC {
  int                           x0;
  int                           y0;
  int                           x1;
  int                           y1;
  unsigned int                  alphamax0_min1;
  unsigned int                  alphamax1_min1;
  float                         pixperframe;
  float                         alphaperframe;
  unsigned int                  frames;
  int                           flags;
  IXCODE_VIDEO_CROP_T           crop;
  struct IXCODE_PIP_MOTION_VEC *pnext;
} IXCODE_PIP_MOTION_VEC_T;

typedef struct IXCODE_PIP_MOTION {
  IXCODE_PIP_MOTION_VEC_T      *pm;
  IXCODE_PIP_MOTION_VEC_T      *pmCur;
  unsigned int                  indexInVec;
  int                           active;
  int                           flags;
} IXCODE_PIP_MOTION_T;

#define PIP_PLACEMENT_WIDTH(p)  ((p).pwidth ? (*(p).pwidth) : (p).width)
#define PIP_PLACEMENT_HEIGHT(p) ((p).pheight ? (*(p).pheight) : (p).height)

typedef struct IXCODE_PIP_PLACEMENT {
  unsigned int                *pwidth;  
  unsigned int                *pheight;
  unsigned int                 width;
  unsigned int                 height;
  unsigned int                 posx;
  unsigned int                 posy;
} IXCODE_PIP_PLACEMENT_T;

typedef struct IXCODE_VIDEO_PIP {
  int                          active;      // Used to indicate that this is a PIP instance
  int                          visible;     // 1 - visible, 0, temporarily non-visible (as if video on hold),
                                            // -1 - video disabled or not available permanantely
  int                          showVisual;
  int                          autoLayout;
  int                          doEncode;
  unsigned int                 indexPlus1;  // index of the streamer instance
  unsigned int                 id;          // global unique id during process lifetime
  int                          zorder;
  int                          haveAlpha;

  IXCODE_PIP_PLACEMENT_T       pos[PIP_ADD_MAX];

  unsigned int                *pwidthCfg;
  unsigned int                *pheightCfg;
  unsigned int                *pwidthInput;
  unsigned int                *pheightInput;
  unsigned int                 alphamax_min1;
  unsigned int                 alphamin_min1;
  int                          flags;
  //unsigned char               *rawDataBuf; // YUV contents buffer
  struct IXCODE_VIDEO_CTXT    *pXOverlay;
  void                        *pPrivData; 
  IXCODE_PIP_MOTION_T         *pMotion;
  struct IXCODE_AUDIO_CTXT    *pXcodeA;
} IXCODE_VIDEO_PIP_T;

#define PIP_BORDER_FLAG_TOP_MIN1       0x01
#define PIP_BORDER_FLAG_BOTTOM_MIN1    0x02

typedef struct IXCODE_PIP_BORDER {
  int                          active;
  int                          hBlocks;
  int                          vBlocks;
  int                          flags;
  int                          edgePx;
  unsigned char                colorRGB[4];
} IXCODE_PIP_BORDER_T;

typedef struct IXCODE_VIDEO_OVERLAY {
  int                          havePip;
  struct IXCODE_VIDEO_CTXT    *p_pipsx[MAX_PIPS]; 
  unsigned int                 pipframeidxs[MAX_PIPS];
  pthread_mutex_t              mtx;
  //int                          maxAutoLayoutDisplayPips;
  IXCODE_PIP_BORDER_T          border;
} IXCODE_VIDEO_OVERLAY_T;


#endif // __IXCODE_PIP_H__
