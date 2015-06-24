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


int vid_convert_fps(const double fps, unsigned int *pClockHz, unsigned int *pFrameDeltaHz) {
 unsigned int num = 0;
  double f = fps;
  unsigned int numer;
  unsigned int denum;

  if(fps == 0.0) {
    return -1;
  } if(fps == 30.0) {
    *pClockHz = 90000;
    *pFrameDeltaHz = 3000;
  } else {

    if(f > 500) {
      f = 500;
      LOG(X_WARNING("Using max fps: %.0f"), f);
    }

    numer = (unsigned int) f;
    while((double) numer != f && num < 5) {
      f *= 10.0;
      numer = (unsigned int) f;
      num++;
    }

    denum = (unsigned int) pow(10, num);
    while(numer > 0 && denum > 0 &&
      (numer/10  * 10 == numer) && (denum/10 * 10 == denum)) {
      numer /= 10;
      denum /= 10;
    }

    *pClockHz = numer;
    *pFrameDeltaHz = denum;

  }

  return 0;
}
