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


#ifndef __CONF_H__
#define __CONF_H__

#include "unixcompat.h"

#ifndef WIN32

#include <arpa/inet.h>

#endif // WIN32


#define CONFKEY_MAXLEN        64 
#define KEYVAL_MAXLEN        448

typedef struct KEYVAL_PAIR {
  struct KEYVAL_PAIR    *pnext;
  char                   key[CONFKEY_MAXLEN];
  char                   val[KEYVAL_MAXLEN];
} KEYVAL_PAIR_T;

typedef struct SRV_CONF {
  KEYVAL_PAIR_T         *pKeyvals;
} SRV_CONF_T;

const char *conf_find_keyval(const KEYVAL_PAIR_T *pKvs, const char *key);
int conf_load_vals_multi(const SRV_CONF_T *pConf, const char *vals[], unsigned int max,
                         const char *key);

int conf_parse_keyval(KEYVAL_PAIR_T *pKv, const char *buf, 
                      unsigned int len, const char delim, int dequote_val);

SRV_CONF_T *conf_parse(const char *parse);
void conf_free(SRV_CONF_T *pConf);

#endif // __CONF_H__
