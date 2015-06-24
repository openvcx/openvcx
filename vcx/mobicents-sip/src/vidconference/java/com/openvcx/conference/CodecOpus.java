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
 *
 * A media codec implementation for the OPUS audio codec 
 *
 */
public class CodecOpus extends Codec implements Cloneable {

    private final Logger m_log = Logger.getLogger(getClass());

    private int m_playbackRate = 0;
    private int m_captureRate = 0;

    CodecOpus(String name) {
        m_name = name;
    }

    CodecOpus(String name, int payloadType) {
        m_name = name;
        m_payloadType = payloadType;
    }

    CodecOpus(String name, int clockRate, int payloadType) {
        m_name = name;
        m_clockRate = clockRate;
        m_payloadType = payloadType;
    }

    CodecOpus(String name, int clockRate, int payloadType, String fmtp) {
        m_name = name;
        m_clockRate = clockRate;
        m_payloadType = payloadType;
        m_channelCount = 1;
        setFmtp(fmtp);
    }

    /**
     * Creates a new copy of this instance
     */
     @Override
    public CodecOpus clone() throws CloneNotSupportedException {
        return (CodecOpus) super.clone();
    }

    /**
     * Sets the <i>fmtp</i> configuration parameters used to initialize the codec
     * @param fmtp The SDP fmtp attribute of the codec
     */
    @Override
    public void setFmtp(String fmtp) {
        m_fmtp = fmtp;
        int index;


        if(null != m_fmtp) {
             String [] arr = m_fmtp.split(";");

             if(arr.length > 0) {
                 for(index = 0; index < arr.length; index++) {

                     String [] keyVal = arr[index].trim().split("=");

                     if("maxplaybackrate".equalsIgnoreCase(keyVal[0].trim())) {
                         m_playbackRate = Integer.parseInt(keyVal[1].trim()); 
                     } else if("sprop-maxcapturerate".equalsIgnoreCase(keyVal[0].trim())) {
                         m_captureRate = Integer.parseInt(keyVal[1].trim()); 
                     }

                 }
             }
        }
    }

    /**
     * <p>Retrieves the actual output media clock rate of the codec.</p>
     * <p>According to http://tools.ietf.org/html/draft-spittka-payload-rtp-opus-03
     * the actual media output rate of OPUS may be different than the SDP published rate of 48000Hz.  The media output rate can be set in <i>fmtp</i> configuration parameters
     * @see <a target="_new" href="http://tools.ietf.org/html/draft-spittka-payload-rtp-opus-03">http://tools.ietf.org/html/draft-spittka-payload-rtp-opus-03</a>
     * @return The codec output media clock rate
     */
    @Override
    public int getOutputClockRate() {
        return (m_playbackRate > 0) ? m_playbackRate : getClockRate();
    }  

    /**
     * Retrieves the value of the fmtp configuration parameter <i>maxplaybackrate</i>
     * @return The max playback rate in Hz
     */
    public int getPlaybackRate() {
        return m_playbackRate;
    }
    
    /**
     * Retrieves the value of the fmtp configuration parameter <i>sprop-maxcapturerate</i>
     * @return The max capture rate in Hz
     */
    public int getCaptureRate() {
        return m_captureRate;
    }

}
