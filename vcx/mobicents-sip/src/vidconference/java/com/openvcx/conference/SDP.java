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
import java.lang.Cloneable;
import java.lang.CloneNotSupportedException;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.util.Iterator;
import java.util.ArrayList;
import java.util.List;

import org.apache.log4j.Logger;

/**
 *
 * Session Description Protocol Document handler and parser 
 *
 */
public class SDP implements Cloneable {

    private final Logger m_log = Logger.getLogger(getClass());

    private String m_version;
    private String m_owner;
    private String m_session;
    private String m_time;
    private List<String> m_listSessionAttr = new ArrayList<String>();
    private boolean m_bIsBundle;
    private Media m_video;
    private Media m_audio;
    private ICEProperties m_iceProperties;
    private Bandwidth m_bandwidth;
    private Fingerprint m_fingerprint;
    private ParseState m_state;
    private boolean m_bIsRequest;
    private DeviceType m_deviceType;
    private String m_rawDocument;

    private static final String TRANSPORT_IP_DEFAULT = "IN IP4";
    private static final String NEWLINE = "\r\n";

    private static final String TRANSPORT_RTP_AVP = "RTP/AVP";
    private static final String TRANSPORT_RTP_AVPF = "RTP/AVPF";
    private static final String TRANSPORT_RTP_SAVP = "RTP/SAVP";
    private static final String TRANSPORT_RTP_SAVPF = "RTP/SAVPF";


    /**
     *
     * Constructor used to initialize the instance
     *
     */
    public SDP() {
        Reset();
    }

    /**
     *
     * Resets the contents of the SDP
     *
     */
    public void Reset() {
        m_version = null;
        m_owner = null;
        m_session = null;
        m_time = null;
        m_listSessionAttr.clear();
        m_bIsBundle = false;
        m_video = null;
        m_audio = null;
        m_iceProperties = new ICEProperties();
        m_bandwidth = new Bandwidth();
        m_fingerprint = new Fingerprint();
        m_state = new ParseState(); 
        m_bIsRequest = false;
        m_deviceType = new DeviceType();
        m_rawDocument = null;
    }

    /**
     *
     * <p>SDP Media Candidate</p>
     * <p>An example of an SDP Media candidate attribute field</p>
     * <p><i>a=rtpmap:0 PCMU/8000</i></p>
     *
     */
    public static class MediaCandidate implements Cloneable {
        String m_rtpmap;
        Codec m_codec;

        /**
         * Retrieves the codec associated with this canddiate
         * @return The codec associated with this canddiate
         */
        public Codec getCodec() {
            return null != m_codec ? m_codec : new Codec(); 
        }

        /**
         * Creates a copy of the instance
         * @return A copy of the instance
         */
        public MediaCandidate clone() throws CloneNotSupportedException {
            MediaCandidate candidate = (MediaCandidate) super.clone();
            candidate.m_codec = m_codec.clone();
            return candidate;
        }

        private MediaCandidate(int payloadType) {
            if(null == m_codec) {
                m_codec = new Codec();
            }
            m_codec.setPayloadType(payloadType);
        }

        private MediaCandidate(Codec codec) throws CloneNotSupportedException {
            m_codec = codec.clone();
            m_rtpmap = codec.getRtpmapCodecDescriptor();
        }

        private void parseRtpmap(String rtpmap) {
            m_rtpmap = MediaCandidate.getPayloadDescription(rtpmap);
            String strType = MediaCandidate.getLineField(m_rtpmap, 0);
            String [] arr = strType.split("/");

            if(arr.length > 0) {

                if(null == m_codec) {
                    m_codec = CodecFactory.create(arr[0].trim());
                } else {
                    m_codec = CodecFactory.create(arr[0].trim(), m_codec.getPayloadType());
                }

                if(arr.length > 1) {
                    m_codec.setClockRate(Integer.parseInt(arr[1].trim())); 
                }

                if(arr.length > 2) {
                    m_codec.setChannelCount(Integer.parseInt(arr[2].trim())); 
                }
            }
        }

        private static int extractPayloadType(String value) throws IOException {
            String field = getLineField(value, 0);
            int match = field.indexOf(':');
            return Integer.parseInt(field.substring(match + 1));
        }

        private static String getLineField(String value, int index){
           String [] arr = value.split(" ");
           if(index > arr.length) {
               return null;
           } else {
               return arr[index].trim();
           }
        }

        private static String getPayloadDescription(String value) {
           int match = value.indexOf(' ');
           if(match >= 0) {
               return value.substring(match + 1).trim();
           } else {
               return null;
           }
        }

        /**
         * Returns a serialized string version of the element which can be included as an SDP atribute field
         * @return A serialized string version of the element which can be included as an SDP atribute field
         */
        @Override
        public String toString() {
            return "rtpmap: " + m_rtpmap + ", codec: " + m_codec;
        }

    }

    /**
     *
     * SDP two-way transmission type
     *
     */
    public enum SDPType {
        /**
         * Defines an SDP which should use media transmission and reception
         */ 
        SDP_TYPE_SENDRECV,

        /**
         * Defines an SDP which should use media transmission only 
         */ 
        SDP_TYPE_SENDONLY,

        /**
         * Defines an SDP which should use media reception only 
         */ 
        SDP_TYPE_RECVONLY,

        /**
         * Defines an SDP uninitialized transmission type
         */ 
        SDP_TYPE_NONE
    }

    /**
     *
     * SDP media type
     *
     */
    public enum MediaType {
    
        /**
         * SDP unknown media type description
         */
        MEDIA_TYPE_UNKNOWN,

        /**
         * SDP audio media type description
         */
        MEDIA_TYPE_AUDIO,

        /**
         * SDP video media type description
         */
        MEDIA_TYPE_VIDEO
    }

    /**
     *
     * SDP General Attribute field
     *
     */
    public static class AttribPair extends Pair<String, String> implements Cloneable {

        /**
         * Constructor used to initialize the instance
         * @param key The attribute key
         * @param value The attribute value
         */
        public AttribPair(String key, String value) {
            super(key, value);
        }

        /**
         * Creates a copy of the instance
         * @return A copy of the instance
         */
        public AttribPair clone() throws CloneNotSupportedException {
            return (AttribPair) super.clone();
        }

        /**
         * Parses an SDP attribute line
         */
        private static AttribPair parse(String line) {
            String key = null;
            String value = null;
            int match = -1;
            if(null != line) {
                match = line.indexOf(':');
            }
            if(match >= 0) {
              key = line.substring(0, match).trim();
              value = line.substring(match + 1).trim();
            } else {
              key = line;
            }
            return new AttribPair(key, value);
        }
    }               

    /**
     *
     * <p>SDP media SSRC descriptor attribute.</p>
     * <p>An example of an SDP SSRC attribute field</p>
     * <p><i>a=ssrc:556784522 cname:02pXsDmY1B93jiWK</i></p>
     */
    public static class SSRCAttrib implements Cloneable {
        long m_ssrc;
        AttribPair m_attribPair;

        /**
         * Constructor used to initialize this instance
         * @param ssrc The SSRC value
         * @param attribPair The RTP SSRC attribute as a parsed <i>AttribPair</i>
         */
        public SSRCAttrib(long ssrc, AttribPair attribPair) {
            m_ssrc = ssrc;
            m_attribPair = attribPair;
        }

        /**
         * Retrieves the SSRC value 
         * @return The SSRC value 
         */
        public long getSSRC() {
            return m_ssrc;
        }

        /**
         * Retrieves the <i>AttribPair</i> element used to initialize this instance
         * @return The <i>AttribPair</i> element used to initialize this instance
         */
        public final AttribPair getAttribPair() {
            return m_attribPair;
        }

        /**
         * Parses and creates an <i>SSRCAttrib</i> instance given an SDP SSRC attribute field
         * @param line An SDP SSRC attribute field
         * @return An initialized <i>SSRCAttrib</i> instance
         */
        private static SSRCAttrib parse(String line) {
            int index = 0;
            line = line.substring(5);
            int match = line.indexOf(' ');
            long ssrc = 0;
            if(match > 0) {
                ssrc = Long.valueOf(line.substring(0, match));
                line = line.substring(match + 1).trim();
            } else {
                ssrc = Long .valueOf(line);
                line = null;
            }
            
            return new SSRCAttrib(ssrc, AttribPair.parse(line));
        }

        /**
         * Creates a copy of the instance
         * @return A copy of the instance
         */
        public SSRCAttrib clone() throws CloneNotSupportedException {
            SSRCAttrib ssrcAttrib = (SSRCAttrib) super.clone();
            ssrcAttrib.m_attribPair = m_attribPair.clone();
            return ssrcAttrib;
        }

        /**
         * Returns a serialized string version of the element which can be included as an SDP atribute field
         * @return A serialized string version of the element which can be included as an SDP atribute field
         */
        private String serialize() {
            StringBuffer str = new StringBuffer();

            str.append("a=ssrc:" + m_ssrc);
            if(null != m_attribPair && null != m_attribPair.getKey()) {
                str.append(" " + m_attribPair.getKey());
                if(null != m_attribPair.getValue()) {
                    str.append(":" + m_attribPair.getValue());
                }
            }

            str.append(NEWLINE);

            return str.toString();
        }

        /**
         * Returns a serialized string version of the element which can be included as an SDP atribute field
         * @return A serialized string version of the element which can be included as an SDP atribute field
         */
        @Override 
        public String toString() {
            return serialize();
        }

    }

    /**
     *
     * <p>SDP media RTCP Feedback Type attribute.</p>
     *
     */
    public static class RTCPFB implements Cloneable {
        private TYPE m_type;
        private int m_payloadType;
        private boolean m_bAllPayloadTypes;

        /**
         * RTCP Feedback Types
         */
        public enum TYPE {
  
            /**
             * An unknown or unitialized value
             */
            UNKNOWN,

            /**
             * RFC 4585 Reference Pic Selection Indication Acknowledgement
             */
            ACK_RPSI,

            /**
             * RFC 4585 Application Specific Acknowledgement
             */
            ACK_APP,

            /**
             * RFC 4585 Generick Negative-Acknowledgement
             */
            NACK_GENERIC,

            /**
             * RFC 4585 Packet Loss Indication
             */
            NACK_PLI,

            /**
             * RFC 4585 Slice Loss Indication
             */
            NACK_SLI,

            /**
             * RFC 4585 Reference Pic Selection Indication Negative-Acknowledgement
             */
            NACK_RPSI,

            /**
             * RFC 4585 Applicaiton Specific Negative-Acknowledgement
             */
            NACK_APP,

            /**
             * RFC 4585 Minimum Interval
             */
            TRRINT,

            /**
             * RFC 5104 Codec Control Message Full Intra Refresh
             */
            CCM_FIR,

            /**
             * RFC 5104 Codec Control Message Temporary Max Media Bitrate
             */
            CCM_TMMBR,

            /**
             * RFC 5104 Codec Control Message Temporal Spatial Trade-off
             */
            CCM_TSTR,

            /**
             * RFC 5104 Codec Control Message Video Playback Channel Message
             */
            CCM_VBCM,

            /** 
             * WebRTC specific Receiver Estimated Maximum Bandwidth
             */ 
            GOOG_REMB,
        }


        /**
         * <p>Constructor used to initialize the instance.</p>
         * <p>The RTP payload type will not be set and the feedback type will pertain to all media payload types.</p> 
         * @param type The RTCP FB message type
         */
        public RTCPFB(TYPE type) {
            set(type, -1);
        }

        /**
         * Constructor used to initialize the instance
         * @param type The RTCP FB message type
         * @param payloadType The specific RTP payload type the feedback type pertains to
         */
        public RTCPFB(TYPE type, int payloadType) {
            set(type, payloadType);
        }

        private void set(TYPE type, int payloadType) {
            m_type = type;
            m_payloadType = payloadType;
            m_bAllPayloadTypes = m_payloadType < 0 ? true : false;
        }

        /**
         * Retrieves the RTCP FB type value
         * @return The RTCP FB type value
         */
        public TYPE getType() {
            return m_type;
        }

        /**
         * Retrieves the RTP payload type the RTCP FB type pertains to 
         * @return The RTP payload type.  <i>-1</i> is returned if the feedback type should pertain to all payload types of the given media. 
         */
        public int getPayloadType() {
            return m_payloadType;
        }

        /**
         * Sets the RTP payload type the RTCP FB type pertains to 
         * @param payloadType The RTP payload type.  This can be set to -1 to pertain to all RTP payload types of the given media.
         */
        public void setPayloadType(int payloadType) {
            m_payloadType = payloadType;
        }

        /**
         * Tests if the RTCP FB type pertains to all RTP payload types
         * @return <i>true</i> if <i>setPayloadType</i> was never called for this instance or the constructor used did not specify a specific RTP payload type value
         */
        public boolean isAllPayloadTypes() {
            return m_bAllPayloadTypes;
        }

        /**
         * Creates a copy of the instance
         * @return A copy of the instance
         */
        public RTCPFB clone() throws CloneNotSupportedException {
            return (RTCPFB) super.clone();
        }

        private String getFbTypeString() {
            
            switch(m_type) {
                case ACK_RPSI:
                case ACK_APP:
                    return "ack";

                case CCM_FIR:
                case CCM_TMMBR:
                case CCM_TSTR:
                case CCM_VBCM:
                    return "ccm";

                case NACK_GENERIC:
                case NACK_PLI:
                case NACK_SLI:
                case NACK_RPSI:
                case NACK_APP:
                    return "nack";
                case TRRINT:
                    return "trr-int";
                case GOOG_REMB:
                    return "goog-remb";
                default:
                    return "";
            }
        }
        private String getFbParamString() {
            
            switch(m_type) {
                case ACK_RPSI:
                case NACK_RPSI:
                    return "rpsi";
                case ACK_APP:
                case NACK_APP:
                    return "app";
                case NACK_SLI:
                    return "sli";
                case NACK_PLI:
                    return "pli";
                case NACK_GENERIC:
                    return "";
                case CCM_FIR:
                    return "fir";
                case CCM_TMMBR:
                    return "tmmbr";
                case CCM_TSTR:
                    return "tstr";
                case CCM_VBCM:
                    return "vbcm";
                case TRRINT:
                    return ""; // TODO
                default:
                    return "";
            }
        }

        private static TYPE toType(String type, String param) {
            
            if("ack".equals(type)) {
                
                if(null == param) {
                    return TYPE.UNKNOWN;
                } else if("rpsi".equals(param)) {
                    return TYPE.NACK_RPSI;
                } else if("app".equals(param)) {
                    return TYPE.NACK_APP;
                }
                
            } else if("nack".equals(type)) {
                
                if(null == param) {
                    return TYPE.NACK_GENERIC;
                } else if("pli".equals(param)) {
                    return TYPE.NACK_PLI;
                } else if("sli".equals(param)) {
                    return TYPE.NACK_SLI;
                } else if("rpsi".equals(param)) {
                    return TYPE.NACK_RPSI;
                } else if("app".equals(param)) {
                    return TYPE.NACK_APP;
                }
                
            } else if("ccm".equals(type)) {
                
                if(null == param) {
                    return TYPE.UNKNOWN;
                } else if("fir".equals(param)) {
                    return TYPE.CCM_FIR;
                } else if("tmmbr".equals(param)) {
                    return TYPE.CCM_TMMBR;
                } else if("tstr".equals(param)) {
                    return TYPE.CCM_TSTR;
                } else if("vbcm".equals(param)) {
                    return TYPE.CCM_VBCM;
                }
                
            } else if("trr-int".equals(type)) {
                
                return TYPE.TRRINT;
                
            } else if("goog-remb".equals(type)) {
                
                return TYPE.GOOG_REMB;
                
            }
            
            return TYPE.UNKNOWN;
        }

        private static RTCPFB parse(String line) {
            int index = 1;
            String param = null;
            TYPE fbType;
            String field = MediaCandidate.getLineField(line, 0);
            int match = field.indexOf(':');
            String payloadTypeStr = field.substring(match + 1);
            String [] arr = line.split(" ");
            String typeStr = arr[index++];
            if(arr.length > index) {
                param = arr[index++];
            }

            if(TYPE.UNKNOWN == (fbType = toType(typeStr, param))) {
                return null;
            } if("*".equals(payloadTypeStr)) {
                return  new RTCPFB(fbType);
            } else {
                return new RTCPFB(fbType, Integer.parseInt(payloadTypeStr));
            }
        }

        /**
         * Returns a serialized string version of the element which can be included as an SDP atribute field
         * @return A serialized string version of the element which can be included as an SDP atribute field
         */
        private String serialize() {
            StringBuffer str = new StringBuffer();
            String payloadType = m_bAllPayloadTypes ? "*" : Integer.valueOf(m_payloadType).toString();
            String fbType = getFbTypeString();
            String fbParam = getFbParamString();

            if(null == fbType || fbType.length() == 0) {
                return null;
            }

            str.append("a=rtcp-fb:" + payloadType + " " + fbType);
            if(null != fbParam && fbParam.length() > 0) {
                str.append(" " + fbParam);
            }

            str.append(NEWLINE);

            return str.toString();
        }

        /**
         * Returns a serialized string version of the element which can be included as an SDP atribute field
         * @return A serialized string version of the element which can be included as an SDP atribute field
         */
        @Override
        public String toString() {
            return serialize();
        }

    }

    /**
     *
     * SDP media RTCP Feedback Type List comprising of one or more feedback types.
     *
     */
    public static class RTCPFBList implements Cloneable {

        private List<RTCPFB> m_listFb = new ArrayList<RTCPFB>();

        /**
         * Default constructor used to initialize the instance
         */
        public RTCPFBList() {
        }

        /**
         * Adds an RTCP Feedback Type to be included in the list of all maintained feedback types
         * @param rtcpFb The feedback type to tbe added
         * @param bNoDuplicates If set to <i>true</i> then no duplicate values of the given feedback type and payload type will be stored.
         */
        public void add(RTCPFB rtcpFb, boolean bNoDuplicates) {
            Iterator<RTCPFB> iter = m_listFb.iterator();

            if(bNoDuplicates) {
                while(iter.hasNext()) {
                    RTCPFB r = (RTCPFB) iter.next();
                    if(r.getType() == rtcpFb.getType() &&
                       ((r.isAllPayloadTypes() && rtcpFb.isAllPayloadTypes()) ||
                        r.getPayloadType() == rtcpFb.getPayloadType())) {
                      return;
                    }
                }
            }
            m_listFb.add(rtcpFb);
        }

        private void parseItem(String line) {
            RTCPFB rtcpFb = RTCPFB.parse(line);
            if(null != rtcpFb) {
                m_listFb.add(rtcpFb);
            }
        }

        /**
         * Creates a copy of the instance
         * @return A copy of the instance
         */
        public RTCPFBList clone() throws CloneNotSupportedException {
            RTCPFBList rtcpFbList = (RTCPFBList) super.clone();

            rtcpFbList.m_listFb = new ArrayList<RTCPFB>(m_listFb.size());
            for(RTCPFB rtcpFb : m_listFb) {
                rtcpFbList.m_listFb.add(rtcpFb.clone());
            }

            return rtcpFbList;
        }

        private String serialize() {
            StringBuffer str = new StringBuffer();

            Iterator iter =  m_listFb.iterator();
            while(iter.hasNext()) {
                RTCPFB rtcpFb = (RTCPFB) iter.next();
                str.append(rtcpFb.serialize());
            }

            return str.toString();
        }

        /**
         * Returns a serialized string version of the each RTCP feedback type
         * @return A serialized string version of the each RTCP feedback type
         */
        @Override
        public String toString() {
            return serialize();
        }
    }

    /**
     *
     * SDP DTLS certificate fingerprint tag
     * <p>An example of an SDP fingerprint attribute field</p>
     * <p><i>a=fingerprint:sha-256 E8:99:D8:CA:83:81:18:45:D1:8B:54:90:64:07:01:7A:F8:9C:36:CC:5B:E0:D8:D1:89:61:0E:FF:B4:61:EF:1E</i></p>
     */
    public static class Fingerprint implements Cloneable {
        private Algorithm m_algo;
        private String m_hash;
        private Fingerprint m_peer = null;

        /**
         * Defines the fingerprint hash type
         */
        public enum Algorithm {
            /**
             * sha_512
             */
            sha_512,

            /**
             * sha_384
             */
            sha_384,

            /**
             * sha_256
             */
            sha_256,

            /**
             * sha_224
             */
            sha_224,

            /**
             * sha_1
             */
            sha_1,

            /**
             * md5
             */
            md5, 

            /**
             * md2
             */
            md2,

            /**
             * unknown
             */
            unknown
        };

        /**
         * Constructor used to initialize the instance
         * @param algo The fingerprint hash algorithm
         * @param hash The fingerprint hash value
         */
        public Fingerprint(Algorithm algo, String hash) {
            m_algo = algo;
            m_hash = hash;
        }

        private Fingerprint() {
            m_algo = Algorithm.unknown; 
        }

        /**
         * Tests if this instance has a valid fingerpring that has been set
         * @return <i>true</i> if this instance has a valid fingerprint, <i>false</i> otherwise
         */
        public boolean isSet() {
            if(Algorithm.unknown != m_algo && null != m_hash) {
                return true;
            } else {
                return false;
            }
        }

        /**
         * Sets the contents of this instance to be equal to the fingerpring argument
         * @param fingerprint The fingerprint that is used to initialize this instance
         */
        public void set(Fingerprint fingerprint) {
            m_algo = fingerprint.m_algo;
            m_hash = fingerprint.m_hash;
        }

        /**
         * Retrieves the fingerprint algorithm of this instance
         * @return The fingerprint algorithm as an enumeration 
         */
        public Algorithm getAlgorithmAsEnum() {
            return m_algo;
        }

        /**
         * Retrieves the fingerprint algorithm of this instance
         * @return The fingerprint algorithm as a string. Any <i>'_'</i> character will be replaced by the <i>'-'</i> character.
         */
        public String getAlgorithmAsString() {
            return m_algo.toString().replace('_', '-');
        }

        /**
         * Retrieves the fingerprint algorithm enumeration type given the string representation
         * @return The fingerprint algorithm as an enumration. Any <i>'-'</i> character will be replaced by the <i>'_'</i> character.
         */
        public static Algorithm StringToAlgorithm(String str) {
            return Algorithm.valueOf(str.replace('-', '_'));
        }

        /**
         * Sets the fingerprint algorithm of this instance
         * @param algo The fingerprint algorithm as a string
         */
        public void setAlgorithm(String algo) {
            m_algo = StringToAlgorithm(algo);
        }

        /**
         * Sets the fingerprint algorithm of this instance
         * @param algo The fingerprint algorithm as an enumeration 
         */
        public void setAlgorithm(Algorithm algo) {
            m_algo = algo;
        }

        /**
         * Retrieves the fingerprint hash of this instance
         * @return The fingerprint hash 
         */
        public String getHash() {
            return m_hash;
        }
  
        /**
         * Sets the fingerprint hash of this instance
         * @param hash The fingerprint hash 
         */
        public void setHash(String hash) {
            m_hash = hash;
        }
         
        /**
         * Retrieves the <i>Fingerprint</i> instance of the remote peer which has been associated with this local instance
         * @return The <i>Fingerprint</i> instance of the remote peer 
         */
        public final Fingerprint getPeer() {
            return m_peer; 
        }

        /**
         * Sets the <i>Fingerprint</i> instance of the remote peer which should be associated with this local instance
         * @param peer The <i>Fingerprint</i> instance of the remote peer 
         */
        public void setPeer(Fingerprint peer) {
            m_peer = peer; 
        }

        /**
         * <p>Tests if the fingerprint argument is equal to this instance.</p>
         * <p>The fingerprints are equal if the hash algorithm and hash values are the same.</p>
         * @param fingerprint A fingerprint instance to be tested
         * @return <i>true</i> if this instance and the fingerprint argument are the same, <i>false</i> otherwise
         */
        public boolean equals(Fingerprint fingerprint) {
            if(Algorithm.unknown != m_algo && m_algo == fingerprint.m_algo &&
               (null != m_hash && null != fingerprint.m_hash &&
                m_hash.equals(fingerprint.m_hash))) {
                return true;
            } else {
                return false;
            }
        }

        /**
         * Parses and creates a <i>Fingerprint</i> instance given an SDP crypto suite attribute field
         * @param line An SDP fingerprint attribute field
         * @return An initialized <i>Fingerprint</i> instance
         */
        private static Fingerprint parse(String line) {

            int length;
            int index = 0;
            String [] arr = line.split(" ");
            Fingerprint fingerprint = new Fingerprint();

            fingerprint.m_algo = StringToAlgorithm(arr[index++].substring(12));
            fingerprint.m_hash = arr[index++];

            return fingerprint;
        }

        /**
         * Creates a copy of the instance
         * @return A copy of the instance
         */
        public Fingerprint clone() throws CloneNotSupportedException {
            Fingerprint fingerprint = (Fingerprint) super.clone();
            return fingerprint;
        }

        private String serialize() {
            StringBuffer str = new StringBuffer();
 
            if(isSet()) {
                str.append("a=fingerprint:" + getAlgorithmAsString() + " " + m_hash);
                str.append(NEWLINE);
            }

            return str.toString();
        }

        /**
         * Returns a serialized string version of the element which can be included as an SDP atribute field
         * @return A serialized string version of the element which can be included as an SDP atribute field
         */
        @Override
        public String toString() {
            return serialize();
        }

    }


    /**
     *
     * <p>SDP crypto suite attribute description for  SRTP-SDES</p>
     * <p>An example of an SDP crypto suite attribute field</p>
     * <p><i>a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:+j1p80f5UWBnk3OBTov8j3w1wf7rjeEYwNJU6ROq</i></p>
     *
     */
    public static class SRTPCrypto implements Cloneable {
        private String m_tag;
        private String m_algo;
        private String m_key;
        private SRTPCrypto m_peer = null;

        /**
         * <p>The default crypto suite protection key length.</p>
         * <p><i>30</i></p>
         */
        public static final int KEY_LENGTH = 30;

        private static final String AES_CM_128_HMAC_SHA1_80 = "AES_CM_128_HMAC_SHA1_80";
        private static final String AES_CM_128_HMAC_SHA1_32 = "AES_CM_128_HMAC_SHA1_32";

        /**
         * <p>Defines the type of SRTP crypto suite protection Algorithm.</p>
         * <p>The crypto suite entries defined should be ordered by priority preference from highest to lowest.</p>
         */
        public enum SRTPAlgorithm {
            //
            //

            /**
             * AES_CM_128_HMAC_SHA1_80
             */
            AES_CM_128_HMAC_SHA1_80,

            /**
             * AES_CM_128_HMAC_SHA1_32
             */
            AES_CM_128_HMAC_SHA1_32
        };

        /**
         * Constructor used to initialize the instance
         * @param tag The crypto suite tag id
         * @param algo The crypto suite algorithm  
         * @param key The crypto suite protection key 
         */
        public SRTPCrypto(String tag, String algo, String key) {
            m_tag = tag;
            m_algo = algo;
            m_key = key;
        }

        private SRTPCrypto() {
        }

        /**
         * Tests if the crypto suite element provides confidentiality protection
         * @return <i>true</i> if the crypto suite element provides confidentiality protection, <i>false</i> otherwise
         */
        public boolean haveConfidentiality() {
            SRTPAlgorithm algo = getAlgorithmAsEnum();
            if(SRTPAlgorithm.AES_CM_128_HMAC_SHA1_80 == algo || SRTPAlgorithm.AES_CM_128_HMAC_SHA1_32 == algo) {
                return true;
            }
            return false;
        }

        /**
         * Tests if the crypto suite element provides authentication 
         * @return <i>true</i> if the crypto suite element provides authentication, <i>false</i> otherwise
         */
        public boolean haveAuthentication() {
            SRTPAlgorithm algo = getAlgorithmAsEnum();
            if(SRTPAlgorithm.AES_CM_128_HMAC_SHA1_80 == algo || SRTPAlgorithm.AES_CM_128_HMAC_SHA1_32 == algo) {
                return true;
            }
            return false;
        }

        /**
         * Retrieves the crypto suite tag id of this instance
         * @return The crypto suite tag id of this instance
         */
        public String getTag() {
            return m_tag;
        }

        /**
         * Sets the crypto suite tag id of this instance
         * @param tag The crypto suite tag id of this instance
         */
        public void setTag(String tag) {
            m_tag = tag;
        }

        /**
         * Retrieves the crypto suite protection algorithm of this instance
         * @return The crypto suite protection algorithm as an enumeration 
         */
        public SRTPAlgorithm getAlgorithmAsEnum() {
            return SRTPAlgorithm.valueOf(m_algo);
        }

        /**
         * Retrieves the crypto suite protection algorithm of this instance
         * @return The crypto suite protection algorithm as an ordinal index 
         */
        public int getAlgorithmOrdinal() {
            return SRTPAlgorithm.valueOf(m_algo).ordinal();
        }

        /**
         * Retrieves the crypto suite protection algorithm of this instance
         * @return The crypto suite protection algorithm as a string
         */
        public String getAlgorithmAsString() {
            return m_algo;
        }

        /**
         * Sets the crypto suite protection algorithm of this instance
         * @param algo The crypto suite protection algorithm as a string
         */
        public void setAlgorithm(String algo) {
            m_algo = algo;
        }

        /**
         * Sets the crypto suite protection algorithm of this instance
         * @param algo The crypto suite protection algorithm as an enumeration 
         */
        public void setAlgorithm(SRTPAlgorithm algo) {
            m_algo = algo.toString();
        }

        /**
         * Retrieves the SRTP protection key of this instance
         * @return The SRTP protection key of this instance which is base64 encoded
         */
        public String getKey() {
            return m_key;
        }
  
        /**
         * Sets the SRTP protection key of this instance
         * @param key The SRTP protection key of this instance which should be base64 encoded
         */
        public void setKey(String key) {
            m_key = key;
        }
         
        /**
         * Retrieves the <i>SRTPCrypto</i> instance of the remote peer which has been associated with this local instance
         * @return The <i>SRTPCrypto</i> instance of the remote peer 
         */
        public final SRTPCrypto getPeer() {
            return m_peer; 
        }

        /**
         * Sets the <i>SRTPCrypto</i> instance of the remote peer which should be associated with this local instance
         * @param peer The <i>SRTPCrypto</i> instance of the remote peer 
         */
        public void setPeer(SRTPCrypto peer) {
            m_peer = peer; 
        }

        /**
         * Parses and creates an <i>SRTPCrypto</i> instance given an SDP crypto suite attribute field
         * @param line An SDP crypto suite attribute field
         * @return An initialized <i>SRTPCrypto</i> instance
         */
        private static SRTPCrypto parse(String line) {

            int length;
            int index = 0;
            String [] arr = line.split(" ");
            SRTPCrypto crypto = new SRTPCrypto();

            crypto.m_tag = arr[index++].substring(7);
            crypto.m_algo = arr[index++];
            String key = arr[index++].substring(7);
            if((length = key.indexOf('|')) > 0) {
                crypto.m_key = key.substring(0, length);
            } else {
                crypto.m_key = key;
            }

            return crypto;
        }

        /**
         * Creates a copy of the instance
         * @return A copy of the instance
         */
        public SRTPCrypto clone() throws CloneNotSupportedException {
            SRTPCrypto crypto = (SRTPCrypto ) super.clone();
            return crypto;
        }

        /**
         * Returns a serialized string version of the element which can be included as an SDP atribute field
         * @return A serialized string version of the element which can be included as an SDP atribute field
         */
        private String serialize() {
            StringBuffer str = new StringBuffer();

            str.append("a=crypto:" + m_tag + " " + m_algo + " inline:" + m_key);
            str.append(NEWLINE);

            return str.toString();
        }

        /**
         * Returns a serialized string version of the element which can be included as an SDP atribute field
         * @return A serialized string version of the element which can be included as an SDP atribute field
         */
        @Override 
        public String toString() {
            return serialize();
        }

    }

    /**
     *
     * An SDP Address type representing both an IP address and port
     *
     */
    public static class ICEAddress implements Cloneable {
        private InetAddress m_address;
        private int m_port;

        /**
         * Constructor used to initialize the instance
         */
        public ICEAddress() {

        }

        /**
         * Constructor used to initialize the instance
         * @param address The IP address used in the <i>ICEAddress</i>
         * @param port The port used in the <i>ICEAddress</i> 
         */
        public ICEAddress(InetAddress address, int port) {
            setAddress(address);
            setPort(port);
        }

        public ICEAddress(String strAddress) throws UnknownHostException {
            String [] arr = strAddress.split(":");
            setAddress(InetAddress.getByName(arr[0]));
            if(arr.length > 0) {
                 setPort(Integer.valueOf(arr[1]));
            }
        }

        /**
         * Retrieves the IP address associated with this instance
         * @return The IP address associated with this instance
         */
        public InetAddress getAddress() {
            return m_address;
        }

        /**
         * Sets the IP address associated with this instance
         * @param address The IP address associated with this instance
         */
        public void setAddress(InetAddress address) {
            m_address = address;
        }

        /**
         * Retrieves the port associated with this instance
         * @return The port associated with this instance
         */
        public int getPort() {
            return m_port;
        }

        /**
         * Sets the port associated with this instance
         * @param port The port associated with this instance
         */
        public void setPort(int port) {
            m_port = port;
        }

        /**
         * Tests if this instance contains both a valid port and IP address
         * @return <i>true</i> if this instance contains both a valid port and IP address, </i>false</i> otherwise
         */
        public boolean isValid() throws UnknownHostException {
            if(m_port <= 0 || null == m_address || 
               m_address.hashCode() == InetAddress.getByName("0.0.0.0").hashCode()) {
                return false;
            } else {
                return true;
            }
        }

        /**
         * Tests if the argument <i>ICEAddress</i> instance is the same as this instance
         * @param addr The address to be compared to this instance
         * @param bComparePorts If set to <i>true</i>, then the port values will be compared in-addition to the IP addresses
         * @return <i>true</i> if the argument <i>ICEAddress</i> instance is the same, <i>false</i> otherwise
         */
        public boolean equals(final ICEAddress addr, boolean bComparePorts) {
            if(null != addr && (!bComparePorts || m_port == addr.m_port) && 
               m_address.hashCode() == addr.m_address.hashCode()) {
                return true;
            } else {
                return false;
            }
        }

        /**
         * Creates a copy of the instance
         * @return A copy of the instance
         */
        public ICEAddress clone() throws CloneNotSupportedException {
            ICEAddress addr = (ICEAddress) super.clone();
            return addr;
        }

        /**
         * Returns a serialized string version of the element which can be included as an SDP atribute field
         * @return A serialized string version of the element which can be included as an SDP atribute field
         */
        @Override
        public String toString() {
            StringBuffer str = new StringBuffer();
            if(null != m_address) {
                str.append(m_address.getHostAddress());
            }
            if(m_port > 0) {
                if(null != m_address) {
                    str.append(":");
                }
                str.append(m_port); 
            }
            return str.toString();
        }
    
    }

    /**
     *
     * SDP media ICE remote candidate type
     *
     */
    public static class ICERemoteCandidate implements Cloneable {
        private int m_id;
        private ICEAddress m_address;

        private ICERemoteCandidate() {

        }

        /**
         * Constructor used to initialize the instance
         * @param id candidate component id 
         * @param address candidate address 
         */
        public ICERemoteCandidate(int id, ICEAddress address) {
            m_id = id;
            m_address = address;
        }

        /**
         * Retrieves the ICE candidate component id
         * @return The ICE candidate component id
         */
        public int getId() {
            return m_id;
        }

        /**
         * Retrieves the ICE candidate address 
         * @return The ICE candidate address 
         */
        public ICEAddress getAddress() {
           return  m_address;
        }

        private String serialize() {
            return new StringBuffer().append(m_id).append(" ").append(m_address.getAddress().getHostAddress()).append(" " + m_address.getPort()).toString();
        }

        /**
         * Returns a serialized string version of the element which can be included as an SDP atribute field
         * @return A serialized string version of the element which can be included as an SDP atribute field
         */
        @Override
        public String toString() {
            return serialize();
        }

        /**
         * Creates a copy of the instance
         * @return A copy of the instance
         */
        public ICERemoteCandidate clone() throws CloneNotSupportedException {
            ICERemoteCandidate iceRemote = (ICERemoteCandidate) super.clone();
            iceRemote.m_address = m_address.clone();
            return iceRemote;
         }

    }

    /**
     *
     * SDP media ICE candidate type list
     *
     */
    public static class ICERemoteCandidates implements Cloneable {
        private List<ICERemoteCandidate> m_list = new ArrayList<ICERemoteCandidate>();

        private ICERemoteCandidates() {

        }

        /**
         * Adds an <i>ICERemoteCandidate</i> element to be included in the list of all maintained ICE candidates
         * @param iceRemoteCandidate The ICE Candidate to tbe added
         * @param bClear If set to <i>true</i> then all previously added candidates will be removed
         */
        public void add(ICERemoteCandidate iceRemoteCandidate, boolean bClear) {
            if(bClear) {
                m_list.clear();
            }
            m_list.add(iceRemoteCandidate);
        }

        /**
         * Retrieves an array of <i>ICERemoteCandidate</i> instances which have been added to the list
         * @return An array of the <i>ICERemoteCandidate</i> instances which have been added to the list
         */
        public ICERemoteCandidate[] getArray() {
           return  m_list.toArray(new ICERemoteCandidate[m_list.size()]);
        }

        /**
         * Returns the number of <i>ICERemoteCandidate</i> instances in the list
         * @return The number of <i>ICERemoteCandidate</i> instances in the list
         */
        public int getCount() {
            return m_list.size();
        }

        private static ICERemoteCandidates parse(String line) throws UnknownHostException {
            int index = 0;
            line = line.substring(18);
            String [] arr = line.split(" ");
            ICERemoteCandidates iceRemoteCandidates = new ICERemoteCandidates();

            for(; index < arr.length; index += 3) {
                iceRemoteCandidates.m_list.add(
                    new ICERemoteCandidate(Integer.valueOf(arr[index]),
                                           new ICEAddress(InetAddress.getByName(arr[index + 1]),
                                                          Integer.valueOf(arr[index + 2]))));
            }
            return iceRemoteCandidates;
        }

        private String serialize() {
            int idx = 0;
            StringBuffer str = new StringBuffer();

            str.append("a=remote-candidates:");

            Iterator iter =  m_list.iterator();
            while(iter.hasNext()) {
                if(idx++ > 0) {
                    str.append(" ");
                }
                ICERemoteCandidate iceRemote = (ICERemoteCandidate) iter.next();
                str.append(iceRemote.serialize());
            }

            str.append(NEWLINE);

            return str.toString();
        }

        /**
         * Returns a serialized string version of the each <i>ICERemoteCandidate</i> type
         * @return A serialized string version of the each <i>ICERemoteCandidate</i> type
         */
        @Override
        public String toString() {
            return serialize();
        }

        /**
         * Creates a copy of the instance
         * @return A copy of the instance
         */
        public ICERemoteCandidates clone() throws CloneNotSupportedException {
            ICERemoteCandidates iceRemoteCandidates = (ICERemoteCandidates) super.clone();

            iceRemoteCandidates.m_list = new ArrayList<ICERemoteCandidate>(m_list.size());
            for(ICERemoteCandidate iceRemoteCandidate : m_list) {
                iceRemoteCandidates.m_list.add(iceRemoteCandidate.clone());
            }
            return iceRemoteCandidates;
         }

    }

    /**
     *
     * SDP media ICE candidate
     * <p>An example of an SDP Media ICE candidate attribute field</p>
     * <p><i>a=candidate:2530088836 1 udp 2113937151 192.168.1.100 56382 typ host</i></p>
     * <pre>
     *             foundation  (component-)id transport    priority   address       port      type attr<br>
     * a=candidate:2530088836  1              udp          2113937151 192.168.1.100 56382 typ host generation 0<br>
     * <br> 
     *  For media streams based on RTP, candidates for the actual RTP media MUST have a<br>
     *  component ID of 1, and candidates for RTCP MUST have a component ID of 2.<br>
     * <br>
     * RFC 5245<br>
     * 4.1.2.1.  Recommended Formula<br>
     * priority = (2^24)*(type preference) +<br>
     *            (2^8)*(IP priority) +<br>
     *            (2^0)*(256 - component ID)<br>
     * 126 host
     * 110 prflx 
     * 100 srflx
     * 0 relay
     * </pre>
     *
     */
    public static class ICECandidate implements Cloneable {
        private String m_foundation;
        private int m_id;
        private TRANSPORT m_transport;
        private long m_priority;
        private ICEAddress m_iceAddress;
        private TYPE m_type;
        private String m_attr;
        private ICEAddress m_iceRelayAddress;


        /**
         * <p>SDP ICE Candidate with component type RTP</p>
         * Component id <i>1</i>
         */
        public static final int ICE_COMPONENT_RTP        = 1;

        /**
         * <p>SDP ICE Candidate with component type RTCP</p>
         * Component id <i>2</i>
         */
        public static final int ICE_COMPONENT_RTCP       = 2;

        /**
         * SDP ICE Candidate transport type
         */
        public enum TRANSPORT {

            /**
             * SDP ICE Candidate transport type UDP
             * Transport type <i>udp</i>
             */
            udp,

            /**
             * SDP ICE Candidate transport type TCP
             * Transport type <i>tcp</i>
             */
            tcp 
        };

        /**
         * <p>SDP ICE Candidate remote endpoint type.</p>
         * <p>This enum is ordered by preferred candidate matching preference and the order should not be rearranged.</p>
         */
        public enum TYPE {
            /**
             * SDP peer-reflexive ICE address type
             * <i>prflx</i>
             */
            prflx,

            /**
             * SDP server-reflexive ICE address type
             * <i>srflx</i>
             */
            srflx,

            /**
             * SDP host ICE address type
             * <i>host</i>
             */
            host,

            /**
             * SDP relay ICE address type
             * <i>relay</i>
             */
            relay
        };

        private ICECandidate() {
            m_iceAddress = new ICEAddress();        
        }

        /**
         * Constructor used to initialize the instance
         * @param foundation The SDP ICE candidate foundation
         * @param id The SDP ICE candidate component id 
         * @param transport The SDP ICE candidate transport type  
         * @param priority The SDP ICE candidate priority value 
         * @param address The SDP ICE candidate address
         * @param type The SDP ICE candidate remote endpoint type 
         */
        public ICECandidate(String foundation, 
                            int id, 
                            TRANSPORT transport, 
                            long priority, 
                            ICEAddress address, 
                            TYPE type) {

            m_foundation = foundation;
            m_id = id;
            m_transport = transport;
            m_priority = priority;
            m_iceAddress = address;
            m_type = type;
        }

        /**
         * Constructor used to initialize the instance
         * @param foundation The SDP ICE candidate foundation
         * @param id The SDP ICE candidate component id 
         * @param transport The SDP ICE candidate transport type  
         * @param priority The SDP ICE candidate priority value 
         * @param address The SDP ICE candidate address
         * @param type The SDP ICE candidate remote endpoint type 
         * @param relayAddress The SDP ICE candidate relay address 
         */
        public ICECandidate(String foundation,
                            int id,
                            TRANSPORT transport,
                            long priority,
                            ICEAddress address,
                            TYPE type,
                            ICEAddress relayAddress) {

            m_foundation = foundation;
            m_id = id;
            m_transport = transport;
            m_priority = priority;
            m_iceAddress = address;
            m_type = type;
            m_iceRelayAddress = relayAddress;
        }
 
        /**
         * Retrieves the ICE candidate foundation value
         * @return The ICE candidate foundation value
         */
        public final String getFoundation() {
            return m_foundation;
        }

        /**
         * Retrieves the ICE candidate component id 
         * @return The ICE candidate component id 
         */
        public int getId() {
            return m_id;
        }

        /**
         * Sets the ICE candidate component id 
         * @param id The ICE candidate component id 
         */
        public void setId(int id) {
            m_id = id;
        }

        /**
         * Retrieves the ICE candidate transport type
         * @return The ICE candidate transport type 
         */
        public TRANSPORT getTransport() {
            return m_transport;
        }

        /**
         * Sets the ICE candidate transport type
         * @param transport The ICE candidate transport type 
         */
        public void setTransport(TRANSPORT transport) {
            m_transport = transport;
        }

        /**
         * Retrieves the ICE candidate priority value
         * @return The ICE candidate priority value
         */
        public long getPriority() {
            return m_priority;
        }

        /**
         * Retrieves the ICE candidate address 
         * @return The ICE candidate address 
         */
        public final ICEAddress getICEAddress() {
            return m_iceAddress;
        }

        /**
         * Sets the ICE candidate IP address 
         * @param address The ICE candidate IP address 
         */
        public void setAddress(InetAddress address) {
            m_iceAddress.setAddress(address);
        }

        /**
         * Sets the ICE candidate port number
         * @param port The ICE candidate port number
         */
        public void setPort(int port) {
            m_iceAddress.setPort(port);
        }

        /**
         * Retrieves the ICE candidate TURN relay address 
         * @return The ICE candidate TURN relay address 
         */
        public final ICEAddress getICERelayAddress() {
            return m_iceRelayAddress;
        }

        /**
         * Retrieves the ICE candidate remote endpoint type 
         * @return The ICE candidate remote endpoint type
         */
        public TYPE getType() {
            return m_type;
        }

        /**
         * Sets the ICE candidate remote endpoint type 
         * @param type The ICE candidate remote endpoint type
         */
        public void setType(TYPE type) {
            m_type= type;
        }

        /**
         * Retrieves the ICE candidate custom attributes
         * @return The ICE candidate custom attributes
         */
        public String getAttributes() {
            return m_attr;
        }

        /**
         * Sets the ICE candidate custom attributes
         * @param attr The ICE candidate custom attributes
         */
        public void setAttributes(String attr) {
            m_attr = attr;
        }

        /**
         * Parses an SDP ICE candidate line
         */
        private static ICECandidate parse(String line) throws UnknownHostException {

            int length;
            int index = 0;
            String [] arr = line.split(" ");
            ICECandidate ice = new ICECandidate();

            ice.m_foundation = arr[index++].substring(10);
            ice.m_id = Integer.valueOf(arr[index++]);
            ice.m_transport = TRANSPORT.valueOf(arr[index++].toLowerCase());
            ice.m_priority = Long.valueOf(arr[index++]);
            ice.m_iceAddress.m_address = InetAddress.getByName(arr[index++]);
            ice.m_iceAddress.m_port = Integer.valueOf(arr[index++]);
            index++; // 'typ'
            ice.m_type = TYPE.valueOf(arr[index++].toLowerCase());

            if(TYPE.relay == ice.m_type) {
                String strRaddr = arr[index++];
                if("raddr".equalsIgnoreCase(strRaddr)) {
                    ice.m_iceRelayAddress = new ICEAddress();
                    ice.m_iceRelayAddress.m_address = InetAddress.getByName(arr[index++]);
                    String strRport = arr[index++];
                    if("rport".equalsIgnoreCase(strRport)) {
                        ice.m_iceRelayAddress.m_port = Integer.valueOf(arr[index++]);
                    }
                }
            }

            ice.m_attr = null;
            for(; index < arr.length; index++) {
                if(null == ice.m_attr) {
                    ice.m_attr = "";
                } else {
                    ice.m_attr += " ";
                }
                ice.m_attr += arr[index];
            }

            return ice;
        }

        /**
         * Creates a copy of the instance
         * @return A copy of the instance
         */
        public ICECandidate clone() throws CloneNotSupportedException {
            ICECandidate iceCandidate = (ICECandidate) super.clone();
            iceCandidate.m_iceAddress = m_iceAddress.clone();
            return iceCandidate;
        }

        /**
         * Returns a serialized string version of the element which can be included as an SDP atribute field
         * @return A serialized string version of the element which can be included as an SDP atribute field
         */
        private String serialize() {
            StringBuffer str = new StringBuffer();

            str.append("a=candidate:" + m_foundation + " " + m_id + " " + m_transport.toString() + " " + m_priority + 
                       " " + m_iceAddress.m_address.getHostAddress() + " " + m_iceAddress.m_port + " typ " + 
                       m_type.toString());
            if(null != m_iceRelayAddress) {
                str.append(" raddr " + m_iceRelayAddress.m_address.getHostAddress() + " rport " + m_iceRelayAddress.m_port);
            }
            if(null != m_attr) {
                str.append(" " + m_attr);
            }
            str.append(NEWLINE);

            return str.toString();
        }

        /**
         * Returns a serialized string version of the element which can be included as an SDP atribute field
         * @return A serialized string version of the element which can be included as an SDP atribute field
         */
        @Override
        public String toString() {
            return serialize();
        }

    }

    /**
     *
     * SDP media ICE atrributes 
     *
     */
    private class ICEProperties implements Cloneable {

        private boolean m_bIsSet;
        private boolean m_bIsSameAudioVideo;
        private boolean m_bIceLite;
        private String m_iceUfrag;
        private String m_icePwd;

        private ICEProperties() {
            m_bIsSet = false;
            m_bIsSameAudioVideo = false;
            m_bIceLite = false; 
            m_iceUfrag = null;
            m_icePwd = null;
        }

        private void setICELite(boolean bIceLite) {
            m_bIsSet = true;
            m_bIceLite = bIceLite;
        }

        private void setICEUfrag(String iceUfrag){
            m_bIsSet = true;
            m_iceUfrag = iceUfrag;
        }

        private void setICEPwd(String icePwd){
            m_bIsSet = true;
            m_icePwd = icePwd;
        }

        public boolean IsSet() {
            return m_bIsSet;
        }

        private String serialize() {
            StringBuffer str = new StringBuffer();

            if(m_bIsSet) {

                if(m_bIceLite) {
                    str.append("a=ice-lite" + NEWLINE);
                }
                if(null != m_iceUfrag && m_iceUfrag.length() > 0) {
                    str.append("a=ice-ufrag:" + m_iceUfrag + NEWLINE);
                }
                if(null != m_icePwd && m_icePwd.length() > 0) {
                    str.append("a=ice-pwd:" + m_icePwd + NEWLINE);
                }
            }

            return str.toString();
        }

        public ICEProperties clone() throws CloneNotSupportedException {
            return (ICEProperties ) super.clone();
        }
    }

    /**
     *
     * SDP bandwidth properties 
     *
     */
    public class Bandwidth implements Cloneable {

        private long m_asBps;
        private long m_ctBps;
        private long m_tiasBps;

        /**
         * Retrieves the Application Specific bandwidth value 
         * @return The value of the Application Specific bandwidth as bits per second
         */
        public long getAS() {
            return m_asBps;
        }

        /**
         * Sets the Application Specific bandwidth value 
         * @param asBps The value of the Application Specific bandwidth in bits per second
         */
        public void setAS(long asBps) {
            m_asBps = asBps;
        }

        /**
         * Retrieves the Conference Transport bandwidth value 
         * @return The value of the Conference Transport bandwidth in bits per second
         */
        public long getCT() {
            return m_ctBps;
        }

        /**
         * Sets the Conference Transport bandwidth value 
         * @param ctBps The value of the Conference Transport bandwidth in bits per second
         */
        public void setCT(long ctBps) {
            m_ctBps = ctBps;
        }

        /**
         * Retrieves the Transport Independent Application Specific bandwidth value 
         * @return the value of the Transport Independent Application Specific bandwidth in bits per second
         */
        public long getTIAS() {
            return m_tiasBps;
        }

        /**
         * Sets the Transport Independent Application Specific bandwidth value 
         * @param tiasBps the value of the Transport Independent Application Specific bandwidth in bits per second
         */
        public void setTIAS(long tiasBps) {
            m_tiasBps = tiasBps;
        }

        private void parse(String key, String value) {
            if("AS".equalsIgnoreCase(key)) {
                m_asBps = Long.parseLong(value) * 1000;
            } else if("CT".equalsIgnoreCase(key)) {
                m_ctBps = Long.parseLong(value) * 1000;
            } else if("TIAS".equalsIgnoreCase(key)) {
                m_tiasBps = Long.parseLong(value);
            }
        }

        private void parse(String line) {
            if(null != line) {
                String key = null;
                String value = null;
                int match = -1;
                if(null != line) {
                    match = line.indexOf(':');
                }
                if(match >= 0) {
                  key = line.substring(0, match).trim();
                  value = line.substring(match + 1).trim();
                  parse(key, value);
                }
            }
        }

        /**
         * Returns a serialized string version of any set bandwidth attributes which can be included in an SDP document
         * @return a serialized string version of any set bandwidth attributes which can be included in an SDP document
         */
        private String serialize() {
            StringBuffer str = new StringBuffer();

            if(m_asBps > 0) {
                str.append("b=AS:" + (m_asBps / 1000) + NEWLINE);
            }
            if(m_tiasBps > 0) {
                str.append("b=TIAS:" + m_tiasBps + NEWLINE);
            }

            return str.toString();
        }

        /**
         * Creates a copy of the instance
         * @return A copy of the instance
         */
        public Bandwidth clone() throws CloneNotSupportedException {
            return (Bandwidth ) super.clone();
        }

    }

    /**
     *
     * SDP media description 
     *
     */
    public class Media implements Cloneable {
        
        private MediaType m_type;
        private SDPType m_sendRecvType;
        private ICEAddress m_addr = new ICEAddress();
        private ICEAddress m_targetAddr = new ICEAddress(); // From a desired ICE candidate 
        private ICEAddress m_targetAddrRTCP = new ICEAddress(); 
        private String m_connect;
        private String m_media;
        private boolean m_bRtcpMux;
        private String m_transType;
        private boolean m_bInactive;
        private ICEProperties m_iceProperties = new ICEProperties();
        private Bandwidth m_bandwidth = new Bandwidth();
        private List<String> m_listAttr = new ArrayList<String>();
        private Fingerprint m_fingerprint = new Fingerprint();
        private List<SRTPCrypto> m_listCrypto = new ArrayList<SRTPCrypto>();
        private List<ICECandidate> m_listIceCandidates = new ArrayList<ICECandidate>();
        private ICERemoteCandidates m_iceRemoteCandidates = new ICERemoteCandidates();
        private RTCPFBList m_rtcpFb = new RTCPFBList();
        private List<SSRCAttrib> m_listSSRC = new ArrayList<SSRCAttrib>();
        private OrderedHashMap<Integer, MediaCandidate> m_mapCandidates = new OrderedHashMap<Integer, MediaCandidate>();
        private boolean m_bIsRequest;
 
        /**
         * Constructor used to initialize the instance
         * @param type The type of media type description, such as audio or video
         */
        public Media(MediaType type) {
            setDefaults();
            m_type = type;
        }

        /**
         * Constructor used to initialize the instance
         * @param type The type of media type description, such as audio or video
         * @param rtpmapAttr The <i>rtpmap</i> media description value starting at the port number which will be parsed
         */
        public Media(MediaType type, String rtpmapAttr) {
            setDefaults();
            m_type = type;
            setDescription(rtpmapAttr);
        }

        /**
         * Fills in default media attributes
         */
        private void setDefaults() {
            m_sendRecvType = SDPType.SDP_TYPE_SENDRECV;
            m_transType = TRANSPORT_RTP_AVP;
            m_bRtcpMux = false;
            m_bInactive = false;
            m_iceProperties.m_bIceLite = false;
        }

        /**
         * Adds a media candidate to the list of media candidates
         * @param codec The media codec description
         */
        public void addCandidate(Codec codec) throws CloneNotSupportedException {

            MediaCandidate candidate = new MediaCandidate(codec);
            m_mapCandidates.put(new Integer(codec.getPayloadType()), candidate);
        }

        /**
         * Finds a media candidate given the codec name
         * @param codecName The media codec name used in the <i>rtpmap</i> media candidate attribute
         */
        public final Codec findCodec(String codecName) {
            Iterator iter =  m_mapCandidates.getOrderedIterator();
            while(iter.hasNext()) {
                MediaCandidate candidate = (MediaCandidate) iter.next();
                if(codecName.equalsIgnoreCase(candidate.getCodec().getName())) {
                    return candidate.getCodec();
                }
            }
            return null;
        }

        /**
         * Removes a media candidate from the list of <i>rtpmap</i> candidates
         * @param candidate A media candidate instance
         */
        public void removeCandidate(final MediaCandidate candidate) {
            m_mapCandidates.remove(new Integer(candidate.getCodec().getPayloadType()));
        }

        /**
         * Retrieves the media type as a string
         * @return A valid media type, such as <i>video</i>, <i>audio</i>, or an empty string
         */
        public String getMediaTypeAsString() {
            if(MediaType.MEDIA_TYPE_VIDEO == m_type) {
                return  "video";
            } else if(MediaType.MEDIA_TYPE_AUDIO == m_type) {
                return  "audio";
            } else {
                return "";
            }
        }

        /**
         * Tests if the media transport method uses a secure audio/video profile such as <i>RTP/SAVP</i>
         * @return <i>true</i> if the media transport method uses a secure audio/video profile, <i>false</i> otherwise
         */
        public boolean isMediaTransportSecure() {
            if(SDP.TRANSPORT_RTP_SAVP.equals(m_transType) || SDP.TRANSPORT_RTP_SAVPF.equals(m_transType)) {
                return true;
            } else {
                return false;
            }
        }

        /**
         * Tests if the media transport method uses a AVPF profile such as <i>RTP/AVPF</i>
         * @return <i>true</i> if the media transport method uses AVPF
         */
        public boolean isMediaAVPF() {
            if(SDP.TRANSPORT_RTP_AVPF.equals(m_transType) || SDP.TRANSPORT_RTP_SAVPF.equals(m_transType)) {
                return true;
            } else {
                return false;
            }
        }

        /**
         * Retrieves the <i>Bandwidth</i> instance associated with this media description
         * @return The <i>Bandwidth</i> instance associated with this media description
         */
        public Bandwidth getBandwidth() {
            return m_bandwidth;
        }

        /**
         * Retrieves the remote peer target media address associated with this media description
         * @return The remote peer target media address associated with this media description
         */
        public final ICEAddress getTarget() {
            return m_targetAddr;
        }

        /**
         * Sets the remote peer target media address associated with this media description
         * @param addr The remote peer target media address associated with this media description
         */
        public final void setTarget(ICEAddress addr) {
            m_targetAddr.setAddress(addr.getAddress());
            m_targetAddr.setPort(addr.getPort());
        }

        /**
         * Retrieves the remote peer target RTCP address associated with this media description
         * @return The remote peer target RTCP address associated with this media description
         */
        public final ICEAddress getTargetRTCP() {
            return m_targetAddrRTCP;
        }

        /**
         * Sets the remote peer target RTCP address associated with this media description
         * @param addr The remote peer target RTCP address associated with this media description
         */
        public final void setTargetRTCP(ICEAddress addr) {
            m_targetAddrRTCP.setAddress(addr.getAddress());
            m_targetAddrRTCP.setPort(addr.getPort());
        }

        /**
         * Retrieves the <i>connect</i> attribute address associated with this media description
         * @return the <i>connect</i> attribute address associated with this media description
         */
        public final ICEAddress getConnect() {
            return m_addr;
        }

        /**
         * Retrieves the transport type associated with this media description
         * @return The transport type associated with this media description, such as <i>RTP/AVP</i>
         */
        public final String getTransportType() {
            return m_transType;
        }

        /**
         * Sets the transport type associated with this media description
         * @param transType The transport type associated with this media description, such as <i>RTP/AVP</i>
         */
        public void setTransportType(String transType) {
            m_transType = transType;
        }

        /**
         * Tests if the media description is marked inactive, such as when the <i>a=inactive</i> attribute is present
         * @return <i>true</i> if the media description is marked inactive, such as when the <i>a=inactive</i> attribute is present
         */
        public final boolean isInactive() {
            return m_bInactive;
        }

        /**
         * Sets the media description is inactive, akin to including the <i>a=inactive</i> attribute 
         * @param bInactive Set to <i>true</i> to mark the media description as inactive, </i>false</i> otherwise
         */
        public final void setInactive(boolean bInactive) {
            m_bInactive = bInactive;
        }

        /**
         * Adds a custom attribute value to the media description
         * @param attr A custom attribute value to be added to the media description
         */
        public void addAttribute(String attr) {
            m_listAttr.add(attr);
        }

        /**
         * <p>Tests if the <i>rtcp-mux</i> attribute is set</p>
         * @return <i>true</i> if the <i>rtcp-mux</i> attribute is set
         */
        public boolean isRtcpMux() {
            return m_bRtcpMux;
        }

        /**
         * <p>Sets the <i>rtcp-mux</i> attribute</p>
         * @param bRtcpMux <i>true</i> to set the <i>rtcp-mux</i> attribute, <i>false</i> to clear it
         */
        public void setRtcpMux(boolean bRtcpMux) {
            m_bRtcpMux = bRtcpMux;
        }

        /**
         * Adds an RTCP Feedback type instance to the media description
         * @param rtcpFb The RTCP Feedback type instance to be added to the media description
         */
        public void addRTCPFB(RTCPFB rtcpFb) {
            m_rtcpFb.add(rtcpFb, true);
        }

        /**
         * Retrieves an iterator used to iterate through the added RTCP Feedback type instances
         * @return An iterator used to iterate through the added RTCP Feedback type instances
         */
        public final Iterator<RTCPFB> getRTCPFBIterator() {
            return (Iterator<RTCPFB>) m_rtcpFb.m_listFb.iterator();
        }

        /**
         * Retrieves the DTLS <i>Fingerprint</i> associated with the media description
         * @return The DTLS <i>Fingerprint</i> associated with the media description
         */
        public Fingerprint getFingerprint() {
            return m_fingerprint;
        }

        /**
         * Adds an SRTP crypto suite entry to the media description
         * @param crypto An SRTP crypto suite entry to be added to the media description
         * @param bClear If set to <i>true</i> then any existing crypto suite entries will be cleared
         */
        public void addCrypto(SRTPCrypto crypto, boolean bClear) throws CloneNotSupportedException {
            if(bClear) {
                m_listCrypto.clear();
            }
            if(null != crypto) {
                m_listCrypto.add(crypto);
            }
        }

        /**
         * Retrieves an iterator used to iterate through the added SRTP crypto suite entries
         * @return An iterator used to iterate through the added SRTP crypto suite entries
         */
        public final Iterator<SRTPCrypto> getCryptoListIterator() {
            return (Iterator<SRTPCrypto>) m_listCrypto.iterator(); 
        }
      
        /**
         * Retrieves the number of SRTP crypto suite entries associated with the media description
         * @return The number of SRTP crypto suite entries associated with the media description
         */
        public int getCryptoCount() {
            return m_listCrypto.size();
        }

        /**
         * Retrieves the first SRTP crypto suite entry
         * @return The first SRTP crypto suite entry or <i>null</i>
         */
        public SRTPCrypto getCrypto() {
            Iterator<SRTPCrypto> iterCrypto = m_listCrypto.iterator();
            if(iterCrypto.hasNext()) {
                return  iterCrypto.next();
            } else {
                return null;
            }
        }

        /**
         * Tests if the <i>ice-lite</i> attribute is set for the media description
         * @return <i>true</i> if the <i>ice-lite</i> attribute is set for the media description, <i>false</i> otherwise
         */ 
        public boolean isICELite() {
            return m_iceProperties.m_bIceLite;
        }

        /**
         * <p>Sets the <i>ice-lite</i> attribute</p>
         * @param bIceLite <i>true</i> to set the <i>ice-lite</i> attribute, <i>false</i> to clear it
         */
        public void setICELite(boolean bIceLite) {
            m_iceProperties.setICELite(bIceLite);
        }

        /**
         * Retrieves the <i>ice-ufrag</i> media attribute entry
         * @return The <i>ice-ufrag</i> media attribute entry
         */
        public String getICEUfrag(){
            return m_iceProperties.m_iceUfrag;
        }

        /**
         * Sets the <i>ice-ufrag</i> attribute to be associated with the media attribute
         * @param iceUfrag The <i>ice-ufrag</i> media attribute entry to be associated with the media attribute
         */
        public void setICEUfrag(String iceUfrag){
            m_iceProperties.setICEUfrag(iceUfrag);
        }

        /**
         * Retrieves the <i>ice-pwd</i> media attribute entry
         * @return The <i>ice-pwd</i> media attribute entry
         */
        public String getICEPwd() {
            return m_iceProperties.m_icePwd;
        }

        /**
         * Sets the <i>ice-pwd</i> attribute to be associated with the media attribute
         * @param icePwd The <i>ice-pwd</i> media attribute entry to be associated with the media attribute
         */
        public void setICEPwd(String icePwd){
            m_iceProperties.setICEPwd(icePwd);
        }

        /**
         * Tests if ICE properties such as <i>ice-ufrag</i> and <i>ice-pwd</i> are the same for both audio and video media descriptions
         * @return <i>true</i> if ICE properties such as <i>ice-ufrag</i> and <i>ice-pwd</i> are the same for both audio and video media descriptions, <i>false</i> otherwise
         */
        public boolean isICESameAudioVideo(){
            return m_iceProperties.m_bIsSameAudioVideo;
        }

        /**
         * Sets both audio and video ICE properties such as <i>ice-ufrag</i> and <i>ice-pwd</i> to be the same for both audio and video media descriptions
         * @param bIsSameAudioVideo Set to <i>true</i> for  both audio and video ICE properties such as <i>ice-ufrag</i> and <i>ice-pwd</i> to be the same for both audio and video media descriptions
         */
        public void setIsICESameAudioVideo(boolean bIsSameAudioVideo){
            m_iceProperties.m_bIsSameAudioVideo = bIsSameAudioVideo;
        }

        /**
         * Adds an ICE media candidate entry to the media description
         * @param iceCandidate An ICE media candidate entry to be added to the media description
         * @param bClear If set to <i>true</i> then any existing ICE media candidate entries will be cleared
         */
        public void addICECandidate(ICECandidate iceCandidate, boolean bClear) throws CloneNotSupportedException {
            if(bClear) {
                m_listIceCandidates.clear();
            }
            if(null != iceCandidate) {
              m_listIceCandidates.add(iceCandidate);
            }
        }

        /**
         * Retrieves an iterator used to iterate through the list of added ICE media candidate entries
         * @return An iterator used to iterate through the list of added ICE media candidate entries
         */
        public final Iterator<ICECandidate> getICECandidatesIterator() {
            Iterator<ICECandidate> iterIceCandidates = m_listIceCandidates.iterator(); 
            return iterIceCandidates;
        }

        /**
         * Retrieves the number of ICE media candidate entries associated with the media description
         * @return The number of ICE media candidate entries associated with the media description
         */
        public int getICECandidatesCount() {
            return m_listIceCandidates.size();
        }

        /**
         * Retrieves the first ICE media candidate entry
         * @return The first ICE media candidate entry or <i>null</i>
         */
        public ICECandidate getICECandidate() {
            Iterator<ICECandidate> iterIceCandidates = m_listIceCandidates.iterator();
            if(iterIceCandidates.hasNext()) {
                return  iterIceCandidates.next();
            } else {
                return null;
            }
        }

        /**
         * Retrieves the <i>ICERemoteCandidate</i> instance containing the listing of ICE <i>remote-candidates</i> 
         * @return The <i>ICERemoteCandidate</i> instance containing the listing of ICE <i>remote-candidates</i> 
         */
        public ICERemoteCandidates getICERemoteCandidates() {
            return m_iceRemoteCandidates;
        }

        /**
         * Retrieves the RTP SSRC value associated with the media description
         * @return The RTP SSRC value associated with the media description
         */
        public long getSSRC() {
             Iterator<SSRCAttrib> iterSSRCAttribs = m_listSSRC.iterator();
             while(iterSSRCAttribs.hasNext()) {
                 SSRCAttrib ssrcAttrib = iterSSRCAttribs.next();
                 return ssrcAttrib.getSSRC(); 
             }
             return 0;
        }

        /**
         * Adds an RTP SSRC attribute entry to the media description
         * @param ssrcAttrib An RTP SSRC attribute entry to be added to the media description
         * @param bClear If set to <i>true</i> then any existing SSRC attribute entries will be cleared
         */
        public void addSSRCAttrib(SSRCAttrib ssrcAttrib, boolean bClear) {
            if(bClear) {
                m_listSSRC.clear();
            }
            if(null != ssrcAttrib) {
                m_listSSRC.add(ssrcAttrib);
            }
        }

        /**
         * Retrieves an iterator used to iterate through the list of added RTP SSRC attribute entries
         * @return An iterator used to iterate through the list of added RTP SSRC attribute entries
         */
        public final Iterator<SSRCAttrib> getSSRCAttribIterator() {
            Iterator<SSRCAttrib> iterSSRCAttribs = m_listSSRC.iterator(); 
            return iterSSRCAttribs;
        }

        /**
         * <p>Finds an SSRC SDP attribute with the given key.</p>
         * <p>The key for te given attribute <i>a=ssrc:556784522 cname:02pXsDmY1B93jiWK</i> is <i>cname</i></p>
         * @param key The key of the SSRC SDP attribute
         * @return The <i>SSRCAttrib</i> element or <i>null</i>
         */
        public SSRCAttrib findSSRCAttrib(String key) {
             if(null == key) {
                 return null;
             }
             Iterator<SSRCAttrib> iterSSRCAttribs = m_listSSRC.iterator();
             while(iterSSRCAttribs.hasNext()) {
                 SSRCAttrib ssrcAttrib = iterSSRCAttribs.next();
                 if(key.equalsIgnoreCase(ssrcAttrib.getAttribPair().getKey())) {
                     return ssrcAttrib;
                 }
             }
             return null;
        }

        /**
         * Tests if this media description is for an SDP document associated with a request 
         * @return <i>true</i> if this media description is for an SDP document associated with a request, <i>false</i> otherwise
         */
        public boolean isRequest() {
            return m_bIsRequest;
        }

        /**
         * Sets whether the SDP document is to be associated with a request or response
         * @param bIsRequest Set<i>true</i> to mark the SDP document to be associated with a request, <i>false</i> to be associated with a response
         */
        public void setIsRequest(boolean bIsRequest) {
            m_bIsRequest = bIsRequest;
        }

        /**
         * Creates a copy of the instance
         * @return A copy of the instance
         */
        public Media clone() throws CloneNotSupportedException {
            Media media = (Media) super.clone();

            media.m_addr = m_addr.clone();
            media.m_targetAddr = m_targetAddr.clone();
            media.m_targetAddrRTCP = m_targetAddrRTCP.clone();

            media.m_iceProperties = m_iceProperties.clone();
            media.m_bandwidth = m_bandwidth.clone();

            media.m_listAttr = new ArrayList<String>(m_listAttr.size());
            for(String attr : m_listAttr) {
                media.m_listAttr.add(new String(attr));
            }

            media.m_fingerprint = m_fingerprint.clone();

            media.m_listCrypto = new ArrayList<SRTPCrypto>(m_listCrypto.size());
            for(SRTPCrypto crypto : m_listCrypto) {
                media.m_listCrypto.add(crypto.clone());
            }

            media.m_listIceCandidates = new ArrayList<ICECandidate>(m_listIceCandidates.size());
            for(ICECandidate iceCandidate : m_listIceCandidates) {
                media.m_listIceCandidates.add(iceCandidate.clone());
            }

            if(null != m_iceRemoteCandidates) {
                media.m_iceRemoteCandidates = m_iceRemoteCandidates.clone();
            }

            media.m_rtcpFb = m_rtcpFb.clone();

            media.m_listSSRC = new ArrayList<SSRCAttrib>(m_listSSRC.size());
            for(SSRCAttrib ssrcAttrib : m_listSSRC) {
                media.m_listSSRC.add(ssrcAttrib.clone());
            }

            media.m_mapCandidates = new OrderedHashMap<Integer, MediaCandidate>();
            Iterator iter =  m_mapCandidates.getOrderedIterator();
            while(iter.hasNext()) {
                MediaCandidate candidate = (MediaCandidate) iter.next();
                media.m_mapCandidates.put(new Integer(candidate.getCodec().getPayloadType()), candidate.clone());
            }

            return media;
        }

        private void setDescription(String description) {
            int index = 0;
            m_media = description;

            String [] arr = description.split(" ");

            m_addr.setPort(Integer.parseInt(arr[index++]));

            if(index < arr.length) {
                m_transType = arr[index++];
            }

            if(index < arr.length) {
                m_mapCandidates.clear();
                while(index < arr.length) {
                    int payloadType = Integer.parseInt(arr[index++]);
                    m_mapCandidates.put(new Integer(payloadType), new MediaCandidate(payloadType));
                }
            }

        }

        private void parseConnect(String connect) throws UnknownHostException {
            m_connect = connect;
 
            if(null != m_connect) {
                String addrType = MediaCandidate.getLineField(m_connect, 0);
                String addrType2 = MediaCandidate.getLineField(m_connect, 1);
                String addrStr = MediaCandidate.getLineField(m_connect, 2);
                m_addr.setAddress(InetAddress.getByName(addrStr));
            }
        }

        /**
         * Retrieves all <i>MediaCandidate</i> entries associated with the media description as an array
         * @return An array of  <i>MediaCandidate</i> entries  associated with the media description
         */
        public MediaCandidate [] toArray() {
            MediaCandidate[] arrMediaCandidates = null;
            int index = 0;
            if(m_mapCandidates.size() > 0) {
                arrMediaCandidates = new MediaCandidate[m_mapCandidates.getSize()];
                Iterator iter =  m_mapCandidates.getOrderedIterator();
                while(iter.hasNext()) {
                    arrMediaCandidates[index++] = (MediaCandidate) iter.next();
                }
           }
           return arrMediaCandidates;
        }

        /**
         * <p>Updates any RTP and RTCP addresses and ports.</p>
         * <p>This method should be called after constructing a media description before serializing it.</i>
         */
        public void updateTargets() throws CloneNotSupportedException {
            setTarget(m_addr.clone());
            setTargetRTCP(m_targetAddr.clone());
            if(false == m_bRtcpMux) {
                m_targetAddrRTCP.setPort(m_targetAddrRTCP.getPort() + 1);
            }
        }

        /**
         * Returns a serialized string version of the media description which can be included in an SDP document
         * @return A serialized string version of the media description which can be included in an SDP document
         */
        private String serialize(String connectStr) {
            StringBuffer str = new StringBuffer();
            Iterator iterObj;

            str.append("m=" + getMediaTypeAsString() + " " + m_addr.getPort() + " " + m_transType);

            //
            // Maintain the same order of the candidates as they were added
            //
            iterObj =  m_mapCandidates.getOrderedIterator();
            while(iterObj.hasNext()) {
                MediaCandidate candidate = (MediaCandidate) iterObj.next();
                if(null != candidate.m_rtpmap) {
                    str.append(" " + candidate.getCodec().getPayloadType());
                }
            }
            str.append(NEWLINE);

            if(null != connectStr && !connectStr.equalsIgnoreCase(m_connect)) {
                str.append("c=" + m_connect + NEWLINE);
            }

            if(null != m_bandwidth) {
                str.append(m_bandwidth.serialize());
            }

//ICEAddress ia = getTargetRTCP();
//if(null!=ia && null!=ia.getAddress()) str.append("a=rtcp: " + (ia.getPort()-1) + " IN IP4 " + ia.getAddress().getHostAddress() + NEWLINE);

            //
            // Serialize any ICE properties
            //
            Iterator<ICECandidate> iterIceCandidates = m_listIceCandidates.iterator();
            while(iterIceCandidates.hasNext()) {
                ICECandidate iceCandidate = iterIceCandidates.next();
                str.append(iceCandidate.serialize());
            }

            if(null != m_iceRemoteCandidates && m_iceRemoteCandidates.getCount() > 0) {
                str.append(m_iceRemoteCandidates.serialize());
            } 

            if(null != m_iceProperties) {
                str.append(m_iceProperties.serialize());
            }

            iterObj =  m_mapCandidates.getOrderedIterator();
            while(iterObj.hasNext()) {
                MediaCandidate candidate = (MediaCandidate) iterObj.next();
                if(null != candidate.m_rtpmap) {
                    str.append("a=rtpmap:" + candidate.getCodec().getPayloadType() + " " + candidate.m_rtpmap + NEWLINE);
                    Codec codec = candidate.m_codec;
                    if(null != codec.getFmtp() && codec.getFmtp().length() > 0) {
                        str.append("a=fmtp:" + codec.getPayloadType() + " " + codec.getFmtp() + NEWLINE);
                    }
                }
            }

            str.append(m_fingerprint.serialize());

            Iterator<SRTPCrypto> iterCrypto = m_listCrypto.iterator();
            while(iterCrypto.hasNext()) {
                SRTPCrypto crypto = iterCrypto.next();
                str.append(crypto.serialize());
            }

            if(m_bRtcpMux) {
                str.append("a=rtcp-mux" + NEWLINE);
            }

            if(null != m_rtcpFb) {
                str.append(m_rtcpFb.serialize());
            }

            if(SDPType.SDP_TYPE_SENDONLY == m_sendRecvType) {
                str.append("a=sendonly" + NEWLINE);
            } else if(SDPType.SDP_TYPE_RECVONLY == m_sendRecvType) {
                str.append("a=recvonly" + NEWLINE);
            }

            Iterator<SSRCAttrib> iterSSRC = m_listSSRC.iterator();
            while(iterSSRC.hasNext()) {
                SSRCAttrib ssrcAttrib = iterSSRC.next();
                str.append(ssrcAttrib.serialize());
            }

            Iterator<String> iterList =  m_listAttr.iterator();
            while(iterList.hasNext()) {
                String val = iterList.next();
                str.append("a=" + val + NEWLINE);
            }

            return str.toString();
        }

        /**
         * Returns a serialized string version of the media description which can be included in an SDP document
         * @return A serialized string version of the media description which can be included in an SDP document
         */
        @Override
        public String toString() {
            return serialize(null);
        }

    }

    /**
     *
     * SDP internal parse context
     *
     */
    private class ParseState {
        String lastConnect = null;
        Media lastMedia = null;  

        private MediaCandidate getMediaCandidate(int pt) {
            MediaCandidate candidate = null;
            if(lastMedia.m_mapCandidates.containsKey(new Integer(pt))) {
                candidate = lastMedia.m_mapCandidates.get(new Integer(pt));
            }
            if(null == candidate ) {
                candidate = new MediaCandidate(pt);
                lastMedia.m_mapCandidates.put(new Integer(pt), candidate);
            }
            return candidate;
        }
    }

    private void parseMedia(String value) throws IOException {

        if("video".equalsIgnoreCase(value.substring(0, 5))) {

            if(null != m_video) {
                m_log.warn("SDP contains multiple media video sections.");
            }

            value = value.substring(6).trim();
            m_state.lastMedia = m_video = new Media(MediaType.MEDIA_TYPE_VIDEO, value);
            m_video.parseConnect(m_state.lastConnect);

        } else if("audio".equalsIgnoreCase(value.substring(0, 5))) {

            if(null != m_audio) {
                m_log.warn("SDP contains multiple media audio sections.");
            }

            value = value.substring(6).trim();
            m_state.lastMedia = m_audio = new Media(MediaType.MEDIA_TYPE_AUDIO, value);
            m_audio.parseConnect(m_state.lastConnect);

        }

    }
/*
    private Fingerprint getFingerprint() {
        if(null != m_state.lastMedia) {
            return m_state.lastMedia.m_fingerprint;
        } else {
            return m_fingerprint;
        }
    }
*/
    private void updateFingerprint() throws CloneNotSupportedException {

        //
        // Copy any SDP global ice properties to create media specific ones
        //
        if(m_fingerprint.isSet()) {

            if(null != m_video && false == m_video.m_fingerprint.isSet()) {
                m_video.m_fingerprint = m_fingerprint.clone();
            }

            if(null != m_audio && false == m_audio.m_fingerprint.isSet()) {
                m_audio.m_fingerprint = m_fingerprint.clone();
            }
        }

    }

    private ICEProperties getICEProperties() {
        if(null != m_state.lastMedia) {
            return m_state.lastMedia.m_iceProperties;
        } else {
            return m_iceProperties;
        }
    }

    private void updateICEProperties() throws CloneNotSupportedException {

        //
        // Copy any SDP global ice properties to create media specific ones
        //
        if(true == m_iceProperties.m_bIsSet) {

            if(null != m_video && false == m_video.m_iceProperties.m_bIsSet) {
                m_video.m_iceProperties = m_iceProperties.clone();
            }

            if(null != m_audio && false == m_audio.m_iceProperties.m_bIsSet) {
                m_audio.m_iceProperties = m_iceProperties.clone();
            }
        }

    }

    private void parseAttrib(String value) throws IOException {

        if(value.length() >= 6 && "rtpmap".equalsIgnoreCase(value.substring(0, 6))) {

            m_state.getMediaCandidate(MediaCandidate.extractPayloadType(value)).parseRtpmap(value);

        } else if(value.length() >= 4 && "fmtp".equalsIgnoreCase(value.substring(0, 4))) {

            MediaCandidate candidate = m_state.getMediaCandidate(MediaCandidate.extractPayloadType(value));
            candidate.m_codec.setFmtp(MediaCandidate.getPayloadDescription(value));

        } else if(value.length() >= 8 && "sendonly".equalsIgnoreCase(value.substring(0, 8))) {

            m_state.lastMedia.m_sendRecvType = SDPType.SDP_TYPE_SENDONLY;

        } else if(value.length() >= 8 && "recvonly".equalsIgnoreCase(value.substring(0, 8))) {

            m_state.lastMedia.m_sendRecvType = SDPType.SDP_TYPE_RECVONLY;

        } else if(value.length() >= 8 && "sendrecv".equalsIgnoreCase(value.substring(0, 8))) {

            m_state.lastMedia.m_sendRecvType = SDPType.SDP_TYPE_SENDRECV;

        } else if(value.length() >= 8 && "inactive".equalsIgnoreCase(value.substring(0, 8))) {

            m_state.lastMedia.m_bInactive = true;

        } else if(value.length() >= 8 && "rtcp-mux".equalsIgnoreCase(value.substring(0, 8))) {

            m_state.lastMedia.m_bRtcpMux = true;

        } else if(value.length() >= 7 && "rtcp-fb".equalsIgnoreCase(value.substring(0, 7))) {

            m_state.lastMedia.m_rtcpFb.parseItem(value);

        } else if(value.length() >= 12 && "fingerprint:".equalsIgnoreCase(value.substring(0, 12))) {

            if(null == m_state.lastMedia) {
                m_fingerprint = Fingerprint.parse(value);
            } else {
                m_state.lastMedia.m_fingerprint = Fingerprint.parse(value);
            }

        } else if(value.length() >= 7 && "crypto:".equalsIgnoreCase(value.substring(0, 7))) {

            m_state.lastMedia.m_listCrypto.add(SRTPCrypto.parse(value));

        } else if(value.length() >= 10 && "candidate:".equalsIgnoreCase(value.substring(0, 10))) {

            m_state.lastMedia.m_listIceCandidates.add(ICECandidate.parse(value));

        } else if(value.length() >= 18 && "remote-candidates:".equalsIgnoreCase(value.substring(0, 18))) {

            m_state.lastMedia.m_iceRemoteCandidates = ICERemoteCandidates.parse(value);

        } else if(value.length() >= 8 && "ice-lite".equalsIgnoreCase(value.substring(0, 8))) {

            getICEProperties().setICELite(true);

        } else if(value.length() >= 10 && "ice-ufrag:".equalsIgnoreCase(value.substring(0, 10))) {

            getICEProperties().setICEUfrag(value.substring(10).trim());

        } else if(value.length() >= 8 && "ice-pwd:".equalsIgnoreCase(value.substring(0, 8))) {

            getICEProperties().setICEPwd(value.substring(8).trim());

        } else if(value.length() >= 4 && "mid:".equalsIgnoreCase(value.substring(0, 4))) {

            //
            // 'a=mid:audio', 'a=mid:video' media attribute when using a=group:BUNDLE audio video' session attribtue 
            //

        } else if(value.length() >= 6 && "group:".equalsIgnoreCase(value.substring(0, 6))) {

            parseGroupAttribute(value);

        } else if(value.length() >= 5 && "ssrc:".equalsIgnoreCase(value.substring(0, 5))) {

            SSRCAttrib ssrcAttrib = SSRCAttrib.parse(value);
            m_state.lastMedia.m_listSSRC.add(ssrcAttrib);

        } else {

            if(null != m_state.lastMedia) {
                m_state.lastMedia.m_listAttr.add(value);
                //m_log.debug("SDP contains unparsed media attribute tag: a=" + value);
            } else {
                m_log.warn("SDP contains non-media specific attribute tag: a=" + value);
            }
        }
    }

    private void parse(String key, String value) throws IOException {
        char keyChar = key.charAt(0);

        try {

            switch(keyChar) {
                case 'v':
                    m_version = value;
                    break;
                case 'o':
                    m_owner = value;
                    break;
                case 's':
                    m_session = value;
                    break;
                case 't':
                    m_time = value;
                    break;
                case 'c':
                    m_state.lastConnect = value; 
                    if(null != m_state.lastMedia) {
                        m_state.lastMedia.parseConnect(value);
                    }
                    break;
                case 'm':
                    parseMedia(value);
                    break;
                case 'b':
                    if(null == m_state.lastMedia) {
                        m_bandwidth.parse(value); 
                    } else {
                        m_state.lastMedia.m_bandwidth.parse(value); 
                    } 
                    break;
                case 'a':
                    parseAttrib(value);
                    break;
                default:
                    //System.out.println("Ignoring SDP line: " + key + "=" + value);
                    m_log.warn("Ignoring SDP line: " + key + "=" + value);
                    break;
            }
        } catch(Exception e) {
            IOException io = new IOException("Error parsing SDP line: " + value + " " + e, e);
            io.setStackTrace(e.getStackTrace());
            throw io;
        }

        //System.out.println("'" +  key + "' -> '" + value + "'" + arrValues.length);
    }

    /**
     * Creates an SDP document instance from a textual description
     * @param text The text describing the SDP document contents
     * @return An initialized SDP document instance
     */
    public static SDP create(String text) throws Exception {
        int index;
        SDP sdp = new SDP();
        Media media;

        String [] arrLines = new String(text).trim().split("\n");
        for(index = 0; index < arrLines.length; index++) {

            int match = arrLines[index].indexOf('=');
  
            if(match <= 0) {
                continue;
            }

            String key = arrLines[index].substring(0, match).trim();
            String value = arrLines[index].substring(match + 1).trim();

            sdp.parse(key, value);

        }

        //
        // Invalidate media which has the "a=inactive" attribute set
        //
        if(null != sdp.m_audio && sdp.m_audio.isInactive()) {
            sdp.invalidateAudio();
        }

        if(null != sdp.m_video && sdp.m_video.isInactive()) {
            sdp.invalidateVideo();
        }

        // make sure that a corresponding m=rtpmap value exists
        MediaCandidate [] candidates = sdp.getAudioCandidates();
        for(index = 0; index < candidates.length; index++) {
            if(null == candidates[index].m_rtpmap) {
                switch(candidates[index].getCodec().getPayloadType()) {
                    case 0:
                        candidates[index].parseRtpmap("rtpmap:" + candidates[index].getCodec().getPayloadType() + 
                                                      " PCMU/8000");
                        break;
                    case 8:
                        candidates[index].parseRtpmap("rtpmap:" + candidates[index].getCodec().getPayloadType() + 
                                                      " PCMA/8000");
                        break;
                }
            }
        }

        sdp.updateICEProperties();
        sdp.updateFingerprint();

        sdp.updateTargets();

        // For an SDP which contains a media description of well known payload types, 

        sdp.m_rawDocument = text;

        return sdp; 
    }

    /**
     * <p>Updates any media level RTP and RTCP addresses and ports.</p>
     * <p>This method should be called after constructing a session description document serializing it.</i>
     */
    public void updateTargets() throws CloneNotSupportedException {

        //
        // Set the SDP target RTP / RTCP ports
        // 
        if(null != m_audio) {
            m_audio.updateTargets();
        }
        if(null != m_video) {
            m_video.updateTargets();
        }

    }

    /**
     * Retrieves a session description field <i>s=</i> of the SDP document
     * @return The session description field <i>s=</i> of the SDP document
     */
    public String getSession() {
        return m_session;
    }

    /**
     * Sets the session description field <i>s=</i> of the SDP document
     * @param session The  session description field <i>s=</i> of the SDP document
     */
    public void setSession(String session) {
        m_session = session;
    }

    /**
     * Retrieves a owner description field <i>o=</i> of the SDP document
     * @return The owner description field <i>o=</i> of the SDP document
     */
    public String getOwner() {
        return m_owner;
    }

    /**
     * Sets the owner description field <i>o=</i> of the SDP document
     * @param owner The owner description field <i>o=</i> of the SDP document
     */
    public void setOwner(String owner) {
        m_owner = owner;
    }


    /**
     * Retrieves the video <i>Media</i> description of the SDP document
     * @return the video <i>Media</i> description of the SDP document
     */
    public Media getVideoMedia() {
        return m_video;
    }

    /**
     * Retrieves the audio <i>Media</i> description of the SDP document
     * @return the audio <i>Media</i> description of the SDP document
     */
    public Media getAudioMedia() {
        return m_audio;
    }

    /**
     * Retrieves the SDP document level DTLS certificate <i>Fingerprint</i> 
     * @return The SDP document level DTLS certificate <i>Fingerprint</i> 
     */
    public Fingerprint getFingerprint() {
        return m_fingerprint;
    }

    /**
     * Tests if the SDP document includes the <i>a=group:BUNDLE audio video</i> document level attribute
     * @return <i>true</i> if the SDP document includes the <i>a=group:BUNDLE audio video</i> document level attribute, <i>false</i> otherwise
     */
    public boolean IsBundle() {
        return m_bIsBundle;
    }

    /**
     * Sets whether the SDP document should include the <i>a=group:BUNDLE audio video</i> document level attribute
     * @param bBundle Set to <i>true</i> if the SDP document includes the <i>a=group:BUNDLE audio video</i> document level attribute, <i>false</i> otherwise
     */
    public void setIsBundle(boolean bBundle) {
        m_bIsBundle = bBundle;
    }

    /**
     * Tests if the SDP document is associated with a request 
     * @return <i>true</i> if the SDP document is associated with a request, <i>false</i> otherwise
     */
    public boolean isRequest() {
        return m_bIsRequest;
    }

    /**
     * Sets whether the SDP document is associated with a request or response
     * @param bIsRequest Set to  <i>true</i> if the SDP document is associated with a request, <i>false</i> if associated with a response 
     */
    public void setIsRequest(boolean bIsRequest) {
        m_bIsRequest = bIsRequest;
        if(null != m_video) {
            m_video.setIsRequest(bIsRequest);
        }  
        if(null != m_audio) {
            m_audio.setIsRequest(bIsRequest);
        }  
    }

    /**
     * Retrieves the <i>DeviceType</i> associated with the SDP document
     * @return The <i>DeviceType</i> associated with the SDP document
     */
    public DeviceType getDeviceType() {
        return m_deviceType;
    }

    /**
     * Sets a <i>DeviceType</i> to be associated with the SDP document
     * @param deviceType A <i>DeviceType</i> to be associated with the SDP document
     */
    public void setDeviceType(DeviceType deviceType) {
        m_deviceType = deviceType;
    }

    /**
     * Invalidates any video media description from the SDP document
     */
    public void invalidateVideo() {
        m_video = null;
    }

    /**
     * Invalidates any audio media description from the SDP document
     */
    public void invalidateAudio() {
        m_audio = null;
    }

    /**
     * Sets the media audio port used by the audio media description
     * @param port The media audio port used by the audio media description
     */
    public void setAudioPort(int port) {
        if(null == m_audio) {
            m_audio = new Media(MediaType.MEDIA_TYPE_AUDIO);
        }
        m_audio.setDescription(Integer.valueOf(port).toString());
    }

    /**
     * Sets the media port used by the video media description
     * @param port The media video port used by the video media description
     */
    public void setVideoPort(int port) {
        if(null == m_video) {
            m_video = new Media(MediaType.MEDIA_TYPE_VIDEO);
        }
        m_video.setDescription(Integer.valueOf(port).toString());
    }

    /**
     * Returns a human readable string describing the target address associated with the SDP document
     * @param bPrintPort Set to <i>true</i> to print the port number in addition to the IP address
     * @return A human readable string describing the target address associated with the SDP document
     */
    public String printTargetIp(boolean bPrintPort) {
        if(null != m_audio && null != m_audio.getTarget().getAddress()) {
            if(bPrintPort) {
                return m_audio.getTarget().getAddress().toString();
            } else {
                return m_audio.getTarget().getAddress().getHostAddress();
            }
        } else if(null != m_video && null != m_video.getTarget().getAddress()) {
            if(bPrintPort) {
                return m_video.getTarget().getAddress().toString();
            } else {
                return m_video.getTarget().getAddress().getHostAddress();
            }
        } else {
            return "";
        }
    } 

    /**
     * Retrieves the video media description two-way transmission type
     * @return The video media description two-way transmission type
     */
    public SDPType getVideoSendRecvType() {
        if(null != m_video)
            return m_video.m_sendRecvType;
        else 
            return SDPType.SDP_TYPE_NONE;
    }

    /**
     * Sets the video media description two-way transmission type
     * @param sendRecvType The video media description two-way transmission type
     */
    public void setVideoSendRecvType(SDPType sendRecvType) {
        if(null != m_video)
            m_video.m_sendRecvType = sendRecvType;
    }

    /**
     * Retrieves the audio media description two-way transmission type
     * @return The audio media description two-way transmission type
     */
    public SDPType getAudioSendRecvType() {
        if(null != m_audio)
            return m_audio.m_sendRecvType;
        else 
            return SDPType.SDP_TYPE_NONE;
    }

    /**
     * Sets the audio media description two-way transmission type
     * @param sendRecvType The audio media description two-way transmission type
     */
    public void setAudioSendRecvType(SDPType sendRecvType) {
        if(null != m_audio)
            m_audio.m_sendRecvType = sendRecvType;
    }

    /**
     * Retrieves the <i>Bandwidth</i> attribute instance associated with the SDP document
     * @return The <i>Bandwidth</i> attribute instance associated with the SDP document
     */    
    public Bandwidth getBandwidth() {
        return m_bandwidth;
    }

    private static InetAddress getSafeAddress(InetAddress address) {
        if(null != address) {
            return address; 
        } else {  
            try {
                return InetAddress.getByName("0.0.0.0");
            } catch(UnknownHostException e) {
                return null;
            }
        }
    }

    /**
     * Sets the audio media description connect address <i>c=</i> 
     * @param ipAddress The audio media description connect address <i>c=</i> 
     */
    public void setAudioAddress(String ipAddress) throws UnknownHostException {
        if(null == m_audio) {
            m_audio = new Media(MediaType.MEDIA_TYPE_AUDIO);
        }
        m_audio.parseConnect(TRANSPORT_IP_DEFAULT + " " + ipAddress);
    }

    /**
     * Sets the video media description connect address <i>c=</i> 
     * @param ipAddress The video media description connect address <i>c=</i> 
     */
    public void setVideoAddress(String ipAddress) throws UnknownHostException {
        if(null == m_video) {
            m_video = new Media(MediaType.MEDIA_TYPE_VIDEO);
        }
        m_video.parseConnect(TRANSPORT_IP_DEFAULT + " " + ipAddress);
    }

    /**
     * Adds a audio media candidate to the list of audio media candidates
     * @param codec The audio media codec description
     */
    public void addAudioCandidate(Codec codec) throws CloneNotSupportedException {
        if(null == m_audio) {
            m_audio = new Media(MediaType.MEDIA_TYPE_AUDIO);
        }
        m_audio.addCandidate(codec);
    }

    /**
     * Adds a video media candidate to the list of video media candidates
     * @param codec The video media codec description
     */
    public void addVideoCandidate(Codec codec) throws CloneNotSupportedException {
        if(null == m_video) {
            m_video = new Media(MediaType.MEDIA_TYPE_VIDEO);
        }
        m_video.addCandidate(codec);
    }

    /**
     * Adds a custom document level attribute to the session description document 
     * @param attr A custom document level attribute to the session description document 
     */
    public void addSessionAttribute(String attr) {
        m_listSessionAttr.add(attr);
    }

    /**
     * Adds a custom audio attribute to the audio media description
     * @param attr A custom audio attribute to be added to the audio media description
     */
    public void addAudioAttribute(String attr) {
        if(null == m_audio) {
            m_audio = new Media(MediaType.MEDIA_TYPE_AUDIO);
        }
        m_audio.addAttribute(attr);
    }

    /**
     * Adds a custom video attribute to the video media description
     * @param attr A custom video attribute to be added to the video media description
     */
    public void addVideoAttribute(String attr) {
        if(null == m_video) {
            m_video = new Media(MediaType.MEDIA_TYPE_VIDEO);
        }
        m_video.addAttribute(attr);
    }

    /**
     * Retrieves all audio <i>MediaCandidate</i> entries associated with the audio media description as an array
     * @return An array of audio <i>MediaCandidate</i> entries associated with the audio media description
     */
    public MediaCandidate [] getAudioCandidates() {
        MediaCandidate[] arr = null;
        if(null != m_audio) {
            arr = m_audio.toArray();
        }
        if(null == arr) {
            arr = new MediaCandidate[0];
        }
        return arr;
    }

    /**
     *  Retrieves the count of audio media candidates associated with the document
     *  @return The count of audio media candidates associated with the document
     */
    public int getAudioCandidatesCount() {
      return null != m_audio ? m_audio.m_mapCandidates.size() : 0;
    }

    /**
     * Retrieves all video <i>MediaCandidate</i> entries associated with the video media description as an array
     * @return An array of video <i>MediaCandidate</i> entries associated with the video media description
     */
    public MediaCandidate [] getVideoCandidates() {
        MediaCandidate[] arr = null;
        if(null != m_video) {
            arr = m_video.toArray();
        }
        if(null == arr) {
            arr = new MediaCandidate[0];
        }
        return arr;
    }

    /**
     *  Retrieves the count of video media candidates associated with the document
     *  @return The count of video media candidates associated with the document
     */
    public int getVideoCandidatesCount() {
      return null != m_video ? m_video.m_mapCandidates.size() : 0;
    }

    /**
     * Retrieves the first audio <i>MediaCandidate</i> entry associated with the audio media description 
     * @return The first audio <i>MediaCandidate</i> entry associated with the audio media description 
     */
    public MediaCandidate getAudioCandidate() {
        MediaCandidate[] arr = getAudioCandidates();
        MediaCandidate candidate = null;
        if(null != arr && arr.length > 0) {
            candidate = arr[0]; 
        }
        return candidate;
    }

    /**
     * Retrieves the first video <i>MediaCandidate</i> entry associated with the video media description 
     * @return The first video <i>MediaCandidate</i> entry associated with the video media description 
     */
    public MediaCandidate getVideoCandidate() {
        MediaCandidate[] arr = getVideoCandidates();
        MediaCandidate candidate = null;
        if(null != arr && arr.length > 0) {
            candidate = arr[0]; 
        }
        return candidate;
    }

    private void ensureDefaults() throws IOException {
        String connectStr = null;

        if(null != m_video && m_video.m_mapCandidates.size() > 0 &&
           null == m_video.m_media) {
            throw new IOException("SDP video media port not set");
        }

        if(null != m_audio && m_audio.m_mapCandidates.size() > 0 &&
           null == m_audio.m_media) {
            throw new IOException("SDP audio media port not set");
        }

        if(null != m_audio && null == m_audio.m_connect && null != m_video) {
            connectStr = m_video.m_connect;
            m_audio.parseConnect(connectStr);
        }

        if(null != m_video && null == m_video.m_connect && null != m_audio) {
            connectStr = m_audio.m_connect;
            m_video.parseConnect(connectStr);
        }

        if(null == connectStr) {
            connectStr = (null != m_audio ? m_audio.m_connect : m_video.m_connect);
        }

        if(null == m_version || m_version.length() <= 0) {
            m_version = "0";
        }

        if(null == m_session || m_session.length() <= 0) {
            m_session = "default";
        }
        if(null == m_time || m_time.length() <= 0) {
            m_time = "0 0";
        }
        if(null == m_owner || m_owner.length() <= 0) {
            long tm = System.currentTimeMillis() / 1000;
            m_owner = "- " + tm + " " + tm + " " + connectStr;
        }

    }

    private boolean isBundled() {
        return (m_bIsBundle && null != m_audio && m_audio.m_mapCandidates.size() > 0 && 
                null != m_video && m_video.m_mapCandidates.size() > 0 && 
                m_audio.getConnect().getPort() == m_video.getConnect().getPort());
    }

    private void parseGroupAttribute(String line) {
        line = line.substring(6);
        String [] arr = line.split(" ");

        if(null != arr && arr.length > 0 && arr[0].trim().equalsIgnoreCase("BUNDLE")) {
            m_bIsBundle = true;
        }

    }

    /**
     * Returns a serialized string representation of the SDP document 
     * @return A serialized string representation of the SDP document 
     * @throws IOException on an error condition
     */
    public String serialize() throws IOException{
        String connectStr = null;
        Iterator<Integer> iter;
        StringBuffer str = new StringBuffer();

        ensureDefaults();
        if(null != m_audio) {
            connectStr = m_audio.m_connect;
        } else if(null != m_video) {
            connectStr = m_video.m_connect;
        }

        str.append("v=" + m_version + NEWLINE);
        str.append("o=" + m_owner + NEWLINE);
        str.append("s=" + m_session + NEWLINE);
        str.append("c=" + connectStr + NEWLINE);

        if(null != m_bandwidth) {
             str.append(m_bandwidth.serialize());
        }

        str.append("t=" + m_time + NEWLINE);

        Iterator<String> iterSessionAttributes =  m_listSessionAttr.iterator();
        while(iterSessionAttributes.hasNext()) {
            String val = iterSessionAttributes.next();
            str.append("a=" + val + NEWLINE);
        }

        if(isBundled()) {
            str.append("a=group:BUNDLE audio video" + NEWLINE);
        }

        if(m_fingerprint.isSet()) {
            str.append(m_fingerprint.serialize());
        }

        //if(m_iceProperties.isSet()) {
        //    str.append(m_audio.m_iceProperties.serialize());
        //}

        if(null != m_audio && false == m_audio.isInactive() && m_audio.m_mapCandidates.size() > 0) {
            str.append(m_audio.serialize(connectStr));
            if(isBundled()) {
                str.append("a=mid:audio" + NEWLINE);
            }
        }

        if(null != m_video && false == m_video.isInactive() && m_video.m_mapCandidates.size() > 0) {
            str.append(m_video.serialize(connectStr));
            if(isBundled()) {
                str.append("a=mid:video" + NEWLINE);
            }
        }

        return str.toString();
    }

/*
    private String toString() throws IOException {
       if(null != m_rawDocument) {
           return m_rawDocument;
       } else {
           return serialize();
       }
    }
*/

    /**
     * Returns a serialized string representation of the SDP document 
     * @return A serialized string representation of the SDP document 
     */
    @Override
    public String toString() {
        try {

           if(null != m_rawDocument) {
               return m_rawDocument;
           } else {
               return serialize();
           }
            
        } catch(IOException e) {
            m_log.error(e);
        }
        return "";
    }

    /**
     * Creates a transport type description field such as <i>RTP/AVP</i>
     * @param bIsMediaTransportSecure Set to <i>true</i> if the transport type includes security
     * @param bIsMediaAVPF Set to <i>true</i> if the transport type includes AVPF 
     * @return A transport type description field such as <i>RTP/AVP</i>
     */
    public static String createTransportType(boolean bIsMediaTransportSecure, boolean bIsMediaAVPF) {
        if(bIsMediaTransportSecure) {
            if(bIsMediaAVPF) {
                return TRANSPORT_RTP_SAVPF; 
            } else {
                return TRANSPORT_RTP_SAVP; 
            }
        } else {
            if(bIsMediaAVPF) {
                return TRANSPORT_RTP_AVPF; 
            } else {
                return TRANSPORT_RTP_AVP; 
            }
        }
    }

    /**
     * Creates a copy of the SDP document 
     * @return A copy of the SDP document 
     */
    public SDP clone() throws CloneNotSupportedException {
        SDP sdp = (SDP) super.clone();

        m_listSessionAttr = new ArrayList<String>(m_listSessionAttr.size());
        for(String attr : m_listSessionAttr) {
            m_listSessionAttr.add(new String(attr));
        }

        m_iceProperties = m_iceProperties.clone();
        m_bandwidth = m_bandwidth.clone();
        m_fingerprint = m_fingerprint.clone();

        if(null != m_video) {
            sdp.m_video = m_video.clone();
        }
        if(null != m_audio) {
            sdp.m_audio = m_audio.clone();
        }
        sdp.m_state = new ParseState();
        sdp.m_deviceType = m_deviceType.clone();
  
        return sdp;
    }

    public static void main(String [] args) {
        
        try {
/*
            Media media;
            SDP sdp = SDP.create(new String(SDPTest.sdpFirefox.getBytes()).trim());
            //sdp.getBandwidth().setAS(1024000);
            //sdp.getVideoMedia().getBandwidth().setTIAS(0);
            System.out.println(sdp.serialize());
*/
/*
            SDP sdp = SDP.create(new String(com.openvcx.test.SDPTest.sdpWebRTCTURN.getBytes()).trim());
            sdp.setAudioAddress("1.2.3.4");
            sdp.setVideoAddress("1.2.3.4");
            System.out.println(sdp.serialize());
*/
/*
            SDP sdp2 = new SDP();
            sdp2.setSession("test");
            sdp2.setAudioPort(2000);
            sdp2.setAudioAddress("127.0.0.1");
            sdp2.addAudioCandidate(new Codec("PCMA", 8, 8000));
            sdp2.addAudioCandidate(new Codec("telephone-event", 101, 8000, "0-15"));
            sdp2.getAudioMedia().addRTCPFB(new SDP.RTCPFB(SDP.RTCPFB.TYPE.CCM_FIR));
            
            //sdp2.getAudioMedia().addCrypto(new SRTPCrypto("1", SRTPCrypto.SRTPAlgorithm.AES_CM_128_HMAC_SHA1_80.toString(), new KeyGenerator().makeKeyBase64(SRTPCrypto.KEY_LENGTH)), false);
            System.out.println(sdp2.serialize());
*/

/*
            sdp.setVideoPort(2002);
            //sdp.setVideoAddress("127.0.0.3");
            sdp.addVideoCandidate(new Codec("H264", 97, 90000), "profile-level-id=42800c; test");
            sdp.addVideoAttribute("sendrecv");

            System.out.println(sdp.serialize());

            }
*/

        } catch(Exception e) {
            e.printStackTrace();
        }

    }


}



