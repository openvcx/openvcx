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

import javax.servlet.sip.Address;

/**
 *
 * Definition type of an audio/video WebRTC video endpoint
 *
 */
public class DeviceTypeWebRTC extends DeviceType  {

    /**
     * <p>Application static WebRTC type name identifiying a webrtc endpoint running in Chrome.</p>
     * <p>This type is identified by the string <i>webrtc_chrome</i>
     */
    public static final String DEVICE_TYPE_WEBRTC_CHROME    = "webrtc_chrome";

    /**
     * Application static WebRTC type name identifiying a webrtc endpoint running in firefox 
     * <p>This type is identified by the string <i>webrtc_firefox</i>
     */
    public static final String DEVICE_TYPE_WEBRTC_FIREFOX   = "webrtc_firefox";

    /**
     * Application static WebRTC type name identifiying a webrtc endpoint running in Chrome on Android 
     * <p>This type is identified by the string <i>webrtc_android</i>
     */
    public static final String DEVICE_TYPE_WEBRTC_ANDROID   = "webrtc_android";

    /**
     * Application static WebRTC type name identifiying a generic webrtc endpoint 
     * <p>This type is identified by the string <i>webrtc</i>
     */
    public static final String DEVICE_TYPE_WEBRTC_GENERIC   = "webrtc";

    public static enum Type {

        /**
         * Any generic non-WebRTC type
         */
        UNKNOWN,

        /**
         * WebRTC not otherwise characterized 
         */
        GENERIC,

        /**
         * WebRTC running in chrome 
         */
        CHROME,

        /**
         * WebRTC running in firefox 
         */
        FIREFOX,

        /**
         * WebRTC running on Android 
         */
        ANDROID,
    }

    /**
     * Creates a WebRTC specific type given a known WebRTC specific device type name
     * @param type A known string device type name to be tested.  This string should match one of the application WebRTC type names for the device to be identified as a WebRTC device.
     * @return A WebRTC specific device type
     */
    public static DeviceTypeWebRTC.Type createType(String type) {

        if(null == type) { 
            return Type.UNKNOWN; 
        } else if(type.equalsIgnoreCase(DEVICE_TYPE_WEBRTC_CHROME)) {
            return Type.CHROME; 
        } else if(type.equalsIgnoreCase(DEVICE_TYPE_WEBRTC_FIREFOX)) {
            return Type.FIREFOX; 
        } else if(type.equalsIgnoreCase(DEVICE_TYPE_WEBRTC_ANDROID)) {
            return Type.ANDROID; 
        } else if(type.equalsIgnoreCase(DEVICE_TYPE_WEBRTC_GENERIC)) {
            return Type.GENERIC; 
        } else {
            return Type.UNKNOWN; 
        }
    }

    /**
     * Tests if the given type is a WebRTC endpoint
     * @param type A known string device type name to be tested.  This string should match one of the application WebRTC type names for the device to be identified as a WebRTC device.
     * @return <i>true</i> if the endpoint is WebRTC, <i>false otherwise</i>
     */
    public static boolean isWebRTC(String type) {
        return isWebRTC(createType(type));
    }

    /**
     * Tests if the given type is a WebRTC endpoint
     * @param type the enumeration device type to be tested
     * @return <i>true</i> if the endpoint is WebRTC, <i>false otherwise</i>
     */
    public static boolean isWebRTC(Type type) {
        switch(type) {
            case CHROME:
            case FIREFOX:
            case ANDROID:
            case GENERIC:
                return true;
            default:
                return false;
        }
    }

}
