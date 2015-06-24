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


#include "vsx_common.h"

#if defined(VSX_HAVE_STREAMER) && defined(XCODE_HAVE_PIP)

#define DIMENSION_EVEN(x) (((x) & 0x01) ? (x)-1 : (x))

#define PIP_VAD_SWITCH_MIN_THRESHOLD_MS   1000

#define PIP_BORDER_COLOR_RGB "0x888888"
#define PIP_BORDER_EDGE 2

typedef enum PIP_LAYOUT_GRID_DIMENSIONS {
  PIP_LAYOUT_GRID_UNKNOWN  = 0,
  PIP_LAYOUT_GRID_1x1,
  PIP_LAYOUT_GRID_1x2,
  PIP_LAYOUT_GRID_1x3,
  PIP_LAYOUT_GRID_1x4,
  PIP_LAYOUT_GRID_1x5,
  PIP_LAYOUT_GRID_1x6,
  PIP_LAYOUT_GRID_1x7,
  PIP_LAYOUT_GRID_1x8,
  PIP_LAYOUT_GRID_2x1,
  PIP_LAYOUT_GRID_3x1,
  PIP_LAYOUT_GRID_4x1,
  PIP_LAYOUT_GRID_5x1,
  PIP_LAYOUT_GRID_6x1,
  PIP_LAYOUT_GRID_7x1,
  PIP_LAYOUT_GRID_8x1,
  PIP_LAYOUT_GRID_2x2,        // | | |
                              // | | |

  PIP_LAYOUT_GRID_2x2_t1,     // |   |
                              // | | |

  PIP_LAYOUT_GRID_2x2_b1,     // | | |
                              // |   |

  PIP_LAYOUT_GRID_3x2,        // | | | |
                              // | | | |

  PIP_LAYOUT_GRID_3x2_t2,     // |  |  |
                              // | | | |

  PIP_LAYOUT_GRID_3x2_b2,     // | | | |
                              // |     |

  PIP_LAYOUT_GRID_2x3,
  PIP_LAYOUT_GRID_4x2,
  PIP_LAYOUT_GRID_2x4,
  PIP_LAYOUT_GRID_3x3,
} PIP_LAYOUT_GRID_DIMENSIONS_T;

typedef struct PIP_NODE {
  LIST_NODE_T            listNode;
  #define pnext listNode.pnext
  IXCODE_VIDEO_CTXT_T    *pPip;
  STREAMER_CFG_T         *pCfgPip;

} PIP_NODE_T;

typedef struct PIP_LAYOUT {
  unsigned int                 width;
  unsigned int                 height; 
  unsigned int                 posx;
  unsigned int                 posy;
} PIP_LAYOUT_T;


static void pip_make_border(IXCODE_PIP_BORDER_T *pBorder, int edge, PIP_LAYOUT_GRID_DIMENSIONS_T type) {

  if(edge > 0 && type != PIP_LAYOUT_GRID_UNKNOWN) {
    
    pBorder->flags = 0;

    switch(type) {
      case PIP_LAYOUT_GRID_1x1:
      case PIP_LAYOUT_GRID_1x2:
      case PIP_LAYOUT_GRID_1x3:
      case PIP_LAYOUT_GRID_1x4:
      case PIP_LAYOUT_GRID_1x5:
      case PIP_LAYOUT_GRID_1x6:
      case PIP_LAYOUT_GRID_1x7:
      case PIP_LAYOUT_GRID_1x8:
        pBorder->hBlocks = 1;
        break;
      case PIP_LAYOUT_GRID_2x2_t1:
        pBorder->flags |= PIP_BORDER_FLAG_TOP_MIN1;
        pBorder->hBlocks = 2;
        break;
      case PIP_LAYOUT_GRID_2x2_b1:
        pBorder->flags |= PIP_BORDER_FLAG_BOTTOM_MIN1;
        pBorder->hBlocks = 2;
        break;
      case PIP_LAYOUT_GRID_2x1:
      case PIP_LAYOUT_GRID_2x2:
      case PIP_LAYOUT_GRID_2x3:
      case PIP_LAYOUT_GRID_2x4:
        pBorder->hBlocks = 2;
        break;
      case PIP_LAYOUT_GRID_3x2_t2:
        pBorder->flags |= PIP_BORDER_FLAG_TOP_MIN1;
        pBorder->hBlocks = 3;
        break;
      case PIP_LAYOUT_GRID_3x2_b2:
        pBorder->flags |= PIP_BORDER_FLAG_BOTTOM_MIN1;
        pBorder->hBlocks = 3;
        break;
      case PIP_LAYOUT_GRID_3x1:
      case PIP_LAYOUT_GRID_3x2:
      case PIP_LAYOUT_GRID_3x3:
        pBorder->hBlocks = 3;
        break;
      default:
        pBorder->hBlocks = 0;
        break;
    }

    switch(type) {
      case PIP_LAYOUT_GRID_1x1:
      case PIP_LAYOUT_GRID_2x1:
      case PIP_LAYOUT_GRID_3x1:
      case PIP_LAYOUT_GRID_4x1:
      case PIP_LAYOUT_GRID_5x1:
      case PIP_LAYOUT_GRID_6x1:
      case PIP_LAYOUT_GRID_7x1:
      case PIP_LAYOUT_GRID_8x1:
        pBorder->vBlocks = 1;
        break;
      case PIP_LAYOUT_GRID_1x2:
      case PIP_LAYOUT_GRID_2x2:
      case PIP_LAYOUT_GRID_2x2_t1:
      case PIP_LAYOUT_GRID_2x2_b1:
      case PIP_LAYOUT_GRID_3x2:
      case PIP_LAYOUT_GRID_3x2_t2:
      case PIP_LAYOUT_GRID_3x2_b2:
      case PIP_LAYOUT_GRID_4x2:
        pBorder->vBlocks = 2;
        break;
      case PIP_LAYOUT_GRID_1x3:
      case PIP_LAYOUT_GRID_2x3:
      case PIP_LAYOUT_GRID_3x3:
        pBorder->vBlocks = 3;
        break;
      default:
        pBorder->vBlocks = 0;
        break;
    }

    pBorder->edgePx = edge;
    strutil_read_rgb(PIP_BORDER_COLOR_RGB, pBorder->colorRGB);
    pBorder->active = 1;

  } else {
    pBorder->active = 0;
  }

}

/*
static void pip_make_border_padding(IXCODE_VIDEO_CTXT_T *pXcode, int edge) {
  IXCODE_VIDEO_CROP_T *pCrop = &pXcode->out[0].crop;

  //
  // PIP border by using PIP outer padding only works if the PIP size is the
  // size of the overlay pip quadrant
  //

  pCrop->padTopCfg = pCrop->padTop = edge;
  pCrop->padBottomCfg = pCrop->padBottom = edge;
  pCrop->padLeftCfg = pCrop->padLeft = edge;
  pCrop->padRightCfg = pCrop->padRight = edge;
  strutil_read_rgb(PIP_BORDER_COLOR_RGB, pCrop->padRGB);
}
*/

static int get_ar_dimensions(IXCODE_VIDEO_CROP_T *pCrop, const IXCODE_VIDEO_PIP_T *pPip, 
                             const IXCODE_VIDEO_OUT_T *pXOut, 
                             unsigned int canvasWidth, unsigned int canvasHeight) {
  unsigned int width, height;
  int rc = -1;

  if(*pPip->pwidthCfg > 0 && *pPip->pheightCfg > 0) {

    //
    // If the PIP has a specific configured output resolution, use that as the input aspect ratio
    //
    width = *pPip->pwidthCfg;
    height = *pPip->pheightCfg;

  } else {

    width = *pPip->pwidthInput;
    height = *pPip->pheightInput;

  }

  //
  // If we are to preserve the input aspect ratio, make sure that
  // the input aspect ratio has been discovered from the input stream capture.
  //
  if(width > 0 && height > 0) {

    memset(pCrop, 0, sizeof(IXCODE_VIDEO_CROP_T));
    rc = xcode_set_crop_pad(pCrop, pXOut, 1, canvasWidth, canvasHeight, width, height);

  }

  return rc;
}

static int get_quadrant_layout_grid(IXCODE_VIDEO_OUT_T *pXOut, unsigned int numQuadrants,
                                    PIP_LAYOUT_T layout[MAX_PIPS], 
                                    PIP_LAYOUT_GRID_DIMENSIONS_T *pborderType) {

  unsigned int widthCanvas = pXOut->resOutH - pXOut->crop.padLeft - pXOut->crop.padRight;
  unsigned int heightCanvas = pXOut->resOutV - pXOut->crop.padTop - pXOut->crop.padBottom;
  unsigned int width, height;
  unsigned int idx;
  int fillThird = 0;
  float ar = (float) widthCanvas / heightCanvas;
  float magicAr = 1.3333;

  //TODO: 90/270 degree rotation
  if(IXCODE_FILTER_ROTATE_90(*pXOut)) {
  }

  memset(layout, 0, sizeof(PIP_LAYOUT_T) * MAX_PIPS);
  layout[0].posx = pXOut->crop.padLeft;
  layout[0].posy = pXOut->crop.padTop;

  if(numQuadrants == 1) {

    layout[0].width = widthCanvas;
    layout[0].height = heightCanvas;
    *pborderType = PIP_LAYOUT_GRID_1x1;

  } else if(numQuadrants == 2) {

    if(ar >= magicAr) {
      width = widthCanvas / 2;
      height = heightCanvas;
      layout[1].posx = layout[0].posx + width; 
      layout[1].posy = layout[0].posy;
      *pborderType = PIP_LAYOUT_GRID_2x1;

    } else {
      width = widthCanvas;
      height = heightCanvas / 2;
      layout[1].posx = layout[0].posx;
      layout[1].posy = layout[0].posy + height;
      *pborderType = PIP_LAYOUT_GRID_1x2;

    }

    for(idx = 0; idx < numQuadrants; idx++) {
      layout[idx].width = width;
      layout[idx].height = height;
    }

  } else if(numQuadrants == 3) {

    if(ar > 2.0f * magicAr) {
      width = widthCanvas / 3;
      height = heightCanvas;
      layout[1].posx = layout[0].posx + width;
      layout[1].posy = layout[0].posy;
      layout[2].posx = layout[1].posx + width;
      layout[2].posy = layout[1].posy;
      *pborderType = PIP_LAYOUT_GRID_3x1;
    } else if(ar <  magicAr / 2.0f) {
      width = widthCanvas;
      height = heightCanvas / 3;
      layout[1].posy = layout[0].posy + height;
      layout[1].posx = layout[0].posx;
      layout[2].posy = layout[1].posy + height;
      layout[2].posx = layout[1].posx;
      *pborderType = PIP_LAYOUT_GRID_1x3;
    } else {
      width = widthCanvas / 2;
      height = heightCanvas / 2;
      layout[1].posx = layout[0].posx;
      layout[1].posy = layout[0].posy + height;
      layout[2].posx = layout[0].posx + width;
      layout[2].posy = layout[0].posy + height;

      *pborderType =   PIP_LAYOUT_GRID_2x2_t1;

      fillThird = 1;
    }

    for(idx = 0; idx < numQuadrants; idx++) {
      layout[idx].width = width;
      layout[idx].height = height;
    }
    if(fillThird) {
      layout[0].width = widthCanvas;
    }

  } else if(numQuadrants == 4) {

    if(ar > 3.0f * magicAr) {
      width = widthCanvas / 4;
      height = heightCanvas;
      for(idx = 1; idx < numQuadrants; idx++) {
        layout[idx].posx = layout[idx - 1].posx + width;
        layout[idx].posy = layout[idx - 1].posy;
      }
      *pborderType = PIP_LAYOUT_GRID_4x1;
    } else if(ar < magicAr / 3.0f) {
      width = widthCanvas;
      height = heightCanvas / 4;
      for(idx = 1; idx < numQuadrants; idx++) {
        layout[idx].posy = layout[idx - 1].posy + height;
        layout[idx].posx = layout[idx - 1].posx;
      }
      *pborderType = PIP_LAYOUT_GRID_1x4;
    } else {
      width = widthCanvas / 2;
      height = heightCanvas / 2;

      layout[1].posx = layout[0].posx + width;
      layout[1].posy = layout[0].posy;

      layout[2].posx = layout[0].posx;
      layout[2].posy = layout[0].posy + height;
      layout[3].posx = layout[0].posx + width;
      layout[3].posy = layout[2].posy;
      *pborderType = PIP_LAYOUT_GRID_2x2;
    }

    for(idx = 0; idx < numQuadrants; idx++) {
      layout[idx].width = width;
      layout[idx].height = height;
    }

  } else if(numQuadrants == 5) {

    if(ar > 4.0f * magicAr) {
      width = widthCanvas / 5;
      height = heightCanvas;
      for(idx = 1; idx < numQuadrants; idx++) {
        layout[idx].posx = layout[idx - 1].posx + width;
        layout[idx].posy = layout[idx - 1].posy;
      }
      *pborderType = PIP_LAYOUT_GRID_5x1;
    } else if(ar < magicAr / 4.0f) {
      width = widthCanvas;
      height = heightCanvas / 5;
      for(idx = 1; idx < numQuadrants; idx++) {
        layout[idx].posy = layout[idx - 1].posy + height;
        layout[idx].posx = layout[idx - 1].posx;
      }
      *pborderType = PIP_LAYOUT_GRID_1x5;
    } else {
      height = heightCanvas / 2;
      width = widthCanvas / 2;
      layout[1].posx = layout[0].posx + width;
      layout[1].posy = layout[0].posy;
      fillThird = 1;
   
      *pborderType =   PIP_LAYOUT_GRID_3x2_t2;

      width = widthCanvas / 3;
      layout[2].posx = layout[0].posx;
      layout[2].posy = layout[0].posy + height;
      layout[3].posx = layout[2].posx + width;
      layout[3].posy = layout[2].posy;
      layout[4].posx = layout[3].posx + width;
      layout[4].posy = layout[3].posy;
    }

    for(idx = 0; idx < numQuadrants; idx++) {
      layout[idx].width = widthCanvas/3;
      layout[idx].height = height;
    }
    if(fillThird) {
      layout[0].width = widthCanvas/2;
      layout[1].width = widthCanvas/2;
    }
  
  } else if(numQuadrants == 6) {

    if(ar > 5.0f * magicAr) {
      width = widthCanvas / 6;
      height = heightCanvas;
      for(idx = 1; idx < numQuadrants; idx++) {
        layout[idx].posx = layout[idx - 1].posx + width;
        layout[idx].posy = layout[idx - 1].posy;
      }
      *pborderType = PIP_LAYOUT_GRID_6x1;
    } else if(ar < magicAr / 5.0f) {
      width = widthCanvas;
      height = heightCanvas / 6;
      for(idx = 1; idx < numQuadrants; idx++) {
        layout[idx].posy = layout[idx - 1].posy + height;
        layout[idx].posx = layout[idx - 1].posx;
      }
      *pborderType = PIP_LAYOUT_GRID_1x6;
    } else {
      width = widthCanvas / 3;
      height = heightCanvas / 2;
      layout[1].posx = layout[0].posx + width;
      layout[1].posy = layout[0].posy;
      layout[2].posx = layout[1].posx + width;
      layout[2].posy = layout[0].posy;

      layout[3].posx = layout[0].posx;
      layout[3].posy = layout[0].posy + height;
      layout[4].posx = layout[1].posx;
      layout[4].posy = layout[3].posy;
      layout[5].posx = layout[2].posx;
      layout[5].posy = layout[3].posy;
      *pborderType = PIP_LAYOUT_GRID_3x2;
    }

    for(idx = 0; idx < numQuadrants; idx++) {
      layout[idx].width = width;
      layout[idx].height = height;
    }

  } else {
    return -1;
  }

  for(idx = 0; idx < numQuadrants; idx++) {
    //fprintf(stderr, "QUADRANT[%d]/%d %dx%d at x:%d, y:%d, padL:%d, padT:%d\n", idx, numQuadrants, layout[idx].width, layout[idx].height, layout[idx].posx, layout[idx].posy, pXOut->crop.padLeft, pXOut->crop.padTop);
  }

  return 0;
}

static int get_quadrant_layout_vad(IXCODE_VIDEO_OUT_T *pXOut, unsigned int numQuadrants,
                                    PIP_LAYOUT_T layout[MAX_PIPS], PIP_NODE_T *pHead) {

  PIP_NODE_T *pNode;
  IXCODE_VIDEO_CROP_T crop;
  unsigned int widthCanvas = pXOut->resOutH - pXOut->crop.padLeft - pXOut->crop.padRight;
  unsigned int heightCanvas = pXOut->resOutV - pXOut->crop.padTop - pXOut->crop.padBottom;
  unsigned int widthMain = widthCanvas;
  unsigned int heightMain = heightCanvas;
  unsigned int width, height, offsetx, offsety;
  unsigned int idx;
  unsigned int edge = 4;
  float fH, fV;
  //float ar = (float) widthCanvas / heightCanvas;

  //TODO: 90/270 degree rotation
  if(IXCODE_FILTER_ROTATE_90(*pXOut)) {
  }

  pNode = (PIP_NODE_T *) pHead->pnext;
  memset(layout, 0, sizeof(PIP_LAYOUT_T) * MAX_PIPS);

  if(numQuadrants <= 6) {

    offsetx = edge;
    offsety = 0;

    for(idx = 1; idx < numQuadrants; idx++ ) {
     
      //
      // Set the dimensions and position of the non-active speaker PIP(s)
      //

      if(pXOut->crop.padAspectR) {

        fV = .40f;
        width = floor((widthCanvas - edge - ((numQuadrants -1) * edge)) / (numQuadrants -1));
        height = heightCanvas * fV;

      } else {

        if(numQuadrants == 2) {
          fH = .45f;
        } else if(numQuadrants == 3) {
          fH = .38f;
        } else if(numQuadrants == 4) {
          fH = .32f;
        } else if(numQuadrants == 5) {
          fH= .23f;
        } else if(numQuadrants == 6) {
          fH= .18f;
        } else {
          return -1;
        }

        width = widthCanvas * fH;
        height = heightCanvas * fH;
      }

      //fprintf(stderr, "VAD L[%d] %dx%d, ar:%.3f\n", idx, width, height, ar);
      width = MIN(widthCanvas - offsetx - edge, width);
      height = MIN(heightCanvas - offsety, height);

      //fprintf(stderr, "VAD L[%d] %dx%d, ar:%.3f w:(%d - %d - %d), h:(%d -%d -%d)\n", idx, width, height, ar, widthCanvas, offsetx, edge, heightCanvas, offsety, edge);

      memset(&crop, 0, sizeof(crop));
      if(pXOut->crop.padAspectR) {
        get_ar_dimensions(&crop, &pNode->pPip->pip, pXOut, width, height);
      }

      layout[idx].width = DIMENSION_EVEN(width - (crop.padLeft + crop.padRight));
      layout[idx].height = DIMENSION_EVEN(height - (crop.padTop + crop.padBottom));

      //fprintf(stderr, "VAD Q[%d]/%d, canvas:%dx%d (ar:%.3f), fH:%.3f, fV:%.3f, pip:%dx%d (%dx%d), padL:%d, padT:%d\n", idx, numQuadrants, widthCanvas, heightCanvas, ar, fH, fV, layout[idx].width, layout[idx].height, width, height, crop.padLeft, crop.padTop);

      //fprintf(stderr, "IDX[%d] %dx%d, padL:%d, padR:%d, padT:%d, padB:%d, final: %dx%d\n", idx, width, height, crop.padLeft, crop.padRight, crop.padTop, crop.padBottom, layout[idx].width, layout[idx].height);

      if((offsetx += layout[idx].width) >= widthCanvas) {
        //fprintf(stderr, "SCRUNCHED from %d to %d\n", offsetx, widthCanvas);
        offsetx = widthCanvas;
      }

      if(idx == 5 && numQuadrants == 6) {
        layout[idx].posx = edge + pXOut->crop.padLeft;
      } else {
        layout[idx].posx = widthCanvas - offsetx + pXOut->crop.padLeft;
      }
      layout[idx].posy = heightCanvas - layout[idx].height - offsety;

      if((offsetx += edge) >= widthCanvas - edge) {
        //fprintf(stderr, "SCRUNCHED2 from %d to %d\n", offsetx, widthCanvas);
        offsetx = widthCanvas - edge;
      }

      //
      // Ensure the non-active speaker elements are placed on top of the main element
      //
      pNode->pPip->pip.zorder = 10;
      pNode = (PIP_NODE_T *) pNode->pnext;
    } // end of for(idx = 1...

  } else if(numQuadrants > 1) {
    return -1;
  }

  //
  // Set the dimensions and placement of the main active-speaker PIP after the dimensinos of the
  // smaller PIP(s) are known
  //
  memset(&crop, 0, sizeof(crop));
  pHead->pPip->pip.zorder = 0;
 
  if(numQuadrants > 1) {
    heightMain = heightCanvas - (.9f * layout[1].height);
  }

  if(pXOut->crop.padAspectR) {
    //
    // Set the crop.padding... values to preserve the input PIP aspect ratio
    //
    get_ar_dimensions(&crop, &pHead->pPip->pip, pXOut, widthMain, heightMain);
  }

  //
  // The first placement has the highest VAD and takes up the entire canvas
  //
  layout[0].width = DIMENSION_EVEN(widthMain - (crop.padLeft + crop.padRight));
  layout[0].height = DIMENSION_EVEN(heightMain - (crop.padTop + crop.padBottom));

  if(numQuadrants > 1) {
    //
    // If there are more PIPs to be placed over the main overlay, position the main
    // overlay as far up as possible.
    //
    crop.padBottom += crop.padTop;
    crop.padTop = 0;

  }

  layout[0].posx = crop.padLeft;
  layout[0].posy = crop.padTop;
  //fprintf(stderr, "VAD layout[0] posx:%d, posy:%d, padL:%d, padR:%d, padT:%d, padB:%d\n", layout[0].posx, layout[0].posy, crop.padLeft, crop.padRight, crop.padTop, crop.padBottom); 


  for(idx = 0; idx < numQuadrants; idx++) {
    //fprintf(stderr, "VAD-QUADRANT[%d]/%d %dx%d at x:%d, y:%d, padL:%d, padT:%d\n", idx, numQuadrants, layout[idx].width, layout[idx].height, layout[idx].posx, layout[idx].posy, pXOut->crop.padLeft, pXOut->crop.padTop);
  }

  return 0;
}

int pip_sort_compare_input_ar(const LIST_NODE_T *pArg1, const LIST_NODE_T *pArg2) {
  PIP_NODE_T *pNode1 = (PIP_NODE_T *) pArg1;
  PIP_NODE_T *pNode2 = (PIP_NODE_T *) pArg2;
  //int rc;
  float ar = 0, arNext = 0;

  if(pNode1 && *pNode1->pPip->pip.pheightInput > 0) {
    ar = *pNode1->pPip->pip.pwidthInput / *pNode1->pPip->pip.pheightInput;
  }
  if(pNode2 && *pNode2->pPip->pip.pheightInput > 0) {
    arNext = *pNode2->pPip->pip.pwidthInput / *pNode2->pPip->pip.pheightInput;
  }

  if(ar > arNext) {
    return 1;
  } else if(ar < arNext) {
    return -1;
  } else {
    return 0;
  }

}

int pip_sort_compare_vad(const LIST_NODE_T *pArg1, const LIST_NODE_T *pArg2) {
  PIP_NODE_T *pNode1 = (PIP_NODE_T *) pArg1;
  PIP_NODE_T *pNode2 = (PIP_NODE_T *) pArg2;
  int vadLatest1 = 0, vadLatest2 = 0;
  //int rc;
  //float ar = 0, arNext = 0;

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)
  if(pNode1 && pNode1->pPip->pip.pXcodeA) {
    vadLatest1 = pNode1->pPip->pip.pXcodeA->vadLatest;
  }
  if(pNode2 && pNode2->pPip->pip.pXcodeA) {
    vadLatest2 = pNode2->pPip->pip.pXcodeA->vadLatest;
  }
  if(vadLatest1 < vadLatest2) {
    return 1;
  }

#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  return 0;
}

static void set_dimensions(IXCODE_VIDEO_PIP_T *pPip, 
                           unsigned int w, unsigned int h, 
                           unsigned int x, unsigned int y,
                           unsigned int pipframeidx) {

  //
  // Here we set the actual pip resolution and placement within the overlay
  //

  //LOG(X_DEBUG("PIP set_dimensions pipframeidx[%d] resOut:%dx%d, at %dx%d, cfgOut:%dx%d"),  pipframeidx, w,h, x,y, *pPip->pwidthCfg,  *pPip->pheightCfg);

  //if(*pPip->pwidth != w || *pPip->pheight != h || pPip->posx != x || pPip->posy != y) {
  //  pPip->changedResolution = 1;
  //}

  //
  // pwidth / pheight is only set for the 0th index.  Since this is typically used by non-conference PIPs
  // where resolution is auto-detected from input resolution.
  // Each overlay chooses which pipframeidx to select based on the array in
  // pStreamerCfgOverlay->xcode.vid.overlay.pipframeidxs[...]
  //
  if(pPip->pos[pipframeidx].pwidth && pPip->pos[pipframeidx].pheight) {
    *pPip->pos[pipframeidx].pwidth = w;
    *pPip->pos[pipframeidx].pheight = h;
  } else {
    pPip->pos[pipframeidx].width = w;
    pPip->pos[pipframeidx].height = h;
  }

  pPip->pos[pipframeidx].posx =  x;
  pPip->pos[pipframeidx].posy =  y;
  
}

static int pip_layout_grid(IXCODE_VIDEO_OUT_T *pXOut, PIP_NODE_T **ppHead, 
                          unsigned int numActive, int useUniquePipLayouts,
                          int haveMainOverlayOutput) {
  unsigned int idx = 0;
  //int rc;
  int do_update;
  unsigned int width, height;
  unsigned int posx, posy;
  int haveUniquePipLayouts = 0;
  unsigned int pipframeidx;
  IXCODE_VIDEO_CROP_T crop;
  PIP_NODE_T *pNode;
  PIP_LAYOUT_T layouts[2][MAX_PIPS];
  IXCODE_VIDEO_CTXT_T *pXOverlay = NULL;
  PIP_LAYOUT_GRID_DIMENSIONS_T borderType = PIP_LAYOUT_GRID_UNKNOWN;

  memset(layouts[1], 0, sizeof(layouts[1]));

  if(get_quadrant_layout_grid(pXOut, numActive, layouts[0], &borderType) < 0) {
    return -1;
  }

  //
  // Create PIP specific unique layout in p2p case
  //
  if(numActive == 2 && useUniquePipLayouts > 0) {

    //
    // Preserve the original multi-person grid layout into the 2nd index, which
    // will be used for rendering by the main overlay
    //
    if(haveMainOverlayOutput) {
      memcpy(layouts[1], layouts[0], sizeof(layouts[1]));
      haveUniquePipLayouts = 1;
    } else {
      numActive = 1;
    }

    layouts[0][0].width = pXOut->resOutH - pXOut->crop.padLeft - pXOut->crop.padRight;
    layouts[0][0].height = pXOut->resOutV - pXOut->crop.padTop - pXOut->crop.padBottom;
    layouts[0][0].posx = pXOut->crop.padLeft;
    layouts[0][0].posy = pXOut->crop.padTop;

    layouts[0][1].width = layouts[0][0].width;
    layouts[0][1].height = layouts[0][0].height;
    layouts[0][1].posx = layouts[0][0].posx;
    layouts[0][1].posy = layouts[0][0].posy;

  }

  //
  // If we have 3 PIPS then we try to put the pip with the greatest aspect ratio into the 
  // widest slot (first).
  // TODO: this should only be for PIP_LAYOUT_GRID_2x2_t1 /  PIP_LAYOUT_GRID_2x2_b1 and not PIP_LAYOUT_GRID_3x1 / PIP_LAYOUT_GRID_1x3
  //
  if(numActive == 3 && pXOut->crop.padAspectR) {
    *ppHead = (PIP_NODE_T *) list_sort((LIST_NODE_T *) *ppHead, pip_sort_compare_input_ar);
  }

  pNode = *ppHead;
  idx = 0;
  while(pNode) {

    if(!pXOverlay) {
      pXOverlay = pNode->pPip->pip.pXOverlay;
    }

    //
    // Check if the dynamic layout dimensions have been determined (possibly from input capture dimensions)
    //
    if(*pNode->pPip->pip.pos[0].pwidth > 0 && *pNode->pPip->pip.pos[0].pheight > 0) {
      do_update = 1;
    } else {
      do_update = 0; 
    }

    for(pipframeidx = 0; pipframeidx < PIP_ADD_MAX; pipframeidx++) {

      if(!do_update) {
        break;
      } else if(pipframeidx > 0 && !haveUniquePipLayouts) {

        if((pNode->pPip->pip.pos[pipframeidx].width > 0 || pNode->pPip->pip.pos[pipframeidx].height > 0)) {
          //
          // Clear any prior dimensions for pipframeidx > 0
          //
          set_dimensions(&pNode->pPip->pip, 0, 0, 0, 0, pipframeidx);
        }

        continue;
      }

      width = layouts[pipframeidx][idx].width;
      height = layouts[pipframeidx][idx].height;
      posx = layouts[pipframeidx][idx].posx;
      posy = layouts[pipframeidx][idx].posy;

    //fprintf(stderr, "PIP GRID set_dimensions quadrant is %dx%d, at x:%d, y:%d, pwdith:%dx%d, *pwidthCfg::%dx%d, *pwdithInput:%dx%d\n",  width,  height, posx, posy, *pNode->pPip->pip.pwidth, *pNode->pPip->pip.pheight, *pNode->pPip->pip.pwidthCfg, *pNode->pPip->pip.pheightCfg, *pNode->pPip->pip.pwidthInput, *pNode->pPip->pip.pheightInput);

      //
      // Preserve the original aspect ratio
      //
      if(pXOut->crop.padAspectR) {

        if(get_ar_dimensions(&crop, &pNode->pPip->pip, pXOut, 
                             layouts[pipframeidx][idx].width, layouts[pipframeidx][idx].height) == 0) {
          width -= (crop.padLeft + crop.padRight);
          height -= (crop.padTop + crop.padBottom);
          posx += crop.padLeft;
          posy += crop.padTop;
          //fprintf(stderr, "padAspectR, padL:%d, padR:%d, padT:%d, padB:%d, widthAr:%dx%d\n", crop.padLeft, crop.padRight, crop.padTop, crop.padBottom, widthAr, heightAr); 
        } else {
          do_update = 0;
        }
      }

      width = DIMENSION_EVEN(width);
      height = DIMENSION_EVEN(height);

      if(do_update) {

        //LOG(X_DEBUG("PIP GRID set_dimensions %dx%d, at x:%d, y:%d, numActive:%d, border:%d"),  width,  height, posx, posy, numActive, pNode->pPip->out[0].crop.padLeft);
        set_dimensions(&pNode->pPip->pip, width, height, posx, posy, pipframeidx);
      }

    } // end of for(...

    pNode = (PIP_NODE_T *) pNode->pnext;
    idx++;
  }


  if(pXOverlay) {
    pip_make_border(&pXOverlay->overlay.border, numActive > 1 ? PIP_BORDER_EDGE : 0, borderType);
  }

  return 0;
}

PIP_NODE_T *pip_vad_sort(PIP_NODE_T *pHead) {
  PIP_NODE_T *pNode = pHead; 
  PIP_NODE_T *pHighestVad = NULL, *pPrevVad = NULL, *pPrev = NULL;

#if defined(XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  //
  // Place the node with the highest VAD at the head, w/o rearranging the rest of the nodes.
  //

  while(pNode) {

    if(pNode->pPip->pip.pXcodeA && 
       (!pHighestVad || pNode->pPip->pip.pXcodeA->vadLatest > pHighestVad->pPip->pip.pXcodeA->vadLatest)) {
      pHighestVad = pNode;
      pPrevVad = pPrev;
    }

    pPrev = pNode;
    pNode = (PIP_NODE_T *) pNode->pnext;
  }
  
  if(pHighestVad && pHighestVad != pHead) {
    pPrevVad->pnext = pHighestVad->pnext;
    pHighestVad->pnext = (LIST_NODE_T *) pHead;
    pHead = pHighestVad;
  }

#endif // (XCODE_HAVE_PIP_AUDIO) && (XCODE_HAVE_PIP_AUDIO > 0)

  return pHead;
}

static int pip_layout_vad(IXCODE_VIDEO_OUT_T *pXOut, PIP_NODE_T **ppHeadOrig, unsigned int numActive, 
                          enum PIP_UPDATE reason, int useUniquePipLayouts) {
  PIP_NODE_T *pNode, *pHead;
  int do_update;
  unsigned int idx;
  unsigned int width, height;
  unsigned int posx, posy;
  struct timeval tv;
  PIP_LAYOUT_T layouts[MAX_PIPS];

  //pNode=pHeadOrig;while(pNode) { fprintf(stderr, "NODE-before idx:%d, vad:%d\n", pNode->pPip->indexPlus1, pNode->pPip->pXcodeA->vadLatest); pNode = (PIP_NODE_T *) pNode->pnext; }

  //
  // Arrange tht PIP(s) list according to VAD
  //
  pHead = pip_vad_sort(*ppHeadOrig);

  //pNode=pHead;while(pNode) { fprintf(stderr, "NODE-after idx:%d, vad:%d\n", pNode->pPip->indexPlus1, pNode->pPip->pXcodeA->vadLatest); pNode = (PIP_NODE_T *) pNode->pnext; }

  if(reason == PIP_UPDATE_VAD) {

    gettimeofday(&tv, NULL);
    if(pHead != *ppHeadOrig) {

      //
      // If there is a change in the active speaker, make sure we don't switch within the last
      // PIP_VAD_SWITCH_MIN_THRESHOLD_MS of the last switch
      //
      if(TIME_TV_DIFF_MS(tv, (*ppHeadOrig)->pCfgPip->pip.tvLastActiveSpkr) < PIP_VAD_SWITCH_MIN_THRESHOLD_MS) {
        //fprintf(stderr, "PIP_LAYOUT_VAD change time frequency not exceeded yet %u ms\n", TIME_TV_DIFF_MS(tv, pHeadOrig->pCfgPip->pip.tvLastActiveSpkr));
        return -1;
      }

      TIME_TV_SET(pHead->pCfgPip->pip.tvLastActiveSpkr, tv);

    } else {
      //
      // Do nothing, since we were called by a VAD update but the active speaker has not changed
      //

      //fprintf(stderr, "PIP_LAYOUT_VAD, no active speaker change\n");
      return -1;
    }
  }


  //
  // Test pip specific unique layout
  //
  if(numActive == 2 && useUniquePipLayouts > 0) {
    PIP_NODE_T nodeTmp;
    PIP_LAYOUT_T layoutsTmp[MAX_PIPS];
    memcpy(&nodeTmp, pHead, sizeof(PIP_NODE_T));
    nodeTmp.pnext = NULL;
    get_quadrant_layout_vad(pXOut, 1, layoutsTmp, &nodeTmp);
    memcpy(&layouts[0], &layoutsTmp[0], sizeof(layouts[0]));

    memcpy(&nodeTmp, pHead->pnext, sizeof(PIP_NODE_T));
    nodeTmp.pnext = NULL;
    get_quadrant_layout_vad(pXOut, 1, layoutsTmp, &nodeTmp);
    memcpy(&layouts[1], &layoutsTmp[0], sizeof(layouts[0]));

    //layouts[1].width = layouts[0].width;
    //layouts[1].height = layouts[0].height;
    //layouts[1].posx = layouts[0].posx;
    //layouts[1].posy = layouts[0].posy;

    numActive = 1;

  } else if(get_quadrant_layout_vad(pXOut, numActive, layouts, pHead) < 0) {
    return -1;
  }

  pNode = pHead;
  idx = 0;
  while(pNode) {

    width = layouts[idx].width;
    height = layouts[idx].height;
    posx = layouts[idx].posx;
    posy = layouts[idx].posy;
    //if(idx > 0 && numActive > 1) {
    //  pip_make_border_padding(pNode->pPip, PIP_BORDER_EDGE, PIP_LAYOUT_GRID_UNKNOWN);
    //} else {
    //  pip_make_border_pading(pNode->pPip, 0, PIP_LAYOUT_GRID_UNKNOWN);
    //}
    do_update = 1;

    if(do_update) {
      //fprintf(stderr, "\n\nPIP VAD set_dimensions %dx%d, at x:%d, y:%d, numActive: %d, border:%d\n",  width,  height, posx, posy, numActive, pNode->pPip->out[0].crop.padLeft);
      set_dimensions(&pNode->pPip->pip, width, height, posx, posy, 0);
    }

    pNode = (PIP_NODE_T *) pNode->pnext;
    idx++;
  }

  return 0;
}

static void test(PIP_NODE_T *pHead, IXCODE_VIDEO_OVERLAY_T *pOverlay, int set) {
  PIP_NODE_T *pNode = pHead;
  unsigned int idx;

  //
  // Create an overlay for the PIP specific output (just like we do for the main overlay output)
  // We do this so the pip can have it's own unique layout view, such as when doing a p2p conversation
  //

  while(pNode) {

    if(set && pNode->pPip->pip.doEncode) {

      pthread_mutex_lock(&pNode->pPip->overlay.mtx);

      memcpy(pNode->pPip->overlay.p_pipsx, pOverlay->p_pipsx, sizeof(pOverlay->p_pipsx));
      for(idx = 0; idx < MAX_PIPS; idx++) {

        pOverlay->pipframeidxs[idx] = 1;

        if(pNode->pPip->overlay.p_pipsx[idx] == pNode->pPip) {
          pNode->pPip->overlay.p_pipsx[idx] = NULL; 
          pip_update_zorder(pNode->pPip->overlay.p_pipsx);
        }
      }

      if(pNode->pPip->overlay.p_pipsx[0]) {
        pNode->pPip->overlay.havePip = 1;
      } else {
        pNode->pPip->overlay.havePip = 0;
      }

      pthread_mutex_unlock(&pNode->pPip->overlay.mtx);

    } else {

      for(idx = 0; idx < MAX_PIPS; idx++) {
        pOverlay->pipframeidxs[idx] = 0;
      }

      pNode->pPip->overlay.havePip = 0;
    }

#if 0

  if(pNode == pHead) {
    for(idx = 0; idx < MAX_PIPS; idx++) {
      if(pOverlay->p_pipsx[idx]) {
        LOG(X_DEBUG("MAIN OVERLAY's pips: p_pipsx[%d].indexPlus1:%d, id:%d"), idx, pOverlay->p_pipsx[idx]->pip.indexPlus1, pOverlay->p_pipsx[idx] ? pOverlay->p_pipsx[idx]->pip.id: -1);
      }
    }
  }

  int idx2;
  for(idx = 0; idx < MAX_PIPS; idx++) {
    LOG(X_DEBUG("PIP.indexPlus1:%d, PIP[%d].overlay.havePip:%d"), pNode->pPip->pip.indexPlus1, idx, pNode->pPip->overlay.havePip);
    if(pNode->pPip->overlay.p_pipsx[idx]) {
      for(idx2 = 0; idx2 < MAX_PIPS; idx2++) {
        if(pNode->pPip->overlay.p_pipsx[idx]->overlay.p_pipsx[idx2]) {
          LOG(X_DEBUG("PIP[%d].id:%d [%d].id:%d"), idx, pNode->pPip->overlay.p_pipsx[idx]->pip.id, idx2, pNode->pPip->overlay.p_pipsx[idx]->overlay.p_pipsx[idx2]->pip.id);
        }
      }
    }
  }
#endif // 

    pNode = (PIP_NODE_T *) pNode->pnext;
  }

}

static int testcount(PIP_NODE_T *pHead, const IXCODE_VIDEO_OVERLAY_T *pOverlay) {
  PIP_NODE_T *pNode = pHead;
  unsigned int idx = 0;

  while(pNode) {

    if(pNode->pPip->pip.doEncode) {
      idx++;
    }
      
    pNode = (PIP_NODE_T *) pNode->pnext;
  }

  return idx;
}

int pip_layout_update(PIP_LAYOUT_MGR_T *pLayout, enum PIP_UPDATE reason) {
  int rc = 0;
  unsigned int idxTmp;
  unsigned int numVisible = 0;
  unsigned int numActive = 0;
  IXCODE_VIDEO_CTXT_T *pPip;
  STREAMER_CFG_T *pCfgOverlay;
  IXCODE_VIDEO_OUT_T *pXOut;
  PIP_NODE_T pipsVisible[MAX_PIPS];
  PIP_NODE_T pipsActive[MAX_PIPS];
  PIP_NODE_T *pHeadVisible;
  PIP_NODE_T *pHeadActive;
  PIP_LAYOUT_TYPE_T layoutType;
  unsigned int outidx = 0;
  int doP2PDisplay = 0;

  if(!pLayout || !pLayout->pPip || !pLayout->pPip->pCfgOverlay) { 
    return -1;
  }

  //if(reason == PIP_UPDATE_VAD) return 0;

  memset(pipsVisible, 0, sizeof(pipsVisible));
  memset(pipsActive, 0, sizeof(pipsActive));

  pCfgOverlay = pLayout->pPip->pCfgOverlay;
  pXOut = &pCfgOverlay->xcode.vid.out[outidx];

  //
  // Don't do anything if the overlay layout type assigned is manual
  //
  if(pCfgOverlay->pip.layoutType == PIP_LAYOUT_TYPE_MANUAL) {
    return 0;
  } else if(reason == PIP_UPDATE_PARTICIPANT_START) {
    //
    // We will wait for PIP_UPDATE_PARTICIPANT_HAVEINPUT (input sequence headers received)
    //
    return 0;
  } else if(reason == PIP_UPDATE_PARTICIPANT_HAVEINPUT) {
    //LOG(X_DEBUG("PIP_UPDATE_PARTICIPANT_HAVE_INPUT...\n\n\n"));
  }

  //PIP_UPDATE_VAD                      = 1,
  //PIP_UPDATE_PARTICIPANT_START        = 2,
  //PIP_UPDATE_PARTICIPANT_END          = 3,
  //PIP_UPDATE_PARTICIPANT_HAVEINPUT    = 4

  layoutType = pCfgOverlay->pip.layoutType;

  //
  // If vad is not enabled in config change to appropriate grid layout
  //
  if(!pCfgOverlay->pip.pMixerCfg->vad) {
    if(layoutType == PIP_LAYOUT_TYPE_VAD_P2P) {
      layoutType = PIP_LAYOUT_TYPE_GRID_P2P;
    } else if(layoutType == PIP_LAYOUT_TYPE_VAD) {
      layoutType = PIP_LAYOUT_TYPE_GRID;
    }
  }

  //
  // lock here so pip_start / pip_stop won't be called, esp when we're exiting the program
  //
  pthread_mutex_lock(&pCfgOverlay->pip.mtx);
  pthread_mutex_lock(&pCfgOverlay->xcode.vid.overlay.mtx);

  numVisible = 0;
  for(idxTmp = 0; idxTmp < MAX_PIPS; idxTmp++) {

    //LOG(X_DEBUG("PIP_UPDATE_LAYOUT pip[%d].active:%d, visible:%d, showVisual:%d, autoLayout:%d"), idxTmp, pCfgOverlay->xcode.vid.overlay.p_pipsx[idxTmp] ? pCfgOverlay->xcode.vid.overlay.p_pipsx[idxTmp]->pip.active : -99, pCfgOverlay->xcode.vid.overlay.p_pipsx[idxTmp] ? pCfgOverlay->xcode.vid.overlay.p_pipsx[idxTmp]->pip.visible : -99, pCfgOverlay->xcode.vid.overlay.p_pipsx[idxTmp] ? pCfgOverlay->xcode.vid.overlay.p_pipsx[idxTmp]->pip.showVisual : -99, pCfgOverlay->xcode.vid.overlay.p_pipsx[idxTmp] ? pCfgOverlay->xcode.vid.overlay.p_pipsx[idxTmp]->pip.autoLayout : -99); 

    if(!(pPip = pCfgOverlay->xcode.vid.overlay.p_pipsx[idxTmp]) || !pPip->pip.active) {
      continue;
    }

    //
    // Create a linked list of the active PIPs
    //
    pipsActive[numVisible].pPip = pPip;
    pipsActive[numVisible].pCfgPip = pCfgOverlay->pip.pCfgPips[pPip->pip.indexPlus1 - 1]; 
    if(++numActive> 1) {
      pipsActive[numActive - 2].pnext = (LIST_NODE_T *) &pipsActive[numActive- 1];
    }

    if(pPip->pip.visible <= 0 || !pPip->pip.showVisual || !pPip->pip.autoLayout) {
      continue;
    }

    //
    // Create a linked list of the visible PIPs
    //
    pipsVisible[numVisible].pPip = pPip;
    pipsVisible[numVisible].pCfgPip = pCfgOverlay->pip.pCfgPips[pPip->pip.indexPlus1 - 1]; 
    if(++numVisible > 1) {
      pipsVisible[numVisible - 2].pnext = (LIST_NODE_T *) &pipsVisible[numVisible - 1];
    }

  }

  //LOG(X_DEBUG("PIP_LAYOUT_UPDATE reason:%d, numActive:%d, numVisible:%d\n"), reason, numActive, numVisible);

  pHeadVisible = &pipsVisible[0];
  pHeadActive = &pipsActive[0];

  if(numVisible > 0 && numVisible <= MAX_PIPS) {

    //
    // For our special case of treating 2 pips as a 2-way call
    // only let the main overlay render <= 1 active pip... for recording
    //
    if((layoutType == PIP_LAYOUT_TYPE_GRID_P2P || layoutType == PIP_LAYOUT_TYPE_VAD_P2P) &&
       //pCfgOverlay->pip.maxUniquePipLayouts > 0 && numActive <= 2 &&
       pCfgOverlay->pip.maxUniquePipLayouts > 0 && numVisible <= 2 &&
       testcount(pHeadVisible, &pCfgOverlay->xcode.vid.overlay) > 0) {
      doP2PDisplay = 1;
//      pCfgOverlay->xcode.vid.overlay.maxAutoLayoutDisplayPips = 1;
    } else {
//      pCfgOverlay->xcode.vid.overlay.maxAutoLayoutDisplayPips = 0;
    }
     
    if(layoutType == PIP_LAYOUT_TYPE_VAD || layoutType == PIP_LAYOUT_TYPE_VAD_P2P) {

      if(pip_layout_vad(pXOut, &pHeadVisible, numVisible, reason, doP2PDisplay) >= 0) {
        pip_update_zorder(pCfgOverlay->xcode.vid.overlay.p_pipsx);
      }

      if(pCfgOverlay->pip.maxUniquePipLayouts > 0) {
        test(pHeadVisible, &pCfgOverlay->xcode.vid.overlay, numVisible > 1 ? doP2PDisplay : 0);
      }

    } else {

      pip_layout_grid(pXOut, &pHeadVisible, numVisible, doP2PDisplay, STREAMER_DO_OUTPUT(pCfgOverlay->action) ? 1 : 0);

      if(pCfgOverlay->pip.maxUniquePipLayouts > 0) {
        test(pHeadVisible, &pCfgOverlay->xcode.vid.overlay, numVisible > 1 ? doP2PDisplay : 0);
      } 

      //TODO: can't assign multipile PIP scale sizes to the same pit... so if active=3,visible=2,
      // non-visible participant needs to see 2 pips 
      // There may be some 'sendonly' PIPs
      //if(numVisible != numActive) {
      //  pip_layout_grid(pXOut, &pHeadActive, numActive, 0);
      //}

    }

  }

  pthread_mutex_unlock(&pCfgOverlay->xcode.vid.overlay.mtx);
  pthread_mutex_unlock(&pCfgOverlay->pip.mtx);

  return rc;
}

PIP_LAYOUT_TYPE_T pip_str2layout(const char *str) {

  if(str) {
    if(!strcasecmp("p2pvad", str)) {
      return PIP_LAYOUT_TYPE_VAD_P2P;
    } else if(!strcasecmp("vad", str)) {
      return PIP_LAYOUT_TYPE_VAD;
    } else if(!strcasecmp("p2pgrid", str)) {
      return PIP_LAYOUT_TYPE_GRID_P2P;
    } else if(!strcasecmp("grid", str)) {
      return PIP_LAYOUT_TYPE_GRID;
    } else if(!strcasecmp("manual", str)) {
      return PIP_LAYOUT_TYPE_MANUAL;
    }   
  }

  return PIP_LAYOUT_TYPE_UNKNOWN;
}

const char *pip_layout2str(PIP_LAYOUT_TYPE_T layoutType) {
  switch(layoutType) {
    case PIP_LAYOUT_TYPE_VAD_P2P:
      return "p2pvad";
    case PIP_LAYOUT_TYPE_VAD:
      return "vad";
    case PIP_LAYOUT_TYPE_GRID_P2P:
      return "p2pgrid";
    case PIP_LAYOUT_TYPE_GRID:
      return "grid";
    case PIP_LAYOUT_TYPE_MANUAL:
      return "manual";
    case PIP_LAYOUT_TYPE_UNKNOWN:
    default:
      return "unknown";
  }

}


#endif //  (VSX_HAVE_STREAMER) && defined(XCODE_HAVE_PIP)

