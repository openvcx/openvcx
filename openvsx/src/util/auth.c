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

int auth_basic_encode(const char *user, const char *pass, char *pOut, unsigned int lenOut) {
  int rc = 0;
  unsigned char buf[512];

  if(!user || !pass) {
    return -1;
  }

  if((rc = snprintf((char *) buf, sizeof(buf), "%s:%s", user, pass)) < 0) {
    return -1;
  }

  return base64_encode(buf, rc, pOut, lenOut);
}

static int get_user_pass(const char *str, char *user, unsigned int lenUser, char *pass, unsigned int lenPass) {
  const char *p = str;
  const char *p2 = str;
  
  while(*p != '\0' && *p != ':') {
    p++;
  }

  if(*p == ':') {
    if(!user || p - p2 + 1 > lenUser) {
      return -1;
    }
    strncpy(user, p2, p - p2);
    user[p - p2] = '\0';
    p++;
  }

  p2 = p;
  while(*p != '\0') {
    p++;
  }

  if(!pass || p - p2 + 1 > lenPass) {
    return -1;
  }
  strncpy(pass, p2, p - p2);
  pass[p - p2] = '\0';

  return 0;
}

int auth_basic_decode(const char *enc, char *user, unsigned int lenUser, char *pass, unsigned int lenPass) {
  int rc = 0;
  unsigned char buf[512];

  if(!enc) {
    return -1;
  }

  if((rc = base64_decode(enc, buf, sizeof(buf))) < 0) {
    return rc;
  }
  if(user && lenUser > 0) {
    user[0] = '\0';
  }
  if(pass && lenPass > 0) {
    pass[0] = '\0';
  }

  return get_user_pass((char *) buf, user, lenUser, pass, lenPass);
}

typedef struct DIGEST_PARSE_CTXT {
  AUTH_LIST_T         *pAuthList;
} DIGEST_PARSE_CTXT_T;

int cbparse_digest_param(void *pArg, const char *p) {
  int rc = 0;
  DIGEST_PARSE_CTXT_T *parseCtxt = (DIGEST_PARSE_CTXT_T *) pArg;
  AUTH_LIST_T *pAuthList = NULL;

  if(!p || !parseCtxt || !(pAuthList = parseCtxt->pAuthList)) {
    return -1;
  }

  if(pAuthList->count >= sizeof(pAuthList->list) / sizeof(pAuthList->list[0])) {
    return -1;
  }

  if((rc = conf_parse_keyval(&pAuthList->list[pAuthList->count], p, 0, '=', 0)) > 0) {
    if(pAuthList->count > 0) {
      pAuthList->list[pAuthList->count- 1].pnext = &pAuthList->list[pAuthList->count];
    }
    pAuthList->count++;
  }

  return rc;
}

int auth_digest_parse(const char *line, AUTH_LIST_T *pAuthList) {
  int rc = 0;
  DIGEST_PARSE_CTXT_T parseCtxt;
  const char *p = line;

  if(!line || !pAuthList) {
    return -1;
  }

  MOVE_WHILE_SPACE(p);
  if(!strncasecmp(AUTH_STR_DIGEST, p, strlen(AUTH_STR_DIGEST))) {
    p += strlen(AUTH_STR_DIGEST);
  }

  memset(pAuthList, 0, sizeof(AUTH_LIST_T));
  memset(&parseCtxt, 0, sizeof(parseCtxt));
  parseCtxt.pAuthList = pAuthList;
  if((rc = strutil_parse_csv(cbparse_digest_param, &parseCtxt, p)) < 0) {
    return rc;
  }

  return rc;
}

int auth_digest_write(const AUTH_LIST_T *pAuthList, char *pOut, unsigned int lenOut) {
  int rc = 0;
  unsigned int idx = 0;
  unsigned int sz = 0;
  const KEYVAL_PAIR_T *pelem = NULL;

  if(!pAuthList) {
    return -1;
  }

  if((rc = snprintf(&pOut[sz], lenOut - sz, AUTH_STR_DIGEST" ")) < 0) {
    return -1;
  } else {
    sz += rc;
  }

  pelem = &pAuthList->list[0];
  while(pelem) {
    if((rc = snprintf(&pOut[sz], lenOut - sz, "%s%s=%s", idx > 0 ? ", " : "", pelem->key, pelem->val)) < 0) {
      return -1;
    } else {
      sz += rc;
    }

    idx++;
    pelem = pelem->pnext;
  }
 
  return rc;
}

int auth_digest_additem(AUTH_LIST_T *pAuthList, const char *key, const char *value, int do_quote) {
  int rc = 0;

  if(!pAuthList || !key) {
    return -1;
  }

  if(pAuthList->count >= sizeof(pAuthList->list) / sizeof(pAuthList->list[0])) {
    return -1;
  }

  strncpy(pAuthList->list[pAuthList->count].key, key, sizeof(pAuthList->list[pAuthList->count].key) - 1);
  if(value) {
    if(do_quote) {
      snprintf(pAuthList->list[pAuthList->count].val, 
               sizeof(pAuthList->list[pAuthList->count].val) - 1, "\"%s\"", value);
    } else {
      strncpy(pAuthList->list[pAuthList->count].val, value, sizeof(pAuthList->list[pAuthList->count].val) - 1);
    }
  } else {
    pAuthList->list[pAuthList->count].val[0] = '\0';
  }
  if(pAuthList->count > 0) {
    pAuthList->list[pAuthList->count - 1].pnext = &pAuthList->list[pAuthList->count];
  }
  pAuthList->count++;

  return rc;
}

char *auth_digest_gethexrandom(unsigned int lenRaw, char *pOut, unsigned int lenOut) {
  int rc = 0;
  unsigned char buf[1024];
  unsigned int idx;

  for(idx = 0; idx < MIN(lenRaw, sizeof(buf)); idx += 2) {
    *((uint16_t *) &buf[idx]) = (random() % RAND_MAX);
  }

  if((rc = hex_encode(buf, lenRaw, pOut, lenOut)) < 0) {
    return NULL;
  }

  return pOut;
}

static int md5hex(const char *buf, unsigned int lenIn, char *pOut, unsigned int lenOut) {
  int rc;
  unsigned char sum[MAX(MD5_DIGEST_LENGTH, SHS_DIGESTSIZE)];

  if((rc = md5_calculate((const unsigned char *) buf, lenIn, sum)) < 0) {
    return rc;
  }

  if((rc = hex_encode(sum, rc, pOut, lenOut)) < 0) {
    return rc;
  }

  return rc;
}


int auth_digest_getH1(const char *username, const char *realm, const char *pass, 
                      char *pOut, unsigned int lenOut) {

  int rc = 0;
  char buf[1024];

  if((rc = snprintf(buf, sizeof(buf), "%s:%s:%s", username ? username : "", realm ? realm : "", 
                    pass ? pass : "")) < 0) {
    return rc;
  }

  rc = md5hex(buf, rc, pOut, lenOut);

  VSX_DEBUG_AUTH( LOG(X_DEBUG("Auth-Digest H1: '%s' -> '%s'"), buf, pOut) );

  return rc;
}

int auth_digest_getH2(const char *method, const char *uri, char *pOut, unsigned int lenOut) {

  int rc = 0;
  char buf[1024];

  if((rc = snprintf(buf, sizeof(buf), "%s:%s", method ? method : "", uri ? uri : "")) < 0) {
    return rc;
  }

  rc = md5hex(buf, rc, pOut, lenOut);

  VSX_DEBUG_AUTH( LOG(X_DEBUG("Auth-Digest H2: '%s' -> '%s'"), buf, pOut) );

  return rc;
}

int auth_digest_getDigest(const char *h1, const char *nonce, const char *h2, 
                          const char *nc, const char *cnonce, const char *qop,
                          char *pOut, unsigned int lenOut) {
  int rc = 0;
  char buf[1024];
  char buf_rfc2617[1024];

  if(!h1 || !nonce || !h2 || !pOut) {
    return -1;
  }

  if(nc && cnonce && qop) {

    if((rc = snprintf(buf_rfc2617, sizeof(buf_rfc2617), "%s%s%s%s%s%s", 
                                                  nc ? nc : "", nc ? ":" : "",
                                                  cnonce ? cnonce : "", cnonce ? ":" : "",
                                                  qop ? qop : "", qop ? ":" : "")) < 0) {
      return rc;
    }

  } else {
    buf_rfc2617[0] = '\0';
  }

  if((rc = snprintf(buf, sizeof(buf), "%s:%s:%s%s", h1, nonce, buf_rfc2617, h2)) < 0) {
    return rc;
  }


  rc = md5hex(buf, rc, pOut, lenOut);

  VSX_DEBUG_AUTH( LOG(X_DEBUG("Auth-Digest digest-compute: '%s', nc:cnonce:qop '%s' -> '%s'"), buf, buf_rfc2617, pOut) );

  return rc;
}
