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

#ifndef __WAV_H__
#define __WAV_H__

typedef struct WAV_FILE {
  // The following variables must be set prior to calling wav_open
  char              *path;
  unsigned int       channels;
  unsigned int       sampleRate;

  // The following variables are for internal house-keeping
  int                fd;
  int                wroteHdr;
  unsigned int       numSamples;
  unsigned int       samplesSinceLastUpdate;
} WAV_FILE_T;

int wav_open(WAV_FILE_T *pWav);
int wav_close(WAV_FILE_T *pWav);
int wav_write(WAV_FILE_T *pWav, const int16_t *pSamples, unsigned int numSamples);

#endif // __WAV_H__
