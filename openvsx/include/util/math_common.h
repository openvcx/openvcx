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

#ifndef __MATH_COMMON_H___
#define __MATH_COMMON_H___


#define HTONL64(ui64)   ((uint64_t) (((unsigned char *)&ui64)[0]) << 56) | \
                        ((uint64_t) (((unsigned char *)&(ui64))[1]) << 48) | \
                        ((uint64_t) (((unsigned char *)&(ui64))[2]) << 40) | \
                        ((uint64_t) (((unsigned char *)&(ui64))[3]) << 32) | \
                        ((uint64_t) (((unsigned char *)&(ui64))[4]) << 24) | \
                        ((uint64_t) (((unsigned char *)&(ui64))[5]) << 16) | \
                        ((uint64_t) (((unsigned char *)&(ui64))[6]) << 8) | \
                        ((uint64_t) (((unsigned char *)&(ui64))[7]));



int math_log2_32(uint32_t ui32);



#endif // __MATH_COMMON_H___
