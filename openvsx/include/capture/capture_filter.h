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


#ifndef __CAPTURE_FILTER_H__
#define __CAPTURE_FILTER_H__

#include <stdio.h>
#include "capture.h"
#include "util/fileutil.h"


int capture_parseFilter(const char *str, CAPTURE_FILTER_T *pFilter);
void capture_freeFilters(CAPTURE_FILTER_T pFilters[], unsigned int numFilters);
int capture_filterFromSdp(const char *sdppath,
                         CAPTURE_FILTER_T  pFilters[],
                         char *listenAddrOut, 
                         unsigned int listenAddrLen,
                         SDP_DESCR_T *pSdp,
                         int novid,
                         int noaud);

int capture_openOutputFile(FILE_STREAM_T *pFile,
                           int overWriteOutput,
                           const char *outDir);

int capture_createOutPath(char *buf, unsigned int sz,
                                    const char *dir,
                                    const char *outputFilePrfx,
                                    const COLLECT_STREAM_HDR_T *pPktHdr,
                                    enum CAPTURE_FILTER_PROTOCOL mediaType);


#endif // __CAPTURE_FILTER_H__
