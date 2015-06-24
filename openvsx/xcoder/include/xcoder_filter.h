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


#ifndef __XCODER_FILTER_H__
#define __XCODER_FILTER_H__

#include "ixcode_filter.h"

#if defined(XCODE_FILTER_UNSHARP) && (XCODE_FILTER_UNSHARP > 0)

#define XCODE_FILTER_ON 1

void filter_unsharp_free(void *pCtx);
void *filter_unsharp_init(int width, void *pCtx);
int filter_unsharp(void *pCtx, const unsigned char *src_data[4], const int src_linesize[4],
                   unsigned char *dst_data[4], const int dst_linesize[4],
                   unsigned int width, unsigned int height);

#endif // (XCODE_FILTER_UNSHARP) && (XCODE_FILTER_UNSHARP > 0)

#if defined(XCODE_FILTER_DENOISE) && (XCODE_FILTER_DENOISE > 0)

#define XCODE_FILTER_ON 1

void filter_denoise_free(void *pCtx);
void *filter_denoise_init(int width, void *pCtx);
int filter_denoise(void *pCtx, const unsigned char *src_data[4], const int src_linesize[4],
                   unsigned char *dst_data[4], const int dst_linesize[4],
                   unsigned int width, unsigned int height);
#endif // ((XCODE_FILTER_DENOISE) && (XCODE_FILTER_DENOISE > 0)

#if defined(XCODE_FILTER_COLOR) && (XCODE_FILTER_COLOR > 0)

#define XCODE_FILTER_ON 1

void filter_color_free(void *pCtx);
void *filter_color_init(int width, void *pCtx);
int filter_color(void *pCtx, const unsigned char *src_data[4], const int src_linesize[4],
                   unsigned char *dst_data[4], const int dst_linesize[4],
                   unsigned int width, unsigned int height);

#endif // (XCODE_FILTER_COLOR) && (XCODE_FILTER_COLOR > 0)

#if defined(XCODE_FILTER_ROTATE) && (XCODE_FILTER_ROTATE > 0)

#define XCODE_FILTER_ON 1

void filter_rotate_free(void *pCtx);
void *filter_rotate_init(int width, void *pCtx);
int filter_rotate(void *pCtx, const unsigned char *src_data[4], const int src_linesize[4],
                unsigned char *dst_data[4], const int dst_linesize[4],
                unsigned int width, unsigned int height);

#endif // (XCODE_FILTER_ROTATE) && (XCODE_FILTER_ROTATE > 0)

#if defined(XCODE_FILTER_TEST) && (XCODE_FILTER_TEST > 0)

#define XCODE_FILTER_ON 1

void filter_test_free(void *pCtx);
void *filter_test_init(int width, void *pCtx);
int filter_test(void *pCtx, const unsigned char *src_data[4], const int src_linesize[4],
                unsigned char *dst_data[4], const int dst_linesize[4],
                unsigned int width, unsigned int height);

#endif // (XCODE_FILTER_TEST) && (XCODE_FILTER_TEST > 0)


#endif // __XCODER_FILTER_H___
