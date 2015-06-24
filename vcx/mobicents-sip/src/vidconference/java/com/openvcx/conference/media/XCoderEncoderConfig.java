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

import org.apache.log4j.Logger;

/**
 *
 * <p>An encoder configuration implementation for the Media Processor (Video Streaming Server) media back-end processor</p>
 * @see <a target="_new" href="http://openvcx.com">http://openvcx.com</a>
 *
 */
public class XCoderEncoderConfig implements IEncoderConfig {

    private final Logger m_log = Logger.getLogger(getClass());

    private String m_xcodeStr;
    private String m_videoCodecName;
    private long m_videoBitrate;
    private long m_videoBitrateMax;
    private long m_videoBitrateMin;
    private int m_videoSliceSizeMax;
    private DeviceType m_deviceType;

    /**
     * Constructor used to initialize this instance
     * @param xcodeStr A Media Processor (Video Streaming Server) xcode transcoder configuration string
     */
    public XCoderEncoderConfig(String xcodeStr) {
        init(xcodeStr);
        m_deviceType = null;
    }

    /**
     * Constructor used to initialize this instance
     * @param xcodeStr An Media Processor (Video Streaming Server) xcode transcoder configuration string
     * @param deviceType The device type that this transcoder configuration is set for
     */
    public XCoderEncoderConfig(String xcodeStr, DeviceType deviceType) {
        init(xcodeStr);
        m_deviceType = deviceType;
    }

    private void reset() {
        m_xcodeStr = null;
        m_videoCodecName = null;
        m_videoBitrate = 0;
        m_videoBitrateMax = 0;
        m_videoBitrateMin = 0;
        m_videoSliceSizeMax = 0;
    }

    private void init(String xcodeStr) {
        reset();
        m_xcodeStr = xcodeStr;
        parse(m_xcodeStr);
    } 

    public String getInitializerString() {
        return m_xcodeStr;
    }

    /**
     * Retrieves the video codec name
     */
    public String getVideoCodecName() {
        return m_videoCodecName;
    }

    /**
     * <p>Retrieves the video encoded slice size maximum</p>
     */
    public int getVideoSliceSizeMax() {
        return m_videoSliceSizeMax;
    }

    /**
     * Retrieves the video encoder bitrate
     */
    public long getVideoBitrate() {
        return m_videoBitrate;
    }

    /**
     * Retrieves the video encoder maximum bitrate
     */
    public long getVideoBitrateMax() {
        return m_videoBitrateMax;
    }

    /**
     * Retrieves the video encoder minimum bitrate
     */
    public long getVideoBitrateMin() {
        return m_videoBitrateMin;
    }

    /**
     * Retrieves the device type associated with this encoder configuration
     */
    public DeviceType getDeviceType() {
        return m_deviceType;
    }

    /**
     * Retrieves a human readable encoder configuration string
     */
    public String toString() {
        String description = "";
        if(null != m_xcodeStr) {
            description += " xcode=" + m_xcodeStr;
        }
        if(null != m_videoCodecName) {
            description += ", (videoCodec:" + m_videoCodecName + ")";
        }
        return description;
    }

    private void parse(String xcodeStr) {

        if(null == xcodeStr || xcodeStr.length() <= 0) {
            return;
        }

        String [] arr = xcodeStr.split(",");
        for(int index = 0; index < arr.length; index++) {
            
            String [] kvs = arr[index].split("=");
            if(kvs.length == 2) {
                if("vc".equalsIgnoreCase(kvs[0]) || "videoCodec".equalsIgnoreCase(kvs[0])) {
                    m_videoCodecName = kvs[1];
                } else if("vbmax".equalsIgnoreCase(kvs[0]) || "videoBitrateMax".equalsIgnoreCase(kvs[0])) {
                    m_videoBitrateMax = Long.parseLong(kvs[1]);
                } else if("vbmin".equalsIgnoreCase(kvs[0]) || "videoBitrateMin".equalsIgnoreCase(kvs[0])) {
                    m_videoBitrateMin = Long.parseLong(kvs[1]);
                } else if("vb".equalsIgnoreCase(kvs[0]) || "videoBitrate".equalsIgnoreCase(kvs[0])) {
                    m_videoBitrate = Long.parseLong(kvs[1]);
                } else if("vslmax".equalsIgnoreCase(kvs[0]) || "videoSliceSizeMax".equalsIgnoreCase(kvs[0])) {
                    m_videoSliceSizeMax = Integer.parseInt(kvs[1]);
                 }
            }
         }
    }


}
