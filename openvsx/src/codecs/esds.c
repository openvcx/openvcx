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

static void esds_writeobjtype(BIT_STREAM_T *pStream, int objTypeIdx) {
  if(objTypeIdx >= 0x1f) {
    bits_write(pStream, 0x1f, 5);
    bits_write(pStream, objTypeIdx - 0x1f, 6);
  } else {
    bits_write(pStream, objTypeIdx, 5);
  }
}

static void esds_writefreqidx(BIT_STREAM_T *pStream, int freqIdx) {
  if(freqIdx >= 0x0f) {
    bits_write(pStream, 0x0f, 4);
    bits_write(pStream, freqIdx - 0x0f, 24);
  } else {
    bits_write(pStream, freqIdx, 4);
  }
}

int esds_encodeDecoderSp(unsigned char *pBuf, unsigned int len,
                         const ESDS_DECODER_CFG_T *pSp) {
  BIT_STREAM_T stream;

  if(!pBuf || len < 2) {
    return -1;
  }

  memset(&stream, 0, sizeof(stream));
  memset(pBuf, 0, len > 8 ? 8 : len);
  stream.sz = len;
  stream.buf = pBuf;

  //0x12 0x10
  esds_writeobjtype(&stream, pSp->objTypeIdx);
  esds_writefreqidx(&stream, pSp->freqIdx);
  bits_write(&stream, pSp->channelIdx, 4);

  if(pSp->frameDeltaHz == 960) {
    bits_write(&stream, 1, 1);
  } else {
    bits_write(&stream, 0, 1);
  }

  bits_write(&stream, pSp->dependsOnCoreCoderFlag, 1);
  bits_write(&stream, pSp->extFlag, 1);

  if(pSp->haveExtTypeIdx) {
    if(len < 6) {
      return -1;
    }
    bits_write(&stream, 0x2b7, 11);
    esds_writeobjtype(&stream, pSp->extObjTypeIdx);
    if(pSp->extObjTypeIdx == 5 && pSp->haveExtFreqIdx) {
      bits_write(&stream, 1, 1);
      esds_writefreqidx(&stream, pSp->extFreqIdx);
    }
  }

  BIT_STREAM_ENDOF_BYTE((&stream));

  return stream.byteIdx;
}

static int esds_readobjtype(BIT_STREAM_T *pstream) {
  int val;
  if((val = bits_read(pstream, 5)) == 0x1f) {
    val = bits_read(pstream, 6) + 0x1f;
  }
  return val;
}

static int esds_readfreqidx(BIT_STREAM_T *pstream) {
  int val;
  if((val = bits_read(pstream, 4)) == 0x0f) {
    val = bits_read(pstream, 24) + 0x0f;
  }
  return val;
}

int esds_decodeDecoderSp(const unsigned char *p, const unsigned int len, ESDS_DECODER_CFG_T *pSp) {

  BIT_STREAM_T stream;
  int i;

  if(!p || len < 2 || !pSp) {
    return -1;
  }

  memset(&stream, 0, sizeof(stream));
  stream.buf = (unsigned char *) p;
  stream.sz = len;

  pSp->objTypeIdx = esds_readobjtype(&stream);
  pSp->freqIdx = esds_readfreqidx(&stream);
  pSp->channelIdx = bits_read(&stream, 4);
  if(bits_read(&stream, 1)) {
    pSp->frameDeltaHz = 960;
  } else {
    pSp->frameDeltaHz = 1024;
  }
  if((pSp->dependsOnCoreCoderFlag = bits_read(&stream, 1))) {
    bits_read(&stream, 14);  // coreCoderDelay
  }
  pSp->extFlag = bits_read(&stream, 1);
 //fprintf(stderr, "AAC OBJTYPEIDX:%d, channelIdx:%d, freq:%d\n", pSp->objTypeIdx, pSp->channelIdx, pSp->freqIdx);


  //fprintf(stderr, "aac %d.%d / %d\n", stream.byteIdx, stream.bitIdx, stream.sz);
  //
  // extension
  //
  if((stream.byteIdx * 8) + stream.bitIdx + 15 <= stream.sz * 8) {
    if((i = bits_read(&stream, 11)) == 0x2b7) {
      pSp->extObjTypeIdx = esds_readobjtype(&stream);
      pSp->haveExtTypeIdx = 1;
      if(pSp->extObjTypeIdx == 5 &&  bits_read(&stream, 1) == 1) {
        pSp->extFreqIdx = esds_readfreqidx(&stream);
        pSp->haveExtFreqIdx = 1;
      }
      //fprintf(stderr, "EXT OBJ TYPE:%d, freqIdx:%d\n", pSp->extObjTypeIdx, pSp->extFreqIdx);
    }
  }

  pSp->init = 1;

  return 0;
}


int esds_cbWriteSample(FILE_STREAM_T *pStreamIn, uint32_t sampleSize, 
                       void *pCbData, int chunkIdx, uint32_t sampleDurationHz) {

  ESDS_CBDATA_WRITE_SAMPLE_T *pCbDataEsds = NULL;
  unsigned int idxByteInSample = 0;
  unsigned int lenRead;
  int idx= 0;
  int rc = 0;

  //fprintf(stderr, "esds_cbWriteSample chunkIdx:%d sampleSize:%d sampleDurationHz:%d\n", chunkIdx, sampleSize, sampleDurationHz);

  if((pCbDataEsds = (ESDS_CBDATA_WRITE_SAMPLE_T *) pCbData) == NULL) {
    return -1;
  }

  if((idx= aac_encodeADTSHdr(pCbDataEsds->bs.buf, pCbDataEsds->bs.sz,
                 &pCbDataEsds->decoderCfg, sampleSize)) < 0) {
    LOG(X_ERROR("Failed to create ADTS header for frame"));
    return -1;
  }

  while(idxByteInSample < sampleSize) {

    if((lenRead = sampleSize - idxByteInSample) > pCbDataEsds->bs.sz - idx) {
      lenRead = pCbDataEsds->bs.sz - idx;
    }

    if(ReadFileStream(pStreamIn, &pCbDataEsds->bs.buf[idx], lenRead) < 0) {
      return -1;
    }

    if((rc = WriteFileStream(pCbDataEsds->pStreamOut, pCbDataEsds->bs.buf, 
                             lenRead + idx)) < 0) {
      return -1;
    }

    idx= 0;
    idxByteInSample += lenRead;

  }

  return rc;
}

const char *esds_getMp4aObjTypeStr(unsigned int objTypeIdx) {

  static const char *strObjType[] = {"Unknown",
                                     "AAC Main",
                                     "AAC LC (Low Complexity)",
                                      "AAC SSR (Scalable Sample Rate)",
                                      "AAC LTP (Long Term Prediction)",
                                      "SBR (Spectral Band Replication)",
                                      "AAC Scalable",
                                      "TwinVQ",
                                      "CELP (Code Excited Linear Prediction)",
                                      "HXVC (Harmonic Vector eXcitation Coding)",
                                      "Reserved",
                                      "Reserved",
                                      "TTSI (Text-To-Speech Interface)",
                                      "Main Synthesis",
                                      "Wavetable Synthesis",
                                      "General MIDI",
                                      "Algorithmic Synthesis and Audio Effects",
                                      "ER (Error Resilient) AAC LC",
                                      "Reserved",
                                      "ER AAC LTP",
                                      "ER AAC Scalable",
                                      "ER TwinVQ",
                                      "ER BSAC (Bit-Sliced Arithmetic Coding)",
                                      "ER AAC LD (Low Delay)",
                                      "ER CELP",
                                      "ER HVXC",
                                      "ER HILN (Harmonic and Individual Lines plus Noise)",
                                      "ER Parametric",
                                      "SSC (SinuSoidal Coding)",
                                      "PS (Parametric Stereo)",
                                      "MPEG Surround",
                                      "Invalid",
                                      "Layer-1",
                                      "Layer-2",
                                      "Layer-3",
                                      "DST (Direct Stream Transfer)",
                                      "ALS (Audio Lossless)",
                                      "SLS (Scalable LosslesS)",
                                      "SLS non-core",
                                      "ER AAC ELD (Enhanced Low Delay)",
                                      "SMR (Symbolic Music Representation) Simple",
                                      "SMR Main",
                                      "USAC (Unified Speech and Audio Coding) (no SBR)",
                                      "SAOC (Spatial Audio Object Coding)",
                                      "LD MPEG Surround",
                                      "USAC",
                                      "Invalid"
                                    };
  static const unsigned int szObjTypes = 46;

  if(objTypeIdx < szObjTypes) {
    return strObjType[objTypeIdx];
  } else {
    return strObjType[szObjTypes];
  }
}


const char *esds_getChannelStr(unsigned int channelIdx) {

  static const char *strChannel[] =   { "Defined in AOT Specifc Cfg ",
                                 "1 fc",
                                 "2 fl, fr",
                                 "3 fc, fl, fr",
                                 "4 fc, fl, fr, bc",
                                 "5 fc, fl, fr, bl, br",
                                 "6 fc, fl, fr, bl, br, LFE",
                                 "8 fc, fl, fr, sl, sr, bl, br, LFE",
                                 "", "", "", "", "", "", "", ""};
  static const unsigned int szStrChannel = 16;

  if(channelIdx < szStrChannel) {
    return strChannel[channelIdx];
  } else {
    return strChannel[szStrChannel - 1];
  }
}

static const uint32_t sndFreq[] = { 96000,
                                    88200,
                                    64000,
                                    48000,
                                    44100,
                                    32000,
                                    24000,
                                    22050,
                                    16000,
                                    12000,
                                    11025,
                                    8000,
                                    7350,
                                    0, 0, 0};

uint32_t esds_getFrequencyVal(unsigned int freqIdx) {

  static const unsigned int szSndFreq = sizeof(sndFreq) / sizeof(sndFreq[0]);

  if(freqIdx < szSndFreq) {
    return sndFreq[freqIdx];
  } else {
    return sndFreq[szSndFreq - 1];
  }
}

int esds_getFrequencyIdx(unsigned int frequency) {
  unsigned int idx;

  for(idx = 0; idx < sizeof(sndFreq) / sizeof(sndFreq[0]); idx++) {
    if(frequency == sndFreq[idx]) {
      return idx;
    }
  }

  return -1;
}


