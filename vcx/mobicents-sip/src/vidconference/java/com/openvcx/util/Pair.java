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

import org.apache.log4j.Logger;

/**
 *
 * A key value pairing
 *
 */
public class Pair<K, V> {

    private final Logger m_log = Logger.getLogger(getClass());

    protected K m_key;
    protected V m_value;

    /**
     * Default constructor 
     */
    public Pair() {
    }

    /**
     * Constructor used to initialize this instance
     * @param key The element key
     * @param value The element value 
     */
    public Pair(K key, V value) {
        m_key = key;
        m_value = value;
    }

    /**
     * Retrieves the element key
     * @return The element key
     */
    public K getKey() {
        return m_key;
    }

    /**
     * Sets the element key
     * @param key The element key
     */
    public void setKey(K key) {
        m_key = key;
    }

    /**
     * Retrieves the element value
     * @return The element value 
     */
    public V getValue() {
        return m_value;
    }

    /**
     * Sets the element value
     * @param value The element value
     */
    public void setValue(V value) {
        m_value = value;
    }

}
