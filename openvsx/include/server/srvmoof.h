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


#ifndef __MOOF_SRV_H__
#define __MOOF_SRV_H__

#include "formats/mp4_moof.h"
#include "formats/mpd.h"

enum MOOFSRV_STATE {
  MOOFSRV_STATE_ERROR               = -1,
  MOOFSRV_STATE_INVALID             = 0,
};

typedef struct MOOFSRV_CTXT_T {
  int                      do_moof_segmentor;
  MP4_CREATE_STATE_MOOF_T  createCtxt;
  OUTFMT_CFG_T            *pRecordOutFmt;
  enum MOOFSRV_STATE       state;
  CODEC_AV_CTXT_T          av;
  int                     *pnovid;
  int                     *pnoaud;
  unsigned int             requestOutIdx; // xcode outidx
  DASH_INIT_CTXT_T         dashInitCtxt;
  MOOF_INIT_CTXT_T         moofInitCtxt;

} MOOFSRV_CTXT_T;

int moofsrv_record_start(MOOFSRV_CTXT_T *pMoofCtxt, STREAMER_OUTFMT_T *pLiveFmt);
int moofsrv_record_stop(MOOFSRV_CTXT_T *pMoofCtxt);
int moofsrv_addFrame(void *, const OUTFMT_FRAME_DATA_T *pFrame);
int moofsrv_init(MOOFSRV_CTXT_T *pMoof, unsigned int vidTmpFrameSz);
int moofsrv_close(MOOFSRV_CTXT_T *pMoof);


#endif // __MOOF_SRV_H__
