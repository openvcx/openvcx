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

#if !defined(ANDROID_LOG)

#include <stdarg.h>
#include <string.h>
#include <time.h>


#ifdef WIN32

#include <sys/stat.h>
#include <sys/types.h>
#include <windows.h>

#include "unixcompat.h"
#include "pthread_compat.h"

#else // WIN32

#include <sys/time.h>
#include <unistd.h>
#include <pthread.h>
#include "unixcompat.h"
#include "pthread_compat.h"

//#define SYSLOG_NAMES 1
#include <syslog.h>

#endif // WIN32

#define USE_STAT_32
#include "fileops.h"
#include "timers.h"
#include "logutil.h"


#define MAX_FILEPATH_LEN                     512
#define LOG_SEVERITY_STR_LEN                   6


typedef struct LOG_PROPERTIES {
  pthread_mutex_t g_mtx_log;
  int g_maxFilesNum;
  int g_maxFileSz;
  char g_logName[MAX_FILEPATH_LEN];
  FILE *g_fp;
  int g_fd;
  int g_log_flags;

  int g_log_level;
  int g_log_level_stderr;
} LOG_PROPERTIES_T;

static LOG_PROPERTIES_T _g_logProps;
static LOG_PROPERTIES_T *g_plogProps = &_g_logProps;

typedef struct LOG_CTXT_INT {
  LOG_PROPERTIES_T *plogProps;
  void *plogutil_tidctxt; // LOGUTIL_THREAD_CTXT_T
} LOG_CTXT_INT_T;

static LOG_CTXT_INT_T g_logCtxtInt;

void *logger_getContext() {
  g_logCtxtInt.plogProps = &_g_logProps;
  g_logCtxtInt.plogutil_tidctxt = logutil_tid_getContext();
  return &g_logCtxtInt;
}

int logger_setContext(void *pCtxt) {
  LOG_CTXT_INT_T *pLogCtxtInt = (LOG_CTXT_INT_T *) pCtxt;

  if(!pLogCtxtInt || !pLogCtxtInt->plogProps || pLogCtxtInt->plogProps->g_fd == 0 || g_plogProps->g_fd != 0) {
    return -1;
  }
  g_plogProps = pLogCtxtInt->plogProps;
  if(pLogCtxtInt->plogutil_tidctxt) {
    logutil_tid_setContext(pLogCtxtInt->plogutil_tidctxt);
  }
  return 0;
}

static const char *logger_getSeverityStr(unsigned short sev) {

  static struct LOG_SEVERITY {
    unsigned short sev;
    char descr[LOG_SEVERITY_STR_LEN];
  } SEVERITY_LIST[] = { { S_CRITICAL,       "CRTL" },
                        { S_ERROR,          "EROR" },
                        { S_WARNING,        "WARN" },
                        { S_INFO,           "INFO" },
                        { S_DEBUG,          "DEBG" },
                        { S_DEBUG_VERBOSE,  "DBGV" } };

  if(sev > 0 && sev <= (sizeof(SEVERITY_LIST) / sizeof(SEVERITY_LIST[0]))) {
    return SEVERITY_LIST[sev - 1].descr;
  }

  return SEVERITY_LIST[3].descr;
}

#ifndef WIN32
static inline int logger_syslogSeverity(int severity) {
  switch(severity) {
    case S_CRITICAL:
      return LOG_CRIT;
    case S_ERROR:
      return LOG_ERR;
    case S_WARNING:
      return LOG_WARNING;
    case S_INFO:
      return LOG_NOTICE;
    case S_DEBUG:
      return LOG_INFO;
    case S_DEBUG_VERBOSE:
      return LOG_DEBUG; 
    default:
      return LOG_INFO;
  }
}
#endif // WIN32

static int logger_open(LOG_PROPERTIES_T *pLogProperties) {

  if((pLogProperties->g_fp = fopen(pLogProperties->g_logName, "a")) == NULL) {
    return -1;
  }

  pLogProperties->g_fd = fileno(pLogProperties->g_fp);

  return 0;
}

static void logger_close(LOG_PROPERTIES_T *pLogProperties) {
  if(pLogProperties->g_fp) {
    fclose(pLogProperties->g_fp);
  }
  pLogProperties->g_fp = NULL;
  pLogProperties->g_fd = -1;
}

int logger_InitSyslog(const char *ident, int syslogOpt, int syslogFacility) {
  LOG_PROPERTIES_T *pLogProperties = NULL;

#ifdef WIN32
  return -1;
#endif // WIN32

  pLogProperties = g_plogProps;
  pthread_mutex_init(&pLogProperties->g_mtx_log, NULL);
  pLogProperties->g_log_flags |= LOG_FLAG_SYSLOG;

  if(pLogProperties->g_log_level == 0) {
    pLogProperties->g_log_level = S_INFO;
  }

#ifndef WIN32
  if(ident) {
    openlog(ident, syslogOpt, syslogFacility);
  } else {
    return -1;
  }
#endif // WIN32

  return 0;
}

int logger_SetFile(const char *fileDir,
                  const char *fileName,
                  unsigned int maxFilesNum,
                  unsigned int maxFileSz,
                  unsigned int outputFlags) {

  size_t len = 0u, len2 = 0u;
  LOG_PROPERTIES_T *pLogProperties =  g_plogProps;

  pthread_mutex_init(&pLogProperties->g_mtx_log, NULL);

  if(pLogProperties->g_fp) {
    logger_close(pLogProperties);
  }

  if(!fileName) {
    return -1;
  }

  //
  // Init of "global" variables
  //
  pLogProperties->g_maxFilesNum = maxFilesNum;
  pLogProperties->g_maxFileSz = maxFileSz;
  // XOR with flags suitable for fileoutput 
  pLogProperties->g_log_flags |= LOG_FLAG_USEFILEOUTPUT | (outputFlags & 
         (LOG_FLAG_USELOCKING | LOG_OUTPUT_PRINT_DATE | LOG_OUTPUT_PRINT_SEV | LOG_OUTPUT_PRINT_SEV_ERROR | 
          LOG_OUTPUT_PRINT_PID | LOG_OUTPUT_PRINT_TID | LOG_OUTPUT_PRINT_TAG));
  

  if(fileDir) {
    len = strlen(fileDir);
  }

  len2 = strlen(fileName); 

  if(len + len2 + 6 >= sizeof(pLogProperties->g_logName)) {
    return -1;
  }

  memset(pLogProperties->g_logName, 0, sizeof(pLogProperties->g_logName));

  if(fileDir && len > 0) {
    strncpy(pLogProperties->g_logName, fileDir, sizeof(pLogProperties->g_logName) -1);

    if(pLogProperties->g_logName[len - 1] != DIR_DELIMETER) {
      pLogProperties->g_logName[len++] = DIR_DELIMETER;
    }
  }

  strncpy(&pLogProperties->g_logName[len], fileName, 
          sizeof(pLogProperties->g_logName) - (len +1));
  len += len2;

  if(!strstr(fileName, ".log")) {
    strncat(&pLogProperties->g_logName[len], ".log", 
           sizeof(pLogProperties->g_logName) - (len+1));
  }

  return logger_open(pLogProperties);
}

int logger_Init(const char *filePath,
                const char *fileName,
                unsigned int maxFilesNum,
                unsigned int maxFileSz,
                unsigned int outputFlags) {

  size_t len = 0u, len2 = 0u;
  LOG_PROPERTIES_T *pLogProperties = g_plogProps;

  // Init of "global" variables
  pthread_mutex_init(&pLogProperties->g_mtx_log, NULL);

  pLogProperties->g_maxFilesNum = LOGGER_MAX_FILES;
  pLogProperties->g_maxFileSz = LOGGER_MAX_FILE_SZ;
  pLogProperties->g_fp = NULL;
  pLogProperties->g_fd = -1;
  pLogProperties->g_log_flags = outputFlags;

  //if(!(pLogProperties->g_log_flags & LOG_FLAG_USEFILEOUTPUT)) {
  //  pLogProperties->g_log_flags |= LOG_FLAG_DEFAULT;
  //}
  if(pLogProperties->g_log_level == 0) {
    pLogProperties->g_log_level = S_INFO;
  }
  pLogProperties->g_log_level_stderr = pLogProperties->g_log_level;

  //if(pLogProperties->g_fp) {
  //  logger_close(pLogProperties);
  //  pLogProperties->g_logName[0] = '\0';
  //}

  if(fileName == NULL) {
    pLogProperties->g_log_flags &= ~LOG_FLAG_USEFILEOUTPUT;
    return -1;
  }

  if(maxFilesNum > 0) {
    pLogProperties->g_maxFilesNum = maxFilesNum;
  }
  if(maxFileSz > 0) {
    pLogProperties->g_maxFileSz = maxFileSz;
  }

  if(filePath) {
    len = strlen(filePath);
  }

  len2 = strlen(fileName); 

  if(len + len2 + 6 >= sizeof(pLogProperties->g_logName)) {
    return -1;
  }

  memset(pLogProperties->g_logName, 0, sizeof(pLogProperties->g_logName));

  if(filePath && len > 0) {
    strncpy(pLogProperties->g_logName, filePath, sizeof(pLogProperties->g_logName) -1);

    if(pLogProperties->g_logName[len - 1] != DIR_DELIMETER) {
      pLogProperties->g_logName[len++] = DIR_DELIMETER;
    }
  }

  strncpy(&pLogProperties->g_logName[len], fileName, sizeof(pLogProperties->g_logName) - (len +1));
  len += len2;

  if(!strstr(fileName, ".log")) {
    strncat(&pLogProperties->g_logName[len], ".log", sizeof(pLogProperties->g_logName) - (len+1));
  }

  return logger_open(pLogProperties);
}

int logger_SetHistory(unsigned int maxFiles, unsigned int maxFileBytes) {
  LOG_PROPERTIES_T *pLogProperties = NULL;

  pLogProperties = g_plogProps;

  if(pLogProperties->g_log_flags & LOG_FLAG_USESYSLOG) {
    return -1;
  }

  pLogProperties->g_maxFilesNum = maxFiles;
  pLogProperties->g_maxFileSz = maxFileBytes;

  return 0;
}

int logger_SetLevel(int sev) {

  LOG_PROPERTIES_T *pLogProperties = g_plogProps;
 
  if(sev < S_CRITICAL || sev > S_DEBUG_VERBOSE) {
    return -1;
  }

  pLogProperties->g_log_level = sev;

  return 0;
}

void logger_AddStderr(int sev, int output_flags) {

  LOG_PROPERTIES_T *pLogProperties = g_plogProps;
  int baseflags = (LOG_FLAG_USESTDERR | LOG_FLAG_FLUSHOUTPUT);

  //pLogProperties->g_log_flags &= (0xffffff00 | baseflags); 
  pLogProperties->g_log_flags |= baseflags;
  pLogProperties->g_log_flags |= output_flags;
  pLogProperties->g_log_level_stderr = sev;

  return;
}

static void print_tag(LOG_PROPERTIES_T *pLogProperties, char strpid[], size_t szpid) {

  const char *tag = NULL;

  if((pLogProperties->g_log_flags & LOG_OUTPUT_PRINT_TAG)) {
    if((tag = logutil_tid_lookup(pthread_self(), 1)) && tag[0] == '\0') {
      tag = NULL;
    }
  }

  if((pLogProperties->g_log_flags & LOG_OUTPUT_PRINT_PID) &&
     (pLogProperties->g_log_flags & LOG_OUTPUT_PRINT_TID)) {
    snprintf(strpid, szpid - 1, "[%d,%"PTHREAD_T_PRINTF"%s%s]",
         (int) getpid(), ( PTHREAD_SELF_TOINT(pthread_self())), tag ? " - " : "", tag ? tag : "");
  } else if(pLogProperties->g_log_flags & LOG_OUTPUT_PRINT_PID) {
    snprintf(strpid, szpid - 1, "[%d%s%s]", (int) getpid(), tag ? " - " : "", tag ? tag : "");
  } else if(pLogProperties->g_log_flags & LOG_OUTPUT_PRINT_TID) {
    snprintf(strpid, szpid - 1, "[%"PTHREAD_T_PRINTF"%s%s]",
             PTHREAD_SELF_TOINT(pthread_self()), tag ? " - " : "", tag ? tag : "");
  } else if(tag != NULL && tag[0] != '\0') {
    snprintf(strpid, szpid -1, "[%s] ", tag);
  }

  strpid[szpid - 1] = '\0';

}

static void logger_write_stderr(LOG_PROPERTIES_T *pLogProperties, int sev, 
                                const char *msg, va_list *pvlist) {

  struct tm *ptm = NULL;
  //const char *tag = NULL;
  char strtmp[64];
  time_t tmNow;

  if(!(pLogProperties->g_log_flags & LOG_FLAG_USESTDERR) ||
    sev == 0 ||
    (sev > 0 && sev > pLogProperties->g_log_level_stderr) ||
    (sev < 0 && sev < (-1 * pLogProperties->g_log_level_stderr))) {
    return;
  }

  if((pLogProperties->g_log_flags & LOG_OUTPUT_PRINT_DATE_STDERR)) {
    tmNow = time(NULL);

    if((ptm = (struct tm *) localtime(&tmNow)) != NULL) {
      strftime(strtmp, sizeof(strtmp) -1, "%b %d %H:%M:%S ", ptm);
      strtmp[sizeof(strtmp) - 1] = '\0';
      fprintf(stderr, "%s ", strtmp);
    }
  }

  //
  // If we're also printing to a file, don't also show the thread-id / thread-tag info to stderr
  //
  if(!(pLogProperties->g_log_flags & LOG_FLAG_USEFILEOUTPUT)) {
    strtmp[0] = '\0';
    print_tag(pLogProperties, strtmp, sizeof(strtmp));
  
    if(strtmp[0] != '\0') {
      fprintf(stderr, "%s ", strtmp);
    } 
  }

  if((pLogProperties->g_log_flags & LOG_OUTPUT_PRINT_SEV) ||
     ((pLogProperties->g_log_flags & LOG_OUTPUT_PRINT_SEV_ERROR) && sev <= S_ERROR)) {
    fprintf(stderr, "%s ", logger_getSeverityStr((unsigned short) sev));
  }

  // Log to stderr
  vfprintf(stderr, msg, *pvlist);

  // flush output
  if(pLogProperties->g_log_flags & LOG_FLAG_FLUSHOUTPUT) {
    fflush(stderr);
  }

  return;
}

static void logger_write_file(LOG_PROPERTIES_T *pLogProperties, int sev, 
                              const char *msg, va_list *pvlist) {

  struct tm *ptm = NULL;
  char strtime[64];
  char strpid[64];
  char strsev[30];
  char strspace[2];
  const TIME_VAL tmNowUs = timer_GetTime();
  const time_t tmNow = tmNowUs / TIME_VAL_US;
  //const time_t tmNow = time(NULL);

  if(!pLogProperties->g_fp ||
    !(pLogProperties->g_log_flags & LOG_FLAG_USEFILEOUTPUT) ||
     sev == 0 ||
    (sev > 0 && sev > pLogProperties->g_log_level) ||
    (sev < 0 && sev < (-1 * pLogProperties->g_log_level))) {
    return;
  }

  if((pLogProperties->g_log_flags & LOG_OUTPUT_PRINT_DATE) &&
    (ptm = (struct tm *) localtime(&tmNow)) != NULL) {
    strftime(strtime, sizeof(strtime) -1, "%b %d %H:%M:%S", ptm);
    strtime[sizeof(strtime) - 1] = '\0';
  } else {
    strtime[0] = '\0';
  }

  strpid[0] = '\0';
  print_tag(pLogProperties, strpid, sizeof(strpid));

  if(pLogProperties->g_log_flags & LOG_OUTPUT_PRINT_SEV) {
    snprintf(strsev, sizeof(strsev) - 1, "%s ", logger_getSeverityStr((unsigned short) sev));
    strsev[sizeof(strsev) - 1] = '\0';
  } else {
    strsev[0] = '\0';
  }

  if(strtime[0] != '\0' || (strpid[0] != '\0' && strsev[0] != '\0')) {
    strspace[0] = ' ';
    strspace[1] = '\0';
  } else {
    strspace[0] = '\0';
  }

  // Log to output file
  fprintf(pLogProperties->g_fp, "%s.%.3llu %s%s%s", strtime, 
          tmNowUs/TIME_VAL_MS%TIME_VAL_MS, strsev, strpid, strspace);
  vfprintf(pLogProperties->g_fp, msg, *pvlist);

  // flush output
  if(pLogProperties->g_log_flags & LOG_FLAG_FLUSHOUTPUT) {

    if(pLogProperties->g_fp != NULL &&
       (pLogProperties->g_log_flags & LOG_FLAG_USEFILEOUTPUT)) {
      fflush(pLogProperties->g_fp);
    }
  }

  return;
}

static void logger_log(LOG_PROPERTIES_T *pLogProperties, int sev, const char *msg, ...) {

  va_list vlist;

  if(pLogProperties->g_fp == NULL) {
    return;
  }

  va_start(vlist, msg);

  logger_write_file(pLogProperties, sev, msg, &vlist); 
    
  va_end(vlist);

}

static int logger_checkRoll(LOG_PROPERTIES_T *pLogProperties) {

#if defined(WIN32) && !defined(__MINGW32__)
  struct _stat st;
#else // WIN32
  struct stat st;
#endif // WIN32

  if(pLogProperties->g_fp == NULL) {
    return -1;
  }

  if(pLogProperties->g_maxFileSz <= 0) {
    return 0;
  }

  if(fstat(pLogProperties->g_fd, &st) != 0) {
    return -1;
  }

  if(st.st_size > pLogProperties->g_maxFileSz) {
    return 1;
  }

  return 0;
}

static int logger_rollFiles(LOG_PROPERTIES_T *pLogProperties) {

  int i;
  char buf[16];
  char logNameOld[sizeof(pLogProperties->g_logName) + sizeof(buf)];
  char logNameNew[sizeof(pLogProperties->g_logName) + sizeof(buf)];
  size_t len;
  //struct timeval tv0, tv1;
  //int ms;

#ifdef WIN32
  struct _stat st;
#else // WIN32
  struct stat st;
#endif // WIN32

  //gettimeofday(&tv0, NULL);

  len = strlen(pLogProperties->g_logName);
  strcpy(logNameOld, pLogProperties->g_logName);
  strcpy(logNameNew, pLogProperties->g_logName);

  for(i = pLogProperties->g_maxFilesNum; i >= 1; i--) {

    if(i <= 1) {
      logger_close(pLogProperties);
      logNameOld[len] = '\0';
    } else {
      sprintf(buf, ".%d", i-1);
      strcpy(&logNameOld[len], buf);
    }

#ifdef WIN32
    if(_stat(logNameOld, &st) == 0) {
#else // WIN32
    if(stat(logNameOld, &st) == 0) {
#endif // WIN32

      sprintf(buf, ".%d", i);
      strcpy(&logNameNew[len], buf);

      if(fileops_MoveFile(logNameOld, logNameNew) != 0) {
        // call internal logger_log method to avoid infinite loop possibility by
        // checking for roll
        logger_log(pLogProperties, X_ERROR("failed to move file '%s' -> '%s'"), logNameOld, logNameNew);
      }
    }

  }

  logger_open(pLogProperties);

  //gettimeofday(&tv1, NULL);

  //ms = ((tv1.tv_sec - tv0.tv_sec) * 1000) + ((tv1.tv_usec - tv0.tv_usec) / 1000);

  // call internal logger_log method to avoid infinite loop possibility by
  // checking for roll
  //logger_log(pLogProperties, X_DEBUG("logger_rollFiles took %d ms to complete."), ms);

  return 0;
}

extern void avc_dumpHex(void *fparg, const unsigned char *buf, unsigned int len, int ascii);

void logger_LogHex(int sev, const void *buf, unsigned int len, int printAscii) {

  LOG_PROPERTIES_T *pLogProperties = NULL;
  pLogProperties = g_plogProps;

  if(!((pLogProperties->g_log_flags & (LOG_FLAG_USESYSLOG | LOG_FLAG_USESTDERR)) ||
       (pLogProperties->g_fp != NULL &&  (pLogProperties->g_log_flags & LOG_FLAG_USEFILEOUTPUT)))) {
    return;
  }

  if(pLogProperties->g_log_flags & LOG_FLAG_USELOCKING) {
    pthread_mutex_lock(&pLogProperties->g_mtx_log);
  }

  if((pLogProperties->g_log_flags & LOG_FLAG_USESTDERR) &&
    ((sev > 0 && sev <= pLogProperties->g_log_level_stderr) ||
    (sev < 0 && sev >= (-1 * pLogProperties->g_log_level_stderr)))) {

     avc_dumpHex(stderr, (const unsigned char *) buf, len, printAscii);
  }

  if(pLogProperties->g_fp &&
    (pLogProperties->g_log_flags & LOG_FLAG_USEFILEOUTPUT) &&
    ((sev > 0 && sev <= pLogProperties->g_log_level) ||
    (sev < 0 && sev >= (-1 * pLogProperties->g_log_level)))) {

     avc_dumpHex(pLogProperties->g_fp, (const unsigned char *) buf, len, printAscii);

    if(logger_checkRoll(pLogProperties) > 0) {
      logger_rollFiles(pLogProperties);
    }
  }

  if(pLogProperties->g_log_flags & LOG_FLAG_USELOCKING) {
    pthread_mutex_unlock(&pLogProperties->g_mtx_log);
  }

}

void logger_Log(int sev, const char *msg, ...) {

  va_list vlist;
  LOG_PROPERTIES_T *pLogProperties = NULL;
  pLogProperties = g_plogProps;

  if(!((pLogProperties->g_log_flags & (LOG_FLAG_USESYSLOG | LOG_FLAG_USESTDERR)) ||
       (pLogProperties->g_fp != NULL &&  (pLogProperties->g_log_flags & LOG_FLAG_USEFILEOUTPUT)))) {
    return;
  }

  if(pLogProperties->g_log_flags & LOG_FLAG_USELOCKING) {
    pthread_mutex_lock(&pLogProperties->g_mtx_log);
  } 

  if(pLogProperties->g_log_flags & LOG_FLAG_USESTDERR) {
    va_start(vlist, msg);
    logger_write_stderr(pLogProperties, sev, msg, &vlist); 
    va_end(vlist);
  }

  if(pLogProperties->g_log_flags & LOG_FLAG_USEFILEOUTPUT) {

    va_start(vlist, msg);
    logger_write_file(pLogProperties, sev, msg, &vlist); 
    va_end(vlist);
    if(logger_checkRoll(pLogProperties) > 0) {
      logger_rollFiles(pLogProperties);
    }
  }

  if(pLogProperties->g_log_flags & LOG_FLAG_USELOCKING) {
    pthread_mutex_unlock(&pLogProperties->g_mtx_log);
  }

}

int logger_GetLogLevel() {
  LOG_PROPERTIES_T *pLogProperties = NULL;

  pLogProperties = g_plogProps;

  return pLogProperties->g_log_level;
}

#endif // ANDROID
