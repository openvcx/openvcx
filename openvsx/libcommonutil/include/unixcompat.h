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

#ifndef __UNIXCOMPAT_H__
#define __UNIXCOMPAT_H__

#if defined(__linux__) || defined(__MINGW32__)

#include <stdint.h>
#include <sys/types.h>
#include <sys/time.h>

//
// __MINGW32__  specific
//
#if defined(__MINGW32__)
#define srandom srand
#define random rand
#define socklen_t uint32_t
#define sleep(x) Sleep(1000 * x)
//do not uncomment use of 'Sleep', as mingw usleep may be broken
#define usleep(x) Sleep(x / 1000)

#ifndef getpid
#define getpid GetCurrentProcessId
#endif // getpid


// The following do not appear to be defined in mingw/include/sys/stat.h
#define S_IRWXO (S_IRWXG >> 3)
#define S_IRWXG (S_IRWXU >> 3)

#endif // __MINGW32_)

#if !defined(strnstr)
#define strnstr(hay, needle, sz) strstr(hay, needle)
#endif // strnstr

//__suseconds_t
#define USECT     "ld"
#define SECT      "ld"

#define PTHREAD_T_PRINTF  "lu"

extern double round ( double );
extern float roundf ( float );
extern long double roundl(long double x);
extern long int lrint(double x);


#else
#if defined(__APPLE__)

#include <sys/types.h>
#include <sys/time.h>

typedef __uint64_t uint64_t;
typedef __uint32_t uint32_t;
typedef __uint16_t uint16_t;
typedef __uint8_t uint8_t;

//__suseconds_t
#define USECT     "u"
#define SECT      "u"

#define PTHREAD_T_PRINTF  "ld"

#else
#if defined(WIN32)

#include <time.h>
#include "unixtypes.h"

// offsetof defined in stddef.h
//#ifndef offsetof
//#define offsetof(TYPE, MEMBER) ((size_t) &((TYPE *)0)->MEMBER)
//#endif // offsetof

#define srandom srand
#define random rand

//#define vsnprintf _vsnprintf
#define snprintf _snprintf
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#define fileno _fileno
#define fstat _fstat
//#define strdup _strdup
#define inline __inline
#define sleep(x) Sleep(1000 * x)
#define usleep(x) Sleep(x / 1000)
#define atoll _atoi64

#define strnstr(hay, needle, sz) strstr(hay, needle)

#ifndef getpid
#define getpid GetCurrentProcessId
#endif // getpid



#ifndef timespec 
struct timespec  {
  time_t  tv_sec;         /* seconds */
  long    tv_nsec;        /* nanoseconds */
};
#endif // timespec

extern char *optarg;
extern int optind, opterr, optopt;

#define no_argument 0
#define required_argument 1
#define optional_argument 2


 struct option {
         char *name;
         int has_arg;
         int *flag;
         int val;
 };
 
int getopt_long(int argc, char * const argv[], const char *optstring,
                     const struct option *longopts, int *longindex);
int getopt(int argc, char *const argv[], const char *optstring);
int gettimeofday(struct timeval *tv, struct timezone *tz);
long sysconf(int name);
double round(double);
float roundf(float);
long lroundf(float);

#define PROT_READ       0x1             /* Page can be read.  */
#define PROT_WRITE      0x2             /* Page can be written.  */
#define PROT_EXEC       0x4             /* Page can be executed.  */
#define PROT_NONE       0x0             /* Page can not be accessed.  */

/* Sharing types (must choose one and only one of these).  */
#define MAP_SHARED      0x01            /* Share changes.  */
#define MAP_PRIVATE     0x02            /* Changes are private.  */
#define MAP_TYPE       0x0f            /* Mask for type of mapping.  */
#define MAP_FIXED       0x10            /* Interpret addr exactly.  */
#define MAP_FILE        0
#define MAP_ANONYMOUS   0x20            /* Don't use a file.  */
#define MAP_ANON        MAP_ANONYMOUS

void *mmap(void *start, size_t length, int prot, int flags, int fd, off_t offset);
int munmap(void *start, size_t length);


//__suseconds_t
#define USECT     "u"
#define SECT      "u"

#define PTHREAD_T_PRINTF  "lu"

#endif // WIN32
#endif // __APPLE__
#endif // __linux__

#if defined(__MINGW32__)
// __MINGW32__ specific equivalent of long long %llu / %lld print
#define LL64   "I64"
#else
#define LL64   "ll"
#endif // __MINGW32__


#if defined(__linux__) || defined(WIN32) 

char *__strcasestr(const char *hay, const char *needle);
#define strcasestr __strcasestr

#endif // __linux__ 



#endif // __UNIXCOMPAT_H__
