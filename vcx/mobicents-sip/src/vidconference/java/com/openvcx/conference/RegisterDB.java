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

import java.io.IOException;
import java.util.Iterator;
import java.util.HashMap;
import java.util.Map;

import org.apache.log4j.Logger;


/**
 *
 * A database store to maintain locally registered SIP users and expiration times
 *
 */
public class RegisterDB {

    private final Logger m_log = Logger.getLogger(getClass());
    private Map<String, RegisteredUser> m_map = null;
    private String m_realm;
    
    /**
     * Constructor used to initialize this instance
     */
    public RegisterDB() {
        m_map = new HashMap<String, RegisteredUser>();
    }

    /**
     *
     */
    public URIUtil.ContactURI getUserContactUri(User to) {
        URIUtil.ContactURI contact = null;

        RegisteredUser registeredUser = findUser(to.toString());
        if(null != registeredUser) {
            contact = registeredUser.getContact();
        }

        return contact;
    }

    /**
     * <p>Adds a registered SIP user into the database.</p>
     * <p>If the user is already in the databse the insertion time is updated.</p>
     * @param uri The user SIP URI
     * @param contact The user SIP contact URI
     * @param expires The registration expiration time in seconds
     */
    public void insertUser(String uri, URIUtil.ContactURI contact, int expires)  {
        RegisteredUser user = new RegisteredUser(stripUserUri(uri), contact, expires);

        m_log.debug("RegisterDB insertUser: " + user.toString());

        user.setTime();

        synchronized(m_map) {
            m_map.put(user.getUri(), user);         
        }
    }

    /**
     * Removes a SIP user from the database 
     * @param uri The user SIP URI
     */
    public void removeUser(String uri)  {
        String userName = stripUserUri(uri);

        m_log.debug("RegisterDB removeUser: '" + uri + "'");

        synchronized(m_map) {
            m_map.remove(userName);         
        }
    }

    /**
     * Finds a registered SIP user in the database 
     * @param uri The user SIP URI
     * @return The registered user instance
     */
    public RegisteredUser findUser(String uri) {
        String userName = stripUserUri(uri);
        RegisteredUser user = null;

        synchronized(m_map) {
            user = m_map.get(userName);
            if(null != user && user.isExpired()) {
                m_log.debug("RegisterDB removing expired user: '" + uri + "'");
                m_map.remove(userName);         
                user = null;
            }
        }
        return user;
    }

    private String stripUserUri(String uri) {

        // Strip any 'sip:' prefix from the input sip uri name
        if(uri.length() > 4 && "sip:".equalsIgnoreCase(uri.substring(0, 4))) {
            uri = uri .substring(4);
        }

        return uri;
    }

    /**
     * <p>A class representing a registered SIP user.</p>
     * <p>Instances of this class are created internally by calling <i>RegisterDB.insertUser</i></p>
     */
    public static class RegisteredUser {
        private String m_uri;
        private URIUtil.ContactURI m_contact;
        private long m_insertTime;
        private int m_expires;

        /**
         * Constructor used to initialize this instance
         * @param uri The user SIP URI
         * @param contact The user SIP contact URI
         * @param expires The registration expiration time in seconds
         */
        private RegisteredUser(String uri, URIUtil.ContactURI contact, int expires) {
            m_uri = uri;
            m_contact = contact;
            m_expires = expires;
            m_insertTime = 0;
        }

        /**
         * Sets the insertion time of the user
         */
        private void setTime() {
            m_insertTime = System.currentTimeMillis();
        }

        /**
         * Tests if a registered user has expired its registration time
         */
        private boolean isExpired() {
            if(m_expires <= 0 || m_insertTime + (m_expires * 1000) < System.currentTimeMillis()) {
                return true;
            } else {
                return false;
            }
        }

        /**
         * Retrieves the The user SIP URI
         * @return The user SIP URI
         */
        public final String getUri() {
            return m_uri;
        }

        /**
         * Retrieves the The user SIP contact URI
         * @return The user SIP contact URI
         */
        public final URIUtil.ContactURI getContact() {
            return m_contact;
        }

        /**
         * Obtains a human readable description of the registered user
         */
        public String toString() {
            return "'" + m_uri + "' contact: '" + m_contact.toString() + "', expires: " + m_expires;
        }

    }

}
