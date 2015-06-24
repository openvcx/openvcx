/**
 *  Copyright 2014 OpenVCX openvcx@gmail.com
 *
 *  You may not use this file except in compliance with the OpenVCX License.

 *  The OpenVCX License is based on the Apache Version 2.0 License with
 *  additional credit attribution clauses mentioned in section 4 (e) and
 *  4 (f).
 *
 *  4 (e) Redistributions in source or binary form must reproduce the
 *        aforementioned copyright notice, list of conditions and any
 *        disclaimers in the documentation and/or other materials provided
 *        with the distribution.
 *
 *    (f) All advertising materials mentioning features or use of this
 *        software must display the following acknowledgement:
 *        "This product includes software from OpenVCX".
 *
 *  You may obtain a copy of the Apache License, Version 2.0  at
 *
 *      http://www.apache.org/licenses/LICENSE-2.0
 *
 *  Unless required by applicable law or agreed to in writing, software
 *  distributed under the License is distributed on an "AS IS" BASIS,
 *  WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 *  See the License for the specific language governing permissions and
 *  limitations under the License.
 *
 */

package com.openvcx.conference;

import java.security.MessageDigest;
import java.security.NoSuchAlgorithmException;
import java.security.SecureRandom;
import java.math.BigInteger;
import java.util.Random;

import org.apache.commons.codec.binary.Base64;
import org.apache.commons.codec.binary.Hex;
import org.apache.log4j.Logger;

/**
 *
 *  A random number and crypto key generator
 *
 */
public class KeyGenerator {

    private SecureRandom m_rand;

    private static final Object m_sync = new Object();
    private final Logger m_log = Logger.getLogger(getClass());

    /**
     * <p>A string identifying the SHA-1 message digest algorithm.</p>
     * <p>The value evaluates to the string <i>SHA-1</i></p>
     */
    public static final String ALGO_SHA1         = "SHA-1";

    /**
     * <p>A string identifying the MD-5 message digest algorithm.</p>
     * <p>The value evaluates to the string <i>MD5</i></p>
     */
    public static final String ALGO_MD5          = "MD5";

    /**
     * Constructor used to initialize this instance
     */
    public KeyGenerator() {
        m_rand = new SecureRandom();
    }

    /**
     * <p>Calculates a message digest of the input byte sequence</p>
     * <p><i>java.security.MessageDigest</i> is used to compute the hash</p>
     * @param raw The input byte sequence which should be used for computing the hash
     * @param algo The hash algorithm
     * @return A sequence of bytes consisting of the computed hash
     */
    public static byte[] createHash(byte[] raw, String algo) throws NoSuchAlgorithmException {
        return MessageDigest.getInstance(algo).digest(raw);
    }

    /**
     * <p>Obtains a sequence of pseudo-random bytes.</p>
     * <p><i>java.util.Random</i> is used as the random number generator</p>
     * @param length The length in bytes of the random byte sequence to generate
     * @return The random byte sequence
     */
    public static byte[] getWeakRandomBytes(int length) {
        byte [] b = new byte[length];
        new Random().nextBytes(b);
        return b;
    }

    /**
     * <p>Obtains a sequence of random bytes.</p>
     * <p>SHA-1 is used to hash the random bytes</p>
     * <p><i>java.security.SecureRandom</i> is used as the random number generator</p>
     * @param length The length in bytes of the random byte sequence to generate
     * @return The random byte sequence
     */
    public byte[] getRandomBytes(int length) throws NoSuchAlgorithmException {
        return getRandomBytes(length, ALGO_SHA1);
    }

    /**
     * <p>Obtains a sequence of random bytes.</p>
     * <p><i>java.security.SecureRandom</i> is used as the random number generator</p>
     * @param length The length in bytes of the random byte sequence to generate
     * @param algo The algorithm used to hash the random bytes
     * @return The random byte sequence
     */
    public byte[] getRandomBytes(int length, String algo) throws NoSuchAlgorithmException {
        int index = 0;
        byte[] randBytes = new byte[length];

        synchronized(m_sync) {

            while(index < length) {
                BigInteger bi = new BigInteger(64, m_rand);
                byte[] b = createHash(bi.toByteArray(), algo);
                //int i; for(i=0; i<b.length; i++) {System.out.printf("0x%02X ", b[i]); }
                int copy = Math.min(length - index, b.length);
                System.arraycopy(b, 0, randBytes, index, copy);
                index += copy;
            }

        }

        return randBytes;
    }

    /**
     * <p>Creates a base64 encoded key</p>
     * <p><i>getRandomBytes</i> is used to generate they key</p>
     * @param length The length of the key before base64 encoding
     * @return The base64 encoded key
     */
    public String makeKeyBase64(int length) throws NoSuchAlgorithmException {
        return toBase64(getRandomBytes(length));
    }

    /**
     * Base64 encodes a sequence of bytes
     * @param raw The input byte sequence
     * @return The base64 encoded bytes
     */
    public static String toBase64(byte [] raw) {
        return new String(Base64.encodeBase64(raw));
    }

    /**
     * Hex encodes a sequence of bytes
     * @param raw The input byte sequence
     * @return The hex encoded bytes
     */
    public static String toHex(byte [] raw) {
        return new String(Hex.encodeHex(raw));
    }

}

