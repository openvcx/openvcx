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


#include <unistd.h>
#include <stdlib.h>
#include <netinet/in.h>
#include <fcntl.h>
#include <stdio.h>
#include "logutil.h"
#include "mixer/wav.h"


static int wav_write_header(WAV_FILE_T *pWav) {

  unsigned int length = pWav->numSamples;
  int rc;
  unsigned char buffer[64];

  if(pWav->fd <= 0) {
    return -1;
  }

  *((uint32_t *) &buffer[0]) = *((uint32_t *) "RIFF");
  *((uint32_t *) &buffer[4]) = (length + 36);
  *((uint32_t *) &buffer[8]) = *((uint32_t *) "WAVE");
  *((uint32_t *) &buffer[12]) = *((uint32_t *) "fmt ");
  *((uint32_t *) &buffer[16]) = 16;                     // chunk size in bits
  *((uint16_t *) &buffer[20]) = 1;                      // PCM
  *((uint16_t *) &buffer[22]) = pWav->channels;               // channels
  *((uint32_t *) &buffer[24]) = (pWav->sampleRate);
  *((uint32_t *) &buffer[28]) = (pWav->sampleRate * pWav->channels * 2);
  *((uint16_t *) &buffer[32]) = (pWav->channels * 2);         // block align
  *((uint16_t *) &buffer[34]) = 16;                     // bits per sample
  *((uint32_t *) &buffer[36]) = *((uint32_t *) "data");
  *((uint32_t *) &buffer[40]) = length;

  if(!(rc = write(pWav->fd, buffer, 44)) < 0) {
    return rc;
  }

  pWav->samplesSinceLastUpdate = 0;

  return 44;
}


static int wav_update_header(WAV_FILE_T *pWav) {

  int rc;
  unsigned char buffer[4];
  off_t offset;
  unsigned int length = pWav->channels * pWav->numSamples * 2;

  if((offset = lseek(pWav->fd, 0, SEEK_CUR)) < 0) {
    return -1;
  }

  if(lseek(pWav->fd, 4, SEEK_SET) < 0) {
    return -1;
  }

  *((uint32_t *) buffer) = (length + 36);
  rc = write(pWav->fd, buffer, 4);

  if(lseek(pWav->fd, 40, SEEK_SET) < 0) {
    return -1;
  }

  *((uint32_t *) buffer) = (length);
  rc = write(pWav->fd, buffer, 4);


  if(lseek(pWav->fd, offset, SEEK_SET) < 0) {
    return -1;
  }

  pWav->samplesSinceLastUpdate = 0;

  //fprintf(stderr, "update_header done ok\n");

  return 0;
}

int wav_open(WAV_FILE_T *pWav) {
  int rc;

  if(!pWav || !pWav->path || pWav->sampleRate <= 0) {
    return -1;
  }

  if(!(pWav->fd = open(pWav->path, O_RDWR | O_CREAT, 0666)) < 0) {
    return -1;
  }

  rc = wav_write_header(pWav);

  return rc;
}


int wav_close(WAV_FILE_T *pWav) {

  wav_update_header(pWav);

  close(pWav->fd);
  pWav->fd = -1;

  return 0;
}

int wav_write(WAV_FILE_T *pWav, const int16_t *pSamples, unsigned int numSamples) {
  int rc;

  if(!pWav || !pSamples || numSamples <= 0) {
    return -1;
  }

  if(pWav->fd <= 0 && (rc = wav_open(pWav)) < 0) {
    return rc;
  }

  if((rc = write(pWav->fd, pSamples, 2 * numSamples)) < 0) {
    return rc;
  }

  pWav->numSamples += numSamples;
  pWav->samplesSinceLastUpdate += numSamples;

  if(pWav->samplesSinceLastUpdate > 4096) {
    rc = wav_update_header(pWav);
  }

  return rc;
}

