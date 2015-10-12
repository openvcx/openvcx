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


#if defined(VSX_HAVE_RTMP_HMAC)

#if !defined(_PTRDIFF_T)
#define _PTRDIFF_T
#endif // _PTRDIFF_T

#include <openssl/sha.h>
#include <openssl/hmac.h>

#define DH_KEY_LEN      128

#if defined(VSX_HAVE_RTMP_DH)

#include <openssl/bn.h>
#include <openssl/dh.h>

/* 2^1024 - 2^960 - 1 + 2^64 * { [2^894 pi] + 129093 } */
#define P1024 \
        "FFFFFFFFFFFFFFFFC90FDAA22168C234C4C6628B80DC1CD1" \
        "29024E088A67CC74020BBEA63B139B22514A08798E3404DD" \
        "EF9519B3CD3A431B302B0A6DF25F14374FE1356D6D51C245" \
        "E485B576625E7EC6F44C42E9A637ED6B0BFF5CB6F406B7ED" \
        "EE386BFB5A899FA5AE9F24117C4B1FE649286651ECE65381" \
        "FFFFFFFFFFFFFFFF"

/* Group morder largest prime factor: */
#define Q1024 \
        "7FFFFFFFFFFFFFFFE487ED5110B4611A62633145C06E0E68" \
        "948127044533E63A0105DF531D89CD9128A5043CC71A026E" \
        "F7CA8CD9E69D218D98158536F92F8A1BA7F09AB6B6A8E122" \
        "F242DABB312F3F637A262174D31BF6B585FFAE5B7A035BF6" \
        "F71C35FDAD44CFD2D74F9208BE258FF324943328F67329C0" \
        "FFFFFFFFFFFFFFFF"


static int dh_checkpubkey(BIGNUM *y, BIGNUM *p, BIGNUM *q) {
  BIGNUM *bn;
  BN_CTX *ctx;
  int rc = 0;

  bn = BN_new();

  /* y must lie in [2,p-1] */
  BN_set_word(bn, 1);
  if(BN_cmp(y, bn) < 0) {
    fprintf(stderr, "DH public key must be at least 2\n");
    BN_free(bn);
    return -1;   
  }

  /* bn = p-2 */
  BN_copy(bn, p);
  BN_sub_word(bn, 1);
  if(BN_cmp(y, bn) > 0) {
    fprintf(stderr, "DH public key must be at most p-2\n");
    BN_free(bn);
    return -1;
  }

  /* Verify with Sophie-Germain prime
   *
   * This is a nice test to make sure the public key position is calculated
   * correctly. This test will fail in about 50% of the cases if applied to
   * random data.
   */
  if(q) {
    /* y must fulfill y^q mod p = 1 */
    ctx = BN_CTX_new(); 
    BN_mod_exp(bn, y, q, p, ctx); 
    BN_CTX_free(ctx);

    if (BN_cmp(bn, BN_value_one()) != 0) {
      fprintf(stderr, "DH public key does not fulfill y^q mod p = 1\n");
      rc = -1;
    }
  }

  BN_free(bn);

  return rc;
}

static DH *dh_init(int lenBits) {
  DH *dh;

  if(!(dh = DH_new())) {
    return NULL;
  }

  if(!(dh->g = BN_new())) {
    DH_free(dh);
    return NULL;
  }

  if(! BN_hex2bn(&dh->p, P1024)) {
    DH_free(dh);
    return NULL;
  }

  BN_set_word(dh->g, 2);   // base 2

  dh->length = lenBits;

  return dh;
}

static int dh_gen_key(DH *dh) {
  int havekey = 0;
  BIGNUM *q1;

  while (!havekey) {
    q1 = NULL;

    if (!DH_generate_key(dh)) {
      return -1;
    }

    if(BN_hex2bn(&q1, Q1024) <= 0) {
      return -1;
    }

    if(dh_checkpubkey(dh->pub_key, dh->p, q1) < 0) {
      BN_free(dh->pub_key);
      BN_free(dh->priv_key);
      dh->pub_key = dh->priv_key = 0;
    } else {
      havekey = 1;
    }

    BN_free(q1);
  }

  return 0;
}

static int dh_get_pub_key(DH *dh, uint8_t *pubkey, unsigned int keyLen) {
  int len;

  len = BN_num_bytes(dh->pub_key);

  if (len <= 0 || len > (int) keyLen) {
    return -1;
  }

  memset(pubkey, 0, keyLen);
  BN_bn2bin(dh->pub_key, pubkey + (keyLen - len));

  return 0;
}

static int dh_makekey(unsigned char *pData, unsigned int len) {
  int rc = 0;
  DH *dhCtxt;

  if(!(dhCtxt = dh_init(1024))) {
    return -1;
  }

  if(rc >= 0 && dh_gen_key(dhCtxt) < 0) {
    rc = -1;
  }

  if(rc >= 0 && dh_get_pub_key(dhCtxt, pData, len) < 0) {
    rc = -1;
  } else {
    rc = DH_KEY_LEN;
  }

  DH_free(dhCtxt);

  return rc;
}

#endif // VSX_HAVE_RTMP_DH 


static const unsigned char fmskey[] = {
  0x47, 0x65, 0x6e, 0x75, 0x69, 0x6e, 0x65, 0x20, 
  0x41, 0x64, 0x6f, 0x62, 0x65, 0x20, 0x46, 0x6c,
  0x61, 0x73, 0x68, 0x20, 0x4d, 0x65, 0x64, 0x69, 
  0x61, 0x20, 0x53, 0x65, 0x72, 0x76, 0x65, 0x72,
  0x20, 0x30, 0x30, 0x31, // Genuine Adobe Flash Media Server 001 

  0xf0, 0xee, 0xc2, 0x4a, 0x80, 0x68, 0xbe, 0xe8, 
  0x2e, 0x00, 0xd0, 0xd1, 0x02, 0x9e, 0x7e, 0x57, 
  0x6e, 0xec, 0x5d, 0x2d, 0x29, 0x80, 0x6f, 0xab, 
  0x93, 0xb8, 0xe6, 0x36, 0xcf, 0xeb, 0x31, 0xae
};

static const unsigned char fpkey[] = {
  0x47, 0x65, 0x6e, 0x75, 0x69, 0x6e, 0x65, 0x20, 
  0x41, 0x64, 0x6f, 0x62, 0x65, 0x20, 0x46, 0x6c,
  0x61, 0x73, 0x68, 0x20, 0x50, 0x6c, 0x61, 0x79,
  0x65, 0x72, 0x20, 0x30, 0x30, 0x31,  // Genuine Adobe Flash Player 001 

  0xf0, 0xee, 0xc2, 0x4a, 0x80, 0x68, 0xbe, 0xe8, 
  0x2e, 0x00, 0xd0, 0xd1, 0x02, 0x9e, 0x7e, 0x57, 
  0x6e, 0xec, 0x5d, 0x2d, 0x29, 0x80, 0x6d, 0xab, 
  0x93, 0xb8, 0xe6, 0x36, 0xcf, 0xeb, 0x31, 0xae
};     

/*
static void HMACsha256(const unsigned char *pData, 
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
}
*/

static int rtmp_sign(const unsigned int digestPos, 
                      const unsigned char *pstart, 
                      const unsigned char *key, 
                      unsigned int lenKey, 
                      unsigned char *signature) {

  const int inLen = RTMP_HANDSHAKE_SZ - SHA256_DIGEST_LENGTH;
  unsigned char inData[inLen];
  int rc;

//fprintf(stderr, "CalculateDigest pos:%d keylen:%d len:%d\n", digestPos, lenKey, inLen);

  memcpy(inData, pstart, digestPos);
  memcpy(inData + digestPos, &pstart[digestPos + SHA256_DIGEST_LENGTH], 
         inLen - digestPos);

  //HMACsha256(inData, inLen, key, lenKey, signature);
  if((rc = hmac_calculate(inData, inLen, key, lenKey, signature, HMAC_CALC_TYPE_SHA256)) < 0) {
    LOG(X_ERROR("HMAC calculation failed"));
#if !defined(VSX_HAVE_CRYPTO) 
    LOG(X_ERROR(VSX_CRYPTO_DISABLED_BANNER));
#endif // (VSX_HAVE_CRYPTO) 
  }

  return rc;
}

typedef int (*RTMP_GET_OFFSET_FUNC)(const unsigned char *);

typedef struct RTMP_OFFSET_TYPES {
  RTMP_GET_OFFSET_FUNC  getOffsetSignature;
  RTMP_GET_OFFSET_FUNC  getOffsetDH;
} RTMP_OFFSET_TYPES_T;



static int offset_DH_1(const unsigned char *pData) {
  uint32_t offset = pData[1532] + pData[1533] + pData[1534] + pData[1535];
  int res = (offset % 632) + 772;

  if(res + DH_KEY_LEN > 1531) {  
    return -1;
  }

  return res;
}

static int offset_DH_2(const unsigned char *pData) {
  uint32_t offset = pData[768] + pData[769] + pData[770] + pData[771];
  int res = (offset % 632) + 8;

  if(res + DH_KEY_LEN > 1535) {   
    return -1;
  }

  return res;
}

static int offset_signature_1(const unsigned char *pData) {
  uint32_t offset = pData[8] + pData[9] + pData[10] + pData[11];
  int res = (offset % 728) + 12;

  if(res + 32 > 1535) { 
    return -1;
  }

  return res;
}

static int offset_signature_2(const unsigned char *pData) {
  uint32_t offset = pData[772] + pData[773] + pData[774] + pData[775];
  int res = (offset % 728) + 776;

  if(res + 32 > 1535) {
    return -1;
  }

  return res;
}


static void getOffsetTypes(RTMP_OFFSET_TYPES_T *pTypes, int type) {

  if(type == 1) {
    pTypes->getOffsetSignature = offset_signature_1;
    pTypes->getOffsetDH = offset_DH_1;
  } else {
    pTypes->getOffsetSignature = offset_signature_2;
    pTypes->getOffsetDH = offset_DH_2;
  }

}

static int verifydigest(const unsigned char *pIn, RTMP_OFFSET_TYPES_T *pTypes,
                        const unsigned char *key, unsigned int lenKey) {
  int rc = 0;
  int offset = -1;
  unsigned char digest[SHA256_DIGEST_LENGTH]; // 32

  getOffsetTypes(pTypes, 2);

  if((offset = pTypes->getOffsetSignature(pIn)) < 0) {
    LOG(X_WARNING("Unable to retrieve rtmp client digest offset"));
    memset(digest, 0, sizeof(digest));
  } else {

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - verifydigest client signature offset: %d"), offset);
                  LOGHEXT_DEBUG(&pIn[offset], 30) );

    if((rc = rtmp_sign(offset, pIn, key, lenKey, digest)) < 0) {
      return rc;
    }
  }

  if(offset < 0 || memcmp(digest, &pIn[offset], SHA256_DIGEST_LENGTH)) {

    LOG(X_DEBUG("Trying secondary rtmp signature location"));

    //
    // Try alternate offset methods
    // 
    getOffsetTypes(pTypes, 1);

    if((offset = pTypes->getOffsetSignature(pIn)) < 0) {
      LOG(X_ERROR("Unable to retrieve rtmp client digest offset"));
      return -1;
    }

    VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - verifydigest client secondary signature offset: %d"), offset);
                    LOGHEXT_DEBUG(&pIn[offset], 30) );

    if((rtmp_sign(offset, pIn, key, lenKey, digest)) < 0) {
      return rc;
    }

    if(memcmp(digest, &pIn[offset], SHA256_DIGEST_LENGTH)) {
      LOG(X_WARNING("Client rtmp handshake signature does not match"));
      return -1;
    }

  }

  LOG(X_DEBUG("Client rtmp handshake signature ok"));

  return offset;
}

#endif // VSX_HAVE_RTMP_HMAC 

static void offset_init_defaults(unsigned char *pOut) { 
  unsigned int idx;

  //
  // server uptime
  //
  *((uint32_t *) &pOut[0]) = htonl(96850050); 

  //
  // FME emulated server version
  //
  pOut[4] = 3;
  pOut[5] = 0;
  pOut[6] = 1;
  pOut[7] = 1;

  //
  // init w/ random bytes
  //
  for(idx = 8; idx < RTMP_HANDSHAKE_SZ; idx += 2) {
    *((uint16_t *) &pOut[idx]) = random();
  }

  //
  // produces signature offset 419 (algo 1)
  //
  pOut[8] = 0x42;
  pOut[9] = 0x46;
  pOut[10] = 0x5b;
  pOut[11] = 0xb4;

  //
  // produces signature offset 1356 (algo 2)
  //
  pOut[772] = 0x15;
  pOut[773] = 0xce;
  pOut[774] = 0x88;
  pOut[775] = 0xd9;

  //
  // produces DH offset 1021 (algo 1)
  //
  pOut[1532] = 0x2d;
  pOut[1533] = 0x9b;
  pOut[1534] = 0x1e;
  pOut[1535] = 0x13;

  //
  // produces DH offset 455 (algo 2)
  //
  pOut[768] = 0x72;
  pOut[769] = 0xc4;
  pOut[770] = 0x72;
  pOut[771] = 0x17;

}

static int fill_srv_handshake(unsigned char *pOut, unsigned char *pClient) {
  int rc;
#if defined(VSX_HAVE_RTMP_HMAC)

  unsigned char digest[SHA256_DIGEST_LENGTH]; // 32
  unsigned char signature[SHA256_DIGEST_LENGTH]; 
  unsigned char clidigest[SHA256_DIGEST_LENGTH]; 
  int offset;
  RTMP_OFFSET_TYPES_T offsetTypes;
  const unsigned char rtmp_hshakedata[] = {
              0x7b, 0x7b, 0x8c, 0xbf, 0x92, 0x44, 0xa4, 0x0f };

#endif // VSX_HAVE_RTMP_HMAC

  //fprintf(stderr, "client handshake:\n");
  //avc_dumpHex(stderr, &pClient[-1], 17, 1);

  //
  // Init the handshake 1536 byte output
  //
  offset_init_defaults(pOut);

  if(pClient[4] == 0) {
    //
    // If the client has not provided a version in the request, just
    // echo back the client request data in the server 2nd (1536 byte) resp
    //
    memcpy(&pOut[RTMP_HANDSHAKE_SZ], pClient, RTMP_HANDSHAKE_SZ);
    LOG(X_DEBUG("rtmp handshake from unversioned client"));
    return 0;
  } 

  LOG(X_DEBUG("RTMP Client version %d.%d.%d.%d"), pClient[4], pClient[5],
             pClient[6], pClient[7]);

#if !defined(VSX_HAVE_RTMP_HMAC) 
  LOG(X_WARNING("Unable to rtmp handshake because crypto is not compiled"));
#else

  //
  // Verify the client signature and obtain the signature offset method that is
  // being utilized by the client
  //
  memset(&offsetTypes, 0, sizeof(offsetTypes));
  if(verifydigest(pClient, &offsetTypes, fpkey, 30) < 0) {
    return -1; 
  }

  if(!offsetTypes.getOffsetSignature || !offsetTypes.getOffsetDH) {
    LOG(X_ERROR("Unable to retrieve rtmp key offsets for handhsake"));
    return -1;
  }

#if defined(VSX_HAVE_RTMP_DH)

  //
  // Insert a DH public key into the appropriate key offset
  //
  if((offset = offsetTypes.getOffsetDH(pOut)) < 0) {
    LOG(X_ERROR("Unable to retrieve rtmp server handshake DH offset"));
    return -1;
  }

  if(dh_makekey(&pOut[offset], DH_KEY_LEN) < 0) {
    LOG(X_ERROR("Unable to generate rtmp server handshake DH key"));
    return -1;
  }

#endif // VSX_HAVE_RTMP_DH 

  //
  // Insert a signature of the server outbound data 
  //
  if((offset = offsetTypes.getOffsetSignature(pOut)) < 0) {
    LOG(X_ERROR("Unable to retrieve rtmp server handshake signature offset"));
    return -1;
  }

  if((rc = rtmp_sign(offset, pOut, fmskey, 36, digest)) < 0) {
    return rc;
  }
  memcpy(&pOut[offset], digest, SHA256_DIGEST_LENGTH);

  //fprintf(stderr, "server signature offset:%d\n", offset);
  //avc_dumpHex(stderr, digest, SHA256_DIGEST_LENGTH, 1);

  if((offset = offsetTypes.getOffsetSignature(pClient)) < 0) {
    LOG(X_ERROR("Unable to retrieve rtmp client handshake signature offset"));
    return -1;
  }

  memcpy(&pOut[RTMP_HANDSHAKE_SZ], pClient, RTMP_HANDSHAKE_SZ);
  memcpy(&pOut[RTMP_HANDSHAKE_SZ], rtmp_hshakedata, 8);

  // 
  // Sign the server 2nd (1536) byte response
  //
  //HMACsha256(&pClient[offset], SHA256_DIGEST_LENGTH, fmskey, sizeof(fmskey), clidigest);
  hmac_calculate(&pClient[offset], SHA256_DIGEST_LENGTH, fmskey, sizeof(fmskey), clidigest, HMAC_CALC_TYPE_SHA256);
  //HMACsha256(&pOut[RTMP_HANDSHAKE_SZ], RTMP_HANDSHAKE_SZ - SHA256_DIGEST_LENGTH, 
  //           clidigest, SHA256_DIGEST_LENGTH, signature);
  hmac_calculate(&pOut[RTMP_HANDSHAKE_SZ], RTMP_HANDSHAKE_SZ - SHA256_DIGEST_LENGTH, 
                 clidigest, SHA256_DIGEST_LENGTH, signature, HMAC_CALC_TYPE_SHA256);

  memcpy(&pOut[2 * RTMP_HANDSHAKE_SZ - SHA256_DIGEST_LENGTH], signature, 
         SHA256_DIGEST_LENGTH);

  //fprintf(stderr, "client digest offset:%d\n", offset); 
  //avc_dumpHex(stderr, clidigest, SHA256_DIGEST_LENGTH, 1);
  //  fprintf(stderr, "signature offset:%d\n", 2 * RTMP_HANDSHAKE_SZ - SHA256_DIGEST_LENGTH);
  //avc_dumpHex(stderr, signature, SHA256_DIGEST_LENGTH, 1);

#endif // VSX_HAVE_RTMP_HMAC 

  return 0;
}


int rtmp_handshake_srv(RTMP_CTXT_T *pRtmp) {
  int rc;
  unsigned char buf[RTMP_HANDSHAKE_SZ + 1];

  // Ensure pRtmp->in.sz >= 2 * RTMP_HANDSHAKE_SZ 
  if((rc = netio_recvnb_exact(&pRtmp->pSd->netsocket, buf, 
                              RTMP_HANDSHAKE_SZ + 1, RTMP_HANDSHAKE_TMT_MS)) < 0) {
    LOG(X_ERROR("Failed to receive %d rtmp handshake bytes"), RTMP_HANDSHAKE_SZ + 1);
    return rc;
  }

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - handshake_srv recv: %d/%d"), rc, RTMP_HANDSHAKE_SZ + 1);
                  LOGHEXT_DEBUG(buf, RTMP_HANDSHAKE_SZ + 1) );

  if(buf[0] != RTMP_HANDSHAKE_HDR) {
    if(!memcmp("POST /fcs/ident2", buf, 16)) {
      LOG(X_ERROR("RTMPT client tunnel request not supported"));
      //http_resp_error(pRtmp->pSd, , HTTP_STATUS_NOTFOUND, 1, NULL, NULL);
    } else {
      LOG(X_ERROR("Invalid rtmp handshake header 0x%x"), buf[0]);
    }
    return -1;
  }

  pRtmp->in.buf[0] = RTMP_HANDSHAKE_HDR;

  if(fill_srv_handshake(&pRtmp->in.buf[1], &buf[1]) < 0) {
    return -1;
  }

  //fprintf(stderr, "server handshake:\n");
  //avc_dumpHex(stderr, pRtmp->in.buf, 17, 1);
  //avc_dumpHex(stderr, &pRtmp->in.buf[1537], 16, 1);

  if((rc = rtmp_sendbuf(pRtmp, pRtmp->in.buf, RTMP_HANDSHAKE_SZ * 2 + 1, "handshake_srv")) < 0) {
    return -1;
  }

  if((rc = netio_recvnb_exact(&pRtmp->pSd->netsocket, buf, RTMP_HANDSHAKE_SZ, 
                            RTMP_HANDSHAKE_TMT_MS)) < 0) {

    LOG(X_ERROR("Failed to receive %d rtmp handshake bytes"), RTMP_HANDSHAKE_SZ);
    return rc;
  }

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - handshake_srv recv: %d/%d"), rc, RTMP_HANDSHAKE_SZ);
                  LOGHEXT_DEBUG(buf, RTMP_HANDSHAKE_SZ) );

  //TODO: verify client handshake
  // memcmp(&buf[1], &pRtmp->in.buf[1], RTMP_HANDSHAKE_SZ);

  pRtmp->state = RTMP_STATE_CLI_HANDSHAKEDONE;
  LOG(X_DEBUG("rtmp handshake completed"));

  return rc;
}

//#undef VSX_HAVE_RTMP_HMAC

int rtmp_handshake_cli(RTMP_CTXT_T *pRtmp, int fp9) {
  int rc = 0;
  unsigned int idx;
  time_t tm;
  int sigOffset;
  int sigOffsetSrv;
  unsigned char bufout[RTMP_HANDSHAKE_SZ];

#if defined(VSX_HAVE_RTMP_HMAC)

  unsigned char digest[SHA256_DIGEST_LENGTH]; // 32
  unsigned char digest2[SHA256_DIGEST_LENGTH]; // 32
  RTMP_OFFSET_TYPES_T offsetTypes;

  if(!fp9) {
    LOG(X_DEBUG("RTMP Handshake signature disabled"));
  }

#else

  if(fp9) {
    LOG(X_WARNING("RTMP HMAC Support not compiled for handshake"));
  }

#endif // VSX_HAVE_RTMP_HMAC

  memset(pRtmp->out.buf, 0, RTMP_HANDSHAKE_SZ + 1);
  pRtmp->out.buf[0] = RTMP_HANDSHAKE_HDR;

  tm = time(NULL);
  memcpy(&pRtmp->out.buf[1], &tm, 4);

  for(idx = 9; idx < RTMP_HANDSHAKE_SZ; idx += 2) {
    *((uint16_t *) &pRtmp->out.buf[idx]) = random();
  }

#if defined(VSX_HAVE_RTMP_HMAC)

  if(fp9) {

    pRtmp->out.buf[5] = 0x0a;
    pRtmp->out.buf[6] = 0x00;
    pRtmp->out.buf[7] = 0x2d;
    pRtmp->out.buf[8] = 0x02;

    if((sigOffset = offset_signature_1(&pRtmp->out.buf[1])) < 0) {
      LOG(X_ERROR("RTMP Failed to generate client handshake digest offset"));
      return -1;
    }

    if((rc = rtmp_sign(sigOffset, &pRtmp->out.buf[1], fpkey, 30, digest)) < 0) {
      return rc;
    }
    memcpy(&pRtmp->out.buf[1 + sigOffset], digest, SHA256_DIGEST_LENGTH);

  }

#endif // VSX_HAVE_RTMP_HMAC

  if((rc = rtmp_sendbuf(pRtmp, pRtmp->out.buf, RTMP_HANDSHAKE_SZ + 1, "handshake_cli")) < 0) {
    return rc;
  }

  if((rc = netio_recvnb_exact(&pRtmp->pSd->netsocket, pRtmp->in.buf,
                              RTMP_HANDSHAKE_SZ * 2 + 1, RTMP_HANDSHAKE_TMT_MS)) < 0) {

    LOG(X_ERROR("Failed to receive %d rtmp handshake bytes"), RTMP_HANDSHAKE_SZ);
    return rc;
  }

  VSX_DEBUG_RTMP( LOG(X_DEBUG("RTMP - handshake_cli recv: %d/%d"), rc, RTMP_HANDSHAKE_SZ * 2 + 1);
                  LOGHEXT_DEBUG(pRtmp->in.buf, rc) );

  if(pRtmp->in.buf[0] != RTMP_HANDSHAKE_HDR) {
    LOG(X_ERROR("RTMP Invalid server handshake header byte 0x%x"), pRtmp->in.buf[0]);
    return -1;
  } 

  LOG(X_DEBUG("RTMP Server version %d.%d.%d.%d"), pRtmp->in.buf[5], pRtmp->in.buf[6],
             pRtmp->in.buf[7], pRtmp->in.buf[8]);


#if !defined(VSX_HAVE_RTMP_HMAC)

  memcpy(bufout, &pRtmp->in.buf[1], RTMP_HANDSHAKE_SZ);

#else

  if(!fp9) {

    memcpy(bufout, &pRtmp->in.buf[1], RTMP_HANDSHAKE_SZ);

  } else {

    for(idx = 0; idx < RTMP_HANDSHAKE_SZ; idx += 2) {
      *((uint16_t *) &bufout[idx]) = random();
    }

    memset(&offsetTypes, 0, sizeof(offsetTypes));
    if((sigOffsetSrv = verifydigest(&pRtmp->in.buf[1], &offsetTypes, fmskey, 36)) < 0) {
      return -1;
    }

    //
    // Sign the (1536) byte response
    //
    //HMACsha256(&pRtmp->in.buf[1 + sigOffsetSrv], SHA256_DIGEST_LENGTH, 
    //             fpkey, sizeof(fpkey), digest);
    hmac_calculate(&pRtmp->in.buf[1 + sigOffsetSrv], SHA256_DIGEST_LENGTH, 
                 fpkey, sizeof(fpkey), digest, HMAC_CALC_TYPE_SHA256);
    //HMACsha256(bufout, RTMP_HANDSHAKE_SZ - SHA256_DIGEST_LENGTH,
    //           digest, SHA256_DIGEST_LENGTH, digest2);
    hmac_calculate(bufout, RTMP_HANDSHAKE_SZ - SHA256_DIGEST_LENGTH,
               digest, SHA256_DIGEST_LENGTH, digest2, HMAC_CALC_TYPE_SHA256);

    memcpy(&bufout[RTMP_HANDSHAKE_SZ - SHA256_DIGEST_LENGTH], digest2,
           SHA256_DIGEST_LENGTH);

  }
#endif // VSX_HAVE_RTMP_HMAC

  if((rc = rtmp_sendbuf(pRtmp, bufout, RTMP_HANDSHAKE_SZ, "handshake_cli")) < 0) {
    return rc;
  }

  pRtmp->out.idx = 0;

  pRtmp->state = RTMP_STATE_CLI_HANDSHAKEDONE;
  LOG(X_DEBUG("rtmp handshake completed"));

  return rc;
}

