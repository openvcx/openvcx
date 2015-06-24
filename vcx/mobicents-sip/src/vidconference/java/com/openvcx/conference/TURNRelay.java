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

//import com.openvcx.util.ConfigUtil;

import java.net.InetAddress;
import java.net.UnknownHostException;


/**
 *
 *
 */
public class TURNRelay implements Cloneable {

    SDP.ICEAddress m_relayAddress;
    SDP.ICEAddress m_peerPermissionAddress;

    public TURNRelay() {
        m_relayAddress = null;
        m_peerPermissionAddress = null;
    }

    public TURNRelay(String strAddress) throws UnknownHostException {
        setAddress(strAddress);
    }

    public SDP.ICEAddress getAddress() {
        return m_relayAddress;
    }

    public void setAddress(String strAddress) throws UnknownHostException {
        m_relayAddress = new SDP.ICEAddress(strAddress);
    }

    public SDP.ICEAddress getPeerPermission() {
        return m_peerPermissionAddress;
    }
    public void setPeerPermission(SDP.ICEAddress address) throws UnknownHostException {
        m_peerPermissionAddress = address;
    }

/*
    public void set(TURNRelay turnRelay) throws UnknownHostException {
        if(null != turnRelay) {
            m_relayAddress = turnRelay.m_relayAddress.clone();
        } else {
            m_relayAddress = null;
        }
    }
*/
    public TURNRelay clone() throws CloneNotSupportedException {
        TURNRelay turnRelay = (TURNRelay) super.clone();
        turnRelay.m_relayAddress = m_relayAddress.clone();
        return turnRelay;
    }

}
