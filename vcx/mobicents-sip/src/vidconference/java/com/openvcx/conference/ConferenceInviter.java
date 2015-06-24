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

import java.util.Map; 
import java.util.HashMap; 
import java.util.List; 
import java.util.ArrayList; 
import java.util.Iterator; 
import java.io.IOException; 

import org.apache.log4j.Logger;

/**
 *
 * An inviter responsible for inviting one or more users to a conference 
 *
 */
public class ConferenceInviter implements Runnable {

    private final Logger m_log = Logger.getLogger(getClass());

    private ConferenceHandler m_conferenceHandler = null;
    private HandlerContext m_handler = null;
    private boolean m_bRunInviter = true;
    private boolean m_bInviteRemoteParties = true;
    private String [] m_arrAllowedDomains = null;
    private String [] m_arrDenyDomains = null;
    private String m_inviteProxy = null;
    private String m_localDomain = null;
    private String m_fromDomain = null;
    private Map<String, ConferenceInviter.Invitation> m_mapInvitations;

    private static final String CONFIG_SIP_LOCAL_DOMAIN             = "sip.local.domain";
    private static final String CONFIG_SIP_INVITE_OUTGOING          = "sip.invite.outgoing";
    private static final String CONFIG_SIP_INVITE_OUTGOING_REMOTE   = "sip.invite.outgoing.remote";
    private static final String CONFIG_SIP_INVITE_DOMAIN_LIST_ALLOW = "sip.invite.domain.list.allow";
    private static final String CONFIG_SIP_INVITE_DOMAIN_LIST_DENY  = "sip.invite.domain.list.deny";
    private static final String CONFIG_SIP_INVITE_PROXY_ROUTE       = "sip.invite.proxy.route";
    private static final String CONFIG_SIP_INVITE_FROM_DOMAIN       = "sip.invite.from.domain";

    /**
     * A conference invitation request
     */
    public static class Invitation {

        /**
         * Conference invitation processing state
         */
        public enum State {
            /**
             * The initial state of the invitation
             */
            NONE,

            /**
             * An INVITE request has been sent and a response is pending
             */
            PENDING,

            /**
             * The user has been succesfully invited
             */
            SUCCESS,

            /**
             * The invitation has encountered an error and should be regarded as invalid
             */
            ERROR,
        }

        private User m_from;
        private User m_to;
        private ConferenceDefinition m_conferenceDefinition;
        private long m_createTime;
        private long m_expireTime;
        private long m_lastInviteTime;
        private long m_nextInviteTime;
        private State m_state;
        private boolean m_bIsRemote;
        DeviceType m_deviceType;
        List<DeviceType> m_listNextDevices;

        /**
         * Constructor used to initialize the invitation
         * @param from The source address which is initiating this invitation request
         * @param to The conference name to which the user is to be invited to.
         */
        public Invitation(String from, String to) {
            set(new User(from), new User(to), null, 0);
        }

        /**
         * Constructor used to initialize the invitation
         * @param from The source address which is initiating this invitation request
         * @param to The conference name to which the user is to be invited to.
         * @param expireSec The validity of the invitation request in seconds
         */
        public Invitation(String from, String to, long expireSec) {
            set(new User(from), new User(to), null, expireSec);
        }

        /**
         * Constructor used to initialize the invitation
         * @param from The source user which is initiating this invitation request
         * @param to The conference address name to which the user is to be invited to.
         */
        public Invitation(User from, User to) {
            set(from, to, null, 0);
        }

        /**
         * Constructor used to initialize the invitation
         * @param from The source user which is initiating this invitation request
         * @param to The conference address name to which the user is to be invited to.
         * @param expireSec The validity of the invitation request in seconds
         */
        public Invitation(User from, User to, long expireSec) {
            set(from, to, null, expireSec);
        }

        /**
         * Constructor used to initialize the invitation
         * @param to The conference address name to which the user is to be invited to.
         * @param conferenceDefinition The conference definition of the conference that the user is being invited to
         */
        public Invitation(User to, ConferenceDefinition conferenceDefinition) {
            set(new User(conferenceDefinition.getConferenceName().getString()), to, conferenceDefinition, 0);
        }

        /**
         * Constructor used to initialize the invitation
         * @param from The source user 
         * @param to The conference address name to which the user is to be invited to.
         * @param conferenceDefinition The conference definition of the conference that the user is being invited to
         */
        public Invitation(User from, User to, ConferenceDefinition conferenceDefinition) {
            set(from, to, conferenceDefinition, 0);
        }

        private void set(User from, User to, ConferenceDefinition conferenceDefinition, long expireSec) {
            m_from = from;
            m_to = to;
            m_conferenceDefinition = conferenceDefinition;
            m_createTime = System.currentTimeMillis();
            if(expireSec > 0) {
                m_expireTime = m_createTime + (expireSec * 1000);
            } else {
                m_expireTime = 0;
            }
            m_lastInviteTime = 0;
            m_nextInviteTime = 0;
            m_state = State.NONE;
            m_bIsRemote = false;
            m_deviceType = new DeviceType();
            m_listNextDevices = new ArrayList<DeviceType>();
        }

        /**
         * Retrieves the timestamp at which the invitation should expire
         * @return The expiration timestamp in miliseconds
         */
        public long getExpireTime() {
            return m_expireTime;
        }

        /**
         * Sets the expiration time of this invitation
         * @param durationSec The time in seconds that this invitation should be valid from the creation time before it expires
         */
        public void setExpires(long durationSec) {
            m_expireTime = m_createTime + (durationSec * 1000);
        }

        /**
         * Retrieves the <i>from</i> user of this invitation 
         * @return The source user which is initiating this invitation request
         */
        public final User getFrom() {
            return m_from;
        }

        /**
         * Sets the <i>from</i> user of this invitation 
         * @param from The source user which is initiating this invitation request
         */
        public void setFrom(User from) {
            m_from = from;
        }

        /**
         * Retrieves the <i>to</i> user of this invitation 
         * @return The conference address name to which the user is to be invited to.
         */
        public final User getTo() {
            return m_to;
        }

        /**
         * Tests if this invitation is for a remote SIP address which is not locally registered
         */
        public boolean isRemote() {
            return m_bIsRemote;
        }

        /**
         * Sets whether this invitation is for a remote user which is not locally registered
         * @param bIsRemote Set to <i>true</i> if the user to be invited is a remote user which is not locally registered
         */
        public void setRemote(boolean bIsRemote) {
            m_bIsRemote = bIsRemote;
        }

        /**
         *  Sets the remote device type that the next invitation request will be intended for
         *  @param type The remote device type that the next invitation request will be intended for
         */
        public void setDeviceType(DeviceType type) {
            m_deviceType = type;
        }

        /**
         *  @return The device type that this invitation is intended for
         *  @return The remote device type that the next invitation request will be intended for
         */
        public DeviceType getDeviceType() {
            return m_deviceType;
        }

        /**
         * <p>Updates the invitation state to use the next stored remote device type for the subsequent invitation request.</p> 
         * <p>If <i>addNextDeviceType</i> has been previously set with multiple types then this method should 
         * be called to try the next possible device type in the subsequent retry of the outgoing INVITE request.</p>
         * @return <i>true</i>If a next device type was set, <i>false</i> otherwise.
         */
        public boolean setToNextDeviceType() {
            while(m_listNextDevices.size() > 0) {
                DeviceType deviceType = (DeviceType) m_listNextDevices.remove(m_listNextDevices.size() - 1);
                if(m_deviceType.getType() != deviceType.getType()) {
                    m_deviceType.setType(deviceType.getType());
                    m_state = State.NONE;
                    return true;
                }
            }
            return false;
        }

        /**
         * <p>An outgoing invitation may be tailored to a specific device type and
         * could fail if the device type is incorrect.</p>
         * <p>To allow for retries, we allow a list of possible device types describing the remote unknown endpoint.</p>
         */
        public void addNextDeviceType(DeviceType deviceType) {
            m_listNextDevices.add(deviceType);
        }

        /**
         * Retrieves the conference definition type associated with this invitation request
         */
        public final ConferenceDefinition getConferenceDefinition() {
            return m_conferenceDefinition;
        }

        /**
         * Returns a human readable description of this invitation instance
         */
        public String toString() {
            String description = "to: " + m_to + ", from: " + m_from + ", " +  (m_bIsRemote ? "remote" : "local");
            if(null != getConferenceDefinition()) {
                description += ", conf def name: " + getConferenceDefinition().getConferenceName();
            }
            description += ", device type: " + m_deviceType;
            if(m_expireTime > 0) {
                long expiration = (m_expireTime - m_createTime) / 1000;
                description += ", expires " + expiration + " sec.";
            } 
            return description; 
        }


    }

    /**
     * Constructorused to initialize this instance
     * @param handler The master context store used by the application
     */
    public ConferenceInviter(HandlerContext handler) {
        m_handler = handler;
        m_mapInvitations = new HashMap<String, ConferenceInviter.Invitation>();

        m_bRunInviter = m_handler.getConfig().getBoolean(CONFIG_SIP_INVITE_OUTGOING, true);
        m_bInviteRemoteParties = m_handler.getConfig().getBoolean(CONFIG_SIP_INVITE_OUTGOING_REMOTE, true);
        m_arrAllowedDomains = ConfigUtil.parseCSV(m_handler.getConfig().getString(
                                                CONFIG_SIP_INVITE_DOMAIN_LIST_ALLOW, null));
        m_arrDenyDomains = ConfigUtil.parseCSV(m_handler.getConfig().getString(
                                                CONFIG_SIP_INVITE_DOMAIN_LIST_DENY, null));
        m_inviteProxy = m_handler.getConfig().getString(CONFIG_SIP_INVITE_PROXY_ROUTE, null);
        m_localDomain = m_handler.getConfig().getString(CONFIG_SIP_LOCAL_DOMAIN, null);
        m_fromDomain = m_handler.getConfig().getString(CONFIG_SIP_INVITE_FROM_DOMAIN, null);

        if(null == m_fromDomain) {
            m_fromDomain = m_localDomain;
        }

        if(false == m_bRunInviter) {
            m_log.info("ConferenceInviter disabled");
        } else {
            m_log.info("ConferenceInviter " + CONFIG_SIP_INVITE_OUTGOING_REMOTE + "=" + m_bInviteRemoteParties + 
                       ", " + CONFIG_SIP_LOCAL_DOMAIN + "=" + m_localDomain + ", " + 
                       CONFIG_SIP_INVITE_FROM_DOMAIN + "=" + m_fromDomain + ", " + 
                       CONFIG_SIP_INVITE_PROXY_ROUTE + "=" + m_inviteProxy);
        }

    }

    private URIUtil.ContactURI getRemoteContactDomain(String sipUri) {

        if(null != m_inviteProxy) {
            return new URIUtil.ContactURI(m_inviteProxy);
        } else {
            return new URIUtil.ContactURI(ConfigUtil.getSIPDomainPart(sipUri));
        }
    }

    private URIUtil.ContactURI  getRemoteAllowedContactDomain(User to) throws IOException {
        URIUtil.ContactURI contactUri = null;

        //
        // Make sure we're allowed to invite remote parties
        //
        if(false == m_bInviteRemoteParties) {
            return null;
        }

        //
        // Make sure we're not sending a remote INVITE request to the local SIP server domain
        //
        if(null == m_localDomain) {
            return null;
        }

        if(true == ConfigUtil.isSipURIMatchFilter("*@" + ConfigUtil.getSIPDomainPart(to.getName()), 
                                                  "*@" + m_localDomain)) {
            //m_log.debug("SIP contact destination domain: " + to.getName() + " is the local domain.");
            return null;
        }

        if(null != m_arrDenyDomains) {
            for(int index = 0; index < m_arrDenyDomains.length; index++) {

                if(true == ConfigUtil.isSipURIMatchFilter("*@" + ConfigUtil.getSIPDomainPart(to.getName()), 
                                                          "*@" + m_arrDenyDomains[index])) {
                    throw new IOException("SIP contact destination domain: " + to.getName() + " is in deny list.");
                }
            }
        }

        if(null != m_arrAllowedDomains) {
            for(int index = 0; index < m_arrAllowedDomains.length; index++) {

                if(true == ConfigUtil.isSipURIMatchFilter("*@" + ConfigUtil.getSIPDomainPart(to.getName()), 
                                                          "*@" + m_arrAllowedDomains[index])) {
                    return getRemoteContactDomain(to.getName());
                }
            }
            return null;
        }

        return getRemoteContactDomain(to.getName());
    }

    private URIUtil.ContactURI  getRemoteAllowedContactUri(User to) throws IOException {
        URIUtil.ContactURI contactUri = null;

        if(null != (contactUri = getRemoteAllowedContactDomain(to))) {
            contactUri.setUsername(ConfigUtil.getSIPUsernamePart(to.getName()));
        }

        return contactUri;
    }

    /**
     * <p>Adds an invitation request to the inviter instance</p>
     * <p>The invitation will be processed asynchronously</p>
     * @param invitation An initialized user invitation instance
     */
    public void addInvitation(Invitation invitation) {

        if(false == m_bRunInviter){
            m_log.warn("ConferenceInviter is not active to handle invitation");
            return;
        }

        if(null != m_fromDomain && null == ConfigUtil.getSIPDomainPart(invitation.getFrom().getName())) {
            invitation.setFrom(new User(invitation.getFrom().getName() + '@' + m_fromDomain));
        }

        synchronized(m_mapInvitations) {
            Invitation existingInvitation = m_mapInvitations.get(invitation.getTo().getName());
            if(null != existingInvitation) {
                m_mapInvitations.remove(invitation.getTo().getName());
            }
            
            m_log.debug("Adding invitation " + invitation.toString());
            m_mapInvitations.put(invitation.getTo().getName(), invitation);
        }
    }

    /**
     * <p>Cancels a previously added invitation instance</p>
     * @param invitation A previously added user invitation instance
     */
    public void CancelInvitation(Invitation invitation) {

        m_log.debug("Cancel invitation " + invitation.toString());

        synchronized(m_mapInvitations) {
            m_mapInvitations.remove(invitation.getTo().getName());
        }
    }

    private enum InviteResult {
        UNKNOWN,
        DESTINATION_UNKNOWN,
        CALL_NOT_ACTIVE,
        EXPIRED,
        OK,
        ERROR
    }

    private InviteResult doOutgoingInvite(Invitation invitation) throws Exception {

        URIUtil.ContactURI contactUri = null;
        URIUtil.ContactURI routeUri = null;

        if(null == m_handler.getB2BClient()) {
            throw new IOException("No B2BClient instance available");
        }

        //
        // Check invitation expiration
        //
        if(invitation.getExpireTime() > 0) {
            long tmNow = System.currentTimeMillis();
            if(invitation.getExpireTime() < tmNow) {
                m_log.warn("Expired invitation " + invitation.toString());
                return InviteResult.EXPIRED;
            }
        }

        m_log.debug("doOutgoingInvite " + invitation.toString());

        if(null != invitation.m_conferenceDefinition) {
            //
            // If the invitation has a conferenceDefinition set, we only want to invite the user if the
            // call/conference is active.
            //
            if(false == m_handler.getConferenceManager().isConferenceActive(
                                          invitation.getConferenceDefinition().getConferenceName())) {
                m_log.debug("doOutgoingInvite conference definition set but conference is not active: " + 
                             invitation.getConferenceDefinition().getConferenceName());
                return InviteResult.CALL_NOT_ACTIVE; 
            }
        }

        //
        // See where the INVITE should be sent to
        //
        if(null == m_handler.getRegisterDB() ||
           null == (contactUri = m_handler.getRegisterDB().getUserContactUri(invitation.getTo()))) {

            if(null == (routeUri = getRemoteAllowedContactUri(invitation.getTo()))) {

                return InviteResult.DESTINATION_UNKNOWN; 
            }

            invitation.setRemote(true);
            m_log.debug("Sending invite to remote contact: " + contactUri + ", route: " + routeUri);

        } else {
            invitation.setRemote(false);
            m_log.debug("Found locally registered invitation user contact: " + contactUri.getUri());
        }

        m_conferenceHandler.inviteParticipant(invitation, contactUri, routeUri); 

        invitation.m_state = Invitation.State.PENDING;
        return InviteResult.OK;
    }

    /**
     * The invitation processor asynchronous run method
     */
    public void run() {
        boolean bHaveInvite = false;

        if(null == m_conferenceHandler) {
            return;
        }

        while(m_bRunInviter) {

            try {
             

                //m_log.debug("ConferenceMonitor checking if conference ended...");

                inviteUsers();

                if(bHaveInvite) {

                } else {
                    try {
                        Thread.sleep(500);
                    } catch(InterruptedException e) {
                        m_bRunInviter = false;
                        m_log.error(e);
                    }
                }
            } catch(Exception e) {
                LogUtil.printError(m_log, null, e);
            }

        }

        m_log.debug("ConferenceInviter exiting");
    }

    /**
     * Stops the invitation processor thread
     */
    public void stop() {
        m_bRunInviter = false;
    }

    /**
     * Starts the invitation processor thread
     * @param conferenceHandler The conference SIP message handler
     */
    public void start(ConferenceHandler conferenceHandler) {
        m_conferenceHandler = conferenceHandler;
        new Thread(this).start();
    }

    private void inviteUsers() {

        synchronized(m_mapInvitations) {

            for(Map.Entry<String, Invitation> entry : m_mapInvitations.entrySet()) {

                Invitation invitation = entry.getValue();
                InviteResult result = InviteResult.UNKNOWN;
                long tmNow = System.currentTimeMillis();
                
                if(invitation.m_nextInviteTime <= tmNow) {

                    try {

                        if(invitation.m_state == Invitation.State.NONE) {
                            result = doOutgoingInvite(invitation);
                        }

                    } catch(Exception e) {
                        LogUtil.printError(m_log, "inviteUsers: ", e);
                        result = InviteResult.ERROR;
                    }

                    if(result == InviteResult.DESTINATION_UNKNOWN) {
                        invitation.m_lastInviteTime = tmNow;
                        invitation.m_nextInviteTime = tmNow + 3000;
                    } else {
                        // InviteResult.CALL_NOT_ACTIVE,
                        // InviteResult.OK,
                        // InviteResult.EXPIRED,
                        // InviteResult.ERROR,
                        // InviteResult.UNKNOWN,
                        m_log.debug("Removing invitation request " + invitation.toString() + ", result: " + result);
                        m_mapInvitations.remove(entry.getKey());
                    }
                }

            }
        
        } 


    }

/*
    public static void main(String [] args) {
        try {
        HandlerContext handler = new HandlerContext("../../../debug-ngconference.conf");
        ConferenceHandler conferenceHandler = new ConferenceHandler(handler);
        ConferenceInviter inviter = new ConferenceInviter(handler);

        inviter.start(conferenceHandler);

        Invitation invitation = new Invitation("from@me.com", "to@exampledomain.com");
        inviter.addInvitation(invitation);
        Thread.sleep(60000);
 
        } catch(Exception e) { System.out.println(e); }

    }
*/

}
