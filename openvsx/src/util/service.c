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

#else // WIN32


#endif // WIN32


#include <stdio.h>
#include <stdlib.h>
#include "util/service.h"

static int getfullpath(const char *exename, char *exepath, const unsigned int len, int enquote) {
  size_t sz;

  if(len < 1) {
    return -1;
  }

  if((sz = GetCurrentDirectoryA(len - 2, exepath)) == 0) {
    fprintf(stderr, "Failed to get current directory path\n");
    return -1;
  }

  if(exename[0] != '\\' && exepath[sz - 1] != '\\') {
    exepath[sz++] = '\\';
    exepath[sz] = '\0';
  }

  strncpy(&exepath[sz], exename, len - sz);

  if(enquote && strchr(exepath, ' ')) {

    if((sz = strlen(exepath)) >= len - 2) {
      return -1;
    }

    exepath[sz + 2] = '\0';
    exepath[sz + 1] = '"';

    while(sz > 0) {
      exepath[sz] = exepath[sz - 1];
      sz--;
    }
    exepath[0] = '"';

  } 

  return 0;
}

int service_install(const char *exename) {

  int rc = 0;
  SC_HANDLE scHandle;
  SC_HANDLE handle;
  PWSTR serviceName[512];
  PWSTR serviceDescr[512];
  PWSTR servicePath[1024];
  char exepath[1024];
  size_t sz = 0;


  if(getfullpath(exename, exepath, sizeof(exepath), 1) < 0) {
    return -1;
  }

  if(MultiByteToWideChar(CP_UTF8, 0, VSX_SERVICE_NAME, -1, (LPWSTR) serviceName, 
    sizeof(serviceName)/sizeof(PWSTR)) <= 0) {
      fprintf(stderr, "Failed to convert service name to unicode (error:%d)\n", GetLastError());
    return -1;
  }

  if(MultiByteToWideChar(CP_UTF8, 0, VSX_SERVICE_DISPNAME, -1, (LPWSTR) serviceDescr, 
    sizeof(serviceDescr)/sizeof(PWSTR)) <= 0) {
    fprintf(stderr, "Failed to convert service description to unicode (error:%d)\n", GetLastError());
    return -1;
  }

  if(MultiByteToWideChar(CP_UTF8, 0, exepath, -1, (LPWSTR) servicePath, 
    sizeof(servicePath)/sizeof(PWSTR)) <= 0) {
    fprintf(stderr, "Failed to convert service path to unicode (error:%d)\n", GetLastError());
    return -1;
  }

  if((scHandle = OpenSCManager(NULL,  SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS)) == NULL) {
    fprintf(stderr, "OpenSCManager failed (error:%d)\n", GetLastError());
    return -1;
  }

  if((handle = OpenService(scHandle, (LPWSTR) serviceName, GENERIC_READ)) != NULL) {

    CloseServiceHandle(handle);

    fprintf(stderr, "Service %s is already installed.\n", VSX_SERVICE_NAME);
    rc = -1;

  } else {

    if((handle = CreateService(scHandle,  (LPWSTR) serviceName, (LPWSTR) serviceDescr,  GENERIC_ALL, 
                         SERVICE_WIN32_OWN_PROCESS, SERVICE_DEMAND_START, 
                         SERVICE_ERROR_IGNORE, (LPWSTR) servicePath, NULL, NULL, NULL, NULL, NULL)) == NULL) {
      fprintf(stderr, "CreateService failed error:%d\n", GetLastError());
      rc = -1;
    }

  }
 
  if(rc == 0) {
   fprintf(stdout, "Installed service %s (%s)\n", VSX_SERVICE_NAME, exepath);
  }

  if(handle) {
    CloseServiceHandle(scHandle);
  }

  if(scHandle) {
   CloseServiceHandle(scHandle);
  }

  return rc;
}

int service_uninstall() {

  int rc = 0;
  SC_HANDLE scHandle = NULL;
  SC_HANDLE handle = NULL;
  PWSTR serviceName[512];

  if(MultiByteToWideChar(CP_UTF8, 0, VSX_SERVICE_NAME, -1, (LPWSTR) serviceName, 
    sizeof(serviceName)/sizeof(PWSTR)) <= 0) {
    fprintf(stderr, "Failed to convert service name to unicode (error:%d)\n", GetLastError());
    return -1;
  }

  if((scHandle = OpenSCManager(NULL,  SERVICES_ACTIVE_DATABASE, SC_MANAGER_ALL_ACCESS)) == NULL) {
    fprintf(stderr, "OpenSCManager failed (error:%d)\n", GetLastError());
    return -1;
  }

  if((handle = OpenService(scHandle, (LPWSTR) serviceName, GENERIC_READ | DELETE)) == NULL) {
    fprintf(stderr, "OpenService %s failed (error:%d)\n", VSX_SERVICE_NAME, GetLastError());
    rc = -1;
  } else if(DeleteService(handle) == 0) {
    fprintf(stderr, "DeleteService failed (error:%d)\n", GetLastError());
    rc = -1;
  }

  if(rc == 0) {
    fprintf(stdout, "Uninstalled service %s\n", VSX_SERVICE_NAME);
  }


  if(handle) {
    CloseServiceHandle(scHandle);
  }

  if(scHandle) {
   CloseServiceHandle(scHandle);
  }

  return rc;
}

static FILE *g_fpdelmesrv;

DWORD WINAPI avcServiceHandler(DWORD dwCtrl, DWORD dwEventType, void *lpEventData, void *lpCtxt) {

  switch(dwCtrl) {
    case SERVICE_CONTROL_CONTINUE:

  if((g_fpdelmesrv = fopen("log\\test.txt", "a"))) {
    fprintf(g_fpdelmesrv, "start msg\n");
    fclose(g_fpdelmesrv);
  }
      return NO_ERROR;
    case SERVICE_CONTROL_STOP:

  if((g_fpdelmesrv = fopen("log\\test.txt", "a"))) {
    fprintf(g_fpdelmesrv, "stop msg\n");
    fclose(g_fpdelmesrv);
  }

      return NO_ERROR;
    default:

  if((g_fpdelmesrv = fopen("log\\test.txt", "a"))) {
    fprintf(g_fpdelmesrv, "unknown msg %d\n", dwCtrl);
    fclose(g_fpdelmesrv);
  }

      return ERROR_CALL_NOT_IMPLEMENTED;
  }


}

static DWORD dwCheckPoint = 1;

static void avcReportServiceStatus(SERVICE_STATUS *pServiceStatus, SERVICE_STATUS_HANDLE serviceHandle,
                                   DWORD state, DWORD exitCode, DWORD waitHint) {


  pServiceStatus->dwCurrentState = state;
  pServiceStatus->dwWin32ExitCode = exitCode;
  pServiceStatus->dwWaitHint = waitHint;

  if(state == SERVICE_START_PENDING) {
    pServiceStatus->dwControlsAccepted = 0;
  } else {
    pServiceStatus->dwControlsAccepted = SERVICE_ACCEPT_STOP;
  }

  if(state == SERVICE_RUNNING || state == SERVICE_STOPPED) {
    pServiceStatus->dwCheckPoint = 0;
  } else {
    pServiceStatus->dwCheckPoint = dwCheckPoint++;
  }

  SetServiceStatus(serviceHandle, pServiceStatus);

}

void WINAPI ServiceMain(DWORD dwArgs, LPTSTR *lpszArgv) {
  //PWSTR serviceName[512];
  SERVICE_STATUS_HANDLE serviceHandle;
  SERVICE_STATUS serviceStatus;

  if(dwArgs > 1) {
    //if(!strncasecmp(lpszArgv[1],
    fprintf(stderr, "--%s--\n", lpszArgv[1]);
  }

  return;

  //logger_SetLevel(S_INFO);
  //logger_AddStderr(S_INFO, LOG_OUTPUT_DEFAULT_STDERR);

  //if(MultiByteToWideChar(CP_UTF8, 0, VSX_SERVICE_NAME, -1, (LPWSTR) VSX_SERVICE_NAME, 
  //  sizeof(serviceName)/sizeof(PWSTR)) <= 0) {
  //    LOG(X_ERROR("Failed to convert service name to unicode (error:%d)"), GetLastError());
  //  return;
  //}

  //RegisterServiceCtrlHandlerEx((LPCTSTR) serviceName, avcServiceHandler, NULL);
  if((serviceHandle = RegisterServiceCtrlHandlerEx(TEXT(VSX_SERVICE_NAME), 
                                                   avcServiceHandler, NULL)) == NULL) {
    //SvcReportEvent(TEXT("RegisterServiceCtrlHandlerEx Failed"));
    return;
  }

  memset(&serviceStatus, 0, sizeof(serviceStatus));
  serviceStatus.dwServiceType = SERVICE_WIN32_OWN_PROCESS;
  serviceStatus.dwServiceSpecificExitCode = 0;

  //avcReportServiceStatus(&serviceStatus, serviceHandle, SERVICE_START_PENDING, NO_ERROR, 3000);
    
  avcReportServiceStatus(&serviceStatus, serviceHandle, SERVICE_RUNNING, NO_ERROR, 3000);
 

  if((g_fpdelmesrv = fopen("og\\test.txt", "a"))) {
    fprintf(g_fpdelmesrv, "inside service main\n");
    fclose(g_fpdelmesrv);
  }

  while(1) {
    Sleep(1000);
  }

}
