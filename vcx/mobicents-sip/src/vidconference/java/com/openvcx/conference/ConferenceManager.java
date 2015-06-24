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

import java.io.BufferedReader;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.File;
import java.io.BufferedWriter;
import java.io.FileWriter;
import java.io.PrintWriter;
import java.lang.SecurityException;
import java.net.MalformedURLException;
import java.net.URLConnection;
import java.net.URL;
import java.net.InetAddress;
import java.net.UnknownHostException;
import java.net.SocketException;
import java.util.ArrayList;
import java.util.HashMap; 
import java.util.Map; 
import java.util.Iterator;
import java.util.List;

import org.apache.log4j.Logger;

/**
 *
 * The Conference controller responsible for managing and updating conferences
 *
 */
public class ConferenceManager {

    private final Logger m_log = Logger.getLogger(getClass());

    private IMediaProcessor m_mediaProcessor = null;
    static ConferenceDB s_confDB = null;
    private ConferenceMonitor m_monitor = null;
    private PortManager m_portManager;
    private String m_tempDir;
    private String m_strConnectAddress;

    private static final String CONFIG_CONFERENCE_TEMP_DIR       = "conference.temp.dir";

    /**
     * The default temporary file path
     */
    private static final String CONFERENCE_TEMP_DIR               = "temp";

    /**
     * The database of running conferences
     */
    static class ConferenceDB {

        Map<String, Conference> m_mapConferences;

        private ConferenceDB() {
            m_mapConferences = new HashMap<String, Conference>(); 
        }
        public static synchronized ConferenceDB create() {
            if(null == s_confDB) {
                s_confDB = new ConferenceDB();
            }
            return s_confDB;  
        }
    }

    /**
     * Constructor used to initialize the instance
     * @param mediaProcessor The underlying media processor used to handle media level tasks
     * @param portManager The port manager use to allocate communications ports
     * @param bRunMonitor Set to true to enable this instance to start a <i>ConferenceMonitor</i> instance which will perform conference maintenance and cleanup duties
     * @param config The application configuration instance
     */
    public ConferenceManager(IMediaProcessor mediaProcessor, 
                             PortManager portManager, 
                             boolean bRunMonitor, 
                             Configuration config) throws IOException, UnknownHostException {
        ConferenceDB.create();
        m_mediaProcessor = mediaProcessor;
        m_portManager = portManager;

        m_tempDir =  config.getString(CONFIG_CONFERENCE_TEMP_DIR, CONFERENCE_TEMP_DIR);
        m_strConnectAddress = InetAddress.getByName(config.getString(
                                                    SDPResponder.CONFIG_SERVER_CONNECT_ADDRESS)).getHostAddress();

        if(bRunMonitor) {
            m_monitor = new ConferenceMonitor(this);
            new Thread(m_monitor).start();
        }

    }

    /**
     * Used to stop and de-initialize the manager instance
     */
    public void stop() {
        if(null != m_monitor) {
            m_monitor.stop();
        }
    }

    private Conference findExistingConference(ConferenceDefinition.Name conferenceName) {
        
        synchronized(s_confDB.m_mapConferences) {
            if(s_confDB.m_mapConferences.containsKey(conferenceName.getString())) {
                return s_confDB.m_mapConferences.get(conferenceName.getString());
            }
        }

        return null;
    }

    /**
     * Stops and removes a running conference and all of it's participants.
     * @param conferenceName The conference name
     */
    public void removeConference(ConferenceDefinition.Name conferenceName) {
        Conference c = null;
        synchronized(s_confDB.m_mapConferences) {

            m_log.debug("Removing conference: " + conferenceName);

            if(null != (c = s_confDB.m_mapConferences.remove(conferenceName.getString()))) { 

                onConferenceRemove(c);

                c.removeUsers();

                try {
                    m_mediaProcessor.closeConference(c);                
                } catch(Exception e) {
                    m_log.error("Failed to close conference: " + conferenceName);
                    m_log.debug(e);
                }

                m_log.info("Removed conference: " + conferenceName + " with " + c.getParticipantCount() + " users.");
            }
        }
    }


    private void waitUntilConferenceStarted(final Conference c, long maxMs) {

        int index = 0;

        try {

            long tmWaited;
            long tmStart = System.currentTimeMillis();

            while(c.isStartInProgress()) {

                if(0 == index) {
                    m_log.debug("Waiting for conference: " + c + " startup");
                }

                if((tmWaited = System.currentTimeMillis() - tmStart) > maxMs) {
                    m_log.debug("Aborting wait for conference: " + c+ " startup after waiting " + tmWaited + "ms");
                    break;
                }

                Thread.sleep(200);
                index++;
            }

        } catch(InterruptedException e) {
            m_log.error("" + e);
        }

        if(index > 0) {
            m_log.debug("Done waiting for conference: " + c + " startup");
        }
    }

    private void checkIfConferenceRunning(ConferenceDefinition.Name conferenceName) {
        //TODO: this should be entirely relegated to the conference monitor
        checkIfConferenceRunning(findExistingConference(conferenceName));
    }

    private void checkIfConferenceRunning(Conference c) {

        if(null == c) {
            return;
        }

        waitUntilConferenceStarted(c, 6000);

        m_log.debug("Check if conference: " + c.getName() + " is running.  Have " + 
                     s_confDB.m_mapConferences.size() + " conferences");

        if((false == m_mediaProcessor.isConferenceParticipantActive(c, null))) {
            m_log.info("Conference " + c.getName() + " is not running");
            removeConference(c.getName());
        }

    }

    private Conference lookupAndStartConference(ConferenceDefinition conferenceDefinition, SDP sdpLocal) 
                           throws Exception {
        Conference c = null;
        ConferenceDefinition.Name conferenceName = conferenceDefinition.getConferenceName();

        if(null != (c = findExistingConference(conferenceName))) {
            return c;
        }

        synchronized(s_confDB.m_mapConferences) {
            Codec videoCodec = null;
            Codec audioCodec = null;
            SDP.MediaCandidate videoCandidate = sdpLocal.getVideoCandidate();
            if(null != videoCandidate) {
                videoCodec = videoCandidate.getCodec();
            }
            SDP.MediaCandidate audioCandidate = sdpLocal.getAudioCandidate();
            if(null != audioCandidate) {
                audioCodec = audioCandidate.getCodec();
            }

            c = new Conference(m_mediaProcessor, 
                               conferenceName, 
                               m_portManager.allocatePort(),
                               conferenceDefinition);

            s_confDB.m_mapConferences.put(conferenceName.getString(), c);
            c.setStartInProgress(true);
            m_log.debug("Created new conference object: " + conferenceName);
        }

        if(null == c) {
            throw new IOException("Unable to lookup conference: " + conferenceName);
        }

        try {

            InstanceId instanceId = m_mediaProcessor.startConference(c);
            m_log.info("Started new conference: " + conferenceName + " on port " + c.getPort() + 
                        ", instanceId: " + instanceId.toString());

            c.setInstanceId(instanceId);
            //c.setLastParticipantUpdateTime();
            c.setStartTime();

            onConferenceAdd(c);
            c.setStartInProgress(false);

        } catch(Exception e) {
            m_log.error("Failed to start conference: " + conferenceName);
            c.setStartInProgress(false);
            removeConference(conferenceName);
            throw e;
        } finally {
            c.setStartInProgress(false);
        }

        return c;
    }

    /**
     * Tests if a conference with the given name is currently ongoing
     * @param conferenceName The conference name
     * @return <i>true</i> if the conference is currently ongoing, </i>false</i> otherwise
     * @throws Exception Upon an error condition
     */
    public boolean isConferenceActive(ConferenceDefinition.Name conferenceName) throws Exception {
        return isConferenceParticipantActive(null, conferenceName, false, false);
    }

    /**
     * Tests if a conference participant is currently a member of the conference
     * @param userName The participant SIP username 
     * @param conferenceName The conference name
     * @param bPollConference Set to <i>true</i> if the underlying media processor should be polled to check if the user is active
     * @param bDelayedStartOk Set to <i>true</i> if the test should return <i>true</i> even if the conference participant has an associated media instance in delay start mode, such before a remote party's 200 OK SDP response has been received.
     * @return <i>true</i> if the participant is currently active, </i>false</i> otherwise
     */
    public boolean isConferenceParticipantActive(String userName, 
                                                 ConferenceDefinition.Name conferenceName, 
                                                 boolean bPollConference, 
                                                 boolean bDelayedStartOk) throws Exception{
        boolean bIsActive = false;
        Conference c = findExistingConference(conferenceName);

        if(null != c) {
            if(null == userName) {
                bIsActive = true;
            } else {
                bIsActive = c.isConferenceUserActive(userName, bPollConference, bDelayedStartOk);          
            }
        }

        return bIsActive;
    }

    /**
     * Finds a conference name that the participant is currently a member of
     * @param userName The participant SIP username
     * @return The conference name
     */
    public ConferenceDefinition.Name findParticipantConference(String userName) throws Exception {
        boolean bIsActive;

        synchronized(s_confDB.m_mapConferences) {
            Iterator iter = s_confDB.m_mapConferences.entrySet().iterator();
            while(iter.hasNext()) {
                Map.Entry pairs = (Map.Entry)iter.next();
                Conference conference = (Conference) pairs.getValue();
                if(true == (bIsActive = conference.isConferenceUserActive(userName, false, true))) {
                    return conference.getName();
                }
            }
        }

        return null;
    }

    /**
     * <p>Adds a participant to the conference</p>
     * <p>If the conference does not exist it is automatically created</p>
     * @param sdpLocal The SDP of the local conference endpoint
     * @param sdpRemote The SDP of the remote user endpoint
     * @param user The user which should be added into the conference
     * @param conferenceDefinition The conference definition of the conference to which the user will be added to
     * @param sipSessionContext An optional <i>SipSessionContext</i> of the sip session attribute of the originating SIP message
     */
    public void addToConference(SDP sdpLocal, 
                                SDP sdpRemote, 
                                User user, 
                                ConferenceDefinition conferenceDefinition,
                                SipSessionContext sipSessionContext,
                                TURNRelay turnRelay) throws Exception {
        Conference c = null;
        String id = null;
        ConferenceDefinition.Name conferenceName = conferenceDefinition.getConferenceName();

        m_log.debug("AddToConference: " + conferenceName + ", user: " + user.getName());

        checkIfConferenceRunning(conferenceName);

        if(null == (c = lookupAndStartConference(conferenceDefinition, sdpLocal))) {
            throw new IOException("Unable to lookup conference: " + conferenceName);
        }

        if(null == (id = c.addUser(user, sdpLocal, sdpRemote, sipSessionContext, turnRelay))) {
           throw new IOException("Unable to add user: " + user.getName() + " to conference: " + conferenceName);
        }

    }

    /**
     * <p>Removes aa participant from the conference</p>
     * @param userName The username which should be removed from the conference
     * @param conferenceName The name of the conference which the user should be removed from
     */
    public void removeFromConference(String userName, ConferenceDefinition.Name conferenceName) throws Exception {
        Conference c = null;

        if(null == (c = findExistingConference(conferenceName))) {
            throw new IOException("Unable to find conference: " + conferenceName);
        }

        c.removeUser(userName);

        checkIfConferenceRunning(conferenceName);

    }

    /**
     * Places a conference participant on hold or resumes from the hold state
     * @param userName The user name or SIP URI of the participant which should be removed from the conference
     * @param conferenceName The name of the conference which the user should be removed from
     * @param bHold The hold state.  Set to true if going on hold, or false if resuming from hold
     */
    public void conferenceHold(String userName, ConferenceDefinition.Name conferenceName, boolean bHold) throws Exception {
        Conference c = null;

        if(null == (c = findExistingConference(conferenceName))) {
            throw new IOException("Unable to find conference: " + conferenceName);
        }

        c.holdUser(userName, bHold);

        //checkIfConferenceRunning(conferenceName);

    }

    /**
     * Updates a conference participant's remote destination address
     * @param userName The user name or SIP URI of the participant which should be removed from the conference
     * @param conferenceName The name of the conference which the user should be removed from
     * @param sdp The current SDP of the remote user endpoint
     */
    public void conferenceParticipantAddressChange(String userName, 
                                                   ConferenceDefinition.Name conferenceName, 
                                                   SDP sdp) 
                    throws Exception {
        Conference c = null;

        if(null == (c = findExistingConference(conferenceName))) {
            throw new IOException("Unable to find conference: " + conferenceName);
        }

        c.updateUserAddress(userName, sdp);

        //checkIfConferenceRunning(conferenceName);

    }

    private void onConferenceAdd(Conference c) {
        String name = c.getInstanceId().getName();
        int port = c.getPort();

        try {

            {
                //
                // Store the live link path
                //
                String pathLive = m_tempDir + File.separatorChar + name + ".live";
                PrintWriter out = new PrintWriter(new BufferedWriter(
                              new FileWriter(new File(pathLive))));

                out.print(m_strConnectAddress + ":" + port);
                out.close();
                m_log.debug("Created conference port mapping file: " + pathLive);
            }

            if(null != c.getRecordingFilePath()) {
                //
                // Store the recorded link path
                //
                String pathRecord = m_tempDir + File.separatorChar + name + ".record";
                PrintWriter out = new PrintWriter(new BufferedWriter(
                              new FileWriter(new File(pathRecord))));

                //
                // c.getRecordingFilePath may contain a full path name to the recording file with 
                // regard to VSX_HOME.  In the link path we only want the last 2 segments,
                // which are the directory within the 'recording/' root directory, followed by
                // the filename path.
                //
                StringBuffer mediaUriPath = new StringBuffer();
                String [] arrRecordingPath = c.getRecordingFilePath().split("/");
                if(arrRecordingPath.length > 1) {
                    int index;
                    for(index = arrRecordingPath.length - 2; index < arrRecordingPath.length; index++) {
                        if(index > arrRecordingPath.length - 2) {
                            mediaUriPath.append(File.separatorChar);
                        }
                        mediaUriPath.append(arrRecordingPath[index]);
                    }
                    out.print(mediaUriPath.toString());
                }

                out.close();
                m_log.debug("Created conference recording mapping file: " + pathRecord + ", path: " + 
                             mediaUriPath.toString());
            }


        } catch(IOException e) {
            LogUtil.printError(m_log, "Failed to create conference port mapping file", e);
        }
        

    }

    private void onConferenceRemove(Conference c) {
        String name = c.getInstanceId().getName();
        String path = m_tempDir + File.separatorChar + name + ".live";

        new File(path).delete();
        m_log.debug("Removed conference port mapping file: " + path);
    }

}
