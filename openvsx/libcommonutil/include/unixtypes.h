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

#ifndef __UNIXTYPES_H__
#define __UNIXTYPES_H__


#ifndef _SC_PAGESIZE
#define _SC_PAGESIZE  1
#endif // _SC_PAGESIZE

#ifndef _SC_REGIONSIZE
#define _SC_REGIONSIZE  2
#endif // _SC_REGIONSIZE

#ifndef SUCCESS
#define SUCCESS   0
#endif // SUCCESS

#ifndef FAILURE
#define FAILURE -1
#endif

#ifndef uint64_t
typedef unsigned long long int uint64_t;
#endif // uint64_t

#ifndef int64_t
typedef long long int int64_t;
#endif // int64_t


#ifndef u_int
typedef unsigned int u_int; 
#endif // u_int

#ifndef uint32_t
typedef unsigned int uint32_t;
#endif // uint32_t

//#ifndef uint32
//typedef unsigned int uint32;
//#endif // uint32


#ifndef u_int32_t
typedef unsigned int u_int32_t;
#endif // u_int32_t

#ifndef int32_t
typedef int          int32_t;
#endif //int32_t

#ifndef ulong
typedef unsigned long ulong;
#endif // ulong

#ifndef uint
typedef unsigned long uint;
#endif // uint

#ifndef int16_t
typedef  short   int16_t;
#endif //int16_t

#ifndef u_int16_t
typedef  unsigned short   u_int16_t;
#endif //u_int16_t

#ifndef uint16_t
typedef  u_int16_t   uint16_t;
#endif //u_int16_t

//#ifndef uint16
//typedef  u_int16_t   uint16;
//#endif //u_int16

#ifndef u_int8_t
typedef  unsigned char   u_int8_t;
#endif //u_int8_t

#ifndef uint8_t
typedef  u_int8_t   uint8_t;
#endif //uint8_t

#ifndef int8_t
typedef  char     int8_t;
#endif //uint8_t

//#ifndef uint8
//typedef  u_int8_t   uint8;
//#endif //uint8

#ifndef off_t
typedef long int off_t;   
#endif // off_t

// ptrdiff_t available in VS 8 + 
//#ifndef ptrdiff_t
//typedef long long ptrdiff_t;
//#endif // ptrdiff_t


//#ifndef atomic_t
//typedef struct { volatile int counter; } atomic_t;
//#endif // atomic_t

#ifndef bool
#define bool BOOL
#endif 

#ifndef true
#define true TRUE
#endif

#ifndef false
#define false FALSE
#endif

#ifdef DISABLE_PCAP
typedef int socklen_t;
#endif // DISABLE_PCAP

#endif // __UNIXTYPES_H__
