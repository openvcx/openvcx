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

#ifndef __AUDIO_PREPROC_H__
#define __AUDIO_PREPROC_H__

#include <pthread.h>
#include <netinet/in.h>

typedef struct AUDIO_PREPROC {
  int                          active;
  pthread_mutex_t              mtx;
  void                        *pctxt;           // internal preprocessor state context
  void                        *pctxt2;           
  int                          cfg_vad;         // VAD flag
  int16_t                      cfg_vad_prob_start; // speex specific vad config (deprecated)
  int16_t                      cfg_vad_prob_continue; // speex specific vad config (deprecated)
  int                          cfg_vad_mode;    // webrtc vad mode 
  int                          cfg_denoise;     // Denoise filter flag
  int                          cfg_agc;         // Acoustic Gain Control flag
  unsigned int                 clockHz;         // clock sample rate in Hz of the source channel
  unsigned int                 chunkHz;         // chunk duration in terms of sampling rate
  unsigned int                 bufferIdx;       // int16_t based index within buffer
  int16_t                     *buffer;          // samples buffer of 1 chunkHz duration
  u_int64_t                    tsHz;            // timestamp of first sample within the buffer
  unsigned char               *vad_buffer;      // buffer storing VAD value of each chunkHz duration
                                                // of samples relative to MIXER_SOURCE::samplesWrIdx
  unsigned int                 cfg_vad_persist; // VAD flag will be persisted up to this value
                                                // after the last VAD samples chunk was set (deprecated)
  unsigned int                 vad_persist;     // running counter of the current VAD persistance (deprecated)
                                                // value

  int                          cfg_vad_smooth;  // Enable vad smoothing
  float                        vad_smooth_avg;  // internal smoothed vad value
} AUDIO_PREPROC_T;


int audio_preproc_init(AUDIO_PREPROC_T *pPreproc,
                       unsigned int clockHz,
                       unsigned int samplesSz,
                       int vad,
                       int denoise,
                       int agc);

void audio_preproc_free(AUDIO_PREPROC_T *pPreproc);

int audio_preproc_exec(AUDIO_PREPROC_T *pPreproc, int16_t *samples, unsigned int sz);








#endif // __AUDIO_PREPROC_H__
