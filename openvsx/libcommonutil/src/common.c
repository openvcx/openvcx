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

#include "unixcompat.h"
#include "logutil.h"
#include <stdio.h>
#include <string.h>
#include "commonutil.h"
#include "vsx_dbg.h"


int avc_isnumeric(const char *s) {

  if(s && *s != '\0') {

    while(*s != '\0') {
      if(*s != '.' && (*s < '0' || *s > '9')) {
        return 0; 
      }
      s++;
    }

    return 1; 
  }

  return 0;
}

void *avc_calloc(size_t count, size_t size) {
  void *p;

  if((p = calloc(count, size)) == NULL) {
    LOG(X_CRITICAL("Failed to calloc %d x %d"), count, size);
  }

  VSX_DEBUG_MEM( LOG(X_DEBUG("MEM - avc_calloc %d x %d = %d at 0x%x"), count, size, count * size, p); );

  return p;
}

void *avc_realloc(void *porig, size_t size) {
  void *p;

  if((p = realloc(porig, size)) == NULL) {
    LOG(X_CRITICAL("Failed to realloc %d"), size);
    return NULL;
  }

  VSX_DEBUG_MEM( LOG(X_DEBUG("MEM - avc_realloc 0x%x, %d now at 0x%x"), porig, size, p); );

  return p;
}

void *avc_recalloc(void *porig, size_t size, size_t size_orig) {
  void *p;

  if(porig && size_orig > size) {
    return NULL;
  } else if(size_orig == size) {
    return porig;
  }


  if((p = avc_calloc(1, size)) == NULL) {
    return NULL;
  }

  VSX_DEBUG_MEM( LOG(X_DEBUG("MEM - avc_recalloc 0x%x, %d now at 0x%x, %d"), porig, size_orig, p, size); );

  if(porig) {
    memcpy(p, porig, size_orig); 
  }

  if(porig) {
    avc_free(&porig);
  }

  return p;
}

void avc_free(void **pp) {
  if(pp && *pp) {
    VSX_DEBUG_MEM( LOG(X_DEBUG("MEM - avc_free at 0x%x"), *pp); );
    free(*pp);
    *pp = NULL;
  }
}


int avc_istextchar(const unsigned char c) {

  if(c == 0 || c == '\n' || c == '\r' || c == '\t' || CHAR_PRINTABLE(c)) {
    return 1;
  } else {
    return 0;
  }
}

int avc_strip_nl(char *str, size_t sz, int stripws) {
  while(sz > 0) {
    if(str[sz - 1] == '\r' || str[sz - 1] == '\n' ||
       (stripws && (str[sz - 1] == ' ' || str[sz - 1] == '\t'))) {
      str[sz - 1] = '\0';
      sz--;
    } else {
      break;
    }
  }
  return sz;
}

char *avc_dequote(const char *p, char *buf, unsigned int szbuf) {
  size_t sz;
  if(p && p[0] == '\"') {
    if(buf == p || !buf) {
      buf = (char *) &p[1];
    } else {
      strncpy(buf,  &(p[1]), szbuf - 1);
      buf[szbuf - 1] = '\0';
    }
    if((sz = strlen(buf)) > 0) {
      if(buf[sz - 1] == '\"') {
        buf[sz - 1] = '\0';
      }
    }
    return buf;
  }
  return (char *) p;
}

static void dumpAscii(FILE *fp, const unsigned char *buf, 
                      unsigned int len, unsigned int start) {
  unsigned int idx;

  fprintf(fp, "   ");

  for(idx = 0; idx < start; idx++) {
    fprintf(fp, "   ");
  }
  fprintf(fp, "|");

  for(idx = 0; idx < len; idx++) {
    if(CHAR_PRINTABLE(buf[idx])) {
      fprintf(fp, "%c", buf[idx]);
    } else {
      fprintf(fp, ".");
    }

  }
  if(start > 0) {
    for(;idx % 16 != 0; idx++) {
      fprintf(fp, " ");
    }
  }
  fprintf(fp, "|");
}

void avc_dumpHex(void *fparg, const unsigned char *buf, unsigned int len, int ascii) {
  unsigned int idx;
  unsigned int mark = 0;
  FILE *fp = (FILE *) fparg;

  for(idx = 1; idx <= len; idx++) {

    if(idx % 16 == 1) {
      fprintf(fp, "%.6x ", idx - 1);
    }

    fprintf(fp, " %2.2x", buf[idx-1]);
    if(idx % 16 == 0) {
      if(ascii) {
        dumpAscii(fp, &buf[idx - 16], 16, 0); 
      }
      mark = idx;
      fprintf(fp, "\n");
    }
  }
  if(idx > 1 && idx % 16 != 1) {
    if(ascii) {
      dumpAscii(fp, &buf[len - (idx - mark) + 1], idx - mark - 1, 17 - (idx - mark)); 
    }
    fprintf(fp, "\n");
  }
}

const char *avc_getPrintableDuration(unsigned long long duration, unsigned int timescale) {
  static char buf[64];
  long durationSec = (long) (duration / timescale);

  snprintf(buf, sizeof(buf) - 1, "%02ld:%02ld:%02ld.%04ld",
      (long)durationSec / 3600, (long)((durationSec / 60)%60), (durationSec % 60),
      (long)(((double)duration / timescale - durationSec) * 10000));

  return buf;
}

const unsigned char *avc_binstrstr(const unsigned char *buf,
                               unsigned int len,
                               const unsigned char *needle,
                               unsigned int lenneedle) {
  unsigned int idx;
  unsigned int idxneedle = 0;
  int diffchar = 0;

  for(idx = 0; idx < len; idx++) {
    if(buf[idx] == needle[idxneedle]) {

      if(++idxneedle == lenneedle) {
        return &buf[idx + 1];
      }
      if(!diffchar && needle[idxneedle] != needle[idxneedle - 1]) {

        while(idx < len && buf[idx] == needle[idxneedle - 1]) {
          idx++;
        }
        idx--;
        diffchar = 1;
      }

    } else {
      diffchar = 0;
      idxneedle = 0;
    }
  }
  return NULL;
}

#if defined(__linux__) 

#include <ctype.h>
char * avc_strcasestr(const char *s, const char *find) {
        char c, sc;
        size_t len;

        if ((c = *find++) != 0) {
                c = tolower((unsigned char)c);
                len = strlen(find);
                do {
                        do {
                                if ((sc = *s++) == 0)
                                        return (NULL);
                        } while ((char)tolower((unsigned char)sc) != c);
                } while (strncasecmp(s, find, len) != 0);
                s--;
        }
        return ((char *)s);
}
#endif // __linux__
