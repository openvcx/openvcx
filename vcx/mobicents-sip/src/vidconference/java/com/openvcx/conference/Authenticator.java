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
import java.net.UnknownHostException;
import java.security.NoSuchAlgorithmException;
import java.util.Iterator;
import java.util.HashMap;
import java.util.Map;

import javax.servlet.sip.*;

import org.apache.log4j.Logger;

/**
 *
 * SIP request credentials authenticator for digest authentication
 *
 */
public class Authenticator {

    private final Logger m_log = Logger.getLogger(getClass());
    private HandlerContext m_handler = null;
    private ConferenceHandler m_confHandler = null;
    private AuthStore m_authStore = null;
    private AuthDB m_authDB = null;

    /**
     * Constructor used to initialize the instance
     * @param authStore The authentication store used for storing authenticated users
     * @param handler The master context store used by the application
     * @param confHandler A conference handler instance used for retrieving a conference definition defining conference specific behavior 
     */
    public Authenticator(AuthDB authDB, 
                         AuthStore authStore, 
                         HandlerContext handler, 
                         ConferenceHandler confHandler) {
        m_authStore = authStore;
        m_authDB = authDB;
        m_handler = handler;
        m_confHandler = confHandler;
    }

    /**
     * <p>Method to authenticate a SIP request and validate the request source</p>
     * This method also validates that the SIP request originates from a source defined with the configuration key <i>sip.address.list.source</i> in the master config file.
     * @param req The SIP request message
     * @param bAuth Set to <i>true</i> to perform digest authentication for SIP request credentials.
     * @return <i>true</i> if the request has been succesfully authenticated and validated, <i>false</i> otherwise
     * @throws IOException on an error condition
     */
    public boolean authenticateRequest(SipServletRequest req, boolean bAuth) throws IOException {

        //
        // Validate the originating IP source of the SIP message
        //
        if(false == validateRequestSource(req)) {
            return false;
        }
 
        AuthStore.User authUser = null;
        DeviceType deviceType = null;
        String conferencePass = null;
        boolean bStoreRespForFutureAuth = false;

        //
        // Validate user credentials
        //
        if(true == bAuth) {

            deviceType = m_handler.getDeviceTypeMappingTable().lookupDeviceType(req);

            authUser = m_authStore.findUser(User.toString(req.getFrom()), req.getInitialRemoteAddr());

            if("INVITE".equals(req.getMethod().toUpperCase())) {
                ConferenceDefinition conferenceDefinition = null;

                try {
                    conferenceDefinition = m_confHandler.loadConferenceDefinition(req);
                } catch(Exception e) {
                    m_log.error("Called party conference configuration not found: " + User.toString(req.getTo()) + ", " + e);
                }

                if(null == conferenceDefinition) {
                    //m_log.error("Called party conference configuration not found: " + User.toString(req.getTo()));
                    SipServletResponse resp = req.createResponse(SipServletResponse.SC_FORBIDDEN);
                    resp.send();
                    return false;
                }

                if(null == (conferencePass = conferenceDefinition.getPassword())) {
                    // Nothing to validate
                    return true;
                }

                if(null == authUser) {
                    authUser = new  AuthStore.User(User.toString(req.getFrom()));
                    m_authStore.insertUser(authUser, req.getInitialRemoteAddr());
                    m_log.debug("Inserted empty user");
                }

                if(null != conferencePass && validatePriorCredentials(req, conferencePass, authUser)) {
                    m_log.debug("validatePriorCredentials is true");
                    return true;
                }


            } else if("REGISTER".equals(req.getMethod().toUpperCase())) {

                int expires = req.getExpires(); // expires: 0 is un-registration
                if(0 == expires) {
                    m_authStore.removeUser(User.toString(req.getFrom()), req.getInitialRemoteAddr());
                    //m_log.debug("UNREGISTER removed user");
                    authUser = null;
                    bAuth = false;
                    return true;
                } else if(DeviceTypeWebRTC.isWebRTC(deviceType.getType())) {

                    // Our current WebRTC SIP stack does not respond to an INVITE authorization challenge
                    // so we created this webrtc specific hack which relies on the credentials provided by
                    // the client's REGISTER method
                    //
                    // The user was not previously authenticatd, but merely put into the authstore permitting
                    // any password, pending future conference password specific authentication which is 
                    // specific for INVITE
                    //
                    m_log.debug("WebRTC authentication mode for " + deviceType);

                    bStoreRespForFutureAuth = true;
                    if(null == authUser) {
                        authUser = new  AuthStore.User(User.toString(req.getFrom()));
                        m_authStore.insertUser(authUser, req.getInitialRemoteAddr());
                        m_log.debug("Inserted empty WebRTC user " + authUser);
                    }
                } else {
                    authUser = null;
                    bAuth = false;
                    m_log.debug("auth requested but authUser is null because no prior invite.... anyone can reg");
                }

            }

        }

        if(bAuth) {

            if(false == authUser.isExpired() && true == authUser.getValidated()) {
                //m_log.debug("User: " + authUser + " is still valid"); 
                return true;
            } else {
                m_log.debug("User: " + authUser + " IsExpired:" + authUser.isExpired() + ", getValidated:" + authUser.getValidated());
            }

            if(false == validateCredentials(req, conferencePass, bStoreRespForFutureAuth)) {
                return false;
            }
        }

        return true;
    }

    /**
     * A class for managing the SIP/HTTP <i>Authoriazation</i> header
     */
    private static class Authorization {
        private String m_scheme;
        private Map<String, String> m_map = new HashMap<String, String>();

        private static final String SCHEME_DIGEST   = "digest";

        public enum KEY {
            algorithm,
            nonce,
            username,
            realm,
            response,
            uri
        }

        public Authorization() {

        }

        public Authorization(String authorization) {
            parse(authorization);
        }

        public final String getScheme() {
            return m_scheme;
        }

        public void setScheme(String scheme) {
            m_scheme = scheme;
        }

        public void set(KEY key, String value) {
            set(key.toString(), value);
        }

        public void set(String key, String value) {
            m_map.put(key.trim().toLowerCase(), value);
        }

        public final String get(KEY key) {
            return get(key.toString());
        }
 
        public final String get(String key) {
            return m_map.get(key.toLowerCase());
        }

        public void parse(String authorization) {

            m_scheme = null;
            m_map.clear();

            if(null == authorization) {
                return;
            }

            String str = authorization.trim();
            int match = str.indexOf(' ');

            if(match > 0) {
                m_scheme = str.substring(0, match);
            } else {
                m_scheme = str;
                return;
            }

            String [] arr = str.substring(match + 1).trim().split(",");
            int index;

            for(index = 0; index < arr.length; index++) {
                match = arr[index].indexOf('=');
                String key = arr[index].substring(0, match).trim().toLowerCase();
                String value = Configuration.unQuote(arr[index].substring(match + 1).trim());
                m_map.put(key, value);
            }

        }

        public String toString() {
            String str = m_scheme + " ";
            int index = 0;
            Iterator iter = m_map.entrySet().iterator();
            while(iter.hasNext()) {
                Map.Entry pairs = (Map.Entry) iter.next();
                if(index++ > 0) {
                    str += ",";
                }
                str += pairs.getKey() + "=\"" + pairs.getValue() + "\"";
            }
            return str;
        }

        public static String createNonce() throws IOException {
            try {
               return KeyGenerator.toHex(new KeyGenerator().getRandomBytes(16, KeyGenerator.ALGO_MD5));
            } catch(NoSuchAlgorithmException e) {
                IOException io = new IOException(e);
                io.setStackTrace(e.getStackTrace());             
                throw io;
            }
        }

    }

    private String getUserRealm(SipServletRequest req) {
        String domain = User.toDomain(req.getFrom());

        return domain;
    }

    private boolean validateCredentials(SipServletRequest req, String conferencePass, boolean bStoreRespForFutureAuth) 
                        throws IOException {

        boolean bError = false;
        String authUsername = null;
        String clientDigest = null;
        Authorization reqAuth = null;
        AuthDB.User dbUser = null;
        AuthStore.User authUser = null;
        String realm = null;

//String realm = "mobicents.org";
//authUsername = "user1";
//String password = "user1";

        try {

            realm = getUserRealm(req);
            //m_log.debug("Realm: '"+realm+"'");

            String strHeaderAuth = req.getHeader("Authorization");
            if(null == strHeaderAuth) {
                m_log.debug(req.getMethod() + " request does not contain Authorization header");
                bError = true;
            } 

            if(!bError) {
                reqAuth = new Authorization(strHeaderAuth);
                clientDigest = reqAuth.get(Authorization.KEY.response);
                if(null == clientDigest || clientDigest.length() <= 0) {
                    m_log.debug(req.getMethod() + " request does not contain response digest field in Authorization header");
                    bError = true;
                } 

            }

            if(!bError) {
                authUsername = reqAuth.get(Authorization.KEY.username);

                if(bStoreRespForFutureAuth) {

                    //
                    // We're not authenticating this go around because we don't have a password to validate
                    // against.   We were called to store the client's digest response and H2 hash for future
                    // validation against a conference password.
                    //
                    String H1 = getH1(authUsername, realm, "");
                    dbUser = new AuthDB.User(authUsername, realm, H1);

                } else if(null != conferencePass) {

                    // Allow any username
                    String H1 = getH1(authUsername, realm, conferencePass);
                    dbUser = new AuthDB.User(authUsername, realm, H1);

                } else {
                    if(null == (dbUser = m_authDB.findUser(authUsername))) {
                        m_log.warn("Invalid username:'" + authUsername + "'");
                        bError = true;
                    }
                }
            }

            if(!bError) {
                String requestRealm = reqAuth.get(Authorization.KEY.realm);
                if(null == requestRealm || false == requestRealm.equals(dbUser.getRealm())) {
                    m_log.warn("Realm mismatch username:'" + authUsername + "', user realm:'" + dbUser.getRealm() + 
                                 "', request realm:'" + requestRealm + "'");
                    bError = true;
                }
            }

            if(!bError) {
                SipSession session = req.getSession();

                String strNonce = (String) session.getAttribute("NONCE");
                if(null == strNonce) {
                    //TODO: need to preserve the server generated nonce since apparently the session object isn't preserved yet
                    strNonce = reqAuth.get(Authorization.KEY.nonce);
                }
                //todo; verify nonce


                String strMethod = req.getMethod().toUpperCase();
                String strUri = reqAuth.get(Authorization.KEY.uri);
                String strH2 = getH2(strMethod, strUri);
                String strH1 = dbUser.getH1();
                String strRespHash = getDigest(strH1 , strNonce, strH2);
 
                //m_log.debug("Client digest resp:'"+clientDigest+"', method:'" + strMethod + "', uri:'" + strUri + "'");
                //m_log.debug("session nonce:'" + (String) session.getAttribute("NONCE") + "'");
                //m_log.debug("client echoed nonce:'" + reqAuth.get(Authorization.KEY.nonce) + "'");

                //m_log.debug("conferencePass: '"+ conferencePass + "'");
                //m_log.debug("Computed H2:'" + strH2 + "'");
                //m_log.debug("Client digest:'" + clientDigest + "'");
                //m_log.debug("Server calculated digest:'" + strRespHash + "'");

                if(bStoreRespForFutureAuth) {
                    authUser = new AuthStore.User(User.toString(req.getFrom()), authUsername);
                    authUser.setAuthResponse(strH2, strNonce, clientDigest);  // Used for future authentication 
                    authUser.setValidated(false);
                    authUser.updateAuthTime();
                    m_authStore.insertUser(authUser, req.getInitialRemoteAddr());
                    m_log.debug("Stored user REGISTER authorization for future conference specific auth");

                } else if(true == strRespHash.equals(clientDigest)) {
                    authUser = new AuthStore.User(User.toString(req.getFrom()), authUsername);
                    authUser.setValidated(true);
                    authUser.updateAuthTime();
                    m_authStore.insertUser(authUser, req.getInitialRemoteAddr());
                    //m_log.debug("succesful auth");
                } else {
                    bError = true;
                    m_log.warn("Client digest authentication mismatch for " + strMethod + ", from: " + 
                                User.toString(req.getFrom()) + ", to: " + User.toString(req.getTo()) + ", URI: '" + 
                                strUri + "', username: '" + reqAuth.get(Authorization.KEY.username) + "', realm: '" + 
                                reqAuth.get(Authorization.KEY.realm) + "'");

                }
            }

        } catch(NoSuchAlgorithmException e) {
            LogUtil.printError(m_log, "validateCredentials: ", e);
            IOException io = new IOException(e);
            io.setStackTrace(e.getStackTrace());
            throw io;
        }


        if(bError) {
            sendUnauthorized(req, realm);
        }

        return !bError;
    }

    private void sendUnauthorized(SipServletRequest req, String realm) throws IOException {

        //SipServletResponse resp=req.createResponse(SipServletResponse.SC_PROXY_AUTHENTICATION_REQUIRED);
        SipServletResponse resp = req.createResponse(SipServletResponse.SC_UNAUTHORIZED);

        Authorization respAuth = new Authorization(Authorization.SCHEME_DIGEST);
        String strNonce = respAuth.createNonce();
        respAuth.set(Authorization.KEY.realm, realm);
        respAuth.set(Authorization.KEY.algorithm, "MD5");
        respAuth.set(Authorization.KEY.nonce, strNonce);
        //resp.setHeader("ProxyAuthenticate", respAuth.toString()); 
        resp.setHeader("WWW-Authenticate", respAuth.toString());

        //SipSession session = req.getSession();
        //session.setAttribute("NONCE", strNonce);

        m_log.debug("Sending Unauthorized response for " + req.getMethod() + ":\n" + resp);
        resp.send();
    }

    private static String getH1(String username, String realm, String pass) throws NoSuchAlgorithmException {
        String strA1 = username + ":" + realm + ":" + pass;
        byte [] hash1 = KeyGenerator.createHash(strA1.getBytes(), KeyGenerator.ALGO_MD5);
        return KeyGenerator.toHex(hash1);
    }

    private static String getH2(String strMethod, String strUri) throws NoSuchAlgorithmException {
        String strA2 = strMethod + ":" + strUri;
        byte [] hash2 = KeyGenerator.createHash(strA2.getBytes(), KeyGenerator.ALGO_MD5);
        return KeyGenerator.toHex(hash2);
    }

    private static String getDigest(String strH1 , String strNonce, String strH2) throws NoSuchAlgorithmException {
        String strResp = strH1 + ":" + strNonce + ":" + strH2;
        byte [] hashResp = KeyGenerator.createHash(strResp.getBytes(), KeyGenerator.ALGO_MD5);
        return KeyGenerator.toHex(hashResp);
    }

    private boolean validatePriorCredentials(SipServletRequest req, String conferencePass, AuthStore.User authUser) 
                        throws IOException {
        String strH1;
        String strRespHash;
        String realm = getUserRealm(req);

        if(authUser.isExpired()) {
            return false;
        }

        if(null == authUser.getDigestResponse() || null == authUser.getDigestH2() || null == authUser.getDigestNonce()) {
            return false;
        }

        authUser.setValidated(false);

        try {
            strH1 = getH1(authUser.getAuthUsername(), realm, conferencePass);
            strRespHash = getDigest(strH1, authUser.getDigestNonce(), authUser.getDigestH2());
        } catch(NoSuchAlgorithmException e) {
            LogUtil.printError(m_log, "validatePriorCredentials: ", e);
            IOException io = new IOException(e);
            io.setStackTrace(e.getStackTrace());
            throw io;
        }

        //m_log.debug("validatePriorCredentials  H1:"+strH1+" , username:"+ authUser.getAuthUsername() + ", realm:" + realm + ", pass:" + conferencePass);

        if(authUser.getDigestResponse().equals(strRespHash)) {
            authUser.setValidated(true);
            //m_log.debug("validatePriorCredentials OK authUser.getDigestResponse():"+authUser.getDigestResponse()+", strRespHash:"+strRespHash);
            return true;
        } else {
            //m_log.debug("validatePriorCredentials authUser.getDigestResponse():"+authUser.getDigestResponse()+", strRespHash:"+strRespHash);
            return false;
        }
    }

    private boolean validateRequestSource(SipServletRequest req) throws IOException {

        NetIPv4Mask [] permittedIps = m_handler.getPermittedIps();

        if(null == permittedIps || permittedIps.length <= 0) {
            return true;
        }

        String strIpSrc = req.getRemoteAddr();

        try {

            for(int index = 0; index < permittedIps.length; index++) {
                if(true == permittedIps[index].isMatch(strIpSrc)) {
                    return true;
                }
            }

        } catch(UnknownHostException e) {
            m_log.error(e);
        }

        m_log.warn("SIP " + req.getMethod() + " is not permitted from source: " + strIpSrc + ", from: " + 
                   new User(req.getFrom()) + ", to: " + new User(req.getTo()));

        SipServletResponse resp = req.createResponse(SipServletResponse.SC_FORBIDDEN);
        resp.send();

        return false;
    }


    public static void main(String [] args) {
        String str ="digest  username=\"test\", realm=\"mobicents.org\", nonce=\"a98d1f62476cb45264e92b61c4f3c7f3\",response=\"9e22dc1ecd0047ae51f1900aed7d05bc\",uri=\"sip:webrtc.mobicents.org\",algorithm=MD5";
        //System.out.println("Auth test: " + str);
        Authorization auth = new Authorization(str);
        //System.out.println("realm enum:" + auth.get(Authorization.KEY.realm));
        //System.out.println("realm str:" + auth.get("realm"));
        //auth.set(Authorization.KEY.nonce, "abc");

        System.out.println("--" + auth.toString() + "--");
    }

};
