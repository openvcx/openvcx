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


#ifndef __HMAC_H__
#define __HMAC_H__

typedef enum HMAC_CALC_TYPE {
  HMAC_CALC_TYPE_SHA1        = 1,
  HMAC_CALC_TYPE_SHA256      = 2 
} HMAC_CALC_TYPE_T;

int hmac_calculate(const unsigned char *pData,
                 unsigned int lenData,
                 const unsigned char *key,
                 unsigned int lenKey,
                 unsigned char *signature,
                 HMAC_CALC_TYPE_T type);

#endif //__HMAC_H__
