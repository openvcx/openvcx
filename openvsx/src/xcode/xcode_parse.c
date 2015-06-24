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


#define XCODE_MAX_KEYVALS     64
#define IS_PARAM_ON(p)   (!strncmp((p), "1", 1) || !strncasecmp((p), "y", 1) || \
                          !strncasecmp((p), "on", 2))

typedef enum XCODE_PARAM_ID {
  XCODE_PARAM_VIDEO_CODEC                  = 0,
  XCODE_PARAM_VIDEO_PASSTHRU                  ,
  XCODE_PARAM_VIDEO_BITRATE                   ,
  XCODE_PARAM_VIDEO_BITRATE_ABRMAX            ,
  XCODE_PARAM_VIDEO_BITRATE_ABRMIN            ,
  XCODE_PARAM_VIDEO_BITRATE_TOL               ,
  XCODE_PARAM_VIDEO_VBVBUF                    ,
  XCODE_PARAM_VIDEO_VBVMAX                    ,
  XCODE_PARAM_VIDEO_VBVMIN                    ,
  XCODE_PARAM_VIDEO_VBVAGGR                   ,
  XCODE_PARAM_VIDEO_VBVINITIAL                ,
  XCODE_PARAM_VIDEO_CFRIN                     ,
  XCODE_PARAM_VIDEO_CFROUT                    ,
  XCODE_PARAM_VIDEO_CFRTOL                    ,
  XCODE_PARAM_VIDEO_ENCSPEED                  ,
  XCODE_PARAM_VIDEO_ENCQUALITY                ,
  XCODE_PARAM_VIDEO_FPS_NUM                   ,
  XCODE_PARAM_VIDEO_FPS_DENUM                 ,
  XCODE_PARAM_VIDEO_FPS                       ,
  XCODE_PARAM_VIDEO_FPS_ABRMAX                ,
  XCODE_PARAM_VIDEO_FPS_ABRMIN                ,
  XCODE_PARAM_VIDEO_GOP                       ,
  XCODE_PARAM_VIDEO_GOP_ABRMAX                ,
  XCODE_PARAM_VIDEO_GOP_ABRMIN                ,
  XCODE_PARAM_VIDEO_GOPMIN                    ,
  XCODE_PARAM_VIDEO_FORCEIDR                  ,
  XCODE_PARAM_VIDEO_LOOKAHEAD                 ,
  XCODE_PARAM_VIDEO_PROFILE                   ,
  XCODE_PARAM_VIDEO_Q                         ,
  XCODE_PARAM_VIDEO_QBRATIO                   ,
  XCODE_PARAM_VIDEO_QIRATIO                   ,
  XCODE_PARAM_VIDEO_QMAX                      ,
  XCODE_PARAM_VIDEO_QMIN                      ,
  XCODE_PARAM_VIDEO_QDIFF                     ,
  XCODE_PARAM_VIDEO_RCTYPE                    ,
  XCODE_PARAM_VIDEO_SCALERTYPE                ,
  XCODE_PARAM_VIDEO_ISCENEAGGR                ,
  XCODE_PARAM_VIDEO_SLICESZMAX                ,
  XCODE_PARAM_VIDEO_THREADS_ENC               ,
  XCODE_PARAM_VIDEO_THREADS_DEC               ,
  XCODE_PARAM_VIDEO_UPSAMPLING                ,
  //XCODE_PARAM_VIDEO_NODECODE                  ,
  XCODE_PARAM_VIDEO_MBTREE                    ,
  XCODE_PARAM_VIDEO_QCOMP                     ,
  XCODE_PARAM_VIDEO_WIDTH                     ,
  XCODE_PARAM_VIDEO_HEIGHT                    ,
  XCODE_PARAM_VIDEO_PIPWIDTH                  ,
  XCODE_PARAM_VIDEO_PIPHEIGHT                 ,
  XCODE_PARAM_VIDEO_CROP_BOTTOM               ,
  XCODE_PARAM_VIDEO_CROP_TOP                  ,
  XCODE_PARAM_VIDEO_CROP_LEFT                 ,
  XCODE_PARAM_VIDEO_CROP_RIGHT                ,
  XCODE_PARAM_VIDEO_PAD_ASPECTR               ,
  XCODE_PARAM_VIDEO_PAD_BOTTOM                ,
  XCODE_PARAM_VIDEO_PAD_TOP                   ,
  XCODE_PARAM_VIDEO_PAD_LEFT                  ,
  XCODE_PARAM_VIDEO_PAD_RIGHT                 ,
  XCODE_PARAM_VIDEO_PAD_RGB                   ,
  XCODE_PARAM_VIDEO_SHARP_LUMA_SIZE           ,
  XCODE_PARAM_VIDEO_SHARP_LUMA_STRENGTH       ,
  XCODE_PARAM_VIDEO_SHARP_CHROMA_SIZE         ,
  XCODE_PARAM_VIDEO_SHARP_CHROMA_STRENGTH     ,
  XCODE_PARAM_VIDEO_DENOISE                   ,
  XCODE_PARAM_VIDEO_DENOISE_FACTOR            ,
  XCODE_PARAM_VIDEO_COLOR_SATURATION          ,
  XCODE_PARAM_VIDEO_COLOR_CONTRAST            ,
  XCODE_PARAM_VIDEO_COLOR_BRIGHTNESS          ,
  XCODE_PARAM_VIDEO_COLOR_HUE                 ,
  XCODE_PARAM_VIDEO_COLOR_GAMMA               ,
  XCODE_PARAM_VIDEO_ROTATE                    ,
  XCODE_PARAM_VIDEO_ROTATE_DEGREES            ,
  XCODE_PARAM_VIDEO_TESTFILTER                ,

  XCODE_PARAM_AUDIO_CODEC                     ,
  XCODE_PARAM_AUDIO_BITRATE                   ,
  XCODE_PARAM_AUDIO_FORCE                     ,
  XCODE_PARAM_AUDIO_SAMPLERATE                ,
  XCODE_PARAM_AUDIO_CHANNELS                  ,
  XCODE_PARAM_AUDIO_VOLUME                    ,
  XCODE_PARAM_AUDIO_PROFILE                   ,
  //XCODE_PARAM_AUDIO_NODECODE                  ,

  XCODE_PARAM_MAX
} XCODE_PARAM_ID_T;

typedef struct XCODE_PARAM {
  XCODE_PARAM_ID_T           id;
  const char                *shortName;
  const char                *longName;
  const char                *extName;
} XCODE_PARAM_T;

static struct XCODE_PARAM XCODE_PARAM_LIST[] = {

                  //
                  // The order here has to match enum XCODE_PARAM_ID

                  { XCODE_PARAM_VIDEO_CODEC,               "vc", "videoCodec", NULL },
                  { XCODE_PARAM_VIDEO_PASSTHRU ,           "passthru", "videoPassthru", NULL },
                  { XCODE_PARAM_VIDEO_BITRATE,             "vb", "videoBitrate", NULL },
                  { XCODE_PARAM_VIDEO_BITRATE_ABRMAX,      "vbmax", "videoBitrateMax", NULL },
                  { XCODE_PARAM_VIDEO_BITRATE_ABRMIN,      "vbmin", "videoBitrateMin", NULL },
                  { XCODE_PARAM_VIDEO_BITRATE_TOL ,        "vbt", "videoBitrateTolerance", "vt" },
                  { XCODE_PARAM_VIDEO_VBVBUF,              "vbvbuf", "vbvBufferSize", NULL  },
                  { XCODE_PARAM_VIDEO_VBVMAX,              "vbvmax", "vbvMaxRate", NULL  },
                  { XCODE_PARAM_VIDEO_VBVMIN,              "vbvmin", "vbvMinRate", NULL  },
                  { XCODE_PARAM_VIDEO_VBVAGGR,             "vbvaggr", "vbvAggressivity", NULL },
                  { XCODE_PARAM_VIDEO_VBVINITIAL,          "vbvinit", "vbvInitialFullness", NULL },
                  { XCODE_PARAM_VIDEO_CFRIN,               "vcfrin", "videoInputConstantFps", NULL  },
                  { XCODE_PARAM_VIDEO_CFROUT,              "vcfrout", "videoOutputConstantFps", NULL  },
                  { XCODE_PARAM_VIDEO_CFRTOL,              "vcfrtol", "videoOutputFpsDriftTolerance", NULL  },
                  { XCODE_PARAM_VIDEO_ENCSPEED,            "vf", "videoEncoderSpeed", },
                  { XCODE_PARAM_VIDEO_ENCQUALITY,          "vqual", "videoEncoderQuality", },
                  { XCODE_PARAM_VIDEO_FPS_NUM ,            "vfn", "videoFpsNumerator", "fn" },
                  { XCODE_PARAM_VIDEO_FPS_DENUM ,          "vfd", "videoFpsDenumerator", "fd" },
                  { XCODE_PARAM_VIDEO_FPS ,                "vfr", "videoFps", "fps"},
                  { XCODE_PARAM_VIDEO_FPS_ABRMAX ,         "vfrmax", "videoFpsMax", "fpsmax" },
                  { XCODE_PARAM_VIDEO_FPS_ABRMIN ,         "vfrmin", "videoFpsMin", "fpsmin" },
                  { XCODE_PARAM_VIDEO_GOP ,                "vgop", "videoGOPMs", "vgmax"},
                  { XCODE_PARAM_VIDEO_GOP_ABRMAX ,         "vgopmax", "videoGOPMsMax", NULL },
                  { XCODE_PARAM_VIDEO_GOP_ABRMIN ,         "vgopmin", "videoGOPMsMin", NULL },
                  { XCODE_PARAM_VIDEO_GOPMIN ,             "vmingop", "videoMinGOPMs", "vgmin"},
                  { XCODE_PARAM_VIDEO_FORCEIDR ,           "vidr", "videoForceIDR", NULL },
                  { XCODE_PARAM_VIDEO_LOOKAHEAD ,          "vlh", "videoLookaheadFramesMin1", NULL },
                  { XCODE_PARAM_VIDEO_PROFILE ,            "vp", "videoProfile", NULL },
                  { XCODE_PARAM_VIDEO_Q ,                  "vq", "videoQuantizer", NULL },
                  { XCODE_PARAM_VIDEO_QBRATIO  ,           "vqbratio", "videoQuantizerBRatio", NULL },
                  { XCODE_PARAM_VIDEO_QIRATIO  ,           "vqiratio", "videoQuantizerIRatio", NULL },
                  { XCODE_PARAM_VIDEO_QMAX ,               "vqmax", "videoQuantizerMax", NULL },
                  { XCODE_PARAM_VIDEO_QMIN ,               "vqmin", "videoQuantizerMin", NULL },
                  { XCODE_PARAM_VIDEO_QDIFF ,              "vqdiff", "videoQuantizerDiff", NULL },
                  { XCODE_PARAM_VIDEO_RCTYPE ,             "vrc", "videoRateControl", NULL },
                  { XCODE_PARAM_VIDEO_SCALERTYPE ,         "vsc", "videoScalerType", NULL },
                  { XCODE_PARAM_VIDEO_ISCENEAGGR,          "vsi", "videoISceneAggressivity", NULL },
                  { XCODE_PARAM_VIDEO_SLICESZMAX,          "vslmax", "videoSliceSizeMax", NULL },
                  { XCODE_PARAM_VIDEO_THREADS_ENC ,        "vth", "videoThreads", "th" },
                  { XCODE_PARAM_VIDEO_THREADS_DEC ,        "vthd", "videoDecoderThreads", "thd" },
                  { XCODE_PARAM_VIDEO_UPSAMPLING ,         "vup", "videoUpsampling", NULL },
                  //{ XCODE_PARAM_VIDEO_NODECODE ,         "vnd", "videoNoDecode", NULL },
                  { XCODE_PARAM_VIDEO_MBTREE ,             "mbtree", "mb-tree", "" },
                  { XCODE_PARAM_VIDEO_QCOMP ,              "qcomp", "qcompress", "" },
                  { XCODE_PARAM_VIDEO_WIDTH ,              "vx", "videoWidth", "x" },
                  { XCODE_PARAM_VIDEO_HEIGHT ,             "vy", "videoHeight", "y" },
                  { XCODE_PARAM_VIDEO_PIPWIDTH ,           "vpipx", "videoPipWidth", NULL },
                  { XCODE_PARAM_VIDEO_PIPHEIGHT ,          "vpipy", "videoPipHeight", NULL },
                  { XCODE_PARAM_VIDEO_CROP_BOTTOM ,        "cropb", "cropBottom", NULL },
                  { XCODE_PARAM_VIDEO_CROP_TOP ,           "cropt", "cropTop", NULL },
                  { XCODE_PARAM_VIDEO_CROP_LEFT ,          "cropl", "cropLeft", NULL },
                  { XCODE_PARAM_VIDEO_CROP_RIGHT ,         "cropr", "cropRight", NULL },
                  { XCODE_PARAM_VIDEO_PAD_ASPECTR ,        "padaspect", "padAspectRatio", NULL },
                  { XCODE_PARAM_VIDEO_PAD_BOTTOM ,         "padb", "padBottom", NULL },
                  { XCODE_PARAM_VIDEO_PAD_TOP ,            "padt", "padTop", NULL },
                  { XCODE_PARAM_VIDEO_PAD_LEFT ,           "padl", "padLeft", NULL },
                  { XCODE_PARAM_VIDEO_PAD_RIGHT ,          "padr", "padRight", NULL },
                  { XCODE_PARAM_VIDEO_PAD_RGB ,            "padRGB", "padColorRGB", NULL },
                  { XCODE_PARAM_VIDEO_SHARP_LUMA_SIZE ,    "sharplsz", "sharpLumaSize", NULL },
                  { XCODE_PARAM_VIDEO_SHARP_LUMA_STRENGTH, "sharpl", "sharpLumaStrength", NULL },
                  { XCODE_PARAM_VIDEO_SHARP_CHROMA_SIZE ,  "sharpcsz", "sharpChromaSize", NULL },
                  { XCODE_PARAM_VIDEO_SHARP_CHROMA_STRENGTH, "sharpc", "sharpChromaStrength", NULL },
                  { XCODE_PARAM_VIDEO_DENOISE,             "denoise", NULL, NULL },
                  { XCODE_PARAM_VIDEO_DENOISE_FACTOR,      "denoisefactor", "denoiseFactor", NULL },
                  { XCODE_PARAM_VIDEO_COLOR_SATURATION,    "sat", "saturation", NULL },
                  { XCODE_PARAM_VIDEO_COLOR_CONTRAST,      "con", "contrast", NULL },
                  { XCODE_PARAM_VIDEO_COLOR_BRIGHTNESS,    "bri", "brightness", NULL },
                  { XCODE_PARAM_VIDEO_COLOR_HUE,           "hue", "hue", NULL },
                  { XCODE_PARAM_VIDEO_COLOR_GAMMA,         "gam", "gamma", NULL },
                  { XCODE_PARAM_VIDEO_ROTATE,              "rotate", NULL, NULL },
                  { XCODE_PARAM_VIDEO_ROTATE_DEGREES,      "degrees", "rotateDegrees", NULL },
                  { XCODE_PARAM_VIDEO_TESTFILTER,          "testfilter", "testfilter", NULL },

                  { XCODE_PARAM_AUDIO_CODEC ,              "ac", "audioCodec", NULL },
                  { XCODE_PARAM_AUDIO_BITRATE ,            "ab", "audioBitrate", NULL },
                  { XCODE_PARAM_AUDIO_FORCE ,              "af", "audioForce", NULL },
                  { XCODE_PARAM_AUDIO_SAMPLERATE ,         "ar", "audioSampleRate", NULL },
                  { XCODE_PARAM_AUDIO_CHANNELS ,           "as", "audioChannels", NULL },
                  { XCODE_PARAM_AUDIO_VOLUME ,             "av", "audioVolume", NULL },
                  { XCODE_PARAM_AUDIO_PROFILE ,            "ap", "audioProfile", NULL },
                  //{ XCODE_PARAM_AUDIO_NODECODE,           "and", "audioNoDecode", NULL },

                };


static XCODE_PARAM_T *xcode_find_param_bykey(const char *key) {
  unsigned int idx;

  if(!key) {
    return NULL;
  }

  for(idx = 0; idx < XCODE_PARAM_MAX; idx++) {

    if((XCODE_PARAM_LIST[idx].shortName && !strcasecmp(key, XCODE_PARAM_LIST[idx].shortName)) ||
       (XCODE_PARAM_LIST[idx].longName && !strcasecmp(key, XCODE_PARAM_LIST[idx].longName)) ||
       (XCODE_PARAM_LIST[idx].extName && !strcasecmp(key, XCODE_PARAM_LIST[idx].extName))) {

      return &XCODE_PARAM_LIST[idx];
    }
  }

  return NULL;
}

static const char *conf_find_keyval_index2(const KEYVAL_PAIR_T *kv, XCODE_PARAM_ID_T paramId, unsigned int idx) {
  char buf[128];
  const char *p;
  const char *search;
  XCODE_PARAM_T *pParam;
  unsigned int idxName;
  
  if(paramId < 0 || paramId >= XCODE_PARAM_MAX) {
    return NULL;
  } 

  pParam = &XCODE_PARAM_LIST[paramId];
  for(idxName = 0; idxName < 3; idxName++) {
  
    search = idxName == 0 ? pParam->shortName : idxName == 1 ? pParam->longName : pParam->extName;
    if(!search) {
      continue;
    }

    if(idx == 0) {
      if(snprintf(buf, sizeof(buf), "%s", search) >= 0 && (p = conf_find_keyval(kv, buf))) {
        return p;
      }
    }

    if(snprintf(buf, sizeof(buf), "%s%d", search, idx + 1) >= 0 && (p = conf_find_keyval(kv, buf))) {
      return p;
    }

  }

  return NULL;
}

static const char *conf_find_keyval2(const KEYVAL_PAIR_T *kv, XCODE_PARAM_ID_T paramId) {
  const char *p;
  const char *search;
  XCODE_PARAM_T *pParam;
  unsigned int idxName;
  
  if(paramId < 0 || paramId >= XCODE_PARAM_MAX) {
    return NULL;
  } 

  pParam = &XCODE_PARAM_LIST[paramId];

  for(idxName = 0; idxName < 3; idxName++) {
  
    search = idxName == 0 ? pParam->shortName : idxName == 1 ? pParam->longName : pParam->extName;

    if(!search) {
      continue;
    }

    if((p = conf_find_keyval(kv, search))) {
      return p;
    }

  }

  return NULL;
}


static int parse_configstr_vid_filter_unsharp(IXCODE_VIDEO_CTXT_T *pV, const KEYVAL_PAIR_T *kv, 
                                              unsigned int outidx) {
  const char *p;
  int lumaSize = 0;
  int haveLumaStrength = 0;
  int haveChromaStrength = 0;
  float lumaStrength = 0.0f;
  int chromaSize = 0;
  float chromaStrength = 0.0f;

  if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_SHARP_LUMA_SIZE, outidx))) {
    lumaSize = abs(atoi(p));
  }

  if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_SHARP_LUMA_STRENGTH, outidx))) {
    lumaStrength = atof(p);
    haveLumaStrength = 1;
  }

  if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_SHARP_CHROMA_SIZE, outidx))) {
    chromaSize = abs(atoi(p));
  }

  if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_SHARP_CHROMA_STRENGTH, outidx))) {
    chromaStrength = atof(p);
    haveChromaStrength = 1;
  }

  if(lumaSize > 0 || chromaSize > 0 || lumaStrength != 0.0f || chromaStrength != 0.0f) {

    if(lumaSize == 0 && chromaSize == 0) {
      lumaSize = chromaSize = 5;
    } else if(lumaSize == 0) {
      lumaSize = chromaSize;
    } else if(chromaSize == 0) {
      chromaSize = lumaSize;
    }
   
    if(lumaStrength != 0 || chromaStrength != 0) {

      pV->out[outidx].unsharp.common.active = 1;
      pV->out[outidx].unsharp.luma.sizeX = pV->out[outidx].unsharp.luma.sizeY = lumaSize;
      pV->out[outidx].unsharp.chroma.sizeX = pV->out[outidx].unsharp.chroma.sizeY = chromaSize;
      pV->out[outidx].unsharp.luma.strength = lumaStrength;
      pV->out[outidx].unsharp.chroma.strength = chromaStrength;

    }

  }

  return 0;
}

static int parse_configstr_vid_filter_denoise(IXCODE_VIDEO_CTXT_T *pV, const KEYVAL_PAIR_T *kv, 
                                              unsigned int outidx) {
  const char *p;
  int val = 0;
  double lumaSpacial = 0;

  if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_DENOISE, outidx))) {
    val = atoi(p);
  }
  if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_DENOISE_FACTOR, outidx))) {
    if((lumaSpacial = atof(p)) > 0) {
      val = 1;
    }
  }

  if(val) {
    pV->out[outidx].denoise.common.active = 1;
    pV->out[outidx].denoise.lumaSpacial = lumaSpacial;
  }

  return 0;
}

static int parse_configstr_vid_filter_color(IXCODE_VIDEO_CTXT_T *pV, const KEYVAL_PAIR_T *kv, unsigned int outidx) {
  const char *p;
  float fContrast = IXCODE_COLOR_CONTRAST_DEFAULT; 
  float fBrightness = IXCODE_COLOR_BRIGHTNESS_DEFAULT; 
  float fHue = IXCODE_COLOR_HUE_DEFAULT;
  float fSaturation = IXCODE_COLOR_SATURATION_DEFAULT;
  float fGamma = IXCODE_COLOR_GAMMA_DEFAULT; 

  //
  // 0, 1 - 3
  //
  if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_COLOR_SATURATION, outidx))) {
    fSaturation = atof(p);
  }

  //
  // 0, 1, 2
  //
  if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_COLOR_CONTRAST, outidx))) {
    fContrast = atof(p);
  }

  //
  // 0, 1, 2
  //
  if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_COLOR_BRIGHTNESS, outidx))) {
    fBrightness = atof(p);
  }

  //
  // 0 - 360
  //
  if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_COLOR_HUE, outidx))) {
    fHue = atof(p);
  }

  //
  // 0 1, 10 
  //
  if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_COLOR_GAMMA, outidx))) {
    fGamma= atof(p);
  }

  if(fContrast != IXCODE_COLOR_CONTRAST_DEFAULT || 
     fBrightness != IXCODE_COLOR_BRIGHTNESS_DEFAULT || 
     fHue != IXCODE_COLOR_HUE_DEFAULT || 
     fSaturation != IXCODE_COLOR_SATURATION_DEFAULT || 
     fGamma != IXCODE_COLOR_GAMMA_DEFAULT) {

    pV->out[outidx].color.common.active = 1;
    pV->out[outidx].color.fContrast = fContrast;
    pV->out[outidx].color.fBrightness = fBrightness;
    pV->out[outidx].color.fHue = fHue;
    pV->out[outidx].color.fSaturation = fSaturation;
    pV->out[outidx].color.fGamma = fGamma;
  }

  return 0;
}

static int parse_configstr_vid_filter_rotate(IXCODE_VIDEO_CTXT_T *pV, const KEYVAL_PAIR_T *kv, unsigned int outidx) {
  const char *p;
  int val = 0;

  if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_ROTATE, outidx))) {
    val = atoi(p);
  }
  if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_ROTATE_DEGREES, outidx))) {
    val = atoi(p);
  }

  if(val) {
    pV->out[outidx].rotate.common.active = 1;
    pV->out[outidx].rotate.degrees = val;
  }

  return 0;
}


static int parse_configstr_vid_filter_test(IXCODE_VIDEO_CTXT_T *pV, const KEYVAL_PAIR_T *kv, unsigned int outidx) {
  const char *p;
  int val = 0;

  if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_TESTFILTER, outidx))) {
    val = atoi(p);
  }

  if(val > 0) {
    pV->out[outidx].testFilter.common.active = 1;
    pV->out[outidx].testFilter.val = val;
  }

  return 0;
}

static int get_threads_auto() {
  int numcpus;

  if((numcpus = sys_getcpus()) <= 0) {
    LOG(X_WARNING("Unable to find number of cpus"));
    return -1;
  } 
  // default value.  Rough optimal val be around (# cores * ~2)
  return MIN(8, numcpus * 2);
}

static int parse_configstr_vid_general(IXCODE_VIDEO_CTXT_T *pV, const KEYVAL_PAIR_T *kv) {
  int rc = 0;
  const char *p;
  unsigned int ui1, ui2;
  unsigned int outidx;
  int passthru;

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    passthru = pV->out[outidx].passthru;

    if(outidx == 0) {
      pV->out[outidx].active = 1;
    } else {
      memcpy(&pV->out[outidx], &pV->out[outidx - 1], sizeof(pV->out[outidx]));
      if(outidx == 1 && pV->out[outidx -1].passthru) {
        pV->out[outidx].active = 1;
      } else {
        pV->out[outidx].active = 0;
        //pV->common.cfgFileTypeOut = XC_CODEC_TYPE_UNKNOWN;
      }
      pV->out[outidx].passthru = passthru;
    }

    if(passthru) {
      continue;
    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_SCALERTYPE, outidx))) {
      pV->out[outidx].cfgScaler = atoi(p);
    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_WIDTH, outidx))) {

      pV->out[outidx].cfgOutH = atoi(p);

      if(outidx > 0 && pV->out[outidx].cfgOutH != pV->out[outidx - 1].cfgOutH) {
        pV->out[outidx].active = 1;
      }
    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_HEIGHT, outidx))) {

      pV->out[outidx].cfgOutV = atoi(p);

      if(outidx > 0 && pV->out[outidx].cfgOutV != pV->out[outidx - 1].cfgOutV) {
        pV->out[outidx].active = 1;
      }
    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_PIPWIDTH, outidx))) {
      pV->out[outidx].cfgOutPipH = atoi(p);
    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_PIPHEIGHT, outidx))) {
      pV->out[outidx].cfgOutPipV = atoi(p);
    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_PAD_BOTTOM, outidx))) {
      pV->out[outidx].crop.padBottomCfg = pV->out[outidx].crop.padBottom = atoi(p);
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_PAD_TOP, outidx))) {
      pV->out[outidx].crop.padTopCfg = pV->out[outidx].crop.padTop = atoi(p);
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_PAD_LEFT, outidx))) {
      pV->out[outidx].crop.padLeftCfg = pV->out[outidx].crop.padLeft = atoi(p);
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_PAD_RGB, outidx))) {
      strutil_read_rgb(p, pV->out[outidx].crop.padRGB);
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_PAD_RIGHT, outidx))) {
      pV->out[outidx].crop.padRightCfg = pV->out[outidx].crop.padRight = atoi(p);
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_PAD_ASPECTR, outidx))) {
      pV->out[outidx].crop.padAspectR = atoi(p);
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_CROP_BOTTOM, outidx))) {
      pV->out[outidx].crop.cropBottom = atoi(p);
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_CROP_TOP, outidx))) {
      pV->out[outidx].crop.cropTop = atoi(p);
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_CROP_LEFT, outidx))) {
      pV->out[outidx].crop.cropLeft = atoi(p);
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_CROP_RIGHT, outidx))) {
      pV->out[outidx].crop.cropRight = atoi(p);
    }

    parse_configstr_vid_filter_color(pV, kv, outidx);

    parse_configstr_vid_filter_unsharp(pV, kv, outidx);

    parse_configstr_vid_filter_denoise(pV, kv, outidx);

    parse_configstr_vid_filter_rotate(pV, kv, outidx);

    parse_configstr_vid_filter_test(pV, kv, outidx);


    //TODO: enable upsamping / unique fps for 2+ encoders

    //
    // Set this outidx to active if specifying unique bitrate / qp parameters
    //
    if(outidx > 0 && ((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_BITRATE, outidx)) ||
       (p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_Q, outidx)))) {
      pV->out[outidx].active = 1;
    }


  } // end of for

  if((p = conf_find_keyval2(kv, XCODE_PARAM_VIDEO_CFRIN))) {
    pV->common.cfgCFRInput = atoi(p);
  }
  if((p = conf_find_keyval2(kv, XCODE_PARAM_VIDEO_CFROUT))) {
    pV->common.cfgCFROutput = atoi(p);
  }
  if((p = conf_find_keyval2(kv, XCODE_PARAM_VIDEO_CFRTOL))) {
    pV->common.cfgCFRTolerance = atoi(p) * 90;
  }
  if(pV->common.cfgCFRTolerance <= 0) {
    pV->common.cfgCFRTolerance = XCODE_CFR_OUT_TOLERANCE_DEFAULT;
  }
  if((p = conf_find_keyval2(kv, XCODE_PARAM_VIDEO_FPS))) {
    if(vid_convert_fps(atof(p), &ui1, &ui2) == 0) {
      pV->cfgOutClockHz = pV->resOutClockHz = ui1;
      pV->cfgOutFrameDeltaHz = pV->resOutFrameDeltaHz = ui2;
    }
  }

  if((p = conf_find_keyval2(kv, XCODE_PARAM_VIDEO_FPS_NUM))) {
    pV->cfgOutClockHz = pV->resOutClockHz = atoi(p);
  }
  if((p = conf_find_keyval2(kv, XCODE_PARAM_VIDEO_FPS_DENUM))) {
    pV->cfgOutFrameDeltaHz = pV->resOutFrameDeltaHz = atoi(p);
  }

  if(pV->cfgOutClockHz == 0 || pV->cfgOutFrameDeltaHz == 0) {
    pV->cfgOutClockHz = 0;
    pV->cfgOutFrameDeltaHz = 0;
  }
  if((p = conf_find_keyval2(kv, XCODE_PARAM_VIDEO_UPSAMPLING))) {
    pV->common.cfgAllowUpsample = atoi(p);
  }

  if((p = conf_find_keyval2(kv, XCODE_PARAM_VIDEO_THREADS_DEC))) {
    pV->cfgThreadsDec = atoi(p);
  }

  //if((p = conf_find_keyval2(kv, XCODE_PARAM_VIDEO_NODECODE))) {
  //  pV->common.cfgNoDecode = atoi(p);
  //}

  if(pV->common.cfgAllowUpsample && 
    (pV->cfgOutFrameDeltaHz == 0 || pV->common.cfgCFROutput <= 0)) {
    pV->common.cfgAllowUpsample = 0;
    LOG(X_WARNING("Disabling upsampling because no constant output frame rate set"));
  }

  return rc;
}

static unsigned int enc_speed(unsigned int speed) {
  if(speed > ENCODER_QUALITY_LOWEST) {
    speed = ENCODER_QUALITY_LOWEST;
  } 
  return speed;
}

static unsigned int enc_qual_to_speed(unsigned int speed) {
  if(speed > ENCODER_QUALITY_LOWEST) {
    speed = ENCODER_QUALITY_LOWEST;
  } 
  return ENCODER_QUALITY_LOWEST - speed;
}

static int readGopMs(const char *p, int *priorValue) {
  unsigned int val = 0;

  if(p) {
    if(!strncasecmp(p, "inf", 3)) {
     val = -1;
    } else if(!avc_isnumeric(p)) {
      val = 0;
    } else if((val = atoi(p)) < 0) {
       val = 0;
    } else if(val == 0) {
       //
       // If the GOP was explicitly set to 0 (all key frames), set to 1ms to take effect
       // at the xcode layer
       //
       val = 1;
    }
  } else if(priorValue && *priorValue >  0) {
    val = *priorValue;
  }

  return val;
}

static int parse_configstr_vid_encoder(IXCODE_VIDEO_CTXT_T *pV, const KEYVAL_PAIR_T *kv) {
  int rc = 0;
  unsigned int outidx;
  unsigned int ui1, ui2;
  float fpsMin = 0;
  float fpsMax = 0;
  const char *p;

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(pV->out[outidx].passthru || !pV->out[outidx].active) {
      continue;
    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_THREADS_ENC, outidx))) {
      pV->out[outidx].cfgThreads = atoi(p);
    } else if(outidx == 0) {
      pV->out[outidx].cfgThreads = get_threads_auto();
    } else if(outidx > 0) {
      pV->out[outidx].cfgThreads = pV->out[outidx - 1].cfgThreads;
    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_QMAX, outidx))) {
      pV->out[outidx].cfgQMax = atoi(p);
    } else if(outidx > 0) {
      pV->out[outidx].cfgQMax = pV->out[outidx - 1].cfgQMax;
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_QMIN, outidx))) {
      pV->out[outidx].cfgQMin = atoi(p);
    } else if(outidx > 0) {
      pV->out[outidx].cfgQMin = pV->out[outidx - 1].cfgQMin;
    }

    if(pV->out[outidx].cfgQMin > 0 && pV->out[outidx].cfgQMax > 0 && pV->out[outidx].cfgQMin > pV->out[outidx].cfgQMax) {
      pV->out[outidx].cfgQMin = pV->out[outidx].cfgQMax;
    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_QDIFF, outidx))) {
      pV->out[outidx].cfgQDiff = atoi(p);
    } else if(outidx > 0) {
      pV->out[outidx].cfgQDiff = pV->out[outidx - 1].cfgQDiff;
    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_QIRATIO, outidx))) {
      pV->out[outidx].cfgIQRatio = atof(p);
    } else if(outidx > 0) {
      pV->out[outidx].cfgIQRatio = pV->out[outidx - 1].cfgIQRatio;
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_QBRATIO, outidx))) {
      pV->out[outidx].cfgBQRatio = atof(p);
    } else if(outidx > 0) {
      pV->out[outidx].cfgBQRatio = pV->out[outidx - 1].cfgBQRatio;
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_Q, outidx))) {
      pV->out[outidx].cfgQ = atoi(p);
    } else if(outidx > 0) {
      pV->out[outidx].cfgQ = pV->out[outidx - 1].cfgQ;
    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_VBVBUF, outidx))) {
      pV->out[outidx].cfgVBVBufSize = strutil_read_numeric(p, 0, 1000, 1000);
    } else if(outidx > 0) {
      pV->out[outidx].cfgVBVBufSize = pV->out[outidx - 1].cfgVBVBufSize;
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_VBVMAX, outidx))) {
      pV->out[outidx].cfgVBVMaxRate = strutil_read_numeric(p, 0, 1000, 1000);
    } else if(outidx > 0) {
      pV->out[outidx].cfgVBVMaxRate = pV->out[outidx - 1].cfgVBVMaxRate;
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_VBVMIN, outidx))) {
      pV->out[outidx].cfgVBVMinRate = strutil_read_numeric(p, 0, 1000, 1000);
    } else if(outidx > 0) {
      pV->out[outidx].cfgVBVMinRate = pV->out[outidx - 1].cfgVBVMinRate;
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_VBVAGGR, outidx))) {
      pV->out[outidx].cfgVBVAggressivity = atof(p);
    } else if(outidx > 0) {
      pV->out[outidx].cfgVBVAggressivity = pV->out[outidx - 1].cfgVBVAggressivity;
    }
    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_VBVINITIAL, outidx))) {
      if((pV->out[outidx].cfgVBVInitial = atof(p)) < 0.0f) {
        pV->out[outidx].cfgVBVInitial = 0.0f;
      } else if(pV->out[outidx].cfgVBVInitial > 1.0f) {
        pV->out[outidx].cfgVBVInitial = 1.0f;
      }
    } else if(outidx > 0) {
      pV->out[outidx].cfgVBVInitial = pV->out[outidx - 1].cfgVBVInitial;
    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_BITRATE, outidx))) {
      pV->out[outidx].cfgBitRateOut = strutil_read_numeric(p, 0, 1000, 1000);
    } else if(outidx > 0) {
      pV->out[outidx].cfgBitRateOut = pV->out[outidx - 1].cfgBitRateOut;
    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_BITRATE_ABRMAX, outidx))) {
      pV->out[outidx].abr.cfgBitRateOutMax = strutil_read_numeric(p, 0, 1000, 1000);
    } else if(outidx > 0) {
      pV->out[outidx].abr.cfgBitRateOutMax = pV->out[outidx - 1].abr.cfgBitRateOutMax;
    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_BITRATE_ABRMIN, outidx))) {
      pV->out[outidx].abr.cfgBitRateOutMin = strutil_read_numeric(p, 0, 1000, 1000);
    } else if(outidx > 0) {
      pV->out[outidx].abr.cfgBitRateOutMin = pV->out[outidx - 1].abr.cfgBitRateOutMin;
    }

    if(pV->out[outidx].abr.cfgBitRateOutMax > 0 || pV->out[outidx].abr.cfgBitRateOutMin > 0) {

      if(pV->out[outidx].abr.cfgBitRateOutMax < pV->out[outidx].abr.cfgBitRateOutMin) {
        pV->out[outidx].abr.cfgBitRateOutMax = pV->out[outidx].abr.cfgBitRateOutMin;
      }
      if(pV->out[outidx].abr.cfgBitRateOutMax < pV->out[outidx].cfgBitRateOut) {
        pV->out[outidx].abr.cfgBitRateOutMax = pV->out[outidx].cfgBitRateOut;
      }
      if(pV->out[outidx].abr.cfgBitRateOutMin > pV->out[outidx].abr.cfgBitRateOutMax) {
        pV->out[outidx].abr.cfgBitRateOutMin = pV->out[outidx].abr.cfgBitRateOutMax;
      }
      if(pV->out[outidx].abr.cfgBitRateOutMin > pV->out[outidx].cfgBitRateOut) {
        pV->out[outidx].abr.cfgBitRateOutMin = pV->out[outidx].cfgBitRateOut;
      }

    }

    if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_BITRATE_TOL, outidx))) {
      pV->out[outidx].cfgBitRateTolOut = strutil_read_numeric(p, 0, 1000, 1000);
    } else if(outidx > 0) {
      pV->out[outidx].cfgBitRateTolOut = pV->out[outidx - 1].cfgBitRateTolOut;
    }

    if(outidx >  0) {
      pV->out[outidx].cfgRateCtrl = pV->out[outidx - 1].cfgRateCtrl;
    }
    if(rc == 0 && (p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_RCTYPE, outidx))) {
      if(!strncasecmp(p, "crf", 3)) {
        pV->out[outidx].cfgRateCtrl = RATE_CTRL_TYPE_CRF;
      } else if(!strncasecmp(p, "cqp", 3)) {
        pV->out[outidx].cfgRateCtrl = RATE_CTRL_TYPE_CQP;
      } else if(!strncasecmp(p, "btrt", 4)) {
        pV->out[outidx].cfgRateCtrl = RATE_CTRL_TYPE_BITRATE;
      } else {
        LOG(X_ERROR("Unrecognized video rate control type: '%s' valid types: btrt, cqp, crf"), p);
        rc = -1;
        break;
      }
    }

    if(rc == 0) {
      if(pV->out[outidx].cfgRateCtrl != RATE_CTRL_TYPE_BITRATE && 
        pV->out[outidx].cfgQ <= 0) {
        LOG(X_ERROR("Video target quantizer ('vq=') not set"));
        rc = -1;
        break;
      } else if(pV->out[outidx].cfgRateCtrl == RATE_CTRL_TYPE_BITRATE &&
                !IS_XC_CODEC_TYPE_VID_RAW(pV->common.cfgFileTypeOut) &&
                 pV->common.cfgFileTypeOut != XC_CODEC_TYPE_UNKNOWN && 
                 pV->out[outidx].cfgBitRateOut <= 0) {
        LOG(X_ERROR("Video bitrate not specified for output %d with 'vb'"), outidx + 1);
        rc = -1;
        break;
      }
    }

    if(rc == 0 && (p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_ISCENEAGGR, outidx))) {
      pV->out[outidx].cfgSceneCut = atoi(p);
    } else if(outidx > 0) {
      pV->out[outidx].cfgSceneCut = pV->out[outidx - 1].cfgSceneCut;
    }

    if(rc == 0) {
      if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_ENCSPEED, outidx))) {
        pV->out[outidx].cfgFast = enc_speed(atoi(p) + 1);
      } else if(outidx > 0) {
        pV->out[outidx].cfgFast = pV->out[outidx - 1].cfgFast;
      } else if(pV->out[outidx].cfgFast == ENCODER_QUALITY_NOTSET) {
        pV->out[outidx].cfgFast = ENCODER_QUALITY_HIGH;
      }
    }

    if(rc == 0) {
      if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_ENCQUALITY, outidx))) {
        pV->out[outidx].cfgFast = enc_qual_to_speed(atoi(p) + 1);
      } else if(outidx > 0) {
        pV->out[outidx].cfgFast = pV->out[outidx - 1].cfgFast;
      } else if(pV->out[outidx].cfgFast == ENCODER_QUALITY_NOTSET) {
        pV->out[outidx].cfgFast = ENCODER_QUALITY_HIGH;
      }
    }

    if(rc == 0 && (p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_PROFILE, outidx))) {
      pV->out[outidx].cfgProfile = atoi(p);
    } else if(outidx > 0) {
      pV->out[outidx].cfgProfile = pV->out[outidx - 1].cfgProfile;
    }

    if(rc == 0 && (p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_SLICESZMAX, outidx))) {
      pV->out[outidx].cfgMaxSliceSz = atoi(p);
    } else if(outidx > 0) {
      pV->out[outidx].cfgMaxSliceSz = pV->out[outidx - 1].cfgMaxSliceSz;
    }
    if(rc == 0 && (p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_FORCEIDR, outidx))) {
      if(IS_PARAM_ON(p)) {
        pV->out[outidx].cfgForceIDR = 1;
      } else {
        pV->out[outidx].cfgForceIDR = 0;
      }
    } else if(outidx > 0) {
      pV->out[outidx].cfgForceIDR = pV->out[outidx - 1].cfgForceIDR;
    }

    pV->out[outidx].cfgGopMaxMs = readGopMs(conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_GOP, outidx),  
                                      outidx > 0 ? &pV->out[outidx - 1].cfgGopMaxMs : NULL);

    pV->out[outidx].cfgGopMinMs = readGopMs(conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_GOPMIN, outidx),  
                                      outidx > 0 ? &pV->out[outidx - 1].cfgGopMinMs : NULL);

    pV->out[outidx].abr.cfgGopMsMax = readGopMs(conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_GOP_ABRMAX, outidx),  
                                      outidx > 0 ? &pV->out[outidx - 1].abr.cfgGopMsMax : NULL);
    pV->out[outidx].abr.cfgGopMsMin = readGopMs(conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_GOP_ABRMIN, outidx),  
                                      outidx > 0 ? &pV->out[outidx - 1].abr.cfgGopMsMin : NULL);
/*
    if(rc == 0 && (p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_GOP, outidx))) {
      if(!strncasecmp(p, "inf", 3)) {
        pV->out[outidx].cfgGopMaxMs = -1;
      } else if(!avc_isnumeric(p)) {
        pV->out[outidx].cfgGopMaxMs = 0;
      } else if((pV->out[outidx].cfgGopMaxMs = atoi(p)) < 0) {
         pV->out[outidx].cfgGopMaxMs = 0;
      } else if(pV->out[outidx].cfgGopMaxMs == 0) {
         //
         // If the GOP was explicitly set to 0 (all key frames), set to 1ms to take effect
         // at the xcode layer
         //
         pV->out[outidx].cfgGopMaxMs = 1;
      }
    } else if(outidx > 0) {
      pV->out[outidx].cfgGopMaxMs = pV->out[outidx - 1].cfgGopMaxMs;
    }
*/

/*
    if(rc == 0 && (p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_GOPMIN, outidx))) {
      if(!avc_isnumeric(p)) {
        pV->out[outidx].cfgGopMinMs = 0;
      } else if((pV->out[outidx].cfgGopMinMs = atoi(p)) < 0) {
        pV->out[outidx].cfgGopMinMs = 0;
      } else if(pV->out[outidx].cfgGopMinMs == 0 && pV->out[outidx].cfgGopMaxMs > 0) {
        pV->out[outidx].cfgGopMinMs = 1;
      }
    } else if(outidx > 0) {
      pV->out[outidx].cfgGopMinMs = pV->out[outidx - 1].cfgGopMinMs;
    }
*/

    //if(pV->out[outidx].cfgGopMinMs == 0 && pV->out[outidx].cfgGopMaxMs > 0) {
    //  if((pV->out[outidx].cfgGopMinMs = (.50 * pV->out[outidx].cfgGopMaxMs)) == 0) {
    //    pV->out[outidx].cfgGopMinMs = 1;
    //  }
    //}

    if(pV->out[outidx].cfgGopMaxMs != -1 && pV->out[outidx].cfgGopMaxMs < pV->out[outidx].cfgGopMinMs) {
      pV->out[outidx].cfgGopMaxMs = pV->out[outidx].cfgGopMinMs;
    }   
    if(pV->out[outidx].cfgGopMaxMs != -1 && pV->out[outidx].cfgGopMinMs > pV->out[outidx].cfgGopMaxMs) {
      pV->out[outidx].cfgGopMinMs = pV->out[outidx].cfgGopMaxMs;
    }

    if(rc == 0 && (p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_LOOKAHEAD, outidx))) {
      pV->out[outidx].cfgLookaheadmin1 = atoi(p);
    } else if(outidx > 0) {
      pV->out[outidx].cfgLookaheadmin1 = pV->out[outidx - 1].cfgLookaheadmin1;
    }

    if(rc == 0 && (p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_MBTREE, outidx))) {
      pV->out[outidx].cfgMBTree = atoi(p);
    } else if(outidx > 0) {
      pV->out[outidx].cfgMBTree = pV->out[outidx - 1].cfgMBTree;
    }

    if(rc == 0 && (p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_QCOMP, outidx))) {
      pV->out[outidx].cfgQCompress = atof(p);
    } else if(outidx > 0) {
      pV->out[outidx].cfgQCompress = pV->out[outidx - 1].cfgQCompress;
    }

    if((p = conf_find_keyval2(kv, XCODE_PARAM_VIDEO_FPS_ABRMAX))) {
      fpsMax = atof(p);
      if(vid_convert_fps(fpsMax, &ui1, &ui2) == 0) {
        pV->out[outidx].abr.cfgOutMaxClockHz = ui1;
        pV->out[outidx].abr.cfgOutMaxFrameDeltaHz = ui2;
      }
    }

    if((p = conf_find_keyval2(kv, XCODE_PARAM_VIDEO_FPS_ABRMIN))) {
      fpsMin = atof(p);
      if(vid_convert_fps(fpsMin, &ui1, &ui2) == 0) {
        pV->out[outidx].abr.cfgOutMinClockHz = ui1;
        pV->out[outidx].abr.cfgOutMinFrameDeltaHz = ui2;
      }
    }

  } // end of for

  return rc;
}


void dump_kvs(const KEYVAL_PAIR_T *pKvs) {
  const KEYVAL_PAIR_T *pKv = pKvs;

  while(pKv) {
    fprintf(stderr, "'%s'->'%s'\n", pKv->key, pKv->val);
    pKv = pKv->pnext;
  }
}

int count_kvs(const KEYVAL_PAIR_T *pKvs) {
  int count = 0;
  const KEYVAL_PAIR_T *pKv = pKvs;

  while(pKv) {
    count++;
    pKv = pKv->pnext;
  }

  return count;
}



static int parse_configstr(const char *pstr, KEYVAL_PAIR_T *pkv, unsigned int numkv, int replace) {

  unsigned int idxKv = 0;
  const char *p, *p2;
  char *pkvfound;
  const XCODE_PARAM_T *pParam;
  char delimeter = '_';
  int rc;

  if(!pstr || !pkv || numkv < 1) {
    return -1;
  }

  //
  // We are appending to any pre-existing populated list
  //
  for(idxKv = 0; idxKv < numkv; idxKv++) {
    if(pkv[idxKv].key[0] == '\0' && pkv[idxKv].val[0] == '\0') {
      break;
    }
  }

  if(idxKv >= numkv) {
    return idxKv;
  }

  if(strstr(pstr, "=")) {
    delimeter = '=';
  }

  //memset(pkv, 0, sizeof(KEYVAL_PAIR_T) * numkv);
  p = pstr;

  while(*p == '"' || *p == '\'') {
    p++;
  }

  while(*p != '\0') {
    p2 = p; 
    while(*p2 != ',' && *p2 != '\0') {
      p2++;
    }

    if((rc = conf_parse_keyval(&pkv[idxKv], p, p2 - p, delimeter, 0)) == 2) {

      pkvfound = NULL;
      if(idxKv > 0) {

        if(replace) {
          if((pkvfound = (char *) conf_find_keyval(&pkv[0], pkv[idxKv].key)) ||
             ((pParam = xcode_find_param_bykey(pkv[idxKv].key)) && 
               (pkvfound = (char *) conf_find_keyval2(pkv, pParam->id)))) {

        //if(replace && (pkvfound = (char *) conf_find_keyval(&pkv[0], pkv[idxKv].key))) {
            memcpy(pkvfound, pkv[idxKv].val, KEYVAL_MAXLEN);
            pkv[idxKv].key[0] = '\0';
            pkv[idxKv].val[0] = '\0';
          }
        }

        if(!pkvfound) {
          pkv[idxKv - 1].pnext = &pkv[idxKv];
        }
      }

      if(!pkvfound) {
        idxKv++;
      }

      if(idxKv >= numkv) {
        break;
      }
    }

    while(*p2 == ',') {
      p2++;
    }
    p = p2;
  }

  //dump_kvs(pkv);

  return (int) idxKv;
}

static int parse_configstr_vid_kv(KEYVAL_PAIR_T *kv, IXCODE_VIDEO_CTXT_T *pXcode, int encoderOpts, int dump) {
  const char *p;
  int setminlookaheadauto = 0;
  int havelookaheadmin1 = 0;
  int setmindecoderthreads = 0;
  //int haveminencoderthreads = 0;
  unsigned int outidx;
  int passthru = -1;
  int haveCodecSetAtStart = 0;
  XC_CODEC_TYPE_T codecType;
  int rc = 0;

  if(!kv || !pXcode) {
    return -1;
  }

  // ac_ac3,as_2,ar_48000,ab_64000,vc_264,x_640,y_360,vb_600,vt_35,fn_2997,fd_100,vf_1,vs_1

  //pXcode->common.cfgFileTypeOut = XC_CODEC_TYPE_UNKNOWN;
  codecType = XC_CODEC_TYPE_UNKNOWN;

  if(pXcode->common.cfgFileTypeOut != XC_CODEC_TYPE_UNKNOWN) {
    haveCodecSetAtStart = 1;
  }

  //
  // Since passthru is always placed into the 0th index, check for global 
  // flag indicating if passthru transcoding is enabled
  //
  if((p = conf_find_keyval2(kv, XCODE_PARAM_VIDEO_PASSTHRU))) {
    if(IS_PARAM_ON(p)) {
      passthru = IXCODE_VIDEO_OUT_MAX - 1;
      pXcode->out[passthru].passthru = 1;
    }
  }

  for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {

    if(rc != 0) {
      break;
    } else if((p = conf_find_keyval_index2(kv, XCODE_PARAM_VIDEO_CODEC, outidx)) || haveCodecSetAtStart) {

      if(!p && haveCodecSetAtStart) {
        codecType = pXcode->common.cfgFileTypeOut;
#if defined(XCODE_HAVE_X264)
      } else if(!strncasecmp(p, "264", 3) || !strncasecmp(p, "h264", 4)) {
        codecType = XC_CODEC_TYPE_H264;
      } else if(!strncasecmp(p, "x264", 4)) {
        codecType = XC_CODEC_TYPE_H264;
        pXcode->common.cfgFlags = 1;
#endif // XCODE_HAVE_X264
      } else if(!strncasecmp(p, "mpg4", 4)) {
        codecType = XC_CODEC_TYPE_MPEG4V;
      } else if(!strncasecmp(p, "h263+", 5) || !strncasecmp(p, "h263p", 5)) {
        codecType = XC_CODEC_TYPE_H263_PLUS;
      } else if(!strncasecmp(p, "h263", 4)) {
        codecType = XC_CODEC_TYPE_H263;
#if defined(XCODE_HAVE_VP8)
      } else if(!strncasecmp(p, "vp8", 3)) {
        codecType = XC_CODEC_TYPE_VP8;
#endif // XCODE_HAVE_VP8
      } else if(!strncasecmp(p, "rgba", 4)) {
        codecType = XC_CODEC_TYPE_RAWV_RGBA32;
      } else if(!strncasecmp(p, "bgra", 4)) {
        codecType = XC_CODEC_TYPE_RAWV_BGRA32;
      } else if(!strncasecmp(p, "rgb565", 6)) {
        codecType = XC_CODEC_TYPE_RAWV_RGB565;
      } else if(!strncasecmp(p, "bgr565", 6)) {
        codecType = XC_CODEC_TYPE_RAWV_BGR565;
      } else if(!strncasecmp(p, "rgb", 3)) {
        codecType = XC_CODEC_TYPE_RAWV_RGB24;
      } else if(!strncasecmp(p, "bgr", 3)) {
        codecType = XC_CODEC_TYPE_RAWV_BGR24;
      } else if(!strncasecmp(p, "nv12", 4)) {
        codecType = XC_CODEC_TYPE_RAWV_NV12;
      } else if(!strncasecmp(p, "nv21", 4) || !strncasecmp(p, "yuv420sp", 8)) {
        codecType = XC_CODEC_TYPE_RAWV_NV21;
      } else if(!strncasecmp(p, "yuva420p", 8))  {
        codecType = XC_CODEC_TYPE_RAWV_YUVA420P;
      } else if(!strncasecmp(p, "yuv420p", 7) || !strncasecmp(p, "yuv", 3)) {
        codecType = XC_CODEC_TYPE_RAWV_YUV420P;
      } else if(!strncasecmp(p, "same", 4) || !strncasecmp(p, "passthru", 8)) {
        //codecType = XC_CODEC_TYPE_UNKNOWN;

        //
        // If pass-thru is selected, it is always part of the first xcode output
        //
        passthru = outidx;
        pXcode->out[outidx].passthru = 1;

      } else {
        pXcode->common.cfgDo_xcode = 0;
        LOG(X_ERROR("Unsupported xcode video codec '%s'"), p);
        rc = -1;
      }

      if(haveCodecSetAtStart) {
        pXcode->common.cfgFileTypeOut = XC_CODEC_TYPE_UNKNOWN;
        haveCodecSetAtStart = 0;
      }

      if(rc == 0 && pXcode->common.cfgFileTypeOut != XC_CODEC_TYPE_UNKNOWN &&
        codecType != pXcode->common.cfgFileTypeOut) {

        pXcode->common.cfgDo_xcode = 0;
        LOG(X_ERROR("Multiple transcoded output must use the same output codec"));
        rc = -1;
      } else if(rc == 0 && codecType != XC_CODEC_TYPE_UNKNOWN) {
        pXcode->common.cfgFileTypeOut = codecType;
        pXcode->common.cfgDo_xcode = 1;
      }

    }
  }

  if(pXcode->common.cfgDo_xcode) {

    if(rc == 0) {
      pXcode->out[0].active = 1;
      pXcode->cfgThreadsDec = XCODE_VID_THREADS_DEC_NOTSET;
      rc = parse_configstr_vid_general(pXcode, kv);
    }

    if(rc == 0 && encoderOpts) {

      rc = parse_configstr_vid_encoder(pXcode, kv);

      //
      // For low frame rates, minimize encoder lookahead to prevent 
      // excessive buffering time overrunning audio stream_net_pes frame queue
      //
      if(rc == 0 && pXcode->cfgOutClockHz > 0 && 
        ((float)pXcode->cfgOutClockHz / pXcode->cfgOutFrameDeltaHz) <= XCODE_VID_LOOKAHEAD_MIN_FPS) {

        for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
          if(!pXcode->out[outidx].active) {
            continue;
          }

          if(pXcode->out[outidx].cfgLookaheadmin1 == XCODE_VID_LOOKAHEAD_AUTO_FLAG) {
            setminlookaheadauto = 1;
            pXcode->out[outidx].cfgLookaheadmin1 = pXcode->out[outidx].resLookaheadmin1 = 1;
          }

          if((pXcode->out[outidx].cfgLookaheadmin1 & ~XCODE_VID_LOOKAHEAD_AUTO_FLAG) == 1) {
            havelookaheadmin1 = 1;
          }
          if(pXcode->out[outidx].cfgThreads == 1) {
            setmindecoderthreads = 1;
          }
        }

        if(setminlookaheadauto) {
          LOG(X_WARNING("Setting minimum encoder lookahead for low fps %u/%u"),
            pXcode->cfgOutClockHz, pXcode->cfgOutFrameDeltaHz);
        }
      }


      if(rc >= 0) {

        if(pXcode->cfgThreadsDec == XCODE_VID_THREADS_DEC_NOTSET) {
          pXcode->cfgThreadsDec = setmindecoderthreads ? 1 : get_threads_auto();
        }
 
        //TODO: warn if encoder / decoder threads > 0 & cfgLookaheadmin1 is set
        //if(havelookaheadmin1 && pXcode->cfgThreadsDec > 1) {
        //}

        //
        // Move the passthru index to 0th
        //
        if(passthru >= 1) {
          for(outidx = passthru; outidx >= 1; outidx--) {
            memcpy(&pXcode->out[outidx], &pXcode->out[outidx -1], sizeof(pXcode->out[outidx]));
          }
          pXcode->out[0].passthru = 1;
          pXcode->out[0].active = 1;
        }

//for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
//  fprintf(stderr, "outidx[%d] active:%d, pasthru:%d bitrate;%d\n", outidx, pXcode->out[outidx].active, pXcode->out[outidx].passthru, pXcode->out[outidx].cfgBitRateOut);
//}

        if(dump) {
          for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
            if(pXcode->out[outidx].active) {
              xcode_dump_conf_video(pXcode, outidx, rc, S_DEBUG);
            }
          } // end of for
        }


      } // end of if(rc >= 0
    }

  }

  return rc >= 0 ? 0: rc;
}

void xcode_dump_conf_video_abr(IXCODE_VIDEO_CTXT_T *pXcode, unsigned int outidx, int logLevel) {
  char buf[64];

  if(pXcode->out[outidx].abr.cfgGopMsMax == -1) {
    snprintf(buf, sizeof(buf), "[%d ms - infinite", pXcode->out[outidx].abr.cfgGopMsMin);
  } else {
    snprintf(buf, sizeof(buf), "[%d - %d]ms", pXcode->out[outidx].abr.cfgGopMsMin,
                                              pXcode->out[outidx].abr.cfgGopMsMax);
  }

  LOG(logLevel, "video abr xcode[%d] config: vb:%.1f - %.1f Kb/s, fps:%d/%d - %d/%d, gop:%s\n",
      outidx, (float)pXcode->out[outidx].abr.cfgBitRateOutMin/1000.0f,
      (float)pXcode->out[outidx].abr.cfgBitRateOutMax/1000.0f,
      pXcode->out[outidx].abr.cfgOutMinClockHz, pXcode->out[outidx].abr.cfgOutMinFrameDeltaHz,
      pXcode->out[outidx].abr.cfgOutMaxClockHz, pXcode->out[outidx].abr.cfgOutMaxFrameDeltaHz, buf);

}

void xcode_dump_conf_video(IXCODE_VIDEO_CTXT_T *pXcode, unsigned int outidx, int rc, int logLevel) {

  int resOutH, resOutV;
  char buf[2][64];

  if(pXcode->out[outidx].passthru) {
    LOG(logLevel, "video xcode[%d] pass-through\n", outidx);
    return;
  }

  if(pXcode->out[outidx].cfgGopMaxMs == -1) {
    snprintf(buf[0], sizeof(buf[0]), "infinite");
  } else {
    snprintf(buf[0], sizeof(buf[0]), "[%d - %d]ms", pXcode->out[outidx].cfgGopMinMs,
                                              pXcode->out[outidx].cfgGopMaxMs);
  }

  if(pXcode->cfgThreadsDec > 0) {
    snprintf(buf[1], sizeof(buf[1]), " (dec:%d)", pXcode->cfgThreadsDec);
  } else {
    buf[1][0] = '\0';
  }

  resOutH = pXcode->out[outidx].cfgOutH;
  resOutV =  pXcode->out[outidx].cfgOutV;
  if(resOutH == 0 && resOutV == 0) {
    resOutH = pXcode->out[outidx].cfgOutPipH;
    resOutV =  pXcode->out[outidx].cfgOutPipV;
  }

  LOG(logLevel, "video xcode[%d] config %s: %s %d x %d pad: {l:%d, t:%d, r:%d, b:%d}, "
                "%s q:%d qmin:%d, qmax:%d, qdiff:%d, vb:%.1f(+/- %.1f) Kb/s, "
                "vbv size: %.1f Kb, max:%.1f Kb/s, min:%.1f Kb/s, factor:%.2f, init:%.2f, "
                 "fps:%d/%d, upsmpl:%d, fast:%d, profile:%d, gop:%s, cfr:(%d,%d,tol:%dms), "
                 "vslmax:%d, "
                 "lookaheadmin1:%d, threads:%d%s, scale type:%d, scene I:%d, "
                 "crop: {l:%d, t:%d, r:%d, b:%d}\n", outidx,
   AVC_RC_STR(rc), codecType_getCodecDescrStr(pXcode->common.cfgFileTypeOut), resOutH, resOutV,

   pXcode->out[outidx].crop.padLeftCfg, pXcode->out[outidx].crop.padTopCfg,
   pXcode->out[outidx].crop.padRightCfg, pXcode->out[outidx].crop.padBottomCfg,
   (pXcode->out[outidx].cfgRateCtrl == RATE_CTRL_TYPE_CQP ? "cqp" :
   (pXcode->out[outidx].cfgRateCtrl == RATE_CTRL_TYPE_CRF ? "crf" : "btrt")),
   ((pXcode->out[outidx].cfgRateCtrl == RATE_CTRL_TYPE_CQP ||
   pXcode->out[outidx].cfgRateCtrl == RATE_CTRL_TYPE_CRF) ? pXcode->out[outidx].cfgQ : 0),
   pXcode->out[outidx].cfgQMin, pXcode->out[outidx].cfgQMax, pXcode->out[outidx].cfgQDiff,
   (float)pXcode->out[outidx].cfgBitRateOut/1000.0f, (float)pXcode->out[outidx].cfgBitRateTolOut/1000.0f,
   (float)pXcode->out[outidx].cfgVBVBufSize/1000.0f, (float)pXcode->out[outidx].cfgVBVMaxRate/1000.0f,
   (float)pXcode->out[outidx].cfgVBVMinRate/1000.0f, pXcode->out[outidx].cfgVBVAggressivity,
   pXcode->out[outidx].cfgVBVInitial,
   pXcode->cfgOutClockHz, pXcode->cfgOutFrameDeltaHz,
   pXcode->common.cfgAllowUpsample, pXcode->out[outidx].cfgFast - 1,
   pXcode->out[outidx].cfgProfile, buf[0], pXcode->common.cfgCFRInput,
   pXcode->common.cfgCFROutput, (int) PTSF_MS(pXcode->common.cfgCFRTolerance),
   pXcode->out[outidx].cfgMaxSliceSz,
   (pXcode->out[outidx].cfgLookaheadmin1 & ~XCODE_VID_LOOKAHEAD_AUTO_FLAG),
   pXcode->out[outidx].cfgThreads, buf[1],
   pXcode->out[outidx].cfgScaler, pXcode->out[outidx].cfgSceneCut,
   pXcode->out[outidx].crop.cropLeft, pXcode->out[outidx].crop.cropTop,
   pXcode->out[outidx].crop.cropRight, pXcode->out[outidx].crop.cropBottom);

}

void xcode_dump_conf_audio(IXCODE_AUDIO_CTXT_T *pXcode, unsigned int outidx, int rc, int logLevel) {

  LOG(X_DEBUG("audio xcode config %s: %s channels:%d, %dHz, %.1f Kb/s total, vol:%d (force:%d)"),
      AVC_RC_STR(rc), codecType_getCodecDescrStr(pXcode->common.cfgFileTypeOut),
      pXcode->cfgChannelsOut, pXcode->cfgSampleRateOut,
      (float)pXcode->cfgBitRateOut/1000.0f, pXcode->cfgVolumeAdjustment, pXcode->cfgForceOut);

}


int xcode_parse_configstr_vid(const char *pstr, IXCODE_VIDEO_CTXT_T *pXcode, int encoderOpts, int dump) {

  KEYVAL_PAIR_T kv[XCODE_MAX_KEYVALS];
  int idxKv = 0;
  int rc = 0;

  if(!pstr || !pXcode) {
    return -1;
  }

  memset(kv, 0, sizeof(kv));

  if((idxKv = parse_configstr(pstr, kv, sizeof(kv) / sizeof(kv[0]), 1)) < 0) {
    return -1;
  } else if(idxKv > 0) {
    //dump_kvs(kv);
    rc = parse_configstr_vid_kv(kv, pXcode, encoderOpts, dump);
  }

  return rc >= 0 ? idxKv : rc;
}

static int set_audio_codec_kv(KEYVAL_PAIR_T *kv, IXCODE_AUDIO_CTXT_T *pXcode) {
  const char *p;
  int rc = 0;
  int haveCodecSetAtStart = 0;

  if(pXcode->common.cfgFileTypeOut != XC_CODEC_TYPE_UNKNOWN) {
    haveCodecSetAtStart = 1;
  }

  if((p = conf_find_keyval2(kv, XCODE_PARAM_AUDIO_CODEC)) || haveCodecSetAtStart) {

    pXcode->common.cfgDo_xcode = 1;

    if(!p && haveCodecSetAtStart) {

#if defined(XCODE_HAVE_AC3)
    } else if(!strncasecmp(p, "ac3", 3)) {
      pXcode->common.cfgFileTypeOut = XC_CODEC_TYPE_AC3;
#endif // XCODE_HAVE_AC3
#if defined(XCODE_HAVE_AAC)
    } else if(!strncasecmp(p, "aac", 3)) {
      pXcode->common.cfgFileTypeOut = XC_CODEC_TYPE_AAC;
#endif // XCODE_HAVE_AAC
#if defined(XCODE_HAVE_AMR)
    } else if(!strncasecmp(p, "amrnb", 5) || !strncasecmp(p, "amr", 3)) {
      pXcode->common.cfgFileTypeOut = XC_CODEC_TYPE_AMRNB;
      pXcode->cfgSampleRateOut = 8000;
#endif // XCODE_HAVE_AMR
#if defined(XCODE_HAVE_VORBIS)
    } else if(!strncasecmp(p, "vorbis", 6)) {
      pXcode->common.cfgFileTypeOut = XC_CODEC_TYPE_VORBIS;
#endif // XCODE_HAVE_VORBIS
#if defined(XCODE_HAVE_SILK)
    } else if(!strncasecmp(p, "silk", 4)) {
      pXcode->common.cfgFileTypeOut = XC_CODEC_TYPE_SILK;
#endif // XCODE_HAVE_SILK
#if defined(XCODE_HAVE_OPUS)
    } else if(!strncasecmp(p, "opus", 4)) {
      pXcode->common.cfgFileTypeOut = XC_CODEC_TYPE_OPUS;
#endif // XCODE_HAVE_OPUS
    } else if(!strncasecmp(p, "g711a", 5)) {
      pXcode->common.cfgFileTypeOut = XC_CODEC_TYPE_G711_ALAW;
      pXcode->cfgSampleRateOut = 8000;
    } else if(!strncasecmp(p, "g711", 4)) {
      pXcode->common.cfgFileTypeOut = XC_CODEC_TYPE_G711_MULAW;
      pXcode->cfgSampleRateOut = 8000;
    } else if(!strncasecmp(p, "pcm", 3)) {
      pXcode->common.cfgFileTypeOut = XC_CODEC_TYPE_RAWA_PCM16LE;
    } else if(!strncasecmp(p, "ulaw", 4)) {
      pXcode->common.cfgFileTypeOut = XC_CODEC_TYPE_RAWA_PCMULAW;
    } else if(!strncasecmp(p, "alaw", 4)) {
      pXcode->common.cfgFileTypeOut = XC_CODEC_TYPE_RAWA_PCMALAW;
    } else {
      pXcode->common.cfgDo_xcode = 0;
      LOG(X_ERROR("Unsupported xcode audiocodec '%s'"), p);
      rc = -1;
    }
  }

  return rc;
}

static int parse_configstr_aud_kv(KEYVAL_PAIR_T *kv, IXCODE_AUDIO_CTXT_T *pXcode, int encoderOpts, 
                                  int dump) {

  const char *p;
  int rc = 0;

  if(!kv || !pXcode) {
    return -1;
  }

  //pXcode->common.cfgFileTypeIn = MEDIA_FILE_TYPE_UNKNOWN;
  //pXcode->common.cfgVerbosity = g_verbosity;

  //
  // Get Audio options
  //
  //if(encoderOpts) {
    rc = set_audio_codec_kv(kv, pXcode);
  //}

  if(encoderOpts && pXcode->common.cfgDo_xcode) {

    if(rc == 0 && (p = conf_find_keyval2(kv, XCODE_PARAM_AUDIO_CHANNELS))) {
      pXcode->cfgChannelsOut = atoi(p);
    }
    if(rc == 0 && (p = conf_find_keyval2(kv, XCODE_PARAM_AUDIO_SAMPLERATE))) {

      pXcode->cfgSampleRateOut = atoi(p);
    }
    if(rc == 0 && (p = conf_find_keyval2(kv, XCODE_PARAM_AUDIO_BITRATE))) {
      pXcode->cfgBitRateOut = atoi(p) * 
             (pXcode->cfgChannelsOut > 0 ? pXcode->cfgChannelsOut : 1);
    }
    if(rc == 0 && (p = conf_find_keyval2(kv, XCODE_PARAM_AUDIO_VOLUME))) {
      pXcode->cfgVolumeAdjustment = atoi(p);
    }
    if(rc == 0 && (p = conf_find_keyval2(kv, XCODE_PARAM_AUDIO_PROFILE))) {
      pXcode->cfgProfile = atoi(p);
    }
    //if(rc == 0 && (p = conf_find_keyval2(kv, XCODE_PARAM_AUDIO_NODECODE))) {
    //  pXcode->common.cfgNoDecode = atoi(p);
    //}
    if(rc == 0 && (p = conf_find_keyval2(kv, XCODE_PARAM_AUDIO_FORCE))) {
      pXcode->cfgForceOut = atoi(p);
    }

    if(dump) {
      xcode_dump_conf_audio(pXcode, 0, rc, S_DEBUG);
    }
  }

  return rc >= 0 ? 0: rc;
}

int xcode_parse_configstr_aud(const char *pstr, IXCODE_AUDIO_CTXT_T *pXcode, 
                              int encoderOpts, int dump) {

  KEYVAL_PAIR_T kv[XCODE_MAX_KEYVALS];
  int idxKv = 0;
  int rc = 0;

  if(!pstr || !pXcode) {
    return -1;
  }

  memset(kv, 0, sizeof(kv));

  if((idxKv = parse_configstr(pstr, kv, sizeof(kv) / sizeof(kv[0]), 1)) < 0) {
    return -1;
  } else if(idxKv > 0) {
    rc = parse_configstr_aud_kv(kv, pXcode, encoderOpts, dump);
  }

  return rc >= 0 ? idxKv : rc;
}


typedef struct XCODE_PARSE_CTXT {
  const char                      *xcodecfgfile;
  int                              inx;
  int                              iny;
  LEX_CONDITIONAL_CONTEXT_T        parse;
  KEYVAL_PAIR_T                    kvs[XCODE_MAX_KEYVALS];
} XCODE_PARSE_CTXT_T;



int xcode_lexical_conditional_cb(LEX_CONDITIONAL_CONTEXT_T *pCtxt, const char *str) {
  int rc = 0;
  KEYVAL_PAIR_T kv;
  const char *val;
  const char *pstr = str;
  int *pcompareval = NULL;
  XCODE_PARSE_CTXT_T *pXCtxt;
  int rc_conditional = 1;
  int idxKv = 0;

  if(!pCtxt || !pCtxt->pCbArg || !str) {
    return -1;
  }

  pXCtxt = (XCODE_PARSE_CTXT_T *) pCtxt->pCbArg;

  //if((rc = conf_parse_keyval(&kv, str, 0, '=')) == 2 && (val = avc_dequote(kv.val, NULL, 0))) {
  if((rc = conf_parse_keyval(&kv, str, 0, '=', 1)) == 2 && (val = kv.val)) {

    if(pXCtxt->parse.test.comparison != LEX_COMPARISON_OPERATOR_NONE) {

      if(!strcasecmp(pXCtxt->parse.test.var_name, "inputWidth")) {
        pcompareval = &pXCtxt->inx;
      } else if(!strcasecmp(pXCtxt->parse.test.var_name, "inputHeight")) {
        pcompareval = &pXCtxt->inx;
      } else {
        LOG(X_ERROR("Invalid comparison variable name '%s'"), pXCtxt->parse.test.var_name);
        return -1;
      }

      rc_conditional = lexical_compare_int(&pXCtxt->parse.test, *pcompareval);

    }

    if(rc_conditional) {
      //fprintf(stderr, "--- '%s' -> '%s'\n", kv.key, val );

      //
      // Look for 'xcode=' or 'xcodeArgs='
      //
      if(!strcasecmp(kv.key, SRV_CONF_KEY_XCODEARGS) || !strcasecmp(kv.key, PIP_KEY_XCODE)) {
        pstr = val;
      }
      if((idxKv = parse_configstr(pstr, pXCtxt->kvs, 
              sizeof(pXCtxt->kvs) / sizeof(pXCtxt->kvs[0]), 1)) < 0) {
        return -1;
      }

    //fprintf(stderr, "EXPRESSION '%s'->'%s', variable:'%s', comparison:%d, constval:%f state:%d numstatements:%d, idxKv:%d rc_cond:%d, val:%d\n", kv.key, val, pCtxt->test.var_name, pCtxt->test.comparison, pCtxt->test.constval, pCtxt->state, pCtxt->numassignments, idxKv, rc_conditional, pcompareval ? *pcompareval : -1); dump_kvs(&pXCtxt->kvs[0]);
    }


  }

  return 0;
}

static int parse_xcfg(XCODE_PARSE_CTXT_T *pCtxt) {
  int rc = 0;
  FILE_HANDLE fp;
  char *p;
  unsigned int linenum = 0;
  char buf[1024];

  if((fp = fileops_Open(pCtxt->xcodecfgfile, O_RDONLY)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open file for reading: %s"), pCtxt->xcodecfgfile);
    return -1;
  }

  while(fileops_fgets(buf, sizeof(buf) - 1, fp)) {

    linenum++;
    p = buf;    
    MOVE_WHILE_SPACE(p);

    if(*p == '\0' || *p == '#' || *p == '\r' || *p == '\n') {
      continue;
    }

    if((rc = lexical_parse_file_line(p, &pCtxt->parse)) < 0) {
      LOG(X_ERROR("Syntax error in file %s, line:%d %s"), pCtxt->xcodecfgfile, linenum, p); 
      break;
    }

  }

  fileops_Close(fp);

  if(rc >= 0) {
    rc = count_kvs(pCtxt->kvs);
  }

  //fprintf(stderr, "KVS:%d\n",rc);dump_kvs(pCtxt->kvs);

  return rc;
}

int xcode_parse_xcfg(STREAM_XCODE_DATA_T *pData, int dump) {
  int rc = 0;
  int idxKv = 0;
  const char *path;
  IXCODE_CTXT_T *pXcode;
  STREAM_XCODE_VID_UDATA_T *pUDataV;
  XCODE_PARSE_CTXT_T ctxt;

  if(!pData || !pData->piXcode) {
    return -1;
  }

  pXcode = pData->piXcode;
  if(!(pUDataV = (STREAM_XCODE_VID_UDATA_T *) pXcode->vid.pUserData)) {
    return -1;
  }

  if(!(path = pUDataV->xcodecfgfile)) {
    return -1;
  }

  memset(&ctxt, 0, sizeof(ctxt));
  ctxt.xcodecfgfile = path;
  ctxt.parse.cbFunc = xcode_lexical_conditional_cb;
  ctxt.parse.pCbArg = &ctxt;
  ctxt.inx = pUDataV->seqHdrWidth;
  ctxt.iny = pUDataV->seqHdrHeight;

  LOG(X_DEBUG("Processing xcode config file %s for input dimensions %dx%d"),
      ctxt.xcodecfgfile, ctxt.inx, ctxt.iny);

  if((idxKv = parse_xcfg(&ctxt)) < 0) {
    return -1;
  }

  //dump_kvs(ctxt.kvs);

  if(rc >= 0) {
    rc = parse_configstr_vid_kv(ctxt.kvs, &pXcode->vid, 1, dump);
  }

  if(rc >= 0) {
    rc = parse_configstr_aud_kv(ctxt.kvs, &pXcode->aud, 1, dump);
  }

  if(idxKv == 0) {
    LOG(X_ERROR("No xcode configuration found in %s for input resolution %dx%d"), 
                path, ctxt.inx, ctxt.iny);
    rc = -1;
  } else if(idxKv > 0 &&
      !pXcode->aud.common.cfgDo_xcode && !pXcode->vid.common.cfgDo_xcode) {
    LOG(X_ERROR("Invalid xcode configuration found in %s for input resolution %dx%d"), 
                path, ctxt.inx, ctxt.iny);
    rc = -1;
  }

  return rc;
}

int xcode_parse_configstr(const char *pstr, IXCODE_CTXT_T *pXcode, 
                          int encoderOpts, int dump) {

  int idxKv = 0;
  struct stat st;
  int rc = 0;
  unsigned int outidx;
  XCODE_PARSE_CTXT_T ctxt;

  if(!pstr || !pXcode) {
    return -1;
  }

  //
  // Init any required settings
  //
  pXcode->vid.common.cfgFileTypeIn = XC_CODEC_TYPE_UNKNOWN;
  pXcode->vid.common.cfgVerbosity = g_verbosity;
  pXcode->vid.out[0].cfgRateCtrl = RATE_CTRL_TYPE_BITRATE;
  pXcode->vid.out[0].cfgLookaheadmin1 |= XCODE_VID_LOOKAHEAD_AUTO_FLAG;
  pXcode->vid.out[0].cfgMBTree = -1;
  pXcode->vid.out[0].cfgQCompress = -1;

  pXcode->aud.common.cfgFileTypeIn = XC_CODEC_TYPE_UNKNOWN;
  pXcode->aud.common.cfgVerbosity = g_verbosity;

  //
  // Check if the input argument is actually the path of an xcode config file 
  //
  if(fileops_stat(pstr, &st) == 0) {

    LOG(X_DEBUG("Will load xcode config from file %s after processing input"), pstr);

    memset(&ctxt, 0, sizeof(ctxt));
    ctxt.xcodecfgfile = pstr;
    ctxt.parse.cbFunc = xcode_lexical_conditional_cb;
    ctxt.parse.pCbArg = &ctxt;

    if(parse_xcfg(&ctxt) < 0) {
      return -1;
    }

    //
    // Check if audio xcode is enabled
    // this will also set pXcode->aud.common.cfgDo_xcode accordingly
    //
    if((rc = set_audio_codec_kv(ctxt.kvs, &pXcode->aud)) < 0) {
      return -1;
    }

    if((conf_find_keyval2(ctxt.kvs, XCODE_PARAM_VIDEO_CODEC))) {
      pXcode->vid.common.cfgDo_xcode = 1;

      //
      // Temporarily mark all output encodes as active to allow the creation of output
      // formatters / packetizers.  The active flag should be disabled for any
      // non-active encoder indexes after the delayed xcfg load is actually done
      // for the specific input parameters such as input resolution
      //
      for(outidx = 0; outidx < IXCODE_VIDEO_OUT_MAX; outidx++) {
        pXcode->vid.common.cfgFileTypeOut = XC_CODEC_TYPE_H264;
        pXcode->vid.out[outidx].active = 1;
      }

    } else {
      LOG(X_ERROR("xcode config file %s does not contain video codec description"), ctxt.xcodecfgfile);
      return -1;
    }

    return -2;
  }

  if(rc >= 0 && (rc = xcode_parse_configstr_vid(pstr, &pXcode->vid, encoderOpts, dump)) >= 0) {
    idxKv += rc;
  }

  if(rc >= 0 && (rc = xcode_parse_configstr_aud(pstr, &pXcode->aud, encoderOpts, dump)) >= 0) {
    idxKv += rc;
  }

  if(idxKv == 0) {
    LOG(X_ERROR("Invalid xcode configuration string or filename '%s'"), pstr);
    rc = -1;
  } else if(idxKv > 0 && 
      !pXcode->aud.common.cfgDo_xcode && !pXcode->vid.common.cfgDo_xcode) { 
    LOG(X_ERROR("Invalid xcode format string '%s'"), pstr);
    rc = -1;
  }

  //fprintf(stderr, "vid do_xcode:%d aud do_xcode:%d\n", pXcode->vid.common.cfgDo_xcode, pXcode->aud.common.cfgDo_xcode);

  return rc >= 0 ? 0 : -1;
}
