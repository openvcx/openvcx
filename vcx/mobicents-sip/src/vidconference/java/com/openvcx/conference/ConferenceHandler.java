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
import java.lang.SecurityException;
import java.net.InetAddress;
import java.net.ConnectException;
import java.net.UnknownHostException;
import java.util.ArrayList;
import java.util.Iterator;

import javax.servlet.ServletException;
import javax.servlet.sip.*;

import org.apache.log4j.Logger;

/**
 *
 * SIP message handler used to handle SIP messages pertaining to an audio/video conference
 *
 */
public class ConferenceHandler implements IHandler {

    private final Logger m_log = Logger.getLogger(getClass());
    private HandlerContext m_handler = null;

    /**
     * An atribute key used to associate a <i>Call</i> object with a <i>SipServletRequest</i> message
     */
    public static final String ATTR_CALL                  = "call";

    /**
     * An attribute key used to associate the conference name with a SIP session
     */
    public static final String ATTR_CONFERENCE_NAME       = "conf_name";

    /**
     * An attribute key used to associate a <i>Call</i> object with a SIP session
     */
    private static final String ATTR_INVITATION            = "invitation";
   
    /**
     * Constructor used to initialize this instance
     */
    public ConferenceHandler(HandlerContext handler) {
        m_handler = handler;
    }

    /**
     * SIP BYE message handler
     * @param req The SIP request message
     */
    @Override
    public void onBye(SipServletRequest req) throws ServletException, IOException {

        User from = new User(req.getFrom());
        User to = new User(req.getTo());
        SipSession session = req.getSession();
        Call call = (Call) session.getAttribute(ATTR_CALL);

        if(null != call) {
            call.setState(Call.State.DISCONNECTED);
        }
        m_log.debug("onBye: from:" + from + ", to:" + to + ", call:" + call);

        try {

            ConferenceDefinition conferenceDefinition = loadConferenceDefinition(req);
            m_handler.getConferenceManager().removeFromConference(from.getName(), conferenceDefinition.getConferenceName());

        } catch(Exception e) {
            LogUtil.printError(m_log, "doBye: ", e);
            //req.createResponse(SipServletResponse.SC_SERVER_INTERNAL_ERROR).send();
            //return;
        } finally {
            session.removeAttribute(ATTR_CALL);
        }

        req.createResponse(SipServletResponse.SC_OK).send();
        m_log.info("doBye sent OK response for BYE from: " + from + ", to: " + to);

    }

    private Call.State checkCallHold(SDP sdpReqNew) throws IOException {
        SDP.SDPType videoType = SDP.SDPType.SDP_TYPE_NONE;
        SDP.SDPType audioType = SDP.SDPType.SDP_TYPE_NONE;

        videoType = sdpReqNew.getVideoSendRecvType();
        audioType = sdpReqNew.getAudioSendRecvType();

        if(SDP.SDPType.SDP_TYPE_SENDONLY == videoType || SDP.SDPType.SDP_TYPE_SENDONLY == audioType) {
            return Call.State.HOLD;
        }
        if((SDP.SDPType.SDP_TYPE_NONE != videoType && SDP.SDPType.SDP_TYPE_SENDONLY != videoType) ||
           (SDP.SDPType.SDP_TYPE_NONE != audioType && SDP.SDPType.SDP_TYPE_SENDONLY != audioType)) {
            return Call.State.CONNECTED;
        }

        return Call.State.UNKNOWN;
    }

    private boolean hasMediaChanged(SDP.Media mediaOld, SDP.Media mediaNew, boolean bCheckAddress) {
        int index;

        if(null == mediaOld && null == mediaNew) {
            return false;
        } else if((null != mediaOld && null == mediaNew) ||
                  (null == mediaOld && null != mediaNew)) {
            return true;
        } 

        if(bCheckAddress && null != mediaOld.getTarget() && mediaOld.getTarget().equals(mediaNew.getTarget(), true)) {
            //m_log.debug("MEDIA HAS CHANGED, old.target:" + mediaOld.getTarget() + ", new target:" + mediaNew.getTarget()); 
            return true;
        }

        return false;
    }

    private boolean hasSDPChanged(Call call, SDP sdpReqNew) throws Exception {
        SDP sdpRespOld = null;
        SDP sdpReqOld = null;
        boolean bHasNewSDP = false;

        if(null == (sdpRespOld = call.getLocalSDP())) {
            return false;
        } else if(null == (sdpReqOld = call.getRemoteSDP())) {
            return false;
        }

        SDP sdpRespNew = m_handler.getConferenceSdpResponder().getResponseSDP(call, sdpReqNew, 
                               null != sdpRespOld.getVideoMedia() ? sdpRespOld.getVideoMedia().getConnect().getPort() : 0, 
                               null != sdpRespOld.getAudioMedia() ? sdpRespOld.getAudioMedia().getConnect().getPort() : 0);
        SDP.MediaCandidate candidate;
        Codec codecOld, codecNew;

        codecOld = codecNew = null; 
        if(null != (candidate = sdpRespOld.getAudioCandidate())) {
            codecOld = candidate.getCodec();
        }

        if(null != (candidate = sdpRespNew.getAudioCandidate())) {
            codecNew = candidate.getCodec();
        }

        if((null != codecOld && null == codecNew) ||
           (null == codecOld && null != codecNew) ||
           (null != codecOld && null != codecNew && false == codecOld.equals(codecNew, true)) ||
            hasMediaChanged(sdpReqOld.getAudioMedia(), sdpReqNew.getAudioMedia(), false)) {
            return true;
        }

        codecOld = codecNew = null; 
        if(null != (candidate = sdpRespOld.getVideoCandidate())) {
            codecOld = candidate.getCodec();
        }

        if(null != (candidate = sdpRespNew.getVideoCandidate())) {
            codecNew = candidate.getCodec();
        }

        if((null != codecOld && null == codecNew) ||
           (null == codecOld && null != codecNew) ||
           (null != codecOld && null != codecNew && false == codecOld.equals(codecNew, true)) ||
            hasMediaChanged(sdpReqOld.getVideoMedia(), sdpReqNew.getVideoMedia(), false)) {
            return true;
        }

        //TODO: check SRTP re-keying
        //TODO: check ICE ufrag, pwd update for STUN
        //TODO: check if client includes a=remote-candidates response to receiving multiple ICE a=candidates

        return false;
    }

    private boolean hasSDPTargetAddressChanged(Call call, SDP sdpReqNew) throws Exception {
        SDP sdpReqOld = null;
        boolean bHasNewSDP = false;

        if(null == call.getLocalSDP()) {
            return false;
        } else if(null == (sdpReqOld = call.getRemoteSDP())) {
            return false;
        }

        if(hasMediaChanged(sdpReqOld.getAudioMedia(), sdpReqNew.getAudioMedia(), true)) {
            return true;
        }

        if(hasMediaChanged(sdpReqOld.getVideoMedia(), sdpReqNew.getVideoMedia(), true)) {
            return true;
        }

        return false;
    }

    /**
     * <p>Loads and initializes a conference definition instance from a conference definition file</p>
     * <p>If the conference definition has been previously loaded and stored with the internally associated <i>Call</i> object then no new instance will be loaded
     * @param req The SIP request message
     * @return The initialized conference definition instance
     */
    public ConferenceDefinition loadConferenceDefinition(SipServletRequest req) 
                                    throws FileNotFoundException, IOException, SecurityException {
        SipSession session = req.getSession();
        Call call = (Call) session.getAttribute(ATTR_CALL);
        ConferenceDefinition conferenceDefinition = null;

        if(null == call || null == (conferenceDefinition = call.getConferenceDefinition())) {

            conferenceDefinition = m_handler.getConferenceDefinitionLoader().load(User.toString(req.getFrom()), 
                                                                                  User.toString(req.getTo()));
        }

        return conferenceDefinition;
    }

    private ConferenceDefinition loadConferenceDefinition(Call call, SipServletRequest req, User from, User to) 
            throws IOException {

        ConferenceDefinition conferenceDefinition = null;

        //
        // Validate conference configuration and destination address
        //
        try {

            if(null == call || null == (conferenceDefinition = call.getConferenceDefinition())) {
                conferenceDefinition = m_handler.getConferenceDefinitionLoader().load(from.getName(), to.getName()); 
                if(null != call) {
                    call.setConferenceDefinition(conferenceDefinition);
                }
                m_log.debug("Got Conference Definition: " + conferenceDefinition.toString());
            }

        } catch(FileNotFoundException e) {
            if(null != call) {
                call.setState(Call.State.ERROR);
            }
            m_log.error("Called party conference configuration not found: " + to.getName() + ", " + e);
            if(null != req) {
                req.createResponse(SipServletResponse.SC_NOT_FOUND, "CONFERENCE NOT AVAILABLE").send();
            }
            return null;
        } catch(SecurityException e) {
            if(null != call) {
                call.setState(Call.State.ERROR);
            }
            m_log.error("Called party " + from.getName() + " not permitted in conference " + to.getName() + ", " + e);
            if(null != req) {
                req.createResponse(SipServletResponse.SC_FORBIDDEN, "USER NOT PERMITTED IN CONFERENCE").send();
            }
            return null;
        } catch(Exception e) {
            if(null != call) {
                call.setState(Call.State.ERROR);
            }
            LogUtil.printError(m_log, "doInvite: ", e);
            if(null != req) {
                req.createResponse(SipServletResponse.SC_SERVER_INTERNAL_ERROR, "INVALID CALLEE CONFIGURATION").send();
            }
            return null;
        }

        return conferenceDefinition;
    }

    private boolean sdpHasMultipleMedia(SDP sdp) {

        if(sdp.getVideoCandidatesCount() > 1) {
            int index;
            int countMedia = 0;
            SDP.MediaCandidate [] candidates = sdp.getAudioCandidates();
            for(index = 0; index < candidates.length; index++) {
  
                //
                // Ignore FEC payloads
                //
                if(false == Codec.FEC_RED.equalsIgnoreCase(candidates[index].getCodec().getName()) ||
                   false == Codec.FEC_ULPFEC.equalsIgnoreCase(candidates[index].getCodec().getName())) {
                    countMedia++;
                }
            }

            if(countMedia > 1) {
                return true;
            }

        } else if(sdp.getAudioCandidatesCount() > 1) {
            int index;
            int countMedia = 0;
            SDP.MediaCandidate [] candidates = sdp.getAudioCandidates();
            for(index = 0; index < candidates.length; index++) {

                //
                // Ignore DTMF payloads 
                //
                if(false == Codec.TELEPHONE_EVENT.equalsIgnoreCase(candidates[index].getCodec().getName())) {
                    countMedia++;
                }
            }

            if(countMedia > 1) {
                return true;
            }
            
        }
        return false;
    }

    private SDP getResponseSDP(Call call, SipServletRequest req, SDP sdpReq, User from, User to, 
                               ConferenceDefinition.Name conferenceName) 
                    throws IOException {
        SDP sdpResp = null;
        boolean bAddToConference = true;

        try {
 
            //
            // Create a response SDP
            //
            sdpResp = m_handler.getConferenceSdpResponder().getResponseSDP(call, sdpReq, 0, 0);

            if(sdpHasMultipleMedia(sdpResp)) {
                call.setState(Call.State.NEGOTIATION);
                bAddToConference = false;
                m_log.debug("doInivte will not add participant to media session");
            }

            m_log.debug("doInvite SDP response contains " + 
                   (sdpReq.getVideoCandidatesCount() > 0 ? 
                    (sdpResp.getVideoCandidatesCount() + " matching video candidate(s) and ") : "") +
                    sdpResp.getAudioCandidatesCount() + " matching audio candidate(s). Will " +
                    (bAddToConference ? "" : " not ") + "add: " + from.getName() + " to conference: " + 
                    conferenceName);

            m_log.info("doInvite created response SDP for INVITE from:" + from.getName() + ", to: " + to.getName() +
                    " with " + (sdpReq.getVideoCandidatesCount() > 0 ?
                     (sdpResp.getVideoCandidatesCount() + " video candidate(s) and ") : "") +
                     sdpResp.getAudioCandidatesCount() + " matching audio candidate(s).");

            //m_log.debug(sdpResp.toString());

        } catch(SecurityException e) {
            call.setState(Call.State.ERROR);
            req.createResponse(SipServletResponse.SC_FORBIDDEN, "SDP DOES NOT CONFORM TO CONFERENCE SECURITY ATTRIBUTES").send();
        } catch(Exception e) {
            LogUtil.printError(m_log, "doInvite: ", e);
            req.createResponse(SipServletResponse.SC_NOT_ACCEPTABLE_HERE, "SDP MISMATCH").send();
            return null;
        }

        return sdpResp;
    }

    private void sendInviteOK(Call call, SipServletRequest req, SDP sdpResp, User from, User to) 
                     throws IOException {

        try {

            //
            // Create and send a 200 OK response with SDP
            //
            String sdpText = null;
            SipServletResponse resp = req.createResponse(SipServletResponse.SC_OK);
            if(null != sdpResp) {
                sdpText = sdpResp.serialize();
                resp.setContent(sdpText.getBytes(), "application/sdp");
            }
            resp.send();
            m_log.info("doInvite sent OK response with SDP for INVITE from:" + from + ", to: " + to + " " +
                        (null != sdpText ? " with SDP" : "without SDP"));

        } catch(IOException e) {
            LogUtil.printError(m_log, "doInvite: ", e);
            req.createResponse(SipServletResponse.SC_SERVER_INTERNAL_ERROR).send();
            return;
        } catch(Exception e) {
            LogUtil.printError(m_log, "doInvite: ", e);
            IOException io = new IOException(e);
            io.setStackTrace(e.getStackTrace());
            throw io;
        }

    }

    private boolean checkHold(Call call, SipServletRequest req, SDP sdpReq, User from, User to, 
                                  ConferenceDefinition.Name conferenceName) throws IOException {

        if(Call.State.CONNECTED != call.getState() && Call.State.HOLD != call.getState()) {
            return false;
        }

        try {

            Call.State stateRequested = checkCallHold(sdpReq);
            if(Call.State.HOLD == stateRequested && Call.State.CONNECTED == call.getState()) {

                m_log.info("doInvite going on HOLD from:" + from + ", to: " + to + ", call state: "
                            + call.getState());

                m_handler.getConferenceManager().conferenceHold(from.getName(), conferenceName, true);
                call.setState(Call.State.HOLD);

                SDP sdpResp = call.getLocalSDP().clone();
                sdpResp.setAudioSendRecvType(SDP.SDPType.SDP_TYPE_RECVONLY);
                sdpResp.setVideoSendRecvType(SDP.SDPType.SDP_TYPE_RECVONLY);

                m_log.debug("doInvite going on HOLD SDP response: \r\n" + sdpResp.toString());
                sendInviteOK(call, req, sdpResp, from, to);
                return true;

            } else if(Call.State.CONNECTED == stateRequested && Call.State.HOLD == call.getState()) {
                m_log.info("doInvite resuming from HOLD from:" + from + ", to: " + to + ", call state: "
                            + call.getState());

                m_handler.getConferenceManager().conferenceHold(from.getName(), conferenceName, false);
                call.setState(Call.State.CONNECTED);

                SDP sdpResp = call.getLocalSDP();

                m_log.debug("doInvite resuming from  HOLD SDP response: \r\n" + sdpResp.toString());
                sendInviteOK(call, req, sdpResp, from, to);
                return true;

            }

        } catch(Exception e) {
                    call.setState(Call.State.ERROR);
                    LogUtil.printError(m_log, "doInvite: ", e);
                    req.createResponse(SipServletResponse.SC_SERVER_INTERNAL_ERROR).send();
                    return true;
        }

        return false;
    }

    private boolean addToConference(Call call, 
                                    SipServletRequest req, 
                                    SipSessionContext sipSessionContext,
                                    SDP sdpRemote, 
                                    SDP sdpLocal, 
                                    ConferenceDefinition conferenceDefinition, 
                                    User from,
                                    TURNRelay turnRelay)
                        throws IOException { 

        Call.State stateStarting = Call.State.CONNECTING;
        Call.State stateStarted = Call.State.CONNECTED;

        //
        // If sdpRemote may be null when it is not yet received from the remote end, such as when sending an INVITE 
        // as a UA client and prior to receiving the remote SDP in a 200 OK response.
        //
        if(null == sdpRemote) {
            stateStarting = Call.State.REQ_MEDIA_LISTENER_STARTING;
            stateStarted = Call.State.REQ_MEDIA_LISTENER_STARTED;
        }
        
        //if(0 == sdpLocal.getVideoCandidatesCount() && sdpRemote.getVideoCandidatesCount() > 0) {
        //    m_log.warn("Unable to support video for participant invite from: " + from + " to: " + to);
        //} else if(0 == sdpLocal.getAudioCandidatesCount() && sdpRemote.getAudioCandidatesCount() > 0) {
        //    m_log.warn("Unable to support audio for participant invite from: " + from + " to: " + to);
        //}

        //if(Call.State.RENEGOTIATE != call.getState() && null != req) {
        //    req.createResponse(SipServletResponse.SC_RINGING).send();
        //    m_log.info ("doInvite sent RINGING for INVITE from: " + from + ", to: " + to);
        //}

        try {

            //
            // Add the user to the conference, possibly creating the new conference instance
            //
            call.setRemoteSDP(sdpRemote);
            call.setState(stateStarting);
            m_handler.getConferenceManager().addToConference(sdpLocal, sdpRemote, from, conferenceDefinition, sipSessionContext, turnRelay);

            //
            // Check if we were asked to setup a TURN relay for any incoming data channel(s)
            //
            if(null != turnRelay) {

                if((null != turnRelay.getAddress() && turnRelay.getAddress().isValid())) {
                    updateSDPTURNRelay(sdpLocal, conferenceDefinition, turnRelay);
                } else {
                    //
                    // Fail if were not able to obtain the TURN data relay address
                    //
                    call.setState(Call.State.ERROR);
                    if(null != req) {
                        req.createResponse(SipServletResponse.SC_SERVER_INTERNAL_ERROR).send();
                    }
                    return false;
                }
            }

            call.setLocalSDP(sdpLocal);
            call.setState(stateStarted);
            return true;

        } catch(SecurityException e) {
            call.setState(Call.State.ERROR);
            if(null != req) {
                req.createResponse(SipServletResponse.SC_FORBIDDEN).send();
            }
            return false;
        } catch(Exception e) {
            call.setState(Call.State.ERROR);
            LogUtil.printError(m_log, "doInvite: ", e);
            if(null != req) {
                req.createResponse(SipServletResponse.SC_SERVER_INTERNAL_ERROR).send();
            }
            return false;
        }

    }

    public static boolean isSDPMediaHaveTURNRelay(SDP sdp) {
        SDP.Media media = null;
        if(!(null == (media = sdp.getAudioMedia()) || null == (media = sdp.getVideoMedia()))) { 
            Iterator<SDP.ICECandidate> iter = media.getICECandidatesIterator();
            while(iter.hasNext()) {
                SDP.ICECandidate candidate = iter.next(); 
                if(SDP.ICECandidate.TYPE.relay == candidate.getType()) {
                    return true;
                }
            }
        }
        return false;
    }

    private SDP.ICEAddress getTURNRelayAddressString(SDP sdp) {
        SDP.Media media = null;
        if(!(null == (media = sdp.getAudioMedia()) || null == (media = sdp.getVideoMedia()))) {
            Iterator<SDP.ICECandidate> iter = media.getICECandidatesIterator();
            while(iter.hasNext()) {
                SDP.ICECandidate candidate = iter.next();
                if(SDP.ICECandidate.TYPE.relay == candidate.getType()) {
                    SDP.ICEAddress relayAddress = null;
                    // Should we be using the remote's internal (relay/raddr) address instead of the TURN server address
                    //relayAddress = candidate.getICERelayAddress();
                    if(null == relayAddress) {
                        relayAddress = candidate.getICEAddress();
                    }
                    return relayAddress;
                }
            }
        }
        return null;
    }

    private void updateSDPTURNRelay(SDP sdp, final ConferenceDefinition conferenceDefinition, final TURNRelay turnRelay) {
        SDP.Media media = null;
        InetAddress turnServerAddress = null;

        try {
            turnServerAddress = new SDP.ICEAddress(conferenceDefinition.getTurnServer()).getAddress();
        } catch(Exception e) {

        }
        
        if(null == turnServerAddress) {
            turnServerAddress = turnRelay.getAddress().getAddress();
        }

        //TODO: need to handle TURN for non-muxed audio+video channels and non rtcp-mux
        if(!(null == (media = sdp.getAudioMedia()))) {
            Iterator<SDP.ICECandidate> iter = media.getICECandidatesIterator();
            while(iter.hasNext()) {
                SDP.ICECandidate candidate = iter.next(); 
                if(SDP.ICECandidate.TYPE.relay == candidate.getType()) {
                    candidate.setAddress(turnServerAddress);
                    candidate.setPort(turnRelay.getAddress().getPort());
                }
            }
            //TODO: the NON-ICE address should be set to the allocated TURN relay per a config option... by default TURn should be listed only as an available TURN ice candidate to enable non-ice clients...
            //try {
            //    sdp.setAudioAddress(turnServerAddress.getHostAddress());
            //} catch(UnknownHostException e) { 
            //}
        }
        if(!(null == (media = sdp.getVideoMedia()))) {
            Iterator<SDP.ICECandidate> iter = media.getICECandidatesIterator();
            while(iter.hasNext()) {
                SDP.ICECandidate candidate = iter.next(); 
                if(SDP.ICECandidate.TYPE.relay == candidate.getType()) {
                    candidate.setAddress(turnServerAddress);
                    candidate.setPort(turnRelay.getAddress().getPort());
                }
            }
            //try {
            //    sdp.setVideoAddress(turnServerAddress.getHostAddress());
            //} catch(UnknownHostException e) { 
            //}
        }
    }


    /**
     * SIP INVITE message handler
     * @param req The SIP request message
     */
    @Override
    public void onInvite(SipServletRequest req) throws ServletException, IOException {

        User from = new User(req.getFrom());
        User to = new User(req.getTo());
        ConferenceDefinition conferenceDefinition = null;
        SipSession session = req.getSession();
        Call call = (Call) session.getAttribute(ATTR_CALL);
        if(null == call) {
            call = new Call(from, to);
            call.setPortManager(m_handler.getPortManagerRtp());
            session.setAttribute(ATTR_CALL, call);
            m_log.debug("doInvite: created new " + call);
        } else {
            m_log.debug("doInvite: existing " + call + ", call.getFrom():" + call.getFrom() + ", user:" + from);
            from = call.getFrom();
        }

        SDP sdpReq = null;
        SDP sdpResp = null;
        //SipServletResponse resp = null;

        // Testing
        call.getFrom().setStunPolicy(User.STUNPolicy.ENABLED_IF_PEER);
        call.setSIPId(session.getId());
 
        try { 

            sdpReq = SDP.create(new String(req.getRawContent()).trim());
            sdpReq.setDeviceType(m_handler.getDeviceTypeMappingTable().lookupDeviceType(req));

        } catch(Exception e) {
            LogUtil.printError(m_log, "doInvite: ", e);
            req.createResponse(SipServletResponse.SC_SERVER_INTERNAL_ERROR, "BAD SDP").send();
            return;
        }

        //
        // Validate conference configuration, permissions and destination address
        //
        if((null == (conferenceDefinition = loadConferenceDefinition(call, req, from, to)))) {
            return;
        }

        //
        // Do not refer to the conference as 'to.getName()' because the conference definition
        // could include an alias
        //
        ConferenceDefinition.Name conferenceName = conferenceDefinition.getConferenceName();

        // 
        // Check to see if the user is currently active in the conference
        //
        try {
            if(true == m_handler.getConferenceManager().isConferenceParticipantActive(from.getName(), conferenceName, true, true) &&
               Call.State.HOLD != call.getState()) {
                call.setState(Call.State.CONNECTED);
            }
        } catch(Exception e) { 
            LogUtil.printError(m_log, "doInvite: ", e);
            call.setState(Call.State.CONNECTED);
        }

        //
        // Check if the INVITE SDP has been changed and if so, re-add the user into the conference
        // according to the new media properties
        //
        try {
            if(hasSDPChanged(call, sdpReq)) {

                m_log.info("doInvite Processing SDP RE-INVITE from:" + from + ", to: " + to + 
                    ", call state: " + call.getState());

                call.setState(Call.State.RENEGOTIATE);
                m_handler.getConferenceManager().removeFromConference(from.getName(), conferenceName);

            }

        } catch(Exception e) {
            call.setState(Call.State.ERROR);
            LogUtil.printError(m_log, "doInvite: ", e);
            req.createResponse(SipServletResponse.SC_SERVER_INTERNAL_ERROR).send();
            return;
        }

        //
        // Check if this is a RE-INVITE going on hold, or resuming from hold 
        //
        if(Call.State.CONNECTED == call.getState() || Call.State.HOLD == call.getState()) {

            if(true == checkHold(call, req, sdpReq, from, to, conferenceName)) {
                return;
            }

        }

        if(Call.State.CONNECTED == call.getState() || Call.State.HOLD == call.getState()) {

            try {

                if(hasSDPTargetAddressChanged(call, sdpReq)) {
                    m_handler.getConferenceManager().conferenceParticipantAddressChange(from.getName(), conferenceName, sdpReq);
                    call.setRemoteSDP(sdpReq);
                    m_log.info("Updated call connect address: " + call + ", target: " + sdpReq.printTargetIp(true));
                } else {
                    m_log.error("Currently already connected: " + call);
                    //req.createResponse(SipServletResponse.SC_BUSY_HERE).send();
                    //return;
                }

            } catch(Exception e) {
                call.setState(Call.State.ERROR);
                LogUtil.printError(m_log, "doInvite: ", e);
                req.createResponse(SipServletResponse.SC_SERVER_INTERNAL_ERROR).send();
                return;
            }

            //
            // Create and send a 200 OK response with SDP
            //
            sendInviteOK(call, req, call.getLocalSDP(), from, to);

            //req.createResponse(SipServletResponse.SC_BUSY_HERE).send();
            return; 
        }

        //
        // Create a response SDP
        //
        if(null == (sdpResp = getResponseSDP(call, req, sdpReq, from, to, conferenceName))) {
            return;
        }


        //
        // Setup a TURN server relay for any incoming data channel(s)
        //
        if(conferenceDefinition.isTURNRelayEnabled() && isSDPMediaHaveTURNRelay(sdpResp)) {

            TURNRelay turnRelay = new TURNRelay();
            turnRelay.setPeerPermission(getTURNRelayAddressString(sdpReq));

            m_log.debug("Conference definition prefers TURN and remote SDP advertises TURN.");
            m_log.debug("TURN server: " + conferenceDefinition.getTurnServer() + ", user: " + conferenceDefinition.getTurnUser() + 
                        " realm: " + conferenceDefinition.getTurnRealm());
            if(false == (addToConference(call, req, 
                                         new SipSessionContext(req.getSession().getId(), req.getApplicationSession(true)),
                                         null, sdpResp, conferenceDefinition, from, turnRelay))) {
                m_log.error("Failed to start media processor TURN relay");
                return;
            }

        }

        m_log.debug(sdpResp.toString());

        //
        // Add the user into the conference 
        //
        if(false == sdpHasMultipleMedia(sdpResp)) {

            if(0 == sdpResp.getVideoCandidatesCount() && sdpReq.getVideoCandidatesCount() > 0) {
                m_log.warn("Unable to support video for participant invite from: " + from + " to: " + to);
            } else if(0 == sdpResp.getAudioCandidatesCount() && sdpReq.getAudioCandidatesCount() > 0) {
                m_log.warn("Unable to support audio for participant invite from: " + from + " to: " + to);
            }

            if(Call.State.RENEGOTIATE != call.getState()) {
                req.createResponse(SipServletResponse.SC_RINGING).send();
                m_log.info ("doInvite sent RINGING for INVITE from: " + from + ", to: " + to);
            }

            if(false == (addToConference(call, req, 
                                         new SipSessionContext(req.getSession().getId(), req.getApplicationSession(true)),
                                         sdpReq, sdpResp, conferenceDefinition, from, null))) {
                return;
            }
        }

        //
        // Create and send a 200 OK response with SDP
        //
        sendInviteOK(call, req, sdpResp, from, to);

        //
        // Send any conference INVITE requests per conferenceDefinition config
        //
        inviteUsers(from, to, conferenceDefinition);

    }

    private void inviteUsers(User from, User to, ConferenceDefinition conferenceDefinition) 
                     throws ServletException, IOException {

        // 
        // If 'inviteCalledParty=true' in the conference definition, invite the called party 'to'
        //
        if(true == conferenceDefinition.isInviteCalledParty() && false == from.getName().equals(to.getName())) {
            m_handler.getConferenceInviter().addInvitation(new ConferenceInviter.Invitation(from, to, 
                                                                   conferenceDefinition));
        }

        // 
        //  Invite any 'inviteSIPUris' to the conference
        //
        String [] toUris = conferenceDefinition.getInviteUsers();

        if(null != toUris) {
            for(int index = 0; index < toUris.length; index++) {
                User calledUser = new User(toUris[index]);

                if(false == from.getName().equals(calledUser.getName())) {
                    m_handler.getConferenceInviter().addInvitation(new ConferenceInviter.Invitation(
                                                         calledUser, conferenceDefinition));
                }
            }
        }

    }

    /** 
     * <p>Invites a participant to join a conference.</p>
     * <p>Calling this method will result in a SIP INVITE request to be sent to the called party
     * @param invitation The invitation
     * @param contactUri An optional contact URI used to construct the contact SIP header of the INVITE request.  The contact header may contain transport specific designation.
     * @param routeUri An optional route URI used to construct a route SIP header.
     */ 
    public void inviteParticipant(ConferenceInviter.Invitation invitation, URIUtil.ContactURI contactUri, 
                           URIUtil.ContactURI routeUri) 
             throws Exception {

        User to = invitation.getTo();
        User conference = invitation.getFrom();
        SDP sdpLocal = null;
        SDP sdpRemoteDummy = null;
        SipServletRequest reqInvite = null;
        ConferenceDefinition conferenceDefinition = invitation.getConferenceDefinition();

        if(null == conferenceDefinition &&
           (null == (conferenceDefinition = loadConferenceDefinition(null, null, to, conference)))) {
            throw new IOException("Failed to load conference definition file: " + to);
        }

        ConferenceDefinition.Name conferenceName = conferenceDefinition.getConferenceName();

        Call call = new Call(conference, to);
        call.setPortManager(m_handler.getPortManagerRtp());
        call.setConferenceDefinition(conferenceDefinition);

        if(invitation.isRemote()) {
            //if(invitation.getDeviceType() != DeviceType.Type.WEBRTC_UNKNOWN) {
            //    invitation.AddNextDeviceType(DeviceType.Type.WEBRTC_UNKNOWN);
            //}
        } else if(null != contactUri) {
            if("WS".equalsIgnoreCase(contactUri.getTransport())) {
                invitation.setDeviceType(new DeviceType(DeviceTypeWebRTC.DEVICE_TYPE_WEBRTC_GENERIC));
            }
        }

        sdpLocal = m_handler.getConferenceSdpResponder().getRequestSDP(call, invitation.getDeviceType(), 0, 0);

        // 
        // Check to see if the user is currently active in the conference
        //
        if(true == m_handler.getConferenceManager().isConferenceParticipantActive(to.getName(), 
                                                                                 conferenceName, true, true)) {
            m_log.warn("User to invite: " + to.getName() + " is already a participant in the conference: " + 
                           conferenceName);
            return;
        }

        ConferenceDefinition.Name existingConferenceName = null;
        if(false == invitation.isRemote() && null != 
           (existingConferenceName = m_handler.getConferenceManager().findParticipantConference(to.getName()))) {
            m_log.warn("User to invite: " + to.getName() + " is currently a participant in another conference: " + 
                       existingConferenceName + " and will not be invited to conference: " + conferenceName);
            return;
        }

        m_log.info("Going to invite: " + to + " to conference: " + conferenceName +
            (null != contactUri ? ", contact: <" + contactUri + ">" : "") +
            (null != routeUri ? ", route: " + routeUri : ""));
        SipApplicationSession applicationSession = m_handler.getSipFactory().createApplicationSession();
        //SipApplicationSession applicationSession = req.getApplicationSession(true);
        call.setLocalSDP(sdpLocal); 

        //
        // Send the INVITE to the user
        //
        reqInvite = m_handler.getB2BClient().sendInvite(applicationSession, call, conference, to, 
                                                        contactUri, routeUri, sdpLocal);
        call.setState(Call.State.REQ_INVITE_SENT);

        to.setStunPolicy(User.STUNPolicy.ENABLED_IF_PEER);
        call.setSIPId(reqInvite.getSession().getId());

        //
        // Store the invitation 
        //
        reqInvite.getSession().setAttribute(ATTR_INVITATION, invitation);

        m_log.debug("inviteParticipant calling addToConference: " + conferenceName + ", remoteUser: " + 
                    to + ", with delaystart");

        //TODO: set call state to other then connecting,connected

        //
        // Add the user into the conference with 'delaystart' in-order to create the listener sockets and respond to STUN
        // bindings only.  This is done by setting the remote SDP to null.
        //
        if(false == (addToConference(call, null,
                                     new SipSessionContext(reqInvite.getSession().getId(), reqInvite.getApplicationSession(true)),
                                      sdpRemoteDummy, sdpLocal, conferenceDefinition, to, null))) {
            throw new IOException("Failed to invite user:" + to + " + to conference: " + conferenceName);
        }


    }

    /**
     * <p>SIP 2xx succesful message handler</p>
     * <p>This handler should used to process 200 OK responses to INVITE requests containing a remote SDP</p>
     * @param resp The SIP response message
     */
    @Override
    public void onSuccessResponse(SipServletResponse resp) throws ServletException, IOException {

        User localUser = null;
        User fromUser = new User(resp.getFrom());
        User remoteUser  = new User(resp.getTo());
        ConferenceDefinition conferenceDefinition = null;
        Call call = null;
        SipSession session = resp.getSession();
        SDP sdpRemote = null;
        SDP sdpLocal = null;
        ConferenceInviter.Invitation invitation = null;

        if(null != session) {
            call = (Call) session.getAttribute(ATTR_CALL);
            invitation = (ConferenceInviter.Invitation) session.getAttribute(ATTR_INVITATION);
        }

        if(null == call) {

            m_log.error("onSuccessResponse: no call object in " + (null == session ? "empty" : "") + " session");
            resp.getSession(true).createRequest("BYE").send();
            return;

        } else if(Call.State.DISCONNECTED == call.getState() ||
                  Call.State.ERROR == call.getState() ||
                  Call.State.HOLD == call.getState()) {

            m_log.debug("onSuccessResponse: call state is: " + call.getState() + ".  Message not handled.");
            return;

        } else if(!(Call.State.REQ_INVITE_SENT == call.getState() ||
                    Call.State.REQ_MEDIA_LISTENER_STARTED == call.getState())) {

            if(Call.State.CONNECTED == call.getState() && resp.getContentLength() <= 0) {
                m_log.debug("onSuccessResponse: call state is: " + call.getState() + ".  Message not handled.");
            } else {
              m_log.error("onSuccessResponse: invalid call state: " + call.getState());
              call.setState(Call.State.ERROR);
              resp.getSession(true).createRequest("BYE").send();
            }
            return;

        } else if(null == (localUser = new User((String) session.getAttribute(ATTR_CONFERENCE_NAME)))) {

            m_log.error("onSuccessResponse: unable to retrieve conference name from session");
            call.setState(Call.State.ERROR);
            resp.getSession(true).createRequest("BYE").send();
            return;

        } else {

            //
            // At this point we should only be processing a 200 OK response to an INVITE
            //

            m_log.debug("onSuccessResponse: " + call + ", remoteUser:" + remoteUser + ", localUser:" + localUser);
            call.setState(Call.State.REQ_200OK);

        }


        // Testing
        remoteUser.setStunPolicy(User.STUNPolicy.ENABLED_IF_PEER);
        call.setSIPId(session.getId());
 
        try { 

            sdpRemote = SDP.create(new String(resp.getRawContent()).trim());
            sdpRemote.setDeviceType(m_handler.getDeviceTypeMappingTable().lookupDeviceType(resp));

        } catch(Exception e) {
            LogUtil.printError(m_log, "onSuccessResponse: ", e);
            call.setState(Call.State.ERROR);
            resp.getSession(true).createRequest("BYE").send();
            return;
        }

        //
        // Validate conference configuration, permissions and destination address
        //
        if((null == (conferenceDefinition = loadConferenceDefinition(call, null, remoteUser, localUser)))) {
            call.setState(Call.State.ERROR);
            resp.getSession(true).createRequest("BYE").send();
            return;
        }

        // Do not refer to the conference as 'to.getName()' because the conference definition
        // could include an alias
        //
        ConferenceDefinition.Name conferenceName = conferenceDefinition.getConferenceName();


        // 
        // Check to see if the user is currently active in the conference
        // set bDelayedStartOk so that if the user has been previously added as a delayedstart user, this will return false
        //
        try {
            if(true == m_handler.getConferenceManager().isConferenceParticipantActive(remoteUser.getName(), 
                                                                                 conferenceName, true, false)) {

                m_log.info("User: " + remoteUser.getName() + " is already a participant in conference: " + conferenceName);
                call.setState(Call.State.CONNECTED);
                return;
            }
        } catch(Exception e) { 
            LogUtil.printError(m_log, "doSuccessResponse: ", e);
            call.setState(Call.State.ERROR);
            resp.getSession(true).createRequest("BYE").send();
            return;
        }
      
        //
        // Update the server-side request SDP to prune away unused media candidates
        // 
        try {
            sdpLocal = m_handler.getConferenceSdpResponder().updateRequestSDP(call.getLocalSDP(), sdpRemote, 
                                                                               conferenceDefinition, false);
            call.setLocalSDP(sdpLocal);
            m_log.debug("Pruned local SDP:" + sdpLocal.toString());

        } catch(Exception e) {

            LogUtil.printError(m_log, "doSuccessResponse: ", e);
            call.setState(Call.State.ERROR);
            resp.getSession(true).createRequest("BYE").send();
            return;
        }


        //
        // Add the user into the conference 
        //
        //if(false == sdpHasMultipleMedia(sdpResp)) {
        //m_log.debug("onSuccessResponse calling addToConference: " + localUser + ", remoteUser: " + remoteUser);

            //if(0 == sdpRemote.getVideoCandidatesCount() && sdpLocal.getVideoCandidatesCount() > 0) {
            //    m_log.warn("Unable to support video for participant invite from: " + fromUser + " to: " + remoteUser);
            //} else if(0 == sdpRemote.getAudioCandidatesCount() && sdpLocal.getAudioCandidatesCount() > 0) {
            //    m_log.warn("Unable to support audio for participant invite from: " + fromUser + " to: " + remoteUser);
            //}

            if(false == (addToConference(call, null, 
                                         new SipSessionContext(resp.getSession().getId(), resp.getApplicationSession(true)),
                                         sdpRemote, sdpLocal, conferenceDefinition, remoteUser, null))) {
                return;
            }

        //}

        //
        // Create and send an ACK to the 200 OK
        //
        resp.createAck().send();
        m_log.info("onSuccessResponse sent ACK response for 200 OK from:" + localUser + ", to: " + remoteUser);

        if(null != session) {
            session.removeAttribute(ATTR_INVITATION);
        }

    }

    /**
     * SIP 4xx error message handler
     * @param resp The SIP response message
     */
    @Override
    public void onErrorResponse(SipServletResponse resp) throws ServletException, IOException {

        User localUser = null;
        User fromUser = new User(resp.getFrom());
        User remoteUser  = new User(resp.getTo());
        Call call = null;
        ConferenceInviter.Invitation invitation = null;
        SipSession session = resp.getSession();
        ConferenceDefinition conferenceDefinition = null;
        int statusCode = resp.getStatus();

        if(null != session) {
            call = (Call) session.getAttribute(ATTR_CALL);
            invitation = (ConferenceInviter.Invitation) session.getAttribute(ATTR_INVITATION);
        }

        if(null == call) {
            m_log.error("onErrorResponse: no call object in " + (null == session ? "empty" : "") + " session");
            return;
        } else if(null == (localUser = new User((String) session.getAttribute(ATTR_CONFERENCE_NAME)))) {
            m_log.error("onErrorResponse: unable to retrieve conference name from session");
            call.setState(Call.State.ERROR);
            return;
        } 

        if(Call.State.ERROR != call.getState()) {

            if(Call.State.REQ_MEDIA_LISTENER_STARTED == call.getState() ||
               Call.State.REQ_MEDIA_LISTENER_STARTING == call.getState()) {
                //TODO: retry as webrtc/non-webrtc
                m_log.info("onErrorResponse abandoning invitation request for call: " + call);
                 call.setState(Call.State.DISCONNECTED);
                 m_log.debug("invitation was:" + invitation);

            } else {
                m_log.debug("onErrorResponse: setting call: " + call + " state to error.");
                call.setState(Call.State.ERROR);
            }
        }

        //
        // Validate conference configuration, permissions and destination address
        //
        if((null == (conferenceDefinition = loadConferenceDefinition(call, null, remoteUser, localUser)))) {
            return;
        }

        try {

            // 
            // Check if the participant exists in the conference db, and if so, remove them
            //
            if(true == m_handler.getConferenceManager().isConferenceParticipantActive(remoteUser.getName(), 
                                                                                   conferenceDefinition.getConferenceName(), false, true)) {
                m_log.debug("onErrorResponse: removing user: " + remoteUser.getName() + ", from conference: " + 
                         conferenceDefinition.getConferenceName());
                m_handler.getConferenceManager().removeFromConference(remoteUser.getName(), conferenceDefinition.getConferenceName());
            }

        } catch(Exception e) {
            LogUtil.printError(m_log, "onErrorResponse: ", e);
            call.setState(Call.State.ERROR);
        } finally {
            session.removeAttribute(ATTR_CALL);
            session.removeAttribute(ATTR_INVITATION);
        }

        if(null != invitation && invitation.setToNextDeviceType()) {
            m_log.debug("Recreating the invitation: " + invitation);
            m_handler.getConferenceInviter().addInvitation(invitation);
        }

    }

    /**
     * SIP REGISTER message handler
     * @param req The SIP request message
     */
    @Override
    public void onRegister(SipServletRequest req) throws ServletException, IOException {

       //m_log.debug("onRegister session.isValid():" + req.getSession().isValid() +", id:" + req.getSession().getId() + ", callId:" + req.getSession().getCallId());

        int expires = req.getExpires();

        //
        // If we have a registered user databse, insert or remove the user
        //
        if(null != m_handler.getRegisterDB()) {
            if(expires > 0) { 
                m_handler.getRegisterDB().insertUser(new User(req.getFrom()).toString(),
                                                 new URIUtil.ContactURI(req.getHeader("Contact")), expires);
            } else {
                m_handler.getRegisterDB().removeUser(new User(req.getFrom()).toString());
            }
        }

        req.createResponse(SipServletResponse.SC_OK).send();

    }
};
