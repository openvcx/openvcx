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

#if defined(VSX_HAVE_SSL)

#undef ptrdiff_t
#include "openssl/ssl.h"
#include "openssl/err.h"

static int g_ssl_init;

int sslutil_init() {

  if(!g_ssl_init) {
    g_ssl_init = 1;

    SSL_load_error_strings();
    SSL_library_init();
    //ERR_load_BIO_strings();
    OpenSSL_add_all_algorithms();
    OpenSSL_add_all_ciphers();
    //OpenSSL_add_all_digests();

  }

  return 0;

}

int sslutil_close() {

  if(g_ssl_init) {
    EVP_cleanup();
    ERR_free_strings();
    g_ssl_init = 0;
  }

  return 0;
}


#endif // VSX_HAVE_SSL
