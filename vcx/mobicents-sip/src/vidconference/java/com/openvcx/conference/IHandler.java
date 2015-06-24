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

import javax.servlet.sip.SipServletRequest;
import javax.servlet.sip.SipServletResponse;
import javax.servlet.ServletException;


/**
 *
 * Interface to a message signalling handler which will provide media services
 *
 */
public interface IHandler {

    /**
     * SIP BYE message handler
     * @param req The SIP request message
     */
    public void onBye(SipServletRequest req) throws ServletException, IOException;

    /**
     * SIP INVITE message handler
     * @param req The SIP request message
     */
    public void onInvite(SipServletRequest req) throws ServletException, IOException;

    /**
     * SIP REGISTER message handler
     * @param req The SIP request message
     */
    public void onRegister(SipServletRequest req) throws ServletException, IOException;

    /**
     * SIP 2xx succesful message handler
     * @param resp The SIP response message
     */
    public void onSuccessResponse(SipServletResponse resp) throws ServletException, IOException;

    /**
     * SIP 4xx error message handler
     * @param resp The SIP response message
     */
    public void onErrorResponse(SipServletResponse resp) throws ServletException, IOException;


}
