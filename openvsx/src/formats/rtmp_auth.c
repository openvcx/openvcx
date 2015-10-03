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

#define RTMP_AUTH_CACHE_LIFETIME_SEC      15 
#define RTMP_CAP_SRV_AUTH_CACHE_SZ_MAX    10

int cbparse_authmod_descr_params(void *pArg, const char *p) {
  int rc = 0;
  AUTH_LIST_T *pAuthList = (AUTH_LIST_T *) pArg;

  if(!pAuthList || !p) {
    return -1;
  }

  //LOG(X_DEBUG("cbparse_authmod_descr_params: '%s'"), p);

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

int cbparse_authmod_descr(void *pArg, const char *p) {
  int rc = 0;
  const char *p2 = NULL;
  const char *p3 = NULL;
  RTMP_AUTH_PARSE_CTXT_T *parseCtxt = (RTMP_AUTH_PARSE_CTXT_T *) pArg;

  if(!p || !parseCtxt) {
    return -1;
  }

  parseCtxt->index++;

  MOVE_WHILE_SPACE(p);
  MOVE_WHILE_CHAR(p, '[');
  MOVE_WHILE_SPACE(p);

  //LOG(X_DEBUG("cbparse_authmod_descr2: '%s' %d"), p, parseCtxt->index);

  if(parseCtxt->index == 1) {

    p2 = p;
    MOVE_UNTIL_CHAR(p2, ']');
    if(strutil_safe_copyToBuf(parseCtxt->errorStr, sizeof(parseCtxt->errorStr), p, p2)) {
      avc_strip_nl(parseCtxt->errorStr, strlen(parseCtxt->errorStr), 1);
    }

  } else if(parseCtxt->index == 2) {
    if((p2 = strstr(p, "authmod="))) {
      p2+=8;
      p3 = p2;
      while(*p3 != '\0' && *p3 != ']' && *p3 != ' ' && *p3 != ';') {
        p3++;
      }
      strutil_safe_copyToBuf(parseCtxt->authmodStr, sizeof(parseCtxt->authmodStr), p2, p3);
    }
  } else if(parseCtxt->index == 3) {
    if(*p == '?') {
      p++;
    }
    rc = strutil_parse_delimeted_str(cbparse_authmod_descr_params, &parseCtxt->authList, p, '&');
  }

  return rc;
}

int rtmp_auth_parse_resp(const char *authDescription, RTMP_AUTH_PARSE_CTXT_T *pParseCtxt) {
  int rc = 0;

  if(!authDescription || !pParseCtxt) {
    return -1;
  }

  rc = strutil_parse_delimeted_str(cbparse_authmod_descr, pParseCtxt, authDescription, ':');

  return rc;
}

int rtmp_auth_parse_req(const char *url, RTMP_AUTH_PARSE_CTXT_T *pParseCtxt) {
  int rc = 0;
  const char *p = NULL;

  if(!url|| !pParseCtxt) {
    return -1;
  }

  if(!(p = strchr(url, '?'))) {
    return rc;
  }
  p++;

  rc = strutil_parse_delimeted_str(cbparse_authmod_descr_params, &pParseCtxt->authList, p, '&');

  if((p = conf_find_keyval(&pParseCtxt->authList.list[0], "authmod"))) {
    strncpy(pParseCtxt->authmodStr, p, sizeof(pParseCtxt->authmodStr) - 1);
  }

  return rc;
}

static char *rtmp_auth_getrandom_base64(unsigned int lenRaw, char *pOut, unsigned int lenOut) {
  int rc = 0;
  unsigned char buf[128];
  unsigned int idx;

  for(idx = 0; idx < MIN(lenRaw, sizeof(buf)); idx += 2) {
    *((uint16_t *) &buf[idx]) = (random() % RAND_MAX);
  }

  if((rc = base64_encode(buf, lenRaw, pOut, lenOut)) < 0) {
    return NULL;
  }

  return pOut;
}

int rtmp_auth_create_challenge(RTMP_AUTH_PERSIST_T *pPersist) {
  int rc = 0;

  if(!pPersist) {
    return -1;
  }

  rtmp_auth_getrandom_base64(4, pPersist->salt, sizeof(pPersist->salt));
  rtmp_auth_getrandom_base64(4, pPersist->challenge, sizeof(pPersist->challenge));
  //rtmp_auth_getrandom_base64(4, pPersist->opaque, sizeof(pPersist->opaque));
  strncpy(pPersist->opaque, pPersist->challenge, sizeof(pPersist->opaque) - 1);

  return rc;
}

static int rtmp_auth_compute_adobe(RTMP_AUTH_CTXT_T *pAuth, const AUTH_CREDENTIALS_STORE_T *pcreds) {
  int rc = 0;
  size_t sz = 0;
  char buf[512];
  unsigned char hash[MD5_DIGEST_LENGTH];

  if((pcreds->username[0] == '\0' && pcreds->pass[0] == '\0') || pAuth->p.salt[0] == '\0' ||
            (pAuth->p.challenge[0] == '\0' && pAuth->p.opaque[0] == '\0')) {
    return -1;
  }

  if(pAuth->challenge_cli[0] == '\0') {
    rtmp_auth_getrandom_base64(4, pAuth->challenge_cli, sizeof(pAuth->challenge_cli));
  }

  buf[0] = '\0';
  if((rc = snprintf(buf, sizeof(buf), "%s%s%s", pcreds->username, pAuth->p.salt, pcreds->pass)) > 0) {
    sz = rc;
  }
  if((rc = md5_calculate((const unsigned char *) buf, strlen(buf), hash)) < 0) {
    return -1;
  }
  if((rc = base64_encode(hash, rc, buf, sizeof(buf))) < 0) {
    return -1;
  }

  sz = rc;
  if((rc = snprintf(&buf[sz], sizeof(buf) - sz, "%s%s", 
                    pAuth->p.opaque ? pAuth->p.opaque : pAuth->p.challenge, pAuth->challenge_cli)) > 0) {
    sz += rc;
  }

  if((rc = md5_calculate((const unsigned char *) buf, sz, hash)) < 0) {
    return -1;
  }
  if((rc = base64_encode(hash, rc, pAuth->response, sizeof(pAuth->response))) < 0) {
    return -1;
  }

  return rc;
}

static int rtmp_auth_compute_llnw(RTMP_AUTH_CTXT_T *pAuth, const AUTH_CREDENTIALS_STORE_T *pcreds) {
  int rc = 0;
/*
  const char *s[4];
  char buf[256];
  unsigned char hash[MD5_DIGEST_LENGTH];

  if((pcreds->username[0] == '\0' && pcreds->pass[0] == '\0') || pAuth->p.salt[0] == '\0' ||
            (pAuth->p.challenge[0] == '\0' && pAuth->p.opaque[0] == '\0')) {
    return -1;
  }

  if(pAuth->challenge_cli[0] == '\0') {
    rtmp_auth_getrandom_base64(4, pAuth->challenge_cli, sizeof(pAuth->challenge_cli));
  }

  buf[0] = '\0';
  if((rc = snprintf(buf, sizeof(buf), "%s:%s:%s", 
                    pcreds->username, pcreds->realm, pAuth->p.salt, pcreds->pass)) > 0) {
    sz = rc;
  }
  if((rc = md5_calculate((const unsigned char *) buf, strlen(buf), hash)) < 0) {
    return -1;
  }
  if((rc = hex_encode(hash, rc, buf, sizeof(buf))) < 0) {
    return -1;
  }
*/

  //TODO: implement RTMP llnw auth
  return -1;

  return rc;
}

int rtmp_auth_compute(RTMP_AUTH_CTXT_T *pAuth, const AUTH_CREDENTIALS_STORE_T *pcreds) {
  int rc = 0;

  if(!pAuth || !pcreds) {
    return -1;
  }

  switch(pAuth->authmod) {
    case RTMP_AUTHMOD_ADOBE:
      return rtmp_auth_compute_adobe(pAuth, pcreds);
    case RTMP_AUTHMOD_LLNW:
      return rtmp_auth_compute_llnw(pAuth, pcreds);
    default:
      return -1;;
  }

  return rc;
}

const char *rtmp_auth_getauthmodstr(RTMP_AUTHMOD_T authmod) {
  switch(authmod) {
    case RTMP_AUTHMOD_ADOBE:
      return RTMP_AUTHMOD_ADOBE_STR;
    case RTMP_AUTHMOD_LLNW:
      return RTMP_AUTHMOD_LLNW_STR;
    default:
      break;
  }

  return "";
}

RTMP_AUTHMOD_T rtmp_auth_getauthmod(const RTMP_AUTH_PARSE_CTXT_T *pParseCtxt) {
  if(pParseCtxt) {
    if(!strcasecmp(pParseCtxt->authmodStr, RTMP_AUTHMOD_ADOBE_STR)) {
      return RTMP_AUTHMOD_ADOBE;
    } else if(!strcasecmp(pParseCtxt->authmodStr, RTMP_AUTHMOD_LLNW_STR)) {
      return RTMP_AUTHMOD_LLNW;
    }
  }
  return RTMP_AUTHMOD_UNKNOWN;
}

int rtmp_auth_cache_init(RTMP_AUTH_PERSIST_STORAGE_T *pcachedAuth) {
  int rc = 0;

  if(!pcachedAuth || pcachedAuth->phead) {
    return -1;
  }

  pcachedAuth->max = RTMP_CAP_SRV_AUTH_CACHE_SZ_MAX;
  pthread_mutex_init(&pcachedAuth->mtx, NULL);

  return rc;
}

int rtmp_auth_cache_close(RTMP_AUTH_PERSIST_STORAGE_T *pcachedAuth) {
  RTMP_AUTH_PERSIST_NODE_T *p, *ptmp;

  if(!pcachedAuth) {
    return -1;
  }

  pthread_mutex_lock(&pcachedAuth->mtx);

  p = pcachedAuth->phead;
  while(p) {
    ptmp = p;
    p = p->pnext;
    avc_free((void *) ptmp);
  }

  pcachedAuth->phead = NULL;

  pthread_mutex_unlock(&pcachedAuth->mtx);

  pcachedAuth->max = 0;
  pthread_mutex_destroy(&pcachedAuth->mtx);

  return 0;
}

int rtmp_auth_cache_store(RTMP_AUTH_PERSIST_STORAGE_T *pcachedAuth, const RTMP_AUTH_PERSIST_T *pElem) {
  int rc = -1;
  unsigned int sz = 0;
  TIME_VAL tm;
  RTMP_AUTH_PERSIST_NODE_T *p, *p_prev = NULL;
  RTMP_AUTH_PERSIST_NODE_T *pinsert = NULL;
  RTMP_AUTH_PERSIST_NODE_T *poldest = NULL;

  if(pElem->opaque[0] == '\0') {
    return -1;
  }

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP auth cache storing opaque: '%s', salt: '%s', challenge: '%s'"), 
                             pElem->opaque, pElem->salt, pElem->challenge); );

  tm = timer_GetTime();

  pthread_mutex_lock(&pcachedAuth->mtx);

  p = pcachedAuth->phead;
  while(p) {

    if(p->tm == 0 || !strcmp(pElem->opaque, p->s.opaque) || 
       p->tm + (RTMP_AUTH_CACHE_LIFETIME_SEC * TIME_VAL_US) < tm) {
      pinsert = p;
      break;
    }
    if(!poldest || poldest->tm > p->tm) {
      poldest = p;
    }

    sz++;
    p_prev = p;
    p = p->pnext;
  }

  if(!pinsert) {
    if(p_prev) {
      if(sz >= RTMP_CAP_SRV_AUTH_CACHE_SZ_MAX) {
        pinsert = poldest;
      } else if(!(pinsert = p_prev->pnext = (RTMP_AUTH_PERSIST_NODE_T *) 
                                             avc_calloc(1, sizeof(RTMP_AUTH_PERSIST_NODE_T)))) {
        pthread_mutex_unlock(&pcachedAuth->mtx);
        return -1;
      }
    } else {
      if(!(pinsert = pcachedAuth->phead = (RTMP_AUTH_PERSIST_NODE_T *) 
                                            avc_calloc(1, sizeof(RTMP_AUTH_PERSIST_NODE_T)))) {
        pthread_mutex_unlock(&pcachedAuth->mtx);
        return -1;
      }
    }
  }

  memcpy(&pinsert->s, pElem, sizeof(RTMP_AUTH_PERSIST_T));
  pinsert->tm = tm;

  pthread_mutex_unlock(&pcachedAuth->mtx);

  return rc;
}

int rtmp_auth_cache_remove(RTMP_AUTH_PERSIST_STORAGE_T *pcachedAuth, 
                           const char *opaque, RTMP_AUTH_PERSIST_T *pElem) {
  int rc = -1;
  RTMP_AUTH_PERSIST_NODE_T *p, *pfound = NULL;

  if(!opaque || opaque[0] == '\0') {
    return -1;
  }

  pthread_mutex_lock(&pcachedAuth->mtx);

  p = pcachedAuth->phead;
  while(p) {

    if(p->tm > 0 && !strcmp(opaque, p->s.opaque)) {

      pfound = p;
      break;
    }

    p = p->pnext;
  }

  if(pfound) {
    memcpy(pElem, &pfound->s, sizeof(RTMP_AUTH_PERSIST_T));
    pfound->tm = 0;
    rc = 0;
    VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP auth cache get opaque: '%s', salt: '%s', challenge: '%s'"), 
                                 pElem->opaque, pElem->salt, pElem->challenge); );
  }

  pthread_mutex_unlock(&pcachedAuth->mtx);

  return rc;
}

#if 0
void rtmp_auth_cache_dump(const RTMP_AUTH_PERSIST_STORAGE_T *pcachedAuth) {
  RTMP_AUTH_PERSIST_NODE_T *pNode = NULL;

  pNode = pcachedAuth->phead;
  fprintf(stderr, "--start--\n");
  while(pNode) {
    fprintf(stderr, "tm: %lld, %s,%s,%s\n", pNode->tm, pNode->s.opaque, pNode->s.salt, pNode->s.challenge);
    pNode = pNode->pnext;
  }
  fprintf(stderr, "--end--\n");

}
#endif // 0
