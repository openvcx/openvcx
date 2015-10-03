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


#ifndef __RTMP_AUTH_H__
#define __RTMP_AUTH_H__

#define RTMP_AUTHMOD_ADOBE_STR    "adobe"
#define RTMP_AUTHMOD_LLNW_STR     "llnw"

#define RTMP_AUTH_REASON_FAILED   "authfailed"
#define RTMP_AUTH_REASON_NEEDAUTH "needauth"

typedef enum RTMP_AUTHMOD {
  RTMP_AUTHMOD_UNKNOWN             = 0,
  RTMP_AUTHMOD_ADOBE               = 1,
  RTMP_AUTHMOD_LLNW                = 2,
} RTMP_AUTHMOD_T;

typedef enum RTMP_AUTH_STATE {
  RTMP_AUTH_STATE_FATAL            = -2,
  RTMP_AUTH_STATE_INVALID          = -1,
  RTMP_AUTH_STATE_UNKNOWN          = 0,
  RTMP_AUTH_STATE_NEED_CHALLENGE   = 1,
  RTMP_AUTH_STATE_HAVE_CHALLENGE   = 2
} RTMP_AUTH_STATE_T;

typedef struct RTMP_AUTH_PERSIST {
  char                           salt[AUTH_ELEM_MAXLEN];
  char                           challenge[AUTH_ELEM_MAXLEN];
  char                           opaque[AUTH_ELEM_MAXLEN];
} RTMP_AUTH_PERSIST_T;

typedef struct RTMP_AUTH_PERSIST_NODE {
  struct RTMP_AUTH_PERSIST_NODE *pnext;
  TIME_VAL                       tm;
  RTMP_AUTH_PERSIST_T            s; 
} RTMP_AUTH_PERSIST_NODE_T;

typedef struct RTMP_AUTH_PERSIST_STORAGE_T {
  unsigned int                   max;
  RTMP_AUTH_PERSIST_NODE_T      *phead;
  pthread_mutex_t                mtx;
} RTMP_AUTH_PERSIST_STORAGE_T;

typedef struct RTMP_AUTH_CTXT {
  RTMP_AUTH_PERSIST_T            p;
  RTMP_AUTHMOD_T                 authmod;
  char                           challenge_cli[AUTH_ELEM_MAXLEN];
  char                           response[AUTH_ELEM_MAXLEN];
  RTMP_AUTH_STATE_T              lastAuthState;
  int                            consecretry;
} RTMP_AUTH_CTXT_T;

typedef struct RTMP_AUTH_PARSE_CTXT {
  AUTH_LIST_T                    authList;
  char                           errorStr[64];
  char                           authmodStr[16];

  int                            index;
} RTMP_AUTH_PARSE_CTXT_T;

typedef struct RTMP_AUTH_STORE {
  RTMP_AUTH_CTXT_T               ctxt;
  const AUTH_CREDENTIALS_STORE_T *pcreds;
} RTMP_AUTH_STORE_T;


int rtmp_auth_compute(RTMP_AUTH_CTXT_T *pAuth, const struct AUTH_CREDENTIALS_STORE *pcreds);
int rtmp_auth_create_challenge(RTMP_AUTH_PERSIST_T *pPersist);
int rtmp_auth_parse_resp(const char *authDescription, RTMP_AUTH_PARSE_CTXT_T *pCtxt);
int rtmp_auth_parse_req(const char *url, RTMP_AUTH_PARSE_CTXT_T *pParseCtxt);
RTMP_AUTHMOD_T rtmp_auth_getauthmod(const RTMP_AUTH_PARSE_CTXT_T *pParseCtxt);
const char *rtmp_auth_getauthmodstr(RTMP_AUTHMOD_T authmod);

int rtmp_auth_cache_remove(RTMP_AUTH_PERSIST_STORAGE_T *pcachedAuth,
                           const char *opaque, RTMP_AUTH_PERSIST_T *pElem);
int rtmp_auth_cache_store(RTMP_AUTH_PERSIST_STORAGE_T *pcachedAuth, const RTMP_AUTH_PERSIST_T *pElem);
int rtmp_auth_cache_close(RTMP_AUTH_PERSIST_STORAGE_T *pcachedAuth);
int rtmp_auth_cache_init(RTMP_AUTH_PERSIST_STORAGE_T *pcachedAuth);
//void rtmp_auth_cache_dump(const RTMP_AUTH_PERSIST_STORAGE_T *pcachedAuth);

#endif // __RTMP_AUTH_H__
