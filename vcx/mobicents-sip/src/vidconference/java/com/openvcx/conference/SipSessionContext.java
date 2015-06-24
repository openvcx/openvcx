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

import java.io.Serializable;

import javax.servlet.sip.SipApplicationSession;

import org.apache.log4j.Logger;

/**
 *
 * SIP session context for preserving the Sip session throughout the application
 *
 */
public class SipSessionContext implements Serializable {

    private final Logger m_log = Logger.getLogger(getClass());

    private static final long serialVersionUID = 1L;
    private String m_sessionId;
    private SipApplicationSession m_sipApplicationSession;

    /**
     * Constructor used to initialize an instance
     * @param sessionId A unique session id
     * @param sipApplicationSession A <i>javax.servlet.sip.SipApplicationSession</i> context used for creating and sending SIP messages
     */
    public SipSessionContext(String sessionId, SipApplicationSession sipApplicationSession) {
        setId(sessionId);
        setSipApplicationSession(sipApplicationSession);
    }

    /**
     * Retrieves the associated <i>SipApplicationSession</i> instance
     * @return The  <i>javax.servlet.sip.SipApplicationSession</i> context associated with this instance.
     */
    public SipApplicationSession getSipApplicationSession() {
        return m_sipApplicationSession;
    }

    /**
     * Retrieves the associated <i>SipApplicationSession</i> instance
     * @param sipApplicationSession A <i>javax.servlet.sip.SipApplicationSession</i> context to be associated with this instance.
     */
    public void setSipApplicationSession(SipApplicationSession sipApplicationSession) {
        m_sipApplicationSession = sipApplicationSession;
    }

    /**
     * Retrieves the associated session id
     * @return The associated session id
     */
    public String getSessionId() {
        return m_sessionId;
    }

    /**
     * Sets an associated session id
     * @param sessionId session The associated session id
     */
    public void setId(String sessionId) {
        m_sessionId = sessionId;
    }

}
