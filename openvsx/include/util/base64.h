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


#ifndef __BASE_64_H__
#define __BASE_64_H__

int base64_encode(const unsigned char *pIn, unsigned int lenIn,
                 char *pOut, unsigned int lenOut);
int base64_decode(const char *pIn, unsigned char *pOut, unsigned int lenOut);
int hex_encode(const unsigned char *pIn, unsigned int lenIn,
                 char *pOut, unsigned int lenOut);
int hex_decode(const char *pIn, unsigned char *pOut, unsigned int lenOut);



#endif //__BASE_64_H__
