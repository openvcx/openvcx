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

#ifndef __MIXER_H__
#define __MIXER_H__

#include <pthread.h>
#include <stdio.h>
#include "mixer/mixdefaults.h"
#include "logutil.h"
#include "mixer/audio_preproc.h"
#include "mixer/ringbuf.h"
#include "mixer/mixer_api.h"


#define MAX_MIXER_SOURCES     (MAX_PARTICIPANTS)

typedef int MIXER_ID;

typedef struct MIXER_OUTPUT {
  RING_BUF_T                   buf; 
  unsigned int                 vad_bufsz;
  unsigned char               *vad_buffer; 
  int                          priorVadIdx;
} MIXER_OUTPUT_T;

typedef struct MIXER_SOURCE {
  LOG_CTXT_T                  *logCtxt;
  struct MIXER                *pMixer;
  int                          active;
  int                          needInputReset;
  MIXER_ID                     id;
  int                          isbuffered;
  float                        gain;         // gain multiplier applied to this audio source
  u_int64_t                    lastWrittenTsHz;
  RING_BUF_T                   buf;
  unsigned int                 clockHz;      // clock sample rate in Hz
  int64_t                      clockOffset;  // ts offset applied to local clock to match master's clock
  int64_t                      discardedSamplesOffset;
  AUDIO_PREPROC_T              preproc;
  MIXER_OUTPUT_T              *pOutput; 
  int                          includeSelfChannel;
  //int                          setOutputVad;
  int                          acceptInputVad;
  int64_t                      thresholdHz;
} MIXER_SOURCE_T;


typedef struct MIXER {
  LOG_CTXT_T                  *logCtxt;
  unsigned int                 numSources;
  MIXER_SOURCE_T              *p_sources[MAX_MIXER_SOURCES];
  u_int64_t                    tsHz;         // timestamp of last output sample 
  u_int64_t                    tsHzPrior;    
  int64_t                      thresholdHz;  // threshold of Hz samples which can be missing before
                                             // empty samples are substituted
  int64_t                      idleSourceThresholdHz;
  unsigned int                 outChunkHz;   // output chunk size of the mixer
  pthread_mutex_t              mtx;
} MIXER_T;


/**
 *
 * mixer_source_init
 *
 * Creates an audio source instance which will be an input into the mixer.
 *
 * durationSec - the duration in seconds of the internal audio samples ring buffer.
 * gain - a multiplier of the input volume.  0 mutes the audio.  1.0 is default.
 * sampleRateHz - the sample rate of the input audio.
 *                Note: all input mixer streams should be at the same sample rate.
 * vad - set to non-zero to use a VAD preprocessor to detect speech and only mix
 *       audio during said occurrance.
 * vad_mode - webrtc vad mode (0 - quality, 1 - low bitrate, 2 - aggr, 3 - very aggressive
 * denoise - set to non-zero to enable denoise filter
 * agc - set to non-zero to enable acoustic gain control
 * includeSelfChannel - Set to 1 to include origin audio in the output audio
 * setOutputVad - Set to 1 to set VAD confidence value of output channel
 * acceptInputVad - Set to 1 to accept input VAD parameter, such as when obtained from
 *                  RTP meta-data
 *
 */
MIXER_SOURCE_T *mixer_source_init(LOG_CTXT_T *logCtxt,
                                  float durationSec,
                                  float gain,
                                  unsigned int sampleRateHz,
                                  int64_t thresholdHz,
                                  int vad,
                                  int vad_mode,
                                  int denoise,
                                  int agc,
                                  int haveOutput,
                                  int includeSelfChannel,
                                  //int setOutputVad,
                                  int acceptInputVad);

/**
 *
 * mixer_source_free
 *
 * Deallocates any audio source instance data of an input source. 
 *
 * pSource - the audio source returned from a call to mixer_source_init.
 *
 */
void mixer_source_free(MIXER_SOURCE_T *pSource);

/**
 *
 * mixer_init
 *
 * Creates a mixer instance used to mix multiple source channels.
 * The mixer will synchronize the input channels and may provide empty filler samples
 * for input channels that are missing source samples because they may be late in arrival.
 *
 * thresholdHz - the threshold in Hz (input samples) that any single mixer input source samples
 * can be late to prevent audio source clipping with white noise.
 * outChunkHz - the desired output chunk size in Hz of the mixer output.  Setting this value
 *              to non-zero will produce same sized output chunks. 
 *
 */
MIXER_T *mixer_init(LOG_CTXT_T *logCtxt,
                    unsigned int thresholdHz,
                    unsigned int outChunkHz);

/**
 *
 * mixer_free
 *
 * Deallocates any mixer instance data allocated with mixer_init
 *
 * pMixer - the  mixer instance returned from a call to mixer_init.
 *
 */
void mixer_free(MIXER_T *pMixer);


/**
 *
 * mixer_add
 *
 * Adds input samples for an input stream into the source buffer
 *
 * pSource - the mixer source instance.
 * pSamples - PCM 16 bit input samples.
 * numSamples - count of input samples.
 * channels - number of input channels.
 * tsHz - the timestamp in Hz of the first sample in pSamples.
 *
 */
int mixer_add(MIXER_SOURCE_T *pSource, 
              const int16_t *pSamples, 
              unsigned int numSamples, 
              unsigned int channels, 
              u_int64_t tsHz);

/**
 *
 * mixer_addbuffered
 *
 * Adds buffered input samples for an input stream into the mixer buffer.
 * The samples are buffered according to the preprocessor chunk duration
 * configuration.  If the preprocessor is not enabled, mixer_add is called
 * directly from this function.
 *
 * pSource - the mixer source instance.
 * pSamples - PCM 16 bit input samples.
 * numSamples - count of input samples.
 * channels - number of input channels.
 * tsHz - the timestamp in Hz of the first sample in pSamples.
 * vad - the VAD confidence value of the input samples
 *       VAD value can range from [ 0 ... 255 ], -1 if unavailable
 * pvad - optional output parameter used to the computed VAD value
 *        of the aggregate input samples.
 *
 *
 */
int mixer_addbuffered(MIXER_SOURCE_T *pSource,
                             const int16_t *pSamples,
                             unsigned int numSamples,
                             unsigned int channels,
                             u_int64_t tsHz,
                             int vad,
                             int *pvad);


/**
 *
 * mixer_addsource
 *
 * pMixer - the mixer instance
 * pSource - the mixer audio source to be added to the mixer
 *
 */
int mixer_addsource(MIXER_T *pMixer, 
                    MIXER_SOURCE_T *pSource);

/**
 *
 * mixer_removesource
 *
 * pMixer - the mixer instance
 * pSource - the mixer audio source to be removed from the mixer
 *
 */
int mixer_removesource(MIXER_T *pMixer, 
                       MIXER_SOURCE_T *pSource);


/**
 *
 * mixer_mix
 * Mixes any input channels. Mixer mix should be called on the master (driver) channel 
 * directly after the master channel adds its own audio source via mixer_add / mixer_addbuffered.
 *
 * Note:  If the mixer outChunkHz has been set to non-zero, mixer_mix can be called in a while loop
 * until the output while the output size > 0.
 *
 * @return - the number of output samples consumed by the mixer
 * pMixer - the mixer instance
 * tsHz - The timestamp in Hz of the next output sample.
 *
 */
int mixer_mix(MIXER_T *pMixer, 
              u_int64_t tsHz);

/**
 *
 * mixer_check_idle
 * Should be called before calling mixer_mix to de-activate any idle input.  Since the mixer
 * synchronizes all source channels, an input channel no longer being supplied with PCM
 * samples may delay mixer output.  This call should be called with the Hz value of a clock
 * respective of real-time.
 *
 * pMixer - the mixer instance
 * tsHz - the timestamp in Hz of the current mixer clock (real-time)
 *
 */
int mixer_check_idle(MIXER_T *pMixer,
                     u_int64_t tsHz);

/**
 *
 * mixer_count_active
 * Returns the count of active input channels being mixed.  An input channel can be internally
 * de-activated by the mixer if it has become idle.  The channel will be re-activated if 
 * PCM samples are provided via a call to mixer_addbuffered or mixer_add.
 *
 * pMixer - the mixer instance
 *
 */
int mixer_count_active(MIXER_T *pMixer);

int mixer_reset_sources(MIXER_T *pMixer);

/**
 *
 * mixer_dump
 * Dumps the internal state of the mixer and its source channel buffer states. 
 *
 * fp - The output file stream (such as 'stdout') 
 * pMixer - the mixer instance
 *
 */
void mixer_dump(FILE *fp, MIXER_T *pMixer, int lock, const char *descr);

void mixer_dumpLog(int logLevel, MIXER_T *pMixer, int lock, const char *descr);


#endif // __MIXER_H__
