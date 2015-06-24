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
import java.io.FileNotFoundException;
import java.io.IOException;
import java.io.Serializable;
import java.lang.SecurityException;
import java.net.UnknownHostException;
import java.security.NoSuchAlgorithmException;
import java.util.ArrayList;
import java.util.Iterator;
import java.util.HashMap;
import java.util.Map;

import javax.servlet.Servlet;
import javax.servlet.ServletConfig;
import javax.servlet.ServletException;
import javax.servlet.sip.*;

import org.apache.log4j.Logger;

/**
 *
 * Mobicents SIP servlets interface to our conferencing server 
 *
 */
public class ConferenceServlet extends SipServlet implements SipServletListener, Servlet, TimerListener {

    private final Logger m_log = Logger.getLogger(getClass());
    private HandlerContext m_handler = null;
    private Authenticator m_auth = null;
    private AuthDB m_authDB = null;
    private AuthStore m_authStore = null;
    private ConferenceHandler m_confHandler = null;
    private SipFactory m_sipFactory = null;
    private ConferenceInviter m_inviter = null;

    private boolean bAuth = true;

    /**
     * <i>SipServletListener</i> overridden initializer method
     */
    @Override
    public void servletInitialized(SipServletContextEvent e) {

        m_log.debug("servletInitialized");
        //super.servletInitialized(e);
    }

    /**
     * <i>javax.Servlet</i> overridden initializer method
     */
    @Override
    public void init(ServletConfig c) throws ServletException {

        m_log.debug("init start");
        super.init(c);

        try {

            // 
            // create SipFactory to enable creation of B2BClient object to create INVITE requests
            //
            m_sipFactory =  (SipFactory) this.getServletContext().getAttribute(this.SIP_FACTORY);

            //
            // Load the config file defined in sip.xml 'sip-ngconference.conf'
            //
            String configFilePath = getServletContext().getInitParameter("config");
            m_handler = new HandlerContext(m_sipFactory, configFilePath);
            m_confHandler = new ConferenceHandler(m_handler);

            m_authDB = new AuthDB(m_handler);
            m_authDB.init();
            m_authStore = new AuthStore(m_handler);
            m_auth = new Authenticator(m_authDB, m_authStore, m_handler, m_confHandler);

            if(null != (m_inviter = m_handler.getConferenceInviter())) {
                m_inviter.start(m_confHandler);

            //m_inviter.addInvitation(new ConferenceInviter.Invitation("1234@exampledomain.com", "testuser@exampledomain.com", 45));

            }

        } catch(Exception e) {
            LogUtil.printError(m_log, "init: ", e);
            throw new ServletException(e);
        }

        m_log.debug("init done");
    }

    /**
     * <i>javax.Servlet</i> overridden cleanup method 
     */
    @Override
    public void destroy() {

        m_log.debug("destroy");
        m_handler.getConferenceManager().stop();
        if(null != m_inviter) {
            m_inviter.stop();
        }
        super.destroy();
    }

    /**
     * <i>SipServlet</i> overridden SIP ACK message handler
     * @param req The SIP message
     */
    @Override
    protected void doAck(SipServletRequest req) throws ServletException, IOException {
        m_log.debug("doAck:\n" + req);
        super.doAck(req);
    }

    /**
     * <i>SipServlet</i> overridden SIP BYE message handler
     * @param req The SIP message
     */
    @Override
    protected void doBye(SipServletRequest req) throws ServletException, IOException {
        m_log.debug("doBye:\n" + req);
        //super.doBye(req);

        if(false == m_auth.authenticateRequest(req, false)) {
            return;
        }

        m_confHandler.onBye(req);

        //req.getSession().invalidate();

    }

    /**
     * <i>SipServlet</i> overridden SIP INVITE message handler
     * @param req The SIP message
     */
    @Override
    protected void doInvite(SipServletRequest req) throws ServletException, IOException {
        m_log.debug("doInvite (isInitial: " + req.isInitial() + ", transport: " + req.getTransport() + ", secure: " + req.isSecure()+ "):\n" + req);
        //m_log.debug("session.isValid():" + req.getSession().isValid() +", id:" + req.getSession().getId() + ", callId:" + req.getSession().getCallId());

        ConferenceDefinition conferenceDefinition = null;

        if(false == m_auth.authenticateRequest(req, bAuth)) {
            return;
        }

        m_confHandler.onInvite(req);

    }

    /**
     * <i>SipServlet</i> overridden SIP REGISTER message handler
     * @param req The SIP message
     */
    @Override
    protected void doRegister(SipServletRequest req) throws ServletException, IOException {
        m_log.debug("doRegister (isInitial: " + req.isInitial() + ", transport: " + req.getTransport() + ", secure: " + req.isSecure()+ ", expires: " + req.getExpires() + "):\n" + req);
        //m_log.debug("session.isValid():" + req.getSession().isValid() +", id:" + req.getSession().getId() + ", callId:" + req.getSession().getCallId());
        SipServletResponse resp = null;

        if(false == m_auth.authenticateRequest(req, bAuth)) {
            return;
        }

        if(m_handler.isHandleRegister()) {

            m_confHandler.onRegister(req);

        } else {
            m_log.warn("doRegister REGISTER is not allowed in configuration when processing request from:" + 
                        req.getFrom() + ", to: " + req.getTo());
            resp = req.createResponse(SipServletResponse.SC_METHOD_NOT_ALLOWED);
            resp.send();
        }

/*
        //TimerService test code here
        SipApplicationSession appSes =req.getApplicationSession(true);
        TimerService ts = (TimerService) getServletContext().getAttribute(TIMER_SERVICE);
        ServletTimer timer = ts.createTimer(appSes, 5000, false, new SessionIdWrapper(req.getSession().getId()));       
        bTestTimer=true;
*/

    }

    /**
     * <i>TimerListener</i> overridden timeout handler
     * @param servletTimer The <i>ServletTimer</i> instance used in the notification
     */
    public void timeout(ServletTimer servletTimer) {
        //TimerService test code here
        //SipSession sipSession = servletTimer.getApplicationSession().getSipSession(((SessionIdWrapper)servletTimer.getInfo()).getId());
        //m_log.debug("TIMEOUT ");
/*
        if(sipSession != null && !State.TERMINATED.equals(sipSession.getState())) {
            try {
        m_log.debug("TIMEOUT sending BYE");
                sipSession.createRequest("BYE").send();
            } catch (IOException e) {
                m_log.error("An unexpected exception occured while sending the BYE", e);
            }
        }
*/
    }

    /**
     * <i>SipServlet</i> overridden SIP message handler
     * <p>Not implemented</p>
     * @param req The SIP message
     */
    @Override
    protected void doMessage(SipServletRequest req) throws ServletException, IOException {
        m_log.debug("doMessage:\n" + req);
        super.doMessage(req);
    }

    /**
     * <i>SipServlet</i> overridden SIP SUBSCRIBE message handler
     * <p>Not implemented</p>
     * @param req The SIP message
     */
    @Override
    protected void doSubscribe(SipServletRequest req) throws ServletException, IOException {
        m_log.debug("doSubscribe:\n" + req);
        super.doSubscribe(req);
    }

    /**
     * <p><i>SipServlet</i> overridden SIP NOTIFY message handler</p>
     * <p>Not implemented</p>
     * @param req The SIP message
     */
    @Override
    protected void doNotify(SipServletRequest req) throws ServletException, IOException {
        m_log.debug("doNotify:\n" + req);
        super.doNotify(req);
    }

    /**
     * <p><i>SipServlet</i> overridden SIP PUBLISH message handler</p>
     * <p>Not implemented</p>
     * @param req The SIP message
     */
    @Override
    protected void doPublish(SipServletRequest req) throws ServletException, IOException {
        m_log.debug("doPublish:\n" + req);
        super.doPublish(req);
    }

    /**
     * <p><i>SipServlet</i> overridden SIP REFER message handler</p>
     * <p>Not implemented</p>
     * @param req The SIP message
     */
    @Override
    protected void doRefer(SipServletRequest req) throws ServletException, IOException {
        m_log.debug("doRefer:\n" + req);
        super.doRefer(req);
    }

    /**
     * <p><i>SipServlet</i> overridden SIP CANCEL message handler</p>
     * @param req The SIP message
     */
    @Override
    protected void doCancel(SipServletRequest req) throws ServletException, IOException {
        m_log.debug("doCancel:\n" + req);
        super.doCancel(req);
    }

    /**
     * <p><i>SipServlet</i> overridden SIP PRACK message handler</p>
     * @param req The SIP message
     */
    @Override
    protected void doPrack(SipServletRequest req) throws ServletException, IOException {
        m_log.debug("doPrack:\n" + req);
        super.doPrack(req);
    }

    /**
     * <p><i>SipServlet</i> overridden SIP UPDATE message handler</p>
     * @param req The SIP message
     */
    @Override
    protected void doUpdate(SipServletRequest req) throws ServletException, IOException {
        m_log.debug("doUpdate:\n" + req);
        super.doUpdate(req);
    }

    /**
     * <p><i>SipServlet</i> overridden SIP INFO message handler</p>
     * @param req The SIP message
     */
    @Override
    protected void doInfo(SipServletRequest req) throws ServletException, IOException {
        m_log.debug("doInfo:\n" + req);
        //super.doInfo(req);

        if(false == m_auth.authenticateRequest(req, false)) {
            return;
        }

        req.createResponse(SipServletResponse.SC_OK).send();
    }

    /**
     * <p><i>SipServlet</i> overridden SIP OPTIONS message handler</p>
     * @param req The SIP message
     */
    @Override
    protected void doOptions(SipServletRequest req) throws ServletException, IOException {
        m_log.debug("doOptions:\n" + req);
        //super.doOptions(req);

        if(false == m_auth.authenticateRequest(req, false)) {
            return;
        }

        req.createResponse(SipServletResponse.SC_OK).send();
    }

    /**
     * <p><i>SipServlet</i> overridden SIP 2xx succesful response message handler</p>
     * @param resp The SIP message
     */
    @Override
    protected void doSuccessResponse(SipServletResponse resp) throws ServletException, IOException {
        m_log.debug("doSuccessResponse:\n" + resp);

        m_confHandler.onSuccessResponse(resp);
        //super.doSuccessResponse(resp);
    }

    /**
     * <p><i>SipServlet</i> overridden SIP 4xx error response message handler</p>
     * @param resp The SIP message
     */
    @Override
    protected void doErrorResponse(SipServletResponse resp) throws ServletException, IOException {
        m_log.debug("doErrorResponse:\n" + resp);

        //TODO: invalidate call obj of any session being setup...

        m_confHandler.onErrorResponse(resp);

        super.doErrorResponse(resp);
    }

    /**
     * <p><i>SipServlet</i> overridden SIP 18x provisional response message handler</p>
     * @param resp The SIP message
     */
    @Override
    protected void doProvisionalResponse(SipServletResponse resp) throws ServletException, IOException {
        m_log.debug("doProvisionalResponse:\n" + resp);
        super.doProvisionalResponse(resp);
    }

    /**
     * <p><i>SipServlet</i> overridden SIP 18x redirect message handler</p>
     * @param resp The SIP message
     */
    @Override
    protected void doRedirectResponse(SipServletResponse resp) throws ServletException, IOException {
        m_log.debug("doRedirectResponse:\n" + resp);
        super.doRedirectResponse(resp);
    }



};
