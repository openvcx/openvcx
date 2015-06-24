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
import java.io.InputStreamReader;
import java.io.IOException;
import java.io.File;
import java.io.FileInputStream;
import java.io.FileNotFoundException;
import java.util.ArrayList;
import java.util.List;
import java.nio.charset.Charset;

import org.apache.log4j.Logger;

/**
 *
 * <p>A conference definition governing all properties and capabilities of a media conference.</p>
 * <p>The conference definition is defined using a conference definition text file with the name matching the conference SIP URI.</p>
 *
 */
public class ConferenceDefinition {

    private final Logger m_log = Logger.getLogger(getClass());

    private EncoderConfigLoader m_encoderLoader;
    private String m_confFilePath;
    private String m_confFileName;
    private String m_name;                         // CONFIG_NAME
    private boolean m_disabled;                    // CONFIG_DISABLED
    private boolean m_allowDialin;                 // CONFIG_ALLOWDIALIN
    private int m_mtu;                             // CONFIG_MTU 
    private String [] m_arrAllowedUsers;           // CONFIG_ALLOWEDUSERS 
    private String m_pass;                         // CONFIG_PASSWORD
    private int m_maxUsers;                        // CONFIG_MAXUSERS;
    private String [] m_arrInviteUsers;            // CONFIG_INVITEUSERS 
    private boolean m_bInviteCalledParty;          // CONFIG_INVITECALLEDPARTY 
    private SRTP_POLICY m_srtpPolicy;              // CONFIG_SRTPPOLICY 
    private boolean m_allowDTLS;                   // CONFIG_DTLS
    private boolean m_preferDTLSSRTP;              // CONFIG_PREFERDTLS
    private String m_recordingName;                // CONFIG_RECORD;
    private String m_streamOutput;                 // CONFIG_OUTPUT;
    private String m_extraArgs;                    // CONFIG_EXTRAARGS
    private String m_layout;                       // CONFIG_LAYOUT
    private IEncoderConfig m_xcodeConfig;          // CONFIG_XCODE
    private boolean m_preferIceHost;               // CONFIG_PREFERICEHOST;
    private boolean m_contactHdrAsTarget;          // CONFIG_CONTACTHDRASTARGET;
    private boolean m_abrAuto;                     // CONFIG_ABRAUTO;
    private boolean m_abrSelf;                     // CONFIG_ABRSELF;
    private boolean m_enableTurnRelay;             // CONFIG_TURNRELAYENABLED;
    private String m_turnServer;                   // CONFIG_TURNSERVER;
    private String m_turnRealm;                    // CONFIG_TURNREALM;
    private String m_turnUser;                     // CONFIG_TURNUSER;
    private String m_turnPass;                     // CONFIG_TURNPASSWORD;
    private String m_joinChime;                    // CONFIG_JOINCHIME
    private String m_leaveChime;                   // CONFIG_LEAVECHIME
    private List<IEncoderConfig> m_listEncoders;   // CONFIG_ENCODER 

    private static final String FILE_SUFFIX               =       ".conference";
    private static final String CONFERENCE_DIR_AUTO       =       "auto/";

    private static final String CONFIG_NAME               =       "name";
    private static final String CONFIG_DISABLED           =       "disabled";
    private static final String CONFIG_ALLOWDIALIN        =       "allowDialin";
    private static final String CONFIG_MTU                =       "mtu";
    private static final String CONFIG_ALLOWEDUSERS       =       "allowedSIPUris";
    private static final String CONFIG_PASSWORD           =       "password";
    private static final String CONFIG_MAXUSERS           =       "maxUsers";
    private static final String CONFIG_INVITEUSERS        =       "inviteSIPUris";
    private static final String CONFIG_INVITECALLEDPARTY  =       "inviteCalledParty";
    private static final String CONFIG_SRTPPOLICY         =       "SRTPPolicy";
    private static final String CONFIG_DTLS               =       "allowDTLS";
    private static final String CONFIG_PREFERDTLS         =       "preferDTLSSRTP";
    private static final String CONFIG_RECORD             =       "record";
    private static final String CONFIG_STREAMOUTPUT       =       "streamOutput";
    private static final String CONFIG_EXTRAARGS          =       "extraArgs";
    private static final String CONFIG_LAYOUT             =       "layout";
    private static final String CONFIG_XCODE              =       "xcode";
    private static final String CONFIG_PREFERICEHOST      =       "icePreferHost";
    private static final String CONFIG_CONTACTHDRASTARGET =       "useContactHeaderAsTarget";
    private static final String CONFIG_ABRAUTO            =       "abr";
    private static final String CONFIG_ABRSELF            =       "abrSelf";
    private static final String CONFIG_TURNRELAYENABLED   =       "TURNRelayEnabled";
    private static final String CONFIG_TURNSERVER         =       "TURNServer";
    private static final String CONFIG_TURNREALM          =       "TURNRealm";
    private static final String CONFIG_TURNUSER           =       "TURNUser";
    private static final String CONFIG_TURNPASS           =       "TURNPassword";
    private static final String CONFIG_JOINCHIME          =       "joinChime";
    private static final String CONFIG_LEAVECHIME         =       "leaveChime";
    private static final String CONFIG_ENCODER            =       "encoder";

    static SRTP_POLICY SRTP_POLICY_DEFAULT         =       SRTP_POLICY.SUPPORTED;

    /**
     * The conference SRTP security policy
     */
    public enum SRTP_POLICY {

        /**
         * An un-initialized SRTP policy.
         */
        UNKNOWN,

        /**
         * SRTP will never be used. 
         */
        NEVER,

        /**
         * The conference SDP responder will try to negotiate SRTP if the remote endpoint is able to support it.
         */
        SUPPORTED,

        /**
         * SRTP is mandatory for an endpoint to join the conference.
         */
        REQUIRED
    }

    private SRTP_POLICY readSRTPPolicy(String value) {
        if("never".equalsIgnoreCase(value)) {
            return SRTP_POLICY.NEVER;
        } else if("required".equalsIgnoreCase(value)) {
            return SRTP_POLICY.REQUIRED;
        } else if("supported".equalsIgnoreCase(value)) {
            return SRTP_POLICY.SUPPORTED;
        } else {
            return SRTP_POLICY.UNKNOWN;
        }
    }

    ConferenceDefinition(String dir, String fromName, String toName) 
               throws FileNotFoundException, IOException {

        init(dir, fromName, toName);
        m_encoderLoader = new EncoderConfigLoader(this);

    }

    private void reset() {

        m_confFilePath = null;
        m_confFileName = null;
        m_name = null;
        m_disabled = false;
        m_allowDialin = true;
        m_mtu = 0;
        m_arrAllowedUsers = null;
        m_pass = null;
        m_maxUsers = 0;
        m_arrInviteUsers = null;
        m_bInviteCalledParty = false;
        m_srtpPolicy = SRTP_POLICY_DEFAULT;
        m_allowDTLS = true;
        m_preferDTLSSRTP = false;  // note we currently wish to use SDES even if a=fingerprint is present in SDP
        m_recordingName = null;
        m_streamOutput = null;
        m_extraArgs = null;
        m_layout = null;
        m_xcodeConfig = null;
        m_preferIceHost = false;
        m_contactHdrAsTarget = false;
        m_abrAuto = true;
        m_abrSelf = false;
        m_enableTurnRelay = false;
        m_turnServer = null;
        m_turnRealm = null;
        m_turnUser = null;
        m_turnPass = null;
        m_joinChime = null;
        m_leaveChime = null;
        m_listEncoders = new ArrayList<IEncoderConfig>();
    }

    /**
     *
     * <p>A class used to encapsulate the conference name</p>
     * <p>This class is here to so that any operations on the Conference name, such as those through 
     * the ConferenceManager API, are done by using the name which can only be obtained from the
     * <i>ConferenceDefinition</i> class, and not from a SIP URI <i>to.getName()</i></p>
     *
     **/
    public static class Name {

        private String m_name;

        private Name(String name) {
            m_name = name;
        }

        static Name createTestOnly(String name) { return new Name(name); }

        /**
         * Retrieves the conference name as a string
         * @return The conference name
         */
        public String getString() {
            return m_name;
        }

        /**
         * Retrieves the conference name as a string
         * @return The conference name
         */
        public String toString() {
            return getString();
        }
    }

    /**
     * <p>Retrieves the conference name</p>
     * <p>The name of the conference is either the filename of the conference definition file or the <i>name</i> element found within the conference definition file.
     * <p>This may be defined by the <i>name</i> conference definition file configuration element</p>
     * @return The name of the conference
     */
    public Name getConferenceName() {
        return new Name(null != m_name && m_name.length() > 0 ? m_name : m_confFileName);
    }

    /**
     * <p>Retrieves the SRTP policy of the conference</p>
     * <p>This is defined by the <i>SRTPPolicy</i> conference definition file configuration element</p>
     * @return The conference SRTP policy
     */
    public SRTP_POLICY getSRTPPolicy() {
        return m_srtpPolicy;
    }

    /**
     * <p>Tests if a participant joining the conference is permitted to utilize DTLS-SRTP.  If this is not enabled then SRTP-SDES may still be utilized.</p>
     * <p>This is defined by the <i>allowDTLS</i> conference definition file configuration element</p>
     * @return The <i>allowDTLS</i> boolean setting
     */
    public boolean isAllowDTLS() {
        return m_allowDTLS;
    }

    /**
     * <p>Explicitly disable DTLS for the conference even if the conference definition file permits it by setting <i>allowDTLS</i> to <i>true</i></p>
     */
    public void disableDTLS() {
        m_allowDTLS = false;
    }

    /**
     * <p>Tests if a participant joining the conference should use DTLS-SRTP in favor of SRTP-SDES if both are supported.</p>
     * <p>This is defined by the <i>preferDTLSSRTP</i> conference definition file configuration element.</p>
     * @return The <i>preferDTLSSRTP</i> boolean setting
     */
    public boolean isPreferDTLSSRTP() {
        return m_preferDTLSSRTP;
    }

    /**
     * <p>Retrieves the video visual layout configuration</p>
     * <p>The layout values are specific to the conference media processor</p>
     * <p>This is defined by the <i>layout</i> conference definition file configuration element</p>
     * @return The conference <i>layout </i> configuration
     */
    public String getLayout() {
        return m_layout;
    }

    /**
     * <p>Retrieves the audio announcement chime to be played when a participant joins the conference</p>
     * <p>This is defined by the <i>joinChime</i> conference definition file configuration element</p>
     * @return The conference <i>joinChime</i> configuration
     */
    public String getJoinChime() {
        return m_joinChime;
    }

    /**
     * <p>Retrieves the audio announcement chime to be played when a participant leaves the conference</p>
     * <p>This is defined by the <i>leaveChime</i> conference definition file configuration element</p>
     * @return The conference <i>leaveChime</i> configuration
     */
    public String getLeaveChime() {
        return m_leaveChime;
    }

    /**
     * <p>Retrieves any additional command line arguments which should be passed to the external media processor startup script when starting a new conference</p>
     * <p>This is defined by the <i>extraArgs</i> conference definition file configuration element</p>
     * @return The conference <i>extraArgs</i> configuration
     */
    public String getExtraArgs() {
        return m_extraArgs;
    }

    /**
     * <p>Retrieves the maximum MTU which should be set for a participant joining the conference.</p>
     * <p>The MTU is defined as the maximum RTP payload size</p>
     * <p>This is defined by the <i>mtu</i> conference definition file configuration element</p>
     * @return The conference <i>mtu</i> configuration
     */
    public int getMTU() {
        return m_mtu;
    }

    /**
     * <p>Retrieves the maximum number of participants allowed to join a conference.</p>
     * <p>This setting can be utilized to restrict the  participant limit to be less than the limit of the conference media processor</p>
     * <p>This is defined by the <i>maxUsers</i> conference definition file configuration element</p>
     * @return The conference <i>maxUsers</i> configuration
     */
    public int getMaxUsers() {
        return m_maxUsers;
    }

    /**
     * <p>Retrieves the conference password.</p>
     * <p>The conference password is used to validate a participant's SIP Digest authentication password.</p>
     * <p>This is defined by the <i>password</i> conference definition file configuration element</p>
     * @return The conference <i>password</i> configuration
     */
    public String getPassword() {
        return m_pass;
    }

    /**
     * <p>Retrieves the conference media transcoder configuration string.</p>
     * <p>The transcoder configuration configures the underlying media processor with any optional transcoder settings</p>
     * <p>This is defined by the <i>xcode</i> conference definition file configuration element</p>
     * @return The conference <i>xcode</i> configuration
     */
    public String getInitializerString() {
        return (null != m_xcodeConfig ? m_xcodeConfig.getInitializerString() : null); 
    }

    /**
     * <p>Retrieves the conference media transcoder video codec name.</p>
     * <p>This is defined by the <i>xcode</i> conference definition file configuration element</p>
     * @return The conference <i>xcode</i> configuration video codec name
     */
    public String getVideoCodecName() {
        return (null != m_xcodeConfig ? m_xcodeConfig.getVideoCodecName() : null); 
    }

    /**
     * <p>Retrieves the conference recording setting.</p>
     * <p>The <i>record</i> setting can either be a boolean or the filename of the recorded content.</p>
     * <p>This is defined by the <i>record</i> conference definition file configuration element</p>
     * @return The conference <i>record</i> configuration
     */
    public String getRecordingName() {
        return m_recordingName;
    }

    /**
     * <p>Retrieves the conference live webcasting stream output setting.</p>
     * <p>This is defined by the <i>streamOutput</i> conference definition file configuration element</p>
     * @return The conference <i>streamOutput</i> configuration
     */
    public String getStreamOutput() {
        return m_streamOutput;
    }

    /**
     * <p>Tests if the conference definition file is disabled.</p>
     * <p>If <i>disabled</i> is set then the system should ignore the conference definition file as if it did not exist.</p>
     * <p>This is defined by the <i>disabled</i> conference definition file configuration element</p>
     * @return The <i>disabled</i> boolean setting
     */
    public boolean isDisabled() {
        return m_disabled;
    }

    /**
     * <p>Tests if the conference definition file supports dialing in by participants other than the conference owner.</p>
     * <p>This is defined by the <i>allowDialin</i> conference definition file configuration element</p>
     * @return The <i>allowDialin</i> boolean setting
     */
    public boolean isAllowDialin() {
        return m_allowDialin;
    }

    /**
     * <p>Tests if the conference ICE candidate lookup algorithm should ignore any <i>srflx</i> and <i>prflx</i> and use <i>host</i> candidates only</p>
     * <p>In some circumstances this setting should be enabled when running the conference server and client on the same machine.</p>
     * <p>This is defined by the <i>icePreferHost</i> conference definition file configuration element</p>
     * @return The <i>icePreferHost</i> boolean setting
     */
    public boolean isPreferIceHost() {
        return m_preferIceHost;
    }

    /**
     * <p>Tests if the conference should use the originating SIP address as the media RTP destination.</p>
     * <p>This setting is used for testing.</p>
     * <p>This is defined by the <i>useContactHeaderAsTarget</i> conference definition file configuration element</p>
     * @return The <i>useContactHeaderAsTarget</i> boolean setting
     */
    public boolean isContactHeaderAsTarget() {
        return m_contactHdrAsTarget;
    }

    /**
     * <p>Tests if the conference media processor should use ABR (Adaptive Bitrate) to adjust streaming output based on RTP recipient network feedback.</p>
     * <p>This is defined by the <i>abr</i> conference definition file configuration element</p>
     * @return The <i>abr</i> boolean setting
     */
    public boolean isABRAuto() {
        return m_abrAuto;
    }

    /**
     * <p>Tests if the conference media processor should use ABR (Adaptive Bitrate) to adjust streaming output based on slow CPU performance.</p>
     * <p>This setting is experimental.</p>
     * <p>This is defined by the <i>abrSelf</i> conference definition file configuration element</p>
     * @return The <i>abrSelf</i> boolean setting
     */
    public boolean isABRSelf() {
        return m_abrSelf;
    }

    /**
     * <p>Tests if the conference media processor should enable TURN if the peer SDP supports it</p>
     * <p>This is defined by the <i>TURNRelayEnabled</i> conference definition file configuration element</p>
     * @return The <i>TURNRelayEnabled</i> boolean setting
     */
    public boolean isTURNRelayEnabled() {
        return m_enableTurnRelay;
    }

    /**
     * <p>Retrieves the TURN server host or address</p>
     * <p>This may be defined by the <i>TURNServer</i> conference definition file configuration element</p>
     * @return The address of the TURN server
     */
    public String getTurnServer() {
        return m_turnServer;
    }

    /**
     * <p>Retrieves the TURN realm</p>
     * <p>This may be defined by the <i>TURNRealm</i> conference definition file configuration element</p>
     * @return The realm used for authenticating with the TURN server
     */
    public String getTurnRealm() {
        return m_turnRealm;
    }

    /**
     * <p>Retrieves the TURN username</p>
     * <p>This may be defined by the <i>TURNUser</i> conference definition file configuration element</p>
     * @return The username used for authenticating with the TURN server
     */
    public String getTurnUser() {
        return m_turnUser;
    }

    /**
     * <p>Retrieves the TURN password</p>
     * <p>This may be defined by the <i>TURNPassword</i> conference definition file configuration element</p>
     * @return The password used for authenticating with the TURN server
     */
    public String getTurnPassword() {
        return m_turnPass;
    }

    /**
     * <p>Retrieves the list of participant specific media transcoder configurations.</p>
     * <p>This is defined by the <i>encoder</i> conference definition file configuration element</p>
     * @return The list of <i>encoder</i> settings
     */
    public List<IEncoderConfig> getEncoderList() {
        return m_listEncoders;
    }

    /**
     * <p>Retrieves the list of SIP participant to invite to the conference.</p>
     * <p>This is defined by the <i>inviteSIPUris</i> conference definition file configuration element</p>
     * @return The list of <i>inviteSIPUris</i> SIP URIs 
     */
    public String [] getInviteUsers() {
        return m_arrInviteUsers;
    }

    /**
     * <p>Tests if the conference should send an INVITE to the called party SIP URI</p>
     * <p>This is defined by the <i>inviteCalledParty</i> conference definition file configuration element</p>
     * @return The <i>inviteCalledParty</i> boolean setting
     */
    public boolean isInviteCalledParty() {
        return m_bInviteCalledParty;
    }

    /**
     * <p>Checks if the user is permitted to join the conference</p>
     * <p>If the conference definition contains one ore more <i>allowedSIPUris</i> then the validation will return <i>true</i> only if the user's SIP URI is in the allowed list</p>
     * @param sipUri  The user SIP URI which will be validated.  
     * @return <i>true</i> if the user is allwoed to join the conference, otherwise <i>false</i>
     */
    public boolean validateUser(String sipUri) {

        if(null == m_arrAllowedUsers) {
            return true;
        }

        for(int index = 0; index < m_arrAllowedUsers.length; index++) {

            if(true == ConfigUtil.isSipURIMatchFilter(sipUri, m_arrAllowedUsers[index])) {
                return true;
            }
        }
        
        return false;
    }

    /**
     * Retrieves the transcoder config loader responsible for retrieving an encoder configuration
     * @return The <i>EncoderConfigLoader</i> instance
     */
    public EncoderConfigLoader getEncoderConfigLoader() {
        return m_encoderLoader;
    }

    private void init(String dir, String fromName, String toName)
               throws FileNotFoundException, IOException {

        String confDir = dir;
        boolean bUseAutoTo = false;

        reset();

        // Append '/' 
        if(confDir.charAt(confDir.length() -1) != File.separatorChar) {
            confDir += File.separator;
        }

        //
        // Try to load the conference definition from the 'conferences/' dir
        //
        if(null == (m_confFilePath = findDefinitionInDir(confDir, new User(toName).getName()))) {

            //
            // Try to load the conference definition from the 'conferences/auto' dir
            // We do this by looking at the 'from' field (not 'to')
            //
            confDir += CONFERENCE_DIR_AUTO;

            if(null == (m_confFilePath = findDefinitionInDir(confDir, new User(fromName).getName()))) {

                bUseAutoTo = true;
                //
                // Try to load the conference definition from the 'conferences/auto' dir
                // We do this by looking at the 'to' field
                //
                m_confFilePath = findDefinitionInDir(confDir, new User(toName).getName());
            }
        }

        if(null != m_confFilePath) {
            load(m_confFilePath);

            if(bUseAutoTo && false == isAllowDialin()) {
                m_log.warn("Conference definition path: " + m_confFilePath + " found but " + 
                     CONFIG_ALLOWDIALIN + " is disabled for call from: " + fromName + ", to: " + toName);
                reset();
            }
        }

        if(null == m_confFilePath) {
            throw new FileNotFoundException("Conference definition for " + fromName + " to " + toName +
                                                " does not exist in: " + dir);
        }

    }

    private String findDefinitionInDir(String confDir, String name) {
        boolean bCanRead = false;
        int match;
        String confName = name;
        String path = null;

        m_confFileName = confName;
        path = confDir + confName;

        //m_log.debug("findDefinitionInDir dir:" + confDir + ", name: " + name);

        // Try to read the conference name matching the name as-is
        if(false == bCanRead) {
            bCanRead = new File(path).canRead();
        }

        // Append the '.conference' suffix to the path and try to read
        if(false == bCanRead) {
            path += FILE_SUFFIX;
            bCanRead = new File(path).canRead();
        }

        if(false == bCanRead) {

            if((match = confName.indexOf('@')) > 0) {
                // Strip out any domain part of the sip uri 
                String confName2 = confName.substring(0, match);
                path = confDir + confName2;
                bCanRead = new File(path).canRead();

                // Append the '.conference' suffix to the path and try to read
                if(false == bCanRead) {
                    path += FILE_SUFFIX;
                    bCanRead = new File(path).canRead();
                }

                if(bCanRead) {
                    m_confFileName = confName2;
                }
            }
        }

        return (true == bCanRead ? path : null);
    }

    private boolean getBoolean(String value) {
        if("true".equalsIgnoreCase(value)) {
            return true;
        } else {
            return false;
        }
    }
   
    private void load(String confFilePath) throws FileNotFoundException, IOException {
        String line;
        int match;

        BufferedReader br = new BufferedReader(new InputStreamReader((new FileInputStream(
                            new File(confFilePath))), Charset.forName("UTF-8")));

        while ((line = br.readLine()) != null) {

            line = line.trim();

            if(line.length() <= 0 || '#' == line.charAt(0) || (match = line.indexOf('=')) <= 0) {
                continue;
            }

            String key = line.substring(0, match).trim();
            String value = Configuration.unQuote(line.substring( match + 1).trim());

            if(CONFIG_NAME.equalsIgnoreCase(key)) {
                m_name = value;
            } else if(CONFIG_DISABLED.equalsIgnoreCase(key)) {
                m_disabled = getBoolean(value);
            } else if(CONFIG_ALLOWDIALIN.equalsIgnoreCase(key)) {
                m_allowDialin = getBoolean(value);
            } else if(CONFIG_DTLS.equalsIgnoreCase(key)) {
                m_allowDTLS = getBoolean(value);
            } else if(CONFIG_PREFERDTLS.equalsIgnoreCase(key)) {
                m_preferDTLSSRTP = getBoolean(value);
            } else if(CONFIG_PREFERICEHOST.equalsIgnoreCase(key)) {
                m_preferIceHost = getBoolean(value);
            } else if(CONFIG_CONTACTHDRASTARGET.equalsIgnoreCase(key)) {
                m_contactHdrAsTarget = getBoolean(value);
            } else if(CONFIG_ABRAUTO.equalsIgnoreCase(key)) {
                m_abrAuto = getBoolean(value);
            } else if(CONFIG_ABRSELF.equalsIgnoreCase(key)) {
                m_abrSelf= getBoolean(value);
            } else if(CONFIG_INVITECALLEDPARTY.equalsIgnoreCase(key)) {
                m_bInviteCalledParty = getBoolean(value);
            } else if(CONFIG_XCODE.equalsIgnoreCase(key)) {
                m_xcodeConfig = new  XCoderEncoderConfig(value);
            } else if(CONFIG_LAYOUT.equalsIgnoreCase(key)) {
                m_layout = value;
            } else if(CONFIG_TURNRELAYENABLED.equalsIgnoreCase(key)) {
                m_enableTurnRelay = getBoolean(value);
            } else if(CONFIG_TURNSERVER.equalsIgnoreCase(key)) {
                m_turnServer = value;
            } else if(CONFIG_TURNREALM.equalsIgnoreCase(key)) {
                m_turnRealm = value;
            } else if(CONFIG_TURNUSER.equalsIgnoreCase(key)) {
                m_turnUser = value;
            } else if(CONFIG_TURNPASS.equalsIgnoreCase(key)) {
                m_turnPass = value;
            } else if(CONFIG_JOINCHIME.equalsIgnoreCase(key)) {
                m_joinChime = value;
            } else if(CONFIG_LEAVECHIME.equalsIgnoreCase(key)) {
                m_leaveChime = value;
            } else if(CONFIG_EXTRAARGS.equalsIgnoreCase(key)) {
                m_extraArgs = value;
            } else if(CONFIG_MTU.equalsIgnoreCase(key)) {
                m_mtu = Integer.parseInt(value);
            } else if(CONFIG_MAXUSERS.equalsIgnoreCase(key)) {
                m_maxUsers = Integer.parseInt(value);
            } else if(CONFIG_RECORD.equalsIgnoreCase(key)) {
                m_recordingName = value;
            } else if(CONFIG_STREAMOUTPUT.equalsIgnoreCase(key)) {
                m_streamOutput = value;
            } else if(CONFIG_PASSWORD.equalsIgnoreCase(key)) {
                if(null != value && value.length() > 0) {
                    m_pass = value;
                }
            } else if(CONFIG_ALLOWEDUSERS.equalsIgnoreCase(key)) {

                String allowedUsers = value;
                if("*".equals(allowedUsers)) {
                    allowedUsers = null;
                }

                m_arrAllowedUsers = ConfigUtil.parseCSV(allowedUsers);

            } else if(CONFIG_SRTPPOLICY.equalsIgnoreCase(key)) {

                if(SRTP_POLICY.UNKNOWN == (m_srtpPolicy = readSRTPPolicy(value))) {
                    m_log.warn("Conference definition " + m_confFileName + " contains unknown " + 
                                CONFIG_SRTPPOLICY + "=" + value);
                    m_srtpPolicy = SRTP_POLICY_DEFAULT;
                }

            } else if(CONFIG_INVITEUSERS.equalsIgnoreCase(key)) {
                m_arrInviteUsers = ConfigUtil.parseCSV(value);

            } else if(CONFIG_ENCODER.equalsIgnoreCase(key)) {
                m_listEncoders.add(new XCoderEncoderConfig(value));
            } else if(null != key && key.substring(0, CONFIG_ENCODER.length() + 1).equalsIgnoreCase(CONFIG_ENCODER + ".")) {
                m_listEncoders.add(new XCoderEncoderConfig(value, new DeviceType(key.substring(CONFIG_ENCODER.length() + 1))));
            }

        }

        br.close();
    }

    /**
     * Creates a human readable descriptive string for the conference definition
     */
    public String toString() {
        String temp;
        String description;

        description = m_confFilePath;
        description += " (" + getConferenceName() + ")";
        description += ", srtp:" + m_srtpPolicy;
        description += ", dtls:" + m_allowDTLS;
        description += ", preferdtls:" + m_preferDTLSSRTP;
        if(0 != m_mtu) {
            description += ", mtu:" + m_mtu; 
        }
        if(0 != m_maxUsers) {
            description += ", maxUsers:" + m_maxUsers; 
        }
        if(null != m_pass) {
            description += ", using password";
        }
        if(null != m_xcodeConfig) {
            description += ", " + m_xcodeConfig.toString();
        }
        if(null != m_layout) {
            description += ", layout=" + m_layout;
        }
        if(null != m_extraArgs) {
            description += ", args=" + m_extraArgs;
        }
        return description; 
    }

/*
    public static void main(String [] args) {
        try {
            ConferenceDefinition cd = new ConferenceDefinition("../../conferences", "1000");

            System.out.println("validate: " + cd.validateUser("test@test.com"));

        } catch(Exception e) {
            System.out.println("Exception: " + e);
        }

    }
*/

}
