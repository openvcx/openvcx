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

#ifndef __MIXER_INT_H__
#define __MIXER_INT_H__

#include <stdio.h>
#include "logutil.h"
#include "mixer/mixer.h"


//
// Internal prototype for mixer samples add
//
typedef int (* FUNC_ADD_SAMPLES) (MIXER_SOURCE_T *, const int16_t *, unsigned int, unsigned int, \
             u_int64_t, int, int *, int);


int addbuffered(MIXER_SOURCE_T *pSource,
                const int16_t *pSamples,
                unsigned int numSamples,
                unsigned int channels,
                u_int64_t tsHz,
                int vad,
                int *pvad,
                int lock);

int addsamples(MIXER_SOURCE_T *pSource,
               const int16_t *pSamples,
               unsigned int numSamples,
               unsigned int channels,
               u_int64_t tsHz,
               int vad,
               int *pvad,
               int lock);



#endif // __MIXER_INT_H__
