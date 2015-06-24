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

import java.net.UnknownHostException;
import java.util.ArrayList;

import javax.servlet.sip.SipFactory;

import org.apache.log4j.Logger;

/**
 *
 * A master context store for maintaining the many instances required to provide conferencing services
 *
 */
public class HandlerContext {

    private final Logger m_log = Logger.getLogger(getClass());

    private SipFactory m_sipFactory = null;
    private ConferenceInviter m_inviter = null;
    private SDPResponder m_conferenceSdpResponder = null;
    private ConferenceDefinitionLoader m_conferenceDefinitionLoader = null;
    private DeviceTypeMappingTable m_deviceTypeMappingTable = null;
    private Configuration m_config = null;
    private XCoderMediaProcessorConfig m_mediaConfig = null;
    private RegisterDB m_registerDB = null;
    private B2BClient m_b2bClient = null;
    private boolean m_bHandleRegister = true;
    private NetIPv4Mask [] m_permittedIps = null;

    //private static final String CONFIG_XCODER_PORT_START           = "vsx.control.tcp.start";
    //private static final String CONFIG_XCODER_PORT_START           = XCoderMediaProcessorConfigImpl.CONFIG_XCODER_PORT_START;
    //private static final String CONFIG_XCODER_PORT_END             = "vsx.control.tcp.end";
    //private static final String CONFIG_XCODER_PORT_END             = XCoderMediaProcessorConfigImpl.CONFIG_XCODER_PORT_END;
    private static final String CONFIG_RTP_PORT_START            = "rtp.port.start";
    private static final String CONFIG_RTP_PORT_END              = "rtp.port.end";
    private static final String CONFIG_SIP_REGISTER_ACCEPT       = "sip.register.accept";
    private static final String CONFIG_SIP_REGISTER_LOCAL_DB     = "sip.register.local.db";
    private static final String CONFIG_SIP_ADDRESS_LIST_SOURCE   = "sip.address.list.source";

    /**
     * Constructor used to initialize this instance
     * @param configFilePath The master configuration file path which will be used to initialize this application
     */
    public HandlerContext(String configFilePath) throws Exception {
        init(configFilePath);
    }

    /**
     * Constructor used to initialize this instance
     * @param sipFactory An optional <i>SipFactory</i> instance used for creating B2B invitations
     * @param configFilePath The master configuration file path which will be used to initialize this application
     * @throws Exception on an error condition
     */
    public HandlerContext(SipFactory sipFactory, String configFilePath) throws Exception {

        m_sipFactory = sipFactory;

        init(configFilePath);
    }

    private void init(String configFilePath) throws Exception {

        //
        // Load the config file defined in sip.xml 'sip-ngconference.conf'
        //
        m_config = new Configuration(configFilePath);

        //
        // Initialize the media processor resource pool
        // 
        m_mediaConfig = new XCoderMediaProcessorConfig(m_config);

        //TODO: get from config
        if(true) {
            m_inviter = new ConferenceInviter(this);
        }

        m_conferenceSdpResponder = new SDPResponder(m_mediaConfig.getSDPNamingImpl(),
                                                    m_config);

        m_conferenceDefinitionLoader = new ConferenceDefinitionLoader(m_config);

        m_deviceTypeMappingTable = new DeviceTypeMappingTable(m_config);;

        //
        // Setting to determine if we will handle SIP REGISTER
        //
        m_bHandleRegister = m_config.getBoolean(CONFIG_SIP_REGISTER_ACCEPT, true);

        //
        // Setting to determine if we will maintain a database of locally registered SIP URIs
        //
        if(true == m_config.getBoolean(CONFIG_SIP_REGISTER_LOCAL_DB, true)) {
            m_registerDB = new RegisterDB();
        }

        String registerDescription = CONFIG_SIP_REGISTER_ACCEPT + "=" + m_bHandleRegister + ".  Server will " +
                                    (false == m_bHandleRegister ? "not " : "") + "handle SIP REGISTER";
        if(m_bHandleRegister) {
            registerDescription += ", " + CONFIG_SIP_REGISTER_LOCAL_DB + "=" + 
                                   (null == m_registerDB ? "false" : "true");
        }
        m_log.info(registerDescription);

        if(null != m_sipFactory) {
            m_b2bClient = new B2BClient(m_sipFactory);
        }

        initPermittedSipAddressListSource();

    }

    /**
     * Obtains the <i>ConferenceManager</i> instance 
     * @return The <i>ConferenceManager</i> instance 
     */
    public ConferenceManager getConferenceManager() {
        return m_mediaConfig.getConferenceManager();
    }

    /**
     * Obtains the <i>SipFactory</i> instance which was used to initialize the context
     * @return The <i>SipFactory</i> instance 
     */
    public SipFactory getSipFactory() {
        return m_sipFactory;
    }

    /**
     * Obtains the <i>ConferenceInviter</i> instance 
     * @return The <i>ConferenceInviter</i> instance 
     */
    public ConferenceInviter getConferenceInviter() {
        return m_inviter;
    }

    /**
     * Obtains the <i>SDPResponder</i> instance 
     * @return The <i>SDPResponder</i> instance 
     */
    public SDPResponder getConferenceSdpResponder() {
        return m_conferenceSdpResponder;
    }

    /**
     * Obtains the underlying <i>IMediaProcessor</i> instance 
     * @return The underlying <i>IMediaProcessor</i> instance 
     */
/*
    public IMediaProcessor getMediaProcessor() {
        return m_mediaConfig.getMediaProcessor();
    }
*/

    /**
     * Obtains the <i>PortManager</i> instance used for assigning the control port to the underlying media processor
     * @return The <i>PortManager</i> instance 
     */
    public PortManager getPortManagerControl() {
        return m_mediaConfig.getPortManagerControl();
    }

    /**
     * Obtains the <i>PortManager</i> instance used for assigning the media streaming port(s)
     * @return The <i>PortManager</i> instance 
     */
    public PortManager getPortManagerRtp() {
        return m_mediaConfig.getPortManagerRtp();
    }

    /**
     * Obtains the <i>ConferenceDefinitionLoader</i> instance 
     * @return The <i>ConferenceDefinitionLoader</i> instance 
     */
    public ConferenceDefinitionLoader getConferenceDefinitionLoader() {
        return m_conferenceDefinitionLoader;
    }

    /**
     * Obtains the <i>DeviceTypeMappingTable</i> instance 
     * @return The <i>DeviceTypeMappingTable</i> instance 
     */
    public DeviceTypeMappingTable getDeviceTypeMappingTable() {
        return m_deviceTypeMappingTable;
    }

    /**
     * Obtains the <i>Configuration</i> instance 
     * @return The <i>Configuration</i> instance 
     */
    public final Configuration getConfig() {
        return m_config;
    }

    /**
     * Obtains the <i>RegisterDB</i> instance 
     * @return The <i>RegisterDB</i> instance 
     */
    public RegisterDB getRegisterDB() {
        return m_registerDB;
    }

    /**
     * Obtains the <i>B2BClient</i> instance 
     * @return The <i>B2BClient</i> instance 
     */
    public B2BClient getB2BClient() {
        return m_b2bClient;
    }

    /**
     * <p>Tests if this instance is configured to handle SIP REGISTER commands.</p>
     * <p>This setting is configured using the configuration file <i>sip.register.accept</i> parameter.</p>
     * @return <i>true</i> or <i>false</i>
     */
    public boolean isHandleRegister() {
        return m_bHandleRegister;
    }

    /**
     * <p>Retrieves the list of trusted sources from which this instance is configured to receive SIP messages from.</p>
     * <p>This setting is configured using the configuration file <i>sip.address.list.source</i> parameter.</p>
     * @return An array of IP address network masks 
     */
    public final NetIPv4Mask [] getPermittedIps() {
        return m_permittedIps;
    }

    private void initPermittedSipAddressListSource() throws UnknownHostException {
        String permittedIpStr = m_config.getString(CONFIG_SIP_ADDRESS_LIST_SOURCE, null);
        if(null != permittedIpStr && permittedIpStr.length() > 0) {

            String [] arr = permittedIpStr.split(",");
            ArrayList<NetIPv4Mask> permittedIps = new ArrayList<NetIPv4Mask>(arr.length);

            for(int index = 0; index < arr.length; index++) {
                String strIp = arr[index].trim();
                try {
                    if(strIp.length() > 0) {
                        permittedIps.add(NetIPv4Mask.parse(strIp));
                        m_log.debug("Permitted SIP source: " + strIp);
                    }
                } catch(UnknownHostException e) {
                    m_log.error("Invalid " + CONFIG_SIP_ADDRESS_LIST_SOURCE + " value: " + strIp);
                    throw e;
                }
            }
            m_permittedIps = permittedIps.toArray(new NetIPv4Mask[0]);
        }
    }



}

