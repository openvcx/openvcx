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

#ifdef WIN32

#include <windows.h>
#include "psapi.h"
#include "unixcompat.h"
#include "pthread_compat.h"

#else  // WIN32

#include <unistd.h>
#include <stdlib.h>
#include <sys/time.h>
#include <string.h>
#include <sys/resource.h>


#endif // WIN32

#include <stdio.h>
#include <stdarg.h>
#include <fcntl.h>
#include "logutil.h"
#include "fileops.h"


#ifdef WIN32

// if process is running, return processID, otherwise return a non-positive number
static int sysops_getProcId(const char *processName) {
  DWORD procIds[2048], procIdsSize, procCount;
  int i;
	HANDLE hProcess;
	HMODULE hMod;
  DWORD modSize;
	int rc = -1;
	TCHAR szProcessName[MAX_PATH];

	// get list of processID.
	if ( !EnumProcesses( procIds, sizeof(procIds), &procIdsSize ) ){
    return rc;
	}
  
	// Calculate process count.
  procCount = procIdsSize / sizeof(DWORD);

	rc = 0;
	for(i = 0; i < (int)procCount; i ++) {
		hProcess = OpenProcess( PROCESS_QUERY_INFORMATION |PROCESS_VM_READ, FALSE, procIds[i] );
    if(hProcess != NULL ) {        
			if( EnumProcessModules( hProcess, &hMod, sizeof(hMod), &modSize) ){
				if(GetModuleBaseName( hProcess, hMod, szProcessName, sizeof(szProcessName))){
					if(!strcmp(processName, szProcessName)){
						// the process is running
			      rc = procIds[i];
						CloseHandle( hProcess );
						break;
					}
				}
      }
			CloseHandle( hProcess );
    }

	}//end of for loop

	return rc;
}

// if process is started, return the processId
int sysops_StartProc(const char *path, const char *args) {

	int rc;
  STARTUPINFO startInfo;
  PROCESS_INFORMATION procInfo;
	char cmd[1024];

  ZeroMemory( &startInfo, sizeof(startInfo) );
  startInfo.cb = sizeof(startInfo);
  ZeroMemory( &procInfo, sizeof(procInfo) );

	if(args != NULL && strlen(args) > 0) {
		_snprintf(cmd, sizeof(cmd), "%s %s", path, args);
	}else{
		_snprintf(cmd, sizeof(cmd), "%s", path);
	}

	rc = -1;
  if(CreateProcess( NULL,           // path and process name
										cmd,           // arguments
										NULL,           // Process handle not inheritable
										NULL,           // Thread handle not inheritable
										FALSE,          // Set handle inheritance to FALSE
										0,              // No creation flags
										NULL,           // Use parent's environment block
										NULL,           // Use parent's starting directory 
										&startInfo,     // Pointer to STARTUPINFO structure
										&procInfo)){    // Pointer to PROCESS_INFORMATION structure
		
		rc = procInfo.dwProcessId;
	}

  return rc;
}

// if process is killed, return processId, if process is not running, return 0,
// return negative number if process is running but cannot be terminated.
int sysops_KillProc(const char *procName) {
	int procId = 0;
  int rc = 0;
	HANDLE hProcess;

	procId = sysops_getProcId(procName);
	if(procId > 0){
		hProcess = OpenProcess( PROCESS_TERMINATE, FALSE, procId);
		if(hProcess != NULL) {
			if(TerminateProcess(hProcess, 1)) {
				rc = procId;
			}else{
				rc = -1;
			}
			CloseHandle( hProcess );
		}else{
			rc = -2;
		}
	}

  return procId;
}

int sysops_CheckProcRunning(const char *processName) {
	bool rc = FALSE;
	if(sysops_getProcId(processName) > 0){
		rc = TRUE;
	}
	return rc;
}

int sysops_ChangeLocalPriority(int priority) {

  HANDLE hProc;
  int rc = 0;
  DWORD dwPriority = NORMAL_PRIORITY_CLASS;

  switch(priority) {
    case 1:
      dwPriority = ABOVE_NORMAL_PRIORITY_CLASS;
      break;
    case 2:
      dwPriority = HIGH_PRIORITY_CLASS;
      break;
    case 3:
      dwPriority = REALTIME_PRIORITY_CLASS;
      break;
    case -1:
      dwPriority = BELOW_NORMAL_PRIORITY_CLASS;
      break;
    case -2:
      dwPriority = IDLE_PRIORITY_CLASS;
      break;

  }
    
  if((hProc = GetCurrentProcess()) == NULL) {
    LOG(X_ERROR("Unable to get handle to current process.  Cannot adjust priority."));
    return -1;
  }
  
  if(SetPriorityClass(hProc, dwPriority) == 0) {
    LOG(X_ERROR("Unable to SetPriorityClass for current process to %d (0x%x)"), priority, dwPriority);
  } else {
    LOG(X_INFO("Set priority to %d (0x%x)"), priority, dwPriority);
  }

  CloseHandle(hProc);

  return 0;
}

#else
static int sequential_number = 0;

static char *ExeSysCmd(char *pCmd, char *fname) {

  char buffer[BUFSIZ + 1];
  char fileName[128];
  char *pcBuff = NULL;
  char *command = NULL;
  int iTotal = 0;
  int chars_read = sizeof(buffer);
  FILE *read_fp;
  struct timeval tcurr;

  gettimeofday(&tcurr, NULL);


  if( (fname == NULL) || (strlen(fname) > sizeof(fileName)) ) {

#ifdef COYOTE

    sprintf(fileName, "/tmp/mtts_%ld_%d.txt", tcurr.tv_sec, ++sequential_number);

#else

    if(sequential_number == 0)
      srand((unsigned)time( NULL ));
    sprintf(fileName, "/tmp/mtts_%ld%ld.%d.%d.%d.%d.txt", 
            tcurr.tv_sec, tcurr.tv_usec, getpid(), (int) pthread_self(), ++sequential_number, rand());

#endif

  } else {
    sprintf(fileName, "%s", fname);
  }
  command = (char *)malloc(strlen(pCmd) + 16 + strlen(fileName));
  if(command == NULL) {
    fprintf(stderr, "Failed to allocate memory for command. \n");
    return NULL;
  }
  strcpy(command, pCmd);
  strcat(command, " > ");
  strcat(command, fileName);

  system(command);

  if(command != NULL) {
    free(command);
  }

  read_fp = fopen(fileName, "rt");
  while(read_fp != NULL && !feof(read_fp) ) {
    memset(buffer, '\0', sizeof(buffer));
    chars_read = fread(buffer, sizeof(char), BUFSIZ, read_fp);
    if(chars_read > 0) {
      pcBuff = (char *)realloc(pcBuff, iTotal + chars_read);
      memcpy(pcBuff + iTotal, buffer, chars_read);
      iTotal += chars_read;
    } else {
      break;
    }
  }
  if( read_fp != NULL ) {
    fclose(read_fp);
  } else {
    return NULL;
  }

  pcBuff = (char *)realloc(pcBuff, iTotal + 1);
  pcBuff[iTotal] = '\0';

  /* cleanup */
  remove(fileName);
  return pcBuff;
}


// always return 1; 
int sysops_StartProc(const char *path, const char *args) {
	unsigned char cmd[1024];
	if(args != NULL && strlen(args) > 0) {
		snprintf(cmd, sizeof(cmd), "%s %s >/dev/null 2>&1 &", path, args);
	}else{
		snprintf(cmd, sizeof(cmd), "%s >/dev/null 2>&1 &", path);
	}
	cmd[sizeof(cmd)-1]='\0';
	system(cmd);
  return 1;
}

// always return 0
int sysops_KillProc(const char *procName) {

	unsigned char cmd[1024];
	snprintf(cmd, sizeof(cmd), "killall %s >/dev/null 2>&1 &", procName);
	cmd[sizeof(cmd)-1]='\0';
	system(cmd);
  return 0;
}

int sysops_CheckProcRunning(const char *processName) {
	unsigned char cmd[1024];
	char *p = NULL;
	snprintf(cmd, sizeof(cmd), "/sbin/killall5 %s 2>/dev/null ", processName);
	cmd[sizeof(cmd)-1]='\0';	
	p = ExeSysCmd(cmd, NULL);
	if (p && *p!='\0'){
		return 1; // process is found;
	}
	return 0;
}

int sysops_ChangeLocalPriority(int priority) {
	int which = PRIO_PROCESS;
	id_t pid;
	int ret;
	pid = getpid();
	ret = setpriority(which, pid, priority);
	return ret;
}


#endif  // WIN32
