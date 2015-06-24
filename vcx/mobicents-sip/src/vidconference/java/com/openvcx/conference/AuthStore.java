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

import java.io.IOException;
import java.util.Iterator;
import java.util.HashMap;
import java.util.Map;

import org.apache.log4j.Logger;

/**
 *
 * An Authentication Store for storing authenticated SIP users within the application context 
 * after they have been succesfully authenticated through the Authenticator
 *
 */
public class AuthStore {

    private final Logger m_log = Logger.getLogger(getClass());
    private HandlerContext m_handler = null;
    private Map<String, AuthStore.User> m_map = null;

    // For WebRTC this has to be > then the re-registration interval
    private static long CREDENTIALS_EXPIRE_TM    = (2 * 60 * 1000);

    //TODO: thread routine should remove expired users
    
    /**
     * Constructor used to initialize the instance
     * @param handler The master context store used by the application
     */
    public AuthStore(HandlerContext handler) {
        m_handler = handler;
        m_map = new HashMap<String, AuthStore.User>();
    }

    /**
     * Looks up a SIP username with the application context
     * @param sipUri the SIP URI username
     * @param remoteAddr The remote address or location of where the user connection is originating 
     */
    public synchronized User findUser(String sipUri, String remoteAddr) {
        User user = m_map.get(makeKey(sipUri, remoteAddr));

        if(null != user) {

            //if(bExpire && user.m_lastAuthTime + user.m_expireDurationTm < System.currentTimeMillis()) {
            //    m_log.debug("User: " + user + " has expired.  Removing from auth store.");
            //    m_map.remove(sipUri);
            //    user = null;
            //}

        }
        return user;
    }

    /**
     * inserts a SIP username into the application context
     * @param user A user instance representing an authenticated SIP user
     * @param remoteAddr The remote address or location of where the user connection is originating 
     */
    public synchronized User insertUser(User user, String remoteAddr) {
        return m_map.put(makeKey(user.m_sipUri, remoteAddr), user);
    }

    /**
     * Removes a SIP username from the application context
     * @param sipUri the SIP URI username
     * @param remoteAddr The remote address or location of where the user connection is originating 
     */
    public synchronized void removeUser(String sipUri, String remoteAddr) {
        m_map.remove(makeKey(sipUri, remoteAddr));
    }

    private String makeKey(String sipUri, String remoteAddr) {
        String key = sipUri; 
        if(null != remoteAddr) {
            sipUri += ":" + remoteAddr;
        }
        return key;
    }


    /**
     * A class defining a user which will be used in the AuthStore database
     */
    public static class User {

        private String m_authUsername;
        //private String m_authRealm;
        private String m_sipUri;
        private String m_sipUsername;
        private String m_sipDomain;
        //private String m_remoteAddr;
        private long m_lastAuthTime;
        private long m_expireDurationTm;
        private boolean m_bValidated;

        private String m_digestH2;
        private String m_digestNonce;
        private String m_digestResponse;

        public User(String sipUri) {
            set(sipUri, null);
        }

        public User(String sipUri, String authName) {
            set(sipUri, authName);
        }

        private void set(String sipUri, String authName) {
            m_sipUri = sipUri;
            m_sipUsername = ConfigUtil.getSIPUsernamePart(sipUri);
            m_sipDomain = ConfigUtil.getSIPDomainPart(sipUri);
            m_authUsername = (null == authName ? m_sipUsername : authName); 
            m_lastAuthTime = 0;
            m_expireDurationTm = CREDENTIALS_EXPIRE_TM;
            m_bValidated = false;
        }

        public String getAuthUsername() {
            return m_authUsername;
        }

        public void setAuthResponse(String digestH2, String digestNonce, String digestResponse) {
            m_digestH2 = digestH2;
            m_digestNonce = digestNonce;
            m_digestResponse = digestResponse;
        }
        public String getDigestH2() {
            return m_digestH2;
        }
        public String getDigestNonce() {
            return m_digestNonce;
        }
        public String getDigestResponse() {
            return m_digestResponse;
        }

        public void updateAuthTime() {
            m_lastAuthTime = System.currentTimeMillis();
        } 

        public boolean getValidated() {
            return m_bValidated;
        }

        public void setValidated(boolean bValidated) {
            m_bValidated = bValidated;
        }

        public boolean isExpired() {
          if(m_lastAuthTime + m_expireDurationTm < System.currentTimeMillis()) {
              return true;
          } else {
              return false;
          }
        }

        public String toString() {
            return m_sipUri + ", auth:" + m_authUsername;
        }
    }

}
