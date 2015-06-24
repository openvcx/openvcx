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
import java.util.Iterator;

import javax.servlet.sip.SipServletMessage;

import org.apache.log4j.Logger;

/**
 *
 * <p>Device type mapping table loader for mapping User-Agent strings to user defined device types.</p>
 * <p>Device types are defined in the master application configuration file using the below syntax.</p> 
 * <p><i>user.agent.string.match.1="WebRTCPhone/webkit/android"</i><br>
 *    <i>user.agent.device.stype.1="webrtc_android"</i></p>
 *
 */
public class DeviceTypeMappingTable {
  
    private final Logger m_log = Logger.getLogger(getClass());

    private OrderedHashMap<String, Pair<String, String>> m_mapDeviceTypes = new OrderedHashMap<String, Pair<String, String>>();
    
    private static final String CONFIG_SERVER_USER_AGENT_STRING_MATCH = "user.agent.string.match";
    private static final String CONFIG_SERVER_USER_AGENT_DEVICE_STYPE = "user.agent.device.stype";

    private static final String USER_AGENT                            = "User-Agent";

    /**
     * Constructor used to initialize this instance
     * @param config The application configuration instance
     */
    public DeviceTypeMappingTable(Configuration config)  {
        loadMappings(config);
    }

    /**
     * Looks up a device type given a SIP request message
     * @param message The remote SIP request message
     */
    public DeviceType lookupDeviceType(SipServletMessage message) {
       return lookupDeviceType(message.getHeader(USER_AGENT));
    }

    /**
     * Looks up a device type given a User-Agent string
     * @param userAgent The User-Agent of the remote SIP requesting device
     */
    public DeviceType lookupDeviceType(String userAgent) {

        if(null != userAgent) {

            userAgent = userAgent.toUpperCase();

            Iterator<Pair<String, String>>  iter = m_mapDeviceTypes.getOrderedIterator();
            while(iter.hasNext()) {
                Pair<String, String> pair = (Pair<String, String>) iter.next();
                
                if(userAgent.contains(pair.getKey())) {
                    return new DeviceType(pair.getValue());
                }
            }

        }

        return new DeviceType();
    }

    private void loadMappings(Configuration config) {

        m_mapDeviceTypes.clear();

        for(int index = 0; index < 30; index++) {

            String keyMatch = CONFIG_SERVER_USER_AGENT_STRING_MATCH + "." + index;
            String valueMatch = null;
            String valueDevice = null;

            try {
                valueMatch = config.getString(keyMatch);
            } catch(IOException e) {
                continue;
            }

            String keyDevice = CONFIG_SERVER_USER_AGENT_DEVICE_STYPE + "." + index;

            try {
                valueDevice  = config.getString(keyDevice);
            } catch(IOException e) {
                m_log.warn("Configuration is mising " + keyDevice);
                continue;
            }

            if(null != valueMatch && null != valueDevice) {
                m_log.debug("Device mapping index " + index + ": '" + valueMatch + "' -> '" + valueDevice + "'");
                
                m_mapDeviceTypes.put(valueMatch.toUpperCase(), 
                    new Pair<String, String>(valueMatch.toUpperCase(), valueDevice));
            }
        }

    }

/*
    public static void main(String [] args) {

        try {
            Configuration config =  new Configuration("../../../test/debug-ngconference.conf"); 
            DeviceTypeMappingTable dmt = new DeviceTypeMappingTable(config);
            //DeviceType dt = dmt.lookupDeviceType("WebRTCPhone/mozilla/unknown");
            DeviceType dt = dmt.lookupDeviceType("BRIa");
            System.out.println("DEVICE TYPE: " + dt);


        } catch(Exception e) {
            e.printStackTrace();
        }
    }
*/

}
