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


#ifndef __XCODER_PIP_H__
#define __XCODER_PIP_H__

#include "ixcode.h"

typedef struct PIX_PLANE {
  unsigned char     *data[4];
  int                linesize[4];
} PIX_PLANE_T;

#define PIX_PLANE_DATA_SZ      (sizeof(unsigned char *) * 4)
#define PIX_PLANE_LINESIZE_SZ  (sizeof(int) * 4)


struct AVFrame;

int pip_create_overlay(IXCODE_VIDEO_CTXT_T *pXcode, unsigned char *data[4], int linesize[4],
                       const unsigned int outidx, unsigned int dst_w, unsigned int dst_h, int lock);
int pip_add(IXCODE_VIDEO_CTXT_T *pXcode, unsigned int outidx, struct AVFrame *pframesIn[PIP_ADD_MAX], struct AVFrame *pframesOut[PIP_ADD_MAX]);
int pip_resize_canvas(int pad, 
                      const unsigned char *src_data[4], const int src_linesize[4],
                      unsigned char *dst_data[4], int dst_linesize[4],
                      int w0, int left, int right,
                      int h0, int top, int bot, const unsigned char *rgb);
int pip_fill_rect(unsigned char *data[4], int linesize[4], unsigned int width, unsigned int height,
                  unsigned int startx, unsigned int starty, unsigned int fillwidth, unsigned int fillheight,
                  const unsigned char *rgb);
int pip_mark(unsigned char *dst_data[4], int dst_linesize[4], unsigned int ht);
//int pip_draw_grid(unsigned char *data[4], int linesize[4], int height, unsigned int hcount, 
//                  unsigned int vcount, int *parrhdividers, unsigned int borderpx, 
//                  const unsigned char *colorYUV);


#endif // __XCODER_PIP_H___
