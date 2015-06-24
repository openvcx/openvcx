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
 * <p>A User Database for persisting SIP user credentials throughout a SIP session.</p>
 * <p>This naive implementation does not use file / database storage.</p>
 *
 */
public class AuthDB {

    private final Logger m_log = Logger.getLogger(getClass());
    private HandlerContext m_handler = null;
    private Map<String, AuthDB.User> m_map = null;
    private boolean m_bIsInit = false;
    private String m_realm;
    
    /**
     * Constructor used to initialize the instance
     * @param handler The master context store used by the application
     */
    public AuthDB(HandlerContext handler) {
        m_handler = handler;
        m_map = new HashMap<String, AuthDB.User>();
    }

    /**
     * <p>Initializer for the database</p>
     * <p>Any file / storage loading should happen here</p>
     */
    public void init() {
        //TODO: load users from file / db storage
        // pass='user1', hex(md5(ser1:mobicents.org:user1))
        m_map.put("user1", new User("user1", "mobicents.org", "56b21d7c5f9f16e94404f5906f66c41c"));
        m_bIsInit = true;
    }

    /**
     * looks up a user in the database
     * @param userName A SIP username
     */
    public User findUser(String userName) {
        return m_map.get(userName);
    }

    /**
     * A database user entity associating a username with a password which should be stored as a 
     * one way hash only.
     */
    public static class User {
        private String m_name;
        private String m_realm;
        private String m_H1; // Digest name:realm:password MD5 Hash

        /**
         * Constructor used to initialize the instance
         * @param name The SIP username
         * @param realm The SIP realm used in authentication
         * @param H1 The SIP H1 digest authentication hash.  The H1 field should be the hexadecimal representation of the MD-5 hash of the concatenation of <i>username:realm:password</i>
         */
        public User(String name, String realm, String H1) {
            m_name = name;
            m_realm = realm;
            m_H1 = H1;
        }

        /**
         * Retrieves the SIP username of the instance
         * @return The SIP username of the instance
         */
        public final String getName() {
            return m_name;
        }

        /**
         * Retrieves the SIP realm used in authentication associated with this instance
         * @return The SIP realm used in authentication 
         */
        public final String getRealm() {
            return m_realm;
        }

        /**
         * <p>Retrieves the SIP H1 digest authentication hash associated with this instance.</p>
         * <p>The H1 field should be the hexadecimal representation of the MD-5 hash of the concatenation of <i>username:realm:password</i></p>
         * @return The SIP H1 digest authentication hash.  
         */
        public final String getH1() {
            return m_H1;
        }

    }

}
