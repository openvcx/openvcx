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


#ifndef __MP4_EXTRACTOR_H__
#define __MP4_EXTRACTOR_H__

#ifdef WIN32
#include "unixcompat.h"
#endif // WIN32

#include "mp4.h"
#include "mp4creator.h"

typedef int (*CB_READ_SAMPLE_FSMP4)(MP4_FILE_STREAM_T *, uint32_t, void*, int, uint32_t);
typedef int (*CB_READ_SAMPLE)(FILE_STREAM_T *, uint32_t, void*, int, uint32_t);

int mp4_readSamplesFromTrack(const MP4_TRAK_T *pMp4BoxSet, MP4_FILE_STREAM_T *pStreamIn, 
                            CB_READ_SAMPLE_FSMP4 sampleRdFunc, void *pCbData,
                            uint64_t startHz, uint64_t durationHz);
int mp4_readSampleFromTrack(MP4_EXTRACT_STATE_INT_T *pState,
                            MP4_MDAT_CONTENT_NODE_T *pContent,
                            uint32_t *pSampleDurationHz);
int mp4_getNextSyncSampleIdx(MP4_EXTRACT_STATE_INT_T *pState);

unsigned int mp4_getSampleCount(const MP4_TRAK_T *pMp4Trak);

MP4_EXTRACT_STATE_T *mp4_create_extract_state(const MP4_CONTAINER_T *pMp4, const MP4_TRAK_T *pTrak);


#if defined(VSX_EXTRACT_CONTAINER)

int mp4_extractRaw(const MP4_CONTAINER_T *pMp4, const char *outPrfx, 
                   float fStart, float fDuration,
                   int overwrite, int extractVid, int extractAud);

#endif // VSX_EXTRACT_CONTAINER


int mp4_initAvcCTrack(const MP4_CONTAINER_T *pMp4, MP4_TRAK_AVCC_T *pMp4BoxSet);
int mp4_initMp4vTrack(const MP4_CONTAINER_T *pMp4, MP4_TRAK_MP4V_T *pMp4BoxSet);
int mp4_initMp4aTrack(const MP4_CONTAINER_T *pMp4, MP4_TRAK_MP4A_T *pMp4BoxSet);
int mp4_initSamrTrack(const MP4_CONTAINER_T *pMp4, MP4_TRAK_SAMR_T *pMp4BoxSet);

BOX_T *mp4_loadTrack(BOX_T *pRoot, MP4_TRAK_T *pBoxes, uint32_t stsdType, int initMoov);

#endif // __MP4_EXTRACTOR_H__
