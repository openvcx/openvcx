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


#ifndef __CAPTURE_CBHANDLER_H__
#define __CAPTURE_CBHANDLER_H__

#include "capture.h"


typedef int (*CAPTURE_CB_STOREDATA) (void *, const void *, uint32_t);

typedef struct CAPTURE_STORE_CBDATA {
  void                   *pCbDataArg;
  CAPTURE_CB_STOREDATA    cbStoreData;
} CAPTURE_STORE_CBDATA_T;

int capture_cbWriteFile(void *pArg, const void *pData, uint32_t szData);
int capture_cbWriteBuf(void *pArg, const void *pData, uint32_t szData);
int capture_addCompleteFrameToQ(CAPTURE_CBDATA_SP_T *pSp, uint32_t ts);
void capture_initCbData(CAPTURE_CBDATA_T *pCbData, const char *outDir, int overWriteOutput);
void capture_deleteCbData(CAPTURE_CBDATA_T *pCbData);
int capture_cbOnStreamAdd(void *pCbUserData, 
                        CAPTURE_STREAM_T *pStream, 
                        const COLLECT_STREAM_HDR_T *pktHdr,
                        const char *filepath);

//int capture_cbOnStreamEnd(CAPTURE_STREAM_T *pStream);




#endif // __CAPTURE_CBHANDLER_H__
