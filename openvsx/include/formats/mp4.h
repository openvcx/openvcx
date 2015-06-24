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


#ifndef __MP4_H__
#define __MP4_H__

#include <stdio.h>
#include "mp4boxes.h"

int mp4_write(MP4_CONTAINER_T *pMp4, const char *path, void *pUserData);
int mp4_write_boxes(FILE_STREAM_T *pStreamOut, MP4_FILE_STREAM_T *pStreamIn, BOX_T *pBoxList,
                    unsigned char *arena, const unsigned int sz_arena, void *pUserData);
unsigned int mp4_write_box_header(BOX_T *pBox, unsigned char *buf, int sz);
int mp4_read_box_hdr(BOX_T *pBox, MP4_FILE_STREAM_T *pStream);
int mp4_read_boxes(MP4_FILE_STREAM_T *pStream, BOX_T *pParent,
                   unsigned char *arena, unsigned int sz_arena);
MP4_CONTAINER_T *mp4_open(const char *path, int validate);
void mp4_freeBoxes(BOX_T *pBox);
void mp4_close(MP4_CONTAINER_T *pMp4);
void mp4_dump(MP4_CONTAINER_T *pMp4, int verbose, FILE *fp);
void mp4_dump_boxes(BOX_T *pBox, int verbose, FILE *fp);
int mp4_deleteTrack(MP4_CONTAINER_T *pMp4, BOX_T *pTrak);
void mp4_free_avsync_stts(BOX_STTS_ENTRY_LIST_T *pList);

BOX_T *mp4_findBoxChild(BOX_T *pBox, uint32_t type);
BOX_T *mp4_findBoxNext(BOX_T *pBox, uint32_t type);
BOX_T *mp4_findBoxParentInTree(BOX_T *pBox, uint32_t type);
BOX_T *mp4_findBoxInTree(BOX_T *pBox, uint32_t type);

const BOX_HANDLER_T *mp4_findHandlerInChain(const BOX_HANDLER_T *pHandlerChain, 
                                            uint32_t type);

long long mp4_updateBoxSizes(BOX_T *pBox);
double mp4_getDurationMvhd(MP4_CONTAINER_T *pMp4);
uint32_t mp4_getSttsSampleDelta(BOX_STTS_ENTRY_LIST_T *pStts);

#endif // __MP4_H__
