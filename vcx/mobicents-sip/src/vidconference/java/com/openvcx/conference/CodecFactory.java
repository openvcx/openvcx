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
 * <p>Factory class used to instantiate a <i>Codec</i> instance</p>
 *
 */
public class CodecFactory {

    /**
     * <p>Creates a class inherited from <i>Codec</i> according to the given name</p>
     * <p>The created class may be of type <i>Codec</i> if the codec implementation can be fulfilled by the generic class</p>
     * @param name The codec name
     * @return A codec instance
     */
    public static Codec create(String name) {
        if("OPUS".equalsIgnoreCase(name)) {
            return new CodecOpus(name);
        } else {
            return new Codec(name);
        }
    }

    /**
     * <p>Creates a class inherited from <i>Codec</i> according to the given name</p>
     * @param name The codec name
     * @param payloadType The RTP payload type identifier of the codec
     * @return A codec instance
     */
    public static Codec create(String name, int payloadType) {
        Codec codec = create(name);
        codec.setPayloadType(payloadType);
        return codec;
    }

    /**
     * <p>Creates a class inherited from <i>Codec</i> according to the given name</p>
     * @param name The codec name
     * @param clockRate The media clock rate in Hz
     * @param payloadType The RTP payload type identifier of the codec
     * @return A codec instance
     */
    public static Codec create(String name, int clockRate, int payloadType) {
        Codec codec = create(name, payloadType);
        codec.setClockRate(clockRate);
        return codec;
    }

    /**
     * <p>Creates a class inherited from <i>Codec</i> according to the given name</p>
     * @param name The codec name
     * @param clockRate The media clock rate in Hz
     * @param payloadType The RTP payload type identifier of the codec
     * @param fmtp The SDP <i>fmtp</i> attribute containing configuration parameters
     * @return A codec instance
     */
    public static Codec create(String name, int clockRate, int payloadType, String fmtp) {
        Codec codec = create(name, clockRate, payloadType);
        codec.setFmtp(fmtp);
        return codec;
    }

}
