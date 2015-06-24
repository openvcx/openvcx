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


#ifdef WIN32

#include <windows.h>
#include "unixcompat.h"

#else // WIN32

#include <unistd.h>

#endif // WIN32

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <pthread.h>
#include <math.h>
#include "logutil.h"
#include "ixcode.h"
#include "xcoder.h"
#include "xcoder_filter.h"

#if defined(XCODE_FILTER_UNSHARP) && (XCODE_FILTER_UNSHARP > 0)

#define FILTER_UNSHARP_MIN_SIZE 2
#define FILTER_UNSHARP_MAX_SIZE 13
#define FILTER_UNSHARP_MIN_STRENGTH -2.0f
#define FILTER_UNSHARP_MAX_STRENGTH 5.0f

/* right-shift and round-up */
#define SHIFTUP(x,shift) (-((-(x))>>(shift)))

typedef struct FilterParam {
    int msize_x;                             ///< matrix width
    int msize_y;                             ///< matrix height
    int amount;                              ///< effect amount
    int steps_x;                             ///< horizontal step count
    int steps_y;                             ///< vertical step count
    int scalebits;                           ///< bits to shift pixel
    int32_t halfscale;                       ///< amount to add to pixel
    uint32_t *sc[(FILTER_UNSHARP_MAX_SIZE * FILTER_UNSHARP_MAX_SIZE) - 1]; ///< finite state machine storage
} FilterParam;

typedef struct UnsharpContext {
    FilterParam luma;   ///< luma parameters (width, height, amount)
    FilterParam chroma; ///< chroma parameters (width, height, amount)
    int hsub, vsub;
} UnsharpContext;


#if 0

//static const uint8_t av_clip_uint8(int a) {
//    if (a&(~0xFF)) return (-a)>>31;
//    else           return a;
//}

#define av_clip_uint8 av_clip_uint8_arm
static av_always_inline av_const uint8_t av_clip_uint8_arm(int a)
{
    unsigned x;
    __asm__ ("usat %0, #8,  %1" : "=r"(x) : "r"(a));
    return x;
}
#endif // 0

void filter_unsharp_free(void *pArg) {
  UnsharpContext *pCtx = (UnsharpContext *) pArg;
  unsigned int i;

  for (i = 0; i < 2 * pCtx->luma.steps_y; i++) {
    av_free(pCtx->luma.sc[i]);
  }
  for (i = 0; i < 2 * pCtx->chroma.steps_y; i++) {
    av_free(pCtx->chroma.sc[i]);
  }

  free(pCtx);
}

static void check_limit_size(unsigned int *pui) {
  if(*pui > FILTER_UNSHARP_MAX_SIZE) {
    *pui = FILTER_UNSHARP_MAX_SIZE;
  } else if(*pui < FILTER_UNSHARP_MIN_SIZE) {
    *pui = FILTER_UNSHARP_MIN_SIZE;
  }
}

static void check_limit_strength(float *pf) {
  if(*pf > FILTER_UNSHARP_MAX_STRENGTH) {
    *pf = FILTER_UNSHARP_MAX_STRENGTH;
  } else if(*pf < FILTER_UNSHARP_MIN_STRENGTH) {
    *pf = FILTER_UNSHARP_MIN_STRENGTH;
  }
}

void *filter_unsharp_init(int width, void *pArg) {
  IXCODE_FILTER_UNSHARP_T *pCfg = (IXCODE_FILTER_UNSHARP_T *) pArg;
  UnsharpContext *pCtx;
  int i;

  if((pCfg->luma.strength != 0 && (pCfg->luma.sizeX < 2 || pCfg->luma.sizeY < 2)) ||
     (pCfg->chroma.strength != 0 && (pCfg->chroma.sizeX < 2 || pCfg->chroma.sizeY < 2))) { 
    LOG(X_ERROR("Invalid sharpen size parameter"));  
    return NULL;
  }

  check_limit_size(&pCfg->luma.sizeX);
  check_limit_size(&pCfg->luma.sizeY);
  check_limit_size(&pCfg->chroma.sizeX);
  check_limit_size(&pCfg->chroma.sizeY);
  check_limit_strength(&pCfg->luma.strength);
  check_limit_strength(&pCfg->chroma.strength);

  if(!(pCtx = av_malloc(sizeof(struct UnsharpContext)))) {
    return NULL;
  }

  pCtx->hsub = pCtx->vsub = 1;

  pCtx->luma.msize_x = pCfg->luma.sizeX;
  pCtx->luma.msize_y = pCfg->luma.sizeY;
  pCtx->luma.amount = pCfg->luma.strength * 65536.0f;
  pCtx->luma.steps_x = pCtx->luma.msize_x / 2;
  pCtx->luma.steps_y = pCtx->luma.msize_y / 2;
  pCtx->luma.scalebits = (pCtx->luma.steps_x + pCtx->luma.steps_y) * 2;
  pCtx->luma.halfscale = 1 << (pCtx->luma.scalebits - 1);

  for (i = 0; i < 2 * pCtx->luma.steps_y; i++) {
    pCtx->luma.sc[i] = av_malloc(sizeof(*(pCtx->luma.sc[i])) * (width + 2 * pCtx->luma.steps_x));
  }

  pCtx->chroma.msize_x = pCfg->chroma.sizeX;
  pCtx->chroma.msize_y = pCfg->chroma.sizeY;
  pCtx->chroma.amount =  pCfg->chroma.strength * 65536.0f;
  pCtx->chroma.steps_x = pCtx->chroma.msize_x / 2;
  pCtx->chroma.steps_y = pCtx->chroma.msize_y / 2;
  pCtx->chroma.scalebits = (pCtx->chroma.steps_x + pCtx->chroma.steps_y) * 2;
  pCtx->chroma.halfscale = 1 << (pCtx->chroma.scalebits - 1);

  width = SHIFTUP(width, 1); // hsub

  for (i = 0; i < 2 * pCtx->chroma.steps_y; i++) {
    pCtx->chroma.sc[i] = av_malloc(sizeof(*(pCtx->chroma.sc[i])) * (width + 2 * pCtx->chroma.steps_x));
  }

  LOG(X_DEBUG("Filter %s luma x:%d, y:%d, %.2f, chroma x:%d, y:%d, %.2f"),
              pCfg->luma.strength > 0 || pCfg->luma.strength > 0 ? "sharpen" : "blur",
              pCtx->luma.msize_x, pCtx->luma.msize_y, pCfg->luma.strength,
              pCtx->chroma.msize_x, pCtx->chroma.msize_y, pCfg->chroma.strength);

  return pCtx;
}

static void apply_unsharp(uint8_t *dst, int dst_stride, const uint8_t *src, int src_stride,
                          int width, int height, FilterParam *fp) {
    uint32_t **sc = fp->sc;
    uint32_t sr[(FILTER_UNSHARP_MAX_SIZE * FILTER_UNSHARP_MAX_SIZE) - 1], tmp1, tmp2;

    int32_t res;
    int x, y, z;
    const uint8_t *src2 = NULL;  //silence a warning

    if (fp->amount == 0.0f) {
        if (dst_stride == src_stride)
            memcpy(dst, src, src_stride * height);
        else
            for (y = 0; y < height; y++, dst += dst_stride, src += src_stride)
                memcpy(dst, src, width);
        return;
    }

    for(y = 0; y < 2 * fp->steps_y; y++) {
        memset(sc[y], 0, sizeof(sc[y][0]) * (width + 2 * fp->steps_x));
    }

    for (y = -fp->steps_y; y < height + fp->steps_y; y++) {
        if (y < height)
            src2 = src;

        memset(sr, 0, sizeof(sr[0]) * (2 * fp->steps_x - 1));
        for (x = -fp->steps_x; x < width + fp->steps_x; x++) {
            tmp1 = x <= 0 ? src2[0] : x >= width ? src2[width-1] : src2[x];
            for (z = 0; z < fp->steps_x * 2; z += 2) {
                tmp2 = sr[z + 0] + tmp1; sr[z + 0] = tmp1;
                tmp1 = sr[z + 1] + tmp2; sr[z + 1] = tmp2;
            }
            for (z = 0; z < fp->steps_y * 2; z += 2) {
                tmp2 = sc[z + 0][x + fp->steps_x] + tmp1; sc[z + 0][x + fp->steps_x] = tmp1;
                tmp1 = sc[z + 1][x + fp->steps_x] + tmp2; sc[z + 1][x + fp->steps_x] = tmp2;
            }
            if (x >= fp->steps_x && y >= fp->steps_y) {
                const uint8_t *srx = src - fp->steps_y * src_stride + x - fp->steps_x;
                uint8_t       *dsx = dst - fp->steps_y * dst_stride + x - fp->steps_x;

                res = (int32_t)*srx + ((((int32_t) * srx - (int32_t)((tmp1 + fp->halfscale) >> fp->scalebits)) * fp->amount) >> 16);

                *dsx = av_clip_uint8(res);
            }
        }
        if (y >= 0) {
            dst += dst_stride;
            src += src_stride;
        }
    }
}

int filter_unsharp(void *pArg, const unsigned char *src_data[4], const int src_linesize[4],
                   unsigned char *dst_data[4], const int dst_linesize[4],
                   unsigned int uiWidth, unsigned int uiHeight) {
  int rc = 0;
  UnsharpContext *pCtx = (UnsharpContext *) pArg;
  int width = (int) uiWidth;
  int height = (int) uiHeight;

  int cw = SHIFTUP(width, pCtx->hsub);
  int ch = SHIFTUP(height, pCtx->vsub);

  //fprintf(stderr, "UNSHARP data: 0x%x 0x%x 0x%x ls:%d,%d,%d, amount luma:%d, chroma: %d, cw:%d, ch:%d\n", dst_data[0], dst_data[1], dst_data[2], dst_linesize[0], dst_linesize[1], dst_linesize[2], pCtx->luma.amount, pCtx->chroma.amount, cw, ch);

  apply_unsharp(dst_data[0], dst_linesize[0], src_data[0], src_linesize[0], width, height, &pCtx->luma);
  apply_unsharp(dst_data[1], dst_linesize[1], src_data[1], src_linesize[1], cw, ch, &pCtx->chroma);
  apply_unsharp(dst_data[2], dst_linesize[2], src_data[2], src_linesize[2], cw, ch, &pCtx->chroma);

  return rc;
}

#endif // (XCODE_FILTER_UNSHARP) && (XCODE_FILTER_UNSHARP > 0)


#if defined(XCODE_FILTER_DENOISE) && (XCODE_FILTER_DENOISE > 0)

typedef struct HQDN3DContext {
    int Coefs[4][512*16];
    unsigned int *Line;
    unsigned short *Frame[3];
    int hsub, vsub;
} HQDN3DContext;

void filter_denoise_free(void *pArg) {
  HQDN3DContext *pCtx = (HQDN3DContext *) pArg;

  if(pCtx->Line) {
    av_freep(&pCtx->Line);
  }
  if(pCtx->Frame[0]) {
    av_freep(&pCtx->Frame[0]);
  }
  if(pCtx->Frame[1]) {
    av_freep(&pCtx->Frame[1]);
  }
  if(pCtx->Frame[2]) {
    av_freep(&pCtx->Frame[2]);
  }
  
  av_free(pCtx);
}

static void precalcCoefs(int *Ct, double Dist25) {
  int i;
  double Gamma, Simil, C;

  Gamma = log(0.25) / log(1.0 - Dist25/255.0 - 0.00001);

  for (i = -255*16; i <= 255*16; i++) {
    Simil = 1.0 - FFABS(i) / (16*255.0);
    C = pow(Simil, Gamma) * 65536.0 * i / 16.0;
    Ct[16*256+i] = lrint(C);
  }

  Ct[0] = !!Dist25;
}

void *filter_denoise_init(int width, void *pArg) {
  IXCODE_FILTER_DENOISE_T *pCfg = (IXCODE_FILTER_DENOISE_T *) pArg;
  HQDN3DContext *pCtx;

  if(!(pCtx = av_mallocz(sizeof(struct HQDN3DContext)))) {
    return NULL;
  }

#define PARAM1_DEFAULT 4.0
#define PARAM2_DEFAULT 3.0
#define PARAM3_DEFAULT 6.0

  if(pCfg->lumaSpacial <= 0) {
    pCfg->lumaSpacial = PARAM1_DEFAULT;
  }
  if(pCfg->chromaSpacial <= 0) {
    pCfg->chromaSpacial = PARAM2_DEFAULT * pCfg->lumaSpacial / PARAM1_DEFAULT;
  }
  if(pCfg->lumaTemporal <= 0) {
    pCfg->lumaTemporal = PARAM3_DEFAULT * pCfg->lumaSpacial / PARAM1_DEFAULT;
  }
  if(pCfg->chromaTemporal <= 0) {
    pCfg->chromaTemporal = pCfg->lumaTemporal * pCfg->chromaSpacial / pCfg->lumaSpacial;
  }

  precalcCoefs(pCtx->Coefs[0], pCfg->lumaSpacial);
  precalcCoefs(pCtx->Coefs[1], pCfg->lumaTemporal);
  precalcCoefs(pCtx->Coefs[2], pCfg->chromaSpacial);
  precalcCoefs(pCtx->Coefs[3], pCfg->chromaTemporal);

  pCtx->hsub = 1;
  pCtx->vsub = 1;
  pCtx->Line = av_malloc(width * sizeof(*pCtx->Line));

  LOG(X_DEBUG("Filter denoise spacial luma:%.2f, chroma:%.2f, temporal luma:%.2f, chroma:%.2f"),
               pCfg->lumaSpacial, pCfg->chromaSpacial, pCfg->lumaTemporal, pCfg->chromaTemporal);

  return pCtx;
}

static inline unsigned int LowPassMul(unsigned int PrevMul, unsigned int CurrMul, int *Coef) {
    //    int dMul= (PrevMul&0xFFFFFF)-(CurrMul&0xFFFFFF);
    int dMul= PrevMul-CurrMul;
    unsigned int d=((dMul+0x10007FF)>>12);
    return CurrMul + Coef[d];
}

static void deNoiseTemporal(const unsigned char *FrameSrc,
                            unsigned char *FrameDest,
                            unsigned short *FrameAnt,
                            int W, int H, int sStride, int dStride,
                            int *Temporal) {
    long X, Y;
    unsigned int PixelDst;

    for (Y = 0; Y < H; Y++) {
        for (X = 0; X < W; X++) {
            PixelDst = LowPassMul(FrameAnt[X]<<8, FrameSrc[X]<<16, Temporal);
            FrameAnt[X] = ((PixelDst+0x1000007F)>>8);
            FrameDest[X]= ((PixelDst+0x10007FFF)>>16);
        }
        FrameSrc  += sStride;
        FrameDest += dStride;
        FrameAnt += W;
    }
}

static void deNoiseSpacial(const unsigned char *Frame,
                           unsigned char *FrameDest,
                           unsigned int *LineAnt,
                           int W, int H, int sStride, int dStride,
                           int *Horizontal, int *Vertical) {
    long X, Y;
    long sLineOffs = 0, dLineOffs = 0;
    unsigned int PixelAnt;
    unsigned int PixelDst;

    /* First pixel has no left nor top neighbor. */
    PixelDst = LineAnt[0] = PixelAnt = Frame[0]<<16;
    FrameDest[0]= ((PixelDst+0x10007FFF)>>16);

    /* First line has no top neighbor, only left. */
    for (X = 1; X < W; X++) {
        PixelDst = LineAnt[X] = LowPassMul(PixelAnt, Frame[X]<<16, Horizontal);
        FrameDest[X]= ((PixelDst+0x10007FFF)>>16);
    }

    for (Y = 1; Y < H; Y++) {
        unsigned int PixelAnt;
        sLineOffs += sStride, dLineOffs += dStride;
        /* First pixel on each line doesn't have previous pixel */
        PixelAnt = Frame[sLineOffs]<<16;
        PixelDst = LineAnt[0] = LowPassMul(LineAnt[0], PixelAnt, Vertical);
        FrameDest[dLineOffs]= ((PixelDst+0x10007FFF)>>16);

        for (X = 1; X < W; X++) {
            unsigned int PixelDst;
            /* The rest are normal */
            PixelAnt = LowPassMul(PixelAnt, Frame[sLineOffs+X]<<16, Horizontal);
            PixelDst = LineAnt[X] = LowPassMul(LineAnt[X], PixelAnt, Vertical);
            FrameDest[dLineOffs+X]= ((PixelDst+0x10007FFF)>>16);
        }
    }
}

static void deNoise(const unsigned char *Frame,
                    unsigned char *FrameDest,
                    unsigned int *LineAnt,
                    unsigned short **FrameAntPtr,
                    int W, int H, int sStride, int dStride,
                    int *Horizontal, int *Vertical, int *Temporal) {
    long X, Y;
    long sLineOffs = 0, dLineOffs = 0;
    unsigned int PixelAnt;
    unsigned int PixelDst;
    unsigned short* FrameAnt=(*FrameAntPtr);

    if (!FrameAnt) {
        (*FrameAntPtr) = FrameAnt = av_malloc(W*H*sizeof(unsigned short));
        for (Y = 0; Y < H; Y++) {
            unsigned short* dst=&FrameAnt[Y*W];
            const unsigned char* src=Frame+Y*sStride;
            for (X = 0; X < W; X++) dst[X]=src[X]<<8;
        }
    }

    if (!Horizontal[0] && !Vertical[0]) {
        deNoiseTemporal(Frame, FrameDest, FrameAnt,
                        W, H, sStride, dStride, Temporal);
        return;
    }
    if (!Temporal[0]) {
        deNoiseSpacial(Frame, FrameDest, LineAnt,
                       W, H, sStride, dStride, Horizontal, Vertical);
        return;
    }

    /* First pixel has no left nor top neighbor. Only previous frame */
    LineAnt[0] = PixelAnt = Frame[0]<<16;
    PixelDst = LowPassMul(FrameAnt[0]<<8, PixelAnt, Temporal);
    FrameAnt[0] = ((PixelDst+0x1000007F)>>8);
    FrameDest[0]= ((PixelDst+0x10007FFF)>>16);

    /* First line has no top neighbor. Only left one for each pixel and
     * last frame */
    for (X = 1; X < W; X++) {
        LineAnt[X] = PixelAnt = LowPassMul(PixelAnt, Frame[X]<<16, Horizontal);
        PixelDst = LowPassMul(FrameAnt[X]<<8, PixelAnt, Temporal);
        FrameAnt[X] = ((PixelDst+0x1000007F)>>8);
        FrameDest[X]= ((PixelDst+0x10007FFF)>>16);
    }

    for (Y = 1; Y < H; Y++) {
        unsigned int PixelAnt;
        unsigned short* LinePrev=&FrameAnt[Y*W];
        sLineOffs += sStride, dLineOffs += dStride;
        /* First pixel on each line doesn't have previous pixel */
        PixelAnt = Frame[sLineOffs]<<16;
        LineAnt[0] = LowPassMul(LineAnt[0], PixelAnt, Vertical);
        PixelDst = LowPassMul(LinePrev[0]<<8, LineAnt[0], Temporal);
        LinePrev[0] = ((PixelDst+0x1000007F)>>8);
        FrameDest[dLineOffs]= ((PixelDst+0x10007FFF)>>16);

        for (X = 1; X < W; X++) {
            unsigned int PixelDst;
            /* The rest are normal */
            PixelAnt = LowPassMul(PixelAnt, Frame[sLineOffs+X]<<16, Horizontal);
            LineAnt[X] = LowPassMul(LineAnt[X], PixelAnt, Vertical);
            PixelDst = LowPassMul(LinePrev[X]<<8, LineAnt[X], Temporal);
            LinePrev[X] = ((PixelDst+0x1000007F)>>8);
            FrameDest[dLineOffs+X]= ((PixelDst+0x10007FFF)>>16);
        }
    }
}


int filter_denoise(void *pArg, const unsigned char *src_data[4], const int src_linesize[4],
                   unsigned char *dst_data[4], const int dst_linesize[4],
                   unsigned int width, unsigned int height) {
  int rc = 0;
  HQDN3DContext *pCtx = (HQDN3DContext *) pArg;
  int cw = width >> pCtx->hsub;
  int ch = height >> pCtx->vsub;

  deNoise(src_data[0], dst_data[0], pCtx->Line, &pCtx->Frame[0], width, height,
            src_linesize[0], dst_linesize[0], pCtx->Coefs[0],
            pCtx->Coefs[0], pCtx->Coefs[1]);

  deNoise(src_data[1], dst_data[1], pCtx->Line, &pCtx->Frame[1], cw, ch,
          src_linesize[1], dst_linesize[1], pCtx->Coefs[2],
          pCtx->Coefs[2], pCtx->Coefs[3]);

  deNoise(src_data[2], dst_data[2], pCtx->Line, &pCtx->Frame[2], cw, ch,
          src_linesize[2], dst_linesize[2], pCtx->Coefs[2],
          pCtx->Coefs[2], pCtx->Coefs[3]);

  return rc;
}

#endif // (XCODE_FILTER_DENOISE) && (XCODE_FILTER_DENOISE > 0)

#if defined(XCODE_FILTER_COLOR) && (XCODE_FILTER_COLOR > 0)

typedef struct COLOR_CTX {
  int            matrix_luma[256];
  int            matrix_gamma[256];
  int            i_sat;
  int            i_sin;
  int            i_cos;
  int            i_x;
  int            i_y;
} COLOR_CTX_T;


#define ADJUST_8_TIMES(x) x; x; x; x; x; x; x; x

#ifndef M_PI
#   define M_PI 3.14159265358979323846
#endif

#if 0

//static const uint8_t av_clip_uint8(int a) {
//    if (a&(~0xFF)) return (-a)>>31;
//    else           return a;
//}

#define av_clip_uint8 av_clip_uint8_arm
static av_always_inline av_const uint8_t av_clip_uint8_arm(int a)
{
    unsigned x;
    __asm__ ("usat %0, #8,  %1" : "=r"(x) : "r"(a));
    return x;
}
#endif // 0

static void filter_color_uv(const COLOR_CTX_T *pCtx, 
                           const unsigned char *src_data[4], const int src_linesize[4],
                           unsigned char *dst_data[4], const int dst_linesize[4],
                           unsigned int height) {

  const uint8_t *p_in_u, *p_in_v;
  uint8_t *p_out_u, *p_out_v;
  uint8_t i_u, i_v;
  unsigned int x, y;

  for(y = 0; y < height/2; y++) {

    x = 0;
    p_in_u = src_data[1] + (y * src_linesize[1]);
    p_in_v = src_data[2] + (y * src_linesize[2]);
    p_out_u = dst_data[1] + (y * dst_linesize[1]);
    p_out_v = dst_data[2] + (y * dst_linesize[2]);

    while(x < src_linesize[1] - 8) {

      ADJUST_8_TIMES(
        i_u = *p_in_u++; 
        i_v = *p_in_v++; 
        if(pCtx->i_sat > 256) {
          *p_out_u++ = av_clip_uint8( (( ((i_u * pCtx->i_cos + i_v * pCtx->i_sin - pCtx->i_x) >> 8) * 
                                pCtx->i_sat) >> 8) + 128); 
          *p_out_v++ = av_clip_uint8( (( ((i_v * pCtx->i_cos - i_u * pCtx->i_sin - pCtx->i_y) >> 8) * 
                                pCtx->i_sat) >> 8) + 128);
        } else {
          *p_out_u++ =  (( ((i_u * pCtx->i_cos + i_v * pCtx->i_sin - pCtx->i_x) >> 8) * 
                                pCtx->i_sat) >> 8) + 128; 
          *p_out_v++ =  (( ((i_v * pCtx->i_cos - i_u * pCtx->i_sin - pCtx->i_y) >> 8) * 
                                pCtx->i_sat) >> 8) + 128;
        }
      );
      x+= 8;
    }

    while(x < src_linesize[1]) {
      i_u = *p_in_u++; 
      i_v = *p_in_v++; 
      if(pCtx->i_sat > 256) {
        *p_out_u++ = av_clip_uint8( (( ((i_u * pCtx->i_cos + i_v * pCtx->i_sin - pCtx->i_x) >> 8) * 
                                pCtx->i_sat) >> 8) + 128); 
        *p_out_v++ = av_clip_uint8( (( ((i_v * pCtx->i_cos - i_u * pCtx->i_sin - pCtx->i_y) >> 8) * 
                                pCtx->i_sat) >> 8) + 128);
      } else {
        *p_out_u++ = (( ((i_u * pCtx->i_cos + i_v * pCtx->i_sin - pCtx->i_x) >> 8) * 
                                pCtx->i_sat) >> 8) + 128; 
        *p_out_v++ = (( ((i_v * pCtx->i_cos - i_u * pCtx->i_sin - pCtx->i_y) >> 8) * 
                                pCtx->i_sat) >> 8) + 128;
      }
      x++;
    }

  }

}

static void filter_color_y(const COLOR_CTX_T *pCtx, 
                           const unsigned char *src_data[4], const int src_linesize[4],
                           unsigned char *dst_data[4], const int dst_linesize[4],
                           unsigned int height) {

  const uint8_t *p_in;
  uint8_t *p_out;
  unsigned int x, y;

  for(y = 0; y < height; y++) {
    x = 0;
    p_in = src_data[0] + (y * src_linesize[0]);
    p_out = dst_data[0] + (y * dst_linesize[0]);
      
    while(x < src_linesize[0] - 8) {
      ADJUST_8_TIMES( *p_out++ = pCtx->matrix_luma[ *p_in++] );
      x+= 8;
    }
    while(x < src_linesize[0]) {
      *p_out++ = pCtx->matrix_luma[ *p_in++ ];
      x++;
    }

  }

}


void *filter_color_init(int width, void *pArg) {
  IXCODE_FILTER_COLOR_T *pCfg = (IXCODE_FILTER_COLOR_T *) pArg;
  COLOR_CTX_T *pCtx = NULL;
  unsigned int idx;
  float f_hue;
  float f_gamma;
  int32_t i_cont;
  int32_t i_lum;

  // Defaults
  //pCfg->fContrast = 1.0f;
  //pCfg->fBrightness = 1.0f;
  //pCfg->fHue = 0.0f;
  //pCfg->fSaturation = 1.0f;
  //pCfg->fGamma = 1.0f;

  if(pCfg->fContrast < 0.0f) {
    pCfg->fContrast = 0.0f;
  } else if(pCfg->fContrast > 2.0f) {
    pCfg->fContrast = 2.0f;
  }

  if(pCfg->fBrightness < 0.0f) {
    pCfg->fBrightness = 0.0f;
  } else if(pCfg->fBrightness > 2.0f) {
    pCfg->fBrightness = 2.0f;
  }

  if(pCfg->fHue < 0.0f) {
    pCfg->fHue = 0.0f;
  } else if(pCfg->fHue > 360.0f) {
    pCfg->fHue = 360.0f;
  }

  if(pCfg->fSaturation < 0.0f) {
    pCfg->fSaturation = 0.0f;
  } else if(pCfg->fSaturation > 3.0f) {
    pCfg->fSaturation = 3.0f;
  }

  if(pCfg->fGamma < 0.01f) {
    pCfg->fGamma = 0.01f;
  } else if(pCfg->fGamma > 10.0f) {
    pCfg->fGamma = 10.0f;
  }

  LOG(X_DEBUG("Filter colors contrast: %.2f, brightness: %.2f, hue: %.2f, saturation:%.2f, gamma: %.2f"),
               pCfg->fContrast, pCfg->fBrightness, pCfg->fHue, pCfg->fSaturation, pCfg->fGamma);

  if(!(pCtx = calloc(1, sizeof(COLOR_CTX_T)))) {
    return NULL;
  }

  i_cont = (int32_t) (pCfg->fContrast * 255);
  i_lum = (int32_t) ((pCfg->fBrightness - 1.0f) * 255);
  f_hue = (float) (pCfg->fHue * M_PI / 180);
  pCtx->i_sat = (int) (pCfg->fSaturation * 256);
  f_gamma = 1.0f / pCfg->fGamma;
  i_lum += 128 - i_cont / 2;

  for(idx = 0; idx < 256; idx++) {
    pCtx->matrix_gamma[idx] = av_clip_uint8( pow(idx / 255.0, f_gamma) * 255.0);
  }

  for(idx = 0; idx < 256; idx++) {
    pCtx->matrix_luma[idx] = pCtx->matrix_gamma[av_clip_uint8( i_lum + i_cont * idx / 256)];
  }

  pCtx->i_sin = sin(f_hue) * 256;
  pCtx->i_cos = cos(f_hue) * 256;

  pCtx->i_x = ( cos(f_hue) + sin(f_hue) ) * 32768;
  pCtx->i_y = ( cos(f_hue) - sin(f_hue) ) * 32768;

  return pCtx;
}

void filter_color_free(void *pArg) {
  COLOR_CTX_T *pCtx = (COLOR_CTX_T *) pArg;

  free(pCtx);

}

int filter_color(void *pArg, const unsigned char *src_data[4], const int src_linesize[4],
                 unsigned char *dst_data[4], const int dst_linesize[4],
                 unsigned int width, unsigned int height) {

  COLOR_CTX_T *pCtx = (COLOR_CTX_T *) pArg;
  int rc = 0;

  //fprintf(stderr, "filter_color %d,%d, height:%d\n", src_linesize[0], dst_linesize[0], height);
  filter_color_y(pCtx, src_data, src_linesize, dst_data, dst_linesize, height);
  filter_color_uv(pCtx, src_data, src_linesize, dst_data, dst_linesize, height);

  return rc;
}

#endif // (XCODE_FILTER_COLOR) && (XCODE_FILTER_COLOR > 0)

#if defined(XCODE_FILTER_ROTATE) && (XCODE_FILTER_ROTATE > 0)

typedef struct ROTATE_CTX {
  int            degrees;
} ROTATE_CTX_T;


void *filter_rotate_init(int width, void *pArg) {
  IXCODE_FILTER_ROTATE_T *pCfg = (IXCODE_FILTER_ROTATE_T *) pArg;
  ROTATE_CTX_T *pCtx = NULL;

  switch(pCfg->degrees) {
    case -180:
    case -270:
    case -90:
    case 0:
    case 90:
    case 180:
    case 270:
      break;
    default:    
      LOG(X_ERROR("Invalid filter rotate value: %d"), pCfg->degrees);
      return NULL;
  }

  if(!(pCtx = calloc(1, sizeof(ROTATE_CTX_T)))) {
    return NULL;
  }


  pCtx->degrees= pCfg->degrees;

  LOG(X_DEBUG("Filter rotate degrees:%d"), pCtx->degrees);

  return pCtx;
}
void filter_rotate_free(void *pArg) {
  ROTATE_CTX_T *pCtx = (ROTATE_CTX_T *) pArg;

  free(pCtx);

}

static int filter_rotate_90(ROTATE_CTX_T *pCtx, const unsigned char *src_data[4], const int src_linesize[4],
                 unsigned char *dst_data[4], const int dst_linesize[4],
                 unsigned int width, unsigned int height) {

  unsigned int x, y;
  unsigned int ybase;
  unsigned int heightuv = height/2;
  unsigned int widthuv = width/2;

  for(y = 0; y < height; y++) {
    ybase = y * width;
    for(x = 0; x < width; x++) {
      dst_data[0][ybase + x] = src_data[0][(height * (width - x - 1)) + y];
    }
  }

  for(y = 0; y < heightuv; y++) {
    ybase = y * widthuv;
    for(x = 0; x < widthuv; x++) {
      dst_data[1][ybase + x] = src_data[1][(heightuv * (widthuv - x - 1)) + y];
    }
  }

  for(y = 0; y < heightuv; y++) {
    ybase = y * widthuv;
    for(x = 0; x < widthuv; x++) {
      dst_data[2][ybase + x] = src_data[2][(heightuv * (widthuv - x - 1)) + y];
    }
  }

  return 0;
}

static int filter_rotate_270(ROTATE_CTX_T *pCtx, const unsigned char *src_data[4], const int src_linesize[4],
                 unsigned char *dst_data[4], const int dst_linesize[4],
                 unsigned int width, unsigned int height) {

  unsigned int x, y;
  unsigned int ybase, yoffset;
  unsigned int heightuv = height/2;
  unsigned int widthuv = width/2;

  for(y = 0; y < height; y++) {
    ybase = y * width;
    yoffset = height - y - 1;
    for(x = 0; x < width; x++) {
      dst_data[0][ybase + x] = src_data[0][(x * height) + yoffset];
    }
  }

  for(y = 0; y < heightuv; y++) {
    ybase = y * widthuv;
    yoffset = heightuv - y - 1;
    for(x = 0; x < widthuv; x++) {
      dst_data[1][ybase + x] = src_data[1][(x * heightuv) + yoffset];
    }
  }

  for(y = 0; y < heightuv; y++) {
    ybase = y * widthuv;
    yoffset = heightuv - y - 1;
    for(x = 0; x < widthuv; x++) {
      dst_data[2][ybase + x] = src_data[2][(x * heightuv) + yoffset];
    }
  }

  return 0;
}

static int filter_rotate_180(ROTATE_CTX_T *pCtx, const unsigned char *src_data[4], const int src_linesize[4],
                 unsigned char *dst_data[4], const int dst_linesize[4],
                 unsigned int width, unsigned int height) {

  unsigned int x, y;
  unsigned int ybase, yoffset;
  unsigned int heightuv = height/2;
  unsigned int widthuv = width/2;

  for(y = 0; y < height; y++) {
    ybase = y * width;
    yoffset = width * (height - y - 1);
    for(x = 0; x < width; x++) {
      dst_data[0][ybase + x] = src_data[0][yoffset + (width - x - 1)];
    }
  }

  for(y = 0; y < heightuv; y++) {
    ybase = y * widthuv;
    yoffset = widthuv * (heightuv - y - 1);
    for(x = 0; x < widthuv; x++) {
      dst_data[1][ybase + x] = src_data[1][yoffset + (widthuv - x - 1)];
    }
  }

  for(y = 0; y < heightuv; y++) {
    ybase = y * widthuv;
    yoffset = widthuv * (heightuv - y - 1);
    for(x = 0; x < widthuv; x++) {
      dst_data[2][ybase + x] = src_data[2][yoffset + (widthuv - x - 1)];
    }
  }

  return 0;
}

int filter_rotate(void *pArg, const unsigned char *src_data[4], const int src_linesize[4],
                 unsigned char *dst_data[4], const int dst_linesize[4],
                 unsigned int width, unsigned int height) {

  ROTATE_CTX_T *pCtx = (ROTATE_CTX_T *) pArg;

  //fprintf(stderr, "FILTER ROTATE deg:%d src_ls:%d,%d, dst_ls:%d,%d, w:%d, h:%d, diff:%d,%d\n", pCtx->degrees, src_linesize[0], src_linesize[1], dst_linesize[0], dst_linesize[1], width, height, dst_data[1] - dst_data[0], dst_data[2] - dst_data[1]);

  switch(pCtx->degrees) {
    case 90:
    case -270:
      return filter_rotate_90(pCtx, src_data, src_linesize, dst_data, dst_linesize, width, height);
    case -90:
    case 270:
      return filter_rotate_270(pCtx, src_data, src_linesize, dst_data, dst_linesize, width, height);
    case 180:
    case -180:
      return filter_rotate_180(pCtx, src_data, src_linesize, dst_data, dst_linesize, width, height);
    default:
      return -1;
  }
}

#endif // (XCODE_FILTER_ROTATE) && (XCODE_FILTER_ROTATE > 0)

#if defined(XCODE_FILTER_TEST) && (XCODE_FILTER_TEST > 0)

typedef struct TEST_CTX {
  int            val;
} TEST_CTX_T;


void *filter_test_init(int width, void *pArg) {
  IXCODE_FILTER_TEST_T *pCfg = (IXCODE_FILTER_TEST_T *) pArg;
  TEST_CTX_T *pCtx = NULL;

  if(!(pCtx = calloc(1, sizeof(TEST_CTX_T)))) {
    return NULL;
  }

  pCtx->val = pCfg->val;

  LOG(X_DEBUG("Filter test value:%d"), pCtx->val);

  return pCtx;
}
void filter_test_free(void *pArg) {
  TEST_CTX_T *pCtx = (TEST_CTX_T *) pArg;

  free(pCtx);

}

int filter_test(void *pArg, const unsigned char *src_data[4], const int src_linesize[4],
                 unsigned char *dst_data[4], const int dst_linesize[4],
                 unsigned int width, unsigned int height) {

  TEST_CTX_T *pCtx = (TEST_CTX_T *) pArg;
  int rc = 0;
  unsigned int x, y;

  memset(dst_data[0], 0, width * height); 
  memset(dst_data[1], 0x80, width/2 * height/2);
  memset(dst_data[2], 0x80, width/2 * height/2);

  return rc;
}

#endif // (XCODE_FILTER_TEST) && (XCODE_FILTER_TEST > 0)
