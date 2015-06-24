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

#if defined(WIN32) && !defined(__MINGW32__)

#include <windows.h>
#include <stdio.h>
#include <time.h>

#include "unixcompat.h"


int gettimeofday(struct timeval *tv, struct timezone *tz) {

#ifndef __GNUC__
#define EPOCHFILETIME (116444736000000000i64)
#else
#define EPOCHFILETIME (116444736000000000LL)
#endif

    FILETIME        ft;
    LARGE_INTEGER   li;
    __int64         t;
    static int      tzflag;

    if (tv) {
		GetSystemTimeAsFileTime(&ft);
		li.LowPart  = ft.dwLowDateTime;
		li.HighPart = ft.dwHighDateTime;
		t  = li.QuadPart;       /* In 100-nanosecond intervals */
		t -= EPOCHFILETIME;     /* Offset to the Epoch time */
		t /= 10;                /* In microseconds */
		tv->tv_sec  = (long)(t / 1000000);
		tv->tv_usec = (long)(t % 1000000);
    }

    //if (tz) {
		//if (!tzflag) {
		//	_tzset();
		//	tzflag++;
		//}
		//tz->tz_minuteswest = _timezone / 60;
		//tz->tz_dsttime = _daylight;
    //}

    return 0;

}



char *optarg;
int optind = 0;
int opterr, optopt;

int getopt(int argc, char * const argv[], const char *optstring) {
	static char *next = NULL;
	char c, *cp;

	if (optind == 0)
		next = NULL;
	optarg = NULL;

	if (next == NULL || *next == '\0') {
		if (optind == 0)
			optind++;
 
    if(optind >= argc)
      return -1;

		if ( argv[optind][0] != '-' || argv[optind][1] == '\0') {
			optarg = NULL;
			if (optind < argc)
				optarg = argv[optind];
      optind++;
			return 1;
		}
 
		if (strcmp(argv[optind], "--") == 0) {
			optind++;
			optarg = NULL;
			if (optind < argc)
				optarg = argv[optind];
			return EOF;
		}

		next = argv[optind]+1;
		optind++;
	}	
 
	c = *next++;
	cp = strchr((char *) optstring, c);
 
	if (cp == NULL || c == ':')
		return '?';
 
	cp++;
	if (*cp == ':') {
		if (*next != '\0') {
			optarg = next;
			next = NULL;
		} else if (optind < argc) {
			optarg = argv[optind];
			optind++;
		} else {
			return '?';
		}
	}
 
	return c;
}

#define MAX(x,y)  ((x) > (y) ? (x) : (y))

static const struct option *findlongopt(const char *str, const struct option *longopts) {
  const char *p = str;
  const struct option *popt = longopts;
  while(*p != '\0' && *p != '=')
    p++;

  while(popt && popt->name) {
    if(!strncmp(popt->name, str, MAX(strlen(popt->name), p - str))) {
      if(popt->has_arg) {
        if(popt->has_arg == required_argument && *p != '=') {
          fprintf(stderr, "Required argument missing for '%s'\n", popt->name);
          return NULL;
        }

        while(*p == '=')
          p++;
        optarg = (char *) p;
        if(optarg && optarg[0] == '\0') {
          optarg = NULL;
        }
      }

      return popt;
    }
    popt++;
  }

  return NULL;
}

int getopt_long(int argc, char * const argv[], const char *optstring,
                const struct option *longopts, int *longindex) {

	static char *next = NULL;
	char c, *cp;
  const struct option *longopt = NULL;

	if (optind == 0)
		next = NULL;
	optarg = NULL;

	if (next == NULL || *next == '\0') {
		if (optind == 0)
			optind++;
 
    if(optind >= argc)
      return -1;

		if ( argv[optind][0] != '-' || argv[optind][1] == '\0') {
			optarg = NULL;
			if (optind < argc)
				optarg = argv[optind];
      optind++;
			return 1;
		}
 
		if (strcmp(argv[optind], "--") == 0) {
			optind++;
			optarg = NULL;
			if (optind < argc)
				optarg = argv[optind];
			return EOF;
		}

		next = argv[optind]+1;
    if(*next != '-') 
		  optind++;
	}	

	c = *next++;
  if(c == '-') {
    if(!(longopt = findlongopt(next, longopts))) {
      //optind++;
      return '?';
    }

    optind++;
    next = NULL;
    //next = argv[++optind];
    //if(next && *next == '-' && *(next + 1) == '-') 
    //if(next) {
      //if(*next == '-')
      //  next++;
      //else
      //  optind++;
    //}

		//if (optind < argc) {
		//optind++;
		//} 

    return longopt->val;
  }

	cp = strchr((char *) optstring, c);
 
	if (cp == NULL || c == ':')
		return '?';
 
	cp++;
	if (*cp == ':') {
		if (*next != '\0') {
			optarg = next;
			next = NULL;
		} else if (optind < argc) {
			optarg = argv[optind];
			optind++;
		} else {
			return '?';
		}
	}
 
	return c;
}

long sysconf(int name) {
  SYSTEM_INFO sysInfo;
  static long pageSize = 0;
  static long regionSize = 0;

  switch(name) {

    case _SC_PAGESIZE :
      if (!pageSize) {      
          GetSystemInfo(&sysInfo);
          pageSize = sysInfo.dwPageSize;
      }
      return pageSize;

    case _SC_REGIONSIZE :
      if(!regionSize) {
        GetSystemInfo(&sysInfo);
        regionSize = sysInfo.dwAllocationGranularity;
      }
      return regionSize;
  
    default:
      return -1;
  }
}

double round(double d) {
  if(d - ((int) d ) > .5) {
    return (((int) d) + 1);
  } else {
    return ((int) d);
  }
}

float roundf(float f) {
  if(f - ((int) f ) > .5) {
    return (float)(((int) f) + 1);
  } else {
    return (float)((int) f);
  }
}

long lroundf(float f) {
  if(f - ((long) f ) > .5) {
    return (((long) f) + 1);
  } else {
    return ((long) f);
  }
}

static long g_pagesize;
static long g_regionsize;
static int g_sl;

void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset) {


  // Wait for spin lock
  while (InterlockedCompareExchange ((LONG volatile *) &g_sl,  1,  0) != 0) 
	  Sleep (0);

  // First time initialization
  if (! g_pagesize) 
      g_pagesize = sysconf(_SC_PAGESIZE);
  if (! g_regionsize) 
      g_regionsize = sysconf(_SC_REGIONSIZE);

  start = VirtualAlloc (start, length, MEM_RESERVE | MEM_COMMIT | MEM_TOP_DOWN, PAGE_READWRITE);

  // Release spin lock
  InterlockedExchange ((LONG volatile *) &g_sl, 0);

  return start;
}

int munmap(void *start, size_t length) {
    int rc = -1;

  // Wait for spin lock
  while (InterlockedCompareExchange ((LONG volatile *) &g_sl,  1,  0) != 0) 
	  Sleep (0);

  // First time initialization
  if (! g_pagesize) 
      g_pagesize = sysconf(_SC_PAGESIZE);
  if (! g_regionsize) 
      g_regionsize = sysconf(_SC_REGIONSIZE);

  /* Free this */
  if (VirtualFree (start, 0, MEM_RELEASE))
    rc = 0;

  // Release spin lock
  InterlockedExchange ((LONG volatile *) &g_sl, 0);

  return rc;
}

#endif // WIN32



#if defined(__linux__) || defined(WIN32)

//#define _GNU_SOURCE
#include <string.h>

#if !defined(strcasestr) || defined(WIN32)

#include <ctype.h>

char *__strcasestr(const char *hay, const char *needle) {
  size_t i, j, leni, lenj;

  if(!hay || !needle || needle[0] == '\0') {
    return NULL;
  }

  j = 0;
  leni = strlen(hay);
  lenj = strlen(needle);

  for(i = 0; i < leni; i++) {
    if(tolower(hay[i]) == tolower(needle[j])) {
      if(++j == lenj) {
        return (char *) &hay[j - lenj];
      }
    } else {
      j = 0;
    }
  }

  return NULL;
}

#endif // strcasestr

#endif // __linux__
