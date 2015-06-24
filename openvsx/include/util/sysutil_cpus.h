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


#include <sys/types.h>

#if !defined(WIN32) && !defined(ANDROID)
#include <sys/sysctl.h>
#endif // ANDROID


static int getcpus() {
  int rc = -1;

#if defined(ANDROID)

  rc = 1;

#elif defined(__linux__)

  rc = sysconf( _SC_NPROCESSORS_ONLN );

#elif defined(WIN32)

  SYSTEM_INFO sysinfo;
  GetSystemInfo( &sysinfo );
  rc = sysinfo.dwNumberOfProcessors;

#elif defined(__APPLE__)

  int mib[4];
  size_t len = sizeof(rc);

  mib[0] = CTL_HW;
  mib[1] = HW_AVAILCPU;
  if(sysctl(mib, 2, &rc, &len, NULL, 0) < 0 || rc < 1) {
    mib[1] = HW_NCPU;
    if(sysctl(mib, 2, &rc, &len, NULL, 0) < 0) {
      return -1;
    }
  }

#endif // WIN32

  return rc;
}

