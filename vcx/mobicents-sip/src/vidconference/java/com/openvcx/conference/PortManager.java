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

import java.io.IOException;

/**
 *
 * TCP/UDP port allocation manager
 *
 */
public class PortManager {
    private int m_start;
    private int m_end;
    private int m_last;
    private int m_mod;

    /**
     * Constructor used to initialize this instance
     * @param start The starting port number for the managed range
     * @param end The ending port number for the managed port range
     * @param mod A modulus value for each call to <i>allocatePort</i>.  A value of 2 will cause each call to the <i>allocatePort</i> method to return a port greater than 2 from the previous call.
     * @throws IOException When the port range arguments are invalid
     */
    public PortManager(int start, int end, int mod) throws IOException {

        if((m_mod = mod) <= 0) {
            m_mod = 1;
        }
        m_start = start / m_mod * m_mod;
        m_end = end;
        m_last = start;
        if(m_end - m_start <= 0) {
            throw new IOException("Illegal port range " + m_start + " - " + m_end);
        }
    }

    /**
     * Allocates a port from the port configured port range
     * @return A port allocated from the configured port range
     */ 
    public synchronized int allocatePort() {
        int port = m_last;
        m_last += m_mod;
        if(m_last > m_end) {
          m_last = m_start;
        }
        return port;
    }

}
