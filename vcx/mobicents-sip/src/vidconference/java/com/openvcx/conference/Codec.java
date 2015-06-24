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

import java.lang.Cloneable;
import java.lang.CloneNotSupportedException;

import org.apache.log4j.Logger;

/**
 * <p>A video or audio media codec</p>
 * <p>A codec instance should be instantiated through the <i>CodecFactory</i></p>
 */
public class Codec implements Cloneable {

    private final Logger m_log = Logger.getLogger(getClass());

    /**
     * <i>telephone-event</i> - DTMF
     */
    public static final String TELEPHONE_EVENT     = "telephone-event";

    /**
     * <i>red</i> - Redundant Data payload
     */
    public static final String FEC_RED             = "red";
    
    /**
     * <i>ulpfec</i> - Forward Error Control 
     */
    public static final String FEC_ULPFEC          = "ulpfec";

    /**
     * Codec name
     */
    protected String m_name;

    /**
     * Codec SDP RTP payload type identifier 
     */
    protected int m_payloadType;

    /**
     * Codec SDP RTP clock rate in Hz 
     */
    protected int m_clockRate;

    /**
     * Codec media channel count
     */
    protected int m_channelCount;

    /**
     * Codec SDP ftmp attribute containing configuration parameters 
     */
    protected String m_fmtp;

    /**
     * No argument constructor
     */
    Codec() {
    }

    /**
     * Constructor
     * @param name The codec name
     */
    Codec(String name) {
        m_name = name;
    }

    /**
     * Constructor
     * @param name The codec name
     * @param payloadType The SDP RTP payloadtype identifier
     */
    Codec(String name, int payloadType) {
        m_name = name;
        m_payloadType = payloadType;
    }

    /**
     * Constructor
     * @param name The codec name
     * @param clockRate The SDP RTP media clock rate in Hz 
     * @param payloadType The SDP RTP payloadtype identifier
     */
    Codec(String name, int clockRate, int payloadType) {
        m_name = name;
        m_clockRate = clockRate;
        m_payloadType = payloadType;
    }

    /**
     * Constructor
     * @param name The codec name
     * @param clockRate The SDP RTP media clock rate in Hz 
     * @param payloadType The SDP RTP payloadtype identifier
     * @param fmtp The SDP fmtp attribute containing configuration parameters 
     */
    Codec(String name, int clockRate, int payloadType, String fmtp) {
        m_name = name;
        m_clockRate = clockRate;
        m_payloadType = payloadType;
        m_channelCount = 1;
        setFmtp(fmtp);
    }

    /**
     * Creates a copy of the codec instance
     */
     @Override
    public Codec clone() throws CloneNotSupportedException {
        return (Codec) super.clone();
    }

    /**
     * <p>Tests if two codecs are the same.</p>
     * <p>The parameters such as codec name and SDP RTP clock rate are compared</p>
     * @param codec The codec to be compared against this instance
     * @param bComparePayloadType If set to <i>true</i> the test will also compare the RTP payload type identifiers
     * @return <i>true</i> if both codecs have the same properties, <i>false</i> otherwise
     */
    public boolean equals(Codec codec, boolean bComparePayloadType) {
        if(false == (getName().equalsIgnoreCase(codec.getName()))) {
            return false;
        } else if(m_clockRate != codec.m_clockRate) {
            return false; 
        } else if(bComparePayloadType && m_payloadType != codec.m_payloadType) {
            return false;
        }
        return true;
    }

    /**
     * <p>Parses a codec initializer string</p>
     * <p>The format of the initializer string should consist of the following elements</i>
     * <p><i> codec name / clock rate / [ Optional channel count ] / payload type / [ Optional fmtp ]</i></p>
     * @param str A codec initializer string
     */
    public static Codec parse(String str) {
         
        Codec codec = null;
        String [] arr = str.split("/");

        if(arr.length > 0) {

            codec = CodecFactory.create(arr[0].trim());

            if(arr.length > 1) {
                codec.setClockRate(Integer.parseInt(arr[1].trim())); 
                if(arr.length > 2) {
                    String channels = arr[2].trim();
                    String pt = null;
                    String fmtp = null;

                    if(arr.length > 3) {
                        pt = arr[3].trim();
                    }
                    if(arr.length > 4) {
                        fmtp = arr[4].trim();
                    }

                    if(null == fmtp) {
                        //
                        // We are either missing the channels or fmtp elements
                        //

                        try {
                            //
                            // If the last element is an integer, it cannot be an fmtp but must be the payload type
                            // with a present optional channel count
                            //
                            Integer.valueOf(pt);
                        } catch(NumberFormatException e) {
                            //
                            // The last element was not an integer so it must be an fmtp and we must be missing the
                            //  optional channel count
                            //
                            codec.setFmtp(pt);
                            pt = channels;
                            channels = null;
                        }
                    } else if(null == pt) {
                        //
                        // We are missing the both the optional channel count and fmtp
                        //
                        pt = channels;
                    } else {
                        //
                        // We have all optional (channel, and fmtp) elements present
                        //
                        codec.setFmtp(fmtp);
                    }

                    if(null != channels) {
                        codec.setChannelCount(Integer.valueOf(channels));
                    }
                    codec.setPayloadType(Integer.parseInt(pt)); 
                    return codec;
                }
            }
        }
 
        return null;
    }

    /**
     * Creates a human readable codec description
     */
    public String toString() {
        return m_payloadType + " " + getRtpmapCodecDescriptor() + (null != m_fmtp ? (" " + m_fmtp) : "");
    }

    /**
     * <p>Retrieves the SDP rtpmap attribute value which follows the payload type descriptor</p>
     * @return A codec descriptor string such as <i>MPEG4-GENERIC/48000/2</i>
     */
    public String getRtpmapCodecDescriptor() {
       String str = getName() + "/" + m_clockRate; 
       if(m_channelCount > 1) {
           str += "/" + m_channelCount;
       }
       return str;
    }

    /**
     * <p>Retrieves the name of the codec</p>
     */
    public String getName() {
        return null != m_name ? m_name : "";
    }

    /**
     * Sets the name of the codec
     * @param name The coddc name
     */
    public void setName(String name) {
        m_name = name;
    }

    /**
     * Retrieves the codec clock rate in Hz
     */
    public int getClockRate() {
        return m_clockRate;
    }

    /**
     * Sets the name of the codec
     * @param clockRate The codec clock rate in Hz 
     */
    public void setClockRate(int clockRate) {
        m_clockRate = clockRate;
    }

    /**
     * <p>Retrieves the actual output media clock rate of the codec.</p>
     * <p>A derived implementation may set this to be different from the configured SDP clock rate</p>
     * @return The codec output media clock rate
     */
    public int getOutputClockRate() {
        return m_clockRate;
    }

    /**
     * Retrieves the channel count
     * @return The codec channel count
     */
    public int getChannelCount() {
        return m_channelCount;
    }

    /**
     * Sets the chanel count 
     * @param channelCount The codec channel count
     */
    public void setChannelCount(int channelCount) {
        m_channelCount = channelCount;
    }

    /**
     * Retrieves the codec RTP payload type identifier
     * @return The RTP payload type identifier
     */
    public int getPayloadType() {
        return m_payloadType;
    }

    /**
     * Sets the RTP payload type identifier 
     * @param payloadType The RTP payload type identifier
     */
    public void setPayloadType(int payloadType) {
        m_payloadType = payloadType;
    }

    /**
     * Retrieves the codec SDP fmtp attribute
     * @return fmtp The SDP fmtp attribute containing configuration parameters 
     */
    public String getFmtp() {
        return m_fmtp;
    }

    /**
     * Sets the codec SDP fmtp attribute
     * @param fmtp The SDP fmtp attribute containing configuration parameters 
     */
    public void setFmtp(String fmtp) {
        m_fmtp = fmtp;
    }

}
