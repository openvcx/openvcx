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

package com.openvcx.conference.media;

import com.openvcx.util.*;

import java.io.IOException;
import java.util.Iterator;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;

import org.apache.log4j.Logger;

/**
 *
 * Class for system resource management
 *
 */
public abstract class XCoderMediaProcessorConfigImpl implements IMediaProcessorConfig {

    private final Logger m_log = Logger.getLogger(getClass());

    private static final String MEDIAPOOL_PREFIX                          = "mediapool";

    static final String CONFIG_SERVER_CONNECT_ADDRESS             = "sdp.connect.location";
    static final String CONFIG_XCODER_PORT_START                  = "mediaprox.control.tcp.start";
    static final String CONFIG_XCODER_PORT_END                    = "mediaprox.control.tcp.end";
    static final String CONFIG_RTP_PORT_START                     = "rtp.port.start";
    static final String CONFIG_RTP_PORT_END                       = "rtp.port.end";
    static final String CONFIG_MEDIA_HOME_DIR                     = "mediaprox.home.dir";
    static final String CONFIG_MEDIA_RECORD_DIR                   = "mediaprox.record.dir";
    static final String CONFIG_MEDIA_TMP_DIR                      = "mediaprox.tmp.dir";
    static final String CONFIG_MEDIA_START_SCRIPT                 = "mediaprox.start.script";
    static final String CONFIG_MEDIA_PIP_SERVER                   = "mediaprox.pip.server.location";
    static final String CONFIG_SERVER_SSL_DTLS_CERT               = "ssl.dtls.certificate";
    static final String CONFIG_SERVER_SSL_DTLS_KEY                = "ssl.dtls.key";

    protected int m_index;
    protected String m_connectAddress;
    protected String m_mediaProcessorHomeDir;
    protected String m_mediaProcessorStartScript;
    protected String m_mediaProcessorTmpDir;
    protected String m_mediaProcessorRecordDir;
    protected String m_mediaPipServer;
    protected String m_dtlsCertPath;
    protected String m_dtlsPrivKeyPath;
    protected int m_rtpStartPort;
    protected int m_rtpEndPort;
    protected int m_controlStartPort;
    protected int m_controlEndPort;

    private int m_usageCount = 0;

    public XCoderMediaProcessorConfigImpl(Configuration config) {
        init(config, -1, null); 
    }

    public XCoderMediaProcessorConfigImpl(Configuration config, int index, IMediaProcessorConfig defaultElement) {
         init(config, index, defaultElement); 
    }

    private void init(Configuration config, int index, IMediaProcessorConfig defaultElement) {
        m_log.debug("element " + index);

        m_index = index;

        if(null == (m_connectAddress = config.getString(createKey(CONFIG_SERVER_CONNECT_ADDRESS, m_index), null)) &&
           null != defaultElement) {
            m_connectAddress = defaultElement.getSDPConnectAddress(); 
        }

        if((m_rtpStartPort = config.getInt(createKey(CONFIG_RTP_PORT_START, m_index), 0)) <= 0 && 
           null != defaultElement) {
            m_rtpStartPort = defaultElement.getRtpStartPort(); 
        }

        if((m_rtpEndPort = config.getInt(createKey(CONFIG_RTP_PORT_END, m_index), 0)) <= 0 && 
           null != defaultElement) {
            m_rtpEndPort = defaultElement.getRtpEndPort(); 
        }

        if((m_controlStartPort = config.getInt(createKey(CONFIG_XCODER_PORT_START, m_index), 0)) <= 0 && 
           null != defaultElement) {
            m_controlStartPort = defaultElement.getControlServerStartPort(); 
        }

        if((m_controlEndPort = config.getInt(createKey(CONFIG_XCODER_PORT_END, m_index), 0)) <= 0 && 
           null != defaultElement) {
            m_controlEndPort = defaultElement.getControlServerEndPort(); 
        }

        if(null == (m_mediaProcessorHomeDir = config.getString(createKey(CONFIG_MEDIA_HOME_DIR, m_index), null)) && 
           null != defaultElement) {
            m_mediaProcessorHomeDir = defaultElement.getHomeDir(); 
        }

        if(null == (m_mediaProcessorStartScript = config.getString(createKey(CONFIG_MEDIA_START_SCRIPT, m_index), 
           null)) && null != defaultElement) {
            m_mediaProcessorStartScript = defaultElement.getStartScript(); 
        }

        if(null == (m_mediaProcessorRecordDir = config.getString(createKey(CONFIG_MEDIA_RECORD_DIR, m_index), 
           null)) && null != defaultElement) {
            m_mediaProcessorRecordDir = defaultElement.getRecordDir(); 
        }

        if(null == (m_mediaProcessorTmpDir = config.getString(createKey(CONFIG_MEDIA_TMP_DIR, m_index), null)) && 
           null != defaultElement) {
            m_mediaProcessorTmpDir = defaultElement.getTmpDir(); 
        }

        if(null == (m_mediaPipServer = config.getString(createKey(CONFIG_MEDIA_PIP_SERVER, m_index), null)) && 
           null != defaultElement) {
            m_mediaPipServer = defaultElement.getControlServerAddress(); 
        }

        if(null == (m_dtlsCertPath = config.getString(createKey(CONFIG_SERVER_SSL_DTLS_CERT, m_index), null)) && 
           null != defaultElement) {
            m_dtlsCertPath = defaultElement.getDtlsCertPath(); 
        }

        if(null == (m_dtlsPrivKeyPath = config.getString(createKey(CONFIG_SERVER_SSL_DTLS_KEY, m_index), null)) && 
           null != defaultElement) {
            m_dtlsPrivKeyPath = defaultElement.getDtlsPrivKeyPath(); 
        }

    }

    public String toString() {
        final String NEWLINE = "\r\n";
        StringBuffer str = new StringBuffer();

        str.append(createKey(CONFIG_SERVER_CONNECT_ADDRESS, m_index) + ": " + m_connectAddress + NEWLINE);
        str.append(createKey(CONFIG_RTP_PORT_START, m_index) + ": " + m_rtpStartPort + ", ");
        str.append(createKey(CONFIG_RTP_PORT_END, m_index) + ": " + m_rtpEndPort + NEWLINE);
        str.append(createKey(CONFIG_XCODER_PORT_START, m_index) + ": " + m_controlStartPort + ", ");
        str.append(createKey(CONFIG_XCODER_PORT_END, m_index) + ": " + m_controlEndPort + NEWLINE);
        str.append(createKey(CONFIG_MEDIA_HOME_DIR, m_index) + ": " + m_mediaProcessorHomeDir + NEWLINE);
        str.append(createKey(CONFIG_MEDIA_RECORD_DIR, m_index) + ": " + m_mediaProcessorRecordDir + NEWLINE);
        str.append(createKey(CONFIG_MEDIA_TMP_DIR, m_index) + ": " + m_mediaProcessorTmpDir + NEWLINE);
        str.append(createKey(CONFIG_MEDIA_START_SCRIPT, m_index) + ": " + m_mediaProcessorStartScript + NEWLINE);
        str.append(createKey(CONFIG_MEDIA_PIP_SERVER, m_index) + ": " + m_mediaPipServer + NEWLINE);
        str.append(createKey(CONFIG_SERVER_SSL_DTLS_CERT, m_index) + ": " + m_dtlsCertPath + NEWLINE);
        str.append(createKey(CONFIG_SERVER_SSL_DTLS_KEY, m_index) + ": " + m_dtlsPrivKeyPath + NEWLINE);
        
        return str.toString();
    }

    public static String createKey(String suffix, int index) {
        String key = null;
        if(index >= 0) {
            key = MEDIAPOOL_PREFIX + "." + index + "." + suffix;
        } else {
            key = suffix;
        }
        return key;
    }

    public int getUsageCount() {
        return m_usageCount;
    }

    public void incrementUsageCount() {
        m_usageCount++;
    }

    public int getIndex() {
        return m_index;
    }

    public final String getSDPConnectAddress() {
        return m_connectAddress;
    }

    public final String getDtlsCertPath() {
        return m_dtlsCertPath;
    }

    public final String getDtlsPrivKeyPath() {
        return m_dtlsPrivKeyPath;
    }

    public int getRtpStartPort() {
        return m_rtpStartPort;
    }

    public int getRtpEndPort() {
        return m_rtpEndPort;
    }

    public int getControlServerStartPort() {
        return m_controlStartPort;
    }

    public int getControlServerEndPort() {
        return m_controlEndPort;
    }

    public final String getHomeDir() {
        return m_mediaProcessorHomeDir;
    }

    public final String getStartScript() {
        return m_mediaProcessorStartScript;
    }

    public final String getTmpDir() {
        return m_mediaProcessorTmpDir;
    }

    public final String getRecordDir() {
        return m_mediaProcessorRecordDir;
    }

    public final String getControlServerAddress() {
         return m_mediaPipServer;
    }
/*
    public static void main(String [] args) {

        try {
            Configuration config =  new Configuration("../../../test/debug-ngconference.conf"); 
            MediaResourcePool pool = new MediaResourcePool(config);
            pool.test();

        } catch(Exception e) {
            e.printStackTrace();
        }
    }
*/
};
