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


#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "logutil.h"
#include "mixer/configfile.h"


#define MAX_CONFIG_ENTRY_LENGTH          1024

#define LOG_CTXT  NULL


static void config_free_entry(CONFIG_ENTRY_T **ppEntry) {

  if((*ppEntry)->key) {
    free((*ppEntry)->key);
    (*ppEntry)->key = NULL;
  }

  if((*ppEntry)->val) {
    free((*ppEntry)->val);
    (*ppEntry)->val = NULL;
  }

  free(*ppEntry);
  *ppEntry = NULL;

}

static const char *dequote(const char *s, unsigned int *psz) {

  while(*s == '"') {
    s++;
    (*psz)--;
  }

  while(*psz > 0 && (s[*psz - 1] == '"' || s[*psz - 1] == '\r' || s[*psz - 1] == '\n' ||
                     s[*psz - 1] == ' ' || s[*psz - 1] == '\t')) {
    (*psz)--;
  }

  return s;
}

static int config_parse_entry(const char *buf,
                              char *key,
                              char *val,
                              const char delim) {
  int rc = 0;
  size_t len;
  const char *p = buf;
  const char *p2, *p3, *p4;
  unsigned int lenkey, lenval;


  if(!buf || !key || !val) {
    return -1;
  }

  len = strlen(buf);
 
  while((*p == ' ' || *p == '\t') && (p - buf) < len) {
    p++;
  }

  if(*p == '\0' || *p == '#' || *p == '\r' || *p == '\n') {
    return 0;
  }

  p2 = p;

  while(*p2 != delim && *p2 != '\0' && (p2 - buf) < len) {
    p2++;
  }

  p3 = p2;

  while((*p3 == delim || *p3 == ' ' || *p3 == '\t') && (p3 - buf) < len) {
    p3++;
  }

  p4 = p3;

  while(*p4 != '\0' && (p4 - buf) < len) {
    p4++;
  }

  if(p4 == p3 && p2 == p) {
    return 0;
  }

  if((lenkey = (p2 - p)) > MAX_CONFIG_ENTRY_LENGTH - 1) {
    lenkey = MAX_CONFIG_ENTRY_LENGTH - 1;
  }

  p = dequote(p, &lenkey);

  if(lenkey > 0) {
    memcpy(key, p, lenkey);
    key[lenkey] = '\0'; 
    rc++;
  }

  if((lenval = (p4 - p3)) > MAX_CONFIG_ENTRY_LENGTH - 1) {
    lenval = MAX_CONFIG_ENTRY_LENGTH - 1;
  }

  p3 = dequote(p3, &lenval);

  if(lenval > 0) {
    memcpy(val, p3, lenval);
    val[lenval] = '\0';
    rc++;
  }

  //fprintf(stderr, "'%s' -> '%s'\n", key, val);

  return rc;
}


static CONFIG_ENTRY_T *config_create_entry(const char *key, const char *val) {
  CONFIG_ENTRY_T *pEntry;
  size_t sz;

  if(!(pEntry = (CONFIG_ENTRY_T *) calloc(1, sizeof(CONFIG_ENTRY_T)))) {
    return NULL;
  }

  sz = strlen(key);
  if(!(pEntry->key = (char *) calloc(1, sz + 1))) {
    config_free_entry(&pEntry);
  }
  memcpy(pEntry->key, key, sz);

  sz = strlen(val);
  if(!(pEntry->val = (char *) calloc(1, sz + 1))) {
    config_free_entry(&pEntry);
  }
  memcpy(pEntry->val, val, sz);

  return pEntry;
}

CONFIG_STORE_T *config_parse(const char *path) {
  FILE *fp;
  CONFIG_STORE_T *pConf = NULL;
  CONFIG_ENTRY_T *pEntry  = NULL;
  CONFIG_ENTRY_T *pEntryPrev = NULL;
  char buf[MAX_CONFIG_ENTRY_LENGTH];
  char key[MAX_CONFIG_ENTRY_LENGTH];
  char val[MAX_CONFIG_ENTRY_LENGTH];
  unsigned int numEntries = 0;

  if(!path) {
    return NULL;
  }

  if(!(fp = fopen(path, "r"))) {
    LOG(X_CRITICAL("Unable to open file for reading: %s"), path);
    return NULL;
  }

  pConf = (CONFIG_STORE_T *) calloc(1, sizeof(CONFIG_STORE_T));

  while(fgets(buf, sizeof(buf) - 1, fp)) {

    key[0] = val[0] = '\0';
    if(config_parse_entry(buf, key, val, '=') > 0) {

      if(!(pEntry = config_create_entry(key, val))) {
        LOG(X_CRITICAL("Failed to create config entry"));
        config_free(pConf);
        pConf = NULL; 
        break;
      }

      if(!pEntryPrev) {
        pConf->pHead = pEntry;
      } else {
        pEntryPrev->pnext = pEntry;
      }

      pEntryPrev = pEntry;
      numEntries++;

    }

  }

  fclose(fp);

  return pConf;
}

void config_free(CONFIG_STORE_T *pConf) {
  CONFIG_ENTRY_T *pnext;

  if(pConf) {

    while(pConf->pHead) {

      pnext = pConf->pHead->pnext;

      config_free_entry(&(pConf->pHead));

      pConf->pHead = pnext;
    }
    free(pConf);
  }

}

const char *config_find_value(const CONFIG_STORE_T *pConf, const char *key) {
  const CONFIG_ENTRY_T *pEntry;

  if(!pConf) {
    return NULL;
  }

  pEntry = pConf->pHead;

  while(pEntry) {
    if(pEntry->key[0] != '\0' && !strncasecmp((char *) key, pEntry->key, strlen(pEntry->key) + 1)) {
      return (char *) pEntry->val;
    }
    pEntry = pEntry->pnext;
  }

  return NULL;
}

int config_find_int(const CONFIG_STORE_T *pConf, const char *key, int def) {
  const char *val;
  
  if((val = config_find_value(pConf,key))) {
    return atoi(val);
  } else {
    return def;
  }
}

void config_dump(FILE *fp, const CONFIG_STORE_T *pConf) {
  const CONFIG_ENTRY_T *pEntry  = pConf->pHead;

  if(!fp) {
    fp = stderr;
  }

  while(pEntry) {
    fprintf(fp, "'%s'->'%s'\n", pEntry->key, pEntry->val);
    pEntry = pEntry->pnext;
  }

}
