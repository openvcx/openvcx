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


#ifndef __SHA_H__
#define __SHA_H__

#define SHS_DIGESTSIZE  20
#define SHS_BLOCKSIZE   64 

void shsSign(const unsigned char *data, int iDataLen, 
              const unsigned char *key, int ikeyLen,
              unsigned char *output, int iOutLen);

void shsSignArray(const char **ppData, unsigned char *output, int iOutLen);

void shsSignEx(const unsigned char *data, int iDataLen, 
               const unsigned char *key, int iKeyLen,
               unsigned char *output, int iOutLen);


#endif //__SHA_H__
