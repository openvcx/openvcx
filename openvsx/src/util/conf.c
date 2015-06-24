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


const char *conf_find_keyval(const KEYVAL_PAIR_T *pKvs, const char *key) {
  const KEYVAL_PAIR_T *pKv = pKvs;

  while(pKv) { 
    if(pKv->key[0] != '\0' &&
       !strncasecmp((char *) key, (char *) pKv->key, sizeof(pKv->val))) {
      return (char *) pKv->val;
    }
    pKv = pKv->pnext;
  }

  return NULL;
}

static const char *conf_find_keyval_multi(const KEYVAL_PAIR_T *pKvs, const char *key, unsigned int idx) {
  const KEYVAL_PAIR_T *pKv = pKvs;

  while(pKv) { 
    if(pKv->key[0] != '\0' &&
       !strncasecmp((char *) key, (char *) pKv->key, sizeof(pKv->val))) {
      if(idx == 0) {
        return (char *) pKv->val;
      } else {
        idx--;
      }
    }
    pKv = pKv->pnext;
  }

  return NULL;
}

int conf_load_addr_multi(const SRV_CONF_T *pConf, const char *addr[], unsigned int max,
                         const char *key) {
  const char *parg;
  unsigned int idx;

  for(idx = 0; idx < max; idx++) {

    if((!addr[idx] || addr[idx][0] == '\0')) {
      if((parg = conf_find_keyval_multi(pConf->pKeyvals, key, idx))) {
        addr[idx] = parg;
      } else {
        break;
      }
    }
  }

  return (int) idx;
}


int conf_parse_keyval(KEYVAL_PAIR_T *pKv, const char *buf, 
                      unsigned int len, const char delim, int dequote_val) {
  int rc = 0;
  const char *p = buf;
  const char *p2, *p3, *p4;
  unsigned int lenkey, lenval, idx;

  if(!pKv || !buf) {
    return -1;
  }

  memset(pKv, 0, sizeof(KEYVAL_PAIR_T));
  if(len == 0) {
    len = strlen(buf); 
  }

  while((*p == ' ' || *p == '\t') && (ptrdiff_t) (p - buf) < (ptrdiff_t)len) {
    p++;
  }

  if(*p == '\0' || *p == '#' || *p == '\r' || *p == '\n') {
    return 0;
  }

  p2 = p;

  while(*p2 != delim && *p2 != '\0' && (ptrdiff_t)(p2 - buf) < (ptrdiff_t)len) {
    p2++;
  }

  p3 = p2;

  while((*p3 == delim || *p3 == ' ' || *p3 == '\t') &&
         (ptrdiff_t)(p3 - buf) < (ptrdiff_t)len) {
    p3++;
  }

  p4 = p3;

  while(*p4 != '\0' && (ptrdiff_t)(p4 - buf) < (ptrdiff_t)len) {
    p4++;
  }

  if(p4 == p3 && p2 == p) {
//fprintf(stderr, "0x%x 0x%x 0x%x 0x%x\n", p, p2, p3, p4);
    return 0;
  }

  
  if((lenkey = (p2 - p)) > sizeof(pKv->key) - 1) {
    LOG(X_WARNING("Key size truncated from %d -> %d"), lenval, sizeof(pKv->val) - 1);
    lenkey = sizeof(pKv->key) - 1;
  }
  if(lenkey > 0) {
    memcpy(pKv->key, p, lenkey);
    rc++;
  }

  if(dequote_val && *p3 == '"') {
    p3++;
  }
  if((lenval = (p4 - p3)) > sizeof(pKv->val) - 1) {
    LOG(X_WARNING("Value size truncated from %d -> %d"), lenval, sizeof(pKv->val) - 1);
    lenval = sizeof(pKv->val) - 1;
  }
  if(lenval > 0) {
    memcpy(pKv->val, p3, lenval);
    rc++;
  }

  for(idx = lenkey; idx > 0; idx--) {
    if(pKv->key[idx - 1] == '\r' || pKv->key[idx - 1] == '\n' || 
       pKv->key[idx - 1] == ' ' || pKv->key[idx - 1] == '\t') {
      pKv->key[idx - 1] = '\0';
    } else {
      break;
    }
  }

  for(idx = lenval; idx > 0; idx--) {
    if(pKv->val[idx - 1] == '\r' || pKv->val[idx - 1] == '\n' || 
       pKv->val[idx - 1] == ' ' || pKv->val[idx - 1] == '\t') {
      pKv->val[idx - 1] = '\0';
    } else {
      break;
    }
  }

  if(dequote_val && idx > 0 && pKv->val[idx - 1] == '"') {
    pKv->val[idx - 1] = '\0';
  }

  //fprintf(stdout, "got conf pair:'%s'='%s'\n", pKv->key, pKv->val);

  return rc; 
}

SRV_CONF_T *conf_parse(const char *path) {
  FILE_HANDLE fp;
  SRV_CONF_T *pConf = NULL;
  KEYVAL_PAIR_T kv;
  KEYVAL_PAIR_T *pKv = NULL;
  KEYVAL_PAIR_T *pKvPrev = NULL;
  char buf[1024];
  unsigned int numEntries = 0;

  if(!path) {
    return NULL;
  }

  if((fp = fileops_Open(path, O_RDONLY)) == FILEOPS_INVALID_FP) {
    LOG(X_CRITICAL("Unable to open file for reading: %s"), path);
    return NULL;
  }

  pConf = (SRV_CONF_T *) avc_calloc(1, sizeof(SRV_CONF_T));

  while(fileops_fgets(buf, sizeof(buf) - 1, fp)) {

    if(conf_parse_keyval(&kv, buf, strlen(buf), '=', 0) > 0) {

      if((pKv = (KEYVAL_PAIR_T *) avc_calloc(1, sizeof(KEYVAL_PAIR_T))) == NULL) {
        LOG(X_CRITICAL("Failed to allocate %d"), sizeof(KEYVAL_PAIR_T));
        break;
      }

      memcpy(pKv->key, kv.key, sizeof(pKv->key));
      memcpy(pKv->val, kv.val, sizeof(pKv->val));

      if(pKvPrev == NULL) {
        pConf->pKeyvals = pKv;
      } else {
        pKvPrev->pnext = pKv;
      }

      pKvPrev = pKv;
      numEntries++;

    }

  }

  fileops_Close(fp);

  return pConf;
}

void conf_free(SRV_CONF_T *pConf) {
  KEYVAL_PAIR_T *pnext;
  if(pConf) {
    while(pConf->pKeyvals) {
      pnext = pConf->pKeyvals->pnext;
      free(pConf->pKeyvals);
      pConf->pKeyvals = pnext;
    }
    free(pConf);
  }
}

#if 0
void conf_dump(const SRV_CONF_T *pConf) {
  const KEYVAL_PAIR_T *pKv = pConf->pKeyvals;

  while(pKv) {
    fprintf(stderr, "'%s'->'%s'\n", pKv->key, pKv->val);
    pKv = pKv->pnext;
  }

}

#endif // 0
