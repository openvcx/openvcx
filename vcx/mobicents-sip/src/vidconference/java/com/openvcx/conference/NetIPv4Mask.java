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

import java.net.InetAddress;
import java.net.Inet4Address;
import java.net.UnknownHostException;
import java.util.regex.Pattern;


/**
 * Class for performing IPv4 addresses and network mask validation
 */
public class NetIPv4Mask {

    private Inet4Address m_addr;
    private byte m_binMask;
    private int m_iAddr;
    private int m_iMask;

    private static final int BITS = 32;

    static private final String IPV4_REGEX = 
              "(([0-1]?[0-9]{1,2}\\.)|(2[0-4][0-9]\\.)|(25[0-5]\\.)){3}(([0-1]?[0-9]{1,2})|(2[0-4][0-9])|(25[0-5]))";
    static private Pattern IPV4_PATTERN = Pattern.compile(IPV4_REGEX);

    /**
     * IPv4 network mask constructor
     * @param strAddress An IPv4 network address represented as a string
     * @param network address mask bits
     */
    private  NetIPv4Mask(String strAddr, byte mask) throws UnknownHostException  {

        if(false == IPV4_PATTERN.matcher(strAddr).matches()) {
            throw new UnknownHostException("Invalid IP:" + strAddr);
        }
        //for(int index = 0; index < strAddr.length; index++) {
        //    strAddr.charAt(index)
        //} 
        set((Inet4Address) InetAddress.getByName(strAddr), mask);
    }
       
    /**
     * IPv4 network mask constructor
     * @param strAddress An IPv4 network address 
     * @param network address mask bits
     */
    private NetIPv4Mask(Inet4Address addr, byte mask) {
        set(addr, mask);
    }
/*
    public NetIPv4Mask() throws UnknownHostException  {
        set((Inet4Address) InetAddress.getByName("0.0.0.0"), (byte) 0);
    }
*/

    /**
     * Parses a string ip address and mask combination
     * @param strIpMask An IPv4 address and mask delimeted by a <i>/</i> such as <i>239.0.0.0/24</i>
     */
    public static NetIPv4Mask parse(String strIpMask) throws UnknownHostException {
        String addr;
        int iMask;
        int pos;

        if(-1 == (pos = strIpMask.indexOf('/'))) {
            addr = strIpMask;
            iMask = BITS;
        } else { 
            addr = strIpMask.substring(0, pos);
            if((iMask = Math.min(Integer.parseInt(strIpMask.substring(pos + 1)), BITS)) < 0) {
                iMask = 0;
            } 
        }
        return new NetIPv4Mask(addr, (byte) iMask);
    }
 
    /**
     * Validates an IPv4 address matching the network address and network address mask
     * @param strIpAddr An IPv4 network address represented as a string which will be validated 
     * @param strIpMask The </i>address/mask</i> which <i>strIp</i> s validated against.
     */
    public static boolean isMatch(String strIpAddr, String strIpMask) throws UnknownHostException {
        NetIPv4Mask ipv4Mask = parse(strIpMask);
        return ipv4Mask.isMatch(strIpAddr);
    }

    private void set(Inet4Address addr, byte mask) {
        m_addr = addr;
        m_binMask = mask;
        m_iAddr = addr2Int(m_addr);
        m_iMask = ~((1 << (32 - m_binMask)) - 1);
    }

    /**
     * Validates an IPv4 address matching the network address and network address mask
     * @param addr An IPv4 network address which will be validated 
     */
    public boolean isMatch(Inet4Address addr) {
        if(0 == m_binMask) {
            return true;
        }
        int iAddr = addr2Int(addr);    
        return ((m_iAddr & m_iMask) == (iAddr & m_iMask));
    }

    /**
     * Validates an IPv4 address matching the network address and network address mask
     * @param strIpAddr An IPv4 network address represented as a string which will be validated 
     */
    public boolean isMatch(String strIpAddr) throws UnknownHostException {
         return isMatch((Inet4Address) InetAddress.getByName(strIpAddr));
    }

    private static int addr2Int(Inet4Address addr) {
         byte[] b = addr.getAddress();  
        return (b[0] << 24) | ((b[1] & 0xff) << 16) | ((b[2] & 0xff) << 8) | (b[3] & 0xff);
    }

    public String toString() {
        return m_addr.getHostAddress() + "/" + m_binMask;
    }

/*
    public static void main(String [] args) {
        try {
            NetIPv4Mask ipv4 = NetIPv4Mask.parse("10.0.2.3/8");
            System.out.println("ipv4:" + ipv4);
            System.out.println("match:" + ipv4.isMatch("11.0.2.20"));
            System.out.println("match:" + NetIPv4Mask.isMatch("10.0.2.20", "10.0.2.3/24"));

        } catch(Exception e) {
           System.out.println(e);
        }

    } 
*/

}
