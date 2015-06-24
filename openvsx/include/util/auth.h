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


#ifndef __AUTH_H__
#define __AUTH_H__

#define AUTH_LIST_MAX_ELEMENTS    12

#define AUTH_STR_BASIC            "Basic"
#define AUTH_STR_DIGEST           "Digest"

typedef struct AUTH_LIST {
  KEYVAL_PAIR_T       list[AUTH_LIST_MAX_ELEMENTS];
  unsigned int        count;
} AUTH_LIST_T;

#define AUTH_ELEM_MAXLEN     KEYVAL_MAXLEN

typedef struct AUTH_CREDENTIALS_STORE {
  char                         username[AUTH_ELEM_MAXLEN];
  char                         pass[AUTH_ELEM_MAXLEN];
  char                         realm[AUTH_ELEM_MAXLEN];
  HTTP_AUTH_TYPE_T             authtype;
} AUTH_CREDENTIALS_STORE_T;

int auth_basic_encode(const char *user, const char *pass, char *pOut, unsigned int lenOut);
int auth_basic_decode(const char *enc, char *user, unsigned int lenUser, char *pass, unsigned int lenPass);

int auth_digest_parse(const char *line, AUTH_LIST_T *pAuthList);
int auth_digest_write(const AUTH_LIST_T *pAuthList, char *pOut, unsigned int lenOut);
int auth_digest_additem(AUTH_LIST_T *pAuthList, const char *key, const char *value, int do_quote);
char *auth_digest_gethexrandom(unsigned int lenRaw, char *pOut, unsigned int lenOut);
int auth_digest_getH1(const char *username, const char *realm, const char *pass,
                      char *pOut, unsigned int lenOut);
int auth_digest_getH2(const char *method, const char *uri, char *pOut, unsigned int lenOut);
int auth_digest_getDigest(const char *h1, const char *nonce, const char *h2, 
                          const char *nc, const char *cnonce, const char *qop,
                          char *pOut, unsigned int lenOut);




#endif //__AUTH_H__
