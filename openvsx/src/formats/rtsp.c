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


int rtsp_isrtsp(const unsigned char *pData, unsigned int len) {
  int rc = 0;

  if(!pData || len < 7) {
    return 0;
  }

  if(!memcmp(RTSP_REQ_OPTIONS, pData, strlen(RTSP_REQ_OPTIONS)) ||
     !memcmp(RTSP_REQ_DESCRIBE, pData, strlen(RTSP_REQ_DESCRIBE)) ||
     !memcmp("GET "VSX_RTSPLIVE_URL, pData, 4 + strlen(VSX_RTSPLIVE_URL)) ||
     !memcmp("POST "VSX_RTSPLIVE_URL, pData, 5 + strlen(VSX_RTSPLIVE_URL))) {
    rc = 1;
  }

  return rc;
}
