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


#ifndef __AVCC_H__
#define __AVCC_H__

#include "vsxconfig.h"
#include "fileutil.h"


typedef struct AVC_DECODER_CFG_BLOB {
  uint16_t len;
  uint16_t pad;
  unsigned char *data;
} AVC_DECODER_CFG_BLOB_T;

typedef struct AVC_DECODER_CFG  {
  //
  //AVCDecoderConfigurationRecord
  //

  unsigned char configversion;  // 1
  unsigned char avcprofile;
  unsigned char profilecompatibility;
  unsigned char avclevel;
  unsigned char lenminusone;   // 6 bits reserved '111111'   , 
                               //2 bits lengthSizeMinusOne of startcode sequence
  unsigned char numsps;        // 3 bits '111', 5 bits value
  AVC_DECODER_CFG_BLOB_T *psps;
  unsigned char numpps; 
  AVC_DECODER_CFG_BLOB_T *ppps;

} AVC_DECODER_CFG_T;

typedef struct AVCC_CBDATA_WRITE_SAMPLE {
  FILE_STREAM_T *pStreamOut;
  unsigned int lenNalSzField;
  unsigned char arena[4096];
} AVCC_CBDATA_WRITE_SAMPLE_T;

#define AVCC_SPS_MAX_LEN         128 
#define AVCC_PPS_MAX_LEN         AVCC_SPS_MAX_LEN

typedef struct SPSPPS_RAW {
  const unsigned char    *sps;
  const unsigned char    *pps;
  unsigned int            sps_len;
  unsigned int            pps_len;
  unsigned char           sps_buf[AVCC_SPS_MAX_LEN];
  unsigned char           pps_buf[AVCC_PPS_MAX_LEN];
} SPSPPS_RAW_T;


int avcc_cbWriteSample(FILE_STREAM_T *pStreamIn, uint32_t sampleSize,
                       void *pCbData, int chunkIdx, uint32_t sampleDurationHz);


int avcc_initCfg(const unsigned char *buf, unsigned int len, AVC_DECODER_CFG_T *pCfg);
void avcc_freeCfg(AVC_DECODER_CFG_T *pCfg);
int avcc_writeNAL(FILE_STREAM_T *pStreamOut, unsigned char *pdata, unsigned int len);
int avcc_writeSPSPPS(FILE_STREAM_T *pStreamOut, const AVC_DECODER_CFG_T *pCfg,
                      uint32_t sampledelta, uint32_t timescale);
int avcc_create(unsigned char *out, unsigned int len, const SPSPPS_RAW_T *pspspps,
                unsigned int nalHdrSz);
int avcc_encodebase64(SPSPPS_RAW_T *pspspps, char *pOut, unsigned int lenOut);
  


#endif //__AVCC_H__
