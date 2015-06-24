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

import org.apache.log4j.Logger;

/**
 * A conference application monitor used for asynchronous housekeeping by the conference manager
 */
public class ConferenceMonitor implements Runnable {

    private final Logger m_log = Logger.getLogger(getClass());

    private static final long CONFERENCE_FRESH_SYNC_INTERVAL_MS    = 4500;
    private static final long CONFERENCE_MATURE_SYNC_INTERVAL_MS   = 9000;
    private static final long CONFERENCE_MATURE_THRESHOLD_MS       = 20000;
    private static final long CONFERENCE_IDLE_MAX_MS               = 13000;

    private ConferenceManager m_conferenceManager = null;
    private boolean bRunMonitor = true;

    /**
     * Constructor used to instantiate the monitor instance
     */
    public ConferenceMonitor(ConferenceManager conferenceManager) {
        m_conferenceManager = conferenceManager;
    }
 
    /**
     * Starts the monitor thread
     */
    public void run() {

        while(bRunMonitor) {

            try {
             
                while(bRunMonitor && checkConferenceUsers());

                //m_log.debug("ConferenceMonitor checking if conference ended...");
                ConferenceDefinition.Name conferenceName = findFinishedConference();
                if(null != conferenceName) {
                    m_log.debug("ConferenceMonitor removing conference: " + conferenceName);
                    m_conferenceManager.removeConference(conferenceName);

                } else {
                    try {
                        Thread.sleep(2000);
                    } catch(InterruptedException e) {
                        bRunMonitor = false;
                        m_log.error(e);
                    }
                }
            } catch(Exception e) {
                LogUtil.printError(m_log, null, e);
            }

        }

        m_log.debug("ConferenceMonitor exiting");
    }

    /**
     * Stops the monitor thread
     */
    public void stop() {
        bRunMonitor = false;
    }

    private Conference findConferenceOldestSync() {
        Conference conference = null;

        for(Map.Entry<String, Conference> entry : 
            m_conferenceManager.s_confDB.m_mapConferences.entrySet()) {
            if(null == conference || entry.getValue().getLastSyncTime() < conference.getLastSyncTime()) {
                conference = entry.getValue();
            }
        }
        return conference;
    }


    private boolean checkConferenceUsers() {
        long tm = System.currentTimeMillis();
        Conference conference = null;

        //
        // Get the conference with the oldest sync time
        //
        synchronized(m_conferenceManager.s_confDB.m_mapConferences) {
            if(null != (conference = findConferenceOldestSync())) {
                long ageMs = tm - conference.getCreateTime();
                if((ageMs <= CONFERENCE_MATURE_THRESHOLD_MS && 
                    conference.getLastSyncTime() + CONFERENCE_FRESH_SYNC_INTERVAL_MS > tm) ||
                    conference.getLastSyncTime() + CONFERENCE_MATURE_SYNC_INTERVAL_MS > tm) {
                    conference = null;
                }
            }
        }

        if(null != conference) {
            //
            // Sync the conference map participant list with what the participants list
            // returned from the conference media processor and remove / send BYE
            // to any participants no longer handled by the media processor.
            //
            conference.syncConferenceUsers();
            //m_log.debug("checkConferenceUsers returning true");
            return true;
        } else {
            //m_log.debug("checkConferenceUsers returning false");
            return false;
        }

    }

    private ConferenceDefinition.Name findFinishedConference() {
        long tm = System.currentTimeMillis();

        synchronized(m_conferenceManager.s_confDB.m_mapConferences) {
            for(Map.Entry<String, Conference> entry : 
                m_conferenceManager.s_confDB.m_mapConferences.entrySet()) {
                Conference conference = entry.getValue();
                long updateTm;
 

                //
                // This just checks if the conference has been running w/o any participants for CONFERENCE_IDLE_MAX_MS
                // ie., the last time a user was removed.
                //
                if(conference.getParticipantCount() <= 0 && (updateTm = conference.getLastParticipantUpdateTime()) > 0 && 
                   updateTm + CONFERENCE_IDLE_MAX_MS < tm) {
                    return conference.getName();
                }
            }
        }
        return null;
    }
 
}
