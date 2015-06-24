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
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;


import javax.servlet.ServletException;
import javax.servlet.sip.*;

import org.apache.log4j.Logger;


/**
 *
 * SIP INVITE requestor
 *
 */
public class B2BClient {

    private final Logger m_log = Logger.getLogger(getClass());
    private SipFactory m_sipFactory;
    private final static String CLIENT_USER_AGENT                 =  "Conference-bridge v0.1";

    public B2BClient(SipFactory sipFactory) {
        m_sipFactory = sipFactory;
    }

    public SipServletRequest sendInvite(SipApplicationSession applicationSession, 
                                        Call call, 
                                        User from, 
                                        User to, 
                                        URIUtil.ContactURI contact, 
                                        URIUtil.ContactURI route, 
                                        SDP sdp) 
                throws ServletException, IOException {

        //SipURI contactUri = (SipURI) m_sipFactory.createURI(contact.getUri());
        //m_log.debug("sendInvite contact:" + contact + ", username:" + contact.getUsername() + ", domain:" + contact.getDomain());

/*
        SipServletRequest req2 = req.getSession(true).createRequest("INVTE");
        req2.setHeader("Allow", "INVITE,ACK,CANCEL,BYE");
        //req2.setHeader("Contact", "sip:videobridge@127.0.0.1:54156;transport=ws");
*/

        SipServletRequest req2 = m_sipFactory.createRequest(applicationSession, "INVITE", 
                                                            from.getSipUri(), to.getSipUri()); 

        SipURI requestUri = null;
        if(null != contact) {
            requestUri = m_sipFactory.createSipURI(contact.getUsername(), contact.getDomain());
            if(null != contact.getTransport()) {
                requestUri.setTransportParam(contact.getTransport());
            }
        } else {
            requestUri = (SipURI) m_sipFactory.createURI(to.getSipUri());
        }

        if(null != route) {
            req2.pushRoute(m_sipFactory.createSipURI("", route.getDomain()));
        }

        //SipServletRequest req2 = req.getB2buaHelper().createRequest(req); 
        //req2.setHeader("From", "sip:1234@exampledomain.com");
        req2.setHeader("User-Agent", CLIENT_USER_AGENT);
        req2.setHeader("To", to.getSipUri());
        req2.setRequestURI(requestUri);

        if(null != sdp) {
            req2.setContent(sdp.serialize().getBytes(), "application/sdp");
        }
        m_log.debug("Sending INVITE:\n" + req2);

        SipSession session = req2.getSession(true);
        session.setAttribute(ConferenceHandler.ATTR_CALL, call);
        session.setAttribute(ConferenceHandler.ATTR_CONFERENCE_NAME, call.getFrom().getName());

        req2.send();

        return req2;
    }

}
