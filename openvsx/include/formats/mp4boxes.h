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


#ifndef __MP4BOXES_H__
#define __MP4BOXES_H__

#include <stdio.h>
#include "fileutil.h"
#include "avcc.h"
#include "esds.h"

enum MP4_NOTIFY_TYPE {
  MP4_NOTIFY_TYPE_READHDR           = 0,
  MP4_NOTIFY_TYPE_READBOX           = 1,
  MP4_NOTIFY_TYPE_READBOXCHILDREN   = 2,
};

typedef int (* MP4_CB_READDATA)(void *, void *, unsigned int);
typedef int (* MP4_CB_SEEKDATA)(void *, FILE_OFFSET_T offset, int whence);
typedef int (* MP4_CB_CLOSE)(void *);
typedef int (* MP4_CB_CHECKFD)(void *);
typedef char *(* MP4_CB_GETNAME)(void *);
typedef int (* MP4_CB_NOTIFYTYPE)(void *, enum MP4_NOTIFY_TYPE type, void *pBox);

typedef struct MP4_FILE_STREAM {
  MP4_CB_READDATA         cbRead;
  MP4_CB_SEEKDATA         cbSeek;
  MP4_CB_CLOSE            cbClose;
  MP4_CB_CHECKFD          cbCheckFd;
  MP4_CB_GETNAME          cbGetName;
  MP4_CB_NOTIFYTYPE       cbNotifyType;
  void                   *pCbData;
  FILE_OFFSET_T           offset;
  FILE_OFFSET_T           size;
  void                   *pUserData;
} MP4_FILE_STREAM_T;

int mp4_cbReadDataFile(void *, void *pData, unsigned int len);
int mp4_cbSeekDataFile(void * , FILE_OFFSET_T offset, int whence);
int mp4_cbCloseDataFile(void *);
char *mp4_cbGetNameDataFile(void *);
int mp4_cbCheckFdDataFile(void *);


typedef struct BOX {

  uint32_t type;
  uint8_t usertype[16];

  FILE_OFFSET_T fileOffset;
  FILE_OFFSET_T fileOffsetData;
  uint32_t szhdr;
  uint64_t szdata;
  uint32_t szdata_ownbox;
  struct BOX *pnext;
  struct BOX *child;
  const void *handler;
  uint16_t subs;
  char name[6];

} BOX_T;

typedef struct FULLBOX {
  char version;
  char flags[3];
} FULLBOX_T;

typedef struct BOX_GENERIC {
  BOX_T box;
  FULLBOX_T fullbox;
} BOX_GENERIC_T;

typedef BOX_T BOX_MOOF_T;

typedef struct MP4_CONTAINER {
  BOX_T                      *proot;
  MP4_FILE_STREAM_T         *pStream;
  struct MP4_LOADER_NEXT    *pNextLoader;

  int                        haveMoov;

  //
  // The presence of the following box indicates that this is an ISO BMFF fragmented mp4 file
  //
  struct BOX_TREX           *pBoxTrex;
  int                        haveMoof;

} MP4_CONTAINER_T;

#define MP4_CONTAINER_IS_ISOBMFF(pMp4)  (((pMp4) && (pMp4)->haveMoof) ? 1 : 0)

typedef BOX_T *(*READ_FUNC)(MP4_FILE_STREAM_T *, BOX_T *, unsigned char *, unsigned int, int *);
typedef void (*DUMP_FUNC)(BOX_T *, int, int, FILE *);
typedef int (*WRITE_FUNC)(FILE_STREAM_T *, BOX_T *, unsigned char *, unsigned int, void *);
typedef void (*FREE_FUNC)(BOX_T *);

typedef struct BOX_HANDLER {
  uint32_t type;
  char description[32];
  READ_FUNC readfunc;
  WRITE_FUNC writefunc;
  DUMP_FUNC dumpfunc;
  FREE_FUNC freefunc;
  struct BOX_HANDLER *pnext;
  struct BOX_HANDLER *pchild;
} BOX_HANDLER_T;






typedef struct BOX_FTYP {
  BOX_T box;

  uint32_t major;
  uint32_t minor;
  uint32_t *pcompatbrands;
  uint32_t lencompatbrands;
} BOX_FTYP_T;


typedef struct BOX_IODS {
  BOX_T box;
  FULLBOX_T fullbox;

  uint8_t type;   // 0x10
  uint8_t extdescrtype[3]; // optional, if present types are Start = 0x80 ; End = 0xFE
  uint8_t len;
  uint8_t id[2];
  uint8_t od_profilelevel;
  uint8_t scene_profilelevel;
  uint8_t audio_profilelevel;
  uint8_t video_profilelevel;
  uint8_t graphics_profilelevel;

  /*
  ???
       -> 1 byte ES ID included descriptor type tag = 8-bit hex value 0x0E
     -> 3 bytes extended descriptor type tag string = 3 * 8-bit hex value
       - types are Start = 0x80 ; End = 0xFE
       - NOTE: the extended start tags may be left out
     -> 1 byte descriptor type length = 8-bit unsigned length

       -> 4 bytes Track ID = 32-bit unsigned value
         - NOTE: refers to non-data system tracks
*/


} BOX_IODS_T;

typedef struct BOX_MVHD {
  BOX_T box;
  FULLBOX_T fullbox;

  uint64_t creattime;
  uint64_t modificationtime;
  uint32_t timescale;
  uint64_t duration;

  uint32_t rate;
  uint16_t volume;
  uint16_t reserved;
  uint32_t reserved2[2];
  uint32_t matrix[9];
  uint32_t predef[6];
  uint32_t nexttrack;

} BOX_MVHD_T;

typedef struct BOX_TKHD  {
  BOX_T box;
  FULLBOX_T fullbox;

  uint64_t creattime;
  uint64_t modificationtime;
  uint32_t trackid;
  uint32_t reserved;
  uint64_t duration;

  uint32_t reserved2[2];
  uint16_t layer;
  uint16_t altgroup;
  uint16_t volume;
  uint16_t reserved3;
  uint32_t matrix[9];
  uint32_t width;
  uint32_t height;
} BOX_TKHD_T;

typedef struct BOX_MDHD  {
  BOX_T box;
  FULLBOX_T fullbox;

  uint64_t creattime;
  uint64_t modificationtime;
  uint32_t timescale;
  uint64_t duration;

  uint16_t language;
  uint16_t predefined;
} BOX_MDHD_T;

typedef struct BOX_HDLR  {
  BOX_T box;
  FULLBOX_T fullbox;

  uint32_t predefined;
  uint32_t handlertype;
  uint32_t reserved[3];
  char name[128];

} BOX_HDLR_T;

typedef struct BOX_SMHD  {
  BOX_T box;
  FULLBOX_T fullbox;

  uint16_t balance;
  uint16_t reserved;
} BOX_SMHD_T;

typedef struct BOX_VMHD  {
  BOX_T box;
  FULLBOX_T fullbox;

  uint16_t graphicsmode;
  uint16_t opcolor[3];
} BOX_VMHD_T;

typedef struct BOX_DREF_ENTRY  {
  BOX_T box;
  FULLBOX_T fullbox;

  char name[64];

  struct BOX_DREF_ENTRY *pnext;

} BOX_DREF_ENTRY_T;

typedef struct BOX_DREF  {
  BOX_T box;
  FULLBOX_T fullbox;

  uint32_t entrycnt;

} BOX_DREF_T;

typedef struct BOX_STSD  {
  BOX_T box;
  FULLBOX_T fullbox;

  uint32_t entrycnt;

} BOX_STSD_T;


typedef struct BOX_MP4A  {
  BOX_T box;

  //SampleEntry
  uint16_t reserved[3];
  uint16_t datarefidx;

  //AudioSampleEntry
  uint16_t soundver;
  uint16_t reserved2[3];
  uint16_t channelcnt;
  uint16_t samplesize;
  uint16_t predefined;
  uint16_t reserved3;
  uint32_t timescale;

} BOX_MP4A_T;

typedef struct BOX_MP4V  {
  BOX_T box;

  uint8_t  reserved[6];
  uint16_t datarefidx;
  uint16_t codecstreamver;
  uint16_t codecstreamrev;
  uint8_t  reserved2[12];
  uint16_t wid;
  uint16_t ht;
  uint32_t horizresolution; //0x00480000  72dpi
  uint32_t vertresolution; //0x00480000  72dpi
  uint32_t datasz;
  uint16_t framesz;
  uint8_t codername[32];

  uint16_t reserved3; // 0x0018
  uint16_t reserved4; // 0xffff 


} BOX_MP4V_T;

typedef struct BOX_ESDS_DESCR {
  uint8_t descriptor; 
  uint8_t extdescrtype[3];  // Start = 0x80 ; End = 0xFE
  uint8_t length;
  uint8_t pad[3];
} BOX_ESDS_DESCR_T;

typedef struct BOX_ESDS_DESCR_TYPE {
  BOX_ESDS_DESCR_T type; // descriptor type 0x03 
  uint16_t id;
  uint8_t priority; // 1 bit stream dependence, 1 bit URL flag, 1 bit OCR stream flag
                    // 5 bits stream priority defaults to 16 and ranges from 0 through to 3
  uint8_t pad;
} BOX_ESDS_DESCR_TYPE_T;

typedef struct BOX_ESDS_DESCR_DECODERCONFIG {
  BOX_ESDS_DESCR_T type; // descriptor type 0x04 
  uint8_t objtypeid;
  uint8_t streamtype;   // 6 bits, 1 bit upstream flag, 1 bit reserved = 1
  uint8_t bufsize[3];
  uint8_t pad[3];
  uint32_t maxbitrate;
  uint32_t avgbitrate;
} BOX_ESDS_DESCR_DECODERCONFIG_T;

typedef struct BOX_ESDS_DESCR_DECODERSPECIFIC {
  BOX_ESDS_DESCR_T type; // descriptor type 0x05 
  uint8_t          startcodes[ESDS_STARTCODES_SZMAX];
} BOX_ESDS_DESCR_DECODERSPECIFIC_T;

typedef struct BOX_ESDS_DESCR_DECODERSL {
  BOX_ESDS_DESCR_T type; // descriptor type 0x06 
  uint8_t val[4];  // 0x02
} BOX_ESDS_DESCR_DECODERSL_T;

typedef struct BOX_ESDS  {
  BOX_T box;
  FULLBOX_T fullbox;

  BOX_ESDS_DESCR_TYPE_T estype;    
  BOX_ESDS_DESCR_DECODERCONFIG_T config;
  BOX_ESDS_DESCR_DECODERSPECIFIC_T specific;
  BOX_ESDS_DESCR_DECODERSL_T sl;
} BOX_ESDS_T;

typedef struct BOX_AVC1  {
  BOX_T box;

  //SampleEntry
  uint16_t reserved[3];
  uint16_t datarefidx;  // index into dref

  //VisualSampleEntry
  uint16_t predefined;
  uint16_t reserved2;
  uint32_t predefined2[3];
  uint16_t width;
  uint16_t height;
  uint32_t horizresolution; //0x00480000  72dpi
  uint32_t vertresolution; //0x00480000  72dpi
  uint32_t reserved3;
  uint16_t framecnt;
  unsigned char compressorname[32];
  uint16_t depth; // 0x0018
  uint16_t predefined3;  // -1
	//CleanApertureBox		clap;	// optional
	//PixelAspectRatioBox	pasp;		// optional

} BOX_AVC1_T;

typedef struct BOX_AVCC  {
  BOX_T box;
  AVC_DECODER_CFG_T avcc;
} BOX_AVCC_T;

typedef struct BOX_SAMR  {
  BOX_T box;

  uint8_t  reserved[6];
  uint16_t datarefidx;  // index into dref
  uint8_t  reserved2[16];
  uint16_t timescale;
  uint16_t reserved3;

} BOX_SAMR_T;

typedef struct BOX_DAMR  {
  BOX_T box;

  uint32_t vendor;
  uint8_t  decoderver;
  uint8_t  pad;
  uint16_t modeset;
  uint8_t  modechangeperiod;
  uint8_t  framespersample;

} BOX_DAMR_T;

typedef struct BOX_BTRT {
  BOX_T box;
  uint32_t bufferSizeDB;
  uint32_t maxBtrt;
  uint32_t avgBtrt;
} BOX_BTRT_T;

typedef struct BOX_STTS_ENTRY {
  uint32_t samplecnt;
  uint32_t sampledelta;
} BOX_STTS_ENTRY_T;

typedef struct BOX_STTS_ENTRY_LIST {
  uint32_t entrycnt;
  BOX_STTS_ENTRY_T *pEntries;
  uint64_t totsamplesduration;
  uint32_t clockHz;
} BOX_STTS_ENTRY_LIST_T;

typedef struct BOX_STTS  {
  BOX_T box;
  FULLBOX_T fullbox;
  BOX_STTS_ENTRY_LIST_T list;
} BOX_STTS_T;

typedef struct BOX_CTTS_ENTRY {
  uint32_t samplecount;
  uint32_t sampleoffset;
} BOX_CTTS_ENTRY_T;

typedef struct BOX_CTTS  {
  BOX_T box;
  FULLBOX_T fullbox;

  uint32_t entrycnt;
  BOX_CTTS_ENTRY_T *pEntries;

} BOX_CTTS_T;

typedef struct BOX_STSC_ENTRY {
  uint32_t firstchunk;
  uint32_t sampleperchunk;
  uint32_t sampledescidx;
} BOX_STSC_ENTRY_T;

typedef struct BOX_STSC  {
  BOX_T box;
  FULLBOX_T fullbox;

  uint32_t entrycnt;
  BOX_STSC_ENTRY_T *pEntries;

} BOX_STSC_T;

typedef struct BOX_STSZ  {
  BOX_T box;
  FULLBOX_T fullbox;

  uint32_t samplesize;
  uint32_t samplecount;

  uint32_t *pSamples;

} BOX_STSZ_T;

typedef struct BOX_STCO  {
  BOX_T box;
  FULLBOX_T fullbox;

  uint32_t entrycnt;

  uint32_t *pSamples;

} BOX_STCO_T;

typedef BOX_STCO_T BOX_STSS_T;

typedef BOX_T BOX_MVEX_T;

typedef struct BOX_MEHD {
  BOX_T box;
  FULLBOX_T fullbox;

  uint64_t duration;

} BOX_MEHD_T;

typedef struct BOX_TREX {
  BOX_T box;
  FULLBOX_T fullbox;

  uint32_t trackid;
  uint32_t default_sample_description_index;
  uint32_t default_sample_duration;
  uint32_t default_sample_size;
  uint32_t default_sample_flags;

#define TREX_SAMPLE_DEPENDS_ON(f) (((f) >> 24) & 0x03)
#define TREX_SAMPLE_IS_DEPENDED_ON(f) (((f) >> 22) & 0x03)
#define TREX_SAMPLE_HAS_REDUNDANCY(f) (((f) >> 20) & 0x03)
#define TREX_SAMPLE_PADDING_VALUE(f) (((f) >> 17) & 0x07)
#define TREX_SAMPLE_IS_DIFFERENCE_SAMPLE(f) (((f) >> 16) & 0x01)
#define TREX_SAMPLE_DEGRADATION_PRIORITY(f) ((f)  & 0xffff)

} BOX_TREX_T;

typedef struct BOX_MFHD {
  BOX_T box;
  FULLBOX_T fullbox;

  uint32_t sequence;

} BOX_MFHD_T;

typedef BOX_T BOX_TRAF_T;

typedef struct BOX_TFHD {
  BOX_T box;
  FULLBOX_T fullbox;

  uint32_t trackid;

#define TFHD_FLAG_BASE_DATA_OFFSET           0x000001
#define TFHD_FLAG_SAMPLE_DESCRIPTION_INDEX   0x000002
#define TFHD_FLAG_DEFAULT_SAMPLE_DURATION    0x000008
#define TFHD_FLAG_DEFAULT_SAMPLE_SIZE        0x000010
#define TFHD_FLAG_DEFAULT_SAMPLE_FLAGS       0x000020
#define TFHD_FLAG_DURATION_IS_EMPTY          0x010000

  int flags;
  uint64_t base_data_offset;
  uint32_t sample_description_index;
  uint32_t default_sample_duration;
  uint32_t default_sample_size;
  uint32_t default_sample_flags;

} BOX_TFHD_T;

typedef struct BOX_TRUN_ENTRY {
  uint32_t sample_duration;
  uint32_t sample_size;
  uint32_t sample_flags;
  uint32_t sample_composition_time_offset;
} BOX_TRUN_ENTRY_T;

typedef struct BOX_TRUN {
  BOX_T box;
  FULLBOX_T fullbox;

  unsigned int alloc_count;
  uint32_t sample_count;
  int32_t data_offset;                    // this + TFHD base_data_offset is the beginning of the MOOF box
                                          // to the beginning of the MDAT box + 8 bytes (start of MDAT content)
  uint32_t first_sample_flags;

#define TRUN_FLAG_DATA_OFFSET                     0x000001
#define TRUN_FLAG_FIRST_SAMPLE_FLAGS              0x000004
#define TRUN_FLAG_SAMPLE_DURATION                 0x000100
#define TRUN_FLAG_SAMPLE_SIZE                     0x000200
#define TRUN_FLAG_SAMPLE_FLAGS                    0x000400
#define TRUN_FLAG_SAMPLE_COMPOSITION_TIME_OFFSET  0x000800

  int flags;                             // obtained from fullbox.flags
  unsigned int sample_size;              // size of each BOX_TRUN_ENTRY_T
  BOX_TRUN_ENTRY_T *pSamples;

} BOX_TRUN_T;

typedef struct BOX_SIDX_ENTRY {
  uint32_t referenced_size;              // 1 bit type, 31 bytes size
  uint32_t subsegment_duration;
  uint32_t sap;                          // 1 bit starts with SAP, 3 bits SAP type, 28 bits SAP delta time 

#define SIDX_MASK_REF_TYPE                 0x80000000
#define SIDX_MASK_REF_SIZE                 0x7fffffff
#define SIDX_MASK_STARTS_WITH_SAP          0x80000000
#define SIDX_MASK_SAP_TYPE                 0x70000000
#define SIDX_MASK_SAP_DELTA_TIME           0x0fffffff
} BOX_SIDX_ENTRY_T;

typedef struct BOX_SIDX {
  BOX_T box;
  FULLBOX_T fullbox;

  uint32_t reference_id;
  uint32_t timescale;
  uint64_t earliest_presentation_time;
  uint64_t first_offset;
  uint16_t reserved;
  uint16_t reference_count;
  BOX_SIDX_ENTRY_T *pSamples;

} BOX_SIDX_T;

typedef struct BOX_TFDT {
  BOX_T box;
  FULLBOX_T fullbox;

  uint64_t baseMediaDecodeTime;
} BOX_TFDT_T;

typedef struct CHUNK_DESCR {
  uint32_t sz;
  //uint32_t offset;
} CHUNK_DESCR_T;

typedef struct CHUNK_SZ {
  CHUNK_DESCR_T *p_stchk;
  uint32_t stchk_sz;
} CHUNK_SZ_T;

typedef BOX_T BOX_MDAT_T;

struct MP4_MDAT_CONTENT_NODE;
typedef int (*WRITE_SAMPLE_FUNC)(FILE_STREAM_T *, struct MP4_MDAT_CONTENT_NODE *, unsigned char *, unsigned int);
typedef struct MP4_MDAT_CONTENT_NODE *(*CB_NEXT_SAMPLE)(void *, int);


typedef struct MP4_MDAT_CONTENT_NODE {

#define MP4_MDAT_CONTENT_FLAG_SYNCSAMPLE      0x01

  FILE_STREAM_T *pStreamIn;
  FILE_OFFSET_T fileOffset;
  uint32_t sizeRd;
  uint32_t sizeWr;
  uint32_t frameId;   // 0th based for entire movie
  uint64_t playOffsetHz;
  uint32_t playDurationHz;
  uint16_t flags;
  int8_t   decodeOffset;  
  uint8_t  pad;

  WRITE_SAMPLE_FUNC cbWriteSample;
  uint32_t tkhd_id;
  uint32_t chunk_idx;

  struct MP4_MDAT_CONTENT_NODE *pnext;

} MP4_MDAT_CONTENT_NODE_T;

typedef struct MP4_MDAT_CONTENT {
  MP4_MDAT_CONTENT_NODE_T *pSampleCbPrev;
  unsigned int samplesBufSz;
  MP4_MDAT_CONTENT_NODE_T  *pSamplesBuf;
} MP4_MDAT_CONTENT_T;

typedef struct MP4_SAMPLES_EXT {
  MP4_CONTAINER_T *pMp4;

  MP4_MDAT_CONTENT_T aud;
  CHUNK_SZ_T stchkAud;
  BOX_STCO_T *pBoxStcoAud;

  MP4_MDAT_CONTENT_T vid;
  CHUNK_SZ_T stchkVid;
  BOX_STCO_T *pBoxStcoVid;

} MP4_SAMPLES_EXT_T;

                                          // 66 * 60 * 60 *24 * 365.242199 * 66yrs
#define  SEC_BETWEEN_EPOCH_1904_1970      ((unsigned long) (2082757116UL + 86400UL))  

                                          // 70 * 60 * 60 *24 * 365.242199  * 66yrs
#define  SEC_BETWEEN_EPOCH_1900_1970      ((unsigned long) (2208984820UL + 86400UL))  


MP4_CONTAINER_T *mp4_createContainer();

#define MP4_BOX_NAME_INT2C(ui) (((unsigned char *)&(ui))[0]), \
                               (((unsigned char *)&(ui))[1]), \
                               (((unsigned char *)&(ui))[2]), \
                               (((unsigned char *)&(ui))[3])


#endif // __MP4BOXES_H__
