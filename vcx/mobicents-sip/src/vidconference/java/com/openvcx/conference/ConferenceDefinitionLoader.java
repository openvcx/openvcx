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

import java.io.FileNotFoundException;
import java.io.IOException;
import java.lang.SecurityException;

import org.apache.log4j.Logger;

/**
 *
 * A loader for looking up a pre-defined ConferenceDefinition instance
 *
 */
public class ConferenceDefinitionLoader {

    private final Logger m_log = Logger.getLogger(getClass());

    private static final String CONFIG_CONFERENCE_DEF   = "conference.definition.dir";
    private static final String CONFERNCE_CONFIG_DIR    = "conferences/";

    private String m_confDefDir;
    private Configuration m_config;

    ConferenceDefinitionLoader(Configuration config) {
        m_config = config;
        m_confDefDir = m_config.getString(CONFIG_CONFERENCE_DEF, CONFERNCE_CONFIG_DIR);

        m_log.info("Conference definition directory: " + m_confDefDir);
    }

    /**
     * <p>Loads and initializes a conference definition instance from a conference definition file</p>
     * @param callerName The SIP calling party name
     * @param calledName The SIP called party name
     * @return The initialized conference definition instance
     * @throws FileNotFoundException If a matching conference definition file was not found
     * @throws IOException If an error was encountered when loading the conference definition
     * @throws SecurityException If the user is not allowed to join the conference or the conference is disabled.
     */
    public ConferenceDefinition load(String callerName, String calledName) 
               throws FileNotFoundException, IOException, SecurityException {

        ConferenceDefinition confConfig = new ConferenceDefinition(m_confDefDir, callerName, calledName);
        
        if(confConfig.isDisabled()) {
            throw new SecurityException("Conference configuration: " + calledName + " is currently disabled.");
        }

        if(null != confConfig && null != callerName && false == confConfig.validateUser(callerName)) {
            throw new SecurityException("User: " + callerName + " not explicitly allowed to join conference: " + 
                                        calledName); 
        }
       
        return confConfig;
    }
}
