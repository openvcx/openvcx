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
 * <p>SIP back message handler used to forward/proxy INVITE requests
 * directly to the called endpoint without conference specific handling.</p>
 * <p>This is an incomplete implementation</p>
 *
 */
public class B2BHandler implements IHandler {

    private final Logger m_log = Logger.getLogger(getClass());
    private SipServlet m_sipServlet;
    private RegisterDB m_registerDB;

    /**
     * Constructor used to initialize this instance
     */ 
    public B2BHandler(SipServlet sipServlet, RegisterDB registerDB) {
        m_sipServlet = sipServlet;
        m_registerDB = registerDB;
    }

    /**
     * SIP BYE message handler
     * @param req The SIP request message
     */
    public void onBye(SipServletRequest req) throws ServletException, IOException {

    }

    /**
     * SIP INVITE message handler
     * @param req The SIP request message
     */
    public void onInvite(SipServletRequest req) throws ServletException, IOException {

        String to = "sip:user2@gmail.com";
        SipURI sipUri = null;

        B2buaHelper helper = req.getB2buaHelper();	
        SipFactory sipFactory = (SipFactory) m_sipServlet.getServletContext().getAttribute(m_sipServlet.SIP_FACTORY);

        RegisterDB.RegisteredUser registeredUser = m_registerDB.findUser(to);
        if(null != registeredUser) {
            m_log.debug("B2B onInvite found registered user: " + registeredUser.getUri());
            URIUtil.ContactURI contactUri = registeredUser.getContact();
            if(null != contactUri) {
                sipUri = (SipURI) sipFactory.createURI(contactUri.getUri());
                if(null != contactUri.getTransport()) {
                    sipUri.setTransportParam(contactUri.getTransport());
                }
            } 

        }
        m_log.debug("B2B onInviite to:" + to + ", sipUri:" + sipUri);
        SipServletRequest req2 = helper.createRequest(req); 
        req2.setHeader("To", to);
        req2.setRequestURI(sipUri);
        m_log.debug("REQ2:" + req2);
        req2.send();


/*
        B2buaHelper helper = req.getB2buaHelper();	
        helper.createResponseToOriginalRequest(req.getSession(), SipServletResponse.SC_TRYING, "").send();

        //SipFactory sipFactory = (SipFactory) m_sipServlet.getServletContext().getAttribute("javax.servlet.sip.SipFactory");
        SipFactory sipFactory = (SipFactory) m_sipServlet.getServletContext().getAttribute(m_sipServlet.SIP_FACTORY);

        //Map<String, List<String>> headers=new HashMap<String, List<String>>();
	//List<String> toHeaderList = new ArrayList<String>();
	//toHeaderList.add("sip:user2@gmail.com");
	//headers.put("To", toHeaderList);

        //SipServletRequest req2 = helper.createRequest(req, true, headers);
        SipServletRequest req2 = sipFactory.createRequest(req.getSession().getApplicationSession(), "INVITE", req.getFrom().getURI(), req.getTo().getURI());
        SipURI sipUri = (SipURI) sipFactory.createURI("sip:user2@127.0.0.1");
        req2.setRequestURI(sipUri);	
        m_log.debug("req.getSession():" + req.getSession());
        m_log.debug("req2.getSession():" + req2.getSession());
        req2.getB2buaHelper().linkSipSessions(req.getSession(), req2.getSession());
        req2.setContent(req.getContent(), req.getContentType());
        m_log.info("req2= " + req2);
        req2.getSession().setAttribute("originalRequest", req);
        req2.send();
*/

/*
        if(req.isInitial()) {
            Proxy proxy = req.getProxy();
            proxy.setRecordRoute(true);
	    proxy.setSupervised(true);
	    proxy.proxyTo(req.getRequestURI()); // bobs uri
	}
*/
    }

    /**
     * SIP REGISTER message handler
     * @param req The SIP request message
     */
    public void onRegister(SipServletRequest req) throws ServletException, IOException {
    }

    /**
     * SIP 2xx succesful message handler
     * @param resp  The SIP response message
     */
    public void onSuccessResponse(SipServletResponse resp) throws ServletException, IOException {
    }

    /**
     * SIP 4xx error message handler
     * @param resp The SIP response message
     */
    public void onErrorResponse(SipServletResponse resp) throws ServletException, IOException {
    }


}
