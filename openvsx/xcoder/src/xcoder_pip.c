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
#include "logutil.h"
#include "ixcode.h"
#include "xcoder_pip.h"
#include "xcoder.h"
#include "vsxlib.h"


static void color_rgb2yuv(const unsigned char rgb[3], unsigned char yuv[3]);

static int pip_draw_grid(unsigned char *data[4], int linesize[4], int height,
                         unsigned int hcount, unsigned int vcount, int flags, unsigned int borderpx, 
                         IXCODE_VIDEO_CROP_T *pCrop, const unsigned char *colorYUV);

static int drawblock(unsigned char *data[4], int linesize[4], 
                     unsigned int topx, unsigned int topy,
                     unsigned int blockx, unsigned int blocky, uint8_t color) {

  unsigned int wd = linesize[0];
  unsigned int start =  (topy * wd) + topx;
  unsigned int end = ((topy + blocky) * wd) + topx + blockx;
  unsigned int pos, posx;

  //TODO: this should be in word sizes, not char sizes
  for(pos = start; pos < end; pos++) {
    posx = pos % wd;
    //TODO: use memset for line range
    if(posx >= topx && posx <= topx + blockx) {
      *((uint8_t *) (data[0] + pos)) = color;
    }
  }

  return 0;
}


int pip_mark(unsigned char *data[4], int linesize[4], unsigned int ht) {
  unsigned int wd = linesize[0];
  unsigned int sz = wd / 12;

  static uint8_t color;
  static int color_chg;

  if(color_chg == 0) {
    if((color += 4) == 0xff) {
      color_chg = 1;
    }
  } else {
    if((color -= 4) == 0x0) {
      color_chg = 0;
    }
  }

  drawblock(data, linesize, wd / 2 - sz / 2, ht / 2 - sz / 2, sz, sz, color);

  drawblock(data, linesize, wd / 2 - sz - sz / 2, ht / 2 - sz - sz / 2, sz, sz, color);
  drawblock(data, linesize, wd / 2 + sz - sz / 2, ht / 2 - sz - sz / 2, sz, sz, color);

  drawblock(data, linesize, wd / 2 - sz - sz / 2, ht / 2 + sz - sz / 2, sz, sz, color);
  drawblock(data, linesize, wd / 2 + sz - sz / 2, ht / 2 + sz - sz / 2, sz, sz, color);

  return 0;
}


static void blend_pixels_noalpha(unsigned char *dst_data[4], 
                         int dst_linesize[4], 
                         const unsigned char *src_data[4], 
                         const int src_linesize[4], 
                         const IXCODE_VIDEO_PIP_T *pPip,
                         unsigned int src_w,
                         unsigned int src_h,
                         int posx, 
                         int posy, 
                         int offsetx, 
                         int dst_w, 
                         int dst_h) {

  int i, j;
  int overlay_end_y;
  int slice_y = 0;
  int slice_end_y;
  int end_y, start_y;
  int width, height;
  int hsub = 0, vsub = 0;
  uint8_t *dp;
  const uint8_t *sp;
  int wp, hp;

  if(offsetx > src_w) {
    offsetx = src_w;
  }

  overlay_end_y = posy + src_h;
  slice_end_y = slice_y + dst_h;
  end_y = MIN(slice_end_y, overlay_end_y);
  start_y = MAX(posy, slice_y);
  width = MIN(dst_w - posx - offsetx, src_w - offsetx) ;
  height = end_y - start_y;

  wp = ALIGN(width, 1<<hsub) >> hsub;
  hp = ALIGN(height, 1<<vsub) >> vsub;

  for (i = 0; i < 3; i++) {

    hsub = i ? 1 : 0; // 1 for 422 chroma values
    vsub = i ? 1 : 0; // 1 for 422 chroma values
    dp = dst_data[i] + (posx >> hsub) + (start_y >> vsub) * dst_linesize[i];
    sp = src_data[i] + (i ? offsetx/2 : offsetx);
    wp = ALIGN(width, 1<<hsub) >> hsub;
    hp = ALIGN(height, 1<<vsub) >> vsub;

    if(slice_y > posy) {
      sp += ((slice_y - posy) >> vsub) * src_linesize[i];
    }

    for(j = 0; j < hp; j++) {
      memcpy(dp, sp, wp);
      dp += dst_linesize[i];
      sp += src_linesize[i];

    } // end of for j

  } // end of for i

}

static void blend_pixels(unsigned char *dst_data[4], 
                         int dst_linesize[4], 
                         const unsigned char *src_data[4], 
                         const int src_linesize[4], 
                         const IXCODE_VIDEO_PIP_T *pPip,
                         unsigned int src_w,
                         unsigned int src_h,
                         int posx, 
                         int posy, 
                         int offsetx, 
                         int dst_w, 
                         int dst_h,
                         int alphamax_min1) {

  int i, j, k;
  int overlay_end_y;
  int slice_y = 0;
  int slice_end_y;
  int end_y, start_y;
  int width, height;
  int hsub, vsub;
  uint8_t *dp, *d;
  const uint8_t *sp, *ap, *s, *a;
  int wp, hp;
  int alpha_v, alpha_h, alpha;

  if(offsetx > src_w) {
    offsetx = src_w;
  }

  overlay_end_y = posy + src_h;
  slice_end_y = slice_y + dst_h;
  end_y = MIN(slice_end_y, overlay_end_y);
  start_y = MAX(posy, slice_y);
  width = MIN(dst_w - posx - offsetx, src_w - offsetx) ;
  height = end_y - start_y;

//fprintf(stderr, "W:%d H:%d, x:%d y:%d\n", width, height, posx, posy);
//fprintf(stderr, "blend x:%d, y:%d, w:%d, h:%d, slice_y:%d, slice_w:%d, slice_h:%d, width:%d, height:%d, start_y:%d, end_y:%d src{%d,%d,%d,%d}, dst{ %d,%d,%d,%d}\n", posx, posy, src_w, src_h, slice_y, dst_w, dst_h, width, height, start_y, end_y,  src->linesize[0], src->linesize[1], src->linesize[2], src->linesize[3], dst->linesize[0], dst->linesize[1], dst->linesize[2], dst->linesize[3]);

  //
  // assuming src YUVA420p -> YUV420p
  //

  for (i = 0; i < 3; i++) {

    hsub = i ? 1 : 0; // 1 for 422 chroma values
    vsub = i ? 1 : 0; // 1 for 422 chroma values
    dp = dst_data[i] + (posx >> hsub) + (start_y >> vsub) * dst_linesize[i];
    sp = src_data[i] + (i ? offsetx/2 : offsetx);
    ap = src_data[3] + offsetx;
    wp = ALIGN(width, 1<<hsub) >> hsub;
    hp = ALIGN(height, 1<<vsub) >> vsub;

    if(slice_y > posy) {
      sp += ((slice_y - posy) >> vsub) * src_linesize[i];
      ap += (slice_y - posy) * src_linesize[3];
    }

    for(j = 0; j < hp; j++) {

      d = dp;
      s = sp;
      a = ap;

      for (k = 0; k < wp; k++) {

        if(hsub && vsub && j+1 < hp && k+1 < wp) {
	  alpha = (a[0] + a[src_linesize[3]] + a[1] + a[src_linesize[3]+1]) >> 2;
	} else if (hsub || vsub) {
	  alpha_h = hsub && k+1 < wp ?  (a[0] + a[1]) >> 1 : a[0];
          alpha_v = vsub && j+1 < hp ?  (a[0] + a[src_linesize[3]]) >> 1 : a[0];
          alpha = (alpha_v + alpha_h) >> 1;
        } else {
          alpha = a[0];
        }

        if(alphamax_min1 == 0 && pPip->alphamax_min1 > 0) {
          alphamax_min1 = pPip->alphamax_min1;
        }
        if(alphamax_min1 != 0 && alpha >= alphamax_min1) {
          alpha = alphamax_min1 - 1;
        } else if(pPip->alphamin_min1 != 0 && alpha < pPip->alphamin_min1) {
          alpha = pPip->alphamin_min1 - 1;
        } 

	*d = (*d * (0xff - alpha) + *s++ * alpha + 128) >> 8;
	d++;
	a += 1 << hsub;

      } // end of for k

      dp += dst_linesize[i];
      sp += src_linesize[i];
      ap += (1 << vsub) * src_linesize[3];

    } // end of for j

  } // end of for i

}

int pip_add(IXCODE_VIDEO_CTXT_T *pXcode, unsigned int outidx, struct AVFrame *pframesPipIn[PIP_ADD_MAX], struct AVFrame *pframesPipOut[PIP_ADD_MAX]) {

  int rc = 0;
  int rcmtx;
  unsigned char *p;
  IXCODE_AVCTXT_T *pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;
  unsigned int pipframeidx = 0; 

  if( pAvCtx->out[outidx].dim_pips[0].height <= 0) {
    LOG(X_ERROR("PIP height not set!"));
  }

  //pthread_mutex_lock(pAvCtx->pframepipmtx);
  rcmtx = pthread_mutex_trylock(pAvCtx->pframepipmtx);

  if(rcmtx == 0) {

    for(pipframeidx = 0; pipframeidx < PIP_ADD_MAX; pipframeidx++) {

      if(!pframesPipIn[pipframeidx] || !pframesPipOut[pipframeidx]) {
        break;
      }

      PIPXLOG("PIP_ADD outidx[%d], data[0]:0x%x, linesize:%d, framespip[%d].data:0x%x, data:0x%x, %dx%d", outidx, pframesPipIn[pipframeidx]->data[0], pframesPipIn[pipframeidx]->linesize[0], pipframeidx, pAvCtx->out[outidx].framespip[pipframeidx].data, pframesPipIn[pipframeidx]->data, pAvCtx->out[outidx].dim_pips[pipframeidx].width, pAvCtx->out[outidx].dim_pips[pipframeidx].height);

      memcpy(pAvCtx->out[outidx].framespip[pipframeidx].data, pframesPipIn[pipframeidx]->data, PIX_PLANE_DATA_SZ);
      memcpy(pAvCtx->out[outidx].framespip[pipframeidx].linesize, pframesPipIn[pipframeidx]->linesize, PIX_PLANE_LINESIZE_SZ);

      p = pframesPipIn[pipframeidx]->data[0];

      xcode_fill_yuv420p(pframesPipOut[pipframeidx]->data, pframesPipOut[pipframeidx]->linesize, 
                         pAvCtx->piprawDataBufs[pipframeidx], pAvCtx->out[outidx].dim_pips[pipframeidx].height);

      memcpy(pframesPipIn[pipframeidx]->data, pframesPipOut[pipframeidx]->data, PIX_PLANE_DATA_SZ);
      memcpy(pframesPipIn[pipframeidx]->linesize, pframesPipOut[pipframeidx]->linesize, PIX_PLANE_LINESIZE_SZ);

      pAvCtx->piprawDataBufs[pipframeidx] = p;

      xcode_fill_yuv420p(pframesPipIn[pipframeidx]->data, pframesPipIn[pipframeidx]->linesize, 
                         pframesPipIn[pipframeidx]->data[0], pAvCtx->out[outidx].dim_pips[pipframeidx].height);
      xcode_fill_yuv420p(pAvCtx->out[outidx].framespip[pipframeidx].data, pAvCtx->out[outidx].framespip[pipframeidx].linesize, 
                          pAvCtx->out[outidx].framespip[pipframeidx].data[0], pAvCtx->out[outidx].dim_pips[pipframeidx].height);

    }

    //
    // Counter showing how many frames were stored by this PIP before being cleared when an overlay is called
    //
    pAvCtx->out[outidx].pipFrameCnt++;

    pthread_mutex_unlock(pAvCtx->pframepipmtx);

    //fprintf(stderr, "PIP UPDATED 0x%x (%d,%d,%d) ls:%d,%d,%d,%d a: 0x%x (0x%x 0x%x)\n", pAvCtx->framespip[0].data[0], pAvCtx->framespip[0].data[1] - pAvCtx->framespip[0].data[0], pAvCtx->framespip[0].data[2] - pAvCtx->framespip[0].data[1], pAvCtx->framespip[0].data[3] - pAvCtx->framespip[0].data[2], pAvCtx->framespip[0].linesize[0], pAvCtx->framespip[0].linesize[1], pAvCtx->framespip[0].linesize[2], pAvCtx->framespip[0].linesize[3], pAvCtx->framespip[0].data[3], pAvCtx->framespip[0].data[3][0], pAvCtx->framespip[0].data[3][32]);
  } else {
    //fprintf(stderr, "PIP MTX IS LOCKED\n");
    LOG(X_DEBUG("pip_add mutex is locked for outidx[%d]"), outidx);
  }

  return rc;
}

static void get_pip_pos(IXCODE_PIP_MOTION_T *pMotion, int *px, int *py, 
                        int *palphamax_min1, int dst_w, int dst_h, 
                        unsigned int pip_width, unsigned int pip_height) {
  int x, y, alphamax_min1;
  int w, h;
  int x0, x1, y0, y1;
  int nextmv = 0;
  float dist, f;
  IXCODE_PIP_MOTION_VEC_T *pMv;

  if(!(pMv = pMotion->pmCur)) {
    pMv = pMotion->pmCur = pMotion->pm;
  }

  if((pMv->flags & PIP_FLAGS_MOTION_X0_RIGHT)) {
    x0 = dst_w - pMv->x0 - pip_width;
  } else {
    x0 = pMv->x0;
  }
  if((pMv->flags & PIP_FLAGS_MOTION_X1_RIGHT)) {
    x1 = dst_w - pMv->x1 - pip_width;
  } else {
    x1 = pMv->x1;
  }
  if((pMv->flags & PIP_FLAGS_MOTION_Y0_BOTTOM)) {
    y0 = dst_h - pMv->y0 - pip_height;
  } else {
    y0 = pMv->y0;
  }
  if((pMv->flags & PIP_FLAGS_MOTION_Y1_BOTTOM)) {
    y1 = dst_h - pMv->y1 - pip_height;
  } else {
    y1 = pMv->y1;
  }

  //
  // Get the current value of the x and y position
  //
  if(x1 != x0 || y1 != y0) {

    w = x1 - x0;
    h = y1 - y0;

    dist = sqrt(abs(w * w) + abs(h * h));
    f = pMv->pixperframe * pMotion->indexInVec;

    x = (f/dist) * w + x0;
    y = (f/dist) * h + y0;

    if((w > 0 && x > x1) ||
       (w < 0 && x < x1)) {
      x = x1; 
    }
    if((h > 0 && y > y1) ||
       (h < 0 && y < y1)) {
      y = y1; 
    }

    if(f >= dist) {
      nextmv |= 1;
    }

  } else {
    x = x0; 
    y = y0; 
    nextmv |= 1;
  }

  //
  // Get the current value of the transitioning alpha mask
  //
  if(palphamax_min1 && pMv->alphamax1_min1 != pMv->alphamax0_min1) {

    dist = (int) pMv->alphamax1_min1 - (int) pMv->alphamax0_min1;
    f = pMv->alphaperframe * pMotion->indexInVec;
    alphamax_min1 = (f/abs(dist)) * dist + pMv->alphamax0_min1;

    if(alphamax_min1 > 255) {
      alphamax_min1 = 255;
    }
    if(alphamax_min1 < 0) {
      alphamax_min1 = 0;
    }

    *palphamax_min1 = alphamax_min1;

    if(f >= abs(dist)) {
      nextmv |= 2;
    }

  } else {
    nextmv |= 2;
  }

  pMotion->indexInVec++;
//fprintf(stderr, "X:%d, Y:%d, nextmv:%d, frames:%d, idx:%d\n", x, y, nextmv, pMotion->pmCur->frames, pMotion->indexInVec);

  //
  // Go to the next pip directive (linked list node)
  //
  if((pMotion->pmCur->frames == 0 || pMotion->indexInVec > pMotion->pmCur->frames) &&
     nextmv == 3) {

    pMotion->indexInVec = 0; 
    pMotion->pmCur = pMotion->pmCur->pnext;

    if(!(pMotion->pmCur)) {
      pMotion->pmCur = pMotion->pm;
    }
  }
  
  *px = x;
  *py = y;

}

int pip_create_overlay(IXCODE_VIDEO_CTXT_T *pXcode, unsigned char *data[4], 
                       int linesize[4], const unsigned int outidx, 
                       unsigned int dst_w, unsigned int dst_h, int lock) {

  int numBlended = 0;
  //int numBlendedAutoLayout = 0;
  unsigned int idx;
  int posx, posy;
  int offsetx;
  int alphamax_min1;
  unsigned int inidx = outidx;
  unsigned int pipframeidx = 0;
  unsigned int pipWidth, pipHeight;
  IXCODE_VIDEO_PIP_T *pPip;
  IXCODE_VIDEO_CTXT_T *pPipX;
  IXCODE_AVCTXT_T *pPipAvCtx;
  IXCODE_AVCTXT_T *pAvCtx = (IXCODE_AVCTXT_T *) pXcode->common.pPrivData;

  //TODO: fix multiple xcode output instead of pip being placed into xcode[0]

  //
  // Currently, only 1 pip image is produced even if using multilpe output encodings
  // This is a bit of an optimization to prevent scaling mulitple pip images for each output frame
  //
  inidx = 0;

  //if(pXcode->out[0].passthru) {
  //  return;
  //}
  
  if(lock) {
    pthread_mutex_lock(&pXcode->overlay.mtx);
  }

  for(idx = 0; idx < MAX_PIPS; idx++) {

    //
    // PIPs should already be ordered according to z-order property
    //
    if((pPipX = pXcode->overlay.p_pipsx[idx])) {
      pPip = &pPipX->pip;
      pPipAvCtx = (IXCODE_AVCTXT_T *) pPip->pPrivData;
    } else {
      pPip = NULL;
      pPipAvCtx = NULL;
    }

    if((pipframeidx = pXcode->overlay.pipframeidxs[idx]) >= PIP_ADD_MAX) {
      pipframeidx = 0;
    }

    if(!pPipAvCtx || !pPipAvCtx->out[inidx].framespip[pipframeidx].data[0] || !pPipAvCtx->pframepipmtx ||
       !pPip->active || pPip->visible <= 0 || !pPip->showVisual) {
      continue;
    //} else if(pPip->autoLayout && pXcode->overlay.maxAutoLayoutDisplayPips > 0 && 
    //          numBlendedAutoLayout >= pXcode->overlay.maxAutoLayoutDisplayPips) {
    //  continue;
    }

    //LOG(X_DEBUG("PIP CREATE OVERLAY idx:%d 0x%x"), idx, &pXcode->overlay);

    pipWidth = pPipAvCtx->out[0].dim_pips[pipframeidx].width; 
    pipHeight = pPipAvCtx->out[0].dim_pips[pipframeidx].height; 

    if(pPipAvCtx->piphavepos0 &&
       (pPipAvCtx->pipposxCfg != pPip->pos[pipframeidx].posx ||
       (pPipAvCtx->pipposyCfg != pPip->pos[pipframeidx].posy))) {
      pPipAvCtx->piphavepos0 = 0;
    }

    //LOG(X_DEBUG("ADD_OVERLAY %s src:%dx%d dst:%dx%d pPipAvCtx:0x%x"), (pPip->flags & PIP_FLAGS_INSERT_BEFORE_SCALE) ? "before" : "after", pipWidth, pipHeight, dst_w, dst_h, pPipAvCtx);
    if(!pPipAvCtx->piphavepos0) {

      pPipAvCtx->pipposxCfg = pPip->pos[pipframeidx].posx;
      pPipAvCtx->pipposyCfg = pPip->pos[pipframeidx].posy;

      if((pPip->flags & PIP_FLAGS_POSITION_RIGHT)) {
	pPipAvCtx->pipposx0 = dst_w - pPip->pos[pipframeidx].posx - pipWidth;
      } else {
	pPipAvCtx->pipposx0 = pPip->pos[pipframeidx].posx;
      }
      if((pPip->flags & PIP_FLAGS_POSITION_BOTTOM)) {
	pPipAvCtx->pipposy0 = dst_h - pPip->pos[pipframeidx].posy - pipHeight;
      } else {
	pPipAvCtx->pipposy0 = pPip->pos[pipframeidx].posy;
      }
      pPipAvCtx->pipposx = pPipAvCtx->pipposx0;
      pPipAvCtx->pipposy = pPipAvCtx->pipposy0;
      pPipAvCtx->piphavepos0 = 1;
    }

    offsetx = 0;
    if((posx = pPipAvCtx->pipposx) < 0) {
      offsetx -= pPipAvCtx->pipposx;
      posx = 0;
    }

    posy = pPipAvCtx->pipposy;
    alphamax_min1 = 0;

    //
    // If PIP motion properties are defined, get the current placement coordinates
    //
    if(pPip->pMotion && pPip->pMotion->active && pPip->pMotion->pm) {
      get_pip_pos(pPip->pMotion, &posx, &posy, &alphamax_min1, dst_w, dst_h, pipWidth, pipHeight);
    }

    pthread_mutex_lock(pPipAvCtx->pframepipmtx);

    PIPXLOG("PIPX BLEND pipframeidx[%d], data[0]:0x%x idx[%d] dst_lsz:%d,%d,%d,%d, src_lsz:0x%x %d,%d,%d,%d, at x:%d, y:%d, offsetx:%d, dst:%dx%d, src:%dx%d, alpha:%d, pipFrameCnt:%d", pipframeidx, data[0], idx, linesize[0], linesize[1], linesize[2], linesize[3], &pPipAvCtx->out[inidx].framespip[pipframeidx], pPipAvCtx->out[inidx].framespip[pipframeidx].linesize[0], pPipAvCtx->out[inidx].framespip[pipframeidx].linesize[1], pPipAvCtx->out[inidx].framespip[pipframeidx].linesize[2], pPipAvCtx->out[inidx].framespip[pipframeidx].linesize[3], posx, posy, offsetx, dst_w, dst_h, pipWidth, pipHeight,  pPip->haveAlpha, pPipAvCtx->out[inidx].pipFrameCnt);

    if(pPip->haveAlpha || pPip->alphamax_min1 > 0 || alphamax_min1 > 0 || pPip->alphamin_min1 > 0) {

      blend_pixels(data, 
                 linesize, 
                 (const unsigned char **) pPipAvCtx->out[inidx].framespip[pipframeidx].data, 
		 pPipAvCtx->out[inidx].framespip[pipframeidx].linesize,
		 pPip, 
                 pipWidth,
                 pipHeight,
		 posx, 
                 posy, 
                 offsetx, 
                 dst_w, 
                 dst_h, 
                 alphamax_min1);

    } else {
      blend_pixels_noalpha(data, 
                 linesize, 
                 (const unsigned char **) pPipAvCtx->out[inidx].framespip[pipframeidx].data, 
		 pPipAvCtx->out[inidx].framespip[pipframeidx].linesize,
		 pPip, 
                 pipWidth,
                 pipHeight,
		 posx, 
                 posy, 
                 offsetx, 
                 dst_w, 
                 dst_h);
    }

    //fprintf(stderr, "BLEND DONE idx[%d]\n", idx);

    //
    // pipFrameCnt is the number of PIP images accumulated before this overlay clears the buffer 
    //
    if(pPipAvCtx->out[inidx].havepipFrameCntPrior) {
      if(pPipAvCtx->out[inidx].pipFrameCntPrior + 1 < pPipAvCtx->out[inidx].pipFrameCnt ||
        pPipAvCtx->out[inidx].pipFrameCnt + 1 < pPipAvCtx->out[inidx].pipFrameCntPrior) {
        //TODO: should create pip_add frame queue of at least 1 frame... 
        //LOG(X_DEBUG("pip_create_overlay outidx[%d] pip.id:%d may be dropping frames. framecnt:%d, prior:%d"), outidx, pPip->id, pPipAvCtx->out[inidx].pipFrameCnt, pPipAvCtx->out[inidx].pipFrameCntPrior);
      }
    }
    pPipAvCtx->out[inidx].pipFrameCntPrior = pPipAvCtx->out[inidx].pipFrameCnt;
    pPipAvCtx->out[inidx].havepipFrameCntPrior = 1;
    pPipAvCtx->out[inidx].pipFrameCnt = 0;

    pthread_mutex_unlock(pPipAvCtx->pframepipmtx);

    //if(pPip->autoLayout) {
    //  numBlendedAutoLayout++;
    //}
    numBlended++;

  } // end of for

  //
  // Draw any grid border
  //
  if(pXcode->overlay.border.active) {

    if(!pAvCtx->out[outidx].setBorderColor || 
       memcmp(pAvCtx->out[outidx].colorBorderRGB, pXcode->overlay.border.colorRGB, 3)) {
      memcpy(pAvCtx->out[outidx].colorBorderRGB, pXcode->overlay.border.colorRGB, 3);
      color_rgb2yuv(pAvCtx->out[outidx].colorBorderRGB, pAvCtx->out[outidx].colorBorderYUV);
      pAvCtx->out[outidx].setBorderColor = 1;
    }

    //LOG(X_DEBUG("DRAWING GRID BORDER 0x%x %dx%d, edge:%d, pad:l:%d,t:%d,r%d,b:%d"), pXcode, pXcode->overlay.border.hBlocks, pXcode->overlay.border.vBlocks, pXcode->overlay.border.edgePx, pXcode->out[outidx].crop.padLeft, pXcode->out[outidx].crop.padTop, pXcode->out[outidx].crop.padRight, pXcode->out[outidx].crop.padBottom);
   
    pip_draw_grid(data, linesize, dst_h, pXcode->overlay.border.hBlocks, pXcode->overlay.border.vBlocks, 
                  pXcode->overlay.border.flags, pXcode->overlay.border.edgePx, 
                  &pXcode->out[outidx].crop, pAvCtx->out[outidx].colorBorderYUV);
  }

  if(lock) {
    pthread_mutex_unlock(&pXcode->overlay.mtx);
  }

  return numBlended;
}

static void color_rgb2yuv(const unsigned char rgb[3], unsigned char yuv[3]) {
  const float Wr = .299f;
  const float Wb = .114f;
  const float Wg = .587f;
  const float Umax = .436f;
  const float Vmax = .615;
  float u,v;
  int i;

  //
  // Convert a single RGB color value to YUV
  //

  yuv[0] = Wr * rgb[0] + Wg * rgb[1] + Wb * rgb[2];
  u = Umax * ((rgb[2] - yuv[0])/256.0f) / (1 - Wb);
  v = Vmax * ((rgb[0] - yuv[0])/256.0f) / (1 - Wr);

  u = MIN(u, .5);
  u = MAX(u, -.5f);
  v = MIN(v, .5);
  v = MAX(v, -.5f);

  if((i = (u * 256) + 128) > 255) {
    i = 255;
  }
  yuv[1] = i;
  if((i = (v * 256) + 128) > 255) {
    i = 255;
  }
  yuv[2] = i;

}

static void copy_lines(unsigned char *pdst, const unsigned char *psrc,
                       unsigned int ht, unsigned int padleft, 
                       unsigned int width, unsigned int padright,
                       unsigned char fillcolor) {
  unsigned int line; 
  const unsigned int wdest = width + padleft + padright;

  //
  // Copy a series of strides, including padding on left and right
  //

  for(line = 0; line < ht; line++) {
    if(padleft > 0) {
      memset(&pdst[line * wdest], fillcolor, padleft);
    }
    if(psrc && width > 0) {
      memcpy(&pdst[padleft + (line * wdest)], &psrc[line * width], width);
    }
    if(padright > 0) {
      memset(&pdst[padleft + width + (line * wdest)], fillcolor, padright);
    }
  }
}

static int grow_canvas(const unsigned char *src_data[4], const int src_linesize[4],
                       unsigned char *dst_data[4], int dst_linesize[4],
                       unsigned int w0, unsigned int padleft, 
                       unsigned int padright, unsigned int h0, 
                       unsigned int padtop, unsigned int padbot,
                       const unsigned char padrgb[3]) {
  unsigned int len;
  unsigned char *p;
  int size = 0;
  int width = src_linesize[0] + padleft + padright;
  unsigned char fillyuv[3] = { 0x00, 0x80, 0x80 };

  color_rgb2yuv(padrgb, fillyuv);

//fill[0]=0x80; fill[1]=0xff; fill[2]=0x0; fprintf(stderr, "YUV post 0x%x 0x%x 0x%x\n", fill[0], fill[1], fill[2]);
 //fprintf(stderr, "GROW x: %d+%d+%d y: %d+%d+%d ls:%d,%d\n", padleft, w0, padright, padtop, h0, padbot, src_linesize[0], dst_linesize[0]);

  //
  // resize luma (Y)
  //
  len = 0;
  p = dst_data[0];

  if(padtop) {
    len = width * padtop;
    memset(p, fillyuv[0], len);
  }
  if(h0 > 0) {
    if(padleft + padright > 0) {
      copy_lines(&p[len], src_data[0], h0, padleft, src_linesize[0], padright, fillyuv[0]);
    } else {
      memcpy(&p[len], src_data[0], width * h0);
    }
    len += (width * h0);
  }
  if(padbot) {
    memset(&p[len], fillyuv[0], width * padbot);
    len += (width * padbot);
  }
  size = len;

  //
  // resize chroma Blue (U)
  //
  len = 0;
  dst_data[1] = &p[size];
  if(padtop) {
    len = (width/2) * (padtop/2);
    memset(&p[size], fillyuv[1], len);
  }
  if(h0 > 0) {
    if(padleft + padright > 0) {
      copy_lines(&p[size + len], src_data[1], h0/2, padleft/2, src_linesize[0]/2, padright/2, fillyuv[1]);
    } else {
      memcpy(&p[size + len], src_data[1], width/2 * h0/2);
    }
    len += ((width/2) * (h0/2));
  }
  if(padbot) {
    memset(&p[ size + len ], fillyuv[1], width/2 * padbot/2);
    len += ((width/2) * (padbot/2));
  }
  size += len;

  //
  // resize chroma Red (V)
  //
  len = 0;
  dst_data[2] = &p[size];
  if(padtop) {
    len = (width/2) * (padtop/2);
    memset(&p[size], fillyuv[2], len);
  }
  if(h0 > 0) {
    if(padleft + padright > 0) {
      copy_lines(&p[size + len], src_data[2], h0/2, padleft/2, src_linesize[0]/2, padright/2, fillyuv[2]);
    } else {
      memcpy(&p[size + len], src_data[2], width/2 * h0/2);
    }
    len += ((width/2) * (h0/2));
  }
  if(padbot) {
    memset(&p[ size + len  ], fillyuv[2], width/2 * padbot/2);
    len += ((width/2) * (padbot/2));
  }
  size += len;

  dst_data[3] = NULL;
  dst_linesize[3] = 0;
  if(padleft > 0 || padright > 0) {
    dst_linesize[0] = width;
    dst_linesize[1] = dst_linesize[2] = width / 2;
  }

  return size;
}

static int crop_canvas(const unsigned char *src_data[4], const int src_linesize[4],
                       unsigned char *dst_data[4], int dst_linesize[4],
                       unsigned int w0, unsigned int cropleft, 
                       unsigned int cropright, unsigned int h0, 
                       unsigned int croptop, unsigned int cropbot) {
  unsigned int len;
  unsigned char *p;
  unsigned int line;
  int size = 0;
  int width = w0 - cropleft - cropright;
  int height = h0 - croptop - cropbot;

  //
  // resize luma (Y)
  //
  len = 0;
  p = dst_data[0];

  if(croptop > 0) {
    len = src_linesize[0] * croptop;
  }
  if(cropleft + cropright ==  0 && src_linesize[0] == w0) {
    memcpy(&p[0], &src_data[0][len], width * height);
  } else {
    for(line = 0; line < height; line++) {
      memcpy(&p[width * line], &src_data[0][ len + cropleft + src_linesize[0] * line], 
             width);
    }
  }
  size = width * height;


  //
  // resize chroma Blue (U)
  //
  len = 0;
  dst_data[1] = &p[size];
  if(croptop > 0) {
    len = src_linesize[1] * (croptop/2);
  }
  if(cropleft + cropright == 0 && src_linesize[1] == w0/2) {
    memcpy(&p[size], &src_data[1][len], width/2 * height/2);
  } else {
    for(line = 0; line < height/2; line++) {
      memcpy(&p[size + (width/2 * line)], 
             &src_data[1][ len + cropleft/2 + src_linesize[1] * line], 
             width/2);
    }
  }
  size += (width/2 * height/2);

  //
  // resize chroma Red (V)
  //
  len = 0;
  dst_data[2] = &p[size];
  if(croptop > 0) {
    len = src_linesize[2] * (croptop/2);
  }
  if(cropleft + cropright == 0 && src_linesize[2] == w0/2) {
    memcpy(&p[size], &src_data[2][len], width/2 * height/2);
  } else {
    for(line = 0; line < height/2; line++) {
      memcpy(&p[size + (width/2 * line)], 
             &src_data[2][ len + cropleft/2 + src_linesize[2] * line], 
             width/2);
    }
  }
  size += (width/2 * height/2);

  dst_data[3] = NULL;
  dst_linesize[3] = 0;
  dst_linesize[0] = width;
  dst_linesize[1] = dst_linesize[2] = width / 2;

  return size;
}

/*
int pip_fill(unsigned char *data[4], int linesize[4], int width, int height, const unsigned char *rgb) {
  int rc = 0;

  rc = grow_canvas(NULL, NULL, data, linesize, 0, 0, 0, 0, height, 0, rgb);

  return rc;
}
*/

int pip_fill_rect(unsigned char *data[4], int linesize[4], unsigned int width, unsigned int height, 
                  unsigned int startx, unsigned int starty, unsigned int fillwidth, unsigned int fillheight, 
                  const unsigned char *rgb) {
  int rc = 0;
  int y;
  int endy;
  unsigned char fill[3] = { 0x00, 0x80, 0x80 };

  if(!data[0] || !data[1] || !data[2] || !linesize[0] || !linesize[1] || !linesize[2]) {
    return -1;
  }

  color_rgb2yuv(rgb, fill);

  if(startx > width) {
    startx = width;
  }
  if(starty > height) {
    starty = height;
  }
  if(startx + fillwidth > width) {
    fillwidth = width - startx;
  }

  endy = MIN(height, starty + fillheight);

  //fprintf(stderr, "PIP_FILL: tid:0x%x %dx%d, startx:%d, starty:%d, endy:%d, ls:%d,%d, fill: 0x%x 0x%x 0x%x, rgb: 0x%x 0x%x 0x%x\n", pthread_self(), width, height, startx, starty, endy, linesize[0], linesize[1], fill[0], fill[1], fill[2], rgb[0], rgb[1], rgb[2]);

  for(y = starty; y < endy; y++) {
    memset(&(data[0][linesize[0] * y + startx]), fill[0], fillwidth);
  }

  startx /= 2;
  starty /= 2;
  endy /= 2;
  fillwidth /= 2;

  for(y = starty; y < endy; y++) {
    memset(&(data[1][linesize[1] * y + startx]), fill[1], fillwidth);
  }

  for(y = starty; y < endy; y++) {
    memset(&(data[2][linesize[2] * y + startx]), fill[2], fillwidth);
  }

  return rc;
}

int pip_resize_canvas(int pad, 
                      const unsigned char *src_data[4], const int src_linesize[4],
                      unsigned char *dst_data[4], int dst_linesize[4],
                      int w0, int left, int right,
                      int h0, int top, int bot, const unsigned char *rgb) {
  int rc = 0;
  unsigned int i;
  int *p[4];

  // assuming yuv420p

  p[0] = &top;
  p[1] = &bot;
  p[2] = &left;
  p[3] = &right;

  for(i = 0; i < 4; i++) {
    if(*(p[i]) < 0) {
      *(p[i]) = 0; 
    }
    if(*(p[i]) & 0x01) {
      (*p[i])--;
    }
  }

  if(top == 0 && bot == 0 && left == 0 && right == 0) {
    return 0;
  }

  if(pad) {
    rc = grow_canvas(src_data, src_linesize, dst_data, dst_linesize, 
                     w0, left, right, h0, top, bot, rgb);
  } else {

    if(top + bot > h0) {
      if(top > h0) {
        top = h0; 
      }
      if(bot > h0) {
        bot = h0; 
      }
      top = h0 - bot; 
    }
    if(left + right > w0) {
      if(left > w0) {
        left = w0; 
      }
      if(right > w0) {
        right = w0; 
      }
      right = w0 - left; 
    }

    rc = crop_canvas(src_data, src_linesize, dst_data, dst_linesize, 
                     w0, left, right, h0, top, bot);
  }

  return rc;
}

static int pip_draw_grid(unsigned char *data[4], int linesize[4], int origHeight,
                         unsigned int hcount, unsigned int vcount, int flags, unsigned int borderpx, 
                         IXCODE_VIDEO_CROP_T *pCrop, const unsigned char *colorYUV) {
  int rc = 0;
  int y, countidx, ystart, yend;
  int width, height;
  unsigned int hcountcur;
  unsigned int hcountaty[16];
  int vblocksz;

  ystart = pCrop->padTop;
  yend = origHeight - pCrop->padBottom;
  height = origHeight - (pCrop->padTop + pCrop->padBottom);

  //TODO: implement padLeft, padRight

  if(!data[0] || !data[1] || !data[2] || !linesize[0] || !linesize[1] || !linesize[2] || 
     height <= 0 || hcount < 1 || vcount < 1 || borderpx < 1 || hcount >= 16 || vcount >= 16) {
    return -1;
  }

  for(countidx = 0; countidx < vcount; countidx++) {
    hcountaty[countidx] = hcount;
  }

  if(hcount >= 2 && vcount >= 2) { 
    if(flags & PIP_BORDER_FLAG_TOP_MIN1) {
        hcountaty[0]--;
    }
    if(flags & PIP_BORDER_FLAG_BOTTOM_MIN1) {
        hcountaty[vcount-1]--;
    }
  }


  //color_rgb2yuv(rgb, colorYUV);

  width = linesize[0];

  //
  // Draw Y
  //

  //
  // Top most horizontal border
  //
  memset(&data[0][width * ystart], colorYUV[0], width * borderpx);

  //
  // Each inner horizontal border
  //
  vblocksz = height/vcount;
  for(countidx = 1; countidx < vcount; countidx++) {
    memset(&data[0][(ystart * width) + (countidx * vblocksz * width) - (width * borderpx/2)], colorYUV[0], width * borderpx);
  }
  for(y = ystart + 1; y < yend - 1; y++) {

    hcountcur = hcountaty[ (y - ystart) / vblocksz ];

    //
    // Left vertical border
    //
    memset(&data[0][y * width], colorYUV[0], borderpx);

    //
    // Each inner vertical border
    //
    for(countidx = 1; countidx < hcountcur; countidx++) {
      memset(&data[0][(y * width) + countidx * (width/hcountcur) - borderpx/2], colorYUV[0], borderpx);
    }

    //
    // Right vertical border
    //
    memset(&data[0][(y * width) + width - borderpx], colorYUV[0], borderpx);

  }

  //
  // Bottom most horizontal border
  //
  memset(&data[0][(yend - borderpx - 1) * width], colorYUV[0], width * borderpx);

  ystart /= 2;
  yend /= 2;
  borderpx /= 2;
  origHeight /= 2;
  height /= 2;
  width /= 2;

  //
  // Draw U
  //

  memset(&data[1][width * ystart], colorYUV[1], width * borderpx);

  vblocksz = height/vcount;
  for(countidx = 1; countidx < vcount; countidx++) {
    memset(&data[1][(ystart * width) + (countidx * vblocksz * width) - (width * borderpx/2)], colorYUV[1], width * borderpx);
  }

  for(y = ystart + 1; y < yend - 1; y++) {

    hcountcur = hcountaty[ (y - ystart) / vblocksz ];

    memset(&data[1][y * width], colorYUV[1], borderpx);

    for(countidx = 1; countidx < hcountcur; countidx++) {
      memset(&data[1][(y * width) + countidx * (width/hcountcur) - borderpx/2], colorYUV[1], borderpx);
    }

    memset(&data[1][(y * width) + width - borderpx], colorYUV[1], borderpx);
  }

  memset(&data[1][(yend - borderpx - 1) * width], colorYUV[1], width * borderpx);

  //
  // Draw V
  //

  memset(&data[2][width * ystart], colorYUV[2], width * borderpx);

  vblocksz = height/vcount;
  for(countidx = 1; countidx < vcount; countidx++) {
    memset(&data[2][(ystart * width) + (countidx * vblocksz * width) - (width * borderpx/2)], colorYUV[2], width * borderpx);
  }

  for(y = ystart + 1; y < yend - 1; y++) {

    hcountcur = hcountaty[ (y - ystart) / vblocksz ];

    memset(&data[2][y * width], colorYUV[2], borderpx);

    for(countidx = 1; countidx < hcountcur; countidx++) {
      memset(&data[2][(y * width) + countidx * (width/hcountcur) - borderpx/2], colorYUV[2], borderpx);
    }

    memset(&data[2][(y * width) + width - borderpx], colorYUV[2], borderpx);
  }

  memset(&data[2][(yend - borderpx - 1) * width], colorYUV[2], width * borderpx);

  return rc;
}

