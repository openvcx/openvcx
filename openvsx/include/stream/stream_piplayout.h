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

#ifndef __STREAM_PIPLAYOUT_H__
#define __STREAM_PIPLAYOUT_H__

enum PIP_UPDATE {
  PIP_UPDATE_UNKNOWN                  = 0,
  PIP_UPDATE_VAD                      = 1,
  PIP_UPDATE_PARTICIPANT_START        = 2,
  PIP_UPDATE_PARTICIPANT_END          = 3,
  PIP_UPDATE_PARTICIPANT_HAVEINPUT    = 4
};

typedef struct PIP_LAYOUT_MGR {

  struct STREAMER_PIP       *pPip;
} PIP_LAYOUT_MGR_T;

int pip_layout_update(PIP_LAYOUT_MGR_T *pLayout, enum PIP_UPDATE reason);
PIP_LAYOUT_TYPE_T pip_str2layout(const char *str);
const char *pip_layout2str(PIP_LAYOUT_TYPE_T layoutType);

#endif // __STREAM_PIPLAYOUT_H__
