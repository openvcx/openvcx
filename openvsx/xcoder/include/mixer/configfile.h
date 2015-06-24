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


#ifndef __CONFIG_H__
#define  __CONFIG_H__

#include <stdio.h>

typedef struct CONFIG_ENTRY {
  char                  *key;
  char                  *val;
  struct CONFIG_ENTRY   *pnext;
} CONFIG_ENTRY_T;

typedef struct CONFIG_STORE {
  CONFIG_ENTRY_T       *pHead;
} CONFIG_STORE_T;

const char *config_find_value(const CONFIG_STORE_T *pEntry, const char *key);
int config_find_int(const CONFIG_STORE_T *pConf, const char *key, int def);
CONFIG_STORE_T *config_parse(const char *path);
void config_free(CONFIG_STORE_T *pConf);
void config_dump(FILE *fp, const CONFIG_STORE_T *pConf);

#endif // __CONFIG_H__

