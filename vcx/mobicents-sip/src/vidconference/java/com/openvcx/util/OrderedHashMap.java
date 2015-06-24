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
import java.util.Iterator;
import java.util.ArrayList;
import java.util.List;
import java.util.Map;
import java.util.HashMap;

import org.apache.log4j.Logger;

/**
 *
 * A hashmap maintaining insertion ordering
 *
 */
public class OrderedHashMap<K, V> extends HashMap<K, V> {

    private final Logger m_log = Logger.getLogger(getClass());

    private List<V> m_list = new ArrayList<V>();

    /**
     * Returns the iterator used for iterating the map values in the same order that they were inserted
     * @return map iterator
     */
    public Iterator<V> getOrderedIterator() {
        return m_list.iterator();
    }

    /**
     * Returns the size of the map
     * @return The number of values stored in the map
     */
    public int getSize() {
        return m_list.size();
    }

    /**
     * Inserts an element identified by the unique key
     * @param key The unique element key
     * @param value The element value 
     * @return The element value which was inserted
     */
    @Override
    public V put(K key, V value) {
        V result = super.put(key, value);
        if(null == result) {
            m_list.add(value);
        } else {
            int index = m_list.lastIndexOf(result);
            if(-1 != index) {
                m_log.warn("Duplicate type " + key + " replacing " + result);
                m_list.remove(result);
                m_list.add(index, value);
            }
        }
        return result;
    }

    /**
     * Removes a value element identified by the unique key
     * @param key The unique element key to be removed
     * @return The element value 
     */
    @Override
    public V remove(Object key) {
        V result = super.remove(key);

        if(null != result) {
            int index = m_list.lastIndexOf(result);
            if(-1 != index) {
                m_list.remove(result);
            }
        }

        return result;
    }

    /**
     * Retrieves the element value identified by the key
     * @param key The unique element key to be retrieved  
     * @return The element value or <i>null</i> if it is not found
     */
    @Override
    public V get(Object key) {
        V result = super.get(key);
        return result;
    }

    /**
     * Clears all elements from the map
     */
    @Override
    public void clear() {
        super.clear();
        m_list.clear();
    }

}
