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

/**
 *
 * Class representing an audio/video call between two participants
 *
 */
public class Call {

    private User m_from;
    private User m_to;
    private String m_sipId;
    private SDP m_sdpRemote = null;
    private SDP m_sdpLocal = null;   
    private int m_portV = 0;
    private int m_portA = 0;
    private ConferenceDefinition m_conferenceDefinition;
    private State m_state;
    private PortManager m_portManager;
    private SDPResponder.SDP_RESPONSE_TYPE m_sdpResponseType;

    /**
     *
     * The current status of the call
     *
     */
    public enum State {

        /**
         * Any invalid state
         */
        UNKNOWN,

        /** 
         * Initial call state upon receiving first INVITE
         */ 
        INITIAL,

        /**
         * An outgoing INVITE request has been sent
         */
        REQ_INVITE_SENT, 

        /**
         * The response to an outgoing INVITE is pending and we are starting a media listener process
         */
        REQ_MEDIA_LISTENER_STARTING,

        /**
         * The response to an outgoing INVITE is pending and we have succesfully started a media listener process
         */
        REQ_MEDIA_LISTENER_STARTED,
        //REQ_180OK, 

        /**
         * We received a 200 OK response to an outoing INVITE
         */
        REQ_200OK, 

        /** 
         * The INVITE SDP response contains multiple media candidates for both parties.
         */ 
        NEGOTIATION,

        /** 
         * The participant is going to be added to the call 
         */ 
        CONNECTING,

        /**
         * The participant has been succesfully added to the call
         */ 
        CONNECTED,

        /**
         * The participant has been placed on HOLD
         */        
        HOLD,

        /** 
         * Any error condition
         */ 
        ERROR,

        /** 
         * We received a RE-INVITE which contains different media SDP parameters.
         * The participant has been temporarily removed from the call.
         */ 
        RENEGOTIATE,

        /**
         * We received a BYE, the participant is going to be removed from the call
         */ 
        DISCONNECTED;
    }

    /**
     * Constructor used to initialize the call
     * @param from The SIP URI of the calling party
     * @param to The SIP URI of the called party
     */
    public Call(User from, User to) {
        m_from = from;
        m_to = to;
        m_state = State.INITIAL;
        m_sdpResponseType = SDPResponder.SDP_RESPONSE_TYPE.SDP_RESPONSE_BEST_MATCH;
    }

    /**
     * Retrieves the calling party participant
     * @return The calling party
     */
    public User getFrom() {
        return m_from;
    }

    /**
     * Retrieves the SDP of remote client.  This is the remote SDP from an incoming INVITE or the remote SDP response to a local INVITE.
     * @return The SDP of the remote participant
     */
    public SDP getRemoteSDP() { 
        return m_sdpRemote;
    }

    /**
     * Sets the SDP of remote party.  This is the remote SDP from an incoming INVITE or the remote SDP response to a local INVITE.
     * @param sdpRemote The SDP of the remote participant
     */
    public void setRemoteSDP(SDP sdpRemote) { 
        m_sdpRemote = sdpRemote;
    }

    /**
     * Retrieves the SDP of the local party.  This is the local response SDP to an incoming INVITE or the local INVITE SDP of an outgoing call. 
     * @return The SDP of the local server.  
     */
    public SDP getLocalSDP() {  
        return m_sdpLocal;
    }

    /**
     * Sets the SDP of the local server.  This is the local response SDP to an incoming INVITE or the local INVITE SDP of an outgoing call. 
     * @param sdpLocal The SDP of the local server.  
     */
    public void setLocalSDP(SDP sdpLocal) { 
        m_sdpLocal = sdpLocal;
    }

    /**
     * Gets an internal SIP session identifier associated with the <i>javax.servlet.sip.SipSession</i> session instance obtained from the remote party
     * @return The internal SIP session identifier
     */
    public String getSIPId() {
        return m_sipId;  
    }

    /**
     * Sets an internal SIP session identifier associated with the <i>javax.servlet.sip.SipSession</i> session instance obtained from the remote party
     * @param sipId The internal SIP session identifier
     */
    public void setSIPId(String sipId) {
        m_sipId = sipId;  
    }

    /**
     * Gets the local call state
     * @return The local call state
     */
    public State getState() {
        return m_state;
    }

    /**
     * Sets the local call state
     * @param state The local call state
     */
    public void setState(State state) {
        m_state = state;
    }

    /**
     * Sets the Port Manager used to allocate tcp/udp ports used in the call
     * @param portManager The Port Manager used to allocate tcp/udp ports used in the call
     */
    public void setPortManager(PortManager portManager) {
        m_portManager = portManager;
    }

    private int allocateServerPort() {
        return m_portManager.allocatePort();
    }

    /**
     * <p>Retrieves the local server video media port for this call.</p>
     * <p>The call will result in the port being allocated and reserved by the port allocator if it has not yet been allocated</p>
     * @return The local server video media port.
     */
    public int getOrAllocateServerVideoPort() {
        if(0 == m_portV) {
            m_portV = allocateServerPort();
        }
        return m_portV;
    }

    public void setServerVideoPort(int port) {
        m_portV = port;
    }

    /**
     * <p>Retrieves the local server audio media port for this call.</p>
     * <p>The call will result in the port being allocated and reserved by the port allocator if it has not yet been allocated</p>
     * @return The local server audio media port.
     */
    public int getOrAllocateServerAudioPort() {
        if(0 == m_portA) {
            m_portA = allocateServerPort();
        }
        return m_portA;
    }

    /**
     * Sets the local server audio media port for this call
     * @param port The local server audio media port
     */
    public void setServerAudioPort(int port) {
        m_portA = port;
    }

    /**
     * Gets the local conference definition associated with this call
     * @return The local conference definition associated with this call
     */
    public ConferenceDefinition getConferenceDefinition() {
        return m_conferenceDefinition;
    }

    /**
     * Sets the local conference definition which will be associated with this call
     * @param conferenceDefinition The local conference definition 
     */
    public void setConferenceDefinition(ConferenceDefinition conferenceDefinition) {
        m_conferenceDefinition = conferenceDefinition;
    }

    /**
     * Gets the SDP responder type used by this call
     * @return The SDP responder type used by this call
     */
    public SDPResponder.SDP_RESPONSE_TYPE getSDPResponseType() {
        return m_sdpResponseType;
    }

    /**
     * Sets the SDP responder type which will be used by this call
     * @param sdpResponseType The SDP responder type 
     */
    public void setSDPResponseType(SDPResponder.SDP_RESPONSE_TYPE sdpResponseType) {
        m_sdpResponseType = sdpResponseType;
    }

    public String toString() {
        return "Call from: " + m_from + ", to: " + m_to + ", " + m_state + ", video port: " + 
                m_portV + ", audio port: " + m_portA;
    }


}
