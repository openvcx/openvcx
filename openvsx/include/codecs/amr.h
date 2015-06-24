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


#ifndef __AMR_H__
#define __AMR_H__

#include "vsxconfig.h"
#include "bits.h"
#include "fileutil.h"
#include "formats/mp4track.h"
#include "formats/mp4.h"

extern const uint8_t AMRNB_SIZES[16];
#define AMR_GETFRAMETYPE(ch)                  ((((uint8_t)(ch)) >> 3) & 0x0f)
#define AMRNB_GETFRAMEBODYSIZE_FROMTYPE(frmtype) (AMRNB_SIZES[(frmtype)])    
#define AMRNB_GETFRAMEBODYSIZE(ch)  AMRNB_GETFRAMEBODYSIZE_FROMTYPE( \
                                               AMR_GETFRAMETYPE(ch) )

#define AMRNB_CLOCKHZ               8000
#define AMRNB_SAMPLE_DURATION_HZ     160

#define AMRNB_TAG                  "#!AMR\n"
#define AMRNB_TAG_LEN              6



typedef struct AMR_CTXT {
  unsigned int             idxBuf;
  uint8_t                  frmtype; 
} AMR_CTXT_T;

typedef struct AMR_DESCR {
  unsigned int              clockHz;
  unsigned int              frameDurationHz;
  FILE_STREAM_T            *pStreamIn; 
  AMR_CTXT_T                ctxt;
  uint32_t                  totFrames;
  MP4_MDAT_CONTENT_T        content; 
  uint32_t                  lastSampleDurationHz;
  uint32_t                  lastFrameId;
  uint64_t                  lastPlayOffsetHz;

  MP4_EXTRACT_STATE_T      *pMp4Ctxt; 
  unsigned int              damrFramesPerSample;
  unsigned int              idxFrameInSample;
} AMR_DESCR_T;


int amr_dump(FILE_STREAM_T *pFile, int verbosity);
int amr_isvalidFile(FILE_STREAM_T *pFile);

int amr_getSamples(AMR_DESCR_T *pAmr);
int amr_createSampleListFromMp4Direct(AMR_DESCR_T *pAmr, MP4_CONTAINER_T *pMp4,
                                      float fStartSec);
MP4_MDAT_CONTENT_NODE_T *amr_cbGetNextSample(void *pArg, int idx, int advanceFrame);
void amr_free(AMR_DESCR_T *pAmr);


#endif // __AMR_H__
