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

package com.openvcx.util;

import java.io.IOException;
import java.io.File;
import java.io.FileInputStream;
import java.lang.StringBuilder;
import java.util.Properties;

import org.apache.log4j.Logger;

/**
 * A general utility class
 *
 */
public class ConfigUtil {

    private final Logger m_log = Logger.getLogger(getClass());

    /**
     * Parse a comma separated value string into an array
     * @param csv A comma separated list of values
     * @return The list of values as an array
     */
    public static String [] parseCSV(String csv) {
        String [] arr = null;

        if(null != csv) {
            int index;
            arr = csv.split(",");
            if(null == arr || arr.length <= 0) {
                return null;
            }
            for(index = 0; index < arr.length; index++) {
                arr[index] = Configuration.unQuote(arr[index].trim());
            }
        }

        return arr;
    }

    /**
      * <p>Extract the SIP username part of a SIP URI </p>
      * <p>If the sipURI is</i>username@domain.com</i></p>
      * <p>The username part would be <i>username</i></p>
      * @param sipURI A SIP URI 
      * @return The SIP username part of the SIP URI
      */
    public static String getSIPUsernamePart(String sipURI) {

        if(null != sipURI) {
            int match = sipURI.indexOf('@');
            if(match >= 0) {
                return sipURI.substring(0, match);
            }
        }
        return sipURI;
    }

    /**
      * <p>Extract the SIP domain part of a SIP URI </p>
      * <p>If the sipURI is </i>username@domain.com</i></p>
      * <p>The domain part would be <i>domain</i></p>
      * @param sipURI A SIP URI 
      * @return The SIP domain part of the SIP URI
      */
    public static String getSIPDomainPart(String sipURI) {

        if(null != sipURI) {
            //
            // sipURI could be 'username@domain.com'
            // username would be 'domain.com'
            //
            int match = sipURI.indexOf('@');
            if(match > 0) {
                return sipURI.substring(match + 1);
            }
        }
        return null;
    }

    /**
      * <p>Tests a SIP URI to see if it matches a specified pattern</p>
      * <p>For example if:</p>
      * <p><i>uriToTest</i> is <i>user1@mydomain.com</i> and <i>uriAllowed</i> is <i>*@mydomain.com</i>  then the pattern tested will match the validation pattern</p>
      * @param uriToTest A SIP URI string pattern to be validated
      * @param uriAllowed A SIP URI string pattern which is permitted 
      * @return true if the pattern <i>uriToTest</i> matches the pattern uriToTest
      */
    public static boolean isSipURIMatchFilter(String uriToTest, String uriAllowed) {

        String userName = getSIPUsernamePart(uriToTest);
        String userDomain = getSIPDomainPart(uriToTest);

        String allowedUserName = getSIPUsernamePart(uriAllowed);
        String allowedDomain = getSIPDomainPart(uriAllowed);

        if(allowedDomain != null && userDomain != null &&
           (false == "*".equals(allowedDomain) && false == allowedDomain.equalsIgnoreCase(userDomain))) {
            return false;
        } else if(allowedUserName != null && userName != null && (allowedUserName.length() == 0 ||
               "*".equals(allowedUserName) || allowedUserName.equals(userName))) {
            return true;
        }

        return false;
    }
    

}
