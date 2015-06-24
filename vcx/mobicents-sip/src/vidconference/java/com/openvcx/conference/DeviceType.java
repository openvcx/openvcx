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

import javax.servlet.sip.Address;

/**
 *
 * Definition type of an audio/video SIP endpoint
 *
 */
public class DeviceType implements Cloneable {

    private String m_type;

    public DeviceType() {
        setType(null);
    }

    public DeviceType(String type) {
        setType(type);
    }

    /**
     * Retrieves the device type
     * @return The device type
     */
    public String getType() {
        return m_type;
    }

    /**
     * Sets the device type
     * @param type The device type
     */
    public void setType(String type) {
        m_type = type;
    }

    /**
     * Tests comparison between two <i>DeviceType</i>instances 
     * @param type The device type to be compared
     * @return <i>true</i> if the two devices are of the same type, </i>false</i> otherwise
     */
    public boolean equals(DeviceType type) {
        if(null != m_type && null != type && m_type.equalsIgnoreCase(type.getType())) {
            return true;
        }
        return false;
    }

    public String toString() {
        return null == m_type ? "device type is not set" : m_type;
    }

    /**
     * Creates a duplicate copy of this instance
     */
    @Override
    public DeviceType clone() throws CloneNotSupportedException {
        DeviceType deviceType = (DeviceType) super.clone();
        return deviceType;
    }
}
