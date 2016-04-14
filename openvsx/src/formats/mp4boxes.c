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


#define READ_UINT8(psrc, pos)      *((uint8_t *) &((unsigned char *)psrc)[pos++]);
#define READ_UINT16(psrc, pos) ntohs(  *((uint16_t *) &((unsigned char *)psrc)[pos])); (pos)+=2;
#define READ_UINT32(psrc, pos) ntohl(  *((uint32_t *) &((unsigned char *)psrc)[pos])); (pos)+=4;
//#define READ_UINT64(psrc, pos) (  *((uint64_t *) &((unsigned char *)psrc)[pos])); (pos)+=8;
#define READ_UINT64(psrc, pos) \
                       (((uint64_t) htonl(*((uint32_t *) &((unsigned char *)psrc)[pos]))) << 32) | \
                        ((uint64_t) htonl(*((uint32_t *) &((unsigned char *)psrc)[pos+4]))); (pos)+=8;

#define WRITE_UINT8(psrc, pos, val)   ((unsigned char *)psrc)[(pos)++] = (uint8_t) (val); 
#define WRITE_UINT16(psrc, pos, val)  *((uint16_t *) &((unsigned char *)psrc)[pos]) = (uint16_t) (val); (pos)+=2;
#define WRITE_UINT32(psrc, pos, val)  *((uint32_t *) &((unsigned char *)psrc)[pos]) = (uint32_t) (val); (pos)+=4;
#define WRITE_UINT64(psrc, pos, val)  *((uint64_t *) &((unsigned char *)psrc)[pos]) = (uint64_t) (val); (pos)+=8;

#define PRINT_INDENT(fp, ctx) while(ctx-- > 1) { fprintf(fp, " |"); }

                                                    



static const char *getPrintableFixedPt16x16(uint32_t fixedPt) {
  static char buf[64];
  double f = (fixedPt & 0x0000ffff) / 0xffff;

  snprintf(buf, sizeof(buf) - 1, "%u.%.5u", fixedPt >> 16, (unsigned int)(f * 10000));

  return buf;
}

static const char *getPrintableFixedPt8x8(uint16_t fixedPt) {
  static char buf[64];
  double f = (fixedPt & 0x00ff) / 0xff;

  snprintf(buf, sizeof(buf) - 1, "%u.%.4u", fixedPt >> 8, (unsigned int)(f * 1000));

  return buf;
}

static const char *getPrintableTm(uint64_t sec) {
  static char buf[128];
  time_t tm = 0;

  if(sec > SEC_BETWEEN_EPOCH_1904_1970) {
    tm = sec - SEC_BETWEEN_EPOCH_1904_1970;
  }

  strftime(buf, sizeof(buf)-1, "%H:%M:%S %m/%d/%Y", localtime(&tm));

  return buf;
}

void dump_box(BOX_T *pBox, int context, FILE *fp) {

  char *pDescription = NULL;
  if(pBox->handler) {
    pDescription = ((BOX_HANDLER_T *)pBox->handler)->description;
  }

  PRINT_INDENT(fp, context);

  if(pBox->type != 0) {
    fprintf(fp, " * %c%c%c%c ",
          ((unsigned char *)&pBox->type)[0], ((unsigned char *)&pBox->type)[1], 
          ((unsigned char *)&pBox->type)[2], ((unsigned char *)&pBox->type)[3]);
    if(pDescription && pDescription[0] != '\0') {
      fprintf(fp, "(%s) ", pDescription);
    }
  }
  fprintf(fp, "size %"LL64"u ", pBox->szhdr + pBox->szdata);
  if(pBox->szhdr + pBox->szdata_ownbox > 0) {
    fprintf(fp, "(%u) ", pBox->szhdr + pBox->szdata_ownbox);
  }

  if(pBox->fileOffsetData > 0) {
    fprintf(fp, "[%"LL64"u (0x%"LL64"x) - %"LL64"u 0x%"LL64"x)]", 
          pBox->fileOffset, pBox->fileOffset,
          (FILE_OFFSET_T)(pBox->szdata + pBox->fileOffsetData - 1), 
          (FILE_OFFSET_T)(pBox->szdata + pBox->fileOffsetData - 1));
  }
  fprintf(fp, "\n");
}



static BOX_T *read_box_genericsubs(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                                   unsigned char *arena, unsigned int sz_arena,
                                   int *prc) {

  BOX_T *pBox = (BOX_T *) avc_calloc(1, sizeof(BOX_T));
  memcpy(pBox, pBoxHdr, sizeof(BOX_T));
  pBox->subs = 1;
  pBox->szdata_ownbox = (uint32_t)(pBoxHdr->fileOffsetData - pBoxHdr->fileOffset) - pBox->szhdr;

  return pBox;
}

unsigned int mp4_write_box_header(BOX_T *pBox, unsigned char *buf, int sz) {

  unsigned int pos = 0;

  if(pBox->szdata > 0xffffffe7) {
    WRITE_UINT32(buf, pos, htonl(1));
  } else {
    WRITE_UINT32(buf, pos, htonl((uint32_t)pBox->szdata + pBox->szhdr));
  }

  WRITE_UINT32(buf, pos, pBox->type);

  if(pBox->szdata > 0xffffffe7) {
      WRITE_UINT64(buf, pos, pBox->szdata + pBox->szhdr);
  }

  if(pBox->type == *((uint32_t *) "uuid")) {
    memcpy(&buf[pos], pBox->usertype, 16);
    pos += 16;
  }

  return pos;
}



static BOX_T *read_box_mdat(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  //int rc;
  BOX_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  //TODO: add support for mp4 > 4GB
  //fprintf(stderr, "MDAT LEN:%u\n", len);
  // length 0 assumes until eof
  if(len == 0) {
    len = (uint32_t)(pStream->size - pStream->offset);
    pBoxHdr->szdata = (uint64_t) len;
  } else if(len < 8) {
    LOG(X_ERROR("Invalid mdat body length: %u"), len);
    return NULL;
  }

  pBox = (BOX_T *) avc_calloc(1, sizeof(BOX_T));
  memcpy(pBox, pBoxHdr, sizeof(BOX_T));

  if(pStream->cbSeek(pStream, len, SEEK_CUR) < 0) {
    return NULL;
  }

  return (BOX_T *) pBox;
}

static BOX_T *read_box_ftyp(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  unsigned int pos = 0;
  BOX_FTYP_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 8 || len > sz_arena) {
    LOG(X_ERROR("Invalid ftyp body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, len)) < 0) {  
    return NULL;
  }
  pBox = (BOX_FTYP_T *) avc_calloc(1, sizeof(BOX_FTYP_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->major, arena, 8);
  pos += 8;

  if((pBox->lencompatbrands = len - pos) % 4 != 0) {
    pBox->lencompatbrands += (pBox->lencompatbrands % 4);
  }
  if(pBox->lencompatbrands > 0) {
    pBox->pcompatbrands = (uint32_t *) avc_calloc(pBox->lencompatbrands, 1);
    memcpy(pBox->pcompatbrands, &arena[pos], len - pos);
  }

  return (BOX_T *) pBox;
}

static int write_box_ftyp(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_FTYP_T *pBoxFtyp = (BOX_FTYP_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxFtyp->major, 8);
  pos += 8;

  if(pBoxFtyp->lencompatbrands > sz_arena - pos) {
    return -1;
  } else if(pBoxFtyp->lencompatbrands > 0) {
    memcpy(&arena[pos], pBoxFtyp->pcompatbrands, pBoxFtyp->lencompatbrands);
    pos += pBoxFtyp->lencompatbrands;
  }

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("ftyp box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_ftyp(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_FTYP_T *pBoxFtyp = (BOX_FTYP_T *) pBox;
  unsigned int idx;

  dump_box(pBox, context, fp);
  if(verbose) {
    PRINT_INDENT(fp, context);
    fprintf(fp, "   { ver:%c%c%c%c (0x%02x), (0x%02x)", 
           ((unsigned char *)&pBoxFtyp->major)[0],((unsigned char *)&pBoxFtyp->major)[1],
           ((unsigned char *)&pBoxFtyp->major)[2],((unsigned char *)&pBoxFtyp->major)[3],
           htonl(pBoxFtyp->major), htonl(pBoxFtyp->minor));
    if(pBoxFtyp->pcompatbrands) {
      for(idx = 0; idx < pBoxFtyp->lencompatbrands; idx += 4) {
        if( *((uint32_t *) &((unsigned char *)pBoxFtyp->pcompatbrands)[idx]) != 0) {
          fprintf(fp, " %c%c%c%c",
             ((unsigned char *)pBoxFtyp->pcompatbrands)[idx + 0],
             ((unsigned char *)pBoxFtyp->pcompatbrands)[idx + 1],
             ((unsigned char *)pBoxFtyp->pcompatbrands)[idx + 2],
             ((unsigned char *)pBoxFtyp->pcompatbrands)[idx + 3]);
        }
      }
    }

    fprintf(fp, " }\n");

  }
}

static void free_box_ftyp(BOX_T *pBox) {
  BOX_FTYP_T *pBoxFtyp = (BOX_FTYP_T *) pBox;

  if(pBoxFtyp->pcompatbrands) {
    free(pBoxFtyp->pcompatbrands);
  }
}

static BOX_T *read_box_iods(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {

  int rc;
  unsigned int lenBody;
  unsigned int lenCopy;
  unsigned int pos = 0;
  BOX_IODS_T *pBoxIods = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 13 || len > sz_arena) {
    LOG(X_ERROR("Invalid iods body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, len)) < 0) {  
    return NULL;
  }

  pBoxIods = (BOX_IODS_T *) avc_calloc(1, sizeof(BOX_IODS_T));
  memcpy(&pBoxIods->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBoxIods->fullbox, arena, pos += 4);

  pBoxIods->type = arena[pos++];
  if(arena[pos] == 0x80 || arena[pos] == 0xfe) {
    memcpy(pBoxIods->extdescrtype, &arena[pos], 3);
    pos += 3;
  }
  pBoxIods->len = arena[pos++];
  lenBody = pBoxIods->len;
  if(lenBody != len - pos) {
    LOG(X_ERROR("Invalid iods internal length parameter: %d"), lenBody);
    *prc = -1;
    return (BOX_T *) pBoxIods;
  }

  if((lenCopy = lenBody) > sizeof(BOX_IODS_T) - ((char*)&pBoxIods->id - (char*)pBoxIods)) {
    lenCopy = sizeof(BOX_IODS_T) - ((char*)&pBoxIods->id - (char*)pBoxIods);
  }
  memcpy(&pBoxIods->id[0], &arena[pos], lenCopy);
  pos += lenBody;

  return (BOX_T *) pBoxIods;
}

static int write_box_iods(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_IODS_T *pBoxIods = (BOX_IODS_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;
  unsigned int lenBody;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxIods->fullbox.version, 4);
  pos += 4;

  arena[pos++] = pBoxIods->type;
  if(pBoxIods->extdescrtype[0] == 0x80 || pBoxIods->extdescrtype[0] == 0xfe) {
    memcpy(&arena[pos], pBoxIods->extdescrtype, 3);
    pos += 3;
  }
  lenBody = pBoxIods->len + 1;
  memcpy(&arena[pos], &pBoxIods->len, lenBody);
  pos += lenBody;

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("iods box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}


static const char *iods_audprofilestr(uint8_t apl) {
  switch(apl) {
    case 0x00: return "Reserved";
    case 0x01: return "Main Audio Profile - Level 1";
    case 0x02: return "Main Audio Profile - Level 2";
    case 0x03: return "Main Audio Profile - Level 3";
    case 0x04: return "Main Audio Profile - Level 4";
    case 0x05: return "Scalable Audio Profile - Level 1";
    case 0x06: return "Scalable Audio Profile - Level 2";
    case 0x07: return "Scalable Audio Profile - Level 3";
    case 0x08: return "Scalable Audio Profile - Level 4";
    case 0x09: return "Speech Audio Profile - Level 1";
    case 0x0A: return "Speech Audio Profile - Level 2";
    case 0x0B: return "Synthetic Audio Profile - Level 1";
    case 0x0C: return "Synthetic Audio Profile - Level 2";
    case 0x0D: return "Synthetic Audio Profile - Level 3";
    case 0x0E: return "High Quality Audio Profile - Level 1";
    case 0x0F: return "High Quality Audio Profile - Level 2";
    case 0x10: return "High Quality Audio Profile - Level 3";
    case 0x11: return "High Quality Audio Profile - Level 4";
    case 0x12: return "High Quality Audio Profile - Level 5";
    case 0x13: return "High Quality Audio Profile - Level 6";
    case 0x14: return "High Quality Audio Profile - Level 7";
    case 0x15: return "High Quality Audio Profile - Level 8";
    case 0x16: return "Low Delay Audio Profile - Level 1";
    case 0x17: return "Low Delay Audio Profile - Level 2";
    case 0x18: return "Low Delay Audio Profile - Level 3";
    case 0x19: return "Low Delay Audio Profile - Level 4";
    case 0x1A: return "Low Delay Audio Profile - Level 5";
    case 0x1B: return "Low Delay Audio Profile - Level 6";
    case 0x1C: return "Low Delay Audio Profile - Level 7";
    case 0x1D: return "Low Delay Audio Profile - Level 8";
    case 0x1E: return "Natural Audio Profile - Level 1";
    case 0x1F: return "Natural Audio Profile - Level 2";
    case 0x20: return "Natural Audio Profile - Level 3";
    case 0x21: return "Natural Audio Profile - Level 4";
    case 0x22: return "Mobile Audio Internetworking Profile - Level 1";
    case 0x23: return "Mobile Audio Internetworking Profile - Level 2";
    case 0x24: return "Mobile Audio Internetworking Profile - Level 3";
    case 0x25: return "Mobile Audio Internetworking Profile - Level 4";
    case 0x26: return "Mobile Audio Internetworking Profile - Level 5";
    case 0x27: return "Mobile Audio Internetworking Profile - Level 6";
    case 0x28: return "AAC Profile - Level 1";
    case 0x29: return "AAC Profile - Level 2";
    case 0x2A: return "AAC Profile - Level 4";
    case 0x2B: return "AAC Profile - Level 5";
    case 0x2C: return "High Efficiency AAC Profile - Level 2";
    case 0x2D: return "High Efficiency AAC Profile - Level 3";
    case 0x2E: return "High Efficiency AAC Profile - Level 4";
    case 0x2F: return "High Efficiency AAC Profile - Level 5";
    case 0xFE: return "Unknown";
    case 0xFF: return "No audio capability";
    default: return "Private";
  }
}

static const char *iods_vidprofilestr(uint8_t vpl) {
  switch (vpl) {
    case 0x00: return "Reserved";
    case 0x01: return "Simple Profile - Level 1";
    case 0x02: return "Simple Profile - Level 2";
    case 0x03: return "Simple Profile - Level 3";
    case 0x08: return "Simple Profile - Level 0";
    case 0x10: return "Simple Scalable Profile - Level 0";
    case 0x11: return "Simple Scalable Profile - Level 1";
    case 0x12: return "Simple Scalable Profile - Level 2";
    case 0x15: return "AVC/H264 Profile";
    case 0x21: return "Core Profile - Level 1";
    case 0x22: return "Core Profile - Level 2";
    case 0x32: return "Main Profile - Level 2";
    case 0x33: return "Main Profile - Level 3";
    case 0x34: return "Main Profile - Level 4";
    case 0x42: return "N-bit Profile - Level 2";
    case 0x51: return "Scalable Texture Profile - Level 1";
    case 0x61: return "Simple Face Animation Profile - Level 1";
    case 0x62: return "Simple Face Animation Profile - Level 2";
    case 0x63: return "Simple FBA Profile - Level 1";
    case 0x64: return "Simple FBA Profile - Level 2";
    case 0x71: return "Basic Animated Texture Profile - Level 1";
    case 0x72: return "Basic Animated Texture Profile - Level 2";
    case 0x81: return "Hybrid Profile - Level 1";
    case 0x82: return "Hybrid Profile - Level 2";
    case 0x91: return "Advanced Real Time Simple Profile - Level 1";
    case 0x92: return "Advanced Real Time Simple Profile - Level 2";
    case 0x93: return "Advanced Real Time Simple Profile - Level 3";
    case 0x94: return "Advanced Real Time Simple Profile - Level 4";
    case 0xA1: return "Core Scalable Profile - Level1";
    case 0xA2: return "Core Scalable Profile - Level2";
    case 0xA3: return "Core Scalable Profile - Level3";
    case 0xB1: return "Advanced Coding Efficiency Profile - Level 1";
    case 0xB2: return "Advanced Coding Efficiency Profile - Level 2";
    case 0xB3: return "Advanced Coding Efficiency Profile - Level 3";
    case 0xB4: return "Advanced Coding Efficiency Profile - Level 4";
    case 0xC1: return "Advanced Core Profile - Level 1";
    case 0xC2: return "Advanced Core Profile - Level 2";
    case 0xD1: return "Advanced Scalable Texture - Level1";
    case 0xD2: return "Advanced Scalable Texture - Level2";
    case 0xE1: return "Simple Studio Profile - Level 1";
    case 0xE2: return "Simple Studio Profile - Level 2";
    case 0xE3: return "Simple Studio Profile - Level 3";
    case 0xE4: return "Simple Studio Profile - Level 4";
    case 0xE5: return "Core Studio Profile - Level 1";
    case 0xE6: return "Core Studio Profile - Level 2";
    case 0xE7: return "Core Studio Profile - Level 3";
    case 0xE8: return "Core Studio Profile - Level 4";
    case 0xF0: return "Advanced Simple Profile - Level 0";
    case 0xF1: return "Advanced Simple Profile - Level 1";
    case 0xF2: return "Advanced Simple Profile - Level 2";
    case 0xF3: return "Advanced Simple Profile - Level 3";
    case 0xF4: return "Advanced Simple Profile - Level 4";
    case 0xF5: return "Advanced Simple Profile - Level 5";
    case 0xF7: return "Advanced Simple Profile - Level 3b";
    case 0xF8: return "Fine Granularity Scalable Profile - Level 0";
    case 0xF9: return "Fine Granularity Scalable Profile - Level 1";
    case 0xFA: return "Fine Granularity Scalable Profile - Level 2";
    case 0xFB: return "Fine Granularity Scalable Profile - Level 3";
    case 0xFC: return "Fine Granularity Scalable Profile - Level 4";
    case 0xFD: return "Fine Granularity Scalable Profile - Level 5";
    case 0xFE: return "Unknown";
    case 0xFF: return "No videocapability";
    default: return "Reserved";
  }
}


static void dump_box_iods(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_IODS_T *pBoxIods = (BOX_IODS_T *) pBox;
  int ctxt = context;

  dump_box(pBox, context, fp);
  if(verbose) {
 
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { type:0x%x, etype:0x%x%x%x, len:%u, id:0x%x%x, profile levels:",
        pBoxIods->type, pBoxIods->extdescrtype[0], pBoxIods->extdescrtype[1], pBoxIods->extdescrtype[2], 
        pBoxIods->len, pBoxIods->id[0], pBoxIods->id[1]);
    if(pBoxIods->od_profilelevel != 0xff) {
      fprintf(fp, "\n");
      ctxt = context;
      PRINT_INDENT(fp, ctxt);
      fprintf(fp, "     od:%u", pBoxIods->od_profilelevel);
    }
    if(pBoxIods->scene_profilelevel != 0xff) {
      fprintf(fp, "\n");
      ctxt = context;
      PRINT_INDENT(fp, ctxt);
      fprintf(fp, "     scene:0x%x", pBoxIods->scene_profilelevel);
    }
    if(pBoxIods->audio_profilelevel != 0xff) {
      fprintf(fp, "\n");
      ctxt = context;
      PRINT_INDENT(fp, ctxt);
      fprintf(fp, "     audio:%s (0x%x)", iods_audprofilestr(pBoxIods->audio_profilelevel),
               pBoxIods->audio_profilelevel);
    }
    if(pBoxIods->video_profilelevel != 0xff) {
      fprintf(fp, "\n");
      ctxt = context;
      PRINT_INDENT(fp, ctxt);
      fprintf(fp, "     video:%s (0x%x)", iods_vidprofilestr(pBoxIods->video_profilelevel),
               pBoxIods->video_profilelevel);
    }
    if(pBoxIods->graphics_profilelevel != 0xff) {
      fprintf(fp, "\n");
      ctxt = context;
      PRINT_INDENT(fp, ctxt);
      fprintf(fp, "     graphics:0x%x", pBoxIods->graphics_profilelevel);
    }
    fprintf(fp, " }\n");

  }
}

static BOX_T *read_box_mvhd(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {

  int rc;
  unsigned int pos = 0;
  BOX_MVHD_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;
  const unsigned int MVHD_LEN = 100;

  if(len < MVHD_LEN) {
    LOG(X_ERROR("Invalid mvhd body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, MVHD_LEN)) < 0) {  
    return NULL;
  }

  pBox = (BOX_MVHD_T *) avc_calloc(1, sizeof(BOX_MVHD_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);

  if(pBox->fullbox.version == 1) {

    if(len != MVHD_LEN + 12) {
      LOG(X_ERROR("Invalid mvhd body length: %u for version 1"), len);
      *prc = -1;
      return (BOX_T *) pBox;
    } else if((rc = pStream->cbRead(pStream, &arena[MVHD_LEN], 12)) < 0) {  
      return NULL;
    }

    pBox->creattime = READ_UINT64(arena, pos);
    pBox->modificationtime = READ_UINT64(arena, pos);
    pBox->timescale = READ_UINT32(arena, pos);
    pBox->duration = READ_UINT64(arena, pos);
    
  } else {

    if(len != MVHD_LEN) {
      LOG(X_ERROR("Invalid mvhd body length: %u for version 0"), len);
      *prc = -1;
      return (BOX_T *) pBox;
    }

    pBox->creattime = READ_UINT32(arena, pos);
    pBox->modificationtime = READ_UINT32(arena, pos);
    pBox->timescale = READ_UINT32(arena, pos);
    pBox->duration = READ_UINT32(arena, pos);
  }

  memcpy(&pBox->rate, &arena[pos], MVHD_LEN - 20);
  pos += (MVHD_LEN - 20);

  return (BOX_T *) pBox;
}

static int write_box_mvhd(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_MVHD_T *pBoxMvhd = (BOX_MVHD_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxMvhd->fullbox.version, 4);
  pos += 4;
  if(pBoxMvhd->fullbox.version == 1) {
    LOG(X_CRITICAL("full box version 1 not supported for mdhd"));
    return -1;
  } else {
    WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxMvhd->creattime));
    WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxMvhd->modificationtime));
    WRITE_UINT32(arena, pos, htonl(pBoxMvhd->timescale));
    WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxMvhd->duration));
  }

  memcpy(&arena[pos], &pBoxMvhd->rate, 80);
  pos += 80;

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("mvhd box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_mvhd(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_MVHD_T *pBoxMvhd = (BOX_MVHD_T *) pBox;
  long durationSec;
  int ctxt = context;

  dump_box(pBox, context, fp);
  if(verbose) {
    durationSec = (long) pBoxMvhd->duration / pBoxMvhd->timescale;

    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { %uHz, %s (0x%x), rate:%s, vol:%s, next:0x%x }\n", 
      pBoxMvhd->timescale, avc_getPrintableDuration(pBoxMvhd->duration, pBoxMvhd->timescale),
      (uint32_t) pBoxMvhd->duration, getPrintableFixedPt16x16(htonl(pBoxMvhd->rate)), 
      getPrintableFixedPt8x8(htons(pBoxMvhd->volume)), htonl(pBoxMvhd->nexttrack));
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { create time: %s", getPrintableTm(pBoxMvhd->creattime));
    fprintf(fp, " mod time: %s }\n", getPrintableTm(pBoxMvhd->modificationtime));

  }
}

static BOX_T *read_box_tkhd(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {

  int rc;
  unsigned int pos = 0;
  BOX_TKHD_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;
  const unsigned int TKHD_LEN = 84;

  if(len < TKHD_LEN) {
    LOG(X_ERROR("Invalid mvhd body length: %d"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, TKHD_LEN)) < 0) {  
    return NULL;
  }

  pBox = (BOX_TKHD_T *) avc_calloc(1, sizeof(BOX_TKHD_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);

  if(pBox->fullbox.version == 1) {

    if(len != TKHD_LEN + 12) {
      LOG(X_ERROR("Invalid tkhd body length: %u for version 1"), len);
      *prc = -1;
      return (BOX_T *) pBox;
    } else if((rc = pStream->cbRead(pStream, &arena[TKHD_LEN], 12)) < 0) {  
      return NULL;
    }

    pBox->creattime = READ_UINT64(arena, pos);
    pBox->modificationtime = READ_UINT64(arena, pos);
    pBox->trackid = READ_UINT32(arena, pos);
    pBox->reserved = READ_UINT32(arena, pos);
    pBox->duration = READ_UINT64(arena, pos);

  } else {

    if(len != TKHD_LEN) {
      LOG(X_ERROR("Invalid tkhd body length: %u for version 0"), len);
      *prc = -1;
      return (BOX_T *) pBox;
    }

    pBox->creattime = READ_UINT32(arena, pos);
    pBox->modificationtime = READ_UINT32(arena, pos);
    pBox->trackid = READ_UINT32(arena, pos);
    pBox->reserved = READ_UINT32(arena, pos);
    pBox->duration = READ_UINT32(arena, pos);

  }

  memcpy(&pBox->reserved2, &arena[pos], TKHD_LEN - 24);
  pos += (TKHD_LEN - 24);

  return (BOX_T *) pBox;
}

static int write_box_tkhd(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_TKHD_T *pBoxTkhd = (BOX_TKHD_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxTkhd->fullbox, 4);
  pos += 4;
  if(pBoxTkhd->fullbox.version == 1) {
    LOG(X_CRITICAL("full box version 1 not supported for mdhd"));
    return -1;
  } else {
    WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxTkhd->creattime));
    WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxTkhd->modificationtime));
    WRITE_UINT32(arena, pos, htonl(pBoxTkhd->trackid));
    WRITE_UINT32(arena, pos, htonl(pBoxTkhd->reserved));
    WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxTkhd->duration));
  }

  memcpy(&arena[pos], &pBoxTkhd->reserved2, 60);
  pos += 60;

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("tkhd box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_tkhd(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_TKHD_T *pBoxTkhd = (BOX_TKHD_T *) pBox;
  BOX_T *pBoxTmp;
  int ctxt = context;
  uint32_t mdhd_timescale = 0;

  //
  // Try to find the track timescale 
  //
  if((pBoxTmp = mp4_findBoxNext(pBox, *((uint32_t *) "mdia"))) &&
     (pBoxTmp = mp4_findBoxChild(pBoxTmp, *((uint32_t *) "mdhd")))) {
    mdhd_timescale = ((BOX_MDHD_T *) pBoxTmp)->timescale;
  }

  dump_box(pBox, context, fp);

  if(verbose) {
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { id:%u, duration: %s (0x%xHz), layer:0x%x, alt:0x%x, vol:%s",
      pBoxTkhd->trackid,
      mdhd_timescale > 0 ? avc_getPrintableDuration(pBoxTkhd->duration, mdhd_timescale) : "",
      (uint32_t) pBoxTkhd->duration, htons(pBoxTkhd->layer),
      htons(pBoxTkhd->altgroup), getPrintableFixedPt8x8(htons(pBoxTkhd->volume)));

      if(pBoxTkhd->width != 0 || pBoxTkhd->height != 0) {
          fprintf(fp, ", %ux%u", (htonl(pBoxTkhd->width) >> 16), (htonl(pBoxTkhd->height) >> 16));
      }
                 
    fprintf(fp, " }\n");

    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { create time: %s", getPrintableTm(pBoxTkhd->creattime));
    fprintf(fp, " mod time: %s }\n", getPrintableTm(pBoxTkhd->modificationtime));

  }
}

static BOX_T *read_box_mdhd(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {

  int rc;
  unsigned int pos = 0;
  BOX_MDHD_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;
  const unsigned int MDHD_LEN = 24;

  if(len < MDHD_LEN) {
    LOG(X_ERROR("Invalid mdhd body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, MDHD_LEN)) < 0) {  
    return NULL;
  }

  pBox = (BOX_MDHD_T *) avc_calloc(1, sizeof(BOX_MDHD_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);

  if(pBox->fullbox.version == 1) {

    if(len != MDHD_LEN + 12) {
      LOG(X_ERROR("Invalid mdhd body length: %u for version 1"), len);
      *prc = -1;
      return (BOX_T *) pBox;
    } else if((rc = pStream->cbRead(pStream, &arena[MDHD_LEN], 12)) < 0) {  
      return NULL;
    }

    pBox->creattime = READ_UINT64(arena, pos);
    pBox->modificationtime = READ_UINT64(arena, pos);
    pBox->timescale = READ_UINT32(arena, pos);
    pBox->duration = READ_UINT64(arena, pos);

  } else {

    pBox->creattime = READ_UINT32(arena, pos);
    pBox->modificationtime = READ_UINT32(arena, pos);
    pBox->timescale = READ_UINT32(arena, pos);
    pBox->duration = READ_UINT32(arena, pos);

  }

  memcpy(&pBox->language, &arena[pos], 4);
  pos += 4;

  return (BOX_T *) pBox;
}

static int write_box_mdhd(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_MDHD_T *pBoxMdhd = (BOX_MDHD_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxMdhd->fullbox.version, 4);
  pos += 4;
  if(pBoxMdhd->fullbox.version == 1) {
    LOG(X_CRITICAL("full box version 1 not supported for mdhd"));
    return -1;
  } else {
    WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxMdhd->creattime));
    WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxMdhd->modificationtime));
    WRITE_UINT32(arena, pos, htonl(pBoxMdhd->timescale));
    WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxMdhd->duration));
  }

  memcpy(&arena[pos], &pBoxMdhd->language, 4);
  pos += 4;

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("mdhd box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_mdhd(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_MDHD_T *pBoxMdhd = (BOX_MDHD_T *) pBox;
  int ctxt = context;

  dump_box(pBox, context, fp);
  if(verbose) {
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { %uHz, %s (0x%x), lang:0x%x }\n", 
               pBoxMdhd->timescale, 
               avc_getPrintableDuration(pBoxMdhd->duration, pBoxMdhd->timescale),
               (unsigned int) pBoxMdhd->duration, htons(pBoxMdhd->language));
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { create time: %s", getPrintableTm(pBoxMdhd->creattime));
    fprintf(fp, " mod time: %s }\n", getPrintableTm(pBoxMdhd->modificationtime));
  }
}

static BOX_T *read_box_hdlr(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {

  int rc;
  unsigned int pos = 0;
  BOX_HDLR_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;
  uint32_t lenName;

  if(len < 25 || len > sz_arena) {
    LOG(X_ERROR("Invalid hdlr body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, len)) < 0) {  
    return NULL;
  }

  pBox = (BOX_HDLR_T *) avc_calloc(1, sizeof(BOX_HDLR_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);

  memcpy(&pBox->predefined, &arena[pos], 20);
  pos += 20;

  lenName = (((len - pos) > sizeof(pBox->name) -1 ? sizeof(pBox->name) -1 : (len - pos)));
  memcpy(pBox->name, &arena[pos], lenName);

  return (BOX_T *) pBox;
}

static int write_box_hdlr(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_HDLR_T *pBoxHdlr = (BOX_HDLR_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;
  int lenName;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxHdlr->fullbox.version, 4);
  pos += 4;

  memcpy(&arena[pos], &pBoxHdlr->predefined, 20);
  pos += 20;

  if((lenName = strlen(pBoxHdlr->name) + 1) > 0) {
    memcpy(&arena[pos], pBoxHdlr->name, lenName);
    pos += lenName;
  }

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("hdlr box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_hdlr(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_HDLR_T *pBoxHdlr = (BOX_HDLR_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {
    PRINT_INDENT(fp, context);
    fprintf(fp, "   { type:%c%c%c%c (0x%x), name:'%s'}\n",
                  ((unsigned char *)&pBoxHdlr->handlertype)[0],((unsigned char *)&pBoxHdlr->handlertype)[1],
                  ((unsigned char *)&pBoxHdlr->handlertype)[2],((unsigned char *)&pBoxHdlr->handlertype)[3],
                  htonl(pBoxHdlr->handlertype), pBoxHdlr->name);

  }
}


static BOX_T *read_box_smhd(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {

  int rc;
  unsigned int pos = 0;
  BOX_SMHD_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len != 8) {
    LOG(X_ERROR("Invalid smhd body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, len)) < 0) {  
    return NULL;
  }

  pBox = (BOX_SMHD_T *) avc_calloc(1, sizeof(BOX_SMHD_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);

  memcpy(&pBox->balance, &arena[pos], 4);
  pos += 4;

  return (BOX_T *) pBox;
}

static int write_box_smhd(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_SMHD_T *pBoxSmhd = (BOX_SMHD_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxSmhd->fullbox.version, 4);
  pos += 4;

  memcpy(&arena[pos], &pBoxSmhd->balance, 4);
  pos += 4;

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("smhd box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_smhd(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_SMHD_T *pBoxSmhd = (BOX_SMHD_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {
    PRINT_INDENT(fp, context);
    fprintf(fp, "   { balance: 0x%x }\n", htons(pBoxSmhd->balance));

  }
}


static BOX_T *read_box_vmhd(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {

  int rc;
  unsigned int pos = 0;
  BOX_VMHD_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len != 12) {
    LOG(X_ERROR("Invalid vmhd body length: %u"), (uint32_t) len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, len)) < 0) {  
    return NULL;
  }

  pBox = (BOX_VMHD_T *) avc_calloc(1, sizeof(BOX_VMHD_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);

  memcpy(&pBox->graphicsmode, &arena[pos], 8);
  pos += 8;

  return (BOX_T *) pBox;
}

static int write_box_vmhd(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_VMHD_T *pBoxVmhd = (BOX_VMHD_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxVmhd->fullbox.version, 4);
  pos += 4;

  memcpy(&arena[pos], &pBoxVmhd->graphicsmode, 8);
  pos += 8;

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("vmhd box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_vmhd(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_VMHD_T *pBoxVmhd = (BOX_VMHD_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {
    PRINT_INDENT(fp, context);
    fprintf(fp, "   { mode: 0x%x opcolor: 0x%x 0x%x 0x%x}\n", htons(pBoxVmhd->graphicsmode),
      htons(pBoxVmhd->opcolor[0]), htons(pBoxVmhd->opcolor[1]), htons(pBoxVmhd->opcolor[2]));

  }
}


static BOX_T *read_box_dref_url(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                                unsigned char *arena, unsigned int sz_arena,
                                int *prc) {

  int rc;
  int lenname;
  unsigned int pos = 0;
  BOX_DREF_ENTRY_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 4 || len > sz_arena) {
    LOG(X_ERROR("Invalid vmhd body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, len)) < 0) {  
    return NULL;
  }

  pBox = (BOX_DREF_ENTRY_T *) avc_calloc(1, sizeof(BOX_DREF_ENTRY_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);

  lenname = (((len - pos) > sizeof(pBox->name) -1 ? sizeof(pBox->name) -1 : (len - pos)));
  memcpy(pBox->name, &arena[pos], lenname);

  return (BOX_T *) pBox;
}

static int write_box_dref_url(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                              unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_DREF_ENTRY_T *pBoxDrefUrl = (BOX_DREF_ENTRY_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;
  int lenName;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxDrefUrl->fullbox.version, 4);
  pos += 4;

  if(pBoxDrefUrl->box.szdata_ownbox > pos && (lenName = strlen(pBoxDrefUrl->name)) > 0) {
    lenName++;
    memcpy(&arena[pos], pBoxDrefUrl->name, lenName);
    pos += lenName;
  }

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("dref url box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_dref_url(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_DREF_ENTRY_T *pBoxDrefEntry = (BOX_DREF_ENTRY_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {
    PRINT_INDENT(fp, context);
    fprintf(fp, "   { name:'%s' }\n", pBoxDrefEntry->name);
  }
}

static BOX_T *read_box_dref(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {

  int rc;
  unsigned int pos = 0;
  BOX_DREF_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 8 || len > sz_arena) {
    LOG(X_ERROR("Invalid dref body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 8)) < 0) {  
    return NULL;
  }

  pBox = (BOX_DREF_T *) avc_calloc(1, sizeof(BOX_DREF_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);

  pBox->entrycnt = READ_UINT32(arena, pos);
  if(pBox->entrycnt > 0) {
    pBox->box.subs = 1;
    pBox->box.szdata_ownbox = pos;
  }

  return (BOX_T *) pBox;
}

static int write_box_dref(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_DREF_T *pBoxDref = (BOX_DREF_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxDref->fullbox.version, 4);
  pos += 4;

  WRITE_UINT32(arena, pos, htonl(pBoxDref->entrycnt));

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("dref box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static BOX_T *read_box_stsd(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  unsigned int pos = 0;
  BOX_STSD_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 8) {
    LOG(X_ERROR("Invalid stsd body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 8)) < 0) {  
    return NULL;
  }

  pBox = (BOX_STSD_T *) avc_calloc(1, sizeof(BOX_STSD_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);
  pBox->entrycnt = READ_UINT32(arena, pos);

  if(pBox->entrycnt > 0) {
    pBox->box.subs = 1;
    pBox->box.szdata_ownbox = pos;
  }

  return (BOX_T *) pBox;
}

static int write_box_stsd(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_STSD_T *pBoxDref = (BOX_STSD_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxDref->fullbox.version, 4);
  pos += 4;

  WRITE_UINT32(arena, pos, htonl(pBoxDref->entrycnt));

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("stsd box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_stsd(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_STSD_T *pBoxStsd = (BOX_STSD_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {
    PRINT_INDENT(fp, context);
    fprintf(fp, "   { entrycnt:%d }\n", pBoxStsd->entrycnt);
  }
}

static BOX_T *read_box_mp4v(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  unsigned int pos = 0;
  BOX_MP4V_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 78) {
    LOG(X_ERROR("Invalid mp4v body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 78)) < 0) {  
    return NULL;
  }

  pBox = (BOX_MP4V_T *) avc_calloc(1, sizeof(BOX_MP4V_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));

  memcpy(&pBox->reserved, &arena[pos], 78);
  pos += 78;

  pBox->codername[sizeof(pBox->codername) - 1] = '\0';
  pBox->box.subs = 1;
  pBox->box.szdata_ownbox = pos;

  return (BOX_T *) pBox;
}

static int write_box_mp4v(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_MP4V_T *pBoxMp4v = (BOX_MP4V_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxMp4v->reserved, 78);
  pos += 78;
  
  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("mp4v box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_mp4v(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_MP4V_T *pMp4v = (BOX_MP4V_T *) pBox;
  int ctxt;

  dump_box(pBox, context, fp);
  if(verbose) {
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { dataref:0x%x, streamver:0x%x, streamrev:0x%x, %u x %u,\n",
       htons(pMp4v->datarefidx), htons(pMp4v->codecstreamver), 
       htons(pMp4v->codecstreamrev), htons(pMp4v->wid),htons(pMp4v->ht));
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "     dpi: 0x%x 0x%x, framesz:0x%x", 
         htonl(pMp4v->horizresolution), htonl(pMp4v->vertresolution),
         htons(pMp4v->framesz));
    if(pMp4v->codername[0] != '\0') {
      fprintf(fp, ",\n");
      ctxt = context;
      PRINT_INDENT(fp, ctxt);
      fprintf(fp, "     coder: '%s'", pMp4v->codername);
    }
    fprintf(fp, " }\n");
  }
}


static BOX_T *read_box_mp4a(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  int i;
  unsigned int pos = 0;
  BOX_MP4A_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 28) {
    LOG(X_ERROR("Invalid mp4a body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 28)) < 0) {  
    return NULL;
  }

  pBox = (BOX_MP4A_T *) avc_calloc(1, sizeof(BOX_MP4A_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));

  memcpy(&pBox->reserved, &arena[pos], 28);
  pos += 28;

  //
  // QT specific
  // http://mp4v2.googlecode.com/svn-history/r388/trunk/src/atom_sound.cpp 
  //
  if(pBox->soundver > 0) {
    if(pos + 16 > len) {
      LOG(X_ERROR("Invalid mp4a body length: %u, soundver:%d"), len, pBox->soundver);
      return NULL;
    }
    if((rc = pStream->cbRead(pStream, arena, 16)) < 0) {  
      return NULL;
    }
    i = READ_UINT32(arena, pos); // samples per packet
    i = READ_UINT32(arena, pos); // bytes per packet
    i = READ_UINT32(arena, pos); // bytes per frame
    i = READ_UINT32(arena, pos); // bytes per sample
  }

  if(pBox->soundver == 2) {
    if(pos + 20 > len) {
      LOG(X_ERROR("Invalid mp4a body (extra) length: %u, soundver:%d"), len, pBox->soundver);
      return NULL;
    }
    if((rc = pStream->cbRead(pStream, arena, 20)) < 0) {  
      return NULL;
    }
    pos += 20;
  }

  pBox->box.subs = 1;
  pBox->box.szdata_ownbox = pos;

  return (BOX_T *) pBox;
}

static int write_box_mp4a(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_MP4A_T *pBoxMp4a = (BOX_MP4A_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxMp4a->reserved, 28);
  pos += 28;
  
  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("mp4a box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_mp4a(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_MP4A_T *pMp4a = (BOX_MP4A_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {
    PRINT_INDENT(fp, context);
    fprintf(fp, "   { dataref:0x%x, sndver:%u, channels:%u, samplesize:%u, %uHz (0x%x) }\n", 
              htons(pMp4a->datarefidx), htons(pMp4a->soundver), htons(pMp4a->channelcnt), 
              htons(pMp4a->samplesize),htonl(pMp4a->timescale)>>16, 
              htonl(pMp4a->timescale)>>16);
  }
}

static BOX_T *read_box_chan(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  unsigned int pos = 0;
  BOX_GENERIC_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if((rc = pStream->cbRead(pStream, arena, len)) < 0) {  
    return NULL;
  }
  pBox = (BOX_GENERIC_T *) avc_calloc(1, sizeof(BOX_GENERIC_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  pos += len;

  //fprintf(stderr, "chan len:%d, pos:%d, ownbox:%d\n", len, pos, pBox->box.szdata_ownbox);
  //avc_dumpHex(stderr, arena, len, 1);

  //pBox->box.subs = 1;
  //pBox->box.szdata_ownbox = pos;

  return (BOX_T *) pBox;
}

static BOX_T *read_box_wave(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  //int rc;
  //unsigned int pos = 0;
  BOX_GENERIC_T *pBox = NULL;
  //uint32_t len = (uint32_t) pBoxHdr->szdata;

  //fprintf(stderr, "wave - len:%d, ownbox:%d\n", len, pBoxHdr->szdata_ownbox);

  pBox = (BOX_GENERIC_T *) avc_calloc(1, sizeof(BOX_GENERIC_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  //pos += len;

  pBox->box.subs = 1;
  pBox->box.szdata_ownbox = 0;

  return (BOX_T *) pBox;
}

static BOX_T *read_box_frma(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  //int rc;
  //unsigned int pos = 0;
  BOX_GENERIC_T *pBox = NULL;
  //uint32_t len = (uint32_t) pBoxHdr->szdata;

  pBox = (BOX_GENERIC_T *) avc_calloc(1, sizeof(BOX_GENERIC_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  //pos += len;

  pBox->box.szdata_ownbox = 0;

  return (BOX_T *) pBox;
}

static BOX_T *read_box_esds(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  unsigned int pos = 0;
  BOX_ESDS_T *pBox = NULL;
  BOX_ESDS_DESCR_T type;
  uint32_t tmp;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 8 || len > sz_arena) {
    LOG(X_ERROR("Invalid esds body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, len)) < 0) {  
    return NULL;
  }

  pBox = (BOX_ESDS_T *) avc_calloc(1, sizeof(BOX_ESDS_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);

  while(pos < len - 2) {

    memset(&type, 0, sizeof(type));
    type.descriptor = arena[pos++];
    if(arena[pos] == 0x80 || arena[pos] == 0xfe) {
      memcpy(&type.extdescrtype, &arena[pos], 3);
      pos += 3;
    }
    type.length = arena[pos++];

    if(type.descriptor == 0x03) {

      memcpy(&pBox->estype.type, &type, sizeof(BOX_ESDS_DESCR_T));
      if(pos + 3 > len) {
        LOG(X_WARNING("Invalid esds 0x03 type length: %d"), pBox->estype.type.length);
        break;
      }
      memcpy(&pBox->estype.id, &arena[pos], 3);
      pos += 3;

    } else if(type.descriptor == 0x04) {

      memcpy(&pBox->config.type, &type, sizeof(BOX_ESDS_DESCR_T));
      if(pos + 13 > len) {
        LOG(X_WARNING("Invalid esds 0x04 type length: %d"), pBox->config.type.length);
        break;
      }
      memcpy(&pBox->config.objtypeid, &arena[pos], 5);
      pos += 5;
      memcpy(&pBox->config.maxbitrate, &arena[pos], 8);
      pos += 8;

    } else if(type.descriptor == 0x05) {

      memcpy(&pBox->specific.type, &type, sizeof(BOX_ESDS_DESCR_T));
      if(pos + pBox->specific.type.length > len) {
        LOG(X_WARNING("Invalid esds 0x05 type length: %d"), pBox->specific.type.length);
        break;
      } else if((tmp = pBox->specific.type.length) > sizeof(pBox->specific.startcodes)) {
        tmp = sizeof(pBox->specific.startcodes);  
      } 
      memcpy(&pBox->specific.startcodes, &arena[pos], tmp);
      pos += pBox->specific.type.length;

    } else if(type.descriptor == 0x06) {

      memcpy(&pBox->sl.type, &type, sizeof(BOX_ESDS_DESCR_T));
      if(pos + pBox->sl.type.length > len || pBox->sl.type.length > sizeof(pBox->sl.val)) {
        LOG(X_WARNING("Invalid esds 0x06 type length: %d"), pBox->sl.type.length);
        break;
      }
      memcpy(&pBox->sl.val, &arena[pos], pBox->sl.type.length);
      pos += pBox->sl.type.length;
    } 

  }

  return (BOX_T *) pBox;
}

static int write_box_esds(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_ESDS_T *pBoxEsds = (BOX_ESDS_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxEsds->fullbox.version, 4);
  pos += 4;

  arena[pos++] = pBoxEsds->estype.type.descriptor;
  arena[pos++] = pBoxEsds->estype.type.length;
  WRITE_UINT16(arena, pos, htons(pBoxEsds->estype.id));
  arena[pos++] = pBoxEsds->estype.priority;

  arena[pos++] = pBoxEsds->config.type.descriptor;
  arena[pos++] = pBoxEsds->config.type.length;
  arena[pos++] = pBoxEsds->config.objtypeid;
  arena[pos++] = pBoxEsds->config.streamtype;
  arena[pos++] = pBoxEsds->config.bufsize[0];
  arena[pos++] = pBoxEsds->config.bufsize[1];
  arena[pos++] = pBoxEsds->config.bufsize[2];
  WRITE_UINT32(arena, pos, pBoxEsds->config.maxbitrate);
  WRITE_UINT32(arena, pos, pBoxEsds->config.avgbitrate);

  arena[pos++] = pBoxEsds->specific.type.descriptor;
  arena[pos++] = pBoxEsds->specific.type.length;
  memcpy(&arena[pos], pBoxEsds->specific.startcodes, pBoxEsds->specific.type.length);
  pos += pBoxEsds->specific.type.length;

  arena[pos++] = pBoxEsds->sl.type.descriptor;
  arena[pos++] = pBoxEsds->sl.type.length;
  memcpy(&arena[pos], pBoxEsds->sl.val, pBoxEsds->sl.type.length);
  pos += pBoxEsds->sl.type.length;

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("esds box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_esds(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_ESDS_T *pEsds = (BOX_ESDS_T *) pBox;
  uint32_t ui32 = 0;
  ESDS_DECODER_CFG_T decoderSp;
  int ctxt;

  dump_box(pBox, context, fp);
  if(verbose) {

    //
    // descriptor type 0x03
    //   
    if(pEsds->estype.type.descriptor != 0) {
      ctxt = context;
      PRINT_INDENT(fp, ctxt);
      fprintf(fp, "   { 0x%x, id: 0x%x stream dependence:%u, url flag:%u, OCR:%u, priority:%u }\n", 
        pEsds->estype.type.descriptor, htons(pEsds->estype.id),
        pEsds->estype.priority & 0x80, pEsds->estype.priority & 0x40,
        pEsds->estype.priority & 0x20, pEsds->estype.priority & 0x1f);
    }

    //
    // descriptor type 0x04
    //
    if(pEsds->config.type.descriptor != 0) {
      ctxt = context;
      PRINT_INDENT(fp, ctxt);

      memcpy(&ui32, pEsds->config.bufsize, 3);
      ui32 <<= 8;

      fprintf(fp, "   { 0x%x, object: 0x%x stream: %u, upstream: %u, bufsize: %u, maxrate:%u, avgrate:%u }\n", 
        pEsds->config.type.descriptor, pEsds->config.objtypeid, 
        pEsds->config.streamtype >> 2, pEsds->config.streamtype & 0x02,
        htonl(ui32),  htonl(pEsds->config.maxbitrate), htonl(pEsds->config.avgbitrate));
    }    

    //
    // descriptor type 0x05
    //
    if(pEsds->specific.type.descriptor != 0) {
      ctxt = context;
      PRINT_INDENT(fp, ctxt);
      fprintf(fp, "   { 0x%x, startcodes:", pEsds->specific.type.descriptor);     
      for(ui32 = 0; ui32 < pEsds->specific.type.length && ui32 < sizeof(pEsds->specific.startcodes); ui32++) {
        if(ui32 > 0 && ui32 % 8 == 0) {
          fprintf(fp, "\n");
          ctxt = context;
          PRINT_INDENT(fp, ctxt);
          fprintf(fp, "                     ");
        }
        fprintf(fp, " 0x%2.2x", pEsds->specific.startcodes[ui32]);
      }
      fprintf(fp, " }\n");

      if(pEsds->config.objtypeid == ESDS_OBJTYPE_MP4A) {
        if(esds_decodeDecoderSp(pEsds->specific.startcodes, 
        ((pEsds->specific.type.length > sizeof(pEsds->specific.startcodes)) ?
        sizeof(pEsds->specific.startcodes) : pEsds->specific.type.length), &decoderSp) == 0)   {

          ctxt = context;
          PRINT_INDENT(fp, ctxt);

          fprintf(fp, "   {      type: %s (0x%x), freq: %uHz (0x%x), channel: %s (0x%x) %u samples/frame }\n", 
                  esds_getMp4aObjTypeStr(decoderSp.objTypeIdx), decoderSp.objTypeIdx,
                  esds_getFrequencyVal(ESDS_FREQ_IDX(decoderSp)), ESDS_FREQ_IDX(decoderSp),
                  esds_getChannelStr(decoderSp.channelIdx), decoderSp.channelIdx,
                  decoderSp.frameDeltaHz);
        } 
      } else if(pEsds->config.objtypeid == ESDS_OBJTYPE_MP4V) {
        //ctxt = context;
        //PRINT_INDENT(fp, ctxt);
        //fprintf(fp, "   {      x }\n");
      }

    }

    //
    // descriptor type 0x06
    //
    if(pEsds->sl.type.descriptor != 0) {
      ctxt = context;
      PRINT_INDENT(fp, ctxt);
      fprintf(fp, "   { 0x%x, sl:", pEsds->sl.type.descriptor);     
      for(ui32 = 0; ui32 < pEsds->sl.type.length; ui32++) {
        fprintf(fp, " 0x%x", pEsds->sl.val[ui32]);
      }
      fprintf(fp, " }\n");
    }


  }
}


static BOX_T *read_box_avc1(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  unsigned int pos = 0;
  BOX_AVC1_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 78) {
    LOG(X_ERROR("Invalid avc1 body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 78)) < 0) {  
    return NULL;
  }

  pBox = (BOX_AVC1_T *) avc_calloc(1, sizeof(BOX_AVC1_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));

  memcpy(&pBox->reserved, &arena[pos], 78);
  pos += 78;

  pBox->box.subs = 1;
  pBox->box.szdata_ownbox = pos;

  return (BOX_T *) pBox;
}

static int write_box_avc1(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_AVC1_T *pBoxAvc1 = (BOX_AVC1_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxAvc1->reserved, 78);
  pos += 78;

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("avc1 box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_avc1(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_AVC1_T *pBoxAvc1 = (BOX_AVC1_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {
    PRINT_INDENT(fp, context);
    fprintf(fp, "   { %dx%d, dpi: 0x%x x 0x%x, framecnt: %d, '%s', depth: 0x%x }\n", 
      htons(pBoxAvc1->width), htons(pBoxAvc1->height),
      htonl(pBoxAvc1->horizresolution), htonl(pBoxAvc1->vertresolution),
      htons(pBoxAvc1->framecnt), pBoxAvc1->compressorname, htons(pBoxAvc1->depth));
  }
}


static BOX_T *read_box_avcC(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  BOX_AVCC_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 8 || len > sz_arena) {
    LOG(X_ERROR("Invalid avcC body length: %u"), len);
    *prc = -1;
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, len)) < 0) {  
    *prc = -1;
    return NULL;
  }

  pBox = (BOX_AVCC_T *) avc_calloc(1, sizeof(BOX_AVCC_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));

  if(avcc_initCfg(arena, len, &pBox->avcc) < 0) {
    free(pBox);
    *prc = -1;
    return NULL;
  }
  
  return (BOX_T *) pBox;
}

static int write_box_avcC(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_AVCC_T *pBoxAvcC = (BOX_AVCC_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;
  unsigned int idx;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  arena[pos++] = pBoxAvcC->avcc.configversion;
  arena[pos++] = pBoxAvcC->avcc.avcprofile;
  arena[pos++] = pBoxAvcC->avcc.profilecompatibility;
  arena[pos++] = pBoxAvcC->avcc.avclevel;
  arena[pos++] = pBoxAvcC->avcc.lenminusone | 0xfc;
  arena[pos++] = pBoxAvcC->avcc.numsps | 0xe0;
  if(pBoxAvcC->avcc.numsps > 0) {
    for(idx = 0; idx < pBoxAvcC->avcc.numsps; idx++) {
      WRITE_UINT16(arena, pos, htons(pBoxAvcC->avcc.psps[idx].len));
      memcpy(&arena[pos], pBoxAvcC->avcc.psps[idx].data, pBoxAvcC->avcc.psps[idx].len);
      pos += pBoxAvcC->avcc.psps[idx].len;
    }
  }

  arena[pos++] = pBoxAvcC->avcc.numpps;
  if(pBoxAvcC->avcc.numsps > 0) {
    for(idx = 0; idx < pBoxAvcC->avcc.numpps; idx++) {
      WRITE_UINT16(arena, pos, htons(pBoxAvcC->avcc.ppps[idx].len));
      memcpy(&arena[pos], pBoxAvcC->avcc.ppps[idx].data, pBoxAvcC->avcc.ppps[idx].len);
      pos += pBoxAvcC->avcc.ppps[idx].len;
    }
  }

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("avcC box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_avcC(BOX_T *pBox, int context, int verbose, FILE *fp) {
  unsigned int idx, idx2;
  int ctxt;
  BOX_AVCC_T *pBoxAvcC = (BOX_AVCC_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { ver:0x%x, profile:%s (0x%x), compat:0x%x, level:%u, len:0x%x }\n", 
      pBoxAvcC->avcc.configversion, h264_getPrintableProfileStr(pBoxAvcC->avcc.avcprofile),
      pBoxAvcC->avcc.avcprofile, pBoxAvcC->avcc.profilecompatibility,
      pBoxAvcC->avcc.avclevel, pBoxAvcC->avcc.lenminusone);

    if(pBoxAvcC->avcc.psps) {
      for(idx = 0; idx < pBoxAvcC->avcc.numsps; idx++) {
        ctxt = context;
        PRINT_INDENT(fp, ctxt);
        fprintf(fp, "   { sps[%d]:", idx);
        for(idx2 = 0; idx2 < pBoxAvcC->avcc.psps[idx].len; idx2++) {
          if(idx2 > 0 && idx2 % 8 == 0) {
            fprintf(fp, "\n");
            ctxt = context;
            PRINT_INDENT(fp, ctxt);
            fprintf(fp, "            ");
          }
          fprintf(fp, " 0x%02x", pBoxAvcC->avcc.psps[idx].data[idx2]);
        }
        fprintf(fp, " }\n");
      }
    }

    if(pBoxAvcC->avcc.ppps) {
      for(idx = 0; idx < pBoxAvcC->avcc.numpps; idx++) {
        ctxt = context;
        PRINT_INDENT(fp, ctxt);
        fprintf(fp, "   { pps[%d]:", idx);
        for(idx2 = 0; idx2 < pBoxAvcC->avcc.ppps[idx].len; idx2++) {
          if(idx2 > 0 && idx2 % 8 == 0) {
            fprintf(fp, "\n");
            ctxt = context;
            PRINT_INDENT(fp, ctxt);
            fprintf(fp, "            ");
          }
          fprintf(fp, " 0x%02x", pBoxAvcC->avcc.ppps[idx].data[idx2]);
        }
        fprintf(fp, " }\n");
      }
    }

  }
}

static void free_box_avcC(BOX_T *pBox) {
  BOX_AVCC_T *pBoxAvcC = (BOX_AVCC_T *) pBox;

  avcc_freeCfg(&pBoxAvcC->avcc); 
}

static BOX_T *read_box_samr(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  unsigned int pos = 0;
  BOX_SAMR_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 28) {
    LOG(X_ERROR("Invalid samr body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 28)) < 0) {  
    return NULL;
  }

  pBox = (BOX_SAMR_T *) avc_calloc(1, sizeof(BOX_SAMR_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));

  memcpy(&pBox->reserved, &arena[pos], 28);
  pos += 28;

  pBox->box.subs = 1;
  pBox->box.szdata_ownbox = pos;

  return (BOX_T *) pBox;
}

static int write_box_samr(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_SAMR_T *pBoxSamr = (BOX_SAMR_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], pBoxSamr->reserved, 6);
  pos += 6;
  WRITE_UINT16(arena, pos, htons(pBoxSamr->datarefidx));
  memcpy(&arena[pos], pBoxSamr->reserved2, 16);
  pos += 16;
  WRITE_UINT16(arena, pos, htons(pBoxSamr->timescale));
  WRITE_UINT16(arena, pos, htons(pBoxSamr->reserved3));

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("samr box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_samr(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_SAMR_T *pSamr = (BOX_SAMR_T *) pBox;
  int ctxt;

  dump_box(pBox, context, fp);
  if(verbose) {
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { dataref:0x%x, %uHz }\n",
            htons(pSamr->datarefidx), htons(pSamr->timescale));
  }
}

static BOX_T *read_box_damr(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  unsigned int pos = 0;
  BOX_DAMR_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 8) {
    LOG(X_ERROR("Invalid damr body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 9)) < 0) {  
    return NULL;
  }

  pBox = (BOX_DAMR_T *) avc_calloc(1, sizeof(BOX_DAMR_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));

  memcpy(&pBox->vendor, &arena[pos], 5);
  pos += 5;

  memcpy(&pBox->modeset, &arena[pos], 4);
  pos += 4;

  return (BOX_T *) pBox;
}

static int write_box_damr(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_DAMR_T *pBoxDamr = (BOX_DAMR_T *) pBox;
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ?
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  WRITE_UINT32(arena, pos, htons(pBoxDamr->vendor));
  WRITE_UINT8(arena, pos, pBoxDamr->decoderver);
  WRITE_UINT16(arena, pos, htons(pBoxDamr->modeset));
  WRITE_UINT8(arena, pos, pBoxDamr->modechangeperiod);
  WRITE_UINT8(arena, pos, pBoxDamr->framespersample);

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("samrbox length mismatch.  write: %d, sz: %d"),
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_damr(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_DAMR_T *pDamr = (BOX_DAMR_T *) pBox;
  int ctxt;

  dump_box(pBox, context, fp);
  if(verbose) {
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { vendor: %c%c%c%c, ver: 0x%x, mode: 0x%x, period: %u, %u fr/sample } \n",
          ((unsigned char *)&pDamr->vendor)[0], ((unsigned char *)&pDamr->vendor)[1],
          ((unsigned char *)&pDamr->vendor)[2], ((unsigned char *)&pDamr->vendor)[3],
          pDamr->decoderver, htons(pDamr->modeset), pDamr->modechangeperiod,
          pDamr->framespersample );
  }
}

static BOX_T *read_box_btrt(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {

  int rc;
  unsigned int pos = 0;
  BOX_BTRT_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len != 12) {
    LOG(X_ERROR("Invalid btrt body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 12)) < 0) {  
    return NULL;
  }

  pBox = (BOX_BTRT_T *) avc_calloc(1, sizeof(BOX_BTRT_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));


  memcpy(&pBox->bufferSizeDB, &arena[pos], 12);
  pos += 12;

  return (BOX_T *) pBox;
}

static int write_box_btrt(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_BTRT_T *pBoxBtrt = (BOX_BTRT_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);


  memcpy(&arena[pos], &pBoxBtrt->bufferSizeDB, 12);
  pos += 12;

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("btrt box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_btrt(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_BTRT_T *pBoxBtrt = (BOX_BTRT_T *) pBox;
  int ctxt = context;

  dump_box(pBox, context, fp);
  if(verbose) {

    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { buffer size: %u, max: %u avg:%u }\n", 
      htonl(pBoxBtrt->bufferSizeDB), htonl(pBoxBtrt->maxBtrt), htonl(pBoxBtrt->avgBtrt));
  }
}

static BOX_T *read_box_stts(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  unsigned int pos = 0;
  int lenread = 0;
  unsigned int entryIdx;
  BOX_STTS_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 8) {
    LOG(X_ERROR("Invalid stts body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 8)) < 0) {  
    return NULL;
  }

  pBox = (BOX_STTS_T *) avc_calloc(1, sizeof(BOX_STTS_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);
  pBox->list.entrycnt = READ_UINT32(arena, pos);

  if(pBox->list.entrycnt > 0) {
    if(pBox->list.entrycnt * 8 > len - 8) {
      LOG(X_WARNING("Invalid stts sample count: %d"), pBox->list.entrycnt);
      *prc = -1;
      return (BOX_T *) pBox;    
    } else if((pBox->list.pEntries = (BOX_STTS_ENTRY_T *) avc_calloc(pBox->list.entrycnt, 
                                                  sizeof(BOX_STTS_ENTRY_T))) == NULL) {
      LOG(X_CRITICAL("Failed to allocate %d bytes for stts block"), pBox->list.entrycnt);
      *prc = -1;
      return (BOX_T *) pBox;
    }
    if(sz_arena % 8 != 0) {
      sz_arena = (sz_arena / 8) * 8;
    }
    for(entryIdx = 0; entryIdx < pBox->list.entrycnt; entryIdx++) {

      if(pos >= sz_arena || lenread <= 0) {
        lenread = ((pBox->list.entrycnt - entryIdx) * 8) > sz_arena ? sz_arena : 
                          ((pBox->list.entrycnt - entryIdx) * 8);
        if((rc = pStream->cbRead(pStream, arena, lenread)) < 0) {  
          *prc = -1;
          return (BOX_T *) pBox;
        }
        pos = 0;
      }

      pBox->list.pEntries[entryIdx].samplecnt = READ_UINT32(arena, pos);
      pBox->list.pEntries[entryIdx].sampledelta = READ_UINT32(arena, pos);
    }
  }

  return (BOX_T *) pBox;
}

static int write_box_stts(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_STTS_T *pBoxStts = (BOX_STTS_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;
  unsigned int numWritten = 0;
  unsigned int idx;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxStts->fullbox.version, 4);
  pos += 4;
  WRITE_UINT32(arena, pos, htonl(pBoxStts->list.entrycnt));

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }
  numWritten += pos;
  pos = 0;

  for(idx = 0; idx < pBoxStts->list.entrycnt; idx++) {

    if(pos >= sz_arena - sizeof(BOX_STTS_ENTRY_T)) {
      if(WriteFileStream(pStreamOut, arena, pos) < 0) {
        return -1;
      }
      numWritten += pos;
      pos = 0;
    }

    WRITE_UINT32(arena, pos, htonl(pBoxStts->list.pEntries[idx].samplecnt));
    WRITE_UINT32(arena, pos, htonl(pBoxStts->list.pEntries[idx].sampledelta));
  }

  numWritten += pos;

  if(numWritten != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("stts box length mismatch.  write: %d, sz: %d"), 
                numWritten, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }
  
  if(pos > 0) {
    if(WriteFileStream(pStreamOut, arena, pos) < 0) {
      return -1;
    }
  }

  return rc;
}



static void dump_box_stts(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_STTS_T *pBoxStts = (BOX_STTS_T *) pBox;
  unsigned int idx;
  int ctxt;
  //int linelen = 0;

  dump_box(pBox, context, fp);
  if(verbose) {
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { entries: %u", pBoxStts->list.entrycnt );
    if(verbose <= VSX_VERBOSITY_NORMAL && pBoxStts->list.entrycnt > 0) {
        fprintf(fp, ", %u:%u (0x%02x)", pBoxStts->list.pEntries[0].samplecnt, 
                  pBoxStts->list.pEntries[0].sampledelta, 
                  pBoxStts->list.pEntries[0].sampledelta);
    } else if(pBoxStts->list.entrycnt > 0) {
      fprintf(fp, ", samplecount:delta follows");
    }
    fprintf(fp, " }\n");

    if(verbose > VSX_VERBOSITY_NORMAL) {

      if(pBoxStts->list.pEntries) {
        ctxt = context;
        PRINT_INDENT(fp, ctxt);
        fprintf(fp, "   {");
        for(idx = 0; idx < pBoxStts->list.entrycnt; idx++) {
          if(idx > 0 && idx % 8 == 0) {
            fprintf(fp, "\n");
            ctxt = context;
            PRINT_INDENT(fp, ctxt);
            fprintf(fp, "    ");
          }
         fprintf(fp, " %u:%u(0x%02x)", pBoxStts->list.pEntries[idx].samplecnt, 
                  pBoxStts->list.pEntries[idx].sampledelta, 
                  pBoxStts->list.pEntries[idx].sampledelta);
        }
        fprintf(fp, " }\n");
      }
    }
  }
}

void mp4_free_avsync_stts(BOX_STTS_ENTRY_LIST_T *pList) {
  if(pList && pList->pEntries) {
    free(pList->pEntries);
  }
}

static void free_box_stts(BOX_T *pBox) {
  BOX_STTS_T *pBoxStts = (BOX_STTS_T *) pBox;

  mp4_free_avsync_stts(&pBoxStts->list);
}


static BOX_T *read_box_ctts(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  unsigned int pos = 0;
  int lenread = 0;
  unsigned int entryIdx;
  BOX_CTTS_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 8) {
    LOG(X_ERROR("Invalid ctts body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 8)) < 0) {  
    return NULL;
  }

  pBox = (BOX_CTTS_T *) avc_calloc(1, sizeof(BOX_CTTS_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);
  pBox->entrycnt = READ_UINT32(arena, pos);

  if(pBox->entrycnt > 0) {
    if(pBox->entrycnt * 8 > len - 8) {
      LOG(X_WARNING("Invalid ctts sample count: %d"), pBox->entrycnt);
      *prc = -1;
      return (BOX_T *) pBox;    
    } else if((pBox->pEntries = (BOX_CTTS_ENTRY_T *) avc_calloc(pBox->entrycnt, sizeof(BOX_CTTS_ENTRY_T))) == NULL) {
      LOG(X_CRITICAL("Failed to allocate %d bytes for ctts block"), pBox->entrycnt);
      *prc = -1;
      return (BOX_T *) pBox;
    }
    if(sz_arena % 8 != 0) {
      sz_arena = (sz_arena / 8) * 8;
    }
    for(entryIdx = 0; entryIdx < pBox->entrycnt; entryIdx++) {

      if(pos >= sz_arena || lenread <= 0) {
        lenread = ((pBox->entrycnt - entryIdx) * 8) > sz_arena ? sz_arena : 
                          ((pBox->entrycnt - entryIdx) * 8);
        if((rc = pStream->cbRead(pStream, arena, lenread)) < 0) {  
          *prc = -1;
          return (BOX_T *) pBox;
        }
        pos = 0;
      }

      pBox->pEntries[entryIdx].samplecount = READ_UINT32(arena, pos);
      pBox->pEntries[entryIdx].sampleoffset = READ_UINT32(arena, pos);
    }
  }

  return (BOX_T *) pBox;
}

static int write_box_ctts(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_CTTS_T *pBoxCtts = (BOX_CTTS_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;
  unsigned int numWritten = 0;
  unsigned int idx;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxCtts->fullbox.version, 4);
  pos += 4;
  WRITE_UINT32(arena, pos, htonl(pBoxCtts->entrycnt));

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }
  numWritten += pos;
  pos = 0;

  for(idx = 0; idx < pBoxCtts->entrycnt; idx++) {

    if(pos >= sz_arena - sizeof(BOX_CTTS_ENTRY_T)) {
      if(WriteFileStream(pStreamOut, arena, pos) < 0) {
        return -1;
      }
      numWritten += pos;
      pos = 0;
    }

    WRITE_UINT32(arena, pos, htonl(pBoxCtts->pEntries[idx].samplecount));
    WRITE_UINT32(arena, pos, htonl(pBoxCtts->pEntries[idx].sampleoffset));
  }

  numWritten += pos;

  if(numWritten != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("ctts box length mismatch.  write: %d, sz: %d"), 
                numWritten, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }
  
  if(pos > 0) {
    if(WriteFileStream(pStreamOut, arena, pos) < 0) {
      return -1;
    }
  }

  return rc;
}

static void dump_box_ctts(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_CTTS_T *pBoxCtts = (BOX_CTTS_T *) pBox;
  unsigned int idx;
  int ctxt;

  dump_box(pBox, context, fp);
  if(verbose) {

    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { entries: %u }\n", pBoxCtts->entrycnt );

    if(verbose > VSX_VERBOSITY_NORMAL) {

      if(pBoxCtts->pEntries) {
        ctxt = context;
        PRINT_INDENT(fp, ctxt);
        fprintf(fp, "   {");
        for(idx = 0; idx < pBoxCtts->entrycnt; idx++) {
          if(idx > 0 && idx % 4 == 0) {
            fprintf(fp, "\n");
            ctxt = context;
            PRINT_INDENT(fp, ctxt);
            fprintf(fp, "    ");
          }
          fprintf(fp, " [%d]:%u 0x%02x", idx, pBoxCtts->pEntries[idx].samplecount,
                  pBoxCtts->pEntries[idx].sampleoffset);
        }
        fprintf(fp, " }\n");
      }
    }
  }

}

static void free_box_ctts(BOX_T *pBox) {
  BOX_CTTS_T *pBoxCtts = (BOX_CTTS_T *) pBox;

  if(pBoxCtts->pEntries) {
    free(pBoxCtts->pEntries);
  }
}

static BOX_T *read_box_stsc(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  unsigned int pos = 0;
  int lenread = 0;
  unsigned int entryIdx;
  BOX_STSC_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 8) {
    LOG(X_ERROR("Invalid stsc body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 8)) < 0) {  
    return NULL;
  }

  pBox = (BOX_STSC_T *) avc_calloc(1, sizeof(BOX_STSC_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);
  pBox->entrycnt = READ_UINT32(arena, pos);

  if(pBox->entrycnt > 0) {
    if(pBox->entrycnt * 12 > len - 8) {
      LOG(X_WARNING("Invalid stsc sample count: %d"), pBox->entrycnt);
      *prc = -1;
      return (BOX_T *) pBox;    
    } else if((pBox->pEntries = (BOX_STSC_ENTRY_T *) avc_calloc(pBox->entrycnt, 
                                                sizeof(BOX_STSC_ENTRY_T))) == NULL) {
      LOG(X_CRITICAL("Failed to allocate %d bytes for stsc block"), pBox->entrycnt);
      *prc = -1;
      return (BOX_T *) pBox;
    }
    if(sz_arena % 12 != 0) {
      sz_arena = (sz_arena / 12) * 12;
    }
    for(entryIdx = 0; entryIdx < pBox->entrycnt; entryIdx++) {

      if(pos >= sz_arena || lenread <= 0) {
        lenread = ((pBox->entrycnt - entryIdx) * 12) > sz_arena ? sz_arena : 
                          ((pBox->entrycnt - entryIdx) * 12);
        if((rc = pStream->cbRead(pStream, arena, lenread)) < 0) {  
          *prc = -1;
          return (BOX_T *) pBox;
        }
        pos = 0;
      }

      pBox->pEntries[entryIdx].firstchunk = READ_UINT32(arena, pos);
      pBox->pEntries[entryIdx].sampleperchunk = READ_UINT32(arena, pos);
      pBox->pEntries[entryIdx].sampledescidx = READ_UINT32(arena, pos);
    }
  }

  return (BOX_T *) pBox;
}

static int write_box_stsc(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_STSC_T *pBoxStsc = (BOX_STSC_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;
  unsigned int numWritten = 0;
  unsigned int idx;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxStsc->fullbox.version, 4);
  pos += 4;
  WRITE_UINT32(arena, pos, htonl(pBoxStsc->entrycnt));

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }
  numWritten += pos;
  pos = 0;

  for(idx = 0; idx < pBoxStsc->entrycnt; idx++) {

    if(pos >= sz_arena - sizeof(BOX_STSC_ENTRY_T)) {
      if(WriteFileStream(pStreamOut, arena, pos) < 0) {
        return -1;
      }
      numWritten += pos;
      pos = 0;
    }

    WRITE_UINT32(arena, pos, htonl(pBoxStsc->pEntries[idx].firstchunk));
    WRITE_UINT32(arena, pos, htonl(pBoxStsc->pEntries[idx].sampleperchunk));
    WRITE_UINT32(arena, pos, htonl(pBoxStsc->pEntries[idx].sampledescidx));
  }

  numWritten += pos;
  if(numWritten != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("stsc box length mismatch.  write: %d, sz: %d"), 
                numWritten, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }
  
  if(pos > 0) {
    if(WriteFileStream(pStreamOut, arena, pos) < 0) {
      return -1;
    }
  }

  return rc;
}

static void dump_box_stsc(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_STSC_T *pBoxStsc = (BOX_STSC_T *) pBox;
  unsigned int idx = 0;
  int ctxt;

  dump_box(pBox, context, fp);
  if(verbose) {

    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { entries: %u }\n", pBoxStsc->entrycnt );

    if(verbose > VSX_VERBOSITY_NORMAL) {

      if(pBoxStsc->pEntries) {
        ctxt = context;
        PRINT_INDENT(fp, ctxt);
        fprintf(fp, "   {");
        for(idx = 0; idx < pBoxStsc->entrycnt; idx++) {
          if(idx > 0 && idx % 4 == 0) {
            fprintf(fp, "\n");
            ctxt = context;
            PRINT_INDENT(fp, ctxt);
            fprintf(fp, "    ");
          }
          fprintf(fp, " [%d]:0x%02x 0x%02x 0x%02x", idx, pBoxStsc->pEntries[idx].firstchunk,
                  pBoxStsc->pEntries[idx].sampleperchunk, pBoxStsc->pEntries[idx].sampledescidx);
        }
        fprintf(fp, " }\n");
      }
    }
  }
}

static void free_box_stsc(BOX_T *pBox) {
  BOX_STSC_T *pBoxStsc = (BOX_STSC_T *) pBox;

  if(pBoxStsc->pEntries) {
    free(pBoxStsc->pEntries);
  }
}


static BOX_T *read_box_stsz(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  unsigned int pos = 0;
  unsigned int entryIdx;
  int lenread = 0;
  BOX_STSZ_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 12) {
    LOG(X_ERROR("Invalid stsz body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 12)) < 0) {  
    return NULL;
  }

  pBox = (BOX_STSZ_T *) avc_calloc(1, sizeof(BOX_STSZ_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);
  pBox->samplesize = READ_UINT32(arena, pos);
  pBox->samplecount = READ_UINT32(arena, pos);

  if(pBox->samplecount > 0 && pBox->samplesize == 0) {
    if(pBox->samplecount * 4 > len - 12) {
      LOG(X_WARNING("Invalid stsz sample count: %d"), pBox->samplecount);
      *prc = -1;
      return (BOX_T *) pBox;    
    } else if((pBox->pSamples = (uint32_t *) avc_calloc(pBox->samplecount, sizeof(uint32_t))) == NULL) {
      LOG(X_CRITICAL("Failed to allocate %d bytes for stsz block"), pBox->samplecount);
      *prc = -1;
      return (BOX_T *) pBox;
    }
    for(entryIdx = 0; entryIdx < pBox->samplecount; entryIdx++) {

      if(pos >= sz_arena || lenread <= 0) {
        lenread = ((pBox->samplecount - entryIdx) * 4) > sz_arena ? sz_arena : 
                          ((pBox->samplecount - entryIdx) * 4);
        if((rc = pStream->cbRead(pStream, arena, lenread)) < 0) {  
          *prc = -1;
          return (BOX_T *) pBox;
        }
        pos = 0;
      }

      pBox->pSamples[entryIdx] = READ_UINT32(arena, pos);
    }
  }

  return (BOX_T *) pBox;
}

static int write_box_stsz(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_STSZ_T *pBoxStsz = (BOX_STSZ_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;
  unsigned int numWritten = 0;
  unsigned int idx;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxStsz->fullbox.version, 4);
  pos += 4;
  WRITE_UINT32(arena, pos, htonl(pBoxStsz->samplesize));
  WRITE_UINT32(arena, pos, htonl(pBoxStsz->samplecount));

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }
  numWritten += pos;
  pos = 0;

  for(idx = 0; idx < pBoxStsz->samplecount; idx++) {

    if(pos >= sz_arena - sizeof(uint32_t)) {
      if(WriteFileStream(pStreamOut, arena, pos) < 0) {
        return -1;
      }
      numWritten += pos;
      pos = 0;
    }

    WRITE_UINT32(arena, pos, htonl(pBoxStsz->pSamples[idx]));
  }

  numWritten += pos;
  if(numWritten != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("stsz box length mismatch.  write: %d, sz: %d"), 
                numWritten, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }
  
  if(pos > 0) {
    if(WriteFileStream(pStreamOut, arena, pos) < 0) {
      return -1;
    }
  }

  return rc;
}

static void dump_box_stsz(BOX_T *pBox, int context, int verbose, FILE *fp) {
  unsigned int idx;
  int ctxt = context;
  BOX_STSZ_T *pBoxStsz = (BOX_STSZ_T *) pBox;


  dump_box(pBox, context, fp);
  if(verbose) {
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { samplesize: %u, samplecount: %u }\n", 
                pBoxStsz->samplesize, pBoxStsz->samplecount );


    if(verbose > VSX_VERBOSITY_NORMAL) {

      if(pBoxStsz->pSamples && pBoxStsz->samplecount > 0) {
        ctxt = context;
        PRINT_INDENT(fp, ctxt);
        fprintf(fp, "   {");
        for(idx = 0; idx < pBoxStsz->samplecount; idx++) {
          if(idx > 0 && idx % 8 == 0) {
            fprintf(fp, "\n");
            ctxt = context;
            PRINT_INDENT(fp, ctxt);
            fprintf(fp, "    ");
          }
          fprintf(fp, " [%d]:%u", idx, pBoxStsz->pSamples[idx]);
        }
        fprintf(fp, " }\n");
      }
    }


  }
}

static void free_box_stsz(BOX_T *pBox) {
  BOX_STSZ_T *pBoxStsz = (BOX_STSZ_T *) pBox;

  if(pBoxStsz->pSamples) {
    free(pBoxStsz->pSamples);
  }
}


static BOX_T *read_box_stco(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  unsigned int pos = 0;
  unsigned int idx;
  int lenread = 0;
  BOX_STCO_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 8) {
    LOG(X_ERROR("Invalid stco body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 8)) < 0) {  
    return NULL;
  }

  pBox = (BOX_STCO_T *) avc_calloc(1, sizeof(BOX_STCO_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);
  pBox->entrycnt = READ_UINT32(arena, pos);

  if(pBox->entrycnt > 0) {
    if(pBox->entrycnt * 4 > len - 8) {
      LOG(X_WARNING("Invalid stco entry count: %d"), pBox->entrycnt);
      *prc = -1;
      return (BOX_T *) pBox;    
    } else if((pBox->pSamples = (uint32_t *) avc_calloc(pBox->entrycnt, sizeof(uint32_t))) == NULL) {
      LOG(X_CRITICAL("Failed to allocate %d bytes for stco block"), pBox->entrycnt);
      *prc = -1;
      return (BOX_T *) pBox;
    }
    for(idx = 0; idx < pBox->entrycnt; idx++) {

      if(pos >= sz_arena || lenread <= 0) {
        lenread = ((pBox->entrycnt - idx) * 4) > sz_arena ? sz_arena : 
                          ((pBox->entrycnt - idx) * 4);
        if((rc = pStream->cbRead(pStream, arena, lenread)) < 0) {  
          *prc = -1;
          return (BOX_T *) pBox;
        }
        pos = 0;
      }

      pBox->pSamples[idx] = READ_UINT32(arena, pos);
    }
  }

  return (BOX_T *) pBox;
}

static int write_box_stco(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_STCO_T *pBoxStco = (BOX_STCO_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;
  unsigned int numWritten = 0;
  unsigned int idx;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxStco->fullbox.version, 4);
  pos += 4;
  WRITE_UINT32(arena, pos, htonl(pBoxStco->entrycnt));

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }
  numWritten += pos;
  pos = 0;

  for(idx = 0; idx < pBoxStco->entrycnt; idx++) {

    if(pos >= sz_arena - sizeof(uint32_t)) {
      if(WriteFileStream(pStreamOut, arena, pos) < 0) {
        return -1;
      }
      numWritten += pos;
      pos = 0;
    }

    WRITE_UINT32(arena, pos, htonl(pBoxStco->pSamples[idx]));
  }

  numWritten += pos;

  if(numWritten != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("stco box length mismatch.  write: %d, sz: %d"), 
                numWritten, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }
  
  if(pos > 0) {
    if(WriteFileStream(pStreamOut, arena, pos) < 0) {
      return -1;
    }
  }

  return rc;
}

static void dump_box_stco(BOX_T *pBox, int context, int verbose, FILE *fp) {
  unsigned int idx;
  int ctxt = context;
  BOX_STCO_T *pBoxStco = (BOX_STCO_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { entrycnt: %u }\n", pBoxStco->entrycnt );

    if(verbose > VSX_VERBOSITY_NORMAL) {

      if(pBoxStco->pSamples && pBoxStco->entrycnt > 0) {
        ctxt = context;
        PRINT_INDENT(fp, ctxt);
        fprintf(fp, "   {");
        for(idx = 0; idx < pBoxStco->entrycnt; idx++) {
          if(idx > 0 && idx % 8 == 0) {
            fprintf(fp, "\n");
            ctxt = context;
            PRINT_INDENT(fp, ctxt);
            fprintf(fp, "    ");
          }
          fprintf(fp, " [%d]:0x%02x", idx, pBoxStco->pSamples[idx]);
        }
        fprintf(fp, " }\n");
      }
    }


  }
}

static void free_box_stco(BOX_T *pBox) {
  BOX_STCO_T *pBoxStco = (BOX_STCO_T *) pBox;

  if(pBoxStco->pSamples) {
    free(pBoxStco->pSamples);
  }
}


static BOX_T *read_box_stss(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  unsigned int pos = 0;
  unsigned int idx;
  int lenread = 0;
  BOX_STSS_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 8) {
    LOG(X_ERROR("Invalid stss body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 8)) < 0) {  
    return NULL;
  }

  pBox = (BOX_STSS_T *) avc_calloc(1, sizeof(BOX_STSS_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);
  pBox->entrycnt = READ_UINT32(arena, pos);

  if(pBox->entrycnt > 0) {
    if(pBox->entrycnt * 4 > len - 8) {
      LOG(X_WARNING("Invalid stss entry count: %d"), pBox->entrycnt);
      *prc = -1;
      return (BOX_T *) pBox;    
    } else if((pBox->pSamples = (uint32_t *) avc_calloc(pBox->entrycnt, sizeof(uint32_t))) == NULL) {
      LOG(X_CRITICAL("Failed to allocate %d bytes for stco block"), pBox->entrycnt);
      *prc = -1;
      return (BOX_T *) pBox;
    }
    for(idx = 0; idx < pBox->entrycnt; idx++) {

      if(pos >= sz_arena || lenread <= 0) {
        lenread = ((pBox->entrycnt - idx) * 4) > sz_arena ? sz_arena : 
                          ((pBox->entrycnt - idx) * 4);
        if((rc = pStream->cbRead(pStream, arena, lenread)) < 0) {  
          *prc = -1;
          return (BOX_T *) pBox;
        }
        pos = 0;
      }

      pBox->pSamples[idx] = READ_UINT32(arena, pos);
    }
  }

  return (BOX_T *) pBox;
}


static int write_box_stss(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_STSS_T *pBoxStss = (BOX_STSS_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;
  unsigned int numWritten = 0;
  unsigned int idx;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxStss->fullbox.version, 4);
  pos += 4;
  WRITE_UINT32(arena, pos, htonl(pBoxStss->entrycnt));

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }
  numWritten += pos;
  pos = 0;

  for(idx = 0; idx < pBoxStss->entrycnt; idx++) {

    if(pos >= sz_arena - sizeof(uint32_t)) {
      if(WriteFileStream(pStreamOut, arena, pos) < 0) {
        return -1;
      }
      numWritten += pos;
      pos = 0;
    }

    WRITE_UINT32(arena, pos, htonl(pBoxStss->pSamples[idx]));
  }

  numWritten += pos;

  if(numWritten != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("stss box length mismatch.  write: %d, sz: %d"), 
                numWritten, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }
  
  if(pos > 0) {
    if(WriteFileStream(pStreamOut, arena, pos) < 0) {
      return -1;
    }
  }

  return rc;
}

static void dump_box_stss(BOX_T *pBox, int context, int verbose, FILE *fp) {
  unsigned int idx;
  int ctxt = context;
  BOX_STSS_T *pBoxStss = (BOX_STSS_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { entrycnt: %u }\n", pBoxStss->entrycnt );

    if(verbose > VSX_VERBOSITY_NORMAL) {

      if(pBoxStss->pSamples && pBoxStss->entrycnt > 0) {
        ctxt = context;
        PRINT_INDENT(fp, ctxt);
        fprintf(fp, "   {");
        for(idx = 0; idx < pBoxStss->entrycnt; idx++) {
          if(idx > 0 && idx % 8 == 0) {
            fprintf(fp, "\n");
            ctxt = context;
            PRINT_INDENT(fp, ctxt);
            fprintf(fp, "    ");
          }
          fprintf(fp, " [%d]:0x%02x", idx, pBoxStss->pSamples[idx]);
        }
        fprintf(fp, " }\n");
      }
    }


  }
}

static void free_box_stss(BOX_T *pBox) {
  BOX_STSS_T *pBoxStss = (BOX_STSS_T *) pBox;

  if(pBoxStss->pSamples) {
    free(pBoxStss->pSamples);
  }
}

static BOX_T *read_box_mehd(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  BOX_MEHD_T *pBoxMehd = NULL;
  unsigned int pos = 0;
  uint32_t len = (uint32_t) pBoxHdr->szdata;
  const unsigned int MEHD_LEN = 8;

  if(len < MEHD_LEN) {
    LOG(X_ERROR("Invalid mehd body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, MEHD_LEN)) < 0) {
    return NULL;
  }

  pBoxMehd = (BOX_MEHD_T *) avc_calloc(1, sizeof(BOX_MEHD_T));
  memcpy(&pBoxMehd->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBoxMehd->fullbox, arena, pos += 4);

  if(pBoxMehd->fullbox.version == 1) {

    if(len != MEHD_LEN + 4) {
      LOG(X_ERROR("Invalid mehd body length: %u for version 1"), len);
      return NULL;
    }

    if((rc = pStream->cbRead(pStream, &arena[MEHD_LEN], 4)) < 0) {
      return NULL;
    }

    pBoxMehd->duration = READ_UINT64(arena, pos);

  } else {

    if(len != MEHD_LEN) {
      LOG(X_ERROR("Invalid mehd body length: %u for version 0"), len);
      return NULL;
    }

    pBoxMehd->duration = READ_UINT32(arena, pos);

  }

  return (BOX_T *) pBoxMehd;
}

static int write_box_mehd(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_MEHD_T *pBoxMehd = (BOX_MEHD_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxMehd->fullbox.version, 4);
  pos += 4;
  WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxMehd->duration));

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("mehd box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_mehd(BOX_T *pBox, int context, int verbose, FILE *fp) {
  int ctxt = context;
  BOX_MEHD_T *pBoxMehd  = (BOX_MEHD_T *) pBox;

  dump_box(pBox, context, fp);

  PRINT_INDENT(fp, ctxt);
  fprintf(fp, "   { fragment duration:%llu }\n", pBoxMehd->duration);

}

static BOX_T *read_box_trex(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  BOX_TREX_T *pBoxTrex = NULL;
  unsigned int pos = 0;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len != 24) {
    LOG(X_ERROR("Invalid trex body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, len)) < 0) {
    return NULL;
  }

  pBoxTrex = (BOX_TREX_T *) avc_calloc(1, sizeof(BOX_TREX_T));
  memcpy(&pBoxTrex->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBoxTrex->fullbox, arena, pos += 4);
  pBoxTrex->trackid = READ_UINT32(arena, pos);
  pBoxTrex->default_sample_description_index = READ_UINT32(arena, pos);
  pBoxTrex->default_sample_duration = READ_UINT32(arena, pos);
  pBoxTrex->default_sample_size = READ_UINT32(arena, pos);
  pBoxTrex->default_sample_flags = READ_UINT32(arena, pos);

  return (BOX_T *) pBoxTrex;
}

static int write_box_trex(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_TREX_T *pBoxTrex = (BOX_TREX_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxTrex->fullbox.version, 4);
  pos += 4;
  WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxTrex->trackid));
  WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxTrex->default_sample_description_index));
  WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxTrex->default_sample_duration));
  WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxTrex->default_sample_size));
  WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxTrex->default_sample_flags));

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("trex box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_trex(BOX_T *pBox, int context, int verbose, FILE *fp) {
  int ctxt = context;
  BOX_TREX_T *pBoxTrex = (BOX_TREX_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {

    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { trackid:%d, description idx:%d, duration:%d, size: %d, flags: 0x%x }\n", 
             pBoxTrex->trackid, 
             pBoxTrex->default_sample_description_index,
             pBoxTrex->default_sample_duration,
             pBoxTrex->default_sample_size,
             pBoxTrex->default_sample_flags);
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { depends on: %d, is depended on:%d, redundancy: %d, padding: %d, \n",
         TREX_SAMPLE_DEPENDS_ON(pBoxTrex->default_sample_flags),
         TREX_SAMPLE_IS_DEPENDED_ON(pBoxTrex->default_sample_flags),
         TREX_SAMPLE_HAS_REDUNDANCY(pBoxTrex->default_sample_flags),
         TREX_SAMPLE_PADDING_VALUE(pBoxTrex->default_sample_flags));
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "     is difference: %d, priority: %d }\n",
         TREX_SAMPLE_IS_DIFFERENCE_SAMPLE(pBoxTrex->default_sample_flags),
         TREX_SAMPLE_DEGRADATION_PRIORITY(pBoxTrex->default_sample_flags));

  }
}

static BOX_T *read_box_mfhd(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  BOX_MFHD_T *pBoxMfhd = NULL;
  unsigned int pos = 0;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len != 8) {
    LOG(X_ERROR("Invalid mfhd body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, len)) < 0) {
    return NULL;
  }

  pBoxMfhd = (BOX_MFHD_T *) avc_calloc(1, sizeof(BOX_MFHD_T));
  memcpy(&pBoxMfhd->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBoxMfhd->fullbox, arena, pos += 4);
  pBoxMfhd->sequence = READ_UINT32(arena, pos);
  //avc_dumpHex(stderr, arena, len , 1);
  //fprintf(stderr, "SEQ:0x%x, len:%d, pos:%d\n", pBoxMfhd->sequence, len, pos);
  //pBoxMfhd->box.subs = 1;
  //pBoxMfhd->box.szdata_ownbox = pos;

  return (BOX_T *) pBoxMfhd;
}

static int write_box_mfhd(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_MFHD_T *pBoxMfhd = (BOX_MFHD_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxMfhd->fullbox.version, 4);
  pos += 4;
  WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxMfhd->sequence));

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("mfhd box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_mfhd(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_MFHD_T *pBoxMfhd = (BOX_MFHD_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {

    PRINT_INDENT(fp, context);
    fprintf(fp, "   { sequence:%d }\n", pBoxMfhd->sequence);

  }
}

static BOX_T *read_box_tfhd(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  BOX_TFHD_T *pBoxTfhd = NULL;
  unsigned int pos = 0;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 8) {
    LOG(X_ERROR("Invalid tfhd body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, len)) < 0) {
    return NULL;
  }

  pBoxTfhd = (BOX_TFHD_T *) avc_calloc(1, sizeof(BOX_TFHD_T));
  memcpy(&pBoxTfhd->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBoxTfhd->fullbox, arena, pos += 4);
  pBoxTfhd->trackid = READ_UINT32(arena, pos);
  memcpy(&pBoxTfhd->flags, &pBoxTfhd->fullbox.flags, 3);
  pBoxTfhd->flags = htonl(pBoxTfhd->flags << 8);

  if(pBoxTfhd->flags & TFHD_FLAG_BASE_DATA_OFFSET) {
    pBoxTfhd->base_data_offset = READ_UINT64(arena, pos);
  }
  if(pBoxTfhd->flags & TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX) {
    pBoxTfhd->sample_description_index = READ_UINT32(arena, pos);
  }
  if(pBoxTfhd->flags & TFHD_FLAG_DEFAULT_SAMPLE_DURATION) {
    pBoxTfhd->default_sample_duration = READ_UINT32(arena, pos);
  }
  if(pBoxTfhd->flags & TFHD_FLAG_DEFAULT_SAMPLE_SIZE) {
    pBoxTfhd->default_sample_size = READ_UINT32(arena, pos);
  }
  if(pBoxTfhd->flags & TFHD_FLAG_DEFAULT_SAMPLE_FLAGS) {
    pBoxTfhd->default_sample_flags = READ_UINT32(arena, pos);
  }

  if(pos > len) {
    LOG(X_ERROR("Invalid tfhd flags 0x%x specified for data length:%d"), pBoxTfhd->flags, len);
    return NULL;
  }

  //avc_dumpHex(stderr, arena, len , 1);
  //fprintf(stderr, "TFHD trackId:%u, flags:0x%x, defs_d:%u, len:%d, pos:%d\n", pBoxTfhd->trackid, pBoxTfhd->flags, pBoxTfhd->default_sample_duration, len, pos);
  //pBoxMfhd->box.subs = 1;
  //pBoxMfhd->box.szdata_ownbox = pos;

  return (BOX_T *) pBoxTfhd;
}

static int write_box_tfhd(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  BOX_TFHD_T *pBoxTfhd = (BOX_TFHD_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxTfhd->fullbox.version, 4);
  pos += 4; 
  WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxTfhd->trackid));

  if(pBoxTfhd->flags & TFHD_FLAG_BASE_DATA_OFFSET) {
    WRITE_UINT64(arena, pos, pBoxTfhd->base_data_offset);
  }
  if(pBoxTfhd->flags & TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX) {
    WRITE_UINT32(arena, pos, htonl((uint32_t)pBoxTfhd->sample_description_index));
  }
  if(pBoxTfhd->flags & TFHD_FLAG_DEFAULT_SAMPLE_DURATION) {
    WRITE_UINT32(arena, pos, htonl(pBoxTfhd->default_sample_duration));
  }
  if(pBoxTfhd->flags & TFHD_FLAG_DEFAULT_SAMPLE_SIZE) {
    WRITE_UINT32(arena, pos, htonl(pBoxTfhd->default_sample_size));
  }
  if(pBoxTfhd->flags & TFHD_FLAG_DEFAULT_SAMPLE_FLAGS) {
    WRITE_UINT32(arena, pos, htonl(pBoxTfhd->default_sample_flags));
  }

  if(pos != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("tfhd box length mismatch.  write: %d, sz: %d"), 
                pos, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}

static void dump_box_tfhd(BOX_T *pBox, int context, int verbose, FILE *fp) {
  BOX_TFHD_T *pBoxTfhd = (BOX_TFHD_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {

    PRINT_INDENT(fp, context);
    fprintf(fp, "   { trackid:%u, flags: 0x%x", pBoxTfhd->trackid, pBoxTfhd->flags);
    if(pBoxTfhd->flags & TFHD_FLAG_BASE_DATA_OFFSET) {
      fprintf(fp, ", base offset: 0x%llx", pBoxTfhd->base_data_offset);
    }
    if(pBoxTfhd->flags & TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX) {
      fprintf(fp, ", sample idx: %u", pBoxTfhd->sample_description_index);
    }
    if(pBoxTfhd->flags & TFHD_FLAG_DEFAULT_SAMPLE_DURATION) {
      fprintf(fp, ", sample duration: %uHz", pBoxTfhd->default_sample_duration);
    }
    if(pBoxTfhd->flags & TFHD_FLAG_DEFAULT_SAMPLE_SIZE) {
      fprintf(fp, ", sample size: %u", pBoxTfhd->default_sample_size);
    }
    if(pBoxTfhd->flags & TFHD_FLAG_DEFAULT_SAMPLE_FLAGS) {
      fprintf(fp, ", sample flags: 0x%x", pBoxTfhd->default_sample_flags);
    }

    fprintf(fp, " }\n");

  }
}

static BOX_T *read_box_trun(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  BOX_TRUN_T *pBoxTrun = NULL;
  unsigned int lenread = 0;
  unsigned int idx;
  unsigned int pos = 0;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 8) {
    LOG(X_ERROR("Invalid trun body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 8)) < 0) {
    return NULL;
  }

  pBoxTrun = (BOX_TRUN_T *) avc_calloc(1, sizeof(BOX_TRUN_T));
  memcpy(&pBoxTrun->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBoxTrun->fullbox, arena, pos += 4);
  memcpy(&pBoxTrun->flags, &pBoxTrun->fullbox.flags, 3);
  pBoxTrun->flags = htonl(pBoxTrun->flags << 8);
  pBoxTrun->sample_count = READ_UINT32(arena, pos);

  if(pBoxTrun->flags & TRUN_FLAG_DATA_OFFSET) {
    if((rc = pStream->cbRead(pStream, &arena[pos], 4)) < 0) {
      return NULL;
    }
    pBoxTrun->data_offset = READ_UINT32(arena, pos);
  }
  if(pBoxTrun->flags & TRUN_FLAG_FIRST_SAMPLE_FLAGS) {
    if((rc = pStream->cbRead(pStream, &arena[pos], 4)) < 0) {
      return NULL;
    }
    pBoxTrun->first_sample_flags = READ_UINT32(arena, pos);
  }

  if(pBoxTrun->sample_count> 0) {

    if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_DURATION) {
      pBoxTrun->sample_size += 4;
    }
    if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_SIZE) {
      pBoxTrun->sample_size += 4;
    }
    if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_FLAGS) {
      pBoxTrun->sample_size += 4;
    }
    if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET) {
      pBoxTrun->sample_size += 4;
    }
    if(pBoxTrun->sample_size <= 0) {
      LOG(X_ERROR("Invalid trun sample size: %d"), pBoxTrun->sample_size); 
      *prc = -1;
      return (BOX_T *) pBoxTrun;
    } else if(pBoxTrun->sample_count * pBoxTrun->sample_size > len - pos) {
      LOG(X_ERROR("Invalid trun sample count: %d, sample size:%d"), 
                    pBoxTrun->sample_count, pBoxTrun->sample_size);
      *prc = -1;
      return (BOX_T *) pBoxTrun;
    } else if((pBoxTrun->pSamples = (BOX_TRUN_ENTRY_T*) avc_calloc(pBoxTrun->sample_count, 
                                                               sizeof(BOX_TRUN_ENTRY_T))) == NULL) {
      LOG(X_CRITICAL("Failed to allocate %d bytes for trun samples block"), pBoxTrun->sample_count);
      *prc = -1;
      return (BOX_T *) pBoxTrun;
    }
    pBoxTrun->alloc_count = pBoxTrun->sample_count;

    pos = 0;
    for(idx = 0; idx < pBoxTrun->sample_count; idx++) {

      if(pos >= sz_arena || lenread <= 0) {
        lenread = ((pBoxTrun->sample_count - idx) * pBoxTrun->sample_size) > sz_arena ? sz_arena :
                          ((pBoxTrun->sample_count - idx) * pBoxTrun->sample_size);
        if((rc = pStream->cbRead(pStream, arena, lenread)) < 0) {
          *prc = -1;
          return (BOX_T *) pBoxTrun;
        }
        pos = 0;
      }

      if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_DURATION) {
        pBoxTrun->pSamples[idx].sample_duration = READ_UINT32(arena, pos);
      }
      if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_SIZE) {
        pBoxTrun->pSamples[idx].sample_size = READ_UINT32(arena, pos);
      }
      if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_FLAGS) {
        pBoxTrun->pSamples[idx].sample_flags = READ_UINT32(arena, pos);
      }
      if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET) {
        pBoxTrun->pSamples[idx].sample_composition_time_offset = READ_UINT32(arena, pos);
      }

    }
  }

  return (BOX_T *) pBoxTrun;
}

static int write_box_trun(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {

  BOX_TRUN_T *pBoxTrun = (BOX_TRUN_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;
  unsigned int numWritten = 0;
  unsigned int idx;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxTrun->fullbox.version, 4);
  pos += 4;
  WRITE_UINT32(arena, pos, htonl(pBoxTrun->sample_count));

  if(pBoxTrun->flags & TRUN_FLAG_DATA_OFFSET) {
    WRITE_UINT32(arena, pos, htonl(pBoxTrun->data_offset));
  }
  if(pBoxTrun->flags & TRUN_FLAG_FIRST_SAMPLE_FLAGS) {
    WRITE_UINT32(arena, pos, htonl(pBoxTrun->first_sample_flags));
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }
  numWritten += pos;
  pos = 0;

  for(idx = 0; idx < pBoxTrun->sample_count; idx++) {

    if(pos >= sz_arena - sizeof(uint32_t)) {
      if(WriteFileStream(pStreamOut, arena, pos) < 0) {
        return -1;
      }
      numWritten += pos;
      pos = 0;
    }

    if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_DURATION) {
      WRITE_UINT32(arena, pos, htonl(pBoxTrun->pSamples[idx].sample_duration));
    }
    if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_SIZE) {
      WRITE_UINT32(arena, pos, htonl(pBoxTrun->pSamples[idx].sample_size));
    }
    if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_FLAGS) {
      WRITE_UINT32(arena, pos, htonl(pBoxTrun->pSamples[idx].sample_flags));
    }
    if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET) {
      WRITE_UINT32(arena, pos, htonl(pBoxTrun->pSamples[idx].sample_composition_time_offset));
    }

  }

  numWritten += pos;

  if(numWritten != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("trun box length mismatch.  write: %d, sz: %d"), 
                numWritten, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }
  
  if(pos > 0) {
    if(WriteFileStream(pStreamOut, arena, pos) < 0) {
      return -1;
    }
  }

  return rc;
}

static void dump_box_trun(BOX_T *pBox, int context, int verbose, FILE *fp) {
  unsigned int idx, width;
  int ctxt = context;
  unsigned int offset = 0;
  BOX_TRUN_T *pBoxTrun = (BOX_TRUN_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {
    ctxt = context;
    PRINT_INDENT(fp, ctxt);
    fprintf(fp, "   { sample count: %u, flags: 0x%x", pBoxTrun->sample_count, pBoxTrun->flags);
    if(pBoxTrun->flags & TRUN_FLAG_DATA_OFFSET) {
      fprintf(fp, ", data offset:0x%x", pBoxTrun->data_offset);
    }
    if(pBoxTrun->flags & TRUN_FLAG_FIRST_SAMPLE_FLAGS) {
      fprintf(fp, ", first sample flags:0x%x", pBoxTrun->first_sample_flags);
    }
    fprintf(fp, " }\n");

    if(verbose > VSX_VERBOSITY_NORMAL) {

      if(pBoxTrun->pSamples && pBoxTrun->sample_count > 0) {
        ctxt = context;
        PRINT_INDENT(fp, ctxt);
        fprintf(fp, "   {");

        if((pBoxTrun->flags & TRUN_FLAG_SAMPLE_DURATION) &&
           (pBoxTrun->flags & TRUN_FLAG_SAMPLE_SIZE) &&
           (pBoxTrun->flags & TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET)) {
          width = 1;
        } else {
          width = 2;
        }

        for(idx = 0; idx < pBoxTrun->sample_count; idx++) {
          if(idx > 0 && idx % width == 0) {
            fprintf(fp, "\n");
            ctxt = context;
            PRINT_INDENT(fp, ctxt);
            fprintf(fp, "    ");
          }
          fprintf(fp, " [%d]", idx);
          if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_DURATION) {
            fprintf(fp, ", duration: %uHz", pBoxTrun->pSamples[idx].sample_duration);
          }
          if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_SIZE) {
            fprintf(fp, ", size: %u (%u)", pBoxTrun->pSamples[idx].sample_size, offset);
            offset += pBoxTrun->pSamples[idx].sample_size;
          } else {
            //TODO: offset should come from tfhd default_sample_size

          }
          if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_FLAGS) {
            fprintf(fp, ", flags: 0x%x", pBoxTrun->pSamples[idx].sample_flags);
          }
          if(pBoxTrun->flags & TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET) {
            //fprintf(fp, ", comp time: 0x%x", pBoxTrun->pSamples[idx].sample_composition_time_offset);
            fprintf(fp, ", comp time: %dHz", (int32_t) pBoxTrun->pSamples[idx].sample_composition_time_offset);
          }
        }
        fprintf(fp, " }\n");
      }
    }


  }

}

static void free_box_trun(BOX_T *pBox) {
  BOX_TRUN_T *pBoxTrun = (BOX_TRUN_T *) pBox;

  if(pBoxTrun->pSamples) {
    free(pBoxTrun->pSamples);
  }
}

static BOX_T *read_box_sidx(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  BOX_SIDX_T *pBoxSidx = NULL;
  unsigned int idx = 20;
  unsigned int pos = 0;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < idx + 4) {
    LOG(X_ERROR("Invalid sidx body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, idx)) < 0) {
    return NULL;
  }

  pBoxSidx = (BOX_SIDX_T *) avc_calloc(1, sizeof(BOX_SIDX_T));
  memcpy(&pBoxSidx->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBoxSidx->fullbox, arena, pos += 4);

  pBoxSidx->reference_id = READ_UINT32(arena, pos);
  pBoxSidx->timescale = READ_UINT32(arena, pos);

  if(pBoxSidx->fullbox.version == 0) {
    pBoxSidx->earliest_presentation_time = READ_UINT32(arena, pos);
    pBoxSidx->first_offset = READ_UINT32(arena, pos);
  } else if(len < idx + 8 + 4) {
    LOG(X_ERROR("Invalid sidx version: %d body length: %u"), pBoxSidx->fullbox.version, len);
    return NULL;
  } else {

    if((rc = pStream->cbRead(pStream, &arena[idx], 8)) < 0) {
      return NULL;
    }
    idx += 8;

    pBoxSidx->earliest_presentation_time = READ_UINT64(arena, pos);
    pBoxSidx->first_offset = READ_UINT64(arena, pos);
  }
  if((rc = pStream->cbRead(pStream, &arena[idx], 4)) < 0) {
    return NULL;
  }
  idx += 4;

  pBoxSidx->reserved = READ_UINT16(arena, pos); 
  pBoxSidx->reference_count = READ_UINT16(arena, pos);

#define MP4_SIDX_MAX_REFERENCE_COUNT   4096

  if(pBoxSidx->reference_count * sizeof(BOX_SIDX_ENTRY_T) > len - pos) {
    LOG(X_ERROR("Invalid sidx reference count: %d"), pBoxSidx->reference_count);
    *prc = -1;
    return (BOX_T *) pBoxSidx;
  } else if(pBoxSidx->reference_count > MP4_SIDX_MAX_REFERENCE_COUNT) {
    LOG(X_ERROR("Unsupported sidx reference count: %d > "), pBoxSidx->reference_count, 
                MP4_SIDX_MAX_REFERENCE_COUNT);
    *prc = -1;
    return (BOX_T *) pBoxSidx;
  } else if((pBoxSidx->pSamples = (BOX_SIDX_ENTRY_T*) avc_calloc(pBoxSidx->reference_count,
                                                               sizeof(BOX_SIDX_ENTRY_T))) == NULL) {
    LOG(X_CRITICAL("Failed to allocate %d bytes for sidx references block"), pBoxSidx->reference_count);
    *prc = -1;
    return (BOX_T *) pBoxSidx;
  }

  for(idx = 0; idx < pBoxSidx->reference_count; idx++) {

    pos = 0;
    if((rc = pStream->cbRead(pStream, arena, 12)) < 0) {
      return NULL;
    }

    pBoxSidx->pSamples[idx].referenced_size = READ_UINT32(arena, pos);
    pBoxSidx->pSamples[idx].subsegment_duration = READ_UINT32(arena, pos);
    pBoxSidx->pSamples[idx].sap = READ_UINT32(arena, pos);

  }

  return (BOX_T *) pBoxSidx;
}

uint64_t htonl64(uint64_t ui64) {
  return HTONL64(ui64);
}

static int write_box_sidx(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {

  BOX_SIDX_T *pBoxSidx = (BOX_SIDX_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;
  unsigned int numWritten = 0;
  unsigned int idx;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxSidx->fullbox.version, 4);
  pos += 4;

  WRITE_UINT32(arena, pos, htonl(pBoxSidx->reference_id));
  WRITE_UINT32(arena, pos, htonl(pBoxSidx->timescale));
  if(pBoxSidx->fullbox.version == 0) {
    WRITE_UINT32(arena, pos, htonl((uint32_t) pBoxSidx->earliest_presentation_time));
    WRITE_UINT32(arena, pos, htonl((uint32_t) pBoxSidx->first_offset));
  } else {
    WRITE_UINT64(arena, pos, htonl64(pBoxSidx->earliest_presentation_time));
    WRITE_UINT64(arena, pos, htonl64(pBoxSidx->first_offset));
  }
  WRITE_UINT16(arena, pos, 0);
  WRITE_UINT16(arena, pos, htons(pBoxSidx->reference_count));

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }
  numWritten += pos;
  pos = 0;

  for(idx = 0; idx < pBoxSidx->reference_count; idx++) {

    if(pos >= sz_arena - sizeof(uint32_t)) {
      if(WriteFileStream(pStreamOut, arena, pos) < 0) {
        return -1;
      }
      numWritten += pos;
      pos = 0;
    }
    WRITE_UINT32(arena, pos, htonl(pBoxSidx->pSamples[idx].referenced_size));
    WRITE_UINT32(arena, pos, htonl(pBoxSidx->pSamples[idx].subsegment_duration));
    WRITE_UINT32(arena, pos, htonl(pBoxSidx->pSamples[idx].sap));
  }


  numWritten += pos;

  if(numWritten != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("sidx box length mismatch.  write: %d, sz: %d"),
                numWritten, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  if(pos > 0) {
    if(WriteFileStream(pStreamOut, arena, pos) < 0) {
      return -1;
    }
  }

  return rc;
}

static void dump_box_sidx(BOX_T *pBox, int context, int verbose, FILE *fp) {
  unsigned int idx;
  int ctxt = context;
  BOX_SIDX_T *pBoxSidx = (BOX_SIDX_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {
    ctxt = context;
    PRINT_INDENT(fp, ctxt);

    fprintf(fp, "   { ver: %d, refId: %u, timescale: %uHz", pBoxSidx->fullbox.version,
                                              pBoxSidx->reference_id, pBoxSidx->timescale);
    if(pBoxSidx->fullbox.version == 0) {
      fprintf(fp, ", earliest: 0x%x, (%uHz) first-offset: 0x%x",
                   (uint32_t) pBoxSidx->earliest_presentation_time, 
                    (uint32_t) pBoxSidx->earliest_presentation_time, (uint32_t) pBoxSidx->first_offset);
    } else {
      fprintf(fp, ", earliest: 0x%llx (%lluHz), first-offset: 0x%llx",
                   pBoxSidx->earliest_presentation_time, 
                   (pBoxSidx->timescale > 0 ? (pBoxSidx->earliest_presentation_time/pBoxSidx->timescale) : 0),
                   pBoxSidx->first_offset);
/*
char *p = (char*) &(pBoxSidx->earliest_presentation_time);
int pos=0;
//uint64_t x64= READ_UINT64(p, pos);
uint64_t x64= pBoxSidx->earliest_presentation_time;
uint64_t x64b= pBoxSidx->earliest_presentation_time - SEC_BETWEEN_EPOCH_1904_1970;
fprintf(fp, "sec: 0x%llx %llu, ----  0x%llx %llu    --- 0x%llx %llu", x64,x64,  SEC_BETWEEN_EPOCH_1904_1970,SEC_BETWEEN_EPOCH_1904_1970, x64b, x64b);
fprintf(fp, "-%s-", getPrintableTm(pBoxSidx->earliest_presentation_time));
//fprintf(fp, "-%s-", getPrintableTm(x64));
*/
    }
    fprintf(fp, ", ref-count: %d", pBoxSidx->reference_count);

    if(pBoxSidx->pSamples) {
      for(idx = 0; idx < pBoxSidx->reference_count; idx++) {

        fprintf(fp, "\n");
        ctxt = context;
        PRINT_INDENT(fp, ctxt);
        fprintf(fp, "     { type:%u, size: %u, duration: %uHz"
                    ", starts-with-SAP:%d, SAP-type: %d, SAP-delta-tm:0x%x }",
          (pBoxSidx->pSamples[idx].referenced_size & SIDX_MASK_REF_TYPE) >> 31,
          (pBoxSidx->pSamples[idx].referenced_size & SIDX_MASK_REF_SIZE),
          pBoxSidx->pSamples[idx].subsegment_duration,
          (pBoxSidx->pSamples[idx].sap & SIDX_MASK_STARTS_WITH_SAP) >> 31,
          (pBoxSidx->pSamples[idx].sap & SIDX_MASK_SAP_TYPE) >> 28,
          (pBoxSidx->pSamples[idx].sap & SIDX_MASK_SAP_DELTA_TIME));

      }
    }

    fprintf(fp, " }\n");

  }

}

static void free_box_sidx(BOX_T *pBox) {
  BOX_SIDX_T *pBoxSidx = (BOX_SIDX_T *) pBox;

  if(pBoxSidx->pSamples) {
    avc_free((void **) &pBoxSidx->pSamples);
  }

}

static BOX_T *read_box_tfdt(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  int rc;
  BOX_TFDT_T *pBoxTfdt = NULL;
  unsigned int idx = 8;
  unsigned int pos = 0;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < idx) {
    LOG(X_ERROR("Invalid tfdt body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, idx)) < 0) {
    return NULL;
  }

  pBoxTfdt = (BOX_TFDT_T *) avc_calloc(1, sizeof(BOX_TFDT_T));
  memcpy(&pBoxTfdt->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBoxTfdt->fullbox, arena, pos += 4);


  if(pBoxTfdt->fullbox.version == 0) {
    pBoxTfdt->baseMediaDecodeTime = READ_UINT32(arena, pos);
  } else {

    if((rc = pStream->cbRead(pStream, &arena[idx], 4)) < 0) {
      return NULL;
    }
    idx += 4;

    pBoxTfdt->baseMediaDecodeTime = READ_UINT64(arena, pos);
  }

  return (BOX_T *) pBoxTfdt;
}

static int write_box_tfdt(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {

  BOX_TFDT_T *pBoxTfdt = (BOX_TFDT_T *) pBox;  
  int rc = 0;
  unsigned int pos = 0;
  unsigned int numWritten = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ? 
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  memcpy(&arena[pos], &pBoxTfdt->fullbox.version, 4);
  pos += 4;

  if(pBoxTfdt->fullbox.version == 0) {
    WRITE_UINT32(arena, pos, htonl((uint32_t) pBoxTfdt->baseMediaDecodeTime));
  } else {
    WRITE_UINT64(arena, pos, htonl64(pBoxTfdt->baseMediaDecodeTime));
  }

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }
  numWritten += pos;
  pos = 0;

  if(numWritten != pBox->szhdr + pBox->szdata_ownbox) {
    LOG(X_ERROR("sidx box length mismatch.  write: %d, sz: %d"),
                numWritten, pBox->szhdr + pBox->szdata_ownbox);
    return -1;
  }

  return rc;
}

static void dump_box_tfdt(BOX_T *pBox, int context, int verbose, FILE *fp) {
  int ctxt = context;
  BOX_TFDT_T *pBoxTfdt = (BOX_TFDT_T *) pBox;

  dump_box(pBox, context, fp);
  if(verbose) {
    ctxt = context;
    PRINT_INDENT(fp, ctxt);

    fprintf(fp, "   { ver: %d", pBoxTfdt->fullbox.version);
    if(pBoxTfdt->fullbox.version == 0) {
      fprintf(fp, ", base media decode time: 0x%x,", (uint32_t) pBoxTfdt->baseMediaDecodeTime);
    } else {
      fprintf(fp, ", base media decode time: 0x%llx,", pBoxTfdt->baseMediaDecodeTime);
    }
    fprintf(fp, " }\n");

  }

}

static BOX_T *read_box_free(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  BOX_T *pBox = (BOX_T *) avc_calloc(1, sizeof(BOX_T));
  memcpy(pBox, pBoxHdr, sizeof(BOX_T));

  if(pStream->cbSeek(pStream, (long) pBox->szdata, SEEK_CUR) < 0) {
    *prc = -1;
    return (BOX_T *) pBox;
  }

  return pBox;
}

static BOX_T *read_box_nmhd(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  unsigned int pos = 0;
  BOX_T *pBox = NULL;
  int rc = 0;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len != 4) {
    LOG(X_ERROR("Invalid nmhd body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, len)) < 0) {  
    return NULL;
  }

  pBox = (BOX_T *) avc_calloc(1, sizeof(BOX_T));
  memcpy(pBox, pBoxHdr, sizeof(BOX_T));
  pos += 4; // skip version

  return pBox;
}

static BOX_T *read_box_tref(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  unsigned int pos = 0;
  BOX_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 4) {
    LOG(X_ERROR("Invalid tref body length: %u"), len);
    return NULL;
  }

  pBox = (BOX_T *) avc_calloc(1, sizeof(BOX_T));
  memcpy(pBox, pBoxHdr, sizeof(BOX_T));
  pos += (unsigned int) pBox->szdata;

  if(pStream->cbSeek(pStream, (long) pBox->szdata, SEEK_CUR) < 0) {
    *prc = -1;
    return (BOX_T *) pBox;
  }

  return pBox;
}

static BOX_T *read_box_edts(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  unsigned int pos = 0;
  BOX_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 4) {
    LOG(X_ERROR("Invalid edts body length: %u"), len);
    return NULL;
  }

  pBox = (BOX_T *) avc_calloc(1, sizeof(BOX_T));
  memcpy(pBox, pBoxHdr, sizeof(BOX_T));
  pos += (unsigned int) pBox->szdata;

  if(pStream->cbSeek(pStream, (long) pBox->szdata, SEEK_CUR) < 0) {
    *prc = -1;
    return (BOX_T *) pBox;
  }

  return pBox;
}

/*
static BOX_T *read_box_meta(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  unsigned int pos = 0;
  int rc;
  BOX_GENERIC_T *pBox = NULL;
  uint32_t len = (uint32_t) pBoxHdr->szdata;

  if(len < 4) {
    LOG(X_ERROR("Invalid meta body length: %u"), len);
    return NULL;
  }

  if((rc = pStream->cbRead(pStream, arena, 4)) < 0) {  
    return NULL;
  }

  pBox = (BOX_GENERIC_T *) avc_calloc(1, sizeof(BOX_GENERIC_T));
  memcpy(&pBox->box, pBoxHdr, sizeof(BOX_T));
  memcpy(&pBox->fullbox, arena, pos += 4);
  pos += pBox->box.szdata;

  pBox->box.subs = 1;
  pBox->box.szdata_ownbox = pos;

  return (BOX_T *) pBox;
}
*/

static BOX_T *read_box_unsupported(MP4_FILE_STREAM_T *pStream, BOX_T *pBoxHdr,
                            unsigned char *arena, unsigned int sz_arena,
                            int *prc) {
  BOX_T *pBox = (BOX_T *) avc_calloc(1, sizeof(BOX_T));
  memcpy(pBox, pBoxHdr, sizeof(BOX_T));

  //LOG(X_WARNING("Unsupported box type %s"), pBox->name);

  if(pStream->cbSeek(pStream, (long) pBox->szdata, SEEK_CUR) < 0) {
    *prc = -1;
    return (BOX_T *) pBox;
  }

  return pBox;
}

/*
static int write_box_unsupported(FILE_STREAM_T *pStreamOut, BOX_T *pBox,
                          unsigned char *arena, unsigned int sz_arena, void *pUserData) {
  int rc = 0;
  unsigned int pos = 0;

  memset(arena, 0, pBox->szhdr + pBox->szdata_ownbox > sz_arena ?
                   sz_arena : pBox->szhdr + pBox->szdata_ownbox);
  pos = mp4_write_box_header(pBox, arena, sz_arena);

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  return rc;
}
*/




static BOX_HANDLER_T *createBoxHandler(uint32_t type, 
                                       char *description,
                                       READ_FUNC readfunc,
                                       WRITE_FUNC writefunc,
                                       DUMP_FUNC dumpfunc,
                                       FREE_FUNC freefunc,
                                       BOX_HANDLER_T *parent) {

  BOX_HANDLER_T *pLastChild = NULL;
  BOX_HANDLER_T *pHandler = (BOX_HANDLER_T *) avc_calloc(1, sizeof(BOX_HANDLER_T));

  pHandler->type = type;
  strncpy(pHandler->description, description, sizeof(pHandler->description) - 1);
  pHandler->readfunc = readfunc;
  pHandler->writefunc = writefunc;
  pHandler->dumpfunc = dumpfunc;
  pHandler->freefunc = freefunc;

  if(parent) {
    if((pLastChild = parent->pchild)) {
      while(pLastChild->pnext) {
        pLastChild = pLastChild->pnext;
      }
      pLastChild->pnext = pHandler;
    } else {
      parent->pchild = pHandler;
    }
  }

  return pHandler;
}

static BOX_HANDLER_T *createDefaultHandlerChain() {

  BOX_HANDLER_T *plast[10];

  plast[0] = createBoxHandler(0, "", NULL, NULL, NULL, NULL, NULL);
  plast[1] = createBoxHandler(*((uint32_t *) "ftyp"), "File Type", 
                read_box_ftyp, write_box_ftyp, dump_box_ftyp, free_box_ftyp, plast[0]);
  plast[1] = createBoxHandler(*((uint32_t *) "styp"), "Segment Type", 
                read_box_ftyp, write_box_ftyp, dump_box_ftyp, free_box_ftyp, plast[0]);
  plast[1] = createBoxHandler(*((uint32_t *) "mdat"), "Media Data", 
                read_box_mdat, NULL, NULL, NULL, plast[0]);
  plast[1] = createBoxHandler(*((uint32_t *) "free"), "Free Space", 
                read_box_free, NULL, NULL, NULL, plast[0]);
  plast[1] = createBoxHandler(*((uint32_t *) "skip"), "Skip Space", 
                read_box_free, NULL, NULL, NULL, plast[0]);
  plast[1] = createBoxHandler(*((uint32_t *) "mfra"), "Track Fragment Random Access", 
                read_box_unsupported, NULL, NULL, NULL, plast[0]);
  plast[1] = createBoxHandler(*((uint32_t *) "moov"), "Movie", 
                read_box_genericsubs, NULL, NULL, NULL, plast[0]);
    plast[2] = createBoxHandler(*((uint32_t *) "iods"), "Initial Object Descriptor", 
                  read_box_iods, write_box_iods, dump_box_iods, NULL, plast[1]);
    plast[2] = createBoxHandler(*((uint32_t *) "mvhd"), "Movie Header", 
                  read_box_mvhd, write_box_mvhd, dump_box_mvhd, NULL, plast[1]);
    plast[2] = createBoxHandler(*((uint32_t *) "udta"), "User Data", 
                  read_box_unsupported, NULL, NULL, NULL, plast[1]);
    plast[2] = createBoxHandler(*((uint32_t *) "drm "), "Digital Rights Mgmt", 
                  read_box_unsupported, NULL, NULL, NULL, plast[1]);
//      plast[3] = createBoxHandler(*((uint32_t *) "meta"), "Meta Data", 
//                    read_box_meta, NULL, NULL, NULL, plast[2]);
//        plast[4] = createBoxHandler(*((uint32_t *) "hdlr"), "Handler Reference", 
//                      read_box_hdlr, write_box_hdlr, dump_box_hdlr, NULL, plast[3]);
    plast[2] = createBoxHandler(*((uint32_t *) "trak"), "Track", 
                  read_box_genericsubs, NULL, NULL, NULL, plast[1]);
      plast[3] = createBoxHandler(*((uint32_t *) "tkhd"), "Track Header", 
                    read_box_tkhd, write_box_tkhd, dump_box_tkhd, NULL, plast[2]);
      plast[3] = createBoxHandler(*((uint32_t *) "edts"), "Edit List Container", 
                    read_box_edts, NULL, NULL, NULL, plast[2]);
      plast[3] = createBoxHandler(*((uint32_t *) "tref"), "Track Reference Container", 
                    read_box_tref, NULL, NULL, NULL, plast[2]);
      plast[3] = createBoxHandler(*((uint32_t *) "udta"), "User Data", 
                    read_box_unsupported, NULL, NULL, NULL, plast[2]);
      plast[3] = createBoxHandler(*((uint32_t *) "mdia"), "Media", 
                    read_box_genericsubs, NULL, NULL, NULL, plast[2]);
        plast[4] = createBoxHandler(*((uint32_t *) "mdhd"), "Media Header", 
                      read_box_mdhd, write_box_mdhd, dump_box_mdhd, NULL, plast[3]);
        plast[4] = createBoxHandler(*((uint32_t *) "hdlr"), "Handler Reference", 
                      read_box_hdlr, write_box_hdlr, dump_box_hdlr, NULL, plast[3]);
        plast[4] = createBoxHandler(*((uint32_t *) "minf"), "Media Information", 
                      read_box_genericsubs, NULL, NULL, NULL, plast[3]);
          plast[5] = createBoxHandler(*((uint32_t *) "nmhd"), "Null Media Header", 
                        read_box_nmhd, NULL, NULL, NULL, plast[4]);
          plast[5] = createBoxHandler(*((uint32_t *) "smhd"), "Sound Media Header", 
                        read_box_smhd, write_box_smhd, dump_box_smhd, NULL, plast[4]);
          plast[5] = createBoxHandler(*((uint32_t *) "vmhd"), "Video Media Header", 
                        read_box_vmhd, write_box_vmhd, dump_box_vmhd, NULL, plast[4]);
          plast[5] = createBoxHandler(*((uint32_t *) "hmhd"), "Hint Media Header", 
                        read_box_unsupported, NULL, NULL, NULL, plast[4]);
          plast[5] = createBoxHandler(*((uint32_t *) "dinf"), "Data Information", 
                        read_box_genericsubs, NULL, NULL, NULL, plast[4]);
            plast[6] = createBoxHandler(*((uint32_t *) "dref"), "Data Reference", 
                          read_box_dref, write_box_dref, NULL, NULL, plast[5]);
              plast[7] = createBoxHandler(*((uint32_t *) "url "), "", 
                  read_box_dref_url, write_box_dref_url, dump_box_dref_url, NULL, plast[6]);

          plast[5] = createBoxHandler(*((uint32_t *) "hdlr"), "Handler Reference",
                      read_box_hdlr, write_box_hdlr, dump_box_hdlr, NULL, plast[4]);
            plast[6] = createBoxHandler(*((uint32_t *) "dref"), "Data Reference", 
                          read_box_dref, write_box_dref, NULL, NULL, plast[5]);
              plast[7] = createBoxHandler(*((uint32_t *) "url "), "", 
                  read_box_dref_url, write_box_dref_url, dump_box_dref_url, NULL, plast[6]);

          plast[5] = createBoxHandler(*((uint32_t *) "stbl"), "Sample Table", 
                        read_box_genericsubs, NULL, NULL, NULL, plast[4]);
            plast[6] = createBoxHandler(*((uint32_t *) "stsd"), "Sample Description", 
                read_box_stsd, write_box_stsd, dump_box_stsd, NULL, plast[5]);
              plast[7] = createBoxHandler(*((uint32_t *) "mp4v"), "", 
                  read_box_mp4v, write_box_mp4v, dump_box_mp4v, NULL, plast[6]);
                plast[8] = createBoxHandler(*((uint32_t *) "esds"), "", 
                    read_box_esds, write_box_esds, dump_box_esds, NULL, plast[7]);
              plast[7] = createBoxHandler(*((uint32_t *) "mp4a"), "", 
                  read_box_mp4a, write_box_mp4a, dump_box_mp4a, NULL, plast[6]);
                plast[8] = createBoxHandler(*((uint32_t *) "esds"), "", 
                    read_box_esds, write_box_esds, dump_box_esds, NULL, plast[7]);
                plast[8] = createBoxHandler(*((uint32_t *) "chan"), "", 
                    read_box_chan, NULL, NULL, NULL, plast[7]);
                plast[8] = createBoxHandler(*((uint32_t *) "wave"), "", 
                    read_box_wave, NULL, NULL, NULL, plast[7]);
                  plast[7] = createBoxHandler(*((uint32_t *) "frma"), "", 
                    read_box_frma, NULL, NULL, NULL, plast[8]);
                  plast[7] = createBoxHandler(*((uint32_t *) "mp4a"), "", 
                    read_box_frma, NULL, NULL, NULL, plast[8]);
                  plast[7] = createBoxHandler(*((uint32_t *) "esds"), "", 
                      read_box_esds, write_box_esds, dump_box_esds, NULL, plast[8]);

              plast[7] = createBoxHandler(*((uint32_t *) "avc1"), "", 
                  read_box_avc1, write_box_avc1, dump_box_avc1, NULL, plast[6]);
                plast[8] = createBoxHandler(*((uint32_t *) "stts"), "Decoding Time To Sample",
                  read_box_stts, write_box_stts, dump_box_stts, free_box_stts, plast[7]);
                plast[8] = createBoxHandler(*((uint32_t *) "avcC"), "", 
                    read_box_avcC, write_box_avcC, dump_box_avcC, free_box_avcC, plast[7]);
                plast[8] = createBoxHandler(*((uint32_t *) "btrt"), "",
                    read_box_btrt, write_box_btrt, dump_box_btrt, NULL, plast[7]);
              plast[7] = createBoxHandler(*((uint32_t *) "samr"), "", 
                  read_box_samr, write_box_samr, dump_box_samr, NULL, plast[6]);
                plast[8] = createBoxHandler(*((uint32_t *) "damr"), "", 
                    read_box_damr, write_box_damr, dump_box_damr, NULL, plast[7]);
                plast[8] = createBoxHandler(*((uint32_t *) "btrt"), "", 
                    read_box_btrt, write_box_btrt, dump_box_btrt, NULL, plast[7]);
              plast[7] = createBoxHandler(*((uint32_t *) "mp4s"), "",
                  read_box_unsupported, NULL, NULL, NULL, plast[6]); 
              plast[7] = createBoxHandler(*((uint32_t *) "rtp "), "RTP Hints",
                  read_box_unsupported, NULL, NULL, NULL, plast[6]); 
            plast[6] = createBoxHandler(*((uint32_t *) "stts"), "Decoding Time To Sample", 
                read_box_stts, write_box_stts, dump_box_stts, free_box_stts, plast[5]);
            plast[6] = createBoxHandler(*((uint32_t *) "ctts"), "Composition Time To Sample", 
                read_box_ctts, write_box_ctts, dump_box_ctts, free_box_ctts, plast[5]);
            plast[6] = createBoxHandler(*((uint32_t *) "stsc"), "Sample To Chunk", 
                read_box_stsc, write_box_stsc, dump_box_stsc, free_box_stsc, plast[5]);
            plast[6] = createBoxHandler(*((uint32_t *) "stco"), "Chunk Offset", 
                read_box_stco, write_box_stco, dump_box_stco, free_box_stco, plast[5]);
            plast[6] = createBoxHandler(*((uint32_t *) "stss"), "Sync Sample", 
                read_box_stss, write_box_stss, dump_box_stss, free_box_stss, plast[5]);
            plast[6] = createBoxHandler(*((uint32_t *) "stsz"), "Sample Sizes Box", 
                read_box_stsz, write_box_stsz, dump_box_stsz, free_box_stsz, plast[5]);
    plast[2] = createBoxHandler(*((uint32_t *) "mvex"), "Movie Extends", 
                  read_box_genericsubs, NULL, NULL, NULL, plast[1]);
      plast[3] = createBoxHandler(*((uint32_t *) "mehd"), "Movie Extends", 
                    read_box_mehd, write_box_mehd, dump_box_mehd, NULL, plast[2]);
      plast[3] = createBoxHandler(*((uint32_t *) "trex"), "Track Extends", 
                    read_box_trex, write_box_trex, dump_box_trex, NULL, plast[2]);
  plast[1] = createBoxHandler(*((uint32_t *) "moof"), "Movie Fragment", 
                read_box_genericsubs, NULL, NULL, NULL, plast[0]);
    plast[2] = createBoxHandler(*((uint32_t *) "mfhd"), "Movie Fragment Header", 
                  read_box_mfhd, write_box_mfhd, dump_box_mfhd, NULL, plast[1]);
    plast[2] = createBoxHandler(*((uint32_t *) "traf"), "Track Fragment", 
                    read_box_genericsubs, NULL, NULL, NULL, plast[1]);
      plast[3] = createBoxHandler(*((uint32_t *) "tfhd"), "Track Fragment Header", 
                    read_box_tfhd, write_box_tfhd, dump_box_tfhd, NULL, plast[2]);
      plast[3] = createBoxHandler(*((uint32_t *) "tfdt"), "Track Fragment Decode Time", 
                    read_box_tfdt, write_box_tfdt, dump_box_tfdt, NULL, plast[2]);
      plast[3] = createBoxHandler(*((uint32_t *) "trun"), "Track Fragment Run", 
                    read_box_trun, write_box_trun, dump_box_trun, free_box_trun, plast[2]);
      plast[3] = createBoxHandler(*((uint32_t *) "sdtp"), "Independent and Disposable", 
                    read_box_unsupported, NULL, NULL, NULL, plast[2]);
  plast[1] = createBoxHandler(*((uint32_t *) "sidx"), "Segment Index Box", 
                read_box_sidx, write_box_sidx, dump_box_sidx, free_box_sidx, plast[0]);

  return plast[0];
}

MP4_CONTAINER_T *mp4_createContainer() {
  MP4_CONTAINER_T *pMp4 = NULL;

  if((pMp4 = (MP4_CONTAINER_T *) avc_calloc(1, sizeof(BOX_T))) == NULL) {
    return NULL;
  }
  if((pMp4->proot = (BOX_T *) avc_calloc(1, sizeof(BOX_T))) == NULL) {
    free(pMp4);
    return NULL;
  }
  pMp4->proot->handler = createDefaultHandlerChain();

  return pMp4;
}
