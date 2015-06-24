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

//
// NIST proposed Secure Hash Standard.
//
// Written 2 September 1992, Peter C. Gutmann.
// This implementation placed in the public domain.
//
// Comments to pgut1@cs.aukuni.ac.nz
//

//   Note: This is actually the original SHA ver, SHA-1 was
//   introduced in 1994 to fix a bug in SHA

#define SHS_BLOCKSIZE	64

typedef struct {
  uint32_t  digest [5];	        // Message digest 
  uint32_t  countLo, countHi;	// 64-bit bit count 
  uint32_t  data [16];		// SHS data buffer 
  uint32_t  data_count;
} SHS_INFO;

void shsInit(SHS_INFO *shsInfo);
void shsUpdate(SHS_INFO *shsInfo, uint8_t *buffer, int count);
void shsFinal(SHS_INFO *shsInfo);


/* The SHS f()-functions */

#define f1(x,y,z)   ( ( x & y ) | ( ~x & z ) )		  /* Rounds  0-19 */
#define f2(x,y,z)   ( x ^ y ^ z )			  /* Rounds 20-39 */
#define f3(x,y,z)   ( ( x & y ) | ( x & z ) | ( y & z ) ) /* Rounds 40-59 */
#define f4(x,y,z)   ( x ^ y ^ z )			  /* Rounds 60-79 */

/* The SHS Mysterious Constants */

#define K1  0x5A827999L 	/* Rounds  0-19 */
#define K2  0x6ED9EBA1L 	/* Rounds 20-39 */
#define K3  0x8F1BBCDCL 	/* Rounds 40-59 */
#define K4  0xCA62C1D6L 	/* Rounds 60-79 */

/* SHS initial values */

#define h0init	0x67452301L
#define h1init	0xEFCDAB89L
#define h2init	0x98BADCFEL
#define h3init	0x10325476L
#define h4init	0xC3D2E1F0L

/* 32-bit rotate - kludged with shifts */

#define S(n,X)	((X << n) | (X >> (32 - n)))

/* The initial expanding function */

#define expand(count)	W [count] = W [count - 3] ^ W [count - 8] ^ W [count - 14] ^ W [count - 16]

/* The four SHS sub-rounds */

#define subRound1(count)  { \
		temp = S (5, A) + f1 (B, C, D) + E + W [count] + K1; \
		E = D; \
		D = C; \
		C = S (30, B); \
		B = A; \
		A = temp; \
	}

#define subRound2(count)  { \
		temp = S (5, A) + f2 (B, C, D) + E + W [count] + K2; \
		E = D; \
		D = C; \
		C = S (30, B); \
		B = A; \
		A = temp; \
	}

#define subRound3(count) { \
		temp = S (5, A) + f3 (B, C, D) + E + W [count] + K3; \
		E = D; \
		D = C; \
		C = S (30, B); \
		B = A; \
		A = temp; \
	}

#define subRound4(count) { \
		temp = S (5, A) + f4 (B, C, D) + E + W [count] + K4; \
		E = D; \
		D = C; \
		C = S (30, B); \
		B = A; \
		A = temp; \
	}

/* The two buffers of 5 32-bit words */

uint32_t h0, h1, h2, h3, h4;
uint32_t A, B, C, D, E;

static void byteReverse(uint32_t *buffer, int byteCount);
void shsTransform(SHS_INFO *shsInfo);

/* Initialize the SHS values */

void shsInit (SHS_INFO *shsInfo) {
	/* Set the h-vars to their initial values */
	shsInfo->digest [0] = h0init;
	shsInfo->digest [1] = h1init;
	shsInfo->digest [2] = h2init;
	shsInfo->digest [3] = h3init;
	shsInfo->digest [4] = h4init;

	/* Initialise bit count */
	shsInfo->countLo = shsInfo->countHi = 0L;

    memset((char *) shsInfo->data, 0, SHS_BLOCKSIZE);
    shsInfo->data_count = 0;
}

/*
 * Perform the SHS transformation.  Note that this code, like MD5, seems to
 * break some optimizing compilers - it may be necessary to split it into
 * sections, eg based on the four subrounds
 */

void shsTransform (SHS_INFO *shsInfo) {
	uint32_t W [80], temp;
	int i;

	/* Step A.	Copy the data buffer into the local work buffer */
	for (i = 0; i < 16; i++)
		W [i] = shsInfo->data [i];

	/* Step B.	Expand the 16 words into 64 temporary data words */
	expand (16); expand (17); expand (18); expand (19); expand (20);
	expand (21); expand (22); expand (23); expand (24); expand (25);
	expand (26); expand (27); expand (28); expand (29); expand (30);
	expand (31); expand (32); expand (33); expand (34); expand (35);
	expand (36); expand (37); expand (38); expand (39); expand (40);
	expand (41); expand (42); expand (43); expand (44); expand (45);
	expand (46); expand (47); expand (48); expand (49); expand (50);
	expand (51); expand (52); expand (53); expand (54); expand (55);
	expand (56); expand (57); expand (58); expand (59); expand (60);
	expand (61); expand (62); expand (63); expand (64); expand (65);
	expand (66); expand (67); expand (68); expand (69); expand (70);
	expand (71); expand (72); expand (73); expand (74); expand (75);
	expand (76); expand (77); expand (78); expand (79);

	/* Step C.	Set up first buffer */
	A = shsInfo->digest [0];
	B = shsInfo->digest [1];
	C = shsInfo->digest [2];
	D = shsInfo->digest [3];
	E = shsInfo->digest [4];

	/* Step D.	Serious mangling, divided into four sub-rounds */
	subRound1  (0); subRound1  (1); subRound1  (2); subRound1  (3);
	subRound1  (4); subRound1  (5); subRound1  (6); subRound1  (7);
	subRound1  (8); subRound1  (9); subRound1 (10); subRound1 (11);
	subRound1 (12); subRound1 (13); subRound1 (14); subRound1 (15);
	subRound1 (16); subRound1 (17); subRound1 (18); subRound1 (19);

	subRound2 (20); subRound2 (21); subRound2 (22); subRound2 (23);
	subRound2 (24); subRound2 (25); subRound2 (26); subRound2 (27);
	subRound2 (28); subRound2 (29); subRound2 (30); subRound2 (31);
	subRound2 (32); subRound2 (33); subRound2 (34); subRound2 (35);
	subRound2 (36); subRound2 (37); subRound2 (38); subRound2 (39);

	subRound3 (40); subRound3 (41); subRound3 (42); subRound3 (43);
	subRound3 (44); subRound3 (45); subRound3 (46); subRound3 (47);
	subRound3 (48); subRound3 (49); subRound3 (50); subRound3 (51);
	subRound3 (52); subRound3 (53); subRound3 (54); subRound3 (55);
	subRound3 (56); subRound3 (57); subRound3 (58); subRound3 (59);

	subRound4 (60); subRound4 (61); subRound4 (62); subRound4 (63);
	subRound4 (64); subRound4 (65); subRound4 (66); subRound4 (67);
	subRound4 (68); subRound4 (69); subRound4 (70); subRound4 (71);
	subRound4 (72); subRound4 (73); subRound4 (74); subRound4 (75);
	subRound4 (76); subRound4 (77); subRound4 (78); subRound4 (79);

	/* Step E.	Build message digest */
	shsInfo->digest [0] += A;
	shsInfo->digest [1] += B;
	shsInfo->digest [2] += C;
	shsInfo->digest [3] += D;
	shsInfo->digest [4] += E;
}

static void byteReverse (uint32_t *buffer, int byteCount) {
	uint32_t value;
	int count;

	/*
	 * Find out what the byte order is on this machine.
	 * Big endian is for machines that place the most significant byte
	 * first (eg. Sun SPARC). Little endian is for machines that place
	 * the least significant byte first (eg. VAX).
	 *
	 * We figure out the byte order by stuffing a 2 byte string into a
	 * short and examining the left byte. '@' = 0x40  and  'P' = 0x50
	 * If the left byte is the 'high' byte, then it is 'big endian'.
	 * If the left byte is the 'low' byte, then the machine is 'little
	 * endian'.
	 *
	 *                          -- Shawn A. Clifford (sac@eng.ufl.edu)
	 */

	/*
	 * Several bugs fixed       -- Pat Myrto (pat@rwing.uucp)
	 */

	if ((*(uint16_t *) ("@P") >> 8) == '@')
		return;

	byteCount /= sizeof (uint32_t);
	for (count = 0; count < byteCount; count++) {
		value = (buffer [count] << 16) | (buffer [count] >> 16);
		buffer [count] = ((value & 0xFF00FF00L) >> 8) | ((value & 0x00FF00FFL) << 8);
	}
}

/*
 * Update SHS for a block of data.  This code assumes that the buffer size is
 * a multiple of SHS_BLOCKSIZE bytes long, which makes the code a lot more
 * efficient since it does away with the need to handle partial blocks
 * between calls to shsUpdate()
 */

void shsUpdate (SHS_INFO *shsInfo, uint8_t *buffer, int count) {
	/* Update bitcount */
	if ((shsInfo->countLo + ((uint32_t) count << 3)) < shsInfo->countLo)
		 shsInfo->countHi++;	/* Carry from low to high bitCount */
	shsInfo->countLo += ((uint32_t) count << 3);
	shsInfo->countHi += ((uint32_t) count >> 29);

	/* Process data in SHS_BLOCKSIZE chunks */
	while (count >= SHS_BLOCKSIZE - shsInfo->data_count) {
		memcpy (&((char *)shsInfo->data)[shsInfo->data_count], buffer, SHS_BLOCKSIZE - shsInfo->data_count);
		byteReverse (shsInfo->data, SHS_BLOCKSIZE);
		shsTransform (shsInfo);
		buffer += (SHS_BLOCKSIZE - shsInfo->data_count);
		count -= (SHS_BLOCKSIZE - shsInfo->data_count);
        shsInfo->data_count = 0;
	}

	/*
	 * Handle any remaining bytes of data.
	 * This should only happen once on the final lot of data
	 */
	memcpy (&((char *)shsInfo->data)[shsInfo->data_count], buffer, count);
    shsInfo->data_count += count;
}

void shsFinal (SHS_INFO *shsInfo) {
	int count;
	uint32_t lowBitcount = shsInfo->countLo, highBitcount = shsInfo->countHi;

	/* Compute number of bytes mod 64 */
	count = (int) ((shsInfo->countLo >> 3) & 0x3F);

	/*
	 * Set the first char of padding to 0x80.
	 * This is safe since there is always at least one byte free
	 */
	((uint8_t *) shsInfo->data) [count++] = 0x80;

	/* Pad out to 56 mod 64 */
	if (count > 56) {
		/* Two lots of padding:  Pad the first block to 64 bytes */
		memset ((uint8_t *) shsInfo->data + count, 0, 64 - count);
		byteReverse (shsInfo->data, SHS_BLOCKSIZE);
		shsTransform (shsInfo);

		/* Now fill the next block with 56 bytes */
		memset (shsInfo->data, 0, 56);
	} else
		/* Pad block to 56 bytes */
		memset ((uint8_t *) shsInfo->data + count, 0, 56 - count);
	byteReverse (shsInfo->data, SHS_BLOCKSIZE);

	/* Append length in bits and transform */
	shsInfo->data [14] = highBitcount;
	shsInfo->data [15] = lowBitcount;

	shsTransform (shsInfo);
	//byteReverse (shsInfo->data, SHS_DIGESTSIZE);  // <--- is this correct, shouldn't it be shsInfo->digest? 
    byteReverse (shsInfo->digest, SHS_DIGESTSIZE);
}


void shsSign (const unsigned char *data, int iDataLen, 
              const unsigned char *key, int iKeyLen,
              unsigned char *output, int iOutLen) {
  SHS_INFO shsInfo;
    if(output && iOutLen >= 20 && data && iDataLen > 0) {

        shsInit (&shsInfo);
        if(key && iKeyLen > 0) {
          shsUpdate(&shsInfo, (unsigned char *) key, iKeyLen);
        }
        shsUpdate(&shsInfo, (unsigned char *)data, iDataLen);
        shsFinal (&shsInfo);

        memcpy(output, shsInfo.digest, 20);
    }
}


//
// Signs an NULL terminated array
//

void shsSignArray(const char **ppData, unsigned char *output, int iOutLen) {
  SHS_INFO shsInfo;
  int k;
  unsigned char buf[20];
  char **ppIterData = (char **) ppData;

    if(ppData && output && iOutLen >= 20) {

        memset(output, 0x00, iOutLen);

        while(*ppIterData) {
            shsInit (&shsInfo);
            shsUpdate(&shsInfo, (unsigned char *) *ppIterData, strlen(*ppIterData));
            shsFinal (&shsInfo);

            memcpy(buf, shsInfo.digest, 20);
            
            for(k = 0; k < 20; k ++)
            {
                output[k] ^= buf[k];
            }

            ppIterData++;
        }    
    }
}



//
// Splits the key into two distinct parts.  The first part
// prepends the data, the second part appends the data.
// This is done so as to prevent updating the hash
// with a key before reaching the 512 bit boundary.
//
// A weak choice is:  iKeyLen + iDataLen % 64 == 0 
//
//

void shsSignEx(const unsigned char *data, int iDataLen, 
               const unsigned char *key, int iKeyLen,
               unsigned char *output, int iOutLen) {

  unsigned int uiHalfKeyLen = iKeyLen / 2;
  SHS_INFO shsInfo;
  unsigned char buffer[SHS_BLOCKSIZE];
  long lBytes1 =0, lBytes2 = 0;
  unsigned char *ptrData = (unsigned char *)key;

    if(output && iOutLen >= 20 && data && iDataLen > 0) {

        shsInit (&shsInfo);
        
        lBytes1 = uiHalfKeyLen;
        while(lBytes1 > SHS_BLOCKSIZE) {       
            shsUpdate(&shsInfo, ptrData, SHS_BLOCKSIZE);
            ptrData += SHS_BLOCKSIZE;
            lBytes1 -= SHS_BLOCKSIZE;
        }
        
        memcpy(buffer, ptrData, lBytes1);

        lBytes2 = SHS_BLOCKSIZE - lBytes1;
        lBytes1 = lBytes2 > iDataLen ? iDataLen : lBytes2;
        memcpy(&buffer[SHS_BLOCKSIZE - lBytes2], data, lBytes1);

        // buffer is full
        if(lBytes2 == lBytes1) {
            shsUpdate(&shsInfo, buffer, SHS_BLOCKSIZE);
            if(lBytes1 < iDataLen)
                ptrData = (unsigned char *) &data[lBytes1];
            lBytes2 = iDataLen - lBytes1;
            while(lBytes2 > SHS_BLOCKSIZE) {       
                shsUpdate(&shsInfo, ptrData, SHS_BLOCKSIZE);
                ptrData += SHS_BLOCKSIZE;
                lBytes2 -= SHS_BLOCKSIZE;
            }

            memcpy(buffer, ptrData, lBytes2);
            lBytes2 = SHS_BLOCKSIZE - lBytes2;

        }
        else {  // lBytes2 > lBytes1
            lBytes2 -= lBytes1;
        }
                
        lBytes1 = (int)uiHalfKeyLen > lBytes2 ? lBytes2 : uiHalfKeyLen;
        memcpy(&buffer[SHS_BLOCKSIZE - lBytes2], &key[uiHalfKeyLen], lBytes1);
        shsUpdate(&shsInfo, buffer, SHS_BLOCKSIZE - lBytes2 + lBytes1);

        if(lBytes1 < (int)uiHalfKeyLen) {
            ptrData = (unsigned char *) &key[uiHalfKeyLen + lBytes1];
            lBytes2 = uiHalfKeyLen - lBytes1;
            while(lBytes2 > 0) {       
                shsUpdate(&shsInfo, ptrData, lBytes2);
                if(lBytes2 >= SHS_BLOCKSIZE)
                    ptrData += SHS_BLOCKSIZE;
                lBytes2 -= SHS_BLOCKSIZE;
            }
        }
        
        shsFinal (&shsInfo);

        memcpy(output, shsInfo.digest, 20);
    }
}
