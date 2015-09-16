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

import com.openvcx.conference.*;
import com.openvcx.util.*;

import java.io.BufferedReader;
import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStreamReader;
import java.net.MalformedURLException;
import java.net.ConnectException;
import java.net.SocketException;
import java.net.URLConnection;
import java.net.URL;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.text.SimpleDateFormat;
import java.util.ArrayList;
import java.util.Calendar;
import java.util.Iterator;
import java.util.List;

import org.apache.log4j.Logger;

/**
 *
 * <p>Media processor implementation for the OpenVSX (Video Streaming Server) media back-end processor</p>
 * @see <a target="_new" href="http://openvcx.com">http://openvcx.com</a>
 *
 */
public class XCoderMediaProcessor implements IMediaProcessor, ISDPNaming {

    private final Logger m_log = Logger.getLogger(getClass());
    private static final int PIP_READ_TIMEOUT_MS         = 3000;
    private static final int PIP_CONNECT_TIMEOUT_MS      = 3000;
    private static final String SDP_TMP_DIR              = "/tmp";
    private static final String START_SCRIPT             = "bin/startconf.sh";
    private static final String PIP_START_URL            = "/pip?pipstart=1";
    private static final String PIP_UPDATE_URL           = "/pip?pipupdate=";
    private static final String PIP_STOP_URL             = "/pip?pipstop=";
    private static final String PIP_STATUS_URL           = "/pip?pipstatus=";
    private static final String PIP_TURNRELAY_URL        = "/status?turn=1&pipid=";
    private static final String CONFIG_URL               = "/config?";
    private static String PIP_SERVER                     = "http://127.0.0.1";

    private String m_pipServer;
    private String m_xcoderStartScriptPath;
    private String m_xcoderHomeDir;
    private String m_xcoderTmpDir;         // Relative to our java process
    private String m_xcoderTmpDirUrlPath;  // Relative to (remote) VSX process
    private String m_xcoderRecordingDir;         // Relative to our java process
    private String m_xcoderRecordingDirUrlPath;  // Relative to (remote) VSX process
    private String m_mediaChimeDirUrlPath;    // Relative to (remote) VSX process
    private String m_strConnectAddress;
    private String m_dtlsCertificatePath;
    private String m_dtlsCertificateUrlPath;
    private String m_dtlsKeyPath;
    private String m_dtlsKeyUrlPath;
    private int m_mtu;
    private static final int PIP_COMMAND_RETRIES_CONFSTART = 7;
    private static final int PIP_COMMAND_RETRIES           = 3;
    private static final int CONFIG_COMMAND_RETRIES        = PIP_COMMAND_RETRIES;
    private static final int PARTICIPANT_COMMAND_RETRIES   = 0;

    /**
     * Application configuration key <i>mediaprox.home.dir</i>
     */
    //private static final String CONFIG_XCODER_HOME_DIR        = "mediaprox.home.dir";
    private static final String CONFIG_XCODER_HOME_DIR        = XCoderMediaProcessorConfigImpl.CONFIG_MEDIA_HOME_DIR;


    /**
     * Application configuration key <i>mediaprox.record.dir</i>
     */
    //private static final String CONFIG_XCODER_RECORD_DIR      = "mediaprox.record.dir";
    private static final String CONFIG_XCODER_RECORD_DIR      = XCoderMediaProcessorConfigImpl.CONFIG_MEDIA_RECORD_DIR;

    /**
     * Application configuration key <i>mediaprox.start.script</i>
     */
    //private static final String CONFIG_XCODER_START_SCRIPT    = "mediaprox.start.script";
    private static final String CONFIG_XCODER_START_SCRIPT    = XCoderMediaProcessorConfigImpl.CONFIG_MEDIA_START_SCRIPT;

    /**
     * Application configuration key <i>mediaprox.pip.server.location</i>
     */
    //private static final String CONFIG_PIP_SERVER           = "mediaprox.pip.server.location";
    private static final String CONFIG_PIP_SERVER           = XCoderMediaProcessorConfigImpl.CONFIG_MEDIA_PIP_SERVER;

    /**
     * Application configuration key <i>media.chime.dir</i>
     */
    private static final String CONFIG_MEDIA_CHIME_DIR      = "media.chime.dir";

    /**
     * Application configuration key <i>rtp.mtu</i>
     */
    private static final String CONFIG_RTP_MTU              = "rtp.mtu";

    /**
     * Application configuration key <i>mediaprox.tmp.dir</i>
     */
    //public static final String CONFIG_XCODER_TMP_DIR         = "mediaprox.tmp.dir";
    public static final String CONFIG_XCODER_TMP_DIR         = XCoderMediaProcessorConfigImpl.CONFIG_MEDIA_TMP_DIR;

    private enum RTPTransportType {
        rtp,
        srtp,
        dtlssrtp
    };

    /**
     * The constructor usd to initialize this instance
     * @param config The configuration store used for initialization
     */
    public XCoderMediaProcessor(Configuration config) throws Exception {

        m_xcoderHomeDir = config.getString(CONFIG_XCODER_HOME_DIR);
        m_xcoderStartScriptPath = config.getString(CONFIG_XCODER_START_SCRIPT, START_SCRIPT);
        File file = new File(m_xcoderStartScriptPath); 
        if(false == file.canRead()) {
            throw new FileNotFoundException("File: " + m_xcoderStartScriptPath + " does not exist.");
        }

        m_strConnectAddress = InetAddress.getByName(config.getString(
                                                    SDPResponder.CONFIG_SERVER_CONNECT_ADDRESS)).getHostAddress();

        m_xcoderTmpDir = config.getString(CONFIG_XCODER_TMP_DIR, SDP_TMP_DIR);
        m_xcoderTmpDirUrlPath = getSubDirOfHomeDir(m_xcoderTmpDir);

        m_xcoderRecordingDir = config.getString(CONFIG_XCODER_RECORD_DIR, SDP_TMP_DIR);
        m_xcoderRecordingDirUrlPath = getSubDirOfHomeDir(m_xcoderRecordingDir);

        m_mediaChimeDirUrlPath = config.getString(CONFIG_MEDIA_CHIME_DIR, null);

        m_dtlsCertificatePath = config.getString(DTLSUtil.CONFIG_SERVER_SSL_DTLS_CERT, null);
        m_dtlsCertificateUrlPath = getSubDirOfHomeDir(m_dtlsCertificatePath);
        m_dtlsKeyPath = config.getString(DTLSUtil.CONFIG_SERVER_SSL_DTLS_KEY, null);
        m_dtlsKeyUrlPath = getSubDirOfHomeDir(m_dtlsKeyPath);

        m_log.debug(CONFIG_XCODER_START_SCRIPT + ": " + m_xcoderStartScriptPath);
        m_log.debug(CONFIG_XCODER_TMP_DIR + ": " + m_xcoderTmpDir);
        m_log.debug(CONFIG_XCODER_TMP_DIR + " (relative to VSX for PIP start): " + m_xcoderTmpDirUrlPath);
        m_log.debug(CONFIG_XCODER_RECORD_DIR + ": " + m_xcoderRecordingDir);
        m_log.debug(CONFIG_XCODER_RECORD_DIR + " (relative to VSX for PIP start): " + m_xcoderRecordingDirUrlPath);
        m_log.debug(CONFIG_MEDIA_CHIME_DIR+ ": " + m_mediaChimeDirUrlPath);

        if(null != m_dtlsCertificatePath) {
            m_log.debug(DTLSUtil.CONFIG_SERVER_SSL_DTLS_CERT + ": " + m_dtlsCertificatePath);
            m_log.debug(DTLSUtil.CONFIG_SERVER_SSL_DTLS_CERT + " (relative to VSX):" + m_dtlsCertificateUrlPath);
        }
        if(null != m_dtlsKeyPath) {
            m_log.debug(DTLSUtil.CONFIG_SERVER_SSL_DTLS_KEY + ": " + m_dtlsKeyPath);
            m_log.debug(DTLSUtil.CONFIG_SERVER_SSL_DTLS_KEY + " (relative to VSX): " + m_dtlsKeyUrlPath);
        }
      
        if((m_mtu = config.getInt(CONFIG_RTP_MTU, 0)) > 0) {
            m_log.debug(CONFIG_RTP_MTU + ": " + m_mtu);
        }

        m_pipServer = config.getString(CONFIG_PIP_SERVER, PIP_SERVER);
        
        m_pipServer = "http://" + m_pipServer;

    }

    private String getSubDirOfHomeDir(String strIn) {
       String strOut;

        if(null != strIn && 
           strIn.charAt(0) != File.separatorChar && strIn.length() >= m_xcoderHomeDir.length() &&
           strIn.substring(0, m_xcoderHomeDir.length()).equalsIgnoreCase(m_xcoderHomeDir)) {

            strOut = strIn.substring(m_xcoderHomeDir.length());
            while(strOut.charAt(0) == File.separatorChar) {
                strOut = strOut.substring(1);
            }

        } else {
            strOut = strIn;
        }

        return strOut;
    }

    @Override
    public String getSDPCodecName(String xcoderCodecName) {

        if("h263p".equalsIgnoreCase(xcoderCodecName) || "h263+".equalsIgnoreCase(xcoderCodecName)) {
            return "H263-1998";
        } else if("h263".equalsIgnoreCase(xcoderCodecName)) {
            return "H263";
        } else if("mpg4v".equalsIgnoreCase(xcoderCodecName)) {
            return "MP4V-ES";
        } else if("g711a".equalsIgnoreCase(xcoderCodecName)) {
            return "PCMA";
        } else if("g711u".equalsIgnoreCase(xcoderCodecName)) {
            return "PCMU";
        } else if("aac".equalsIgnoreCase(xcoderCodecName)) {
            return "MPEG4-GENERIC";
        } else {
            return xcoderCodecName;
        }
        
    }

    @Override
    public String getImplCodecName(String sdpCodecName) {
        return getXcoderCodecName(sdpCodecName);
    }

    private String getXcoderCodecName(String sdpCodecName) {
        if("SILK".equalsIgnoreCase(sdpCodecName)) {
            return "silk";
        } else if("OPUS".equalsIgnoreCase(sdpCodecName)) {
            return "opus";
        } else if("PCMA".equalsIgnoreCase(sdpCodecName)) {
            return "g711a";
        } else if("PCMU".equalsIgnoreCase(sdpCodecName)) {
            return "g711u";
        } else if("MPEG4-GENERIC".equalsIgnoreCase(sdpCodecName)) {
            return "aac";
        } else if("H264".equalsIgnoreCase(sdpCodecName)) {
            return "h264";
        } else if("VP8".equalsIgnoreCase(sdpCodecName)) {
            return "vp8";
        } else if("MP4V-ES".equalsIgnoreCase(sdpCodecName)) {
            return "mpg4v";
        } else if("H263".equalsIgnoreCase(sdpCodecName)) {
            return "h263";
        } else if("H263-1998".equalsIgnoreCase(sdpCodecName)) {
            return "h263p";
        } else { 
            return null;
        }
    }

    private SDP createListenerSDP(SDP sdpLocal, SDP sdpRemote) 
                    throws UnknownHostException, CloneNotSupportedException {
        SDP sdpListener = new SDP();
        boolean bIsRtcpMux;
        boolean bIsAVPF;

        //
        // Create the SDP for the VSX  listener PIP
        //

        SDP.MediaCandidate candidate;

        if(null != (candidate = sdpLocal.getAudioCandidate())) {

            String sdpConnectIpAddress = SDP.STRINADDR_ANY_IPV4;

            if(null != sdpRemote) {
                bIsRtcpMux = sdpRemote.getAudioMedia().isRtcpMux();
                bIsAVPF = sdpRemote.getAudioMedia().isMediaAVPF();

                if(null != sdpRemote.getAudioMedia() && sdpRemote.getAudioMedia().getTarget().isIPv6()) {
                    sdpConnectIpAddress = SDP.STRINADDR_ANY_IPV6;
                }

            } else {
                bIsRtcpMux = sdpLocal.getAudioMedia().isRtcpMux();
                bIsAVPF = sdpLocal.getAudioMedia().isMediaAVPF();
            }


            Codec codec = candidate.getCodec().clone();
            codec.setFmtp(null);
            //codec.setChannelCount(1); // Assume always mono, even w/ Opus
            sdpListener.setAudioPort(sdpLocal.getAudioMedia().getTarget().getPort());
            sdpListener.setAudioAddress(sdpConnectIpAddress);
            sdpListener.addAudioCandidate(codec);

            //
            // Add any RFC2833 DTMF telepone-event
            //
            final Codec dtmfCodec;
            if(null != (dtmfCodec = sdpLocal.getAudioMedia().findCodec(Codec.TELEPHONE_EVENT))) {
                sdpListener.getAudioMedia().addCandidate(dtmfCodec);
            }

            sdpListener.getAudioMedia().setRtcpMux(bIsRtcpMux);
            sdpListener.getAudioMedia().setTransportType(SDP.createTransportType(false, bIsAVPF));

            // 
            // If the local response is SRTP, assume it is because the remote has advertised
            // crypto keys or DTLS certificate fingerprint and will be sending SRTP
            //
            if(sdpLocal.getAudioMedia().isMediaTransportSecure()) {

                 sdpListener.getAudioMedia().setTransportType(SDP.createTransportType(true, bIsAVPF));

                 if(sdpLocal.getAudioMedia().getFingerprint().isSet()) {
                     sdpListener.getAudioMedia().getFingerprint().set(sdpLocal.getAudioMedia().getFingerprint().getPeer());
                 } else {
                     sdpListener.getAudioMedia().addCrypto(sdpLocal.getAudioMedia().getCrypto().getPeer(), true);
                 }
            }

            //
            // Set any local ICE username/password for STUN
            //
            sdpListener.getAudioMedia().setICEUfrag(sdpLocal.getAudioMedia().getICEUfrag());
            sdpListener.getAudioMedia().setICEPwd(sdpLocal.getAudioMedia().getICEPwd());
        }

        if(null != (candidate = sdpLocal.getVideoCandidate())) {

            String sdpConnectIpAddress = SDP.STRINADDR_ANY_IPV4;

            if(null != sdpRemote) {
                bIsRtcpMux = sdpRemote.getVideoMedia().isRtcpMux();
                bIsAVPF = sdpRemote.getVideoMedia().isMediaAVPF();

                if(null != sdpRemote.getAudioMedia() && sdpRemote.getAudioMedia().getTarget().isIPv6()) {
                    sdpConnectIpAddress = SDP.STRINADDR_ANY_IPV6;
                }

            } else {
                bIsRtcpMux = sdpLocal.getVideoMedia().isRtcpMux();
                bIsAVPF = sdpLocal.getVideoMedia().isMediaAVPF();
            }

            Codec codec = candidate.getCodec().clone();
            codec.setFmtp(null);
            sdpListener.setVideoPort(sdpLocal.getVideoMedia().getTarget().getPort());

            sdpListener.setVideoAddress(sdpConnectIpAddress);
            sdpListener.addVideoCandidate(codec);
            sdpListener.getVideoMedia().setRtcpMux(bIsRtcpMux);
            sdpListener.getVideoMedia().setTransportType(SDP.createTransportType(false, bIsAVPF));

            if(sdpLocal.getVideoMedia().isMediaTransportSecure()) {
                 sdpListener.getVideoMedia().setTransportType(SDP.createTransportType(true, bIsAVPF));

                 if(sdpLocal.getVideoMedia().getFingerprint().isSet()) {
                     sdpListener.getVideoMedia().getFingerprint().set(sdpLocal.getVideoMedia().getFingerprint().getPeer());
                 } else {
                     sdpListener.getVideoMedia().addCrypto(sdpLocal.getVideoMedia().getCrypto().getPeer(), true);
                 }
            }

            //
            // Set any local ICE username/password for STUN
            //
            sdpListener.getVideoMedia().setICEUfrag(sdpLocal.getVideoMedia().getICEUfrag());
            sdpListener.getVideoMedia().setICEPwd(sdpLocal.getVideoMedia().getICEPwd());

            //
            // Set any RTCP FB options
            //
            Iterator<SDP.RTCPFB> iterRTCPFb = sdpLocal.getVideoMedia().getRTCPFBIterator();
            while(iterRTCPFb.hasNext()) {
                SDP.RTCPFB rtcpFb = iterRTCPFb.next();
                if(rtcpFb.getType() == SDP.RTCPFB.TYPE.CCM_FIR ||
                   rtcpFb.getType() == SDP.RTCPFB.TYPE.NACK_GENERIC ||
                   rtcpFb.getType() == SDP.RTCPFB.TYPE.GOOG_REMB) {
                    sdpListener.getVideoMedia().addRTCPFB(new SDP.RTCPFB(rtcpFb.getType(), codec.getPayloadType()));
                }
            }

        }

        return sdpListener;
    }

    private static StringBuffer getRTCPPorts(SDP sdpRemote) throws IOException {
        StringBuffer strPorts = null; 
        SDP.Media audio = sdpRemote.getAudioMedia();
        SDP.Media video = sdpRemote.getVideoMedia();

        if((null != audio && audio.getTarget().getPort() + 1 != audio.getTargetRTCP().getPort()) ||
           (null != video && video.getTarget().getPort() + 1 != video.getTargetRTCP().getPort())) {

           strPorts = new StringBuffer();

            if(null != video) {
                strPorts.append(Integer.valueOf(video.getTargetRTCP().getPort()).toString());
            }

            if(null != audio) {
                if(null != strPorts) {
                    strPorts.append(",");
                } 
                strPorts.append(Integer.valueOf(audio.getTargetRTCP().getPort()).toString());
            }

        }

        return strPorts;
    }


    private static String getSRTPSDESKeys(SDP sdpLocal) throws IOException {
        String strKeys = null;
        SDP.Media audio = sdpLocal.getAudioMedia();
        SDP.Media video = sdpLocal.getVideoMedia();

        if(null != video && video.isMediaTransportSecure() && false == video.getFingerprint().isSet()) {
            strKeys = video.getCrypto().getKey();
        }

        if(null != audio && audio.isMediaTransportSecure() && false == audio.getFingerprint().isSet()) {
            String key = audio.getCrypto().getKey();
            if(null != strKeys) {
                strKeys += "," + key;
            } else {
                strKeys = key;
            }
             
        }

        return strKeys;
    }

    private static RTPTransportType localSenderMediaTransportType(SDP.Media mediaRemote, SDP.Media mediaLocal) {

        if(null != mediaRemote && null != mediaLocal && (mediaRemote.getCryptoCount() > 0 || mediaRemote.getFingerprint().isSet())) {

            if(mediaLocal.getFingerprint().isSet() && null != mediaLocal.getFingerprint().getPeer() && 
               mediaLocal.getFingerprint().getPeer().isSet()) {

                return RTPTransportType.dtlssrtp;

            } else if(null != mediaLocal.getCrypto() && null != mediaLocal.getCrypto().getPeer()) {

                return RTPTransportType.srtp;

            }
        }
        return RTPTransportType.rtp;
    }

    private static RTPTransportType localSenderTransportType(SDP sdpRemote, SDP sdpLocal) throws IOException {

        RTPTransportType audioTransportType = RTPTransportType.rtp;
        RTPTransportType videoTransportType = RTPTransportType.rtp;

        if(null == sdpRemote.getAudioMedia() && null == sdpRemote.getVideoMedia()) {
            return RTPTransportType.rtp;
        }

        if(null != sdpRemote.getAudioMedia()) {
           audioTransportType = localSenderMediaTransportType(sdpRemote.getAudioMedia(), sdpLocal.getAudioMedia());
        }

        if(null != sdpRemote.getVideoMedia()) {
           videoTransportType = localSenderMediaTransportType(sdpRemote.getVideoMedia(), sdpLocal.getVideoMedia());
        }

        if(sdpLocal.getVideoCandidatesCount() == 0) {
            return audioTransportType;
        } else if(sdpLocal.getAudioCandidatesCount() == 0) {
            return videoTransportType;
        } else if(audioTransportType != videoTransportType) {
                throw new IOException ("Cannot mix both secure and non-secure outgoing transport types");
        } else {
            return audioTransportType;
        }

    }

    private static StringBuffer getRemoteDestination(SDP sdpRemote, RTPTransportType rtpTransportType) {
        StringBuffer strDestination = new StringBuffer();
        SDP.Media audio = sdpRemote.getAudioMedia();
        SDP.Media video = sdpRemote.getVideoMedia();
        String strIp = null;

        if(null != audio) {
            strIp = audio.getTarget().getAddress().getHostAddress();
        } else {
            strIp = video.getTarget().getAddress().getHostAddress();
        }

        strDestination.append(rtpTransportType.toString()).append("://").append(strIp);

        if(null != video && video.getTarget().getPort() > 0) {
            strDestination.append(":").append(video.getTarget().getPort());
        }

        if(null != audio) {
            if(null != video && video.getTarget().getPort() > 0) {
                strDestination.append(",");
            } else {
                strDestination.append(":"); 
            }
            strDestination.append(audio.getTarget().getPort());
        }
        return strDestination;
    }

    private static String getRemotePayloadTypes(SDP sdpLocal) {
        String strPayloadTypes = null;
        SDP.MediaCandidate candidate;

        if(null != (candidate = sdpLocal.getVideoCandidate())) {
            strPayloadTypes = new Integer(candidate.getCodec().getPayloadType()).toString();
        }

        if(null != (candidate = sdpLocal.getAudioCandidate())) {
            if(null != strPayloadTypes) {
                strPayloadTypes += "," + new Integer(candidate.getCodec().getPayloadType()).toString();
            } else {
                strPayloadTypes = new Integer(candidate.getCodec().getPayloadType()).toString();
            }
        }
        return strPayloadTypes;
    }

    private static String getRemoteSSRCs(SDP sdpLocal) {
        String strSSRCs = null;
        long ssrc = 0;

        if(sdpLocal.getVideoCandidatesCount() > 0 && (ssrc = sdpLocal.getVideoMedia().getSSRC()) > 0) {
            strSSRCs = new Long(ssrc).toString();
        }

        if(sdpLocal.getAudioCandidatesCount() > 0 && (ssrc = sdpLocal.getAudioMedia().getSSRC()) > 0) {
            if(null != strSSRCs ) {
                strSSRCs += "," + new Long(ssrc).toString();
            } else {
                strSSRCs = new Long(ssrc).toString();
            }
        }
        return strSSRCs;
    }

    private static void writeSdpPath(String sdpText, String path) throws FileNotFoundException, IOException {
        FileOutputStream sdpFile;

        sdpFile = new FileOutputStream(path);
        sdpFile.write(sdpText.getBytes());
        sdpFile.flush();
        sdpFile.close();

    }

    private static void removeSdpPath(String path) {

        if(".sdp".equalsIgnoreCase(path.substring(path.length() -4))) {
            File file = new File(path);
            file.delete();
        }

    }

    private String getConfigSendonlyUrl(String id, int port, boolean bHold) {
        StringBuffer urlString = null;

        urlString = new StringBuffer(m_pipServer).append(":").append(port).append(CONFIG_URL);
        urlString.append("sendonly=").append(bHold ? "1" : "0");
        urlString.append("&pipid=").append(id);

        return urlString.toString();
    }

    private String getConfigNewOutputUrl(String id, int port, String strRemoteDestination) {
        StringBuffer urlString = null;

        urlString = new StringBuffer(m_pipServer).append(":").append(port).append(CONFIG_URL);
        urlString.append("out=").append(strRemoteDestination);
        urlString.append("&pipid=").append(id);

        return urlString.toString();
    }

    private String getSTUNCombinedUsername(SDP.Media mediaRemote, SDP.Media mediaLocal) {
        String stunUsername = null;

        if(null != mediaRemote && null != mediaRemote.getICEUfrag()) {
            stunUsername = mediaRemote.getICEUfrag();
        }

        if(null != mediaLocal && null != mediaLocal.getICEUfrag()) {
            if(null == stunUsername) {
                stunUsername = mediaLocal.getICEUfrag();
            } else {
                stunUsername += ":" + mediaLocal.getICEUfrag();
            }
        }
        return stunUsername;
    }

    private StringBuffer getRTCPParameters(SDP sdpRemote, SDP sdpLocal) {
        boolean bDoRtpRetransmit = false;
        StringBuffer urlString = new StringBuffer();

        if(null != sdpRemote && null != sdpRemote.getVideoMedia()) {

            //
            // Set any RTCP FB options
            //
            Iterator<SDP.RTCPFB> iterRTCPFb = sdpRemote.getVideoMedia().getRTCPFBIterator();

            //
            // Some business logic here if the remote endpoint is firefox, which sends RTCP RR/FB-NACKs
            // but fails to indicate it in the SDP
            //
            while(iterRTCPFb.hasNext()) {
                SDP.RTCPFB rtcpFb = iterRTCPFb.next();
                if(rtcpFb.getType() == SDP.RTCPFB.TYPE.NACK_GENERIC) {
                    bDoRtpRetransmit = true;
                    break;
                }
            }

            if(false == bDoRtpRetransmit && 
               DeviceTypeWebRTC.Type.FIREFOX == DeviceTypeWebRTC.createType(sdpRemote.getDeviceType().getType())) { 
                m_log.debug("Setting rtpretransmit=1 for device type: " + sdpRemote.getDeviceType() + 
                            " even though remote SDP does not contain " + 
                            new SDP.RTCPFB(SDP.RTCPFB.TYPE.NACK_GENERIC).toString());
                bDoRtpRetransmit = true;
            }

        }

        if(bDoRtpRetransmit) {
            urlString.append("&rtpretransmit=1");
        }

        return urlString;
    }

    private static int getStunPolicyAsInt(User.STUNPolicy stunPolicy) {

        switch(stunPolicy) {
            case ENABLED_ALWAYS:
                return 1;
            case ENABLED_IF_PEER:
                return 2;
            case RESPOND_ONLY: // Testing
                return 3;
            default:
                return 0;
         }

    }

    private StringBuffer getTURNParameters(ConferenceDefinition conferenceDef, TURNRelay turnRelay) {
        StringBuffer urlString = new StringBuffer();

        if(null != conferenceDef.getTurnServer()) {

            urlString.append("&turnserver=" + conferenceDef.getTurnServer());        

            if(null != conferenceDef.getTurnRealm()) {
                urlString.append("&turnrealm=" + conferenceDef.getTurnRealm());        
            }

            if(null != conferenceDef.getTurnUser()) {
                urlString.append("&turnuser=" + conferenceDef.getTurnUser());        
            }

            if(null != conferenceDef.getTurnPassword()) {
              urlString.append("&turnpass=" + conferenceDef.getTurnPassword());        
            }

            if(null != turnRelay && null != turnRelay.getPeerPermission()) {
                urlString.append("&turnpeer=" + turnRelay.getPeerPermission().toString());
            }

        }

        return urlString;
    }

    private StringBuffer getSTUNParameters(SDP sdpRemote, SDP sdpLocal, IUser user) {
        StringBuffer urlString = new StringBuffer();
        SDP.Media media;

        if(user.getStunPolicy() != User.STUNPolicy.NONE) {

            urlString.append("&stunrespond=1");

            if(null != sdpRemote) {

                urlString.append("&stunrequest=" + getStunPolicyAsInt(user.getStunPolicy()));

                String stunVideoUsername = null;
                String stunAudioUsername = null;
                if(null != sdpLocal.getVideoMedia() && null != sdpRemote.getVideoMedia()) {
                    stunVideoUsername = getSTUNCombinedUsername(sdpRemote.getVideoMedia(), 
                                                                sdpLocal.getVideoMedia());
                }
                if(null != sdpLocal.getAudioMedia() && null != sdpRemote.getAudioMedia()) {
                    stunAudioUsername = getSTUNCombinedUsername(sdpRemote.getAudioMedia(), 
                                                                sdpLocal.getAudioMedia());
                }

                if(null != stunVideoUsername || null != stunAudioUsername) {
                    urlString.append("&stunrequestuser=");
                    if(null != stunVideoUsername) {
                        urlString.append(stunVideoUsername);
                    }
                    if(null != stunAudioUsername) {
                        if(null != stunVideoUsername) {
                            urlString.append(",");
                        }
                        urlString.append(stunAudioUsername);
                    }
                }

   	        String stunVideoPwd = null;
    	        String stunAudioPwd = null;
                if(null != sdpLocal.getVideoMedia() && null != (media = sdpRemote.getVideoMedia())) {
                    stunVideoPwd =  media.getICEPwd();
                }
                if(null != sdpLocal.getAudioMedia() && null != (media = sdpRemote.getAudioMedia())) {
                    stunAudioPwd =  media.getICEPwd();
                }

                if(null != stunVideoPwd || null != stunAudioPwd) {
                    urlString.append("&stunrequestpass=");
                    if(null != stunVideoPwd) {
                        urlString.append(stunVideoPwd);
                    }
                    if(null != stunAudioPwd) {
                        if(null != stunVideoPwd) { 
                            urlString.append(",");
                        }
                        urlString.append(stunAudioPwd);
                    }
                }

            }
 
        }

        return urlString;
    }

    private String getPipStartUrl(SDP sdpLocal, SDP sdpRemote, String sdpLocalPath, 
                                  Conference conference, IUser user, String idUpdate, TURNRelay turnRelay) 
                                      throws IOException {
        SDP.MediaCandidate candidate;
        boolean bHaveXcodeConfig = false;
        StringBuffer urlString = null;
        String strRemotePayloadTypes = getRemotePayloadTypes(sdpLocal);
        String strRemoteSSRCs = getRemoteSSRCs(sdpLocal);
        int port = conference.getPort();
        ConferenceDefinition conferenceDef = conference.getConferenceDefinition();
        IEncoderConfig encoderConfig = null;
        int mtu = 0;
        int sliceSizeMax = 0;
        String srtpKeys = null;
        StringBuffer rtcpPorts = null;
        String videoXcodeConfig = null;
        String joinChime = null;
        String leaveChime = null;
        boolean bDelayStart = false;
        DeviceType remoteDeviceType = null;
        RTPTransportType rtpTransportType = RTPTransportType.rtp;

        urlString = new StringBuffer(m_pipServer).append(":").append(port);
        if(null == idUpdate || idUpdate.length() <= 0) { 
            urlString.append(PIP_START_URL);
        } else {
            urlString.append(PIP_UPDATE_URL).append(idUpdate);
        }

        urlString.append("&in=").append(sdpLocalPath);

        if(null != sdpRemote) {
            rtpTransportType = localSenderTransportType(sdpRemote, sdpLocal);
            urlString.append("&out=").append(getRemoteDestination(sdpRemote, rtpTransportType)); 
            remoteDeviceType = sdpRemote.getDeviceType();
        } else {
            //
            // If sdpRemote is set assume we are only interested in setting up an RTP / STUN listener
            // and doing a delayed start of the RTP sender
            //
            bDelayStart = true;
            urlString.append("&out=rtp://127.0.0.1:10000,10002&delaystart=1");
        }

        if(RTPTransportType.dtlssrtp == rtpTransportType) {
            urlString.append("&dtlsclient=").append("1");
        }

        if(null != strRemotePayloadTypes) {
            urlString.append("&rtppayloadtype=").append(strRemotePayloadTypes);
        }
        if(null != strRemoteSSRCs) {
            urlString.append("&rtpssrc=").append(strRemoteSSRCs);
        }

        if(null != (srtpKeys = getSRTPSDESKeys(sdpLocal))) {
            urlString.append("&srtpkey=").append(srtpKeys);
        }
        if(null != sdpRemote && null != (rtcpPorts = getRTCPPorts(sdpRemote))) {
            urlString.append("&rtcpports=").append(rtcpPorts);
        }

        if(user.getStunPolicy() != User.STUNPolicy.NONE) {
            urlString.append(getSTUNParameters(sdpRemote, sdpLocal, user));
        }

        if(null != turnRelay) {
            urlString.append(getTURNParameters(conferenceDef, turnRelay));
        }

        if(null != sdpRemote) {
            urlString.append(getRTCPParameters(sdpRemote, sdpLocal));
        }

        if(conferenceDef.isABRAuto()) {
            urlString.append("&abrauto=1");
        }

        if(conferenceDef.isABRSelf()) {
            urlString.append("&abrself=1");
        }

        if(null != conferenceDef) {

            mtu = conferenceDef.getMTU();

            if(null != sdpLocal.getVideoCandidate() &&
               null != (encoderConfig = conferenceDef.getEncoderConfigLoader().getEncoderConfig(
                                                                 sdpLocal.getVideoCandidate().getCodec(), 
                                                                 remoteDeviceType,
                                                                 this))) {
                videoXcodeConfig = encoderConfig.getInitializerString();
                sliceSizeMax = encoderConfig.getVideoSliceSizeMax();
            }

            //
            // If we're setting up delayed start then the startchime won't be processed until we
            // intend to start the output transmission, ie. when we have the remote SDP.
            //
            if(false == bDelayStart && null != (joinChime = getChimeFilePath(conferenceDef.getJoinChime()))) {
                urlString.append("&startchime=" + joinChime);
            }

            if(null != (leaveChime = getChimeFilePath(conferenceDef.getLeaveChime()))) {
                urlString.append("&stopchime=" + leaveChime);
            }
            
        }

        if(0 == mtu) {
            mtu = m_mtu;
        }

        if(sliceSizeMax > 0) {
             urlString.append("&rtppktzmode=0");
            if( mtu > sliceSizeMax) {
                mtu = sliceSizeMax;
            }
        } 

        if(mtu > 0) {
            urlString.append("&mtu=").append(mtu);
        }

        if(null != (candidate = sdpLocal.getAudioCandidate())) {
            String codecName = getXcoderCodecName(candidate.getCodec().getName());
            int clockRate = candidate.getCodec().getOutputClockRate();
            int channels = candidate.getCodec().getChannelCount();
            if(channels <= 0) {
                channels = 1;
            }
            // We should not be getting any stereo audio at this time, even with Opus
            if(channels > 1) {
                channels = 1;
            }
 
            urlString.append("&xcode=ac=").append(codecName).append(",ar=").append(clockRate).append(",as=").append(channels);
            bHaveXcodeConfig = true;
        }
         
        if(null != conferenceDef && null != videoXcodeConfig) {

            if(false ==bHaveXcodeConfig) {
                urlString.append("&xcode=");
            }
            urlString.append(",").append(videoXcodeConfig);
        }

        return urlString.toString();
    }

    private String getPipStopUrl(String id, int port) {
        StringBuffer urlString = null;

        urlString = new StringBuffer(m_pipServer).append(":").append(port).append(PIP_STOP_URL).append(id);

        return urlString.toString();
    }

    private String getPipStatusUrl(String id, int port) {
        StringBuffer urlString = null;

        urlString = new StringBuffer(m_pipServer).append(":").append(port).append(PIP_STATUS_URL).
                                                 append(null != id ? id : "-1");

        return urlString.toString();
    }

    private String getPipTURNRelayUrl(String id, int port) {
        StringBuffer urlString = null;

        urlString = new StringBuffer(m_pipServer).append(":").append(port).append(PIP_TURNRELAY_URL).
                                                 append(null != id ? id : "-1");

        return urlString.toString();
    }


    private String sendPipCommand(String urlStr, int numRetries) 
                    throws MalformedURLException, IOException, InterruptedException {

        m_log.debug("sendPipCommand: " + urlStr);

        int index = 0;
        String id = null;
        BufferedReader in = null;
        URL url = new URL(urlStr);
        URLConnection pipConnection = null;

        do {
            try {
                pipConnection = url.openConnection();
                pipConnection.setConnectTimeout(PIP_CONNECT_TIMEOUT_MS);
                pipConnection.setReadTimeout(PIP_READ_TIMEOUT_MS);
                in = new BufferedReader(new InputStreamReader(pipConnection.getInputStream()));
                break;
            } catch(ConnectException e) {
                if(++index < numRetries) {
                    m_log.debug("sendPipCommand: " + e);
                    Thread.sleep(1000);
                } else {
                    m_log.debug("sendPipCommand ConnectException url.openConnection aborting after " + index + " attempts.");
                    throw e;
                }
            }

        } while(null == in);

        String inputLine;

        while ((inputLine = in.readLine()) != null)  {
            id = inputLine;
            break;
        }
        in.close();

        m_log.debug("sendPipCommand received: " + id);

        return id;
    }

    
    private String getPipResponseElement(String resp, String keyName) {

        if(null != resp) {
            int index;
            String [] arr = resp.split("&");
            for(index = 0; index < arr.length; index++) {
                String [] kv = arr[index].split("=");
                if(kv.length >= 1 && keyName.equalsIgnoreCase(kv[0])) {
                    return kv[1];
                }
            }
        }
        return null;
    }
  

    private String getPipResponseCode(String resp) {
        return getPipResponseElement(resp, "code");
    }

    private String getStatusResponseCode(String resp) {
        return getPipResponseElement(resp, "status");
    }

    private String getConfigResponseCode(String resp) {
         return getPipResponseElement(resp, "config");
    }

    private int getPipCommandRetryCount(Conference conference, int retries) {
        if(null == conference || 
          (conference.getStartTime() > 0 && System.currentTimeMillis() - conference.getStartTime() >  1000 * PIP_COMMAND_RETRIES_CONFSTART)) { 
            return retries;
        } else {
            return PIP_COMMAND_RETRIES_CONFSTART;
        }
    }

    private String startPip(SDP sdpLocal, SDP sdpRemote, String sdpLocalPath, 
                            Conference conference, IUser user, String idUpdate, TURNRelay turnRelay) 
                    throws MalformedURLException, IOException, InterruptedException {

        String respCode;
        String strRespCode;
        String urlStr = getPipStartUrl(sdpLocal, sdpRemote, sdpLocalPath, conference, user, idUpdate, turnRelay);


        if(null == (respCode = getPipResponseCode((strRespCode = 
                    sendPipCommand(urlStr, getPipCommandRetryCount(conference, PIP_COMMAND_RETRIES))))) || "-1".equals(respCode)) {
            m_log.error("Received error response code: " + strRespCode + " to: " + urlStr);
            respCode = null;
        }

        return respCode;
    }

    private String stopPip(String id, int port) throws MalformedURLException, IOException, InterruptedException {

        String respCode;
        String strRespCode;
        String urlStr = getPipStopUrl(id, port);

        if(null == (respCode = getPipResponseCode((strRespCode = 
                    sendPipCommand(urlStr, getPipCommandRetryCount(null, PIP_COMMAND_RETRIES))))) || "-1".equals(respCode)) {
            m_log.error("Received error response code: " + strRespCode + " to: " + urlStr);
            respCode = null;
        }
 
        return respCode;
    }

    private String [] getTURNRelayAddress(Conference conference, String id) {

        int port = conference.getPort();
        String respCode;
        String strResp;
        String [] arrRelays = null;
        String urlStr = getPipTURNRelayUrl(id, port);

        try {
            if(null == (respCode = getStatusResponseCode((strResp =
                                       sendPipCommand(urlStr, getPipCommandRetryCount(conference, PARTICIPANT_COMMAND_RETRIES))))) ||
                false == "OK".equals(respCode)) {
                return null;
            }

            String idList;
            if(null != (idList = getPipResponseElement(strResp, "turnRelayRtp"))) {
                String [] arr = idList.split(",");
                if(arr.length > 0) {
                    arrRelays = new String[arr.length];
                    int index;
                    for(index = 0; index < arr.length; index++) {
                        arrRelays[index] = arr[index].trim();
                    }
                }
            }

        } catch(SocketException e) {
            m_log.debug("getTURNRelayAddress: " + conference.getName() + " returned error: " + e);
            //throw e;
            return null;
        } catch(Exception e) {
            m_log.debug("getTURNRelayAddress: " + conference.getName() + " returned error: " + e);
        }

        return arrRelays;
    }

    public boolean isConferenceParticipantActive(Conference conference, String id) {

        int port = conference.getPort();
        String respCode;
        String strRespCode;
        String urlStr = getPipStatusUrl(id, port);

        try {
            if(null == (respCode = getPipResponseCode((strRespCode = 
                                       sendPipCommand(urlStr, getPipCommandRetryCount(conference, PARTICIPANT_COMMAND_RETRIES))))) ||
                "-1".equals(respCode)) {
                m_log.warn("isConferenceParticipantActive: " + conference.getName() + ", id: " + id + 
                             " returned error: " + strRespCode);
                return false;
            }

        } catch(SocketException e) {
            LogUtil.printError(m_log, "CheckConferenceParticipant: " + conference.getName() + ", id: " + id + 
                         " returned error: ", e);
            //throw e;
            return false;
        } catch(Exception e) {
            LogUtil.printError(m_log, "CheckConferenceParticipant: " + conference.getName() + ", id: " + id + 
                         " returned error: ", e);
            //CheckConference return false: java.io.IOException: Server returned HTTP response code: 500 for URL: http://127.0.0.1:9000/pip?pipstop=-1
        }
        //m_log.debug("CheckConference returning true");

        return true;

    }

    public String [] getConferenceParticipants(Conference conference) {

        int port = conference.getPort();
        String respCode;
        String strResp;
        String [] arrIds = null;
        String urlStr = getPipStatusUrl(null, port);

        try {
            if(null == (respCode = getPipResponseCode((strResp = 
                                       sendPipCommand(urlStr, getPipCommandRetryCount(conference, PARTICIPANT_COMMAND_RETRIES))))) ||
                "-1".equals(respCode)) {
                m_log.debug("getConferenceParticipants: " + conference.getName() + " returned error: " + respCode);
                return null;
            }

            String idList;
            if(null != (idList = getPipResponseElement(strResp, "ids"))) {
                String [] arr = idList.split(",");
                if(arr.length > 0) {
                    arrIds = new String[arr.length];
                    int index;
                    for(index = 0; index < arr.length; index++) {
                        arrIds[index] = arr[index].trim();
                    }
                }
            }
            
        } catch(SocketException e) {
            m_log.debug("getConferenceParticipants: " + conference.getName() + " returned error: " + e);
            //throw e;
            return null;
        } catch(Exception e) {
            m_log.debug("getConferenceParticipants: " + conference.getName() + " returned error: " + e);
            //CheckConference return false: java.io.IOException: Server returned HTTP response code: 500 for URL: http://127.0.0.1:9000/pip?pipstop=-1
        }
        //m_log.debug("CheckConference returning true");

        return arrIds;
    }



    private String sendPipConfigUrl(Conference conference, String urlStr) 
                    throws MalformedURLException, IOException, InterruptedException {
        String respCode;
        String strRespCode;

        if(null == (respCode = getConfigResponseCode((strRespCode =
                    sendPipCommand(urlStr, getPipCommandRetryCount(conference, CONFIG_COMMAND_RETRIES))))) || false == "OK".equals(respCode)) {
            m_log.error("Received error response code: " + strRespCode + " to: " + urlStr);
            respCode = null;
        }
        return respCode;
    }

    private String updateConfigSendonly(Conference conference, String id, int port, boolean bHold)
                    throws MalformedURLException, IOException, InterruptedException {

        return sendPipConfigUrl(conference, getConfigSendonlyUrl(id, port, bHold));
    }

    private String updateConfigRemoteDestination(Conference conference, String id, int port, String strRemoteDestination)
                    throws MalformedURLException, IOException, InterruptedException {

        return sendPipConfigUrl(conference, getConfigNewOutputUrl(id, port, strRemoteDestination));
    }

    private String getRecordingFilePath(InstanceId instanceId) {
        return m_xcoderRecordingDirUrlPath + File.separatorChar +  instanceId.getName() + File.separatorChar + 
               instanceId.toString();
    }

    private String getChimeFilePath(String chimeFile) {

        if(null != chimeFile) {
            if(chimeFile.charAt(0) == File.separatorChar) {
                return chimeFile;
            } else {
                return m_mediaChimeDirUrlPath + File.separator + chimeFile;
            }
        }

        return null;
    }

    public InstanceId startConference(Conference conference) throws Exception {
        int port = conference.getPort();
        ConferenceDefinition conferenceDefinition = conference.getConferenceDefinition();
        String conferenceName = conference.getName().getString();
        String recordingName = null;
        String streamOutput = null;
        String videoLayout = null;
        String extraArgs = "";
        String xcodeArgs = "";
        boolean bSetLocalAddress = false;
        String argPort = new Integer(port).toString();
        InstanceId instanceId = InstanceId.create(conferenceName);
        String [] arrStart = new String[6];

        if(m_mtu > 0) {
            extraArgs += " --mtu=" + m_mtu;
        }

        if(null != conferenceDefinition.getExtraArgs()) {
            extraArgs += conferenceDefinition.getExtraArgs();
            bSetLocalAddress = true;
        }

        if(null != conferenceDefinition.getInitializerString()) {
            xcodeArgs += conferenceDefinition.getInitializerString();
        }

        if(null != (streamOutput = conferenceDefinition.getStreamOutput())) {
            if("auto".equalsIgnoreCase(streamOutput)) {
                extraArgs += " broadcast=auto";
                bSetLocalAddress = true;
            }
        }

        if(null != (recordingName = conferenceDefinition.getRecordingName()) &&
           false == "false".equalsIgnoreCase(recordingName)) {

            if(true == "true".equalsIgnoreCase(recordingName)) {
                recordingName = getRecordingFilePath(instanceId);
                conference.setRecordingFilePath(recordingName);
            } 

            //
            // recording name should not have a file suffix - it's automatically assigned by the
            // startup script depending on the a/v codecs.
            //
            extraArgs += " record=" + recordingName;

        }

        if(null != (videoLayout = conferenceDefinition.getLayout())) {
            extraArgs += " --layout=" + videoLayout;
        }

        if(bSetLocalAddress) {
            extraArgs += " --localhost=" + m_strConnectAddress;
        }

        if(conferenceDefinition.isAllowDTLS()) {
            if(null != m_dtlsCertificateUrlPath) {
                extraArgs += " --dtlscert=" + m_dtlsCertificateUrlPath;
            }
            if(null != m_dtlsKeyUrlPath) {
                extraArgs += " --dtlskey=" + m_dtlsKeyUrlPath;
            }
        }

        //TODO: create source addr connetion filter so pip/config can use filter and http/rtsp be global
        //argPort = "http://127.0.0.1:" + argPort;
        
        arrStart[0] = ProcessControl.SYSTEM_SHELL;
        arrStart[1] = m_xcoderStartScriptPath;
        arrStart[2] = instanceId.toString();
        arrStart[3] = argPort;
        arrStart[4] = null != xcodeArgs && xcodeArgs.length() > 0 ? xcodeArgs : "none";
        arrStart[5] = null != extraArgs && extraArgs.length() > 0 ? extraArgs : "none";

        m_log.debug("startConference port: " + port + ", instanceId: " + instanceId.toString());
        m_log.debug(arrStart[0] + ", " + arrStart[1] + ", " + arrStart[2] + ", " + arrStart[3] + ", " + 
                     arrStart[4] + ", " + arrStart[5]);

        ProcessControl.startProcess(arrStart);

        //pid = ProcessControl.getProcessPid(m_xcoderTmpDir + File.separatorChar + nstanceId.toString() + ".pid", 2000);
        // Blind sleep here to allow the process to spawn
        Thread.sleep(1000);

        return instanceId;
    }

    public void closeConference(Conference conference) throws Exception {
        InstanceId instanceId = conference.getInstanceId();
        String [] arrStart = new String[4];

        arrStart[0] = ProcessControl.SYSTEM_SHELL;
        arrStart[1] = m_xcoderStartScriptPath;
        arrStart[2] = instanceId.toString();
        arrStart[3] = "kill";

        m_log.debug("closeConference port: " + conference.getPort() + ", instanceId: " + instanceId.toString());
        m_log.debug(arrStart[0] + ", " + arrStart[1] + ", " + arrStart[2] + ", " + arrStart[3]);

        ProcessControl.startProcess(arrStart);

    }

    private void setTURNRelayAddress(Conference conference, String id, TURNRelay turnRelay) throws InterruptedException {

        long tm0 = System.currentTimeMillis();

        int TURN_RELAY_ADDRESS_TIME_LIMIT = 5;

        do {

            try {
                String arrTurnRelays[] = getTURNRelayAddress(conference, id);
                if(null != arrTurnRelays && arrTurnRelays.length > 0) {
                    turnRelay.setAddress(arrTurnRelays[0]);
                 }
   
                if(null != turnRelay.getAddress() && turnRelay.getAddress().isValid()) {
                    m_log.debug("Retrieved TURN Relay binding allocation address: " + turnRelay.getAddress());
                    return;
                }

            } catch(UnknownHostException e) {
            }

            Thread.sleep(1000);

        } while(System.currentTimeMillis() - tm0 < 1000 * TURN_RELAY_ADDRESS_TIME_LIMIT);

        m_log.error("Failed to retrieve TURN Relay binding allocation address");
    }


    /**
     * Adds a participant to an existing conference instance
     * @param conference A previously started conference instance
     * @param sdpLocal The SDP of the local server.  This is the local response SDP to an incoming INVITE or the local INVITE SDP of an outgoing call. 
     * @param sdpRemote The SDP of remote client.  This is the remote SDP from an incoming INVITE or the remote SDP response to a local INVITE.
     *                  <i>sdpRemote</i> can be null if a remote 200 OK INVITE response has not yet been received.  In such case, the PIP user will be 
     *                  added with the 'delaystart=1' parameter set.
     * @param user The participant user instance being added to the conference
     * @param idUpdate An optional parameter instructing the underlying media implementation that an existing participant 
     *                 state should be updated instead of added to the conference.  The value of
     *                 this parameter should be the return value of a previous call to add the participant into the 
     *                 conference.  If a conference is not being updated but a participant is being added then this parameter should be null.
     * @param turnRelay 
     * @return A unique identifier of the conference participant assigned by the underlying media implementation
     */
    public String addConferenceParticipant(Conference conference, 
                                           SDP sdpLocal, 
                                           SDP sdpRemote, 
                                           IUser user, 
                                           String idUpdate,
                                           TURNRelay turnRelay) throws Exception {
        String id = "-1";
        Exception error = null;
        String strTmNow = new Long(System.currentTimeMillis()).toString();
        SDP sdpListener = createListenerSDP(sdpLocal, sdpRemote);
        String sdpListenerText = sdpListener.serialize();
        ConferenceDefinition conferenceDef = conference.getConferenceDefinition();
        SDP.Media media = (null != sdpLocal.getAudioMedia() ? sdpLocal.getAudioMedia() : sdpLocal.getVideoMedia());
        String sdpPath = m_xcoderTmpDir + "/conf_" + String.valueOf(media.getTarget().getPort()) + "_" + strTmNow + ".sdp";
        String sdpUrlPath = m_xcoderTmpDirUrlPath + "/conf_" + String.valueOf(media.getTarget().getPort()) + "_" + strTmNow + ".sdp";
        boolean bTURNEnabled = false;

        m_log.debug("addConferenceParticipant user: " + user.getName() + ", to: " + conference.getName() + 
                    ", remote-type:" + (sdpRemote != null ? sdpRemote.getDeviceType() : ""));

        writeSdpPath(sdpListenerText, sdpPath);
        m_log.debug("Wrote SDP file " + sdpPath + "\r\n" + sdpListenerText);

        conferenceDef = conference.getConferenceDefinition();
        if(null != turnRelay && conferenceDef.isTURNRelayEnabled() && ConferenceHandler.isSDPMediaHaveTURNRelay(sdpLocal) &&
           null != conferenceDef.getTurnServer()) {
            bTURNEnabled = true;
        }

        try {

            id = startPip(sdpLocal, sdpRemote, sdpUrlPath, conference, user, idUpdate, (bTURNEnabled ? turnRelay : null));

        } catch(Exception e) {
            m_log.error("Failed to start PIP: " + e);
            error = e;
        } finally {
            if(null != id) {
                // Sleep here so that we can safely delete the SDP temp file
                Thread.sleep(1000);
            } else {
                error = new IOException("Failed to " + (null != idUpdate ? "update" : "start") +" PIP");
            }
            removeSdpPath(sdpPath);
            m_log.debug("Cleaned up SDP file " + sdpPath);
        }

        if(null != error) {
            throw error;
        }

        if(bTURNEnabled) {
            setTURNRelayAddress(conference, id, turnRelay);
        }

        return id;
    }

    public void removeConferenceParticipant(Conference conference, String id) throws Exception {
        int port = conference.getPort();
        String code;

        m_log.debug("removeConferenceParticipant id: " + id);

        code = stopPip(id, port);

    }

    public void holdConferenceParticipant(Conference conference, String id, boolean bHold) throws Exception {
        int port = conference.getPort();
        String code;
    
        m_log.debug("holdConferenceParticipant id: " + id + ", hold: " + bHold);

        code = updateConfigSendonly(conference, id, port, bHold);

        if(false == "OK".equals(code)) {
          throw new IOException("Failed to " + (bHold ? "hold" : "resume") + " user id: " + id + 
                                " in conference: " + conference.getName());
        }

    }

    public void updateConferenceParticipantDestination(Conference conference, String id, SDP sdpRemote) throws Exception {
        int port = conference.getPort();
        String code;
        StringBuffer strRemoteDestination = getRemoteDestination(sdpRemote, RTPTransportType.rtp);
        StringBuffer rtcpPorts;

        if(null != (rtcpPorts = getRTCPPorts(sdpRemote))) {
            strRemoteDestination.append("&rtcpports=").append(rtcpPorts);
        }

        m_log.debug("UpdateParticipantDestination id: " + id + ", remote: " + strRemoteDestination.toString());

        code = updateConfigRemoteDestination(conference, id, port, strRemoteDestination.toString());

        if(false == "OK".equals(code)) {
          throw new IOException("Failed to update remote destination address for user id: " + id +
                                " in conference: " + conference.getName());
        }

    }

/*
    public static void main(String [] args) {

        try {
            Configuration config =  new Configuration("../../debug-ngconference.conf");
            XCoderMediaProcesso n  = new XCoderMediaProcessor(config);
            Call call = new Call(new User(null), new User(null), new PortManager(7000,8000,2));
            Conference c = new Conference(null, "my conf", 5001, null, null, null);

            //boolean b = n.isConferenceParticipantActive(c, null);
            String [] ids= n.getConferenceParticipants(c);
            if(null != ids) {
                int i;
                for(i = 0; i < ids.length; i++) {
                    System.out.println("_id:'"+ids[i]+"'");
                }
            } 

        } catch(Exception e) {
            System.out.println(e);
        }

    }
*/
}
