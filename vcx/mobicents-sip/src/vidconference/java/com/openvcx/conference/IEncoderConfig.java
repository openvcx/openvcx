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

import org.apache.log4j.Logger;

/**
 *
 * <p>Interface definition for an audio/video encoder configuration</i>
 *
 */
public interface IEncoderConfig {
    
    /**
     * Retrieves the initialization string which was used to configure this encoder configuration 
     */
    public String getInitializerString();

    /**
     * Retrieves the video codec name
     */
    public String getVideoCodecName();

    /**
     * Retrieves the video encoded slice size maximum
     */
    public int getVideoSliceSizeMax();

    /**
     * Retrieves the video encoder bitrate
     */
    public long getVideoBitrate();

    /**
     * Retrieves the video encoder maximum bitrate
     */
    public long getVideoBitrateMax();

    /**
     * Retrieves the video encoder minimum bitrate
     */
    public long getVideoBitrateMin();

    /**
     * Retrieves the device type associated with this encoder configuration
     */
    public DeviceType getDeviceType();

}
