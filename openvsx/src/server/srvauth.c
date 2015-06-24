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


const char *srv_check_authorized(const AUTH_CREDENTIALS_STORE_T *pAuthStore,
                                 const HTTP_REQ_T *pReq, 
                                 char *authbuf) {

  if(pAuthStore && IS_AUTH_CREDENTIALS_SET(pAuthStore)) {

    if(srvauth_authenticate(pAuthStore, pReq, NULL, NULL, NULL, authbuf, AUTH_BUF_SZ) < 0) {
      return (const char *) authbuf;
    }
  }

  return NULL;
}

int srvauth_authenticate(const AUTH_CREDENTIALS_STORE_T *pAuthStore, const HTTP_REQ_T *pReq, 
                         const char *nonce_srv, const char *opaque_srv, const char *realm_srv,
                         char *bufAuthHdr, unsigned int bufOutSz) {
  int rc = 0;
  int ok_auth = 0;
  int do_basic = 0;
  char bufAuth[1024];
  char tmpH1[KEYVAL_MAXLEN];
  char tmpH2[KEYVAL_MAXLEN];
  char tmpDigest[KEYVAL_MAXLEN];
  char tmps[6][KEYVAL_MAXLEN];
  char bufnonce[AUTH_NONCE_MAXLEN];
  const char *pAuthHdr = NULL;
  const char *enc = NULL;
  const char *realm = NULL;
  const char *uri_cli = NULL;
  const char *qop_cli = NULL;
  const char *nc_cli = NULL;
  const char *cnonce_cli = NULL;
  const char *nonce_cli = NULL;
  const char *resp_cli = NULL;
  AUTH_LIST_T authList;
  AUTH_CREDENTIALS_STORE_T authStore;

  if(!pAuthStore || !pReq) {
    return -1;
  }


  if(!IS_AUTH_CREDENTIALS_SET(pAuthStore)) {
    //
    // No username and password has been set
    //
    return 0;
  }

  if(!(realm = realm_srv)) {
    realm = pAuthStore->realm;
  }

  if(!realm || realm[0] == '\0') {
    realm = VSX_BINARY;
  }

  if(pAuthStore->authtype == HTTP_AUTH_TYPE_FORCE_BASIC) {
    do_basic = 1;
  }

  if(ok_auth >= 0 && 
     !(pAuthHdr = conf_find_keyval((const KEYVAL_PAIR_T *) &pReq->reqPairs, HTTP_HDR_AUTHORIZATION))) {
    LOG(X_WARNING("Client request doest not include "HTTP_HDR_AUTHORIZATION" header for %s %s"),
                pReq->method, pReq->url);
    ok_auth = -1;
  }

  if(ok_auth >= 0) {
    enc = pAuthHdr;
    MOVE_WHILE_SPACE(enc);
    if(!strncasecmp(AUTH_STR_BASIC, enc, strlen(AUTH_STR_BASIC))) {
      enc += strlen(AUTH_STR_BASIC);
      MOVE_WHILE_SPACE(enc);
      if(pAuthStore->authtype == HTTP_AUTH_TYPE_ALLOW_BASIC || pAuthStore->authtype == HTTP_AUTH_TYPE_FORCE_BASIC) {
        do_basic = 1;
      } else {
        LOG(X_WARNING("Client wants disabled Basic authentication for %s %s"), pReq->method, pReq->url);
        ok_auth = -1;
      }
    } else if(!strncasecmp(AUTH_STR_DIGEST, enc, strlen(AUTH_STR_DIGEST))) {
      enc += strlen(AUTH_STR_DIGEST);
      MOVE_WHILE_SPACE(enc);
    } else {
      LOG(X_WARNING("Client request contains unsupported "HTTP_HDR_AUTHORIZATION
                    " header type '%s' for %s %s"), pAuthHdr, pReq->method, pReq->url);
      ok_auth = -1;
    }
  }

  if(ok_auth >= 0 && do_basic) {

    //
    // Basic Auth 
    //

    memset(&authStore, 0, sizeof(authStore));

    //
    // Parse the header Authorization: Basic <Base64(username:password)>
    //
    if(ok_auth >= 0 &&
       auth_basic_decode(enc, authStore.username, sizeof(authStore.username), 
                         authStore.pass, sizeof(authStore.pass)) < 0) {
      LOG(X_WARNING("Client "HTTP_HDR_AUTHORIZATION" Basic Auth decode failed for '%s' for %s %s"),
                      enc, pReq->method, pReq->url);
      ok_auth = -1;
    }

    //
    // Verify the username and password
    //
    if(ok_auth >= 0) {
      if(((pAuthStore->username[0] != '\0' && !strcmp(pAuthStore->username, authStore.username)) &&
          (pAuthStore->pass[0] != '\0' && !strcmp(pAuthStore->pass, authStore.pass)))) {
        ok_auth = 1;
      } else {
        LOG(X_WARNING("Client "HTTP_HDR_AUTHORIZATION" incorrect Basic auth credentials for "
                      "'%s' for %s %s username: %s"), pAuthHdr, pReq->method, pReq->url, pAuthStore->username);
        ok_auth = -1;
      }
    }

  } else if(ok_auth >= 0) {

    //
    // Parse the Digest Authorization header
    //
    if(ok_auth  >= 0 && auth_digest_parse(enc, &authList) < 0) {
      LOG(X_WARNING("Failed to parse client "HTTP_HDR_AUTHORIZATION" '%s' for %s %s"), 
                    pAuthHdr, pReq->method, pReq->url);
      ok_auth = -1;
    } else if(ok_auth >= 0) {

      uri_cli = avc_dequote(conf_find_keyval(&authList.list[0], "uri"), tmps[0], sizeof(tmps[0]));
      qop_cli = avc_dequote(conf_find_keyval(&authList.list[0], "qop"), tmps[1], sizeof(tmps[1]));
      nc_cli = avc_dequote(conf_find_keyval(&authList.list[0], "nc"), tmps[2], sizeof(tmps[2]));
      cnonce_cli  = avc_dequote(conf_find_keyval(&authList.list[0], "cnonce"), tmps[3], sizeof(tmps[3]));
      nonce_cli = avc_dequote(conf_find_keyval(&authList.list[0], "nonce"), tmps[4], sizeof(tmps[4]));
      resp_cli = avc_dequote(conf_find_keyval(&authList.list[0], "response"), tmps[5], sizeof(tmps[5]));

      // 
      // If nonce_srv is supplied, we try to do nonce validation
      // (prior generated nonce should not be stored per-connection but globally with matching
      // client IP and creation time
      //

      if(ok_auth >= 0 && (!nonce_cli || (nonce_srv && strcmp(nonce_cli, nonce_srv)))) {
        LOG(X_WARNING("Invalid nonce from client: '%s', expected: '%s' for %s %s"), 
            nonce_cli, nonce_srv, pReq->method, pReq->url);
        ok_auth = -1;
      }

      if(ok_auth >= 0 && 
         (rc = auth_digest_getH1(pAuthStore->username, realm, pAuthStore->pass, tmpH1, sizeof(tmpH1))) < 0) {
        ok_auth = -1;
      }
      if(ok_auth >= 0 && (rc = auth_digest_getH2(pReq->method, uri_cli, tmpH2, sizeof(tmpH2))) < 0) {
        ok_auth = -1;
      }
      if(ok_auth >= 0 && 
         (rc = auth_digest_getDigest(tmpH1, nonce_srv ? nonce_srv : nonce_cli, tmpH2, nc_cli, cnonce_cli,
                                     qop_cli, tmpDigest, sizeof(tmpDigest))) < 0) {
        ok_auth = -1;
      }
      if(ok_auth >= 0 && resp_cli  && !strcmp(tmpDigest, resp_cli)) {
        ok_auth = 1;
      } else {
        LOG(X_WARNING("Client "HTTP_HDR_AUTHORIZATION" incorrect Digest auth credentials for '%s' "
                      "for %s %s username: %s"), pAuthHdr, pReq->method, pReq->url, pAuthStore->username);
        ok_auth = -1;
      }
    }

  }

  if(ok_auth > 0) {
    LOG(X_DEBUG("Succesful %s authentication for %s %s username: %s"), 
                do_basic ? "Basic" : "Digest", pReq->method, pReq->url, pAuthStore->username);
  } else if(ok_auth <= 0 && bufAuthHdr && bufOutSz > 0) {

    if(do_basic == 1) {
      //
      // Basic Auth unauthorized resposne
      //
      if((rc = snprintf(bufAuthHdr, bufOutSz, "%s: %s realm=\"%s\"\r\n",
                        HTTP_HDR_WWW_AUTHENTICATE, AUTH_STR_BASIC, realm)) < 0) {

        return -1;
      }

    } else {
      //
      // Digest Auth RFC 2069 / RFC 2617 unauthorized response
      //

      memset(&authList, 0, sizeof(authList));
      if(nonce_srv && nonce_srv[0] == '\0') {
        auth_digest_gethexrandom(16, (char *) nonce_srv, AUTH_NONCE_MAXLEN);
      } else {
        auth_digest_gethexrandom(16, (char *) bufnonce, AUTH_NONCE_MAXLEN);
      }
      if(opaque_srv && opaque_srv[0] == '\0') {
        auth_digest_gethexrandom(16, (char *) opaque_srv, AUTH_OPAQUE_MAXLEN);
      }

      //auth_digest_additem(&authList, "qop", "auth", 1);
      auth_digest_additem(&authList, "realm", realm, 1);
      auth_digest_additem(&authList, "nonce", nonce_srv ? nonce_srv : bufnonce, 1);
      if(opaque_srv) {
        auth_digest_additem(&authList, "opaque", opaque_srv, 1);
      }

      if((rc = auth_digest_write(&authList, bufAuth, sizeof(bufAuth))) < 0) {
        return -1;
      }

      if((rc = snprintf(bufAuthHdr, bufOutSz, "%s: %s\r\n", HTTP_HDR_WWW_AUTHENTICATE, bufAuth)) < 0) {
        return -1;
      }
    }

    //if(rtsp_resp_sendhdr(pRtsp->pSd, statusCode, pReq->cseq,
    //                     pRtsp->pSession, 0, NULL, bufAuthHdr) < 0) {
    //  rc = -1;
    //}
  }

  return ok_auth > 0 ? 0 : -1;
}
