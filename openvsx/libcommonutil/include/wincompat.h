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

#ifndef __WIN32COMPAT_H__
#define __WIN32COMPAT_H__

#if !defined(WIN32)

#define SOCKET_ERROR            -1
#define INVALID_SOCKET          -1
typedef int                     SOCKET;
#define closesocket             close

#ifndef ptrdiff_t

//#if defined(__APPLE__)
//#define ptrdiff_t long long
//#else
#define ptrdiff_t unsigned int
//#endif // __APPLE__

#endif // ptrdiff_t

#endif // WIN32


#endif // __WIN32COMPAT_H__
