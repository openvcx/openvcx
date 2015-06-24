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

import com.openvcx.util.ConfigUtil;

import javax.servlet.sip.Address;

/**
 *
 * Class for representing a user (endpoint) in an audio/video call
 *
 */
public class User implements IUser {

    private String m_name = null;
    private STUNPolicy m_stunPolicy;

    /**
     * The STUN policy 
     */
    public enum STUNPolicy {
        /**
         * STUN is disabled.  This is the default STUN policy
         */
        NONE,

        /**
         * STUN is always enabled.  STUN binding requests will be sent on each RTP / RTCP channel(s).
         */
        ENABLED_ALWAYS,

        /**
         * STUN is enabled only if the remote party has stun enabled.  STUN will become enabled upon receiving a remote STUN binding request
         */
        ENABLED_IF_PEER,

        /**
         * STUN binding responder is enabled.  No STUN binding requests will be sent.
         */
        RESPOND_ONLY 
    }

    /**
     * Constructor used to initialize this instance
     * @param address A <i>javax.servlet.sip.Address</i> such as a SIP URI
     */
    public User(Address address) {
        m_name = parseAddress(address);
        m_stunPolicy = STUNPolicy.NONE;
    }

    /**
     * Constructor used to initialize this instance
     * @param sipUri A SIP URI of the user
     */
    public User(String sipUri) {
        m_name = parseSipUri(sipUri);
        m_stunPolicy = STUNPolicy.NONE;
    }

    /**
     * Obtains a string representation of the <i>javax.servlet.sip.Address</i>
     */
    public static String toString(Address address) {
        return parseAddress(address);
    }

    /**
     * Obtains a string representation of the username portion of the <i>javax.servlet.sip.Address</i>
     */
    public static String toUsername(Address address) {
        return ConfigUtil.getSIPUsernamePart(parseAddress(address));
    }

    /**
     * Obtains a string representation of the domain portion of the <i>javax.servlet.sip.Address</i>
     */
    public static String toDomain(Address address) {
        return ConfigUtil.getSIPDomainPart(parseAddress(address));
    }

    /**
     * <p>Obtains a string representation of the <i>javax.servlet.sip.Address</i></p>
     * <p>This is the same as calling {@link #toString()}</p>
     */
    public static String parseAddress(Address address) {
        String sipURI = null;

        if(null != address && null != (sipURI = address.getURI().toString())) {
            sipURI = parseSipUri(sipURI);
        }

        return sipURI;
    }

    private static String parseSipUri(String sipURI) {

        //
        // Example SIP Address could be: 'sip:username@domain.com;user=phone'
        // sipURI would be 'username@domain.com'
        //

        if(sipURI.length() > 4 && "sip:".equalsIgnoreCase(sipURI.substring(0, 4))) {
            sipURI = sipURI.substring(4); 
        }

        int match = sipURI.indexOf(';');
        if(match > 0) {
            sipURI = sipURI.substring(0, match); 
        }

        return sipURI;
    }

    /**
     * Retrieves the SIP user name used to initialize this instance.
     */
    public String getName() {
        return m_name;
    }

    /**
     * <p>Retrieves the SIP URI name used to initialize this instance.</p>
     * <p>This will add <i>sip:</i> before the SIP user name</p> 
     */
    public String getSipUri() {
        if(null != m_name) {
            return "sip:" + m_name;
        }
        return m_name;
    }

    /**
     * Retrieves the STUN policy of this user
     * @return The STUN policy of this user
     */
    @Override
    public STUNPolicy getStunPolicy() {
       return m_stunPolicy;
    }

    /**
     * Sets STUN policy of this user
     * @param stunPolicy The STUN policy of this user
     */
    public void setStunPolicy(STUNPolicy stunPolicy) {
        m_stunPolicy = stunPolicy;
    }

    /**
     * <p>Retrieves a string representation of this user</p>
     * <p>This is the same as calling {@link #getName()}</p>
     * @return The user name
     */
    @Override
    public String toString() {
        return getName();
    }

}
