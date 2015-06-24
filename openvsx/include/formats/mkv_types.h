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


#ifndef __MKV_TYPES_H__
#define __MKV_TYPES_H__

#include "ebml.h"


#define MKV_ID_VOID                       EBML_ID_VOID 
#define MKV_ID_CRC32                      0xbf

//
// Top level type
//
#define MKV_ID_HEADER                     0x1a45dfa3

//
// Child of MKV_ID_HEADER
//
#define MKV_ID_EBMLVERSION                0x4286
#define MKV_ID_EBMLREADVERSION            0x42f7
#define MKV_ID_EBMLMAXIDLENGTH            0x42f2
#define MKV_ID_EBMLMAXSIZELENGTH          0x42f3
#define MKV_ID_DOCTYPE                    0x4282
#define MKV_ID_DOCTYPEVERSION             0x4287
#define MKV_ID_DOCTYPEREADVERSION         0x4285



//
// Top level type
//
#define MKV_ID_SEGMENT                    0x18538067

//
// Child of MKV_ID_SEGMENT
//
#define MKV_ID_INFO                       0x1549a966
#define MKV_ID_TRACKS                     0x1654ae6b
#define MKV_ID_CUES                       0x1C53bb6b
#define MKV_ID_TAGS                       0x1254c367
#define MKV_ID_SEEKHEAD                   0x114d9b74
#define MKV_ID_ATTACHMENTS                0x1941a469
#define MKV_ID_CLUSTER                    0x1f43b675
#define MKV_ID_CHAPTERS                   0x1043a770


//
// Child of MKV_ID_SEEKHEAD
//
#define MKV_ID_SEEKENTRY                  0x4dbb

//
// Child of MKV_ID_SEEKENTRY
//
#define MKV_ID_SEEKID                     0x53ab
#define MKV_ID_SEEKPOSITION               0x53ac

//
// Child of MKV_ID_TAGS
//
#define MKV_ID_TAG                        0x7373

//
// Child of MKV_ID_TAG
//
#define MKV_ID_TAGTARGETS                 0x63c0
#define MKV_ID_SIMPLETAG                  0x67c8

//
// Child of MKV_ID_TAGTARGETS
//
#define MKV_ID_TAGTYPE                    0x63cA
#define MKV_ID_TAGTYPEVAL                 0x68cA
#define MKV_ID_TAGTRACKUID                0x63c5
#define MKV_ID_TAGATTACHUID               0x63c6
#define MKV_ID_TAGCHAPTERUID              0x63c4

//
// Child of MKV_ID_SIMPLETAG
//
#define MKV_ID_TAGNAME                    0x45a3
#define MKV_ID_TAGSTRING                  0x4487
#define MKV_ID_TAGLANGUAGE                0x447a

//
// Child of MKV_ID_CUES
//
#define MKV_ID_POINTENTRY                 0xbb
//
// Child of MKV_ID_POINTENTRY
//
#define MKV_ID_CUETIME                    0xb3
#define MKV_ID_CUETRACKPOSITION           0xb7

//
// Child of MKV_ID_CUETRACKPOSITION
//
#define MKV_ID_CUETRACK                   0xf7
#define MKV_ID_CUECLUSTERPOSITION         0xf1
#define MKV_ID_CUEBLOCKNUMBER             0x5378

//
// Child of MKV_ID_CHAPTERS
//
#define MKV_ID_EDITIONENTRY               0x45B9

//
// Child of MKV_ID_EDITIONENTRY
//
#define MKV_ID_CHAPTERATOM                 0xb6
#define MKV_ID_EDITIONUID                  0x45bc
#define MKV_ID_EDITIONFLAGHIDDEN           0x45bd
#define MKV_ID_EDITIONFLAGDEFAULT          0x45db
#define MKV_ID_EDITIONFLAGORDERED          0x45dd

//
// Child of MKV_ID_CHAPTERATOM
//
#define MKV_ID_CHAPTERTIMESTART            0x91
#define MKV_ID_CHAPTERTIMEEND              0x92
#define MKV_ID_CHAPTERUID                  0x73c4
#define MKV_ID_CHAPTERDISPLAY              0x80
#define MKV_ID_CHAPTERFLAGHIDDEN           0x98
#define MKV_ID_CHAPTERFLAGENABLED          0x4598
#define MKV_ID_CHAPTERPHYSEQUIV            0x63c3

//
// Child of MKV_ID_CHAPTERDISPLAY
//
#define MKV_ID_CHAPSTRING                  0x85
#define MKV_ID_CHAPLANG                    0x437c

//
// Child MKV_ID_ATTACHMENTS
//
#define MKV_ID_ATTACHEDFILE                 0x61a7

//
// Child of MKV_ID_ATTACHEDFILE
//
#define MKV_ID_FILEDESC                     0x467e
#define MKV_ID_FILENAME                     0x466e
#define MKV_ID_FILEMIMETYPE                 0x4660
#define MKV_ID_FILEDATA                     0x465c
#define MKV_ID_FILEUID                      0x46ae

//
// Child of MKV_ID_TRACKS
//
#define MKV_ID_TRACKENTRY                   0xae

//
// Child of MKV_ID_TRACKENTRY
//
#define MKV_ID_TRACKNUMBER                  0xd7
#define MKV_ID_TRACKNAME                    0x536e
#define MKV_ID_TRACKUID                     0x73c5
#define MKV_ID_TRACKTYPE                    0x83
#define MKV_ID_TRACKLANGUAGE                0x22b59c
#define MKV_ID_TRACKDEFAULTDURATION         0x23e383
#define MKV_ID_TRACKTIMECODESCALE           0x23314f
#define MKV_ID_TRACKFLAGDEFAULT             0x88
#define MKV_ID_TRACKFLAGFORCED              0x55aa
#define MKV_ID_TRACKVIDEO                   0xe0
#define MKV_ID_TRACKAUDIO                   0xe1
#define MKV_ID_TRACKOPERATION               0xe2
#define MKV_ID_TRACKCONTENTENCODINGS        0x6d80
#define MKV_ID_TRACKFLAGENABLED             0xb9
#define MKV_ID_TRACKFLAGLACING              0x9c
#define MKV_ID_TRACKMINCACHE                0x6de7
#define MKV_ID_TRACKMAXCACHE                0x6df8
#define MKV_ID_TRACKMAXBLKADDID             0x55ee
#define MKV_ID_TRACKATTACHMENTLINK          0x7446
#define MKV_ID_CODECID                      0x86
#define MKV_ID_CODECPRIVATE                 0x63a2
#define MKV_ID_CODECNAME                    0x258688
#define MKV_ID_CODECDECODEALL               0xaa
#define MKV_ID_CODECINFOURL                 0x3b4040
#define MKV_ID_CODECDOWNLOADURL             0x26b240

//
// Child of MKV_ID_TRACKVIDEO
//
#define MKV_ID_VIDEOFRAMERATE               0x2383e3
#define MKV_ID_VIDEODISPLAYWIDTH            0x54b0
#define MKV_ID_VIDEODISPLAYHEIGHT           0x54ba
#define MKV_ID_VIDEOPIXELWIDTH              0xb0
#define MKV_ID_VIDEOPIXELHEIGHT             0xba
#define MKV_ID_VIDEOPIXELCROPB              0x54aa
#define MKV_ID_VIDEOPIXELCROPT              0x54bb
#define MKV_ID_VIDEOPIXELCROPL              0x54cc
#define MKV_ID_VIDEOPIXELCROPR              0x54dd
#define MKV_ID_VIDEODISPLAYUNIT             0x54b2
#define MKV_ID_VIDEOFLAGINTERLACED          0x9a
#define MKV_ID_VIDEOSTEREOMODE              0x53b8
#define MKV_ID_VIDEOASPECTRATIO             0x54b3
#define MKV_ID_VIDEOCOLORSPACE              0x2eb524

//
// Child of MKV_ID_TRACKAUDIO
//
#define MKV_ID_AUDIOSAMPLINGFREQ            0xb5
#define MKV_ID_AUDIOOUTSAMPLINGFREQ         0x78b5
#define MKV_ID_AUDIOBITDEPTH                0x6264
#define MKV_ID_AUDIOCHANNELS                0x9f

//
// Child of MKV_ID_TRACKOPERATION
//
#define MKV_ID_TRACKCOMBINEPLANES           0xe3

//
// Child of MKV_ID_TRACKCOMBINEPLANES
//
#define MKV_ID_TRACKPLANE               0xe4

//
// Child of MKV_ID_TRACKPLANE
//
#define MKV_ID_TRACKPLANEUID                0x35
#define MKV_ID_TRACKPLANETYPE               0xe6

//
// Child of MKV_ID_TRACKCONTENENCODINGS
//
#define MKV_ID_TRACKCONTENTENCODING         0x6240

//
// Child of MKV_ID_TRACKCONTENTENCODING
//
#define MKV_ID_ENCODINGORDER                0x5031
#define MKV_ID_ENCODINGSCOPE                0x5032
#define MKV_ID_ENCODINGTYPE                 0x5033
#define MKV_ID_ENCODINGCOMPRESSION          0x5034

//
// Child of MKV_ID_ENCODINGCOMPRESSION
//
#define MKV_ID_ENCODINGCOMPALGO             0x4254
#define MKV_ID_ENCODINGCOMPSETTINGS         0x4255

//
// Child of MKV_ID_INFO
//
#define MKV_ID_TIMECODESCALE                0x2ad7b1
#define MKV_ID_DURATION                     0x4489
#define MKV_ID_TITLE                        0x7ba9
#define MKV_ID_WRITINGAPP                   0x5741
#define MKV_ID_MUXINGAPP                    0x4d80
#define MKV_ID_DATEUTC                      0x4461
#define MKV_ID_SEGMENTUID                   0x73a4


//
// Child of MKV_ID_CLUSTER
//
#define MKV_ID_CLUSTERTIMECODE              0xe7
#define MKV_ID_SILENTTRACKS                 0x5854
#define MKV_ID_SILENTTRACKNUM               0x58d7
#define MKV_ID_CLUSTERPOSITION              0xa7
#define MKV_ID_CLUSTERPREVSIZE              0xab
#define MKV_ID_SIMPLEBLOCK                  0xa3
#define MKV_ID_BLOCKGROUP                   0xa0
#define MKV_ID_BLOCK                        0xa1

//
// Child of MKV_ID_BLOCK / MKV_ID_SIMPLEBLOCK
//
#define MKV_ID_BLOCKDURATION                0x9b
#define MKV_ID_BLOCKREFERENCE               0xfb


//#define MKV_ID_EDITIONFLAGHIDDEN           0x45bd
//#define MKV_ID_EDITIONFLAGDEFAULT          0x45db
//#define MKV_ID_EDITIONFLAGORDERED          0x45dd


typedef enum MKV_TRACK_TYPE {
  MKV_TRACK_TYPE_NONE                       = 0x0,
  MKV_TRACK_TYPE_VIDEO                      = 0x1,
  MKV_TRACK_TYPE_AUDIO                      = 0x2,
  MKV_TRACK_TYPE_COMPLEX                    = 0x3,
  MKV_TRACK_TYPE_LOGO                       = 0x10,
  MKV_TRACK_TYPE_SUBTITLE                   = 0x11,
  MKV_TRACK_TYPE_CONTROL                    = 0x20,
} MKV_TRACK_TYPE_T;




typedef struct MKV_HEADER {
  uint32_t                     version;
  uint32_t                     readVersion;
  uint32_t                     maxIdLength;
  uint32_t                     maxSizeLength;
  uint32_t                     docVersion;
  uint32_t                     docReadVersion;
  char                         docType[16];
} MKV_HEADER_T;

typedef struct MKV_SEEKENTRY {
  uint64_t                     id;
  uint64_t                     pos;
} MKV_SEEKENTRY_T;

typedef struct MKV_SEEKHEAD {
  EBML_ARR_T                    arrSeekEntries; // MKV_SEEKENTRY_T
} MKV_SEEKHEAD_T;

typedef struct MKV_SEGINFO {
  uint64_t                     timeCodeScale;
  double                       duration;
  char                         title[48];
  char                         muxingApp[48];
  char                         writingApp[48];
  unsigned char                segUid[16];
} MKV_SEGINFO_T;

typedef struct MKV_TRACK_VID {
  uint32_t                     pixelWidth;
  uint32_t                     pixelHeight;
  uint32_t                     displayWidth;
  uint32_t                     displayHeight;
  uint32_t                     displayUnit;
  double                       fps;

  //
  // Additional fields not set by parser
  //
  //int                          active;

} MKV_TRACK_VID_T;

typedef struct MKV_TRACK_AUD {
  double                        samplingFreq;
  double                        samplingOutFreq;
  uint32_t                      channels;
  uint32_t                      bitDepth;

  //
  // Additional fields not set by parser
  //
  //int                           active;

} MKV_TRACK_AUD_T;

typedef struct MKV_TRACK {
  uint32_t                     number;
  uint32_t                     type; // MKV_TRACK_TYPE_T
  //uint32_t                     uid;
  uint64_t                     uid;
  uint32_t                     flagLacing;
  uint64_t                     defaultDuration;
  double                       timeCodeScale;
  char                         language[32];
  char                         codecId[32];
  char                         codecName[32];

  MKV_TRACK_VID_T              video;
  MKV_TRACK_AUD_T              audio;
  EBML_BLOB_T                  codecPrivate;

  //
  // Additional fields not set by parser
  //

  XC_CODEC_TYPE_T              codecType;

} MKV_TRACK_T;


typedef struct MKV_TRACKS {
  EBML_ARR_T                    arrTracks; // MKV_TRACK_T
} MKV_TRACKS_T;

typedef struct MKV_TAG_TARGET {
  uint64_t                      trackUid;
  uint64_t                      chapterUid;
  uint64_t                      attachUid;
  uint64_t                      typeVal;
  char                          type[32];
} MKV_TAG_TARGET_T;

typedef struct MKV_TAG_SIMPLE {
  char                          name[32];
  char                          str[32];
  char                          language[32];
} MKV_TAG_SIMPLE_T;

typedef struct MKV_TAG {
  MKV_TAG_TARGET_T             tagTarget;
  MKV_TAG_SIMPLE_T             simpleTag;
} MKV_TAG_T;


typedef struct MKV_TAGS {
  EBML_ARR_T                    arrTags; // MKV_TAG_T
} MKV_TAGS_T;


typedef struct MKV_SEGMENT {
  MKV_SEEKHEAD_T               seekHead;
  MKV_TAGS_T                   tags;
  MKV_SEGINFO_T                info;
  MKV_TRACKS_T                 tracks;
} MKV_SEGMENT_T;


typedef struct MKV_BLOCKGROUP {
  EBML_BLOB_PARTIAL_T          simpleBlock;
  uint64_t                     duration;
  uint64_t                     reference;
  //uint64_t non_simple;
} MKV_BLOCKGROUP_T;

typedef struct MKV_CLUSTER {
  uint64_t                     timeCode;
  EBML_ARR_T                    arrBlocks; // MKV_BLOCKGROUP_T
} MKV_CLUSTER_T;


#endif // __MKV_TYPES_H__
