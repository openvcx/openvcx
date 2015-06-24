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

#ifndef __XCODER_VERSION_H__
#define __XCODER_VERSION_H__

#if defined(ARCH_mac_x86_64)
#define ARCH "x86_64"
#elif defined(ARCH_linux_x86_64)
#define ARCH "x86_64"
#elif defined(ARCH_linux_x86_32)
#define ARCH "x86_32"
#elif defined(WIN32)
#define ARCH "win32"
#elif defined(ANDROID)
#define ARCH "android_armeabi"
#endif


#define VSX_VERSION "[gpl] %s"
#define BUILD_INFO_NUM  "(unknown version)"











#endif // __XCODER_VERSION_H__
