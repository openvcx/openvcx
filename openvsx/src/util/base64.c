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


#include "vsx_common.h"


int hex_decode(const char *pIn, unsigned char *pOut, unsigned int lenOut) {
  unsigned int idxIn;
  uint8_t nib;

  if(!pIn || !pOut || lenOut < 1) {
    return -1;
  }

  for(idxIn = 0; pIn[idxIn] != '\0'; idxIn++) {
    if(idxIn + 1 >= 2 * lenOut) {
      return -1;
    } else if(pIn[idxIn] >= 'a' && pIn[idxIn] <= 'f') {
      nib = 10 + pIn[idxIn] - 'a';
    } else if(pIn[idxIn] >= 'A' && pIn[idxIn] <= 'F') {
      nib = 10 + pIn[idxIn] - 'A';
    } else if(pIn[idxIn] >= '0' && pIn[idxIn] <= '9') {
      nib = pIn[idxIn] - '0';
    } else {
      return -1;
    }
    if(idxIn % 2 == 0) {
      pOut[idxIn / 2] = (nib & 0x0f) << 4; 
    } else {
      pOut[idxIn / 2] |= (nib & 0x0f);
    }
  }

  if(idxIn % 2 == 1) {
    pOut[idxIn++ / 2] >>= 4;
  }

  pOut[idxIn / 2 + 1] = '\0';

  return idxIn / 2;
}

int hex_encode(const unsigned char *pIn, unsigned int lenIn,
               char *pOut, unsigned int lenOut) {
  unsigned int idxIn = 0;
  //static const char *shexvals = "0123456789ABCDEF";
  static const char *shexvals = "0123456789abcdef";

  if(!pIn || !pOut || lenOut < lenIn * 2 + 1) {
    return -1;
  }

  for(idxIn = 0; idxIn < lenIn; idxIn++) {
    pOut[idxIn * 2] = shexvals[ ((pIn[idxIn] >> 4) & 0x0f) ];
    pOut[idxIn * 2 + 1] = shexvals[ (pIn[idxIn] & 0x0f) ];
  }

  pOut[idxIn * 2] = '\0';

  return (int) idxIn * 2;
}

int base64_encode(const unsigned char *pIn, unsigned int lenIn,
                  char *pOut, unsigned int lenOut) {
  static const char *sb64vals =
        "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";
  unsigned int idxIn;
  unsigned int idxOut = 0;
  uint32_t val = 0;
  int32_t shift = 0; 

  if(!pIn || !pOut || lenOut < (lenIn+2) / 3 * 4 + 1) {
    return -1;
  }

  for(idxIn = 0; idxIn < lenIn; idxIn++) { 
    val = (val << 8) + pIn[idxIn];
    shift += 8;

    do {
      pOut[idxOut++] = sb64vals[ ((val << 6) >> shift) & 0x3f ];
      shift -= 6;
    } while(shift > 6 || (shift > 0 && idxIn + 1 == lenIn));
  }

  while(idxOut & 0x03) {
    pOut[idxOut++] = '=';  
  }
  pOut[idxOut] = '\0';

  return (int) idxOut;
}

int base64_decode(const char *pIn, unsigned char *pOut, unsigned int lenOut) {
  unsigned int idxIn;
  unsigned int idxOut;
  uint32_t val;
  
  for(idxIn = 0, idxOut = 0; pIn[idxIn] != '\0'; idxIn++) {

    if(idxOut + 1 >= lenOut) {
      return -1;
    } if(pIn[idxIn] >= 'A' && pIn[idxIn] <= 'Z') {
      val = pIn[idxIn] - 'A'; 
    } else if(pIn[idxIn] >= 'a' && pIn[idxIn] <= 'z') {
      val = pIn[idxIn] - 'a' + 26; 
    } else if(pIn[idxIn] >= '0' && pIn[idxIn] <= '9') {
      val = pIn[idxIn] - '0' + 52;
    } else if(pIn[idxIn] == '+') {
      val = 62;
    } else if(pIn[idxIn] == '/') {
      val = 63;
    } else if(pIn[idxIn] == '=') {
      val = 0;
      break;
    } else {
      return -1;
    }
//fprintf(stderr, "val:0x%x %d\n", val, val);
    if(idxIn % 4 == 0) {
      pOut[idxOut] = (val << 2);
    } else if(idxIn % 4 == 1) {
      pOut[idxOut++] |= ((val >> 4) & 0x03);
//fprintf(stderr, "[%d] 0x%x \n", idxOut-1, pOut[idxOut-1]);
      pOut[idxOut] = (val << 4);
    } else if(idxIn % 4 == 2) {
      pOut[idxOut++] |= (val >> 2);
//fprintf(stderr, "[%d] 0x%x \n", idxOut-1, pOut[idxOut-1]);
      pOut[idxOut] = (val << 6);
    } else if(idxIn % 4 == 3) {
      pOut[idxOut++] |= (val & 0x3f);
//fprintf(stderr, "[%d] 0x%x \n", idxOut-1, pOut[idxOut-1]);
    }

  }

  pOut[idxOut] = '\0';

  return idxOut;
}
