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


int srvprop_write(const SRV_PROPS_T *pProps, const char *path) {
  FILE_HANDLE fp;
  unsigned int idx;

  if(!pProps || !path) {
    return -1;
  }

  if((fp = fileops_Open(path, O_RDWR | O_CREAT)) == FILEOPS_INVALID_FP) {
    LOG(X_ERROR("Unable to open %s for writing"), path);
    //fprintf(stderr, "err:%u\n", GetLastError());
    return -1;
  }

  fileops_Write(fp, "#%s user configuration"EOL, VSX_APPNAME);
  for(idx = 0; idx < SRV_PROP_DEST_MAX; idx++) {
    fileops_Write(fp, "dstaddr%d=%s"EOL, idx + 1, pProps->dstaddr[idx]);
  }
  fileops_Write(fp, "capaddr=%s"EOL, pProps->capaddr);
  fileops_Write(fp, "captranstype=%d"EOL, pProps->captranstype);
  fileops_Write(fp, "loop=%d"EOL, pProps->loop);
  fileops_Write(fp, "loop_pl=%d"EOL, pProps->loop_pl);
  fileops_Write(fp, "live=%d"EOL, pProps->tslive);
  fileops_Write(fp, "httplive=%d"EOL, pProps->httplive);
  fileops_Write(fp, "dirsort=%d"EOL, pProps->dirsort);
  fileops_Write(fp, "xcodeprofile=%d"EOL, pProps->xcodeprofile);

  fileops_Close(fp);

  fprintf(stderr, "srvprop_write - wrote to %s %d\n", path, pProps->dirsort);

  return 0;
}

int srvprop_read(SRV_PROPS_T *pProps, const char *path) {
  FILE_HANDLE fp;
  char buf[1024];
  KEYVAL_PAIR_T kv;
  char *p;
  int rc = 0;

  memset(pProps, 0, sizeof(SRV_PROPS_T)); 

  if((fp = fileops_Open(path, O_RDONLY)) == FILEOPS_INVALID_FP) {
    return -1;
  }

  while(fileops_fgets(buf, sizeof(buf) - 1, fp)) {
    p = buf;
    while(*p == ' ' || *p == '\t') {
      p++;
    }
    if(*p == '\0' || *p == '#' || *p == '\r' || *p == '\n') {
      continue;
    }
    if(conf_parse_keyval(&kv, buf, sizeof(buf), '=', 0) > 0) {
      if(!strncasecmp(kv.key, "dstaddr1", sizeof(kv.key))) {
        strncpy(pProps->dstaddr[0], kv.val, sizeof(pProps->dstaddr[0]) - 1);
      } else if(!strncasecmp(kv.key, "dstaddr2", sizeof(kv.key))) {
        strncpy(pProps->dstaddr[1], kv.val, sizeof(pProps->dstaddr[1]) - 1);
      } else if(!strncasecmp(kv.key, "dstaddr3", sizeof(kv.key))) {
        strncpy(pProps->dstaddr[2], kv.val, sizeof(pProps->dstaddr[2]) - 1);
      } else if(!strncasecmp(kv.key, "capaddr", sizeof(kv.key))) {
        strncpy(pProps->capaddr, kv.val, sizeof(pProps->capaddr) - 1);
      } else if(!strncasecmp(kv.key, "captranstype", sizeof(kv.key))) {
        pProps->captranstype = atoi(kv.val);  
      } else if(!strncasecmp(kv.key, "loop", sizeof(kv.key))) {
        pProps->loop = atoi(kv.val);  
      } else if(!strncasecmp(kv.key, "loop_pl", sizeof(kv.key))) {
        pProps->loop_pl = atoi(kv.val);  
      } else if(!strncasecmp(kv.key, "live", sizeof(kv.key))) {
        pProps->tslive = atoi(kv.val);  
      } else if(!strncasecmp(kv.key, "httplive", sizeof(kv.key))) {
        pProps->httplive = atoi(kv.val);  
      } else if(!strncasecmp(kv.key, "dirsort", sizeof(kv.key))) {
        pProps->dirsort = atoi(kv.val);  
      } else if(!strncasecmp(kv.key, "xcodeprofile", sizeof(kv.key))) {
        pProps->xcodeprofile = atoi(kv.val);  
      }
    }

  }

  fileops_Close(fp);

  return rc;
}
