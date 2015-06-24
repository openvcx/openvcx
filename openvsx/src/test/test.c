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

#if 0
int testdtls() {

  int rc = 0;
  char fingerprint[256];
  STREAM_DTLS_CTXT_T dtls;

  logger_SetLevel(S_DEBUG);
  logger_AddStderr(S_DEBUG, 0);

  memset(fingerprint, 0, sizeof(fingerprint));
  memset(&dtls, 0, sizeof(dtls));
  //rc = dtls_get_cert_fingerprint("dtlscert.pem", fingerprint, sizeof(fingerprint), DTLS_FINGERPRINT_TYPE_SHA256);
  //fprintf(stderr, "rc:%d '%s'\n", rc, fingerprint);

  rc = dtls_ctx_init(&dtls, NULL, NULL);
  fprintf(stderr, "dtls_ctx_init:%d\n", rc);

  dtls_ctx_close(&dtls);

  return 0;
}
#endif // 0

#if 0
int testturn() {

  logger_SetLevel(S_DEBUG);
  logger_AddStderr(S_DEBUG, 0);

  int fdhighest = 0;
  fd_set fds;
  struct timeval tv;
  int have_relays[2];
  int rc;
  unsigned int idx;
  TIME_VAL tm0 = timer_GetTime();;
  unsigned char data[4096];
  TURN_THREAD_CTXT_T turnThread;
  memset(&turnThread, 0, sizeof(turnThread));
  pthread_mutex_init(&turnThread.mtx, NULL);
  pthread_mutex_init(&turnThread.notify.mtx, NULL);
  pthread_cond_init(&turnThread.notify.cond, NULL);

  TURN_CTXT_T turns[2];
  memset(turns, 0, sizeof(turns));
  STUN_CTXT_T stuns[2];
  memset(stuns, 0, sizeof(stuns));
  stuns[0].pTurn = &turns[0];
  stuns[1].pTurn = &turns[1];

  turns[0].saTurnSrv.sin_addr.s_addr = inet_addr("127.0.0.1");
  turns[0].saTurnSrv.sin_port = htons(3478);
  turns[0].integrity.pass = "pass1";
  turns[0].integrity.user = "user1";
  turns[0].integrity.realm = "localhost.com";
  memcpy(&turns[0].saPeerRelay, &turns[0].saTurnSrv, sizeof(turns[0].saPeerRelay));
  turns[0].saPeerRelay.sin_port = htons(1234);

  memcpy(&turns[1], &turns[0], sizeof(turns[1]));
  turns[1].saPeerRelay.sin_port = htons(4321);

  struct sockaddr_in saLocals[2];
  memset(saLocals, 0, sizeof(saLocals));
  saLocals[0].sin_addr.s_addr = INADDR_ANY;
  saLocals[0].sin_port = htons(10200);
  saLocals[1].sin_addr.s_addr = INADDR_ANY;
  saLocals[1].sin_port = htons(10201);

  have_relays[0] = have_relays[1] = 0;
  int numrelays = 1;

  struct sockaddr_in saFrom;
  socklen_t fromLen;

  NETIO_SOCK_T netsocks[2];
  netio_opensocket(&netsocks[0], SOCK_DGRAM, 0, 0, &saLocals[0]);
  net_setsocknonblock(NETIOSOCK_FD(netsocks[0]), 1);
  netio_opensocket(&netsocks[1], SOCK_DGRAM, 0, 0, &saLocals[1]);
  net_setsocknonblock(NETIOSOCK_FD(netsocks[1]), 1);

  fprintf(stderr, "turn_thread_start: %d\n", turn_thread_start(&turnThread));

  //data[0] = 0x7f; data[1] = 0x00; data[2] = 0x02; data[3] = 0xc5;
  //fprintf(stderr, "turn_ischanneldata: %d\n", turn_ischanneldata(data, 713));
  //return 0;
  //turn_relay_bindchannel(&turn);
  //fprintf(stderr, "channel: 0x%x\n", turn.reqArgChannelId);
  //return 0;

  while(1) {

    for(idx = 0; idx < numrelays; idx++) {
      if(have_relays[idx] == 0) {
        if((rc = turn_relay_setup(&turnThread, &turns[idx], &netsocks[idx], NULL)) < 0) {
          fprintf(stderr, "turn_relay_setup[%d] failed\n", idx);
          break;
        }
        have_relays[idx] = 1;
      }
    }

    tv.tv_sec = 0;
    tv.tv_usec = 500000;
   
    fdhighest=0; 
    fdhighest = MAX(fdhighest, (NETIOSOCK_FD(netsocks[0])));
    fdhighest = MAX(fdhighest, (NETIOSOCK_FD(netsocks[1])));
    FD_ZERO(&fds);
    FD_SET(NETIOSOCK_FD(netsocks[0]), &fds);
    FD_SET(NETIOSOCK_FD(netsocks[1]), &fds);

    if(select(fdhighest + 1, &fds, NULL, NULL, &tv) > 0) {
      for(idx = 0; idx < numrelays; idx++) {
        fromLen = sizeof(saFrom);
        if((rc = recvfrom(NETIOSOCK_FD(netsocks[idx]), data, sizeof(data), 0, (struct sockaddr *) &saFrom, &fromLen)) > 0) {
          fprintf(stderr, "recvfrom[%d] got len: %d\n", idx, rc);
          if(stuns[idx].pTurn->delme) {
            fprintf(stderr, "ignoring resp...\n");
          } else {
            rc = stun_onrcv(&stuns[idx], data, rc, &saFrom, &saLocals[idx], &netsocks[idx]); 
            //fprintf(stderr, "stun_onrcv rc: %d\n", rc);
          }
        }
      }
    }

    if(have_relays[0] == 1 && ((timer_GetTime() - tm0)/TIME_VAL_MS) > 4000) {
      fprintf(stderr, "calling turn_thread_stop\n");
      //stuns[0].pTurn->delme=1;
      turn_thread_stop(&turnThread);
      have_relays[0] = 2;
    }


  }

  fprintf(stderr, "main sleeping to exit...\n");
  sleep(5);

  return 0;
}
#endif // 0

#if 0
int teststun() {
  unsigned char c[4];

  logger_SetLevel(S_DEBUG);
  logger_AddStderr(S_DEBUG, 0);

/*
#define STUN_MSG_CLASS(t) ((((t) & 0x0100) >> 7) | (((t) & 0x0010) >> 4))
#define STUN_MSG_TYPE(t) ( (((t) & 0x3e00) >> 2) | ((t) & 0x00e0) >> 1) | (((t) & 0x000f))
  c[0] = 0x03;
  c[1] = 0x11;
  uint16_t type = ntohs( *((uint16_t *) &c[0]));
  fprintf(stderr, "type: 0x%x, 0x%x 0x%x, class:0x%x, type:0x%x\n", type, c[0], c[1], STUN_MSG_CLASS(type), STUN_MSG_TYPE(type) );
*/
/*
  c[0] = 0xc8;
  c[1] = 0x23;
  uint16_t port = ntohs( *((uint16_t *) &c[0]));
  port ^= (STUN_COOKIE >> 16);
  fprintf(stderr, "PORT xor :0x%x %d\n", port, port);

  c[0] = 0xe1;
  c[1] = 0xba;
  c[2] = 0xa6;
  c[3] = 0x37;
  struct in_addr addr4xor;
  memset(&addr4xor, 0, sizeof(addr4xor));
  addr4xor.s_addr =  *((uint32_t *) &c[0]);
  addr4xor.s_addr = htonl(htonl(addr4xor.s_addr) ^ STUN_COOKIE);
  fprintf(stderr, "IPv4 xor: %s\n", inet_ntoa(addr4xor));

  c[0] = 0xe9;
  c[1] = 0x31;
  port = ntohs( *((uint16_t *) &c[0]));
  fprintf(stderr, "PORT:0x%x %d\n", port, port);


  c[0] = 0xc0;
  c[1] = 0xa8;
  c[2] = 0x02;
  c[3] = 0x75;
  memset(&addr4xor, 0, sizeof(addr4xor));
  addr4xor.s_addr =  *((uint32_t *) &c[0]);
  fprintf(stderr, "IPv4  %s\n", inet_ntoa(addr4xor));
*/

/*
unsigned char hash[20];
//unsigned char x[] = "test\n";
unsigned char x[] = "user:realm:pass";
md5_calculate(x, strlen(x), hash);
avc_dumpHex(stderr, hash, 16, 1);
*/


STUN_MESSAGE_INTEGRITY_PARAM_T hmac;
memset(&hmac, 0, sizeof(hmac));


/*
unsigned char msg[] = {
0x00,0x01,0x00,0x60,0x21,0x12,0xa4,0x42,0x4f,0x65,0x55,0x4b,0x31,0x54,0x6c,0x70
,0x48,0x66,0x58,0x78,0x00,0x06,0x00,0x21,0x48,0x6c,0x5a,0x52,0x64,0x74,0x4a,0x69
,0x64,0x74,0x67,0x34,0x44,0x64,0x4e,0x5a,0x3a,0x34,0x79,0x62,0x62,0x50,0x64,0x52
,0x6a,0x62,0x2f,0x41,0x4d,0x35,0x31,0x30,0x66,0x00,0x00,0x00,0x80,0x2a,0x00,0x08
,0xc1,0xf8,0x0a,0xa5,0x76,0xd9,0x7b,0xec,0x00,0x25,0x00,0x00,0x00,0x24,0x00,0x04
,0x6e,0x00,0x1e,0xff,0x00,0x08,0x00,0x14,0x72,0xd1,0x43,0x3f,0xb2,0x19,0x98,0x67
,0xfd,0xea,0xab,0x4b,0x6d,0xb6,0xc9,0x3e,0x28,0x5f,0x11,0xa8,0x80,0x28,0x00,0x04
,0xb5,0x08,0xa3,0xaa};
*/

hmac.pass="mdJhQBDy3X4r2bkmm2jbQ6zP";
unsigned char msg[] = {
0x00,0x01,0x00,0x6c,0x21,0x12,0xa4,0x42,0x43,0x0e,0x00,0x00,0xa9,0xc9,0x63,0x55,
0x0a,0x35,0x55,0x76,0x00,0x24,0x00,0x04,0x7e,0xff,0xff,0xff,0x00,0x25,0x00,0x00,
0x80,0x2a,0x00,0x08,0x58,0x5d,0x95,0xb5,0x68,0x0f,0xcb,0x65,0x80,0x22,0x00,0x10,
0x42,0x72,0x69,0x61,0x20,0x69,0x4f,0x53,0x20,0x32,0x2e,0x34,0x2e,0x30,0x00,0x00,
0x00,0x06,0x00,0x19,0x66,0x6e,0x33,0x36,0x49,0x63,0x67,0x4b,0x45,0x44,0x6f,0x6f,
0x74,0x66,0x48,0x32,0x3a,0x36,0x34,0x66,0x32,0x38,0x39,0x34,0x32,0x00,0x00,0x00,
0x00,0x08,0x00,0x14,0x9f,0x39,0x0b,0x87,0x49,0x01,0xe8,0xed,0x4d,0x5a,0x36,0xe4,
0xeb,0x4c,0x09,0xee,0x88,0x8f,0x94,0x7c,0x80,0x28,0x00,0x04,0x81,0xd7,0x8d,0xae};


/*
//RFC 5769 Sample IPv4 Request
hmac.pass="VOkJxbRl1RmTxUk/WvJxBt";
unsigned char msg[] = {
 0x00,0x01,0x00,0x58,0x21,0x12,0xa4,0x42,0xb7,0xe7,0xa7,0x01,0xbc,0x34,0xd6,0x86
,0xfa,0x87,0xdf,0xae,0x80,0x22,0x00,0x10,0x53,0x54,0x55,0x4e,0x20,0x74,0x65,0x73
,0x74,0x20,0x63,0x6c,0x69,0x65,0x6e,0x74,0x00,0x24,0x00,0x04,0x6e,0x00,0x01,0xff
,0x80,0x29,0x00,0x08,0x93,0x2f,0xf9,0xb1,0x51,0x26,0x3b,0x36,0x00,0x06,0x00,0x09
,0x65,0x76,0x74,0x6a,0x3a,0x68,0x36,0x76,0x59,0x20,0x20,0x20,0x00,0x08,0x00,0x14
,0x9a,0xea,0xa7,0x0c,0xbf,0xd8,0xcb,0x56,0x78,0x1e,0xf2,0xb5,0xb2,0xd3,0xf2,0x49
,0xc1,0xb5,0x71,0xa2,0x80,0x28,0x00,0x04,0xe5,0x7a,0x3b,0xcf};
*/

/*
//RFC 5769 Sample IPv4 Response
hmac.pass="VOkJxbRl1RmTxUk/WvJxBt";
unsigned char msg[] = {
 0x01,0x01,0x00,0x3c,0x21,0x12,0xa4,0x42,0xb7,0xe7,0xa7,0x01,0xbc,0x34,0xd6,0x86
,0xfa,0x87,0xdf,0xae,0x80,0x22,0x00,0x0b,0x74,0x65,0x73,0x74,0x20,0x76,0x65,0x63
,0x74,0x6f,0x72,0x20,0x00,0x20,0x00,0x08,0x00,0x01,0xa1,0x47,0xe1,0x12,0xa6,0x43
,0x00,0x08,0x00,0x14,0x2b,0x91,0xf5,0x99,0xfd,0x9e,0x90,0xc3,0x8c,0x74,0x89,0xf9
,0x2a,0xf9,0xba,0x53,0xf0,0x6b,0xe7,0xd7,0x80,0x28,0x00,0x04,0xc0,0x7d,0x4c,0x96};
*/


STUN_MSG_T stun;
memset(&stun, 0, sizeof(stun));
fprintf(stderr, "stun_parse rc:%d\n",  stun_parse(&stun, msg, sizeof(msg)));
stun_dump(stderr, &stun);

unsigned char buf[512];
int rc = stun_serialize(&stun, buf, sizeof(buf), &hmac);
fprintf(stderr, "stun_serialize:%d\n", rc);
if(rc>0) avc_dumpHex(stderr, buf, rc, 1);

fprintf(stderr, "stun_validate_crc:%d\n", stun_validate_crc(msg, sizeof(msg)));
fprintf(stderr, "stun_validate_hmac:%d\n", stun_validate_hmac(msg, sizeof(msg), &hmac));


/*
struct sockaddr_in saRemote;
memset(&saRemote, 0, sizeof(saRemote));
saRemote.sin_addr.s_addr = inet_addr("10.0.2.3");
saRemote.sin_port = htons(1234);
struct sockaddr_in saLocal;
memset(&saLocal, 0, sizeof(saLocal));
saLocal.sin_addr.s_addr = INADDR_ANY;
saLocal.sin_port = htons(5004);
STUN_SOCKET socket;
STUNSOCK_FD(socket) = 1;
STUN_CTXT_T stunRcvr;
memset(&stunRcvr, 0, sizeof(stunRcvr));
stunRcvr.integrity.pass = hmac.pass;

fprintf(stderr, "stun_onrcv: %d\n", stun_onrcv(&stunRcvr, msg, sizeof(msg), &saRemote, &saLocal, &socket));
*/


/*
unsigned char hash[16];
char *s = "user1:127.0.0.1:user1";
md5_calculate(s, strlen(s), hash);
avc_dumpHex(stderr, hash, 16, 1);
*/

  return 0;
}
#endif // 0

#if 0
int test64() {
  unsigned char c[8];
  uint64_t ui64;


  c[0] = 0x01; c[1] = 0x02; c[2] = 0x03; c[3] = 0x04;
  c[4] = 0x05; c[5] = 0x06; c[6] = 0x07; c[7] = 0x08;

  ui64 = ((((uint64_t)c[0]) << 56) | (((uint64_t)c[1]) << 48) | (((uint64_t)c[2]) << 40) | (((uint64_t)c[3]) << 32) | (((uint64_t)c[4]) << 24) | (((uint64_t)c[5]) << 16) | ((uint64_t)c[6]) << 8 | ((uint64_t)c[7]));
  fprintf(stderr, "0x%llx, %llu, 0x%llx\n", ui64, ui64, *((uint64_t *) c)); 

  ui64 = htonl64(*((uint64_t *) &c[0]));
  fprintf(stderr, "0x%llx, %llu, 0x%llx\n", ui64, ui64, *((uint64_t *) c)); 

  ui64 = HTONL64(*((uint64_t *) &c[0]));
  fprintf(stderr, "0x%llx, %llu, 0x%llx\n", ui64, ui64, *((uint64_t *) c)); 

  ui64 = hton_dbl(*((uint64_t *)&c[0]));
  fprintf(stderr, "0x%llx, %llu, 0x%llx\n", ui64, ui64, *((uint64_t *) c)); 

  return 0;
}
#endif // 0

#if 0

int testaac() {
  unsigned char buf[] = { 0x13, 0x10, 0x56, 0xe5, 0x9d, 0x48, 0x00 };
  //unsigned char buf[] = { 0x13, 0x88 };
  unsigned char buf2[32];
  ESDS_DECODER_CFG_T sp;
  int rc;

  memset(&sp, 0, sizeof(sp));

  if((rc = esds_decodeDecoderSp(buf, 7, &sp)) < 0) {
    fprintf(stderr, "esds_decodeDecoderSp failed\n");
  }
  fprintf(stderr, "init:%d\n", sp.init);
  fprintf(stderr, "obj:%d %s\n", sp.objTypeIdx, esds_getMp4aObjTypeStr(sp.objTypeIdx));
  fprintf(stderr, "freq:%d %dHz\n", sp.freqIdx, esds_getFrequencyVal(sp.freqIdx));
  fprintf(stderr, "extType:%d, %d\n", sp.haveExtTypeIdx, sp.extObjTypeIdx);
  fprintf(stderr, "extFreq:%d, %d %dHz\n", sp.haveExtFreqIdx, sp.extFreqIdx, esds_getFrequencyVal(sp.extFreqIdx));
  fprintf(stderr, "chan:%d %s\n", sp.channelIdx, esds_getChannelStr(sp.channelIdx));
  fprintf(stderr, "extFlag:%d\n", sp.extFlag);
  fprintf(stderr, "frameDeltaHz:%d\n", sp.frameDeltaHz);
  fprintf(stderr, "dependsOnCoreCoderFlag:%d\n", sp.dependsOnCoreCoderFlag);

  if((rc = esds_encodeDecoderSp(buf2, sizeof(buf2), &sp)) < 0) {
    fprintf(stderr, "esds_encodeDecoderSp failed\n");
  }
  fprintf(stderr, "esds_encodeDecoderSp:\n"); avc_dumpHex(stderr, buf2, rc, 1);

  if((rc = esds_decodeDecoderSp(buf2, rc, &sp)) < 0) {
    fprintf(stderr, "esds_decodeDecoderSp failed\n");
  }
  fprintf(stderr, "init:%d\n", sp.init);
  fprintf(stderr, "obj:%d %s\n", sp.objTypeIdx, esds_getMp4aObjTypeStr(sp.objTypeIdx));
  fprintf(stderr, "freq:%d %dHz\n", sp.freqIdx, esds_getFrequencyVal(sp.freqIdx));
  fprintf(stderr, "extType:%d, %d\n", sp.haveExtTypeIdx, sp.extObjTypeIdx);
  fprintf(stderr, "extFreq:%d, %d %dHz\n", sp.haveExtFreqIdx, sp.extFreqIdx, esds_getFrequencyVal(sp.extFreqIdx));
  fprintf(stderr, "chan:%d %s\n", sp.channelIdx, esds_getChannelStr(sp.channelIdx));
  fprintf(stderr, "extFlag:%d\n", sp.extFlag);
  fprintf(stderr, "frameDeltaHz:%d\n", sp.frameDeltaHz);
  fprintf(stderr, "dependsOnCoreCoderFlag:%d\n", sp.dependsOnCoreCoderFlag);

  return 0;
}

#endif // 0

#if 0
void burstmeter_dump(const BURSTMETER_SAMPLE_SET_T *pSamples);
int testburstmeter() {
  BURSTMETER_SAMPLE_SET_T bm;
  struct timeval tv, tv0;

  //memset(&bm, 0, sizeof(bm));
  burstmeter_init(&bm, 5000/5, 5000);

  gettimeofday(&tv, NULL);
  TIME_TV_SET(tv0, tv);
  fprintf(stderr, "start time: %u.%u\n", tv0.tv_sec, tv0.tv_usec);
  burstmeter_AddSample(&bm, 1200, &tv);
  TV_INCREMENT_MS(tv, 999); burstmeter_AddSample(&bm, 1201, &tv);
  TV_INCREMENT_MS(tv, 1000); burstmeter_AddSample(&bm, 1202, &tv);
  TV_INCREMENT_MS(tv, 1000); burstmeter_AddSample(&bm, 1203, &tv);
  TV_INCREMENT_MS(tv, 1000); burstmeter_AddSample(&bm, 1204, &tv);
  TV_INCREMENT_MS(tv, 1000); burstmeter_AddSample(&bm, 1205, &tv);
  TV_INCREMENT_MS(tv, 1000); burstmeter_AddSample(&bm, 1206, &tv);
  fprintf(stderr, "end time: %u.%u, %d ms\n", tv.tv_sec, tv.tv_usec, TIME_TV_DIFF_MS(tv,tv0));

  burstmeter_dump(&bm);
  fprintf(stderr, "%.3f pkt/s, %.3f b/s\n", burstmeter_getPacketratePs(&bm), burstmeter_getBitrateBps(&bm));

  //TV_INCREMENT_MS(tv, 990);
  //burstmeter_updateCounters(&bm, &tv);
  //fprintf(stderr, "%.3f pkt/s, %.3f b/s\n", burstmeter_getPacketratePs(&bm), burstmeter_getBitrateBps(&bm));

  burstmeter_close(&bm);
  
  return 0;
}
#endif // 0

#if 0

const struct ABR_CONFIG *find_abr_config(int w, int h, int bps);

int testabr() {
  
  find_abr_config(320, 240, 161);

  return 0;
}

#endif // 0

#if 0
int testoutfmt() {
  STREAMER_OUTFMT_LIST_T outfmtlist;
  OUTFMT_FRAME_DATA_T frame;
  unsigned char buf[4096];

  logger_SetLevel(S_DEBUG);
  logger_AddStderr(S_DEBUG, 0);

  memset(&outfmtlist, 0, sizeof(outfmtlist));
  memset(&frame, 0, sizeof(frame));
  OUTFMT_DATA(&frame) = buf;

  outfmt_init(&outfmtlist);

  frame.isvid= 0; frame.isaud = 1;
  OUTFMT_PTS(&frame) = 90000;
  OUTFMT_LEN(&frame) = 12;
  outfmt_invokeCbs(&outfmtlist, &frame);

  frame.isvid= 0; frame.isaud = 1;
  OUTFMT_PTS(&frame) = 180000;
  OUTFMT_LEN(&frame) = 13;
  outfmt_invokeCbs(&outfmtlist, &frame);

  frame.isvid = 1; frame.isaud = 0;
  OUTFMT_PTS(&frame) = 182000;
  OUTFMT_LEN(&frame) = 32;
  outfmt_invokeCbs(&outfmtlist, &frame);

  frame.isvid = 0; frame.isaud = 1;
  OUTFMT_PTS(&frame) = 270000;
  OUTFMT_LEN(&frame) = 14;
  outfmt_invokeCbs(&outfmtlist, &frame);

  frame.isvid = 0; frame.isaud = 1;
  OUTFMT_PTS(&frame) = 271000;
  OUTFMT_LEN(&frame) = 14;
  outfmt_invokeCbs(&outfmtlist, &frame);

  frame.isvid = 0; frame.isaud = 1;
  OUTFMT_PTS(&frame) = 272000;
  OUTFMT_LEN(&frame) = 14;
  outfmt_invokeCbs(&outfmtlist, &frame);

  frame.isvid = 1; frame.isaud = 0;
  OUTFMT_PTS(&frame) = 183000;
  OUTFMT_LEN(&frame) = 33;
  outfmt_invokeCbs(&outfmtlist, &frame);


  frame.isvid= 0; frame.isaud = 1;
  OUTFMT_PTS(&frame) = 273000;
  OUTFMT_LEN(&frame) = 15;
  outfmt_invokeCbs(&outfmtlist, &frame);

  frame.isvid= 1; frame.isaud = 0;
  OUTFMT_PTS(&frame) = 271000;
  OUTFMT_LEN(&frame) = 34;
  outfmt_invokeCbs(&outfmtlist, &frame);

  frame.isvid= 0; frame.isaud = 1;
  OUTFMT_PTS(&frame) = 276000;
  OUTFMT_LEN(&frame) = 36;
  outfmt_invokeCbs(&outfmtlist, &frame);

  frame.isvid= 1; frame.isaud = 0;
  OUTFMT_PTS(&frame) = 274000;
  OUTFMT_LEN(&frame) = 35;
  outfmt_invokeCbs(&outfmtlist, &frame);


  outfmt_close(&outfmtlist);

  return 0;
}
#endif // 0

#if 0
int testremb() {
  uint32_t i;
  unsigned char uc[24];

  uc[16] = 0x01;
  uc[17] = 0x0a; uc[18] = 0x10; uc[19] = 0xf6;
  //uc[17] = 0x07; uc[18] = 0xf9; uc[19] = 0xdd;

  uint8_t brExp = uc[17] >> 2;
  uint32_t brMantissa = ((uc[17] & 0x03) << 16) | (uc[18] << 8) | uc[19];
  //brMantissa = htonl(brMantissa);
  fprintf(stderr, "REMB brExp: 0x%x, brMantissa: 0x%x, %d, buf: 0x%x 0x%x 0x%x\n", brExp, brMantissa, brMantissa, uc[17], uc[18], uc[19]);

  uint64_t br = brMantissa << brExp;
  brExp = 0;
  for(i=0; i<64; i++) {       
    if(br <= ((uint32_t)262143 << i)) {   
      brExp = i;
      break;
    }
  }    
  brMantissa = (br >> brExp);
  uc[17]=(uint8_t)((brExp << 2) + ((brMantissa >> 16) & 0x03));
  uc[18]=(uint8_t)(brMantissa >> 8);
  uc[19]=(uint8_t)(brMantissa);
  fprintf(stderr, "BR:%lld,  REMB brExp: 0x%x, brMantissa: 0x%x, %d, buf: 0x%x 0x%x 0x%x\n", br, brExp, brMantissa, brMantissa, uc[17], uc[18], uc[19]);

  return 0;
}
#endif // 0

#if 0

static unsigned char *pdelmeaud;
static unsigned int delmeaudsz;
static unsigned char *pdelmevid;
static unsigned int delmevidsz;

static void *test_producevid(void *pArg) {
  VSXLIB_STREAM_PARAMS_T *p = (VSXLIB_STREAM_PARAMS_T *) pArg;
  TIME_VAL tvV, tvA, tv, tv0;
  unsigned int idx = 0;
  TIME_VAL tvFps;
  unsigned int vframes = 0;
  int sz, sztmp;


  tvFps = tv0 = tvV = tvA = timer_GetTime();

  while(1) {

    tv = timer_GetTime();
    if((tv - tvV) / TIME_VAL_MS > 47) {

      if((tv - tv0) / TIME_VAL_MS > 1000) {
        if(vsxlib_onVidFrame(p, pdelmevid, delmevidsz) < 0) {
          break;
        }
        vframes++;
      }

      if((tv - tvFps) / TIME_VAL_MS > 1000) {
         
        fprintf(stderr, "%lld vid input: %.3f fps\n", tv / TIME_VAL_MS, (float) vframes * 1000000.f / (tv - tvFps));
        vframes = 0;
        tvFps = tv;
      }

      tvV = tv;
    }
    if((tv - tvA) / TIME_VAL_MS > 50) {
      sz = (int)(8000 * 2 * ((double)(tv - tvA) / TIME_VAL_US)) & ~0x01;
      //fprintf(stderr, "%lld aud read sz: %d (%lld)\n", tv / TIME_VAL_MS, sz, (tv - tvA) / TIME_VAL_MS); 
      if((sztmp = sz) >  delmeaudsz - idx) {
        sztmp = delmeaudsz - idx;
      }
      vsxlib_onAudSamples(p, &pdelmeaud[idx], sztmp);
      idx += sztmp;
      if(sztmp < sz) {
        fprintf(stderr, "looping pcm audio\n");
        idx = 0; 
        sztmp = sz - sztmp;
        if(vsxlib_onAudSamples(p, &pdelmeaud[idx], sztmp) < 0) {
          break; 
        }
        idx += sztmp;
      }
      tvA = tv;
    }


    usleep(1000);
  }

  fprintf(stderr, "test_producevid thread exiting\n"); 
  return NULL;
}

static void *test_consumevid(void *pArg) {
  VSXLIB_STREAM_PARAMS_T *p = (VSXLIB_STREAM_PARAMS_T *) pArg;
  unsigned char *buf;
  struct timeval tv;
  int rc;
  unsigned int sz = 320 * 240 * 2;

  buf = (unsigned char *) malloc(sz);

  while(1) {

    if((rc = vsxlib_readVidFrame(p, buf, sz, &tv)) < 0) {
      if(rc == -2) {
        usleep(20000);
      } else if(rc == -1) {
        break;
      }
    } else {
      fprintf(stderr, "vsxlib_readV rc:%d, tm:%d:%d at %llu\n", rc, tv.tv_sec, tv.tv_usec, timer_GetTime());
    }

  }

  return NULL;
}

static void *test_consumeaud(void *pArg) {
  VSXLIB_STREAM_PARAMS_T *p = (VSXLIB_STREAM_PARAMS_T *) pArg;
  unsigned char *buf;
  struct timeval tv;
  int rc;
  unsigned int sz = 16000 * 2;

  buf = (unsigned char *) malloc(sz);
  

  while(1) {

    if((rc = vsxlib_readAudSamples(p, buf, sz/10, &tv)) < 0) {
      if(rc == -2) {
        usleep(20000);
      } else if(rc == -1) {
        break;
      }
    } else {
      usleep(1000);
    }
    //fprintf(stderr, "vsxlib_readA rc:%d, tm:%d:%d at %llu\n", rc, tv.tv_sec, tv.tv_usec, timer_GetTime());

  }

  return NULL;
}

unsigned char *loadfile(const char *path, unsigned int *plenout) {
  int fd;
  int rc;
  unsigned int idx = 0;
  int readsz;
  struct stat st;
  unsigned char *p;

  if((fd = open(path, O_RDONLY)) <= 0 || fstat(fd, &st) || st.st_size <= 0) {
    fprintf(stderr, "%s not found\n", path);
    return NULL;
  }

  *plenout = st.st_size;
  p = malloc(st.st_size);

  while(idx < st.st_size) {
    if((readsz = (st.st_size - idx)) > SSIZE_MAX) {
      readsz = SSIZE_MAX;
    }
    if((rc = read(fd, &p[idx], readsz)) < readsz) {
    }
    idx += rc;
  }

  close(fd);
  fprintf(stderr, "read %d / %lld from %s\n", idx, st.st_size, path);

  return p;
}

const char *testsdp() {
  static *p="sdp://v=0\n"
            "o=- 756261453 1299615730 IN IP4 192.168.1.151\n"
            "s=\n"
            "t=0 0\n"
            "c=IN IP4 0.0.0.0\n"
            "m=video 10000 RTP/AVP 97\n"
            "a=rtpmap:97 MP4V-ES/1000\n"
            "a=fmtp:97 profile-level-id=1;config=000001B003000001B509000001000000012000844005282C2090A31F;\n"
            "m=audio 10002 RTP/AVP 96\n"
            "a=rtpmap:96 AMR/8000/1\n"
            "a=fmtp:96 octet-align=1;\n";
  return p;
}


int testvsxlib() {
  pthread_t ptd, ptd2, ptd3;
  pthread_attr_t attr, attr2, attr3;
  VSXLIB_STREAM_PARAMS_T params;


  if(!(pdelmeaud = loadfile("test.pcm", &delmeaudsz)) ||
     !(pdelmevid = loadfile("test.yuv", &delmevidsz))) {
    return -1;
  }

//while(1) {

  vsxlib_open(&params);

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&ptd, &attr, test_producevid, &params);

  params.verbosity = VSX_VERBOSITY_HIGH;
  //params.verbosity = VSX_VERBOSITY_HIGHEST + 1;
  params.tslive="8080";
  params.output="rtp://192.168.1.2:5004";
  //params.transproto="rtp";
  params.inputs[0] = "/dev/dummyvideo";
  params.strfilters[0] = "type=yuv420sp,size=480x724";
  params.inputs[1] = "/dev/dummyaudio";
  params.strfilters[1] = "type=pcm,clock=8000,channels=1";
  params.inputs[0] = testsdp();
  params.strfraudqueuesz="20";
  params.strxcode="vc=h264,vb=400,x=240,y=300,fn=15,fd=1,vf=1,vl=1,th=1,vcfrout=-1,vgmin=500,vgmax=1000,vp=66,ac=aac,ab=8000,ar=8000,as=1";
  //params.streamflags |= VSX_STREAMFLAGS_PREFERAUDIO;
  //params.streamflags |=VSX_STREAMFLAGS_AV_SAME_START_TM;

 
/*
  vsxlib_open(&params);

  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);
  pthread_create(&ptd, &attr, test_consumevid, &params);

  pthread_attr_init(&attr2);
  pthread_attr_setdetachstate(&attr2, PTHREAD_CREATE_DETACHED);
  pthread_create(&ptd2, &attr2, test_consumeaud, &params);


  params.verbosity = VSX_VERBOSITY_HIGH;
  params.tslive="8080";
  params.islive=1;
  params.inputs[0] = "rtp://0.0.0.0:5004";
  params.strfilters[0] = "type=m2t";
  //params.strxcode="vc=rgb565,x=320,y=240,fn=15,fd=1,ac=pcm,ar=16000,as=1,sc=1";
  //params.outaudbufdurationms = 200;
  //params.novid = 1;
  //params.noaud = 0;
*/

/*
  params.verbosity = VSX_VERBOSITY_HIGH;
  //params.verbosity++;
  params.tslive="8080";
  params.islive=1;
  params.inputs[0] = "rtp://0.0.0.0:5004,5006";
  params.strfilters[0] = "type=h264,clock=24,dstport=5004";
  //params.inputs[1] = "rtp://0.0.0.0:5006,dstport=5006";
  params.strfilters[1] = "type=aac,clock=44100,channels=2";
  params.strxcode="vc=rgb565,x=320,y=240,ac=pcm,ar=16000,as=1,sc=1";
  //params.outaudbufdurationms = 200;
  //params.novid = 1;
  //params.noaud = 0;
*/


  if(params.verbosity > VSX_VERBOSITY_NORMAL) {
    logger_SetLevel(S_DEBUG);
    logger_AddStderr(S_DEBUG, 0);
  }

  fprintf(stderr, "vsxlib_stream rc:%d\n", vsxlib_stream(&params));

  fprintf(stderr, "vsxlib_close\n");
  vsxlib_close(&params);

//};

  return 0;
}

#endif  // 0

#if 0
int testparse() {

  logger_SetLevel(S_DEBUG);
  logger_AddStderr(S_DEBUG, 0);
  struct sockaddr_in sain;

  int rc = capture_parseAddr("127.0.0.1:0", &sain, 0);
  fprintf(stderr, "rc:%d, %s:%d\n", rc, inet_ntoa(sain.sin_addr), htons(sain.sin_port));

  return 0;
}
#endif // 0

#if 0
static double rtmp_hton_dbl(const unsigned char *arg) {
  double d;
  uint32_t ui = *((uint32_t *)arg);
  uint32_t ui2 =  *((uint32_t *) &((arg)[4]));

  *((uint32_t *)&d) = htonl(ui2);
  *((uint32_t *)&(((unsigned char *)&d)[4])) = htonl(ui);

  return  d;
}
#endif // 0

#if 0

static int testauth_basic() {
  int rc;
  char buf[128];
  char user[64];
  char pass[64];

  memset(buf, 0, sizeof(buf));
  //rc = base64_decode("QWxhZGRpbjpvcGVuIHNlc2FtZQ==", buf, sizeof(buf));
  //avc_dumpHex(stderr, buf, 64, 1);
  //fprintf(stderr, "rc:%d, buf:'%s'\n", rc, buf);

  memset(buf, 0, sizeof(buf));
  //rc = auth_basic_encode("Aladdin", "open sesame", (char *) buf, sizeof(buf));
  rc = auth_basic_encode("Aladdin", "open sesame", (char *) buf, sizeof(buf));
  fprintf(stderr, "rc:%d, buf:'%s'\n", rc, buf);

  memset(user, 0, sizeof(user));
  memset(pass, 0, sizeof(pass));
  rc = auth_basic_decode((char *) buf, user, 64, pass, 12); 

  fprintf(stderr, "--rc:%d, user:'%s', pass: '%s'\n", rc, user, pass);

  return 0;
}

int testauth_digest_parse(const char *line, AUTH_LIST_T *pAuthList);
static int testauth_digest() {
  int idx;
  int rc = 0;
  AUTH_LIST_T authList;
  char buf[512];

  vsxlib_initlog(VSX_VERBOSITY_HIGH, "stderr", NULL, NULL, 0, 0);
  fprintf(stderr, "sslutil_init: %d\n", sslutil_init());

  memset(&authList, 0, sizeof(authList));
  sprintf(buf, " Digest username=\"Mufasa\", \
realm=\"testrealm@,host.com\", \
nonce=\"dcd98b7102dd2f0e8b11d0f600bfb0c093\", \
uri=\"/dir/index.html\", \
qop=auth, \
nc=00000001, \
cnonce=\"0a4f113b\", \
response=\"6629fae49393a05397450978507c4ef1\", \
opaque=\"5ccc069c403ebaf9f0171e9517f40e41\"");
  rc = auth_digest_parse(buf, &authList);

  fprintf(stderr, "rc: %d\n", rc);
  for(idx = 0; idx < AUTH_LIST_MAX_ELEMENTS; idx++) {
    fprintf(stderr, "list[%d] '%s'->'%s' obj: 0x%x, pnext: 0x%x\n", idx, authList.list[idx].key, authList.list[idx].val, &authList.list[idx], authList.list[idx].pnext);
  }

  buf[0] = '\0';
  rc = auth_digest_write(&authList, buf, sizeof(buf));
  fprintf(stderr, "rc: %d authList.count:%d, '%s'\n", rc, authList.count, buf);

  buf[0] = '\0';
  auth_digest_gethexrandom(16, buf, sizeof(buf));
  fprintf(stderr, "random: '%s'\n", buf);

  char h1[512];
  auth_digest_getH1("Mufasa", "testrealm@host.com", "Circle Of Life", h1, sizeof(h1));
  fprintf(stderr, "h1: '%s'\n", h1);
  char h2[512];
  auth_digest_getH2("GET", "/dir/index.html", h2, sizeof(h2));
  fprintf(stderr, "h2: '%s'\n", h1);
  char *nonce = "dcd98b7102dd2f0e8b11d0f600bfb0c093";
  //rc = auth_digest_getDigest(h1, nonce, NULL, NULL, NULL, h2, buf, sizeof(buf));
  rc = auth_digest_getDigest(h1, nonce, "00000001", "0a4f113b", "auth", h2, buf, sizeof(buf));
  fprintf(stderr, "digest rc:%d : '%s'\n", rc, buf);

  
  return 0;
}

#endif // 0

#if 0
int testauth_parseinit(const char *optarg) {

  const char *p = optarg ? optarg : "aa:bb";
  AUTH_CREDENTIALS_STORE_T auth;
  memset(&auth, 0, sizeof(auth));

  int rc = capture_parseAuthUrl(&p, &auth);

  fprintf(stderr, "rc: %d, auth.usename: '%s', pass: '%s', realm: '%s', p: '%s'\n", rc, auth.username, auth.pass, auth.realm, p);
  return 0;
}

#endif // 0

#if 0

int get_requested_path(const char *uriPrefix, const char *puri, const char **ppargpath, const char **ppargrsrc);

int testoutfmt_parse(const char *optarg) {

  int rc = 0;
  char uri[512];
  char *puri = uri;
  const char *pend = NULL;
  const char *pargsrc = NULL;
  const char *pargpath = NULL;
  sprintf(uri, "/httplive/5/d3/id_xyz/a.mp4/prof_2");

  if(optarg && optarg[0] != '\0') {
    puri = (char *) optarg;
  }
  //rc = outfmt_getoutidx(url, &pend);
  //fprintf(stderr, "url: '%s', rc: %d, pend: '%s'\n", uri, rc, pend);

  rc = get_requested_path(VSX_HTTPLIVE_URL, puri, &pargpath, &pargsrc);
  fprintf(stderr, "rc: %d, uri: '%s', pargpath: '%s', pargsrc: '%s'\n", rc, puri, pargpath, pargsrc);

  return 0;
}

#endif // 0

int runtests(const char *optarg) {

  logger_SetLevel(S_DEBUG);
  logger_AddStderr(S_DEBUG, 0);

  //testauth_parseinit(optarg);
  //testauth_digest();
  //testoutfmt_parse(optarg);
  return 0;
}
