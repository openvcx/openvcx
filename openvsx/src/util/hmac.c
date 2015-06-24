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

#if defined(VSX_HAVE_CRYPTO) 

#if !defined(_PTRDIFF_T)
#define _PTRDIFF_T
#endif // _PTRDIFF_T

#include <openssl/sha.h>
#include <openssl/hmac.h>
#include <openssl/md5.h>


static int hmac_calculate_sha1(const unsigned char *pData,
                      unsigned int lenData,
                      const unsigned char *key,
                      unsigned int lenKey,
                      unsigned char *signature) {

  unsigned int digestLen;

  HMAC_CTX ctx;
  HMAC_CTX_init(&ctx);
  HMAC_Init_ex(&ctx, key, lenKey, EVP_sha1(), NULL);
  HMAC_Update(&ctx, pData, lenData);
  HMAC_Final(&ctx, signature, &digestLen);
  HMAC_CTX_cleanup(&ctx);

  return SHA_DIGEST_LENGTH;
}


static int hmac_calculate_sha256(const unsigned char *pData,
                 unsigned int lenData,
                 const unsigned char *key,
                 unsigned int lenKey,
                 unsigned char *signature) {

  unsigned int digestLen;

  HMAC_CTX ctx;
  HMAC_CTX_init(&ctx);
  HMAC_Init_ex(&ctx, key, lenKey, EVP_sha256(), NULL);
  HMAC_Update(&ctx, pData, lenData);
  HMAC_Final(&ctx, signature, &digestLen);
  HMAC_CTX_cleanup(&ctx);

  return SHA256_DIGEST_LENGTH;
}

int hmac_calculate(const unsigned char *pData,
                 unsigned int lenData,
                 const unsigned char *key,
                 unsigned int lenKey,
                 unsigned char *signature,
                 HMAC_CALC_TYPE_T type) {

  switch(type) {
    case HMAC_CALC_TYPE_SHA1:
      return hmac_calculate_sha1(pData, lenData, key, lenKey, signature);
    case HMAC_CALC_TYPE_SHA256:
      return hmac_calculate_sha256(pData, lenData, key, lenKey, signature);
    default:
      return -1;
  }

}

/*
int md5_calculate(const unsigned char *pData,
                         unsigned int lenData,
                         unsigned char *hash) {
  MD5_CTX c;
  MD5_Init(&c);
  MD5_Update(&c,pData, lenData);
  MD5_Final(&(hash[0]),&c);

  return MD5_DIGEST_LENGTH;
}
*/

#else // VSX_HAVE_CRYPTO

/*
int md5_calculate(const unsigned char *pData,
                         unsigned int lenData,
                         unsigned char *hash) {
  return -1;
}
*/

int hmac_calculate(const unsigned char *pData,
                 unsigned int lenData,
                 const unsigned char *key,
                 unsigned int lenKey,
                 unsigned char *signature,
                 HMAC_CALC_TYPE_T type) {
  return -1;
}

#endif // VSX_HAVE_CRYPTO
