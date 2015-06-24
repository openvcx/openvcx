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

import com.openvcx.conference.media.*;
import com.openvcx.util.*;

import java.io.BufferedReader;
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.lang.InterruptedException;
import java.nio.charset.Charset;

import org.apache.log4j.Logger;


/**
 *
 * SSL / Datagram Transport Layer Security utility methods for dealing with
 * X.509 certificates
 *
 */
public class DTLSUtil {

    private final Logger m_log = Logger.getLogger(getClass());

    private static final String FINGERPRINT_SCRIPT                           = "bin/certfingerprint.sh";
    private static final String TMP_DIR                                      = "/tmp";
    private static final long MAX_FINGERPRINT_FILE_WAIT_MS                   = 1500;

    public static final String CONFIG_SERVER_SSL_DTLS_FINGERPRINT_SCRIPT     = "ssl.dtls.fingerprint.script";
    public static final String CONFIG_SERVER_SSL_DTLS_CERT                   = "ssl.dtls.certificate";
    public static final String CONFIG_SERVER_SSL_DTLS_KEY                    = "ssl.dtls.key";

    /**
     * <p>Obtains the fingerprint of an X.509 certificate public key used for DTLS</p> 
     * <p>The script defined by the application configuration key <i>ssl.dtls.fingerprint.script</i> is used to calculate the fingerprint value.  The default value of the script is <i>bin/certfingerprint.sh</i></p>
     * @param certificatePath The filepath of the X.509 certificate containing the public key
     * @param config The application configuration instance
     * @param algo The fingerprint algorithm to be used
     */
    public SDP.Fingerprint getCertificateFingerprint(String certificatePath, 
                                                     Configuration config, 
                                                     SDP.Fingerprint.Algorithm algo) 
                           throws Exception {

        String fingerprintScriptPath = config.getString(CONFIG_SERVER_SSL_DTLS_FINGERPRINT_SCRIPT, FINGERPRINT_SCRIPT);
        String tmpDir = config.getString(XCoderMediaProcessor.CONFIG_XCODER_TMP_DIR, TMP_DIR);
        String tmpOut = tmpDir + File.separatorChar + "fingerprint.tmp";

        String [] arrStart = new String[5];

        //
        // launch the bin/fingerprint.sh script which is a wrapper for the openssl command:
        // openssl x509 -noout -in cert.pem -fingerprint -sha256
        //
        arrStart[0] = ProcessControl.SYSTEM_SHELL;
        arrStart[1] = fingerprintScriptPath;
        arrStart[2] = certificatePath;
        arrStart[3] = tmpOut;
        arrStart[4] = algo.toString().replaceAll("_", "");

        FingerprintOutputReader fingerprintReader = new FingerprintOutputReader(algo);
        ProcessControl.startProcess(arrStart, tmpOut, MAX_FINGERPRINT_FILE_WAIT_MS, fingerprintReader);
        return fingerprintReader.getFingerprint();
    }

    /**
     * <p>Obtains the SHA-256 fingerprint of an X.509 certificate public key used for DTLS</p> 
     * <p>The script defined by the application configuration key <i>ssl.dtls.fingerprint.script</i> is used to calculate the fingerprint value.  The default value of the script is <i>bin/certfingerprint.sh</i></p>
     * @param certificatePath The filepath of the X.509 certificate containing the public key
     * @param config The application configuration instance
     */
    public SDP.Fingerprint getCertificateFingerprint(String certificatePath, 
                                                     Configuration config) throws Exception {
        return getCertificateFingerprint(certificatePath, config, SDP.Fingerprint.Algorithm.sha_256);
    }

    /**
     * <p>Obtains the SHA-256 fingerprint of an X.509 certificate public key used for DTLS</p> 
     * <p>The filepath of the X.509 certificate containing the public key is defined by the application configuration key <i>ssl.dtls.certificate</i></p>
     * <p>The script defined by the application configuration key <i>ssl.dtls.fingerprint.script</i> is used to calculate the fingerprint value.  The default value of the script is <i>bin/certfingerprint.sh</i></p>
     * @param config The application configuration instance
     */
    public SDP.Fingerprint getCertificateFingerprint(Configuration config) throws Exception {
        return getCertificateFingerprint(config.getString(CONFIG_SERVER_SSL_DTLS_CERT), 
                                         config, SDP.Fingerprint.Algorithm.sha_256);
    }

    private class FingerprintOutputReader implements IProcessControl {
        private SDP.Fingerprint m_fingerprint;
        private SDP.Fingerprint.Algorithm m_algo;

        public FingerprintOutputReader(SDP.Fingerprint.Algorithm algo) {
            m_algo = algo;
        }

        public SDP.Fingerprint getFingerprint() {
            return m_fingerprint;
        }

        public void onReadProcessOutput(BufferedReader bufferedReader) throws Exception {
            m_fingerprint = new SDP.Fingerprint(m_algo, bufferedReader.readLine());
        }

    }
/*
    public static void main(String [] args) {
        try {
            Configuration config =  new Configuration("../../../test/debug-ngconference.conf");
            DTLSUtil dtlsUtil = new DTLSUtil();
            SDP.Fingerprint fingerprint = dtlsUtil.getCertificateFingerprint(config.getString("ssl.dtls.certificate"), config);
            System.out.println("cert-fingerprint: " + fingerprint);
        } catch(Exception e) {
           System.out.println(e);
        }
    } 
*/

}
