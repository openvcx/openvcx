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


static int getHostAndPorts(const char *str, char host[], size_t szHost,
                           uint16_t ports[], size_t numPorts, int nodupPorts, 
                           char uri[], size_t szuri) {
  const char *p, *p2;
  size_t sz;

  if((p = strstr(str, ":"))) {
    sz = p - str;
    p++;
  } else {
    sz = strlen(str);
    p = NULL;
  }

  if(host && szHost > 0) {
    if(sz >= szHost) {
      sz = szHost -1;
    }
    memcpy(host, str, sz);
    host[sz] = '\0';
  }
  if(uri && szuri > 0) {
    p2 = p; 
    MOVE_UNTIL_CHAR(p2, '/');
    if((sz  = strlen(p2)) >= szuri) {
      sz = szuri - 1;
    }
    memcpy(uri, p2, sz);    
    uri[sz] = '\0';
  }

  if(p) {
    return capture_parsePortStr(p, ports, numPorts, nodupPorts);
  }

  if(sz == 0 || inet_addr(host) != INADDR_NONE ||
    !strncmp(host, "255.255.255.255", sz)) {
    LOG(X_ERROR("No port number specified"));
    return -1;
  }

  return 0;
}

STREAM_PROTO_T strutil_convertProtoStr(const char *protoStr) {

  if(!strcasecmp("m2t", protoStr)) {
    return STREAM_PROTO_MP2TS;
  } else if(!strcasecmp("rtp", protoStr) || !strcasecmp("native", protoStr)) {
    return STREAM_PROTO_RTP;
  } else {
    LOG(X_ERROR("Unrecognized protocol string: %s"), protoStr);
    return STREAM_PROTO_INVALID;
  }
}

int strutil_convertDstStr(const char *dstArg, char dstHost[], size_t szHost,
                         uint16_t dstPorts[], size_t numDstPorts, 
                         char dstUri[], size_t szUri) {
  int numPorts = 0;

  if(!dstArg) {
    LOG(X_ERROR("No destination host specified"));
    return -1;
  } else if((numPorts = getHostAndPorts(dstArg, dstHost, szHost, dstPorts, numDstPorts, 0, dstUri, szUri)) < 0) {
    return -1;
  } else  if((numPorts == 0 || dstPorts[0] == 0) && inet_addr(dstHost) != INADDR_NONE) {
    LOG(X_ERROR("No destination port given"));
    return -1;
  }

  return numPorts;
}

int path_getLenNonPrefixPart(const char *path) {
  size_t idx;
  size_t sz = strlen(path);

  for(idx = sz; idx > 0; idx--) {
    switch(path[idx - 1]) {
      case '\\':
      case '/':
        return sz;
      case '.':
        return idx - 1;
      default:
        break;
    }
  }
  return sz;
}

int path_getLenDirPart(const char *path) {
  size_t idx;
  size_t sz = strlen(path);

  for(idx = sz; idx > 0; idx--) {
    if(path[idx - 1] == DIR_DELIMETER && (idx == 1 || path[idx - 2] != DIR_DELIMETER)) {
      return idx;
    }
  }
  return idx;
}

int get_port_range(const char *str, uint16_t *pstart, uint16_t *pend) {
  const char *p, *p2;
  char buf[32];

  p = str;
  if(!p || !(p2 = strstr(str, "-")) || p2 - p > sizeof(buf) - 1) {
    return -1;
  }
  if(pstart) {
    memcpy(buf, p, p2 - p);
    buf[p2 - p] = '\0';
    *pstart = atoi(buf);
  }

  while(*p2 == '-') {
    p2++;
  }
  if(pend) {
    *pend = atoi(p2);
  }

  return 0;
}

int strutil_parseDimensions(const char *str, int *px, int *py) {
  const char *p, *p2;
  char tmp[32];

  //
  // parses a dimensions string such as 'w x h'
  //

  if(!str || !px || !py) {
    return -1;
  }

  p = str;
  while(*p != '\0' && (*p < '0' || *p > '9')) {
    p++;
  }
  p2 = p;
  while(*p2 != '\0' && *p2 >= '0' && *p2 <= '9') {
    p2++;
  }

  if((ptrdiff_t) (p2 - p) > sizeof(tmp) - 1) {
    return -1;
  }
  memcpy(tmp, p, (p2 - p));
  tmp[(p2 - p)] = '\0';
  *px = atoi(tmp);

  while(*p2 != '\0' && (*p2 < '0' || *p2 > '9')) {
    p2++;
  }

  p = p2;
  while(*p2 != '\0' && *p2 >= '0' && *p2 <= '9') {
    p2++;
  }
  memcpy(tmp, p, (p2 - p));
  tmp[(p2 - p)] = '\0';
  *py = atoi(tmp);

  return 0;
}


static const char *find_end(const char *line) {
  const char *p = line;
  int inquote = 0;

  while(*p != '\0' && *p != '\n') {
    if(*p == '\"') {
      inquote = !inquote;
    } else if(!inquote && *p == ',') {
      return p;
    }
    p++;
  }
  return p;
}

const char *strutil_skip_key(const char *p0, unsigned int len) {
  const char *p = p0;

  p += len;

  while(*p == ' ' || *p == '\t') {
    p++;
  }
  if(*p != '=') {
    LOG(X_ERROR("Invalid parse key element '%s'"), p0);
    return NULL;
  }
  while(*p == '=') {
    p++;
  }
  while(*p == ' ' || *p == '\t') {
    p++;
  }
  //TODO: ok.. this is bad
  avc_strip_nl((char *) p, strlen(p), 0);

  return p;
}

int strutil_parse_csv(STRUTIL_PARSE_CSV_TOKEN cbToken, void *pCbTokenArg, const char *line) {
  const char *p, *p2;
  unsigned int sz;
  int rc = 0;
  char buf[1024];

  if(!cbToken || !line) {
    return -1;
  }

  p = (char *) line;
  while(*p != '\n' && *p != '\0') {

    p2 = (char *) find_end(p);
    if(*p2 != '\n' && *p2 != '\0') {
      sz = MIN(p2 - p, sizeof(buf) - 1);
      while(sz > 1 && (p[sz - 1] == ' ' || p[sz - 1] == '\t')) {
        sz--;
      }
      memcpy(buf, p, sz);
      buf[sz] = '\0';
      p = buf;
      p2++;
    }

    while(*p == ' ' || *p == '\t') {
      p++;
    }
    //fprintf(stderr, "line: --%s--\n", p);

    if((rc = cbToken(pCbTokenArg, p)) < 0) {
      break;
    }

    p = p2;
  }

  return rc;
}

int strutil_read_rgb(const char *str, unsigned char RGB[3]) {
  unsigned int i;
  int val;
  char tmp[4];
  const char *p = str;

  RGB[0] = RGB[1] = RGB[2] = 0;

  if(p[0] != '0' && (p[1] != 'x' || p[1] != 'X')) {
    return -1;
  }
  p += 2;
  tmp[2] = '\0';

  for(i = 0; i < 3; i++) {

    if((tmp[0] = p[0]) == '\0' || (tmp[1] = p[1]) == '\0') {
      break;
    } 

    if((val = strtol(tmp, NULL, 16)) < 0 || val > 255) {
      val = 0;
    }
    RGB[i] = (unsigned char) val;
    p += 2;
  }

  return 0;
}

int strutil_parse_pair(const char *str, void *pParseCtxt, PARSE_PAIR_CB cbParseElement) {
  int rc;
  int count = 0;
  char buf[PARSE_PAIR_BUF_SZ];
  const char *p, *p2, *pdata;

  if(!pParseCtxt || !cbParseElement) {
    return -1;
  }

  p = str;

  while(p && count++ < 2) {

    if((p2 = strchr(p, ','))) {
      pdata = capture_safe_copyToBuf(buf, sizeof(buf), p, p2);
    } else {
      pdata = p;
    }

    if(pdata) {
      while(*pdata == ' ') {
        pdata++;
      }
    }

    p = pdata;

    if((rc = cbParseElement(pParseCtxt, pdata)) < 0) {
      return rc;
    }

    if((p = p2)) {
      p++;
    }
  }

  return count;
}

int64_t strutil_read_numeric(const char *s, int bAllowbps, int bytesInK, int dflt) {
  int64_t val;
  char buf[64];
  int multiplier = 1;
  int haveFloat = 0;
  const char *p, *pnum;

  if(!s) {
    return -1;
  }

  if(bytesInK == 0) {
    bytesInK = 1024;
  }

  p = pnum = s; 
  while((*p >= '0' && *p <= '9') || *p == '.') {
    if(*p == '.') {
      haveFloat = 1;
    }
    p++;
  }
  
  if(*p != '\0') {
    strncpy(buf, s, MIN(p - s, sizeof(buf) - 1));
    pnum = buf;
    if(*p == 'b' || *p == 'B') {
      multiplier = 1;
    } else if(*p == 'K' || *p == 'k') {
      multiplier = bytesInK;
    } else if(*p == 'M' || *p == 'm') {
      multiplier = bytesInK * bytesInK;
    }

    if(bAllowbps && multiplier > 1 && p[1] == 'b') {
      multiplier *= 8;
    }

  } else if(dflt == 1000 || dflt == 1024) {
    // If the expected value is in K
    multiplier = bytesInK;
  }

  if(haveFloat) {
    val = (int64_t) (atof(pnum) * multiplier);
  } else {
    val = (int64_t) atoi(pnum) * multiplier;
  }

  return val;
}

int strutil_isTextFile(const char *path) {
  FILE_STREAM_T fs;
  int rc = 0;
  unsigned char buf[256];
  size_t sz;
  size_t idx;
  FILE_OFFSET_T fSize;

  if(OpenMediaReadOnly(&fs, path) != 0) {
    return 0;
  }

  fSize = fs.size;

  while(fs.offset < fSize) {

    sz = sizeof(buf);
    if(fs.offset + sz > fSize) {
      sz = (size_t) (fSize - fs.offset);
    }

    if(ReadFileStream(&fs, buf, sz) != sz) {
      break;
    }

    // Simply check if the file is a text file
    for(idx = 0; idx < sz; idx++) {
      //if(!isascii(buf[idx])) {
      if(!avc_istextchar(buf[idx])) {
        CloseMediaFile(&fs);
        return 0;
      }
    }

    if(fs.offset == fSize) {
      rc = 1;
    }

  }

  CloseMediaFile(&fs);

  return rc;
}

const char *strutil_getFileExtension(const char *filename) {
  size_t len;
  const char *pext = NULL;

  if((len = strlen(filename)) >= 2) {
    pext = &filename[len - 1];
    while(pext > filename + 1 && *(pext - 1) != '.') {
      pext--;
    }     
  }     

  return pext;
}

const char *strutil_getFileName(const char *path) {
  size_t sz;

  sz = strlen(path);

  while(sz > 0 && path[sz - 1] != DIR_DELIMETER) {
      sz--;
  }

  return &path[sz];
}
