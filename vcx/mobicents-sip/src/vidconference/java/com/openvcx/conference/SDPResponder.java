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

import com.openvcx.util.*;

import java.io.IOException;
import java.lang.SecurityException;
import java.security.NoSuchAlgorithmException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.List;
import java.util.Random;

import org.apache.log4j.Logger;

/**
 *
 * Class for constructing an SDP response and request document
 *
 */
public class SDPResponder {

    private final Logger m_log = Logger.getLogger(getClass());

    /**
     * <p>Configuration string for the master configuration file defining the published connect address of the server.</p>
     * <p><i>sdp.connect.location</i></p>
     */
    public static final String CONFIG_SERVER_CONNECT_ADDRESS                 = "sdp.connect.location";
    private static final String CONFIG_SERVER_CODEC_AUDIO                    = "codec.audio";
    private static final String CONFIG_SERVER_CODEC_VIDEO                    = "codec.video";
    private static final int MAX_CODEC_COUNT                                 = 30;

    private String m_connectLocation;
    private List<Codec> m_listCodecsV;
    private List<Codec> m_listCodecsA;
    private InetAddress m_listenAddress;
    private ISDPNaming m_namingI;
    private SDP.Fingerprint m_dtlsCertFingerprint;

    /**
     * Enumeration defining the type of SDP document to be created
     */
    public enum SDP_RESPONSE_TYPE {
        /**
         * <p>SDP creation type which should include only one best matching media candidate for audio and video</p>
         */
        SDP_RESPONSE_BEST_MATCH,

        /**
         * <p>SDP creation type which should include all supported media candidates for audio and video</p>
         */
        SDP_RESPONSE_ALL_SUPPORTED,

        /**
         * <p>SDP creation type which should include multiple matching media candidates for audio and video</p>
         */
        SDP_RESPONSE_MULTIPLE_MATCH,
    }
 
    /**
     * <p>Constructor used to initialize the instance.</p>
     * @param namingI An SDP naming instance used to normalize the naming convention between SDP naming formats and the underlying media processor implementation specific name mappings.</p>
     * @param config The application configuration instance
     */
    public SDPResponder(ISDPNaming namingI, Configuration config) throws Exception {
        m_namingI = namingI;
        m_listCodecsV = new ArrayList<Codec>();
        m_listCodecsA = new ArrayList<Codec>();
        m_dtlsCertFingerprint = null;

       init(config);

    }

    private void loadCodecs(Configuration config, String configPrefix, List<Codec> list) {
        list.clear();
        for(int index = 0; index < MAX_CODEC_COUNT; index++) {
            String key = configPrefix + "." + index;
            String value = null;
            try { 
                value = config.getString(key);
            } catch(IOException e) {
                continue;
            }
            if(null != value) { 
                Codec codec = Codec.parse(value);
                if(null != codec) {
                    m_log.debug("Configured codec: " + codec.toString());
                    list.add(codec);
                }
            }
        }

    }

    private void init(Configuration config) throws Exception {

        //
        // Set Global IP to be published in server SDP 'c='
        //
        m_listenAddress = InetAddress.getByName(config.getString(CONFIG_SERVER_CONNECT_ADDRESS));

        m_log.info("Server published global SDP connect location is: " + m_listenAddress.getHostAddress()); 

        //
        // Load the prioritized list of supported media codecs
        //
        loadCodecs(config, CONFIG_SERVER_CODEC_AUDIO, m_listCodecsA);
        loadCodecs(config, CONFIG_SERVER_CODEC_VIDEO, m_listCodecsV);

        m_log.info("Loaded " + m_listCodecsA.size() + " audio codecs, " + m_listCodecsV.size() + 
                    " video codecs from " + config.getFilePath());

        if(m_listCodecsA.size() <= 0) {
            throw new IOException("No audio codecs loaded from config file " + config.getFilePath());
        }

        initDtls(config);

    }

    private void initDtls(Configuration config) {

        String certificatePath = config.getString(DTLSUtil.CONFIG_SERVER_SSL_DTLS_CERT, null);

        if(null != certificatePath) {
            try {
                m_dtlsCertFingerprint = new DTLSUtil().getCertificateFingerprint(certificatePath, config);
                m_log.info("DTLS certificate: " + certificatePath + ", fingerprint:" + 
                       m_dtlsCertFingerprint.getAlgorithmAsString() + " " + m_dtlsCertFingerprint.getHash());
            } catch(Exception e) {
                LogUtil.printError(m_log, "Failed to obtain DTLS fingerprint for certificate " + 
                        DTLSUtil.CONFIG_SERVER_SSL_DTLS_CERT + ": ", e);
            }
        } else {
            m_log.info("DTLS is not enabled");
        }
    }

    private SDP.MediaCandidate findConferenceExplicitCandidate(String conferenceExplicitCodecName,
                                                              List<Codec> listSupported,
                                                              SDP.MediaCandidate [] advertisedCandidates) 
                                                              throws CloneNotSupportedException {
        if(null == conferenceExplicitCodecName) {
            return null;
        }

        Iterator<Codec> iterSupported = listSupported.iterator();
        while(iterSupported.hasNext()) {
            Codec codec = iterSupported.next();

            for(int index = 0; index < advertisedCandidates.length; index++) {
                if(advertisedCandidates[index].getCodec().equals(codec, false) &&
                   conferenceExplicitCodecName.equalsIgnoreCase(codec.getName())) {

                    SDP.MediaCandidate candidate = advertisedCandidates[index].clone();
                    //
                    // This overrides any SDP codec fmtp parameters with those specified in the local codec config
                    //
                    candidate.getCodec().setFmtp(codec.getFmtp());
                    return candidate;
                }
            }

        }

        return null;
    }

    private SDP.MediaCandidate findBestMatchingCandidate(ConferenceDefinition conferenceDef,
                                                         List<Codec> listSupported,
                                                         SDP.MediaCandidate [] advertisedCandidates,
                                                         DeviceType deviceType) 
                                                         throws CloneNotSupportedException {
        SDP.MediaCandidate candidate = null;
        //
        // Attempt to find a match using the conference (shared) video encoder output codec
        //
        if(null != conferenceDef && 
           null != (candidate = findConferenceExplicitCandidate(
                                                    m_namingI.getSDPCodecName(conferenceDef.getVideoCodecName()),
                                                    listSupported,
                                                    advertisedCandidates))) {
            return candidate;
        }

        Iterator<Codec> iterSupported = listSupported.iterator();
        List<IEncoderConfig> listEncoders;
        Iterator<IEncoderConfig> iterEncoders;

        while(iterSupported.hasNext()) {
            Codec codec = iterSupported.next();

            for(int index = 0; index < advertisedCandidates.length; index++) {

                if(advertisedCandidates[index].getCodec().equals(codec, false)) {

                    boolean acceptCodec = true;

                    if(null != conferenceDef) {

                        if(null == conferenceDef.getEncoderList()) {
                            return null;
                        } else if(null == conferenceDef.getEncoderConfigLoader().getEncoderConfig(codec, deviceType, m_namingI)) {
                            acceptCodec = false;
                        }

                    }

                    if(acceptCodec) {
                        candidate = advertisedCandidates[index].clone();

                        //
                        // This overrides any SDP codec fmtp parameters with those specified in the local codec config
                        //
                        candidate.getCodec().setFmtp(codec.getFmtp());
                        return candidate;
                    }

                }
            }
      }

        return null; 
    }

    private SDP createSDP() {
        SDP sdp = new SDP();        

        sdp.setSession("conference");

        return sdp;
    }

    private void setVideoPort(Call call, SDP sdpReq, SDP sdpResp, boolean bBundlePorts, int videoPort) {
        //
        // If the request SDP is using the same port for audio and video, we will do the same in the response SDP
        //
        if(bBundlePorts || (null != sdpReq && null != sdpReq.getAudioMedia() && null != sdpReq.getVideoMedia() &&
           sdpReq.getAudioMedia().getConnect().getPort() == sdpReq.getVideoMedia().getConnect().getPort())) {

            call.setServerVideoPort(call.getOrAllocateServerAudioPort());
            sdpResp.setVideoPort(call.getOrAllocateServerVideoPort());
        } else {
            sdpResp.setVideoPort(videoPort > 0 ? videoPort : call.getOrAllocateServerVideoPort());
        }
    }

    private SDP getAllSupportedSDP(Call call, SDP sdpReq, int videoPort, int audioPort) 
                    throws IOException, CloneNotSupportedException {
        Iterator<Codec> iterCodec;
        ConferenceDefinition conferenceDef = call.getConferenceDefinition();
        String conferenceVideoCodecName = null != conferenceDef ? 
                                      m_namingI.getSDPCodecName(conferenceDef.getVideoCodecName()) : null;
        SDP sdp = createSDP();

        if(m_listCodecsA.size() > 0) {
            sdp.setAudioPort(audioPort > 0 ? audioPort : call.getOrAllocateServerAudioPort());
            sdp.setAudioAddress(m_listenAddress.getHostAddress()); 
        }

        iterCodec = m_listCodecsA.iterator();
        while(iterCodec.hasNext()) {
            Codec codec = iterCodec.next();
            sdp.addAudioCandidate(codec);
        }

        if(m_listCodecsV.size() > 0) {
            setVideoPort(call, sdpReq, sdp, (null == sdpReq && videoPort == audioPort ? true : false), videoPort);
            sdp.setVideoAddress(m_listenAddress.getHostAddress()); 
        }


        iterCodec = m_listCodecsV.iterator();
        while(iterCodec.hasNext()) {
            Codec codec = iterCodec.next();
            if(null == conferenceVideoCodecName || codec.getName().equalsIgnoreCase(conferenceVideoCodecName)) {
                sdp.addVideoCandidate(codec);
            }
        }

        return sdp;
    }

    private SDP getResponseSDPMultipleMatch(Call call, SDP sdpReq, int videoPort, int audioPort) 
                    throws IOException, CloneNotSupportedException {
        Iterator<Codec> iterCodec;
        ConferenceDefinition conferenceDef = call.getConferenceDefinition();
        String conferenceVideoCodecName = null != conferenceDef ? 
                                      m_namingI.getSDPCodecName(conferenceDef.getVideoCodecName()) : null;;
        SDP sdp = createSDP();        

        iterCodec = m_listCodecsA.iterator();
        while(iterCodec.hasNext()) {
            Codec codec = iterCodec.next();
            SDP.MediaCandidate [] advertisedCandidates = sdpReq.getAudioCandidates();
            for(int index = 0; index < advertisedCandidates.length; index++) {

                if(advertisedCandidates[index].getCodec().equals(codec, false)) {

                    Codec peerCodec = CodecFactory.create(codec.getName(), 
                                                          advertisedCandidates[index].getCodec().getPayloadType(), 
                                                          codec.getClockRate());
                    sdp.addAudioCandidate(peerCodec);
                }
            }
        }

        if(sdp.getAudioCandidatesCount() > 0) {
            sdp.setAudioPort(audioPort > 0 ? audioPort : call.getOrAllocateServerAudioPort());
            sdp.setAudioAddress(m_listenAddress.getHostAddress()); 
        }

        iterCodec = m_listCodecsV.iterator();
        while(iterCodec.hasNext()) {
            Codec codec = iterCodec.next();

            if(null != conferenceVideoCodecName && 
               false == codec.getName().equalsIgnoreCase(conferenceVideoCodecName)) {
                continue;
            }

            SDP.MediaCandidate [] advertisedCandidates = sdpReq.getVideoCandidates();
            for(int index = 0; index < advertisedCandidates.length; index++) {

                if(advertisedCandidates[index].getCodec().equals(codec, false)) {

                    Codec peerCodec = CodecFactory.create(codec.getName(),
                                                          advertisedCandidates[index].getCodec().getPayloadType(), 
                                                          codec.getClockRate());
                    sdp.addVideoCandidate(peerCodec);
                }
            }
        }

        if(sdp.getVideoCandidatesCount() > 0) {
            setVideoPort(call, sdpReq, sdp, false, videoPort);
            sdp.setVideoAddress(m_listenAddress.getHostAddress());
        }

        return sdp;
    }


    private SDP getResponseSDPBestMatch(Call call, SDP sdpReq, int videoPort, int audioPort) throws Exception {
        SDP sdpResp = null;        
        SDP.Media media;
        SDP.MediaCandidate audioCandidate = null;
        SDP.MediaCandidate videoCandidate = null;

        if(null != (media = sdpReq.getAudioMedia()) && media.getConnect().getPort() > 0) {
            audioCandidate = findBestMatchingCandidate(null, m_listCodecsA, sdpReq.getAudioCandidates(), 
                                                       sdpReq.getDeviceType());
        }
        if(null != (media = sdpReq.getVideoMedia()) && media.getConnect().getPort() > 0) {
            videoCandidate = findBestMatchingCandidate(call.getConferenceDefinition(), m_listCodecsV, 
                                                       sdpReq.getVideoCandidates(), sdpReq.getDeviceType());
        }

        if(null == audioCandidate && null == videoCandidate) {
            throw new IOException("No supported matching SDP candidates found for: " + sdpReq.toString());
        }

        sdpResp = createSDP();

        if(null != audioCandidate) {
            sdpResp.setAudioPort(audioPort > 0 ? audioPort : call.getOrAllocateServerAudioPort());
            sdpResp.setAudioAddress(m_listenAddress.getHostAddress()); 
            sdpResp.addAudioCandidate(audioCandidate.getCodec()); 
        }

        if( null != videoCandidate) {

            setVideoPort(call, sdpReq, sdpResp, false, videoPort);
            sdpResp.setVideoAddress(m_listenAddress.getHostAddress()); 
            sdpResp.addVideoCandidate(videoCandidate.getCodec());
        }

        return sdpResp;
    }
    
    private void updateSDPCrypto(SDP.Media mediaReq, SDP.Media mediaResp, 
                                   ConferenceDefinition.SRTP_POLICY srtpPolicy) 
            throws NoSuchAlgorithmException, CloneNotSupportedException {

        Iterator<SDP.SRTPCrypto> iterCrypto = mediaReq.getCryptoListIterator();
        SDP.SRTPCrypto cryptoHighestPrio = null;

        while(iterCrypto.hasNext()) {
            SDP.SRTPCrypto crypto = iterCrypto.next();

            if(null == cryptoHighestPrio ||
               crypto.getAlgorithmOrdinal() < cryptoHighestPrio.getAlgorithmOrdinal()) {
                cryptoHighestPrio = crypto;
            }
        }

        if(null == cryptoHighestPrio) {
            m_log.error("SDP request " + mediaReq.getMediaTypeAsString() + " is missing crypto description(s)");
        }

        SDP.SRTPCrypto cryptoResponse = new SDP.SRTPCrypto(cryptoHighestPrio.getTag(),
                                               cryptoHighestPrio.getAlgorithmAsString(),
                                               new KeyGenerator().makeKeyBase64(SDP.SRTPCrypto.KEY_LENGTH));
        cryptoResponse.setPeer(cryptoHighestPrio);
        mediaResp.addCrypto(cryptoResponse, true);

    }

    private void updateSDPFingerprint(SDP.Media mediaReq, SDP.Media mediaResp,
                                   ConferenceDefinition.SRTP_POLICY srtpPolicy)
            throws NoSuchAlgorithmException, CloneNotSupportedException {

        if(null != m_dtlsCertFingerprint) {
            mediaResp.getFingerprint().set(m_dtlsCertFingerprint);
            mediaResp.getFingerprint().setPeer(mediaReq.getFingerprint());
        }

    }
 
    private void setCandidateMatch(SDP.ICECandidate [] arrMatch, SDP.ICECandidate iceCandidate) {

        if(null == arrMatch[0] && iceCandidate.getId() == SDP.ICECandidate.ICE_COMPONENT_RTP) {
            arrMatch[0] = iceCandidate;
        } else if(null == arrMatch[1] && iceCandidate.getId() == SDP.ICECandidate.ICE_COMPONENT_RTCP) {
            arrMatch[1] = iceCandidate;
        }
    }

    private void updateMediaICE(final SDP.Media mediaReq, SDP.Media mediaResp, SDP.Media mediaRespAv,
                                boolean bSet, ConferenceDefinition conferenceDefinition, 
                                boolean bPreferIceHost, boolean bEnableTURN) 
            throws UnknownHostException, NoSuchAlgorithmException, CloneNotSupportedException {

        final int iceCandidateTypes = SDP.ICECandidate.TYPE.relay.ordinal();
        Iterator<SDP.ICECandidate> iterIce = mediaReq.getICECandidatesIterator();
        SDP.ICECandidate [][] iceCandidateMatch = new SDP.ICECandidate[iceCandidateTypes + 1]
                                                                      [SDP.ICECandidate.ICE_COMPONENT_RTCP];
        SDP.ICECandidate iceCandidateRTP = null, iceCandidateRTCP = null;
        int index;

        if(mediaResp.getConnect().getPort() <= 0 || null == mediaResp.getConnect().getAddress()) {
            return;
        }

        while(iterIce.hasNext()) {
            SDP.ICECandidate iceCandidate = iterIce.next();

            //
            // This iteration sets the first candidate (assuming priority ordered)
            //
            if((SDP.ICECandidate.TRANSPORT.udp == iceCandidate.getTransport())) {

                if(iceCandidate.getICEAddress().isIPv6()) {
                    m_log.debug("Skipping ICE IPv6 candidate: " + iceCandidate);
                    continue;
                }

                if(false == bPreferIceHost && SDP.ICECandidate.TYPE.prflx == iceCandidate.getType()) {
                    setCandidateMatch(iceCandidateMatch[SDP.ICECandidate.TYPE.prflx.ordinal()], iceCandidate);
                } else if(false == bPreferIceHost  && SDP.ICECandidate.TYPE.srflx == iceCandidate.getType()) {
                    setCandidateMatch(iceCandidateMatch[SDP.ICECandidate.TYPE.srflx.ordinal()], iceCandidate);
                } else if(SDP.ICECandidate.TYPE.host == iceCandidate.getType()) {
                    setCandidateMatch(iceCandidateMatch[SDP.ICECandidate.TYPE.host.ordinal()], iceCandidate);
                } else if(SDP.ICECandidate.TYPE.relay == iceCandidate.getType()) {
                    setCandidateMatch(iceCandidateMatch[SDP.ICECandidate.TYPE.relay.ordinal()], iceCandidate);
                }
            }

        }

        //
        // Go through the priority list of candidates and set according to preferred candidate type (prflx,srflx,host) order
        //
        for(index = 0; index < iceCandidateTypes; index++) { 
            if(null != iceCandidateMatch[index][0]) {
                iceCandidateRTP = iceCandidateMatch[index][0];
                iceCandidateRTCP = iceCandidateMatch[index][1];
                break;
            }
        }

        if(bSet && null != iceCandidateRTP) {

            //
            // Set an SDP RTP response ICE candidate of the server
            //
            SDP.ICECandidate localIceRTP = new SDP.ICECandidate(iceCandidateRTP.getFoundation(),
                                                                SDP.ICECandidate.ICE_COMPONENT_RTP,
                                                                iceCandidateRTP.getTransport(),
                                                                iceCandidateRTP.getPriority(),
                                                                mediaResp.getConnect().clone(),
                                                                iceCandidateRTP.getType());
            mediaResp.addICECandidate(localIceRTP, true);

            //
            // Set an SDP RTCP response ICE candidate of the server
            //
            SDP.ICECandidate localIceRTCP = localIceRTP.clone();
            localIceRTCP.setId(SDP.ICECandidate.ICE_COMPONENT_RTCP);
            if(false == mediaResp.isRtcpMux()) {
                localIceRTCP.getICEAddress().setPort(localIceRTP.getICEAddress().getPort() + 1);
            }
            mediaResp.addICECandidate(localIceRTCP, false);

            //
            // Add TURN relay ICE candidate(s)
            //
            if(bEnableTURN && null != iceCandidateRTP && 
               SDP.ICECandidate.TYPE.relay != iceCandidateRTP.getType() && 
               null != iceCandidateMatch[SDP.ICECandidate.TYPE.relay.ordinal()][0]) {

                SDP.ICECandidate iceCandidateTURNRTP = iceCandidateMatch[SDP.ICECandidate.TYPE.relay.ordinal()][0];
                SDP.ICEAddress turnAddress = null;
                try {
                    turnAddress = new SDP.ICEAddress(conferenceDefinition.getTurnServer());
                } catch(Exception e) {
                    turnAddress = mediaResp.getConnect();
                    m_log.warn("TURN ice-candidate advertising remote TURN server address " + 
                               turnAddress.toString() + " because no TURN server has been configured");
                }

                SDP.ICECandidate localIceTURNRTP = new SDP.ICECandidate(
                        iceCandidateMatch[SDP.ICECandidate.TYPE.relay.ordinal()][0].getFoundation(),
                                                                 SDP.ICECandidate.ICE_COMPONENT_RTP,
                                                                 iceCandidateTURNRTP.getTransport(),
                                                                 iceCandidateTURNRTP.getPriority(),
                                                                 turnAddress.clone(),
                                                                 iceCandidateTURNRTP.getType(),
                                                                 mediaResp.getConnect().clone());

                mediaResp.addICECandidate(localIceTURNRTP, false);
                SDP.ICECandidate localIceTURNRTCP = localIceTURNRTP.clone();
                localIceTURNRTCP.setId(SDP.ICECandidate.ICE_COMPONENT_RTCP);
                //if(false == mediaResp.isRtcpMux()) {
                    //localIceRTCP.getICEAddress().setPort(localIceRTP.getICEAddress().getPort() + 1);
                //}
                mediaResp.addICECandidate(localIceTURNRTCP, false);
            }
 

            //
            // Indicate that we are doing ice-lite
            //
            mediaResp.setICELite(true);

            //
            // Create a username, password for ICE/STUN/TURN
            // Each unique RTP port will have it's own credentials
            // 
            if(null == mediaRespAv || (false == mediaRespAv.isICESameAudioVideo() && 
                                       mediaRespAv.getConnect().getPort() != mediaResp.getConnect().getPort())) {
                mediaResp.setICEUfrag(KeyGenerator.toBase64(new KeyGenerator().getRandomBytes(12)));
                mediaResp.setICEPwd(KeyGenerator.toBase64(new KeyGenerator().getRandomBytes(18)));
            } else {
                mediaResp.setICEUfrag(mediaRespAv.getICEUfrag());
                mediaResp.setICEPwd(mediaRespAv.getICEPwd());
            }

        }

        if(null != iceCandidateRTP) {

            //
            // set the request SDP target port (preferred candidate)
            //
            mediaReq.setTarget(iceCandidateRTP.getICEAddress().clone());
            SDP.ICEAddress iceTargetRTCP = mediaReq.getTarget().clone();
            if(null != iceCandidateRTCP) {
                iceTargetRTCP.setPort(iceCandidateRTCP.getICEAddress().getPort());
            } else {
                iceTargetRTCP.setPort(iceTargetRTCP.getPort() + 1);
            }
            mediaReq.setTargetRTCP(iceTargetRTCP);
            m_log.debug("ICE remote-candidate (target) from SDP RTP:" + mediaReq.getTarget().toString() + 
                         ", RTCP:" + mediaReq.getTargetRTCP());
/*
            //
            // Set a response-candidates list indicating which candidate(s) we are choosing to use
            //
            mediaResp.getICERemoteCandidates().add(
                       new SDP.ICERemoteCandidate(iceCandidateRTP.getId(), mediaReq.getTarget().clone()), true);
            if(null != iceCandidateRTCP) {
                mediaResp.getICERemoteCandidates().add(
                       new SDP.ICERemoteCandidate(iceCandidateRTCP.getId(), iceCandidateRTCP.getICEAddress().clone()), false);
            }
*/
        
        }
    }

    private void updateMediaRTCP(SDP.Media mediaReq, SDP.Media mediaResp, boolean bIsVideo) 
                     throws CloneNotSupportedException {

        //
        // Set any reponse rtcp-mux parameters to match the requestor
        //
        mediaResp.setRtcpMux(mediaReq.isRtcpMux());

        //
        // For video always set rtcp-fb:* ccm fir
        //
        if(bIsVideo) {
            int fbPayloadType = -1;            
            SDP.MediaCandidate [] candidates = mediaResp.toArray();
            if(null != candidates && candidates.length > 0) {
                fbPayloadType = candidates[0].getCodec().getPayloadType();
            }

            mediaResp.addRTCPFB(new SDP.RTCPFB(SDP.RTCPFB.TYPE.CCM_FIR, fbPayloadType));
            mediaResp.addRTCPFB(new SDP.RTCPFB(SDP.RTCPFB.TYPE.NACK_GENERIC, fbPayloadType));

            Iterator<SDP.RTCPFB> iterRTCPFb = mediaReq.getRTCPFBIterator();
            while(iterRTCPFb.hasNext()) {
                SDP.RTCPFB rtcpFb = (SDP.RTCPFB) iterRTCPFb.next();

                if(SDP.RTCPFB.TYPE.NACK_PLI == rtcpFb.getType()) {
                    //
                    // Set nack pli, if it is present in the request SDP
                    //
                    rtcpFb = rtcpFb.clone();
                    rtcpFb.setPayloadType(fbPayloadType);
                    mediaResp.addRTCPFB(rtcpFb);
               } else if(SDP.RTCPFB.TYPE.GOOG_REMB == rtcpFb.getType()) {
                    //
                    // Set goog-remb, if it is present in the request SDP
                    //
                    rtcpFb = rtcpFb.clone();
                    rtcpFb.setPayloadType(fbPayloadType);
                    mediaResp.addRTCPFB(rtcpFb);
               }

            }

        }
 
    }

    private void updateMediaDTMF(SDP.Media mediaReq, SDP.Media mediaResp) {

        //TODO: currently, nothing is done w/ DTMF telephone-event 
        //      so, we don't signal it back in the SDP response as

        //Codec codec = mediaReq.findCodec(Codec.TELEPHONE_EVENT);
        //if(null != codec) {
        //    mediaResp.addCandidate(codec);
        //}

    }

    private long createSSRC() {
        long ssrc;
        while((ssrc = (long) new Random().nextInt()) < 0) {
            ssrc *= -1;
        }
        return ssrc;
    }

    private long updateMediaSSRCElement(String tag, long ssrc, String tagValue,
                                        final SDP.Media mediaReq, SDP.Media mediaResp, 
                                        final SDP.Media mediaRespAv) {
        SDP.SSRCAttrib ssrcAttrib;

        if(null != (ssrcAttrib = mediaReq.findSSRCAttrib(tag))) {
            if(0 == ssrc) {
                ssrc = createSSRC();
            }
            SDP.SSRCAttrib ssrcAttribResp = null;
            if(null != mediaRespAv && null != (ssrcAttribResp  = mediaRespAv.findSSRCAttrib(tag))) {
                tagValue = ssrcAttribResp.getAttribPair().getValue();
            }
            mediaResp.addSSRCAttrib(new SDP.SSRCAttrib(ssrc,
                                   new SDP.AttribPair(ssrcAttrib.getAttribPair().getKey(), tagValue)), false);
        }

        return ssrc;
    }

    private void updateMediaSSRC(final SDP.Media mediaReq, SDP.Media mediaResp, SDP.Media mediaRespAv) {
        SDP.SSRCAttrib ssrcAttrib;
        long ssrc = 0;

        String tagValueCname = KeyGenerator.toBase64(KeyGenerator.getWeakRandomBytes(12));
        String tagValueMsid1 = KeyGenerator.toBase64(KeyGenerator.getWeakRandomBytes(24));
        String tagValueMsid2 = KeyGenerator.toBase64(KeyGenerator.getWeakRandomBytes(24));

        ssrc = updateMediaSSRCElement("cname", ssrc, tagValueCname, mediaReq, mediaResp, mediaRespAv);
        ssrc = updateMediaSSRCElement("msid", ssrc, tagValueMsid1 + " " + tagValueMsid2, mediaReq, mediaResp, mediaRespAv);
        //ssrc = updateMediaSSRCElement("mslabel", ssrc, tagValueMsid1, mediaReq, mediaResp, mediaRespAv);
        //ssrc = updateMediaSSRCElement("label", ssrc, tagValueMsid2, mediaReq, mediaResp, mediaRespAv);

    }

    private void updateMediaSDP(final SDP.Media mediaReq, 
                                SDP.Media mediaResp, 
                                ConferenceDefinition conferenceDefinition, 
                                boolean bIsVideo)
            throws NoSuchAlgorithmException, CloneNotSupportedException, SecurityException {

        ConferenceDefinition.SRTP_POLICY srtpPolicy = ConferenceDefinition.SRTP_POLICY_DEFAULT; 
        SDP.SRTPCrypto crypto = null;

        if(null != conferenceDefinition) {
            srtpPolicy = conferenceDefinition.getSRTPPolicy();
        }

        m_log.debug("Conference definition " + (null != conferenceDefinition ? conferenceDefinition.getConferenceName() : "") + 
             " has SRTP policy: " + srtpPolicy + " for " + mediaReq.getMediaTypeAsString() + ".  Request SDP " + 
             mediaReq.getTransportType() + " has " + mediaReq.getCryptoCount() + " crypto keys.");

        //
        // Set response RTCP parameters
        //
        updateMediaRTCP(mediaReq, mediaResp, bIsVideo);

        if(mediaReq.getCryptoCount() > 0 || mediaReq.getFingerprint().isSet()) {

            boolean bHaveFingerprint = false;
            boolean bHaveCrypto = false;

            if(srtpPolicy == ConferenceDefinition.SRTP_POLICY.NEVER) {
                m_log.warn("Conference definition in " + conferenceDefinition.getConferenceName() + 
                            " prohibits SRTP for " + mediaReq.getMediaTypeAsString());
            } else {

                if(mediaReq.getFingerprint().isSet() && null != m_dtlsCertFingerprint && conferenceDefinition.isAllowDTLS() &&
                      (mediaReq.getCryptoCount() == 0 || conferenceDefinition.isPreferDTLSSRTP() || mediaResp.isRequest())) {
                    updateSDPFingerprint(mediaReq, mediaResp, srtpPolicy);
                    bHaveFingerprint = true;
                }

                //if((mediaResp.isRequest() || false == bHaveFingerprint) && mediaReq.getCryptoCount() > 0) {
                if((false == bHaveFingerprint) && mediaReq.getCryptoCount() > 0) {
                    updateSDPCrypto(mediaReq, mediaResp, srtpPolicy);
                    bHaveCrypto = true;
                } 

                if(false == bHaveFingerprint && false == bHaveCrypto) {

                    if(mediaReq.getFingerprint().isSet() && null == m_dtlsCertFingerprint) {
                        m_log.warn("DTLS has not been enabled or configured for SDP response");
                    } else {
                        m_log.warn("Unable to provide SRTP in local SDP");
                    }
                }

            }
        } 

        boolean isRespSecureToken = false;
        if(mediaResp.getCryptoCount() > 0 || mediaResp.getFingerprint().isSet()) {
            isRespSecureToken = true;
        }

        //
        // Set the response SDP RTP/AVP type
        //
        mediaResp.setTransportType(SDP.createTransportType(isRespSecureToken, mediaReq.isMediaAVPF()));

        if(false == mediaResp.getTransportType().equals(mediaReq.getTransportType())) {
            if(mediaReq.isMediaTransportSecure() && false == mediaResp.isMediaTransportSecure()) {
                m_log.warn("SDP response transport type " + mediaResp.getTransportType() + " for " +  
                   mediaReq.getMediaTypeAsString() + " does not match peer SDP transport type " + 
                   mediaReq.getTransportType());
            } else {
                m_log.info("Using SDP " + mediaReq.getMediaTypeAsString() + " response transport type " + 
                    mediaResp.getTransportType() + " for peer SDP transport type" + mediaReq.getTransportType());
            }
        }

        if(mediaResp.isMediaTransportSecure() && false == isRespSecureToken) {
           m_log.warn("SDP response for " + mediaResp.getTransportType() + " is missing security for " + 
                       conferenceDefinition.getConferenceName() + " for " + mediaResp.getMediaTypeAsString());
        }

        //
        // Check if SRTP is required to join the conference
        //
        if((ConferenceDefinition.SRTP_POLICY.REQUIRED == srtpPolicy)  && 
           (false == mediaResp.isMediaTransportSecure() ||
           (false == mediaResp.getFingerprint().isSet() &&
           (null == (crypto = mediaResp.getCrypto()) || false == crypto.haveConfidentiality() ||
            false == crypto.haveAuthentication())))) {

          //m_log.debug("media resp:" + mediaResp.getTransportType() + ", crypto:" + crypto + " conf:" + (null !=crypto ? crypto.haveConfidentiality() : "null") + ", auth:" + (null != crypto ? crypto.haveAuthentication() : "null"));
            String strError = "Conference definition " + conferenceDefinition.getConferenceName() + 
                              " requires SRTP for " + mediaResp.getMediaTypeAsString();
            m_log.error(strError);
            throw new SecurityException(strError);
        }

    }

    private void updateSDPBandwidth(SDP sdpRemote, SDP sdpLocal, ConferenceDefinition conferenceDefinition) {
        SDP.Media videoRemote = null, videoLocal = null;
        SDP.MediaCandidate videoCandidate = sdpLocal.getVideoCandidate();

        if(null != (videoRemote = sdpRemote.getVideoMedia()) && null != (videoLocal = sdpLocal.getVideoMedia()) &&
           null != (videoCandidate = sdpLocal.getVideoCandidate())) {

            long videoBps = 0;

            IEncoderConfig encoderConfig = conferenceDefinition.getEncoderConfigLoader().getEncoderConfig( 
                                                                                   videoCandidate.getCodec(), 
                                                                                   sdpRemote.getDeviceType(), 
                                                                                   m_namingI);
            if(null != encoderConfig) {
                if(encoderConfig.getVideoBitrateMax() > 0) {
                    videoBps = encoderConfig.getVideoBitrateMax();
                } else if(encoderConfig.getVideoBitrate() > 0) {
                    videoBps = encoderConfig.getVideoBitrate();
                }
                videoBps *= 1050;
            }
            if(videoBps > 0) {
                videoLocal.getBandwidth().setTIAS(videoBps);
            }
        }

    }

    private void updateSDP(SDP sdpRemote, SDP sdpLocal, ConferenceDefinition conferenceDefinition) 
            throws UnknownHostException, NoSuchAlgorithmException, CloneNotSupportedException, SecurityException {

        SDP.Media videoRemote = null, videoLocal = null;
        SDP.Media audioRemote = null, audioLocal = null;
        boolean bPreferIceHost = false;
        boolean bEnableTURN = false;

        if(null != conferenceDefinition) {

            //
            // If ConferenceDefinition.CONFIG_ICE_HOST is set, only look at the host candidates
            // to enable localhost testing
            //
            if(conferenceDefinition.isPreferIceHost()) {
                bPreferIceHost = true;
                m_log.debug("Using ICE candidate lookup preferred host instead of server or peer rflx");
            }

            if(conferenceDefinition.isTURNRelayEnabled()) {
                bEnableTURN = true;
                m_log.debug("ICE candidate TURN relay enabled");
            }
        }

        //
        // Explicitly disable DTLS if the certificate fingerprint has not been loaded
        //
        if(null == m_dtlsCertFingerprint && conferenceDefinition.isAllowDTLS()) {
            conferenceDefinition.disableDTLS();
        }

        if(null != (videoRemote = sdpRemote.getVideoMedia()) && null != (videoLocal = sdpLocal.getVideoMedia())) {

            updateMediaSDP(videoRemote, videoLocal, conferenceDefinition, true);
            updateMediaSSRC(videoRemote, videoLocal, null);
            updateMediaICE(videoRemote, videoLocal, null, true, conferenceDefinition, bPreferIceHost, bEnableTURN);

            if(false == videoRemote.getTarget().isValid() || videoRemote.getTarget().getPort() <= 1024) {
                sdpLocal.invalidateVideo();
                String err = "SDP video request contains invalid target address: " + videoRemote.getTarget().toString(); 
                throw new UnknownHostException(err);
            }

            //TODO: we need to support FEC, payload redundancy
            //Codec codec;
            //if(null != (codec = videoRemote.findCodec("red"))) videoLocal.addCandidate(codec);
            //if(null != (codec = videoRemote.findCodec("ulpfec"))) videoLocal.addCandidate(codec);

        }

        if(null != (audioRemote = sdpRemote.getAudioMedia()) && null != (audioLocal = sdpLocal.getAudioMedia())) {

            updateMediaSDP(audioRemote, audioLocal, conferenceDefinition, false);
            updateMediaSSRC(audioRemote, audioLocal, videoLocal);
            //videoLocal.setIsICESameAudioVideo(true); // Maintain same STUN/ICE credentials across channels (firefox)
            updateMediaICE(audioRemote, audioLocal, videoLocal, true, conferenceDefinition, bPreferIceHost, bEnableTURN);
            updateMediaDTMF(audioRemote, audioLocal);

            
            //
            // We're still only running a mono audio mixer, so only request 1 audio channel
            //
            //if(sdpLocal.getAudioCandidate().getCodec().getChannelCount() > 1) {
            //    sdpLocal.getAudioCandidate().getCodec().setChannelCount(1);
            //}

            if(false == audioRemote.getTarget().isValid() || audioRemote.getTarget().getPort() <= 1024) {
                sdpLocal.invalidateAudio();
                String err = "SDP audio request contains invalid target address: " + audioRemote.getTarget().toString();
                throw new UnknownHostException(err);
            }

        }
/*
if(null != conferenceDefinition && conferenceDefinition.isContactHeaderAsTarget()) {
            //TODO: get Invite Contact Header domain portion and use that as RTP target, such as when STUN is not enabled in client
            SDP.ICEAddress target = audioRemote.getTarget().clone();
t.setAddress(InetAddress.getByName("127.0.0.1"));
sdpReq.getAudioMedia().setTarget(t);
SDP.ICEAddress t2 = sdpReq.getAudioMedia().getTargetRTCP().clone();
t2.setAddress(InetAddress.getByName("127.0.0.1"));
sdpReq.getAudioMedia().setTargetRTCP(t2);

        }
*/

 
        //
        // Add the a=group BUNDLE session attribute when video & audio channels are muxed
        //
        if(sdpLocal.getAudioCandidatesCount() > 0 && sdpLocal.getVideoCandidatesCount() > 0 &&
           sdpLocal.getAudioMedia().getConnect().getPort() == sdpLocal.getVideoMedia().getConnect().getPort()) {
            sdpLocal.setIsBundle(true);
        }

        updateSDPBandwidth(sdpRemote, sdpLocal, conferenceDefinition);

    }

    /**
     * <p>Creates an SDP response document to an incoming SIP INVITE request</p>
     * <p>The call object argument should define the <i>SDPResponder.SDP_RESPONSE_TYPE</i> of response document which should be created</p> 
     * @param call The associated <i>Call</i> object of the request
     * @param sdpReq The SDP request document associated with the SIP INVITE request that the requested SDP document is a response for
     * @param videoPort 0, or a previously allocated video port for this response.  The port should be set if this invocation is for an updated response SDP document being requested in the case that the original request SDP has changed.
     * @param audioPort 0, or a previously allocated audio port for this response.  The port should be set if this invocation is for an updated response SDP document being requested in the case that the original request SDP has changed.
     * @return An SDP response document
     * @throws Exception on an error condition
     */
    public SDP getResponseSDP(Call call, SDP sdpReq, int videoPort, int audioPort) throws Exception {
        SDP sdpResp = null;
        SDP_RESPONSE_TYPE type = call.getSDPResponseType();
    
        switch(type) {

            case SDP_RESPONSE_BEST_MATCH:
                sdpResp = getResponseSDPBestMatch(call, sdpReq, videoPort, audioPort);
                break;

            case SDP_RESPONSE_ALL_SUPPORTED:
                sdpResp = getAllSupportedSDP(call, sdpReq, videoPort, audioPort);
                break;

            case SDP_RESPONSE_MULTIPLE_MATCH:
                sdpResp = getResponseSDPMultipleMatch(call, sdpReq, videoPort, audioPort);
                break;
        
        }

        //
        // Since we're manually creating the response SDP make sure the media target RTP/RTCP addresses are not null
        //
        sdpResp.updateTargets();

        //
        // Set SDP properties specific for ICE, DTMF, rtcp-mux, SSRC, etc...
        //
        updateSDP(sdpReq, sdpResp, call.getConferenceDefinition());

        return sdpResp;
    }

    /**
     * <p>Creates an SDP request document for an outgoing SIP INVITE request</p>
     * <p>The call object argument should define the <i>SDPResponder.SDP_RESPONSE_TYPE</i> of response document which should be created</p> 
     * @param call The associated <i>Call</i> object of the request
     * @param deviceType The deviceType the SDP is intended for
     * @param videoPort 0, or the video port if it has been previously allocated for this response.  The port should be set if this invocation is for an updated response SDP if the original request SDP has changed.
     * @param audioPort 0, or the audio port if it has been previously allocated for this response.  The port should be set if this invocation is for an updated response SDP if the original request SDP has changed.
     * @return An SDP response document
     * @throws Exception on an error condition
     */
    public SDP getRequestSDP(Call call, DeviceType deviceType, int videoPort, int audioPort) 
                   throws Exception {
        SDP sdpReq;
        SDP sdpDummy;
        boolean bShowSSRC = false;
        boolean bBundlePorts = false;
        boolean bRtcpMux = false;
        boolean bCrypto = false;
        boolean bFingerprint = false;
        boolean bAVPF = false;
        boolean bIce = true;

        if(DeviceTypeWebRTC.isWebRTC(deviceType.getType())) {
            bShowSSRC = false;
            bBundlePorts = true;
            bRtcpMux = true;
            bIce = true;
            bCrypto = true;
            bFingerprint = true;
            bAVPF = true;
        }

        if(videoPort <= 0 && audioPort <= 0) {
            if(bBundlePorts) {
                videoPort = audioPort = -1;
            } else {
                videoPort = -1;
                audioPort = -2;
            }
        }

        sdpReq = getAllSupportedSDP(call, null, videoPort, audioPort);

        String tagValueCname = KeyGenerator.toBase64(KeyGenerator.getWeakRandomBytes(12));
        String tagValueMsid1 = KeyGenerator.toBase64(KeyGenerator.getWeakRandomBytes(24));
        String tagValueMsid2 = KeyGenerator.toBase64(KeyGenerator.getWeakRandomBytes(24));
        String iceFoundation = Integer.toString(Math.abs(new Random().nextInt()));
        long priorityRtp = 2113994751;  // 0x7e(host) << 24 | 0xff(ipv4) < 8 | 0xff(256-component))  
        long priorityRtcp = 2113994751;  // 0x7e(host) << 24 | 0xff(ipv4) < 8 | 0xff(256-component))  

        sdpDummy = sdpReq.clone();

        if(sdpDummy.getAudioCandidatesCount() > 0) {

            if(bIce) {
                sdpDummy.getAudioMedia().addICECandidate(new SDP.ICECandidate(iceFoundation, 
                         SDP.ICECandidate.ICE_COMPONENT_RTP, 
                         SDP.ICECandidate.TRANSPORT.udp,
                         priorityRtp,
                         new SDP.ICEAddress(m_listenAddress, sdpDummy.getAudioMedia().getConnect().getPort()),
                         SDP.ICECandidate.TYPE.host), false);

                sdpDummy.getAudioMedia().addICECandidate(new SDP.ICECandidate(iceFoundation, 
                         SDP.ICECandidate.ICE_COMPONENT_RTCP, 
                         SDP.ICECandidate.TRANSPORT.udp,
                         priorityRtp,
                         new SDP.ICEAddress(m_listenAddress, sdpDummy.getAudioMedia().getConnect().getPort()),
                         SDP.ICECandidate.TYPE.host), false);
            }

            if(bShowSSRC) { 
                sdpDummy.getAudioMedia().addSSRCAttrib(new SDP.SSRCAttrib(0, 
                                           new SDP.AttribPair("msid", tagValueCname)), false);
                sdpDummy.getAudioMedia().addSSRCAttrib(new SDP.SSRCAttrib(0, 
                                           new SDP.AttribPair("cname", tagValueMsid1 + tagValueMsid2)), false);
            }

            if(bRtcpMux) {
                sdpDummy.getAudioMedia().setRtcpMux(true);
            }


            if(bCrypto) {
                sdpDummy.getAudioMedia().addCrypto(new SDP.SRTPCrypto("1", 
                                               SDP.SRTPCrypto.SRTPAlgorithm.AES_CM_128_HMAC_SHA1_80.toString(), 
                KeyGenerator.toBase64(new KeyGenerator().getWeakRandomBytes(SDP.SRTPCrypto.KEY_LENGTH))), false);
            }

            if(bFingerprint && null != m_dtlsCertFingerprint) {
                sdpDummy.getAudioMedia().getFingerprint().set(m_dtlsCertFingerprint);
            }

            sdpDummy.getAudioMedia().setTransportType(SDP.createTransportType(
                        sdpDummy.getAudioMedia().getCryptoCount() > 0 ? true : false, bAVPF));

        }

        if(sdpDummy.getVideoCandidatesCount() > 0) {

            if(bIce) {
                sdpDummy.getVideoMedia().addICECandidate(new SDP.ICECandidate(iceFoundation, 
                         SDP.ICECandidate.ICE_COMPONENT_RTP, 
                         SDP.ICECandidate.TRANSPORT.udp,
                         priorityRtp,
                         new SDP.ICEAddress(m_listenAddress, sdpDummy.getAudioMedia().getConnect().getPort()),
                         SDP.ICECandidate.TYPE.host), false);

                sdpDummy.getVideoMedia().addICECandidate(new SDP.ICECandidate(iceFoundation, 
                         SDP.ICECandidate.ICE_COMPONENT_RTCP, 
                         SDP.ICECandidate.TRANSPORT.udp,
                         priorityRtp,
                         new SDP.ICEAddress(m_listenAddress, sdpDummy.getAudioMedia().getConnect().getPort()),
                         SDP.ICECandidate.TYPE.host), false);
            }

            if(bShowSSRC) { 
                sdpDummy.getVideoMedia().addSSRCAttrib(new SDP.SSRCAttrib(0, 
                                                   new SDP.AttribPair("msid", tagValueCname)), false);
                sdpDummy.getVideoMedia().addSSRCAttrib(new SDP.SSRCAttrib(0, 
                                           new SDP.AttribPair("cname", tagValueMsid1 + tagValueMsid2)), false);
            }

            if(bRtcpMux) {
                sdpDummy.getVideoMedia().setRtcpMux(true);
            }

            if(bCrypto) {
                sdpDummy.getVideoMedia().addCrypto(new SDP.SRTPCrypto("1", 
                                               SDP.SRTPCrypto.SRTPAlgorithm.AES_CM_128_HMAC_SHA1_80.toString(), 
                KeyGenerator.toBase64(new KeyGenerator().getWeakRandomBytes(SDP.SRTPCrypto.KEY_LENGTH))), false);
            }

            if(bFingerprint && null != m_dtlsCertFingerprint) {
                sdpDummy.getVideoMedia().getFingerprint().set(m_dtlsCertFingerprint);
            }

            sdpDummy.getVideoMedia().setTransportType(SDP.createTransportType(
                        sdpDummy.getVideoMedia().getCryptoCount() > 0 ? true : false, bAVPF));
        }

        //
        // Since we're manually creating the response SDP make sure the media target RTP/RTCP addresses are not null
        //
        sdpReq.updateTargets();
        sdpReq.setIsRequest(true);

        //
        // Set SDP properties specific for ICE, DTMF, rtcp-mux, SSRC, etc...
        //
        updateSDP(sdpDummy, sdpReq, call.getConferenceDefinition());

        SDP.Fingerprint fingerprint = null;

        if((sdpReq.getAudioCandidatesCount() > 0 &&
            (sdpReq.getVideoCandidatesCount() == 0 || (null != (fingerprint = sdpReq.getAudioMedia().getFingerprint()) && fingerprint.equals(sdpReq.getVideoMedia().getFingerprint())))) || 
           (sdpReq.getVideoCandidatesCount() > 0 &&
            (sdpReq.getAudioCandidatesCount() == 0 || (null != (fingerprint = sdpReq.getVideoMedia().getFingerprint()) && fingerprint.equals(sdpReq.getAudioMedia().getFingerprint()))))) {
            sdpReq.getFingerprint().set(fingerprint);
        }

        return sdpReq;
    }

    private void pruneMediaCandidates(SDP.Media mediaPrune, final SDP.Media media, boolean bAllowMultiple) {

        SDP.MediaCandidate [] candidatesPrune = mediaPrune.toArray();
        SDP.MediaCandidate [] candidates = media.toArray();
        int indexPrune, index;
        boolean bFound;


        if(null != candidatesPrune) {

            for(indexPrune = 0; indexPrune < candidatesPrune.length; indexPrune++) {

                Codec codec = candidatesPrune[indexPrune].getCodec();
                bFound = false;

                for(index = 0; (bAllowMultiple && index < candidates.length) || 0 == index; index++) {
                    Codec candidateCodec = candidates[index].getCodec();

                    if(true == codec.getName().equalsIgnoreCase(candidateCodec.getName()) &&
                       codec.getClockRate() == candidateCodec.getClockRate() &&
                       codec.getChannelCount() == candidateCodec.getChannelCount()) {

                        bFound = true;
                        break;
                    }
                }

                if(false == bFound) {
                    mediaPrune.removeCandidate(candidatesPrune[indexPrune]);
                }
            }
        }
    }

    /**
     * <p>Updates a local SDP request document which was previously used in an INVITE request. The update should occur after the remote party's SDP response document has been received in a 200OK response to the INVITE request.</p> 
     * <p>The update prunes away unused media candidates.</p>
     * @param sdpReq The original SDP request document sent to the remote party
     * @param sdpResp The SDP response doument received from the remote party
     * @param conferenceDefinition The conference definition of the conference
     * @param bAllowMultiple Set to <i>true</i> to allow multiple media candidate variations  matching the same codec, otherwise <i>false</i>.
     * @return The updated SDP request document
     */
    public SDP updateRequestSDP(SDP sdpReq, final SDP sdpResp, ConferenceDefinition conferenceDefinition, 
                                boolean bAllowMultiple) throws Exception {
        SDP sdpUpdated = null;
        boolean bPreferIceHost = false;
        boolean bEnableTURN = false;

        sdpUpdated = sdpReq.clone();

        //
        // If ConferenceDefinition.CONFIG_ICE_HOST is set, only look at the host candidates
        // to enable localhost testing
        //
        if(null != conferenceDefinition && conferenceDefinition.isPreferIceHost()) {
            bPreferIceHost = true;
            m_log.debug("Using ICE candidate lookup preferred host instead of server or peer rflx");
        }

        // Prune the offered audio candidates
        if(sdpUpdated.getAudioCandidatesCount() > 0) {

            if(sdpResp.getAudioCandidatesCount() <= 0) {
                sdpUpdated.invalidateAudio();
            } else {
                pruneMediaCandidates(sdpUpdated.getAudioMedia(), sdpResp.getAudioMedia(), bAllowMultiple);
            }

        }

        if(sdpUpdated.getAudioCandidatesCount() > 0) {
            updateMediaICE(sdpResp.getAudioMedia(), sdpUpdated.getAudioMedia(), null, false, 
                           conferenceDefinition, bPreferIceHost, bEnableTURN);

            if(null != sdpUpdated.getAudioMedia().getCrypto() && null != sdpResp.getAudioMedia()) {
                sdpUpdated.getAudioMedia().getCrypto().setPeer(sdpResp.getAudioMedia().getCrypto());
            }
            if(sdpUpdated.getAudioMedia().getFingerprint().isSet() && null != sdpResp.getAudioMedia()) {
                sdpUpdated.getAudioMedia().getFingerprint().setPeer(sdpResp.getAudioMedia().getFingerprint());
            }
        }

        // Prune the offered video candidates
        if(sdpUpdated.getVideoCandidatesCount() > 0) {

            if(sdpResp.getVideoCandidatesCount() <= 0) {
                sdpUpdated.invalidateVideo();
            } else {
                pruneMediaCandidates(sdpUpdated.getVideoMedia(), sdpResp.getVideoMedia(), bAllowMultiple);
            }

        }

        if(sdpUpdated.getVideoCandidatesCount() > 0) {
            updateMediaICE(sdpResp.getVideoMedia(), sdpUpdated.getVideoMedia(), null, false, 
                           conferenceDefinition, bPreferIceHost, bEnableTURN);

            if(null != sdpUpdated.getVideoMedia().getCrypto() && null != sdpResp.getVideoMedia()) {
                sdpUpdated.getVideoMedia().getCrypto().setPeer(sdpResp.getVideoMedia().getCrypto());
            }
            if(sdpUpdated.getVideoMedia().getFingerprint().isSet() && null != sdpResp.getVideoMedia()) {
                sdpUpdated.getVideoMedia().getFingerprint().setPeer(sdpResp.getVideoMedia().getFingerprint());
            }
        }


        sdpUpdated.updateTargets();

        return sdpUpdated;
    }
/*
    public static void main(String [] args) {

        //java.util.Map<String, String> env = System.getenv();
        //for (String envName : env.keySet()) {
        //    System.out.format("%s=%s%n",
        //                      envName,
        //                      env.get(envName));
        //}

        try {


            Configuration config =  new Configuration("../../../test/debug-ngconference.conf"); 
            XCoderMediaProcessor ng = new XCoderMediaProcessor (config);
            SDPResponder re = new SDPResponder(ng, config);

            ConferenceDefinition confDef = new ConferenceDefinition("../../../conferences/", "fromhere@from.com", "1000@to.com");
            Call call = new Call(new User((javax.servlet.sip.Address)null), new User((javax.servlet.sip.Address)null)); 
            call.setPortManager(new PortManager(4000,5000, 2));
            call.setConferenceDefinition(confDef);

            Conference c = new Conference(null, ConferenceDefinition.Name.createTestOnly("my conf"), 1000, confDef); 

            //SDP sdpInvite = re.getRequestSDP(call, new DeviceType(WEBRTC_CHROME), 0, 0);
            //System.out.println(sdpInvite.serialize());
            //SDP sdp200OK = SDP.create(new String(sdpResp2All.getBytes()).trim());
            //SDP sdpInviteUpdated = re.updateRequestSDP(sdpInvite, sdp200OK, call.getConferenceDefinition(), false);
            //System.out.println(sdpInviteUpdated.serialize());


            SDP sdpReq = SDP.create(new String(com.openvcx.test.SDPTest.sdpWebRTCTURN.getBytes()).trim());
            System.out.println(sdpReq.serialize());

            SDP sdpResp = re.getResponseSDP(call,sdpReq,0,0);
            System.out.println(sdpResp.serialize());
*/
/*
            //SDP.ICERemoteCandidates rc = sdpReq.getAudioMedia().getICERemoteCandidates();
            //SDP.ICEAddress [] addr = rc.getAddressArray();
            //int index;
            //for(index=0; index < addr.length; index++) {
            //    System.out.println(addr[index].getPort());
            //}
            //System.out.println(rc);

            //SDP sdpResp = re.getResponseSDP(call, sdpReq, 0, 0);
            //sdpResp.getAudioMedia().getICERemoteCandidates().add(new SDP.ICERemoteCandidate(5, new SDP.ICEAddress(InetAddress.getByName("127.0.0.1"), 1234)), false);
            //System.out.println("\n\n\n\n" + sdpResp.serialize());

//            ng.addConferenceParticipant(c, sdpResp, sdpReq, "myuser");

*/

/*
            SDP sdp = new SDP();
            sdp.setSession("test");
            sdp.setAudioPort(2000);
            sdp.setAudioAddress("127.0.0.1");
            sdp.addAudioCandidate(new Codec("PCMA", 8000, 8));
            sdp.addAudioCandidate(new Codec("telephone-event", 8000, 101),  "0-15");

            sdp.setVideoPort(2002);
            //sdp.setVideoAddress("127.0.0.3");
            sdp.addVideoCandidate(new Codec("H264", 90000, 97), "profile-level-id=42800c; test");

            SDP sdp2 = (SDP) sdp.clone();
            sdp2.setSession("session2");
            //sdp2.addVideoAttribute("sendrecv - session2");
            sdp2.setAudioAddress("192.168.1.1");
            sdp2.setAudioSendRecvType(SDP.SDPType.SDP_TYPE_RECVONLY);
            sdp2.setVideoSendRecvType(SDP.SDPType.SDP_TYPE_RECVONLY);
            sdp.setAudioAddress("127.0.0.2");
            SDP.MediaCandidate mc = sdp2.getVideoCandidate();
            mc.m_fmtp = "sdp2 fmtp";
            mc.getCodec().setName("test codec");
            mc.getCodec().setPayloadType(54);

            System.out.println(sdp.serialize());
            System.out.println("\n\n" + sdp2.serialize());
           

            //SDP sdpResp = ng.getResponseSDP(sdp);
            //System.out.println(sdpResp.serialize());
*/
/* 
        } catch(Exception e) {
            e.printStackTrace();
        }
    }
*/

}
