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

package com.openvcx.conference.media;

import com.openvcx.conference.*;

/**
 *
 * <p>Interface to a media processor conferencing implementation</p>
 */
public interface IMediaProcessor {

    /**
     * <p>Start a new audio/video conference</p>
     * <p>A conference is an audio/video media session with one or more participants</p>
     * @throws Exception 
     * @param conference A conference instance to be started
     * @return a unique conference instance identifier assigned by the underlying media implementation
     */
    public InstanceId startConference(Conference conference) throws Exception;

    /**
     * Terminates a conference instance and destroys all associated resources created by the underlying media implementation
     * @param conference A previously started conference instance
     */
    public void closeConference(Conference conference) throws Exception;

    /**
     * Adds a participant to an existing conference instance
     * @param conference A previously started conference instance
     * @param sdpLocal The SDP of the local server.  This is the local response SDP to an incoming INVITE or the local INVITE SDP of an outgoing call. 
     * @param sdpRemote The SDP of remote client.  This is the remote SDP from an incoming INVITE or the remote SDP response to a local INVITE.
     *                  <i>sdpRemote</i> can be null if a remote 200 OK INVITE response has not yet been received.
     * @param user The participant user instance being added to the conference
     * @param idUpdate An optional parameter instructing the underlying media implementation that an existing participant 
     *                 state should be updated instead of added to the conference.  The value of
     *                 this parameter should be the return value of a previous call to add the participant into the 
     *                 conference.  If a conference is not being updated but a participant is being added then this parameter should be null.
     * @param turnRelay An optional paramater requesting establishment of a TURN relay for incoming media
     * @return A unique identifier of the conference participant assigned by the underlying media implementation
     */
    public String addConferenceParticipant(Conference conference, SDP sdpLocal, SDP sdpRemote, IUser user, String idUpdate, TURNRelay turnRelay)  
                      throws Exception;

    /**
     * Removes a participant from the conference
     * @param conference A previously started conference instance
     * @param id The unique identifier of the conference participant 
     */
    public void removeConferenceParticipant(Conference conference, String id) 
                    throws Exception;

    /**
     * Checks if a conference participant is currently a recognized and active member of the conference according to the
     * underlying media implementation 
     * @param conference A previously started conference instance
     * @param id The unique identifier of the conference participant 
     * @return true if the participant is active and is currently being serviced by the media processor.  
     *         false if the participant is no longer a member of the media conference
     */
    public boolean isConferenceParticipantActive(Conference conference, String id);

    /**
     * Attempts to place a conference participant on hold or removes a participant from the hold state
     * @param conference A previously started conference instance
     * @param id The unique identifier of the conference participant 
     * @param bHold The hold state.  Set to true if going on hold, or false if resuming from hold 
     */
    public void holdConferenceParticipant(Conference conference, String id, boolean bHold) throws Exception;

    /**
     * Returns a listing of all active conference participants
     * @param conference A previously started conference instance
     */
    public String [] getConferenceParticipants(Conference conference);

    /**
     * <p>Updates any media prooperties of an active conference participant.</p>
     * <p>For eg., the output RTP destination, ICE credentials, SRTP keys, etc...</p>
     * @param conference A previously started conference instance
     * @param id The unique identifier of the conference participant 
     * @param sdpRemote An updated version of the SDP of the remote client 
     */
    public void updateConferenceParticipantDestination(Conference conference, String id, SDP sdpRemote) throws Exception;


} 
