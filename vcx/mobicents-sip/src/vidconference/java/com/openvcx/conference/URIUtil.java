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

import java.lang.Cloneable;
import java.lang.CloneNotSupportedException;
import java.net.UnknownHostException;

import org.apache.log4j.Logger;

/**
 *
 * Utility for manipulating a SIP URI
 *
 */
public class URIUtil{

    //private final Logger m_log = Logger.getLogger(getClass());

    /**
     * A class used to encapsulate the contact header URI in a SIP message
     */
    public static class ContactURI implements Cloneable{

        private final Logger m_log = Logger.getLogger(getClass());

        private String m_str;
        private String m_uri;
        private String m_username;
        private String m_domain;
        private int m_port;
        private String m_transport;

        /**
         * Constructor used to initialize the instance
         * @param contactHeader The contact header field in a SIP message
         */
        public ContactURI(String contactHeader) { 
            m_str = contactHeader;
            m_uri = null;
            m_username = null;
            m_domain= null;
            m_port = 0;
            m_transport = null;


            int index;
            String s = null;
            if((index = m_str.indexOf("<sip:")) >= 0) {
                s = m_str.substring(index + 5);
                if((index = s.indexOf(">")) > 0) {
                    s = s.substring(0, index);
                }
            } else if((index = m_str.indexOf("sip:")) >= 0) {
                s = m_str.substring(index + 4);
            }

            if(null != s) {
                String [] arr = s.split(";");
                for(index = 0; index < arr.length; index++) {
                    if(0 == index) {
                        m_uri = arr[index];
                    } else if(arr[index].length() > 10 && 
                              "transport=".equalsIgnoreCase(arr[index].substring(0, 10))) { 
                        m_transport = arr[index].substring(10);
                    }
                }

            } else {
                m_uri = contactHeader;
            }

            if(null != m_uri) {

                String [] arr = m_uri.split(":");
                if(null != arr && arr.length > 0) {
                    m_uri= arr[0]; 
                    if(arr.length > 1) {
                        m_port = Integer.valueOf(arr[1]);
                    }
                }
            }

            if((index = m_uri.indexOf("@")) >= 0) {
                m_username = ConfigUtil.getSIPUsernamePart(m_uri);
                m_domain = ConfigUtil.getSIPDomainPart(m_uri);
            } else {
                m_username = null;
                m_domain = ConfigUtil.getSIPUsernamePart(m_uri);
            }

        }

        private String getUriAndPort() {
            String str = "sip:";

            if(null != m_username && null != m_domain) {
                str += m_username + "@" + m_domain;
            } else {
                str += m_uri;
            }

            if(0 != m_port) {
                str += ":" + m_port;
            }
            return str;
        }

        /**
         * Retrieves the SIP URI and port that was included in the contact header
         * @return The SIP URI and port that was included in the contact header
         */
        public String getUri() {
            return getUriAndPort();
        }

        /**
         * Retrieves the SIP username that was included in the contact header
         * @return The SIP username that was included in the contact header
         */
        public String getUsername() {
            return m_username;
        }

        /**
         * Sets the SIP username for this contact SIP URI
         * @param username The SIP username for this contact SIP URI
         */
        public void setUsername(String username) {
            m_username = username;
        }

        /**
         * Retrieves the SIP address domain that was included in the contact header
         * @return The SIP address domain that was included in the contact header
         */
        public String getDomain() {
            String str = m_domain;

            if(0 != m_port) {
                str += ":" + m_port;
            }

            return str;
        }

        /**
         * Retrieves the SIP address transport method that was included in the contact header
         * @return The SIP address transport method that was included in the contact header
         */
        public String getTransport() {
            return m_transport;
        }
 
        /**
         * Retrieves the SIP address port that was included in the contact header
         * @return The SIP address port that was included in the contact header
         */
        public int getPort() {
            return m_port;
        }

        /**
         * Creates a copy of this instance
         */
        public ContactURI clone() throws CloneNotSupportedException {
            return (ContactURI) super.clone();
        }

        /**
         * Retrieves the contact header URI as a parseable string
         */
        public String toString() {
            String str = getUriAndPort();
            if(null != m_transport) {
                str += ";transport=" + m_transport;
            }
            return str;
        }
    }

/*
    public static void main(String [] args) {
        try {
            //String contact = "\"User Two\" <sip:user2@10.0.2.3:50040;transport=TCP;ob>;+sip.ice;reg-id=1;+sip.instance=\"<urn:uuid:8B69427B-8138-4B1F-B405-B9570A8F81A5>\"";
            //String contact = "\"User Two\" <sip:user2@10.0.2.3:50040;;ob>;+sip.ice;reg-id=1;+sip.instance=\"<urn:uuid:8B69427B-8138-4B1F-B405-B9570A8F81A5>\"";
            String contact="<sip:user1@127.0.0.1:7515;rinstance=dae2ecca93ddb94b>";

            URIUtil.ContactURI u = new URIUtil.ContactURI(contact);
            System.out.println(u.getUri());
            System.out.println(u.getTransport());

        } catch(Exception e) {
           System.out.println(e);
        }

    } 
*/

}
