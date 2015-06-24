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

import com.openvcx.conference.media.*;
import com.openvcx.util.*;

import java.io.IOException;
import java.util.HashMap;
import java.util.Map;
import java.util.Iterator;

import javax.servlet.sip.SipSession;
import javax.servlet.sip.SipServletRequest;

import org.apache.log4j.Logger;

/**
 *
 * <p>Class representing an audio/video conference</p>
 * <p>A conference is defined as a media session  with one or more participants</p>
 *
 */
public class Conference {

    private final Logger m_log = Logger.getLogger(getClass());

    private Map<String, ConferenceUser> m_mapUsers = new HashMap<String, ConferenceUser>();
    private ConferenceDefinition.Name m_name;
    private int m_port;
    //private int m_pid;
    private long m_updateTm = 0;  // The last time a user was added / removed
    private long m_createTm = 0; 
    private long m_startTm = 0; 
    private long m_lastSyncTm = 0;
    private InstanceId m_instanceId;
    private String m_recordingFilePath;
    private ConferenceDefinition m_conferenceDefinition;
    private boolean m_bStartInProgress;
    private IMediaProcessor m_mediaProcessor = null;

    /**
     * The conference constructor used to initialize this instance
     * @param mediaProcessor The underlying media processor implementation
     * @param name The unique conference name
     * @param port The base tcp port used for REST communication with the underlying media processor implementation
     * @param conferenceDefinition The conference definition used to define the conference capabilities
     */
    public Conference(IMediaProcessor mediaProcessor, ConferenceDefinition.Name name, int port, 
               ConferenceDefinition conferenceDefinition) {
        m_mediaProcessor = mediaProcessor;
        m_name = name;
        m_port = port;
        m_createTm = System.currentTimeMillis();
        m_lastSyncTm = m_createTm;
        m_recordingFilePath = null;
        m_conferenceDefinition = conferenceDefinition;
        m_bStartInProgress = false;
    }

    /**
     * Retrieves the conference name or SIP URI used to instantiate this conference
     * @return The unique conference name or SIP URI of the conference
     */
    public ConferenceDefinition.Name getName() {
        return m_name;
    }

    /**
     * Retrieves the base port used to instantiate this conference
     * @return The base tcp port used for REST communication with the underlying media processor implementation
     */
    public int getPort() {
        return m_port;
    }

    /**
     * Retrieves the conference definition used to instantiate this conference
     * @return conferenceDefinition The conference definition used to define the conference capabilities
     */
    public final ConferenceDefinition getConferenceDefinition() {
        return m_conferenceDefinition;
    }

    /**
     * Retrieves the unique conference id which was  assigned and recognized by the underlying media implementation
     * @return The unique conference id assigned by the underlying media processor implementation
     */
    public InstanceId getInstanceId() {
        return m_instanceId;
    }

    /**
     * Sets the unique conference id which should be assigned by the underlying media implementation
     * @param instanceId The unique conference id assigned by the underlying media processor implementation
     */
    public void setInstanceId(InstanceId instanceId) {
        m_instanceId = instanceId;
    }

    /**
     * Retrieves the media recording file path which is assigned by the underlying media implementation
     * @return The conference media recording file path
     */
    public String getRecordingFilePath() {
        return m_recordingFilePath;
    }

    /**
     * Sets the media recording file path which may be assigned by the underlying media implementation
     * @param recordingFilePath The conference media recording file path
     */
    public void setRecordingFilePath(String recordingFilePath) {
        m_recordingFilePath = recordingFilePath;
    }

    /**
     * Retrieves the last time at which a conference participant was added, removed, or the conference itself was started.
     * @return The conference last update time
     */
    public long getLastParticipantUpdateTime() {
        return m_updateTm;
    }

    /**
     * Sets the last time at which a conference participant was added, removed, or the conference itself created.
     */
    private void setLastParticipantUpdateTime() {
        m_updateTm = System.currentTimeMillis();
    }

    /**
     * Sets the time at which the conference particpant state and listing was last synced with the state obtained from the
     * underlying media implementation
     * @param lastSyncTm The last time the state was synced
     */
    public void setLastSyncTime(long lastSyncTm) {
        m_lastSyncTm = lastSyncTm;
    }

    /**
     * Retrieves the time at which the conference particpant state and listing was last synced with the state obtained from the
     * underlying media implementation
     * @return The last time the state was synced
     */
    public long getLastSyncTime() {
        return m_lastSyncTm;
    }

    /**
     * Retrieves the time at which this conference instance was created 
     * @return The conference object creation time 
     */
    public long getCreateTime() {
        return m_createTm;
    }

    /**
     * Retrieves the time at which this conference was succesfully started
     * @return The conference start time
     */
    public long getStartTime() {
        return m_startTm;
    }

    /**
     * Sets the time at which this conference was succesfully started
     */
    public void setStartTime() {
        m_startTm = m_updateTm = System.currentTimeMillis();
    }

    /**
     * Checks whether the conference is currently being started by the underlying media implementation
     * @return true if the conference is currently being started by the underlying media implementation
     */
    public boolean isStartInProgress() {
         return m_bStartInProgress;
    }

    /**
     * Sets the state of the conference, such as if it is currently being started by the underlying media implementation
     * @param bStartInProgress Should be set to true if the conference is currently being started by the underlying media implementation.  
     */
    public void setStartInProgress(boolean bStartInProgress) {
        m_bStartInProgress = bStartInProgress;
    }

    /**
     * Retrieves the number of participants in the conference
     * @return the number of participants
     */
    public int getParticipantCount() {
        int size;

        synchronized(m_mapUsers) {
            size = m_mapUsers.size();
        }
 
        return size;
    }

    /**
     * Adds a participant to the conference
     * @param user The participant user instance being added to the conference
     * @param sdpLocal The SDP of the local server.  This is the local response SDP to an incoming remote INVITE or the local INVITE SDP of an outgoing call. 
     * @param sdpRemote The SDP of the remote client.  This is the remote SDP from an incoming INVITE or the remote SDP response to a local INVITE.
     * @param sipSessionContext A sip session context for sending SIP messages when encountering an error or expiration event
     */
    public String addUser(IUser user, SDP sdpLocal, SDP sdpRemote, SipSessionContext sipSessionContext, TURNRelay turnRelay) 
                      throws Exception {
        String id = null;
        ConferenceUser conferenceUser = null;
        int size = 0;
        boolean bUpdateUser = false;

        synchronized(m_mapUsers) {
            size = m_mapUsers.size();

            if(m_mapUsers.containsKey(user.getName())) {
                conferenceUser = m_mapUsers.get(user.getName());
            } 

            if(null != conferenceUser) {

                if(conferenceUser.isDelayedStart()) {
                    bUpdateUser = true;
                } else {
                    String msg = "Unable to do update user: " + conferenceUser.getName() + ", id: " + 
                                 conferenceUser.getId() + ", in conference: " + m_name;
                    m_log.error(msg);
                    throw new IOException(msg);
                }

            } else {

                if(m_conferenceDefinition.getMaxUsers() > 0 && size >= m_conferenceDefinition.getMaxUsers()) {
                    String msg = "Conference: " + m_name + " already has maximum " + size + " participants";
                    m_log.error(msg);
                    throw new SecurityException(msg);
                }

                conferenceUser = new ConferenceUser(user.getName());
                size = m_mapUsers.size();
                m_mapUsers.put(user.getName(), conferenceUser);
                setLastParticipantUpdateTime();
                m_log.debug("Created " + (null == sdpRemote ? "delayedstart " : "") + "user: " + user.getName() + 
                             ", conference: " + m_name + ", participants: " + size );
            }

            if(null != sdpRemote) {
                //
                // Used for sending BYE/INFO asynchronous requests
                //
                conferenceUser.setSipSessionContext(sipSessionContext);
            }

            //
            // If we don't have a remote SDP, then the user is being added with a delayed start property
            // to setup RTP / STUN listener and subsequent delayed start of RTP output
            //
            if(conferenceUser.isDelayedStart() && null != sdpRemote)  {
                conferenceUser.setDelayedStart(false);
            }
        }

        try {

            id = m_mediaProcessor.addConferenceParticipant(this, sdpLocal, sdpRemote, user, 
                                                  (bUpdateUser ? conferenceUser.getId() : null), turnRelay);

            //TODO: setting a sipSessionContext can generate a BYE on an error or expiration...
            // If acting as a UA client, we need to make sure a BYE is only sent if a dialog is established, 
            // such as after  an INVITE has gone out already... in ConferenceHandler, not here
            if(null == sdpRemote) {
                //
                // Used for sending BYE/INFO asynchronous requests
                //
                conferenceUser.setSipSessionContext(sipSessionContext);
            }

        } catch(Exception e) {
            m_log.error("Unable to add user: " + user.getName() + " to conference: " + m_name + " with " + size + 
                         " participants: " + e);
            removeUser(user.getName());
            conferenceUser.setPendingIdRetrieval(false);
            throw e;
        }
        conferenceUser.setId(id);
        conferenceUser.setPendingIdRetrieval(false);

        m_log.info("Added " + (null == sdpRemote ? "delayedstart " : "") + "user: " + user.getName() + 
                    " with id: " + id + " to conference: " + m_name + ", now with " + m_mapUsers.size() + 
                    " participants");

        return id;
    }

    private synchronized void onRemove(ConferenceUser user) throws Exception {

        Exception exceptionRemoveUser = null;
        m_log.debug("onRemove called for : name:" + user.getName() + " id: " + user.getId());

        //
        // If the user is synced (exists in both the media processor instance and the conference user map
        // then tell the media processor instance to remove this user from the conference
        //
        if(false == user.getUnsynced() && null != user.getId()) {
            try {
                m_mediaProcessor.removeConferenceParticipant(this, user.getId());
            } catch(Exception e) {
                exceptionRemoveUser = e;
            }
        }

        if(null != user.getSipSessionContext()) {
            //TODO: should synchronize between here and ConferenceHandler onBye handler
            Call call;
            SipSession session = null;
            if(user.getSipSessionContext().getSipApplicationSession().isValid()) {
                session = user.getSipSessionContext().getSipApplicationSession().getSipSession(
                                                              user.getSipSessionContext().getSessionId());
            } else {
                m_log.debug("onRemove user: " + user.getName() + ", sip application session invalidated");
            }
            //TODO: check if dialog has been previously established... (in the case where UDP INVITE was sent & nothing received)

            if(null != session && null != (call = (Call) session.getAttribute(ConferenceHandler.ATTR_CALL)) &&
               call.getState() != Call.State.DISCONNECTED && call.getState() != Call.State.RENEGOTIATE) {

                call.setState(Call.State.DISCONNECTED);

                if(session.isValid() && SipSession.State.EARLY != session.getState()) {
                    try {
                        m_log.debug("Sending SIP BYE from onRemove for user: " + user.getName());
                        SipServletRequest req = session.createRequest("BYE");
                        m_log.debug("\n" + req);
                        req.send();
                    } catch(Exception e) {
                        LogUtil.printError(m_log, "Failed to send SIP BYE from onRemove for user " + 
                                           user.getName()+ " from conference: " + m_name, e);
                    }
                }
            }
        }

        if(null != exceptionRemoveUser) {
            throw exceptionRemoveUser;
        }

    }

    private ConferenceUser findUser(String userName) {
        synchronized(m_mapUsers) {
            if(m_mapUsers.containsKey(userName)) {
                return m_mapUsers.get(userName);
            }
       }
       return null;
    }

    /**
     * Removes a participant from the conference
     * @param name The user name or SIP URI
     */
    public void removeUser(String name) {
        ConferenceUser user = null;

        m_log.debug("RemoveUser name: " + name);

        synchronized(m_mapUsers) {
            if(null != (user = m_mapUsers.remove(name))) {
                try {
                    m_log.debug("Removing user: " + name + " from conference: " + m_name + " id: " + user.getId());
                    onRemove(user);
                    setLastParticipantUpdateTime();
                    m_log.info("Removed user: " + name + " from conference: " + m_name);
                } catch(Exception e) {
                    LogUtil.printError(m_log, "Failed to remove user: " + name + " from conference: " + m_name, e);
                }
            }
        }
    }

    /**
     * Places a conference participant on hold or removes from the hold state
     * @param name The user name or SIP URI of the participant
     * @param bHold The hold state.  Set to true if going on hold, or false if resuming from hold
     */
    public void holdUser(String name, boolean bHold) throws Exception {
        ConferenceUser user = null;

        m_log.debug((bHold ? "Putting User on hold" : "Resuming user from hold") + ": " + name);

        if(null != (user = findUser(name))) {
            String id = user.getId();
            if(null != id) {
                m_mediaProcessor.holdConferenceParticipant(this, id, bHold);
            }
        }
    }

    /**
     * Updates a conference participant's remote destination address
     * @param name The user name or SIP URI of the participant
     * @param sdp The SDP of the remote client.  The SDP may contain update remote connection information.
     */
    public void updateUserAddress(String name, SDP sdp) throws Exception {
        ConferenceUser user = null;

        m_log.debug("Updating User: " + name + ", connect location: " + sdp.printTargetIp(false));

        if(null != (user = findUser(name))) {
            String id = user.getId();
            if(null != id) {
                m_mediaProcessor.updateConferenceParticipantDestination(this, id, sdp);
            }
        }
    }

    /**
     * Removes all participants from the conference
     */
    public synchronized void removeUsers() {
        Iterator<String> iter;
        ConferenceUser user;

        synchronized(m_mapUsers) {
            iter = m_mapUsers.keySet().iterator();
            while(iter.hasNext()) {
                String userName = iter.next();
                if(null != (user = m_mapUsers.remove(userName))) {
                    try {
                        onRemove(user);
                    } catch(Exception e) {
                        LogUtil.printError(m_log, null, e);
                    }
                }
            }
        }

    }

    /**
     * Checks if a conference participant is currently an active member of the conference
     * @param name The user name or SIP URI
     * @param bPollConference If set to <i>true</i> then the underlying media implementation will be queried to check
     *                        the current participant state.  If set to <i>false</i> then only the current un-synced listing
     *                        of active users is queried.
     * @param bDelayedStartOk If set to <i>false</i> then this method is allowed to return <i>true</i> even if the user was added
     *                        without any SDP, such as when adding a user before a remote 200 OK INVITE response has been received.
     * @return <i>true</i> if the participant is a member of the conference, false otherwise
     * 
     */
    public boolean isConferenceUserActive(String name, boolean bPollConference, boolean bDelayedStartOk) 
                       throws Exception{
        boolean bIsActive = false;
        ConferenceUser user = findUser(name);

        if(null != user) {

            if(bPollConference) {

                String id = user.getId();
                if(null != id) {
                    if(true == (bIsActive = m_mediaProcessor.isConferenceParticipantActive(this, id))) {
                        if(false == bDelayedStartOk && user.isDelayedStart()) {
                            return false;
                        }
                    }
                }

                // Remove the user from the map if the conference interface implemenation
                // has reported the user to be inactive
                if(false == bIsActive) {
                    removeUser(name);
                }

            } else {
                bIsActive = true;
            }
        }

        return bIsActive;
    }

    /**
     *
     */
    void syncConferenceUsers() {
        String [] idsActive = null;
        ConferenceUser [] usersStored = null;
        int indexStored = 0;

        m_lastSyncTm = System.currentTimeMillis();
        idsActive = m_mediaProcessor.getConferenceParticipants(this);

        synchronized(m_mapUsers) {
            usersStored = new ConferenceUser[m_mapUsers.size()];
            for(Map.Entry<String, ConferenceUser> entry : m_mapUsers.entrySet()) {
                ConferenceUser confUser = entry.getValue();

                if(null != confUser) {
                    // Since this instance will be referred to outside of the locked m_mapUsers scope
                    // it's a copy of the ConferenceUser object
                    if(false == confUser.isPendingIdRetrieval()) {
                        usersStored[indexStored++] = new ConferenceUser(confUser.getName(), confUser.getId());
                    }
                }
            }
        }

        if(null != usersStored) {
            int indexActive;
            for(indexStored = 0; indexStored < usersStored.length; indexStored++) {

                if(null == usersStored[indexStored]) {
                    continue;
                }

                boolean bFound = false;

                if(null != idsActive) {
                    for(indexActive = 0; indexActive < idsActive.length; indexActive++) {
                        if(null != usersStored[indexStored].getId() && 
                           usersStored[indexStored].getId().equals(idsActive[indexActive])) {
                            bFound = true;
                            break;
                        }

                    }
                }
                if(false == bFound) {
                    m_log.info("syncConferenceUsers removing user:" + usersStored[indexStored].getName() + 
                                 ", id: " + usersStored[indexStored].getId() + " from conference: " + m_name);
                    //
                    // Mark the user as out-of-sync with the local conference db 
                    // ie., the user does not exist anymore in the media processor instance
                    //
                    setUnSyncedState(usersStored[indexStored].getName(), true);
                    removeUser(usersStored[indexStored].getName());
                }
            }
        }

    }

    private void setUnSyncedState(String userName, boolean bSyncedState) {
        synchronized(m_mapUsers) {
            if(m_mapUsers.containsKey(userName)) {
                ConferenceUser conferenceUser = m_mapUsers.get(userName);
                if(null != conferenceUser) {
                    conferenceUser.setUnsynced(bSyncedState);
                }
            }
        }
    }

    private class ConferenceUser {

        private String m_name;  // SIP name
        private String m_id;    // conference manager internal unique ID
        private boolean m_bDelayedStart;
        private boolean m_bUnsynced;
        private boolean m_bPendingIdRetrieval;
        private SipSessionContext m_sipSessionContext; // SIP specific user unique id
 
        ConferenceUser(String name) {
            set(name, null);
        }

        ConferenceUser(String name, String id) {
            set(name, id);
        }

        private void set(String name, String id) {
            m_name = name;
            m_id = id;
            m_bUnsynced = false;            
            m_sipSessionContext = null;
            m_bDelayedStart = true;
            m_bPendingIdRetrieval = true;
        }

        String getName() {
            return m_name;
        }

        String getId() {
            return m_id;
        }

        void setId(String id) {
            m_id = id;
        }

        private boolean isPendingIdRetrieval() {
            return m_bPendingIdRetrieval;
        }

        private void setPendingIdRetrieval(boolean bPendingIdRetrieval) {
            m_bPendingIdRetrieval = bPendingIdRetrieval;
        }

        boolean getUnsynced() {
            return m_bUnsynced;
        }

        void setUnsynced(boolean bUnsynced) {
            m_bUnsynced = bUnsynced;
        }

        SipSessionContext getSipSessionContext() {
            return m_sipSessionContext;
        }

        void setSipSessionContext(SipSessionContext sipSessionContext) {
            m_sipSessionContext = sipSessionContext;
        }

        boolean isDelayedStart() {
            return m_bDelayedStart;
        }

        void setDelayedStart(boolean bDelayedStart) {
            m_bDelayedStart = bDelayedStart;
        }

    }

} // end of class Conference
