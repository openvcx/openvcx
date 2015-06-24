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

#if defined(VSX_HAVE_SSL_DTLS) && defined(VSX_HAVE_SSL)

#undef ptrdiff_t
#include "openssl/ssl.h"
#include "openssl/err.h"

#define DTLS_SRTP_PROFILES_AUTH_CONF  SRTP_PROFILE_AES128_CM_SHA1_80":"SRTP_PROFILE_AES128_CM_SHA1_32
//#define DTLS_SRTP_PROFILES_AUTH  SRTP_PROFILE_NULL_HMAC_SHA1_80":"SRTP_PROFILE_NULL_HMAC_SHA1_32
#define DTLS_SRTP_PROFILE  DTLS_SRTP_PROFILES_AUTH_CONF

#define DTLS_CIPHER_LIST  "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH"
//#define DTLS_CIPHER_LIST  "ALL"

#define DTLS_SRTP_KEYS_LABEL "EXTRACTOR-dtls_srtp"


static const EVP_MD *dtlsFingerprintTypeToEVP(DTLS_FINGERPRINT_TYPE_T type) {
  switch(type) {
    case DTLS_FINGERPRINT_TYPE_MD5:
      return EVP_md5();
    //case DTLS_FINGERPRINT_TYPE_MD2:
    //  return EVP_md2();
    case DTLS_FINGERPRINT_TYPE_SHA1: 
      return EVP_sha1();
    case DTLS_FINGERPRINT_TYPE_SHA256: 
      return EVP_sha256();
    case DTLS_FINGERPRINT_TYPE_SHA512: 
      return EVP_sha512();
    case DTLS_FINGERPRINT_TYPE_SHA224: 
      return EVP_sha224();
    case DTLS_FINGERPRINT_TYPE_SHA384: 
      return EVP_sha384();
    default:
      return NULL;
  }
}

/*
char *dtls_fingerprint2str(const DTLS_FINGERPRINT_T *pFingerprint, char *buf, unsigned int len) {
  int rc = 0;
  unsigned int idxIn;
  unsigned int idxOut = 0;

  for(idxIn = 0; idxIn < pFingerprint->len; idxIn++) {

    if(idxOut + 3 >= len) {
      buf[idxOut] = '\0';
      return buf;
    }

    if((rc = snprintf(&buf[idxOut], len - idxOut, "%s%.2X", idxIn > 0 ? ":" : "", pFingerprint->buf[idxIn])) > 0) {
      idxOut += rc;
    }
  }

  buf[idxOut] = '\0';

  return buf;
}

int dtls_str2fingerprint(const char *str, DTLS_FINGERPRINT_T *pFingerprint) {
  int rc = 0;
  char buf[8];
  const char *p = str;
  unsigned int idxIn;

  pFingerprint->len = 0;

  while(*p == ' ') {
    p++;
  }

  while(*p != '\0' && *p != '\r' && *p!= '\n') {
    idxIn = 0;
    buf[idxIn++] = *p;
    p++;
    if(*p != ':') {
      buf[idxIn++] = *p;
      p++;
    }

    buf[idxIn] = '\0';
    if((rc = hex_decode(buf, &pFingerprint->buf[pFingerprint->len], DTLS_FINGERPRINT_MAXSZ - pFingerprint->len)) < 0) {
      return rc;
    } else {
      pFingerprint->len += rc;
    }

    if(*p == ':') {
      p++;
    }
  }

  return pFingerprint->len;
}
*/

int dtls_get_cert_fingerprint(const char *certPath, DTLS_FINGERPRINT_T *pFingerprint) {
  int rc = 0;
  unsigned int lenHash;
  X509 *pX509 = NULL;
  BIO *pBIO = NULL;
  const EVP_MD *pEvp = NULL;

  if(!certPath || !pFingerprint) {
    return -1;
  }

  pFingerprint->buf[0] = '\0';
  pFingerprint->len = 0;

  if(!(pEvp = dtlsFingerprintTypeToEVP(pFingerprint->type))) {
    LOG(X_ERROR("DTLS invalid fingerprint digest type"));
    return -1;
  }

  if(!(pBIO = BIO_new(BIO_s_file()))) {
    LOG(X_ERROR("DTLS BIO_new failed for '%s': %s"), certPath,  ERR_reason_error_string(ERR_get_error()));
    return -1;
  }

  if(rc >= 0 && BIO_read_filename(pBIO, certPath) != 1){
    LOG(X_ERROR("DTLS BIO_read_filename failed for '%s': %s"), certPath,  ERR_reason_error_string(ERR_get_error()));
    rc = -1;
  }

  if(rc >= 0 && !(pX509 = PEM_read_bio_X509(pBIO, NULL, 0, NULL))){
    LOG(X_ERROR("DTLS PEM_read_bio_X509 failed for '%s': %s"), certPath,  ERR_reason_error_string(ERR_get_error()));
    rc = -1;
  }

  lenHash = sizeof(pFingerprint->buf);

  if(rc >= 0 && (X509_digest(pX509, pEvp, pFingerprint->buf, &lenHash) != 1 || lenHash <= 0)) {
    LOG(X_ERROR("DTLS X509_digest failed for '%s': %s"), certPath,  ERR_reason_error_string(ERR_get_error()));
    rc = -1;
  } else {
    pFingerprint->len = lenHash;
    rc = pFingerprint->len;
  }

  //avc_dumpHex(stderr, buf, sizeof(buf), 1);

  if(pX509) {
    X509_free(pX509);
  }

  if(pBIO) {
    BIO_free_all(pBIO);
  }

  return rc;
}

static int s_sslCbDataIndex=-1;

int dtls_verify_cb(int preverify_ok, X509_STORE_CTX *x509_ctx) {
  //int rc = 0;
  unsigned int lenHash;
  const EVP_MD *pEvp = NULL;
  DTLS_FINGERPRINT_T fingerprint;
  X509 *pX509 = NULL;
  SSL *pSSLCtxt = NULL;
  int sslCbDataIndex;
  char buf[1024];
  char buf2[1024];
  DTLS_FINGERPRINT_VERIFY_T *pFingerprintVerify = NULL;


  pSSLCtxt = X509_STORE_CTX_get_ex_data(x509_ctx, SSL_get_ex_data_X509_STORE_CTX_idx());

  for(sslCbDataIndex = 0; sslCbDataIndex <= s_sslCbDataIndex; sslCbDataIndex++) {
    if((pFingerprintVerify = SSL_get_ex_data(pSSLCtxt, sslCbDataIndex))) {
      break;
    }
  }

  //LOG(X_DEBUG("DTLS_VERIFY_CB preverify_ok:%d, pFingerprintVerify: 0x%x, name:'%s'"), preverify_ok, pFingerprintVerify, X509_NAME_oneline(X509_get_subject_name(x509_ctx->cert), buf, sizeof(buf)));

  if(!pFingerprintVerify || pFingerprintVerify->verified) {
    return 1; 
  }

  //LOG(X_DEBUG("Verify fingerprint type:%d"), pFingerprintVerify->fingerprint.type); LOGHEX_DEBUG(pFingerprintVerify->fingerprint.buf, pFingerprintVerify->fingerprint.len);

  if(!(pX509 = x509_ctx->cert)) {
    return 0;
  }
  memset(&fingerprint, 0, sizeof(fingerprint));
  fingerprint.type = pFingerprintVerify->fingerprint.type;

  if(!(pEvp = dtlsFingerprintTypeToEVP(fingerprint.type))) {
    LOG(X_ERROR("Invalid DTLS fingerprint verification digest type %s (%d)"), 
        sdp_dtls_get_fingerprint_typestr(fingerprint.type), fingerprint.type);
    return 0;
  }

  lenHash = sizeof(fingerprint.buf);

  if(X509_digest(pX509, pEvp, fingerprint.buf, &lenHash) != 1 || lenHash <= 0) {
    LOG(X_ERROR("DTLS X509_digest failed for peer certificate: %s"), ERR_reason_error_string(ERR_get_error()));
    return 0;
  } else {
    fingerprint.len = lenHash;
  }

  if(fingerprint.len != pFingerprintVerify->fingerprint.len || 
     memcmp(fingerprint.buf, pFingerprintVerify->fingerprint.buf, fingerprint.len)) {
    LOG(X_ERROR("DTLS fingerprint verification failed for %s fingerprint length %d for remote certificate: %s"),
        sdp_dtls_get_fingerprint_typestr(fingerprint.type), fingerprint.len,
        X509_NAME_oneline(X509_get_subject_name(x509_ctx->cert), buf, sizeof(buf)));
    LOG(X_DEBUG("DTLS remote fingerprint %s does not match %s"), 
                dtls_fingerprint2str(&fingerprint, buf2, sizeof(buf2)),
                dtls_fingerprint2str(&pFingerprintVerify->fingerprint, buf, sizeof(buf)));
    return 0;
  }

  pFingerprintVerify->verified = 1;
  LOG(X_DEBUG("DTLS verified %s fingerprint length %d for remote cerficate: %s"),
      sdp_dtls_get_fingerprint_typestr(fingerprint.type), fingerprint.len,
      X509_NAME_oneline(X509_get_subject_name(x509_ctx->cert), buf, sizeof(buf)));
  //LOG(X_DEBUG("DTLS_VERIFY_CB fingerprint type:%d, len: %d"), fingerprint.type, fingerprint.len); LOGHEX_DEBUG(fingerprint.buf, fingerprint.len);

  return 1;
}

void dtls_ctx_close(STREAM_DTLS_CTXT_T *pCtxt) {

  if(!pCtxt) {
    return;
  }

  if(pCtxt->pctx) {
    SSL_CTX_free(pCtxt->pctx);
    pCtxt->pctx = NULL;
  }

  if(pCtxt->active) {
    pthread_mutex_destroy(&pCtxt->mtx);
    pCtxt->pCfg = NULL;
    pCtxt->pDtlsShared = NULL;
    pCtxt->haveRcvdData = 0;
    pCtxt->active = 0;
  }

}

//static pthread_mutex_t g_dtls_ctx_mtx = PTHREAD_MUTEX_INITIALIZER;

int dtls_ctx_init(STREAM_DTLS_CTXT_T *pCtxt, const DTLS_CFG_T *pDtlsCfg, int dtls_srtp) {
  int rc = 0;
  const SSL_METHOD *method = NULL;
  const char *srtpProfiles = NULL;
  //const char *cipherList = NULL;

  if(!pCtxt || !pDtlsCfg) {
    return -1;
  } else if(!pDtlsCfg->certpath) {
    LOG(X_ERROR("DTLS certificate path not set"));
    return -1;
  } else if(!pDtlsCfg->privkeypath) {
    LOG(X_ERROR("DTLS private key path not set"));
    return -1;
  }

  //pthread_mutex_lock(&g_dtls_ctx_mtx);
  pthread_mutex_lock(&g_ssl_mtx);

  if((rc = sslutil_init()) >= 0) {
    method = DTLSv1_method();
    //method = DTLSv1_client_method();
    //method = DTLSv1_servert_method();
  }

  if(rc >= 0 && (!method || !(pCtxt->pctx = SSL_CTX_new(method)))) {
    LOG(X_ERROR("DTLS SSL_CTX_new failed"));
    rc = -1;
  }

  //pthread_mutex_unlock(&g_dtls_ctx_mtx);
  pthread_mutex_unlock(&g_ssl_mtx);

  if(rc >= 0) {
     SSL_CTX_set_mode(pCtxt->pctx, SSL_MODE_AUTO_RETRY);
     SSL_CTX_set_verify(pCtxt->pctx, SSL_VERIFY_NONE, NULL);
     //cipherList = "ALL:!ADH:!LOW:!EXP:!MD5:@STRENGTH";
     //cipherList = "ALL";
     if(SSL_CTX_set_cipher_list(pCtxt->pctx, DTLS_CIPHER_LIST) <= 0) {
       LOG(X_ERROR("DTLS SSL_CTX_set_cipher_list '%s' failed"), ERR_reason_error_string(ERR_get_error())); 
       rc = -1;
     }
  }

  pCtxt->dtls_srtp = dtls_srtp;

  if(rc >= 0 && dtls_srtp) {
    srtpProfiles = DTLS_SRTP_PROFILE;

    if(SSL_CTX_set_tlsext_use_srtp(pCtxt->pctx, srtpProfiles) != 0) {
      LOG(X_ERROR("DTLS SSL_CTX_set_tlsext_use_srtp failed: %s"), ERR_reason_error_string(ERR_get_error()));
      rc = -1;
    }

  }

  if(rc >= 0) {
    SSL_CTX_set_verify_depth(pCtxt->pctx, 4);
  }

  if(rc >= 0 && SSL_CTX_use_certificate_file(pCtxt->pctx, pDtlsCfg->certpath, SSL_FILETYPE_PEM) != 1) {
    LOG(X_ERROR("DTLS SSL_CTX_use_certificate_file '%s' failed: %s"), 
                 pDtlsCfg->certpath, ERR_reason_error_string(ERR_get_error()));
    rc = -1;
  }

  if(rc >= 0 && SSL_CTX_use_PrivateKey_file(pCtxt->pctx, pDtlsCfg->privkeypath, SSL_FILETYPE_PEM) != 1) {
    LOG(X_ERROR("DTLS SSL_CTX_use_PrivateKey_file '%s' failed: %s"), 
                pDtlsCfg->privkeypath, ERR_reason_error_string(ERR_get_error()));
    rc = -1;
  }

  if(rc >= 0 && SSL_CTX_check_private_key(pCtxt->pctx) != 1) {
    LOG(X_ERROR("DTLS SSL_CTX_check_private_key failed: %s"), ERR_reason_error_string(ERR_get_error()));
    rc = -1;
  }

  if(rc < 0) {
    dtls_ctx_close(pCtxt);
  } else {

    pthread_mutex_init(&pCtxt->mtx, NULL);
    pCtxt->pCfg = pDtlsCfg;
    pCtxt->pDtlsShared = NULL;
    pCtxt->haveRcvdData = 0;
    pCtxt->active = 1;

    LOG(X_DEBUG("Initialized DTLS%s %scontext cert-file: %s, key-file: %s"), 
        (pCtxt->dtls_srtp ? "-SRTP" : ""),
        (pCtxt->dtls_srtp ? (pDtlsCfg->dtls_handshake_server ? "server key " : "client key ") : ""),
        pDtlsCfg->certpath ? pDtlsCfg->certpath : "",
        pDtlsCfg->privkeypath ? pDtlsCfg->privkeypath : "");
  }

  return rc;
}

int dtls_netsock_init(const STREAM_DTLS_CTXT_T *pDtlsCtxt, 
                      NETIO_SOCK_T *pnetsock, 
                      const DTLS_FINGERPRINT_VERIFY_T *pFingerprintVerify,
                      const DTLS_KEY_UPDATE_CTXT_T *pKeysUpdateCtxt) {
  int rc = 0;

  if(!pDtlsCtxt || !pnetsock) {
    return -1;
  }

  if(!pnetsock || PNETIOSOCK_FD(pnetsock) == INVALID_SOCKET) {
    return -1;
  }

  pthread_mutex_lock(&((STREAM_DTLS_CTXT_T *) pDtlsCtxt)->mtx);

  if(pDtlsCtxt->active != 1) {
    pthread_mutex_unlock(&((STREAM_DTLS_CTXT_T *) pDtlsCtxt)->mtx);
    return -1;
  }

  if(!(pnetsock->ssl.pCtxt = SSL_new(pDtlsCtxt->pctx))) {
    LOG(X_ERROR("DTLS SSL_new failed: %s"), ERR_reason_error_string(ERR_get_error()));
    rc = -1;
  }
  pnetsock->ssl.ownCtxt = 1;

  if(rc >= 0 && !(pnetsock->ssl.pBioRd = BIO_new(BIO_s_mem()))) {
    LOG(X_ERROR("DTLS BIO_new (reader) failed: %s"), ERR_reason_error_string(ERR_get_error()));
    rc = -1;
  }
  pnetsock->ssl.ownBioRd = 1;

  if(rc >= 0 && !(pnetsock->ssl.pBioWr = BIO_new(BIO_s_mem()))) {
    LOG(X_ERROR("DTLS BIO_new (writer) failed: %s"), ERR_reason_error_string(ERR_get_error()));
    rc = -1;
  }
  pnetsock->ssl.ownBioWr = 1;

  if(rc >= 0) {

    BIO_set_mem_eof_return(pnetsock->ssl.pBioRd, -1);
    BIO_set_mem_eof_return(pnetsock->ssl.pBioWr, -1);

    SSL_set_bio(pnetsock->ssl.pCtxt, pnetsock->ssl.pBioRd, pnetsock->ssl.pBioWr);
    pnetsock->ssl.ownBioRd = 0;
    pnetsock->ssl.ownBioWr = 0;

    SSL_set_mode(pnetsock->ssl.pCtxt, SSL_MODE_AUTO_RETRY);

    SSL_set_read_ahead(pnetsock->ssl.pCtxt, 1);

    //SSL_set_connect_state(pnetsock->ssl.pCtxt);
    //SSL_set_accept_state(pnetsock->ssl.pCtxt);

    if(pFingerprintVerify) {
      if(s_sslCbDataIndex == -1) {
        s_sslCbDataIndex = SSL_get_ex_new_index(0, "verifycbIndex", NULL, NULL, NULL);
      }
      SSL_set_verify(pnetsock->ssl.pCtxt, (SSL_VERIFY_PEER | SSL_VERIFY_FAIL_IF_NO_PEER_CERT), dtls_verify_cb);
      SSL_set_ex_data(pnetsock->ssl.pCtxt, s_sslCbDataIndex, (void *) pFingerprintVerify);
      //LOG(X_DEBUG("SSL_set_ex_data index:%d, pFingerprintVerify: 0x%x"), s_sslCbDataIndex, pFingerprintVerify);
    }

  }

  pnetsock->ssl.pDtlsCtxt = pDtlsCtxt;

  pnetsock->flags |= NETIO_FLAG_SSL_DTLS;
  if(pDtlsCtxt->dtls_srtp) {
    pnetsock->flags |= NETIO_FLAG_SRTP;
    if(pKeysUpdateCtxt) {
      memcpy(&pnetsock->ssl.dtlsKeysUpdateCtxt, pKeysUpdateCtxt, sizeof(pnetsock->ssl.dtlsKeysUpdateCtxt));
    }
  }

  pthread_mutex_unlock(&((STREAM_DTLS_CTXT_T *) pDtlsCtxt)->mtx);

  return rc;
}


int dtls_netsock_setmtu(NETIO_SOCK_T *pnetsock, int mtu) {

  if(!pnetsock || !pnetsock->ssl.pCtxt) {
    return -1;
  } else if(mtu <= 0) {
    return 0;
  } else {
#if defined(__linux__) 

    if(mtu < 228) {
      LOG(X_ERROR("Cannot set DTLS MTU %d below minimum %d"), 228);
      return -1;
    }

    pthread_mutex_lock(&((STREAM_DTLS_CTXT_T *) pnetsock->ssl.pDtlsCtxt)->mtx);

    LOG(X_DEBUG("Setting DTLS MTU %d (netsock: 0x%x)"), mtu, pnetsock);
    // openSSL MTU may be broken on Mac, but may work on Linux
    //SSL_set_options(pnetsock->ssl.pCtxt, SSL_OP_NO_QUERY_MTU); //TODO: setting this makes the socket mtu 0, and the ssl/d1_both.c:268 assert crashes the app 
    SSL_set_mtu(pnetsock->ssl.pCtxt, mtu);
    //SSL_set_mtu(pnetsock->ssl.pCtxt, mtu - 28);

    pthread_mutex_unlock(&((STREAM_DTLS_CTXT_T *) pnetsock->ssl.pDtlsCtxt)->mtx);

#endif // (__linux__) 
    return 0;
  }

}

int dtls_netsock_setclient(NETIO_SOCK_T *pnetsock, int client) {
  int rc = 0; 

  if(!pnetsock || !(pnetsock->flags & NETIO_FLAG_SSL_DTLS) || !pnetsock->ssl.pCtxt) {
    return -1;
  }

  if(client) {
    LOG(X_DEBUG("DTLS handshake SSL_set_connect_state"));
    SSL_set_connect_state(pnetsock->ssl.pCtxt);
    pnetsock->flags &= ~NETIO_FLAG_SSL_DTLS_SERVER;
  } else {
    LOG(X_DEBUG("DTLS handshake SSL_set_accept_state"));
    SSL_set_accept_state(pnetsock->ssl.pCtxt);
    pnetsock->flags |= NETIO_FLAG_SSL_DTLS_SERVER;
  }

  rc = 0;

  return rc;
}

static int dlts_netsock_onhandshake_completed(NETIO_SOCK_T *pnetsock) {
  int rc = 0;
  SRTP_PROTECTION_PROFILE *srtpProfile = NULL;
  unsigned char keys[DTLS_SRTP_KEYING_MATERIAL_SIZE];

  if(!(pnetsock->flags & NETIO_FLAG_SRTP)) {
    return 0;
  }

  if(!(srtpProfile = SSL_get_selected_srtp_profile(pnetsock->ssl.pCtxt))) {
    LOG(X_ERROR("DTLS-SRTP SSL_get_selected_srtp_profile failed, peer may not be DTLS-SRTP capable: %s"),
            ERR_reason_error_string(ERR_get_error()));
    return -1;
  }

  memset(keys, 0, sizeof(keys));
  if(SSL_export_keying_material(pnetsock->ssl.pCtxt, keys, sizeof(keys),
                             DTLS_SRTP_KEYS_LABEL, strlen(DTLS_SRTP_KEYS_LABEL), NULL, 0, 0) != 1) {
    LOG(X_ERROR("DTLS-SRTP SSL_export_keying_material failed: %s"),
            ERR_reason_error_string(ERR_get_error()));
    return -1;
  }

  //LOG(X_DEBUG("dlts_netsock_onhandshake_completed DTLS-SRTP handshake selected profile: '%s', master-keys: "), srtpProfile->name); LOGHEX_DEBUG(keys, sizeof(keys));

  if(pnetsock->ssl.dtlsKeysUpdateCtxt.cbKeyUpdateFunc) {
    pnetsock->ssl.dtlsKeysUpdateCtxt.cbKeyUpdateFunc(pnetsock->ssl.dtlsKeysUpdateCtxt.pCbData, pnetsock,
                                                     srtpProfile->name, pnetsock->ssl.dtlsKeysUpdateCtxt.dtls_serverkey,
                                                     pnetsock->ssl.dtlsKeysUpdateCtxt.is_rtcp,
                                                     keys, sizeof(keys));
  }

  return rc;
}

static int dtls_netsock_handshake_int(NETIO_SOCK_T *pnetsock, const struct sockaddr_in *dest_addr) {
  int rc = 0;
  int code;
  int len;
  char *pData = NULL;
  struct sockaddr_in saTmp;
  socklen_t socklen;

  if(!pnetsock || !dest_addr || !pnetsock->ssl.pCtxt) {
    return -1;
  } else if(pnetsock->ssl.state >= SSL_SOCK_STATE_HANDSHAKE_COMPLETED) {
    return 1;
  } else if(pnetsock->ssl.state == SSL_SOCK_STATE_ERROR) {
    return -1;
  }

  if(pnetsock->ssl.state < SSL_SOCK_STATE_HANDSHAKE_INPROGRESS) {
    pnetsock->ssl.state = SSL_SOCK_STATE_HANDSHAKE_INPROGRESS;
  }

  //LOG(X_DEBUG("Calling SSL_do_handshake"));
  if((rc = SSL_do_handshake(pnetsock->ssl.pCtxt)) != 1){

    code = SSL_get_error(pnetsock->ssl.pCtxt, rc);
    //LOG(X_DEBUG("SSL_do_handshake rc:%d, code:%d"), rc, code);
    switch(code) {
      case SSL_ERROR_WANT_READ:  // 2
      case SSL_ERROR_WANT_WRITE: // 3
      case SSL_ERROR_NONE:       // 0
        break;
      default:
        LOG(X_ERROR("DTLS%s %shandshake SSL_do_handshake failed with rc: %d, code: %d %s"), 
            (pnetsock->flags & NETIO_FLAG_SRTP) ? "-SRTP" : "",
            (pnetsock->flags & NETIO_FLAG_SSL_DTLS_SERVER) ? "passive " : "active ",
            rc, code, ERR_reason_error_string(ERR_get_error()));
        pnetsock->ssl.state = SSL_SOCK_STATE_ERROR;
        return -1;
    }
  }

  rc = 0;

  if((len = BIO_get_mem_data(pnetsock->ssl.pBioWr, &pData)) > 0 && pData){
    //LOG(X_DEBUG("SSL handshake - BIO_get_mem_data len:%d"), len);

    if((rc = SENDTO_DTLS(pnetsock, pData, len, 0, dest_addr)) < len) {
    //if((rc = sendto(PNETIOSOCK_FD(pnetsock), pData, len, 0, (const struct sockaddr *) dest_addr, 
    //                sizeof(struct sockaddr_in))) < len) {

      socklen = sizeof(saTmp);
      getsockname(PNETIOSOCK_FD(pnetsock), (struct sockaddr *) &saTmp, &socklen);
      LOG(X_ERROR("DTLS%s %shandshake sendto %d -> %s:%d for %d bytes failed with "ERRNO_FMT_STR),
                 (pnetsock->flags & NETIO_FLAG_SRTP) ? "-SRTP" : "",
                 (pnetsock->flags & NETIO_FLAG_SSL_DTLS_SERVER) ? "passive " : "active ",
                 ntohs(saTmp.sin_port), inet_ntoa(dest_addr->sin_addr), ntohs(dest_addr->sin_port), len, ERRNO_FMT_ARGS);
      pnetsock->ssl.state = SSL_SOCK_STATE_ERROR;
      rc = -1;

    } else {
     
      socklen = sizeof(saTmp);
      getsockname(PNETIOSOCK_FD(pnetsock), (struct sockaddr *) &saTmp, &socklen);
      LOG(X_DEBUG("DTLS%s %shandshake sent %d bytes from :%d to %s:%d"), 
           (pnetsock->flags & NETIO_FLAG_SRTP) ? "-SRTP" : "",
           (pnetsock->flags & NETIO_FLAG_SSL_DTLS_SERVER) ? "passive " : "active ",
           len, ntohs(saTmp.sin_port), inet_ntoa(dest_addr->sin_addr), ntohs(dest_addr->sin_port));
      rc = 0;
    }
  }

  //LOG(X_DEBUG("SSL handshake - Calling BIO_reset, BIO_get_mem_data had len:%d"), len);
  (void) BIO_reset(pnetsock->ssl.pBioRd);
  (void) BIO_reset(pnetsock->ssl.pBioWr);

  if(SSL_is_init_finished(pnetsock->ssl.pCtxt)){

    if((rc = dlts_netsock_onhandshake_completed(pnetsock)) < 0) {
      pnetsock->ssl.state = SSL_SOCK_STATE_ERROR;
    } else {

      LOG(X_DEBUG("DTLS%s %shandshake to %s:%d completed"), 
          (pnetsock->flags & NETIO_FLAG_SRTP) ? "-SRTP" : "",
           (pnetsock->flags & NETIO_FLAG_SSL_DTLS_SERVER) ? "passive " : "active ",
          inet_ntoa(dest_addr->sin_addr), ntohs(dest_addr->sin_port));

      pnetsock->ssl.state = SSL_SOCK_STATE_HANDSHAKE_COMPLETED;
      rc = 1;
    }
  }

  return rc;
}

int dtls_netsock_handshake(NETIO_SOCK_T *pnetsock, const struct sockaddr_in *dest_addr) {
  int rc = 0;

  if(!pnetsock || !dest_addr) {
    return -1;
  }

  if(!pnetsock->ssl.pCtxt || !pnetsock->ssl.pDtlsCtxt) {
     return -1;
  }

  pthread_mutex_lock(&((STREAM_DTLS_CTXT_T *) pnetsock->ssl.pDtlsCtxt)->mtx);

  rc = dtls_netsock_handshake_int(pnetsock, dest_addr);

  pthread_mutex_unlock(&((STREAM_DTLS_CTXT_T *) pnetsock->ssl.pDtlsCtxt)->mtx);

  return rc;
}

int dtls_netsock_ondata(NETIO_SOCK_T *pnetsock, const unsigned char *pData, unsigned int len,
                        const struct sockaddr_in *psaSrc) {
  int rc = 0;
  int code = 0;

  if(!pnetsock || !pData) {
    return -1;
  }

  if(!pnetsock->ssl.pCtxt || !pnetsock->ssl.pDtlsCtxt) {
     return -1;
  }

  //LOG(X_DEBUG("dtls_netsock_ondata len:%d, state:%d"), len, pnetsock->ssl.state);

  pthread_mutex_lock(&((STREAM_DTLS_CTXT_T *) pnetsock->ssl.pDtlsCtxt)->mtx);

  if(len > 0 && !pnetsock->ssl.pDtlsCtxt->haveRcvdData) {
    ((STREAM_DTLS_CTXT_T *) pnetsock->ssl.pDtlsCtxt)->haveRcvdData = 1;
  }

  if(pnetsock->ssl.state != SSL_SOCK_STATE_ERROR && pnetsock->ssl.state != SSL_SOCK_STATE_HANDSHAKE_COMPLETED) {
    dtls_netsock_handshake_int(pnetsock, psaSrc);
  }

  if((rc = BIO_write(pnetsock->ssl.pBioRd, pData, len)) != len){
    LOG(X_ERROR("DTLS BIO_write (input data) failed with %d for %d bytes: %s"), 
                rc, len, ERR_reason_error_string(ERR_get_error()));
    pthread_mutex_unlock(&((STREAM_DTLS_CTXT_T *) pnetsock->ssl.pDtlsCtxt)->mtx);
    return -1;
  }     

  if((rc = SSL_read(pnetsock->ssl.pCtxt, (void *) pData, len)) < 0) {
    code = SSL_get_error(pnetsock->ssl.pCtxt, rc);
    //LOG(X_DEBUG("SSL_read for len:%d, rc:%d, code:%d"), len, rc, code);
    switch(code) {
      case SSL_ERROR_WANT_READ:  // 2
      case SSL_ERROR_WANT_WRITE: // 3
      case SSL_ERROR_NONE:       // 0
        rc = 0;
        break;
      default:
        LOG(X_ERROR("DTLS data handler length:%d  SSL_read failed with rc: %d, code: %d %s"),
            len, rc, code, ERR_reason_error_string(ERR_get_error()));
        pnetsock->ssl.state = SSL_SOCK_STATE_ERROR;
      pthread_mutex_unlock(&((STREAM_DTLS_CTXT_T *) pnetsock->ssl.pDtlsCtxt)->mtx);
      return -1;
    }
  }
  //LOG(X_DEBUG("DTLS SSL_read for len:%d, rc:%d, code:%d"), len, rc, code);

  if(pnetsock->ssl.state != SSL_SOCK_STATE_ERROR && pnetsock->ssl.state != SSL_SOCK_STATE_HANDSHAKE_COMPLETED) {
    dtls_netsock_handshake_int(pnetsock, psaSrc);
  }

  if(pnetsock->ssl.state != SSL_SOCK_STATE_HANDSHAKE_COMPLETED) {
    rc = 0;
  }

  pthread_mutex_unlock(&((STREAM_DTLS_CTXT_T *) pnetsock->ssl.pDtlsCtxt)->mtx);

  return rc;
}

int dtls_netsock_write(NETIO_SOCK_T *pnetsock, const unsigned char *pDataIn, unsigned int lenIn,
                       unsigned char *pDataOut, unsigned int lenOut) {
  int rc = 0;
  int code = 0;
  long sz0, sz;
  char *p = NULL;

  if(!pnetsock || !pDataIn || !pDataOut || !pnetsock->ssl.pCtxt || !pnetsock->ssl.pDtlsCtxt || !pnetsock->ssl.pBioWr) {
     return -1;
  }

  pthread_mutex_lock(&((STREAM_DTLS_CTXT_T *) pnetsock->ssl.pDtlsCtxt)->mtx);

  if((rc = SSL_write(pnetsock->ssl.pCtxt, pDataIn, lenIn)) < 0) {
    code = SSL_get_error(pnetsock->ssl.pCtxt, rc);
    LOG(X_DEBUG("DTLS SSL_write for len:%d, rc:%d, code:%d"), lenIn, rc, code);
  }

  sz0 = BIO_get_mem_data(pnetsock->ssl.pBioWr, &p);

  if((rc = BIO_read(pnetsock->ssl.pBioWr, pDataOut, lenOut)) < 0){
    LOG(X_ERROR("DTLS BIO_read (output data) failed with %d for %d -> %d bytes: %s"), 
                rc, lenIn, lenOut, ERR_reason_error_string(ERR_get_error()));
    rc = -1;
  } else {
    //LOG(X_DEBUG("DTLS BIO_read for lenIn:%d -> lenOut:%d/%d (buf:%d)"), lenIn, rc, sz0, lenOut);
  }

  if(rc >= 0) {

    if(!BIO_eof(pnetsock->ssl.pBioWr)) {
      sz = BIO_get_mem_data(pnetsock->ssl.pBioWr, &p);
      LOG(X_WARNING("DTLS output BIO discarding unread %lu/%ld bytes"), sz, sz0); 
    }

    (void) BIO_reset(pnetsock->ssl.pBioWr);

  }

  pthread_mutex_unlock(&((STREAM_DTLS_CTXT_T *) pnetsock->ssl.pDtlsCtxt)->mtx);

  return rc;
}

#else //(VSX_HAVE_SSL_DTLS) && defined(VSX_HAVE_SSL)

int dtls_netsock_write(NETIO_SOCK_T *pnetsock, const unsigned char *pDataIn, unsigned int lenIn,
                       unsigned char *pDataOut, unsigned int lenOut) {
  return -1;
}

int dtls_netsock_ondata(NETIO_SOCK_T *pnetsock, const unsigned char *pData, unsigned int len,
                        const struct sockaddr_in *psaSrc) {
  return -1;
}

#endif //(VSX_HAVE_SSL_DTLS) && defined(VSX_HAVE_SSL)

char *dtls_fingerprint2str(const DTLS_FINGERPRINT_T *pFingerprint, char *buf, unsigned int len) {
  int rc = 0;
  unsigned int idxIn;
  unsigned int idxOut = 0;

  for(idxIn = 0; idxIn < pFingerprint->len; idxIn++) {

    if(idxOut + 3 >= len) {
      buf[idxOut] = '\0';
      return buf;
    }

    if((rc = snprintf(&buf[idxOut], len - idxOut, "%s%.2X", idxIn > 0 ? ":" : "", pFingerprint->buf[idxIn])) > 0) {
      idxOut += rc;
    }
  }

  buf[idxOut] = '\0';

  return buf;
}

int dtls_str2fingerprint(const char *str, DTLS_FINGERPRINT_T *pFingerprint) {
  int rc = 0;
  char buf[8];
  const char *p = str;
  unsigned int idxIn;

  pFingerprint->len = 0;

  while(*p == ' ') {
    p++;
  }

  while(*p != '\0' && *p != '\r' && *p!= '\n') {
    idxIn = 0;
    buf[idxIn++] = *p;
    p++;
    if(*p != ':') {
      buf[idxIn++] = *p;
      p++;
    }

    buf[idxIn] = '\0';
    if((rc = hex_decode(buf, &pFingerprint->buf[pFingerprint->len], DTLS_FINGERPRINT_MAXSZ - pFingerprint->len)) < 0) {
      return rc;
    } else {
      pFingerprint->len += rc;
    }

    if(*p == ':') {
      p++;
    }
  }

  return pFingerprint->len;
}

