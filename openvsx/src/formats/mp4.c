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

extern void dump_box(BOX_T *pBox, int context, FILE *fp);
//extern unsigned int write_box_header(BOX_T *pBox, unsigned char *buf, int sz);

//int mp4_cbReadDataFile(void *, void *pData, unsigned int len);
//int mp4_cbSeekDataFile(void * , FILE_OFFSET_T offset, int whence);
//int mp4_cbCloseDataFile(void *);
//char *mp4_cbGetNameDataFile(void *);
//int mp4_cbCheckFdDataFile(void *);



static int write_box_generic(FILE_STREAM_T *pStreamOut, MP4_FILE_STREAM_T *pStreamIn, BOX_T *pBox,
                             unsigned char *arena, unsigned int sz_arena, void *pUserData) {

  int rc = 0;
  unsigned int pos = 0;
  unsigned int lenbody;
  int lenread;

  pos = mp4_write_box_header(pBox, arena, sz_arena);

  if(WriteFileStream(pStreamOut, arena, pos) < 0) {
    return -1;
  }

  if(pBox->child) {
    if(pBox->szdata_ownbox == pBox->szdata) {
      return rc;
    }
    lenbody = pBox->szdata_ownbox;
  } else {
    lenbody = (uint32_t) pBox->szdata;
  }

  if(lenbody > 0 && pStreamIn == NULL) {
    LOG(X_ERROR("Cannot write box type %c%c%c%c size because no input stream given"), 
          MP4_BOX_NAME_INT2C(pBox->type));
          //((unsigned char *)&pBox->type)[0], ((unsigned char *)&pBox->type)[1], 
          //((unsigned char *)&pBox->type)[2], ((unsigned char *)&pBox->type)[3]);
    return -1;
  } 
  
  //if(lenbody > 0 && SeekMediaFile(pStreamIn, pBox->fileOffsetData, SEEK_SET) != 0) {
  if(lenbody > 0 && pStreamIn->cbSeek(pStreamIn, pBox->fileOffsetData, SEEK_SET) != 0) {
    return -1;
  }

  while(lenbody > 0) {
    lenread = lenbody > sz_arena ? sz_arena : lenbody;
    if(pStreamIn->cbRead(pStreamIn, arena, lenread) < 0) {
      return -1;
    }
    
    if(WriteFileStream(pStreamOut, arena, lenread) < 0) {
      return -1;
    }

    lenbody -= lenread;
  }

  return rc;
}

int mp4_read_box_hdr(BOX_T *pBox, MP4_FILE_STREAM_T *pStream) {

  uint64_t size = 0;
  uint32_t ui[3];
  struct {
    uint32_t size;
    uint32_t type;
  } box;

  memset(pBox, 0, sizeof(BOX_T));
  pBox->fileOffset = pStream->offset;

  if(pStream->cbRead(pStream, &box, 8) < 0) {
    return -1;
  }

  box.size = ntohl(box.size);

  if(box.size == 1) {
    if(pStream->cbRead(pStream, ui, 8) < 0) {  
      return -1;
    }
    ui[2] = htonl(ui[0]);
    ui[0] = htonl(ui[1]);
    ui[1] = ui[2];
    memcpy(&size, ui, 8);
    
    LOG(X_DEBUG("Read box %c%c%c%c largesize(8) %lld 0x%llx"), 
                 MP4_BOX_NAME_INT2C(box.type),
     //((unsigned char *)&box.type)[0],((unsigned char *)&box.type)[1],
     //((unsigned char *)&box.type)[2],((unsigned char *)&box.type)[3],
     size, size);

  } else if(box.size == 0) {

    if(box.type != 0) {
      size = box.size = ntohl(box.type);
      if(pStream->cbRead(pStream, &box.type, 4) < 0) {  
        return -1;
      }
      pBox->fileOffset += 4;
    } else {
      size = (pStream->size - pStream->offset) + 8;
    }


  } else {
    size = box.size;
  }

  pBox->szhdr = 8;
  pBox->type = box.type;
  memcpy(pBox->name, &box.type, 4);
  
  if(box.type == *((uint32_t *) "uuid")) {
    if(pStream->cbRead(pStream, pBox->usertype, 16) < 0) {  
      return -1;
    }
    pBox->szhdr += 16;
  }

  // pBox->szhdr is not the size of the header on disk but rather the size
  // as it will be written to disk.  A box can be read which uses the 64 bit
  // size but may be written with a 32 bit size.
  if(size > 0xffffffe7) {
    pBox->szhdr += 8;
  }

//fprintf(stderr, "SIZE: %lld file:0x%x %c%c%c%c\n", size, pBox->fileOffset, ((unsigned char *)&box.type)[0],((unsigned char *)&box.type)[1],((unsigned char *)&box.type)[2],((unsigned char *)&box.type)[3]);

  pBox->szdata = size - (pStream->offset - pBox->fileOffset);
  pBox->szdata_ownbox = (uint32_t) pBox->szdata;
  pBox->fileOffsetData = pStream->offset;

  return 0;
}


const BOX_HANDLER_T *mp4_findHandlerInChain(const BOX_HANDLER_T *pHandler, uint32_t type) {
  const BOX_HANDLER_T *pHandlerFound = NULL;
  while(pHandler) {

    if(pHandler->type == type) {
      return pHandler;
    }
    if(pHandler->pchild) {
      if((pHandlerFound = mp4_findHandlerInChain(pHandler->pchild, type))) {
        return pHandlerFound;
      }
    }

    pHandler = pHandler->pnext;
  }

  return pHandlerFound;
}

static BOX_T *mp4_read_box(MP4_FILE_STREAM_T *pStream, const BOX_HANDLER_T *pHandlers,
                    unsigned char *arena, unsigned int sz_arena, int *prc) {

  uint32_t len;
  FILE_OFFSET_T prevFileOffset;
  BOX_T box;
  BOX_T *pBox = NULL;
  int rc;
  const BOX_HANDLER_T *pHandler = pHandlers;
  
  if(mp4_read_box_hdr(&box, pStream) != 0) {
    return NULL;
  }
  //fprintf(stderr, "GOT BOX HDR '%s', szhdr:%d, szdata:%llu, szdata_own:%d, subs:%d\n", box.name, box.szhdr, box.szdata, box.szdata_ownbox, box.subs);

  //
  // Invoke the notify type callback to tell the reader what type of box is about to be consumed
  //
  if(pStream->cbNotifyType && (rc = pStream->cbNotifyType(pStream, MP4_NOTIFY_TYPE_READHDR, &box)) < 0) {
    if(prc) {
      *prc = rc;
    }
    return NULL;
  }

  while(pHandler) {

    if(box.type == pHandler->type) {
      prevFileOffset = pStream->offset;
      //fprintf(stderr, "found handler '%s' box.name:'%s', offset:%llu\n", pHandler->description, box.name, pStream->offset);

      rc = 0;
      if((pBox = pHandler->readfunc(pStream, &box, arena, sz_arena, &rc))) {
        pBox->handler = (BOX_HANDLER_T *) pHandler;
      }
      if(prc && rc != 0) {
        *prc = rc;
      }
//fprintf(stderr, "DONE wi: %c%c%c%c file:0x%llx, prev:0x%llx box.szdata:%lld subs:%d\n", ((unsigned char *)&box.type)[0],((unsigned char *)&box.type)[1],((unsigned char *)&box.type)[2],((unsigned char *)&box.type)[3], pStream->offset, prevFileOffset, box.szdata, pBox->subs);

      if(pBox == NULL ||
         rc != 0 ||
        (!pBox->subs && pStream->offset != prevFileOffset + box.szdata)) {

        //fprintf(stderr, "pBox %s offset:0x%x (0x%x + %u)=0x%x\n", pBox ? pBox->name :"", pStream->offset, prevFileOffset, box.szdata, prevFileOffset+box.szdata);
        if(pStream->offset != prevFileOffset + box.szdata) {

          if(pStream->cbSeek(pStream, prevFileOffset + (FILE_OFFSET_T) box.szdata, SEEK_SET) < 0) {
            if(prc) {
              *prc = -1;
            }
          }
        }

      }

      //
      // Invoke the notify type callback to tell the reader what type of box is about to be consumed
      //
      if(pStream->cbNotifyType && (rc = pStream->cbNotifyType(pStream, MP4_NOTIFY_TYPE_READBOX, &box)) < 0) {
        if(prc) {
          *prc = rc;
        }
        return NULL;
      }

      return pBox;
    }
    //fprintf(stderr, "tried handler '%s'\n", pHandler->description);
    pHandler = pHandler->pnext;
  }

  LOG(X_WARNING("Unrecognized box type %c%c%c%c size 0x%x (%d) at offset 0x%llx in %s"), 
              MP4_BOX_NAME_INT2C(box.type),
              //((unsigned char *)&box.type)[0], ((unsigned char *)&box.type)[1], 
              //((unsigned char *)&box.type)[2], ((unsigned char *)&box.type)[3],
              (uint32_t) box.szdata + box.szhdr, (uint32_t) box.szdata + box.szhdr, 
              box.fileOffset, pStream->cbGetName(pStream));

  len = (uint32_t) box.szdata;

  //while(len > 0) {
  //  if((rc = ReadFileStream(pStream, arena, (len > sz_arena ? sz_arena : len))) < 0) {  
    if(pStream->cbSeek(pStream, len, SEEK_CUR) != 0) {
      return NULL;
    }
  //  len -= rc;
  //}

  pBox = (BOX_T *) avc_calloc(1, sizeof(BOX_T));
  memcpy(pBox, &box, sizeof(BOX_T));

  return pBox;
}

static void read_boxes(MP4_FILE_STREAM_T *pStream, BOX_T *pParent,
                       unsigned char *arena, unsigned int sz_arena,
                       int *prc, int nesting) {

  int rc = 0;
  BOX_T *pLastChild = NULL;
  BOX_T *pBox = NULL;
  const BOX_HANDLER_T *pHandlers = ((BOX_HANDLER_T *) pParent->handler)->pchild;

  //fprintf(stderr,"mp4 input stream offset %llu , size:%llu, %llu, %llu\n", pStream->offset, pStream->size, pParent->fileOffsetData, pParent->szdata);

  //if(pStream->size > 0 && pStream->offset >= pParent->fileOffsetData + pParent->szdata) {
  if((pStream->size > 0 || nesting > 0) && pStream->offset >= pParent->fileOffsetData + pParent->szdata) {
    LOG(X_ERROR("mp4 input stream offset %llu >= %llu, file size: %llu"), 
                pStream->offset, pParent->fileOffsetData + pParent->szdata, pStream->size);
    return;
  }

  if((pLastChild = pParent->child)) {
    while(pLastChild->pnext) {
      pLastChild = pLastChild->pnext;
    }
  }

  while((pBox = mp4_read_box(pStream, pHandlers, arena, sz_arena, prc)) != NULL) {


    if(pLastChild) {
      pLastChild->pnext = pBox;
    } else {
      pParent->child = pBox;
    }
    pLastChild = pBox;

    if(pBox->subs) {
      read_boxes(pStream, pBox, arena, sz_arena, prc, nesting + 1);
    }
    //fprintf(stderr, "back from children of %s, parent: %s, offset:%llu, %llu (%llu + %llu), subs:%d, nesting:%d\n", pBox->name, pParent->name, pStream->offset, pParent->fileOffsetData + pParent->szdata, pParent->fileOffsetData, pParent->szdata, pBox->subs, nesting);

    //
    // Invoke the notify type callback to tell the reader what type of box is about to be consumed
    //
    if(pStream->cbNotifyType && (rc = pStream->cbNotifyType(pStream, MP4_NOTIFY_TYPE_READBOXCHILDREN, pBox)) < 0) {
      if(prc) {
        *prc = rc;
      }
    }

    if((pStream->size > 0 || nesting > 0)&& pStream->offset >= pParent->fileOffsetData + pParent->szdata) {
    //if(pStream->size > 0 && pStream->offset >= pParent->fileOffsetData + pParent->szdata) {
    //if(pStream->offset >= pParent->fileOffsetData + pParent->szdata) {
      //fprintf(stderr,"mp4 read boxes breaking... '%s' offset %llu , size:%llu, %llu, %llu\n", pBox->name,pStream->offset, pStream->size, pParent->fileOffsetData, pParent->szdata);
      break;
    }
  }

}

int mp4_read_boxes(MP4_FILE_STREAM_T *pStream, BOX_T *pParent,
                   unsigned char *arena, unsigned int sz_arena) {
  int rc = 0;
  int nesting = 0;

  read_boxes(pStream, pParent, arena, sz_arena, &rc, nesting);

  return rc;
}

int mp4_write_boxes(FILE_STREAM_T *pStreamOut, MP4_FILE_STREAM_T *pStreamIn, BOX_T *pBoxList,
                    unsigned char *arena, const unsigned int sz_arena, void *pUserData) {

  int rc = 0;
  BOX_T *pBox = pBoxList;

  if(!pStreamOut || !arena) {
    return -1;
  }

  while(pBox) {

    if(pBox->handler != NULL &&
      ((BOX_HANDLER_T *)pBox->handler)->writefunc) {

      ((BOX_HANDLER_T *)pBox->handler)->writefunc(pStreamOut, pBox, arena, sz_arena, pUserData);
    } else {
      rc = write_box_generic(pStreamOut, pStreamIn, pBox, arena, sz_arena, pUserData);        
    }

    if(rc < 0) {
      return rc;
    }

    if(pBox->child) {
      if((rc = mp4_write_boxes(pStreamOut, pStreamIn, pBox->child, arena, sz_arena, pUserData)) < 0) {
        break;
      }
    }

    pBox = pBox->pnext;
  }


  return rc;
} 

long long mp4_updateBoxSizes(BOX_T *pBox) {
  
  long long size = 0;
  long long rc;

  while(pBox) {

    if(pBox->child) {
      rc = mp4_updateBoxSizes(pBox->child);
      pBox->szdata = pBox->szdata_ownbox + rc;
    } 

    size += pBox->szhdr + pBox->szdata;
    

    pBox = pBox->pnext;
  }

  return size;
}


void mp4_freeBoxes(BOX_T *pBox) {
  BOX_T *pNext;

  while(pBox) {

    if(pBox->child) {
      mp4_freeBoxes(pBox->child);
      pBox->child = NULL;
    }

    pNext = pBox->pnext;
    if(pBox->handler && ((BOX_HANDLER_T *)pBox->handler)->freefunc != NULL) {
      ((BOX_HANDLER_T *)pBox->handler)->freefunc(pBox);
    } 

    free(pBox);

    pBox = pNext;
  }
}

static void mp4_freeHandlers(BOX_HANDLER_T *pHandler) {
  BOX_HANDLER_T *pNext;

  while(pHandler) {

    if(pHandler->pchild) {
      mp4_freeHandlers(pHandler->pchild);
      pHandler->pchild = NULL;
    }

    pNext = pHandler->pnext;
    free(pHandler);

    pHandler = pNext;
  }
}

void mp4_close(MP4_CONTAINER_T *pMp4) {
  BOX_HANDLER_T *pHandlers = NULL;

  if(pMp4) {

    pHandlers = (BOX_HANDLER_T *) pMp4->proot->handler;

    mp4_freeBoxes(pMp4->proot);

    if(pHandlers) {
      mp4_freeHandlers(pHandlers);
    }

    if(pMp4->pStream) {
      //fprintf(stderr, "CLOSING MP4 CONTAINER '%s'\n", pMp4->pStream->cbGetName(pMp4->pStream));
      pMp4->pStream->cbClose(pMp4->pStream);
      avc_free((void **) &pMp4->pStream);
    }

    if(pMp4->pNextLoader) {
      avc_free((void **) &pMp4->pNextLoader);
    }

    free(pMp4);
  }
}

static BOX_T *findBox(BOX_T *pBox, uint32_t type) {
  BOX_T *pBoxFound = NULL;

  while(pBox) {

    if(pBox->type == type) {
      return pBox; 
    }

    if(pBox->child) {
      if((pBoxFound = findBox(pBox->child, type))) {
        return pBoxFound;
      }
    }

    pBox = pBox->pnext;
  }

  return pBoxFound;
}

static BOX_T *findBoxParent(BOX_T *pBox, uint32_t type, int level, int *plevelFound) {
  BOX_T *pBoxFound = NULL;

  while(pBox) {

    if(level > 0 && pBox->type == type) {
      *plevelFound = level;
      return pBox;
    }

    if(pBox->child) {
      if((pBoxFound = findBoxParent(pBox->child, type, level + 1, plevelFound))) {
        if(level + 1 == *plevelFound) {
          pBoxFound = pBox;
        }
        break;
      }       
    }

    pBox = pBox->pnext;
  }

  return pBoxFound;
}

BOX_T *mp4_findBoxParentInTree(BOX_T *pParent, uint32_t type) {
  int level = 0;
  return findBoxParent(pParent, type, 0, &level);
}

BOX_T *mp4_findBoxInTree(BOX_T *pParent, uint32_t type) {
  if(pParent->child) {
    return findBox(pParent->child, type);
  }
  return NULL;
}

BOX_T *mp4_findBoxChild(BOX_T *pBox, uint32_t type) {
  if(pBox->child) {
    pBox = pBox->child;
    while(pBox) {

      if(pBox->type == type) {
        return pBox;
      }

      pBox = pBox->pnext;
    }
  }
  return NULL;
}

BOX_T *mp4_findBoxNext(BOX_T *pBox, uint32_t type) {

  while(pBox) {

    if((pBox = pBox->pnext) == NULL) {
      return NULL;
    }

    if(pBox->type == type) {
      return pBox;
    }
  }
  return NULL;
}

int mp4_deleteTrack(MP4_CONTAINER_T *pMp4, BOX_T *pTrak) {
  BOX_T *pBox = NULL;
  int rc = -1;

  if(pTrak && (pBox = mp4_findBoxChild(pMp4->proot, *((uint32_t *) "moov")))) {

    if(pBox->child == pTrak) {
      pBox->child = pBox->child->pnext;
      rc = 0;
    } else {
      pBox = pBox->child;
      while(pBox != NULL) {
        if(pBox->pnext == pTrak) {
          pBox->pnext = pBox->pnext->pnext;
          pTrak->pnext = NULL;
          rc = 0;
          break;
        }
        pBox = pBox->pnext;
      }
    }

  }

  if(rc == 0) {
    mp4_freeBoxes(pTrak); 
  }

  return rc;
}

#if defined(VSX_DUMP_CONTAINER)

static void dump_boxes(BOX_T *pBox, int context, int verbose, FILE *fp) {

  while(pBox) {

    if(pBox->handler != NULL  &&
         ((BOX_HANDLER_T *)pBox->handler)->dumpfunc != NULL) {
        ((BOX_HANDLER_T *)pBox->handler)->dumpfunc(pBox, context, verbose, fp);
    } else {
      dump_box(pBox, context, fp);
    }

    if(pBox->child) {
      dump_boxes(pBox->child, context+1, verbose, fp);
    }

    pBox = pBox->pnext;
  }

}

void mp4_dump_boxes(BOX_T *pBox, int verbose, FILE *fp) {
  dump_boxes(pBox, 0, verbose, fp);
}

void mp4_dump(MP4_CONTAINER_T *pMp4, int verbose, FILE *fp) {

  if(!pMp4 || !fp) {
    return;
  }

  mp4_updateBoxSizes(pMp4->proot);

  if(pMp4->pStream) {
    fprintf(fp, "filename: %s\n", pMp4->pStream->cbGetName(pMp4->pStream));
  }
  dump_boxes(pMp4->proot, 0, verbose, fp);
}
#endif // VSX_DUMP_CONTAINER

static int mp4_cbOpenDataFile(void *pArg, const char *path);

MP4_CONTAINER_T *mp4_open(const char *path, int validate) {
  MP4_FILE_STREAM_T *pStream = NULL;
  MP4_CONTAINER_T *pMp4 = NULL;
  BOX_FTYP_T *pBoxStyp = NULL;
  BOX_T *pBoxMoov = NULL;
  BOX_T *pBox;
  unsigned char arena[4096];
  int rc = 0;

  if(!path) {
    return NULL;
  }

  if(!(pStream = avc_calloc(1, sizeof(MP4_FILE_STREAM_T)))) {
    return NULL;
  }

  pStream->cbRead = mp4_cbReadDataFile;
  pStream->cbSeek = mp4_cbSeekDataFile;
  pStream->cbClose = mp4_cbCloseDataFile;
  pStream->cbCheckFd = mp4_cbCheckFdDataFile;
  pStream->cbGetName = mp4_cbGetNameDataFile;

  if(mp4_cbOpenDataFile(pStream, path) < 0) {
    free(pStream);
    return NULL;
  }

  if((pMp4 = mp4_createContainer()) == NULL) {
    pStream->cbClose(pStream);
    free(pStream);
    return NULL;
  }

  pMp4->pStream = pStream;
  pMp4->proot->szdata = pMp4->pStream->size;
  pMp4->proot->subs = 1;

  LOG(X_DEBUG("Reading %s"), pMp4->pStream->cbGetName(pMp4->pStream));

  rc = mp4_read_boxes(pMp4->pStream, pMp4->proot, arena, sizeof(arena));
  if(!validate) {
    return pMp4; 
  }

  if((pBoxMoov = mp4_findBoxChild(pMp4->proot, *((uint32_t *) "moov")))) {
    pMp4->haveMoov = 1;
  }

  if(rc != 0) {
    LOG(X_ERROR("Encountered errors when processing %s"), path);
  } else if(mp4_findBoxChild(pMp4->proot, *((uint32_t *) "ftyp")) == NULL &&
            (pBoxStyp = (BOX_FTYP_T *) mp4_findBoxChild(pMp4->proot, *((uint32_t *) "styp"))) == NULL) {
    LOG(X_ERROR("No ftyp box present"));
  } else if(!pBoxStyp && !pBoxMoov) {
    LOG(X_ERROR("No moov box present"));
  //} else if(mp4_findBoxChild(pMp4->proot, *((uint32_t *) "mdat")) == NULL) {
  //  LOG(X_ERROR("No mdat box present"));
  } else {

    //
    // Discover if this mp4 is an ISO BMFF (fragmented moof style container)
    //
    if((pBox = mp4_findBoxChild(pMp4->proot, *((uint32_t *) "mvex")))) {
      pMp4->pBoxTrex = (BOX_TREX_T *) mp4_findBoxChild(pBox, *((uint32_t *) "trex"));
    }
    if(mp4_findBoxChild(pMp4->proot, *((uint32_t *) "moof"))) {
        pMp4->haveMoof = 1;
    }

    return pMp4;
  }
  
  mp4_close(pMp4);

  return NULL;
}

int mp4_write(MP4_CONTAINER_T *pMp4, const char *path, void *pUserData) {
  FILE_STREAM_T fStreamOut;
  int rc = 0;
  unsigned char arena[4096];

  if(!pMp4 || !path) {
    return -1;
  } 

  //if(pMp4->pStream && pMp4->pStream->fp != FILEOPS_INVALID_FP) {
  if(pMp4->pStream && pMp4->pStream->cbCheckFd(pMp4->pStream)) {
    if(pMp4->pStream->cbSeek(pMp4->pStream, 0, SEEK_SET) != 0) {
    //if(SeekMediaFile(pMp4->pStream, 0, SEEK_SET) != 0) {
      return -1;
    }
  }

  if((fStreamOut.fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_CRITICAL("Unable to open file for writing: %s"), path);
    return -1;
  }
  fStreamOut.offset = 0;
  

  if(pMp4->proot) {

    mp4_updateBoxSizes(pMp4->proot);

    rc = mp4_write_boxes(&fStreamOut, pMp4->pStream, pMp4->proot->child, arena, 
                         sizeof(arena), pUserData);
  }

  fileops_Close(fStreamOut.fp);

  return rc;
}

double mp4_getDurationMvhd(MP4_CONTAINER_T *pMp4) {
  BOX_MVHD_T *pMvhd = NULL;
  double durationSec = 0;

  if(!pMp4) {
    return 0;
  }

  if((pMvhd = (BOX_MVHD_T *) mp4_findBoxInTree(pMp4->proot, *((uint32_t *) "mvhd"))) == NULL) {
    LOG(X_ERROR("Unable to find mvhd in %s"), pMp4->pStream ? pMp4->pStream->cbGetName(pMp4->pStream): "");
    return -1;
  }

  if(pMvhd->timescale > 0) {
    durationSec = (double)pMvhd->duration / pMvhd->timescale;
  }

  return durationSec;
}

uint32_t mp4_getSttsSampleDelta(BOX_STTS_ENTRY_LIST_T *pStts) {
  unsigned int idx;
  uint32_t minSampleDelta = 0;

  if(!pStts || !pStts->pEntries) {
    return 0;
  }

  if(pStts->entrycnt == 1) {
    return pStts->pEntries[0].sampledelta;
  }

  for(idx = 0; idx < pStts->entrycnt; idx++) {
    if(minSampleDelta == 0 || (pStts->pEntries[idx].sampledelta > 0 &&
                               pStts->pEntries[idx].sampledelta < minSampleDelta)) {
      minSampleDelta = pStts->pEntries[idx].sampledelta;
    }
  }
  return minSampleDelta;
}


int mp4_cbReadDataFile(void *pArg, void *pData, unsigned int len) {
  int rc = 0;
  MP4_FILE_STREAM_T *pMp4Fs = (MP4_FILE_STREAM_T *) pArg;
  FILE_STREAM_T *pFs = (FILE_STREAM_T *) pMp4Fs->pCbData;

  //fprintf(stderr, "cbReadDataFile len:%d %llu/%llu\n", len, pFs->offset, pFs->size);

  rc = ReadFileStream(pFs, pData, len);

  if(pFs) {
    pMp4Fs->offset = pFs->offset;
    //pMp4Fs->size = pFs->size;
  }

  return rc;
}

int mp4_cbSeekDataFile(void *pArg, FILE_OFFSET_T offset, int whence) {
  int rc = 0;
  MP4_FILE_STREAM_T *pMp4Fs = (MP4_FILE_STREAM_T *) pArg;
  FILE_STREAM_T *pFs = (FILE_STREAM_T *) pMp4Fs->pCbData;

  //fprintf(stderr, "cbSeekDataFile offset:%lld / %lld, whence:%d, pFs->offset:%llu '%s'\n", offset, pFs->size, whence, pFs->offset, pFs->filename);
  rc = SeekMediaFile(pFs, offset, whence);
  
  if(pFs) {
    pMp4Fs->offset = pFs->offset;
    //pMp4Fs->size = pFs->size;
  }

  return rc;
}

static int mp4_cbOpenDataFile(void *pArg, const char *path) {
  int rc = 0;
  MP4_FILE_STREAM_T *pMp4Fs = (MP4_FILE_STREAM_T *) pArg;

  if(!pMp4Fs->pCbData) {
    if(!(pMp4Fs->pCbData =  avc_calloc(1, sizeof(FILE_STREAM_T)))) {
      return -1;
    }
  }

  if((rc = OpenMediaReadOnly(pMp4Fs->pCbData, path)) < 0) {
    avc_free((void **) &pMp4Fs->pCbData);
  } else {
    pMp4Fs->offset = ((FILE_STREAM_T *) pMp4Fs->pCbData)->offset;
    pMp4Fs->size = ((FILE_STREAM_T *) pMp4Fs->pCbData)->size;
  }

  return rc;
}

int mp4_cbCloseDataFile(void *pArg) {
  int rc = 0;
  MP4_FILE_STREAM_T *pMp4Fs = (MP4_FILE_STREAM_T *) pArg;

  rc = CloseMediaFile(pMp4Fs->pCbData);

  avc_free((void **) &pMp4Fs->pCbData);

  return rc;
}

int mp4_cbCheckFdDataFile(void *pArg) {
  //int rc = 0;
  MP4_FILE_STREAM_T *pMp4Fs = (MP4_FILE_STREAM_T *) pArg;
  FILE_STREAM_T *pFs = (FILE_STREAM_T *) pMp4Fs->pCbData;

  if(pFs->fp != FILEOPS_INVALID_FP) {
    return 1;
  }

  return 0;
}

char *mp4_cbGetNameDataFile(void *pArg) {
  MP4_FILE_STREAM_T *pMp4Fs = (MP4_FILE_STREAM_T *) pArg;
  FILE_STREAM_T *pFs = (FILE_STREAM_T *) pMp4Fs->pCbData;

  if(!pFs) {
    return "";
  }

  return pFs->filename;
}

