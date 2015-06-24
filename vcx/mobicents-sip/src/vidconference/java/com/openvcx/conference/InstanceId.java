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

import java.text.DateFormat;
import java.text.SimpleDateFormat;
import java.util.Calendar;
import java.util.Date;

/**
 *
 * <p>A unique instance id used to distinguish each conference.</p>
 * <p>The instance id consists of a user supplied name and a timestamp</p>
 *
 */
public class InstanceId {
  
    private String m_name;
    private Date m_date;
    private String m_id;
    
    private InstanceId(String name) {
        m_name = name;
        m_date = Calendar.getInstance().getTime();
        m_id = safeString(name) + "_" + new SimpleDateFormat("EEE-MMM-dd_HH:mm:ss").format(m_date);
    }

    /**
     * Factory method to create an instance id
     * @param name A name used to identify the instance id
     */
    public static InstanceId create(String name) {
        return new InstanceId(name);
    }

    /**
     * Retrieves the name used to identify an instanc
     * @return The name used to identify the instance id
     */
    public String getName() {
        return m_name;
    }

    /**
     * Filters out any non-printable characters from the input string
     */
    private static String safeString(String input) {
        char[] allowed = "0123456789abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ".toCharArray();
        char[] charArray = input.toString().toCharArray();
        StringBuilder result = new StringBuilder();
        for (char c : charArray) {
            for (char a : allowed) {
                if(c==a) {
                    result.append(a);
                }
            }
        }
        return result.toString();
    }

    /**
     * Obtains a string representation of this instance id
     * @return A string representation of this instance id
     */
    public String toString() {
        return m_id;
    }

}
