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

/**
 *
 * Interface to SDP naming normalization between SDP defined naming formats and any media processor
 * implementation specific name mappings.
 *
 */
public interface ISDPNaming {

    /**
     * Retrieves the name of a codec as used by standard SDP naming convention
     * @param codecName The underlying implementation's representation of the codec name
     * @return The name of the codec using standard SDP naming convention according to the published RFC or IETF specification of the codec.
     */
    public String getSDPCodecName(String codecName);

    /**
     * Retrieves the name of a codec as used by the underlying implementation 
     * @param codecName The name of a codec using standard SDP naming convention according to the published RFC or IETF specification of the codec.
     * @return The underlying implementation's representation of the codec name
     */
    public String getImplCodecName(String codecName);

}
