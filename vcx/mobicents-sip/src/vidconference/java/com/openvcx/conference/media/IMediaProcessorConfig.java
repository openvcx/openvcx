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

/**
 *
 * <p>Interface to a media processor resource configuration</p>
 */
public interface IMediaProcessorConfig {

    public String getSDPConnectAddress();

    public String getDtlsCertPath();

    public String getDtlsPrivKeyPath();

    public String getHomeDir();

    public String getStartScript();

    public String getTmpDir();

    public String getRecordDir();

    public String getControlServerAddress();

    public int getRtpStartPort();

    public int getRtpEndPort();

    public int getControlServerStartPort();

    public int getControlServerEndPort();


} 
