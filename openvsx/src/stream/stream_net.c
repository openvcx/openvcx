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
#include "stream_net.h"

#if defined(VSX_HAVE_STREAMER)

enum STREAM_NET_ADVFR_RC stream_net_check_time(TIME_VAL *ptvStart, uint64_t *pframeId, unsigned int clockHz, 
                                               unsigned int frameDeltaHz, uint64_t *pPtsOut, int64_t *pPtsOffset) {
  TIME_VAL tvNow = timer_GetTime();
  double tvElapsed, tvEquiv;
  uint64_t us;

  if(*pframeId == 0) {
    *ptvStart = tvNow;
  } else if((tvElapsed = (double)(tvElapsed = (tvNow - *ptvStart))/1000) <
            (tvEquiv = (double)1000 * *pframeId * frameDeltaHz / clockHz)) {

    us = (tvEquiv - tvElapsed) * 1000;
    //fprintf(stderr, "streamnet_check_time notavail elapsed:%lldms, frameId:%lld, equiv of %.3f ms (%.3f, %.3f %llu us) %d/%dHz\n", (tvNow - *ptvStart)/1000,  *pframeId, (double)1000 * *pframeId / clockHz, tvElapsed, tvEquiv, us, clockHz, frameDeltaHz);

    if(us > 2000) {
      usleep(MIN(5000, us - 2000));
    }

    return STREAM_NET_ADVFR_RC_NOTAVAIL;
  }


  if(pPtsOut) {
    *pPtsOut = (uint64_t) 90000.0f * ((double) *pframeId * frameDeltaHz / clockHz);
    if(pPtsOffset) {
      if(*pPtsOffset < 0 && -1 * *pPtsOffset > *pPtsOut) {
        *pPtsOut = 0;
      } else {
        (*pPtsOut) += *pPtsOffset;
      }
    }
    //fprintf(stderr, "stream_net_check_time PTS:%.3f, frameId:%llu, clockHz:%uHz, deltaHz:%dHz, elapsed:%lldms, equiv of %.3fms %d/%dHz\n", PTSF(*pPtsOut), *pframeId, clockHz, frameDeltaHz, (tvNow - *ptvStart)/1000, (double) tvEquiv, clockHz, frameDeltaHz);
  }

  (*pframeId)++;

  return STREAM_NET_ADVFR_RC_OK;
}

#endif // (VSX_HAVE_STREAMER)
