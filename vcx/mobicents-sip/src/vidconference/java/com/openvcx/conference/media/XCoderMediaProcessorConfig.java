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
import com.openvcx.util.*;

import java.io.IOException;
import java.util.Iterator;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;

import org.apache.log4j.Logger;

/**
 *
 * Class for system resource management
 *
 */
public class XCoderMediaProcessorConfig extends XCoderMediaProcessorConfigImpl {

    private final Logger m_log = Logger.getLogger(getClass());

    private XCoderMediaProcessor m_mediaProcessor = null;
    private PortManager m_portManagerControl = null;
    private PortManager m_portManagerRtp = null;
    private ConferenceManager m_conferenceManager = null;

    public XCoderMediaProcessorConfig(Configuration config) throws Exception {
        super(config); 
        init(config);
    }

    public XCoderMediaProcessorConfig(Configuration config, int index, IMediaProcessorConfig defaultElement) throws Exception {
        super(config, index, defaultElement); 
        init(config);
    }

    private void init(Configuration config) throws Exception {

        m_mediaProcessor = new XCoderMediaProcessor(config);

        m_portManagerControl = new PortManager(m_controlStartPort, m_controlEndPort, 3);

        m_portManagerRtp = new PortManager(m_rtpStartPort, m_rtpEndPort, 2);

        m_conferenceManager = new ConferenceManager(m_mediaProcessor, m_portManagerControl, true, config);
    }

    public String toString() {
        return super.toString();
    }

    /**
     * Obtains the underlying <i>IMediaProcessor</i> instance 
     * @return The underlying <i>IMediaProcessor</i> instance 
     */
    public IMediaProcessor getMediaProcessor() {
        return m_mediaProcessor;
    }

    /**
     * Obtains the underlying <i>ISDPNaming</i> instance 
     * @return The underlying <i>ISDPNaming</i> instance 
     */  
    public ISDPNaming getSDPNamingImpl() {
        return m_mediaProcessor;
    }

    /**
     * Obtains the <i>PortManager</i> instance used for assigning the control port to the underlying media processor
     * @return The <i>PortManager</i> instance 
     */
    public PortManager getPortManagerControl() {
        return m_portManagerControl;
    }

    /**
     * Obtains the <i>PortManager</i> instance used for assigning the media streaming port(s)
     * @return The <i>PortManager</i> instance 
     */
    public PortManager getPortManagerRtp() {
        return m_portManagerRtp;
    }

    /**
     * Obtains the <i>ConferenceManager</i> instance 
     * @return The <i>ConferenceManager</i> instance 
     */
    public ConferenceManager getConferenceManager() {
        return m_conferenceManager;
    }

};
