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

import java.util.Iterator;
import java.util.List;

/**
 * Utility class for retrieving a <i>EncoderConfig</i> instance for an particular device type
 */
public class EncoderConfigLoader {

    private final Logger m_log = Logger.getLogger(getClass());

    private ConferenceDefinition m_conferenceDefinition;

    public EncoderConfigLoader(final ConferenceDefinition conferenceDefinition) {
        m_conferenceDefinition = conferenceDefinition;
    }


    private IEncoderConfig findEncoderConfig(String codecName, final DeviceType deviceType, 
                                               List<IEncoderConfig> listEncoders) {

        IEncoderConfig encoderConfig = null;
        Iterator<IEncoderConfig> iterEncoders;

        if(null == (iterEncoders = listEncoders.iterator())) {
            return null;
        }

        while(iterEncoders.hasNext()) {
            IEncoderConfig encoderConfigTemp = iterEncoders.next();

            if((null == deviceType && null == encoderConfigTemp.getDeviceType()) || 
               (null != deviceType && true == deviceType.equals(encoderConfigTemp.getDeviceType()))) {

                //TODO: in addition to matching by name, this should match by profile/level also
                if(codecName.equalsIgnoreCase(encoderConfigTemp.getVideoCodecName())) {
                    encoderConfig = encoderConfigTemp;
                    break;
                }
            }
        }

        return encoderConfig;
    }

    /**
     * <p>Tries to find a suitable encoder configuration</p>
     * @param codec The media video codec to look for
     * @param deviceType The encoder specific device type configuration to look for
     * @param namingI The naming conversion interface for translating the SDP codec name to the underlying media implementation naming convention
     */
    public IEncoderConfig getEncoderConfig(final Codec codec, final DeviceType deviceType, ISDPNaming namingI) {
        IEncoderConfig encoderConfig = null;

        if(null == codec) {
            return null;
        }

        //m_log.debug("TRANSCODERCONFIG GETENCODERCONFIG deviceType: " + deviceType);

        List<IEncoderConfig> listEncoders;
        String codecName = namingI.getImplCodecName(codec.getName());

        if(null == codecName || null == (listEncoders = m_conferenceDefinition.getEncoderList())) {
            return null;
        }

        if(null == (encoderConfig = findEncoderConfig(codecName, deviceType, listEncoders)) && null != deviceType) {
            encoderConfig = findEncoderConfig(codecName, null, listEncoders);
        }

        //if(null!=encoderConfig) m_log.debug("TRANSCODERCONFIG GETENCODERCONFIG vb:" + encoderConfig.getVideoBitrate());

        return encoderConfig;
    }

}
