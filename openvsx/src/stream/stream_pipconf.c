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


typedef struct PIP_MOTION_PARSE {
  IXCODE_PIP_MOTION_VEC_T     *pMv;
  int                          have_x1;
  int                          have_y1;
  int                          count;
  int                         *palphamax_min1;
  int                         *palphamin_min1;
  int                         *pzorder;
  int                         *pnovid;
  int                         *pnoaud;
  char                        *pinput;
  char                        *pxcodestr;
} PIP_MOTION_PARSE_T;


int cbparse_pip_param(void *pArg, const char *p) {
  int rc = 0;
  int unprocessed = 0;
  PIP_MOTION_PARSE_T *pMvP = (PIP_MOTION_PARSE_T *) pArg;

  //
  // The string comparison is only for the length of the searched string, 
  // so be careful matching 'x' and then 'xcode'
  //

  if(!strncasecmp(p, PIP_KEY_XCODE, strlen(PIP_KEY_XCODE))) {

    if((p = strutil_skip_key(p, strlen(PIP_KEY_XCODE)))) {
      p = avc_dequote(p, NULL, 0);
      strncpy(pMvP->pxcodestr, p,  KEYVAL_MAXLEN - 1);
    }

    unprocessed = 1;

  } else if(!strncasecmp(p, PIP_KEY_POSX1_RIGHT, strlen(PIP_KEY_POSX1_RIGHT))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_POSX1_RIGHT)))) {
      pMvP->pMv->x1 = atoi(p);
      pMvP->have_x1 = 1;
      pMvP->pMv->flags |= PIP_FLAGS_MOTION_X1_RIGHT;
    }
  } else if(!strncasecmp(p, PIP_KEY_POSX1, strlen(PIP_KEY_POSX1))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_POSX1)))) {
      pMvP->pMv->x1 = atoi(p);
      pMvP->have_x1 = 1;
    }
  } else if(!strncasecmp(p, PIP_KEY_POSX_RIGHT, strlen(PIP_KEY_POSX_RIGHT))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_POSX_RIGHT)))) {
      pMvP->pMv->x0 = atoi(p);
      pMvP->pMv->flags |= PIP_FLAGS_MOTION_X0_RIGHT;
    }
  } else if(!strncasecmp(p, PIP_KEY_POSX, strlen(PIP_KEY_POSX))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_POSX)))) {
      pMvP->pMv->x0 = atoi(p);
    }
  } else if(!strncasecmp(p, PIP_KEY_POSY1_BOTTOM, strlen(PIP_KEY_POSY1_BOTTOM))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_POSY1_BOTTOM)))) {
      pMvP->pMv->y1 = atoi(p);
      pMvP->have_y1 = 1;
      pMvP->pMv->flags |= PIP_FLAGS_MOTION_Y1_BOTTOM;
    }
  } else if(!strncasecmp(p, PIP_KEY_POSY1, strlen(PIP_KEY_POSY1))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_POSY1)))) {
      pMvP->pMv->y1 = atoi(p);
      pMvP->have_y1 = 1;
    }
  } else if(!strncasecmp(p, PIP_KEY_POSY_BOTTOM, strlen(PIP_KEY_POSY_BOTTOM))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_POSY_BOTTOM)))) {
      pMvP->pMv->y0 = atoi(p);
      pMvP->pMv->flags |= PIP_FLAGS_MOTION_Y0_BOTTOM;
    }
  } else if(!strncasecmp(p, PIP_KEY_POSY, strlen(PIP_KEY_POSY))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_POSY)))) {
      pMvP->pMv->y0 = atoi(p);
    }
  } else if(!strncasecmp(p, PIP_KEY_PIX_PER_FRAME, strlen(PIP_KEY_PIX_PER_FRAME))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_PIX_PER_FRAME)))) {
      pMvP->pMv->pixperframe = atof(p);
    }
  } else if(!strncasecmp(p, PIP_KEY_PAD_BOTTOM, strlen(PIP_KEY_PAD_BOTTOM))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_PAD_BOTTOM)))) {
      pMvP->pMv->crop.padBottomCfg = pMvP->pMv->crop.padBottom = atoi(p);
    }
  } else if(!strncasecmp(p, PIP_KEY_PAD_TOP, strlen(PIP_KEY_PAD_TOP))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_PAD_TOP)))) {
      pMvP->pMv->crop.padTopCfg = pMvP->pMv->crop.padTop = atoi(p);
    }
  } else if(!strncasecmp(p, PIP_KEY_PAD_LEFT, strlen(PIP_KEY_PAD_LEFT))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_PAD_LEFT)))) {
      pMvP->pMv->crop.padLeftCfg = pMvP->pMv->crop.padLeft = atoi(p);
    }
  } else if(!strncasecmp(p, PIP_KEY_PAD_RGB, strlen(PIP_KEY_PAD_RGB))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_PAD_RGB)))) {
      strutil_read_rgb(p, pMvP->pMv->crop.padRGB);
    }
  } else if(!strncasecmp(p, PIP_KEY_PAD_RIGHT, strlen(PIP_KEY_PAD_RIGHT))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_PAD_RIGHT)))) {
      pMvP->pMv->crop.padRightCfg = pMvP->pMv->crop.padRight = atoi(p);
    }
  } else if(!strncasecmp(p, PIP_KEY_PAD_ASPECT_RATIO, 
            strlen(PIP_KEY_PAD_ASPECT_RATIO))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_PAD_ASPECT_RATIO)))) {
      pMvP->pMv->crop.padAspectR = atoi(p);
    }
  } else if(!strncasecmp(p, PIP_KEY_CROP_BOTTOM, strlen(PIP_KEY_CROP_BOTTOM))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_CROP_BOTTOM)))) {
      pMvP->pMv->crop.cropBottom = atoi(p);
    }
  } else if(!strncasecmp(p, PIP_KEY_CROP_TOP, strlen(PIP_KEY_CROP_TOP))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_CROP_TOP)))) {
      pMvP->pMv->crop.cropTop = atoi(p);
    }
  } else if(!strncasecmp(p, PIP_KEY_CROP_LEFT, strlen(PIP_KEY_CROP_LEFT))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_CROP_LEFT)))) {
      pMvP->pMv->crop.cropLeft = atoi(p);
    }
  } else if(!strncasecmp(p, PIP_KEY_CROP_RIGHT, strlen(PIP_KEY_CROP_RIGHT))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_CROP_RIGHT)))) {
      pMvP->pMv->crop.cropRight = atoi(p);
    }
  } else if(!strncasecmp(p, PIP_KEY_ALPHA_MAX0, strlen(PIP_KEY_ALPHA_MAX0))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_ALPHA_MAX0)))) {
      pMvP->pMv->alphamax0_min1 = atoi(p) + 1;
    }
  } else if(!strncasecmp(p, PIP_KEY_ALPHA_MAX1, strlen(PIP_KEY_ALPHA_MAX1))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_ALPHA_MAX1)))) {
      pMvP->pMv->alphamax1_min1 = atoi(p) + 1;
    }
  } else if(!strncasecmp(p, PIP_KEY_ALPHA_PER_FRAME, strlen(PIP_KEY_ALPHA_PER_FRAME))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_ALPHA_PER_FRAME)))) {
      pMvP->pMv->alphaperframe = atof(p);
    }
  } else if(!strncasecmp(p, PIP_KEY_FRAME_DURATION, strlen(PIP_KEY_FRAME_DURATION))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_FRAME_DURATION)))) {
      pMvP->pMv->frames = atoi(p);
    }
  } else if(!strncasecmp(p, PIP_KEY_ALPHA_MAX, strlen(PIP_KEY_ALPHA_MAX))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_ALPHA_MAX)))) {
      *pMvP->palphamax_min1 = atoi(p) + 1;
    }
    unprocessed = 1;
  } else if(!strncasecmp(p, PIP_KEY_ALPHA_MIN, strlen(PIP_KEY_ALPHA_MIN))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_ALPHA_MIN)))) {
      *pMvP->palphamin_min1 = atoi(p) + 1;
    }
    unprocessed = 1;
  } else if(!strncasecmp(p, PIP_KEY_ZORDER, strlen(PIP_KEY_ZORDER))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_ZORDER)))) {
      *pMvP->pzorder = atoi(p);
    }
    unprocessed = 1;
  } else if(!strncasecmp(p, PIP_KEY_NOVIDEO, strlen(PIP_KEY_NOVIDEO))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_NOVIDEO)))) {
      *pMvP->pnovid = atoi(p);
    }
    unprocessed = 1;
  } else if(!strncasecmp(p, PIP_KEY_NOAUDIO, strlen(PIP_KEY_NOAUDIO))) {
    if((p = strutil_skip_key(p, strlen(PIP_KEY_NOAUDIO)))) {
      *pMvP->pnoaud = atoi(p);
    }
    unprocessed = 1;
  } else if(!strncasecmp(p, "input", strlen("input"))) {

    if((p = strutil_skip_key(p, strlen("input")))) {
      p = avc_dequote(p, NULL, 0);
      strncpy(pMvP->pinput, p, VSX_MAX_PATH_LEN - 1);
    }

    unprocessed = 1;
  } else {
    LOG(X_WARNING("Unprocessed PIP motion parameter '%s'"), p);
    unprocessed = 1;
  }

  if(!unprocessed) {
    pMvP->count++;
  }

  return rc;
}

IXCODE_PIP_MOTION_T *pip_read_conf(PIP_CFG_T *pipCfg, char input[VSX_MAX_PATH_LEN], char pipXcodestrbuf[KEYVAL_MAXLEN]) {
  int rc = 0;
  FILE_STREAM_T fs;
  char *p;
  PIP_MOTION_PARSE_T pipParse;
  IXCODE_PIP_MOTION_T *pMotion = NULL;
  IXCODE_PIP_MOTION_VEC_T *pVec = NULL;
  IXCODE_PIP_MOTION_VEC_T *pVecPrev = NULL;
  int alphamax_min1 = 0;
  int alphamin_min1 = 0;
  unsigned int idx = 0;
  char buf[1024];

  if(!pipCfg || !pipCfg->cfgfile) {
    return NULL;
  }

  if(OpenMediaReadOnly(&fs, pipCfg->cfgfile) < 0) {
    return NULL;
  }

  input[0] = '\0';

  while(fileops_fgets(buf, sizeof(buf) - 1, fs.fp)) {

    p = buf;
    while(*p == ' ' || *p == '\t') {
      p++;
    }
    if(*p == '#' || *p == '\r' || *p == '\n') {
      continue;
    }

    if(!pVec && !(pVec = (IXCODE_PIP_MOTION_VEC_T *) 
                             avc_calloc(1, sizeof(IXCODE_PIP_MOTION_VEC_T)))) {
      break;
    }
    memset(&pipParse, 0, sizeof(pipParse));
    pipParse.pMv = pVec;
    pipParse.count = 0;
    pipParse.pinput = input;
    pipParse.pxcodestr = pipXcodestrbuf;
    pipParse.palphamax_min1 = &alphamax_min1;
    pipParse.palphamin_min1 = &alphamin_min1;
    pipParse.pzorder = &pipCfg->zorder;
    pipParse.pnovid = &pipCfg->novid;
    pipParse.pnoaud = &pipCfg->noaud;

    if((rc = strutil_parse_csv(cbparse_pip_param, &pipParse, p)) < 0) {
      break;
    }

    if(pipParse.count > 0) {

      if(!pipParse.have_x1) {
        pipParse.pMv->x1 = pipParse.pMv->x0;
        if((pipParse.pMv->flags & PIP_FLAGS_MOTION_X0_RIGHT)) {
          pipParse.pMv->flags |=  PIP_FLAGS_MOTION_X1_RIGHT;
        }
      }
      if(!pipParse.have_y1) {
        pipParse.pMv->y1 = pipParse.pMv->y0;
        if((pipParse.pMv->flags & PIP_FLAGS_MOTION_Y0_BOTTOM)) {
          pipParse.pMv->flags |=  PIP_FLAGS_MOTION_Y1_BOTTOM;
        }
      }

      if(pVecPrev == NULL) {
        if(!(pMotion = (IXCODE_PIP_MOTION_T *) 
                         avc_calloc(1, sizeof(IXCODE_PIP_MOTION_T)))) {
          break;
        }
        pMotion->pm = pMotion->pmCur = pVec;
        pMotion->active = 1;
      } else {
        pVecPrev->pnext = pVec;
      }

      //fprintf(stderr, "idx:%d x:%d->%d, y:%d->%d %.3fppf :0x%x\n", idx, pVec->x0, pVec->x1, pVec->y0, pVec->y1, pVec->pixperframe, pVec);

      pVecPrev = pVec;
      pVec = NULL;

      if(++idx >= PIP_MOTION_VEC_MAX) { 
        LOG(X_WARNING("Reached max %d pip directives in %s"), PIP_MOTION_VEC_MAX, pipCfg->cfgfile);
        break;
      }

    }
  
  }

  CloseMediaFile(&fs);

  if(pVec && (pMotion && !pMotion->pm)) {
    free(pVec);
  }
  if(alphamax_min1 != 0) {
    pipCfg->alphamax_min1 = alphamax_min1;
  } 
  if(alphamin_min1 != 0) {
    pipCfg->alphamin_min1 = alphamin_min1;
  } 
  if(input[0] != '\0') {
    pipCfg->input = input;
  }
  if(pipXcodestrbuf[0] != '\0') {
    pipCfg->strxcode = pipXcodestrbuf;
  }

  LOG(X_DEBUG("Processed PIP configuration %s with %d directive(s) for %s"),
        pipCfg->cfgfile, idx, pipCfg->input);

  return pMotion;
} 

#endif // (VSX_HAVE_STREAMER) && defined(XCODE_HAVE_PIP)
