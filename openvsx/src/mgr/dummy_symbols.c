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


//
// Fulfill linker definitions for functionality that should not be present in mgr build
//

//stream/stream_abr.c
int stream_abr_cbOnStream(struct STREAM_STATS *pStreamStats) {
  return 0;
}

void stream_abr_close(STREAM_ABR_T *pAbr) {
}

//stream/stream_monitor.c
int stream_monitor_start(STREAM_STATS_MONITOR_T *pMonitor,
                         const char *statfilepath,
                         unsigned int intervalMs) {
  return 0;
}
int stream_monitor_stop(STREAM_STATS_MONITOR_T *pMonitor) {
  return 0;
}
STREAM_STATS_T *stream_monitor_createattach(STREAM_STATS_MONITOR_T *pMonitor,
                                            const struct sockaddr *psaRemote,
                                            STREAM_METHOD_T method,
                                            STREAM_MONITOR_ABR_TYPE_T abrEnabled) {
  return 0;
}
int stream_stats_destroy(STREAM_STATS_T **ppStats, pthread_mutex_t *pmtx) {
  return 0;
}

//stream/stream_srtp.c
int srtp_loadKeys(const char *in,  SRTP_CTXT_T *pCtxtV, SRTP_CTXT_T *pCtxtA) {
  return 0;
}

const char *srtp_streamTypeStr(SRTP_AUTH_TYPE_T authType, SRTP_CONF_TYPE_T confType, int safe) {
  static const char *s_srtpStreamTypes[] = { "" };

  return s_srtpStreamTypes[0];
}

int srtp_streamType(const char *streamTypeStr, SRTP_AUTH_TYPE_T *pAuthType, SRTP_CONF_TYPE_T *pConfType) {
  return 0;
}

int srtp_createKey(SRTP_CTXT_T *pCtxt) {
  return 0;
}

int srtp_sendto(const NETIO_SOCK_T *pnetsock,
                void *buffer,
                size_t length,
                int flags,
                const struct sockaddr *pdest_addr,
                const SRTP_CTXT_T *pSrtp,
                enum SENDTO_PKT_TYPE pktType,
                int no_protect) {
  return 0;
}

// stream/stream_dtls.c
int dtls_str2fingerprint(const char *str, DTLS_FINGERPRINT_T *pFingerprint) {
  return 0;
}

char *dtls_fingerprint2str(const DTLS_FINGERPRINT_T *pFingerprint, char *buf, unsigned int len) {
  return buf;
}

int dtls_get_cert_fingerprint(const char *certPath, DTLS_FINGERPRINT_T *pFingerprint) {
  return 0;
}

