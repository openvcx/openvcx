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


#ifndef __PKTQUEUE_H__
#define __PKTQUEUE_H__

#include "unixcompat.h"
#include "pthread_compat.h"

#ifndef WIN32
#include <sys/time.h>
#endif // WIN32

#define PKTQUEUE_FLAG_HAVEPKTDATA          0x01
#define PKTQUEUE_FLAG_HAVECOMPLETE         0x02
#define PKTQUEUE_FLAG_KEYFRAME             0x04


typedef struct PKTQ_TIME {
  uint64_t             pts;
  int64_t              dts;
} PKTQ_TIME_T;


typedef struct PKTQ_EXTRADATA {
  PKTQ_TIME_T          tm;
  void                *pQUserData;
} PKTQ_EXTRADATA_T;


typedef struct PKTQUEUE_PKT {
  uint32_t             len;
  uint32_t             flags;
  unsigned char        *pData;
  unsigned char        *pBuf;
  PKTQ_EXTRADATA_T      xtra;
  unsigned int          idx;          // set to uniqueWrIdx for reader to
                                      // check if slot was overwritten 
                                      // in direct read
  unsigned int          allocSz;      // does not include prebufOffset
} PKTQUEUE_PKT_T;

typedef enum PKTQ_TYPE {
  PKTQ_TYPE_DEFAULT       = 0,
  PKTQ_TYPE_FRAMEBUF      = 1,
  PKTQ_TYPE_RINGBUF       = 2
} PKTQ_TYPE_T;

typedef enum PKTQ_OVERWRITE_TYPE {
  PKTQ_OVERWRITE_DEFAULT        = 0,
  PKTQ_OVERWRITE_OK             = 1,
  PKTQ_OVERWRITE_FIND_KEYFRAME  = 2,
  PKTQ_OVERWRITE_FIND_PTS       = 3,
  PKTQ_OVERWRITE_SKIP_COUNT     = 4
} PKTQ_OVERWRITE_TYPE_T;

#define PKTQUEUE_OVERWRITE_PTS_TOLERANCE       30000

typedef struct PKTQUEUE_CONFIG {
  PKTQ_TYPE_T         type;

  int                 id;             // optional id of this queue
                                      // used in log output
  int                 userDataType;
  int                 uselock;
  unsigned int        maxPkts; 
  unsigned int        maxPktLen;      // does not include prebufOffset
  unsigned int        growMaxPkts;
  unsigned int        growMaxPktLen;  // does not include prebufOffset
  int                 growSzOnlyAsNeeded;
  unsigned int        userDataLen;

  unsigned int        prebufOffset;   // prebuffer before insertion point
                                      // to avoid any memcpy by 
                                      // pktqueue user and to allow contiguous
                                      // reads.

  int                 concataddenum;  // if > 0, when adding and the prior 
                                      // add's slot has not yet been read, 
                                      // try to fill last add slot in
                                      // multiples of concataddenum
  int                 prealloc;
  PKTQ_OVERWRITE_TYPE_T overwriteType;
  int                   overwriteVal;
  int                   maxRTlatencyMs;  
} PKTQUEUE_CONFIG_T;

typedef struct PKTQUEUE_COND {
  pthread_cond_t      cond;
  pthread_mutex_t     mtx;  
} PKTQUEUE_COND_T;


typedef struct PKTQUEUE_STATS_REPORT {
  THROUGHPUT_STATS_T            stats;
} PKTQUEUE_STATS_REPORT_T;

typedef struct PKTQUEUE {
  PKTQUEUE_CONFIG_T   cfg;
  THROUGHPUT_STATS_T *pstats;
  PKTQUEUE_PKT_T     *pkts;
  PKTQUEUE_PKT_T     *pktTmp;
  int                 ownTmpPkt;
  pthread_mutex_t     mtx;  
  pthread_mutex_t     mtxRd;  
  pthread_mutex_t     mtxWr;  
  unsigned int        idxWr;
  unsigned int        idxInWr;
  unsigned int        idxRd;
  unsigned int        idxInRd;
  unsigned int        numOverwritten;
  PKTQUEUE_COND_T     notify;
  PKTQUEUE_COND_T    *pnotifyComplement;
  int                 haveRdr;
  int                 haveRdrSetManually;
  uint16_t            haveData;
  uint16_t            isInit;
  uint32_t            uniqueWrIdx;
  int                 consecOverwritten;
  int                 quitRequested;
  int                 inDirectRd;
  int                 prevWrWasEnd;
  uint64_t            ptsLastWr;
  uint64_t            ptsLastOverwrite;
  struct PKTQUEUE    *pQComplement;
} PKTQUEUE_T;

enum PKTQUEUE_RC {
  PKTQUEUE_RC_OK                = 0,
  PKTQUEUE_RC_ERROR             = -1,
  PKTQUEUE_RC_SIZETOOBIG        = -2,
  PKTQUEUE_RC_WOULDOVERWRITE    = 3
};


PKTQUEUE_T *pktqueue_create(unsigned int maxPkts, unsigned int szPkt,
                            unsigned int growMaxPkts, unsigned int growSzPkts,
                            unsigned int prebufOffset, unsigned int userDataLen, 
                           int uselock, int prealloc);
PKTQUEUE_T *framebuf_create(unsigned int sz, unsigned int prebufOffset);
PKTQUEUE_T *ringbuf_create(unsigned int sz);
void pktqueue_destroy(PKTQUEUE_T *pQ);
int pktqueue_reset(PKTQUEUE_T *pQ, int lock);
enum PKTQUEUE_RC pktqueue_addpkt(PKTQUEUE_T *pQ, const unsigned char *pData, 
                                 unsigned int len, PKTQ_EXTRADATA_T *pXtra, int keyframe);
int pktqueue_readpkt(PKTQUEUE_T *pQ, unsigned char *pData, 
                                  unsigned int lenTot, PKTQ_EXTRADATA_T *pXtra, 
                                  unsigned int *pNumOverwritten); 

int pktqueue_readexact(PKTQUEUE_T *pQ, unsigned char *pData, unsigned int lenTot);
int pktqueue_waitforunreaddata(PKTQUEUE_T *pQ);
void pktqueue_wakeup(PKTQUEUE_T *pQ);
void pktqueue_setrdr(PKTQUEUE_T *pQ, int force);

enum PKTQUEUE_RC pktqueue_addpartialpkt(PKTQUEUE_T *pQ, const unsigned char *pData, 
                                        unsigned int len, PKTQ_EXTRADATA_T *pXtra, 
                                        int endingPkt, int overwrite);
const PKTQUEUE_PKT_T *pktqueue_readpktdirect(PKTQUEUE_T *pQ);
int pktqueue_readpktdirect_havenext(PKTQUEUE_T *pQ);
int pktqueue_readpktdirect_done(PKTQUEUE_T *pQ);
int pktqueue_swapreadslots(PKTQUEUE_T *pQ, PKTQUEUE_PKT_T **ppSlot);
int pktqueue_havepkt(PKTQUEUE_T *pQ);
//int pktqueue_createstats(PKTQUEUE_T *pQ, unsigned int rangeMs1, unsigned int rangeMs2);
int pktqueue_getstats(PKTQUEUE_T *pQ, PKTQUEUE_STATS_REPORT_T *pReport);
int pktqueue_lock(PKTQUEUE_T *pQ, int lock);
int pktqueue_syncToRT(PKTQUEUE_T *pQ);
int64_t pktqueue_getAgePts(PKTQUEUE_T *pQ, uint64_t *ptsLastWr);

typedef void (* PKTQUEUE_DUMP_USERDATA) (void *);
void pktqueue_dump(PKTQUEUE_T *pQ, PKTQUEUE_DUMP_USERDATA cbDumpUserData);


#endif //  __PKTQUEUE_H__
