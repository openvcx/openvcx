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

package com.openvcx.test;

import com.openvcx.conference.*;
import com.openvcx.util.*;

public class SDPTest {
/*
    public static final String sdpAudio = new String("v=0\r\n" +
"v=0\r\n" +
"o=test1 3540 3859 IN IP4 10.0.2.3\r\n" +
"s=Talk\r\n" +
"c=IN IP4 10.0.2.3\r\n" +
"b=AS:380\r\n" +
"t=0 0\r\n" +
"m=audio 7076 RTP/AVP 111 110 100 3 0 8 120 9 101\r\n" +
"a=rtpmap:111 speex/16000\r\n" +
"a=fmtp:111 vbr=on\r\n" +
"m=video 5000 RTP/AVP 103\r\n" +
"a=rtpmap:103 VP8/90000\r\n" +
"a=inactive\r\n");

    public static final String sdpMagor = new String("v=0\r\n" +
"o=Magor 1377085655 1377085657 IN IP4 216.13.45.141\r\n"+
"s=Magor\r\n"+
"c=IN IP4 216.13.45.141\r\n"+
"b=TIAS:2048000\r\n"+
"b=AS:2048\r\n"+
"b=CT:2048\r\n"+
"t=0 0\r\n"+
"m=audio 24628 RTP/AVP 115 107 9 6 70 0 8 101\r\n"+
"a=rtpmap:115 G7221/32000\r\n"+
"a=fmtp:115 bitrate=48000\r\n"+
"a=rtpmap:107 G7221/16000\r\n"+
"a=fmtp:107 bitrate=32000\r\n"+
"a=rtpmap:9 G722/8000\r\n"+
"a=rtpmap:6 DVI4/16000\r\n"+
"a=rtpmap:70 L16/8000\r\n"+
"a=rtpmap:0 PCMU/8000\r\n"+
"a=rtpmap:8 PCMA/8000\r\n"+
"a=rtpmap:101 telephone-event/8000\r\n"+
"a=fmtp:101 0-16\r\n"+
"a=silenceSupp:off - - - -\r\n"+
"a=ptime:20\r\n"+
"m=video 24632 RTP/AVP 96 97 34\r\n"+
"b=TIAS:2048000\r\n"+
"a=rtpmap:96 H264/90000\r\n"+
"a=fmtp:96 profile-level-id=42e016; max-fs=8192; max-mbps=244800\r\n"+
"a=rtpmap:97 H264/90000\r\n"+
"a=fmtp:97 profile-level-id=64e016; max-fs=8192; max-mbps=244800\r\n"+
"a=rtpmap:34 H263/90000\r\n"+
"a=fmtp:34 CIF4=2;4CIF=2;CIF=1;QCIF=1\r\n"+
"a=content:main^Ma=rtcp-fb:* ccm fir\r\n"+
"a=rtcp-fb:* ccm tmmbr\r\n"+
"a=rtcp-fb:* nack pli\r\n"+
"m=video 24636 RTP/AVP 96 97 34\r\n"+
"b=TIAS:2148000\r\n"+
"a=rtpmap:96 H264/90000\r\n"+
"a=fmtp:96 profile-level-id=42e016; max-fs=8192; max-mbps=244800\r\n"+
"a=rtpmap:97 H264/90000\r\n"+
"a=fmtp:97 profile-level-id=64e016; max-fs=8192; max-mbps=244800\r\n"+
"a=rtpmap:34 H263/90000\r\n"+
"a=fmtp:34 CIF4=2;4CIF=2;CIF=1;QCIF=1\r\n"+
"a=content:slides\r\n"+
"a=rtcp-fb:* ccm fir\r\n"+
"a=rtcp-fb:* ccm tmmbr\r\n"+
"a=rtcp-fb:* nack pli\r\n"+
"m=application 24640 UDP/BFCP *\r\n"+
"a=floorctrl:c-only\r\n"+
"a=setup:passive\r\n"+
"a=connection:new\r\n");
/*
    public static final String sdpAll = new String("v=0\r\n" +
"o=- 1376101994 1376101994 IN IP4 10.0.2.1\r\n" +
"s=conference\r\n" +
"c=IN IP4 10.0.2.1\r\n" +
"t=0 0\r\n" +
"a=group:BUNDLE audio video\r\n" +
"m=audio 10202 RTP/SAVPF 0 8 107 109 113 112 111 110\r\n" +
"a=candidate:1474096815 1 udp 2113994751 10.0.2.1 10202 typ host\r\n" +
"a=candidate:1474096815 2 udp 2113994751 10.0.2.1 10202 typ host\r\n" +
"a=ice-lite\r\n" +
"a=ice-ufrag:Lwksz+dcY9UOr1N7\r\n" +
"a=ice-pwd:54GeTBAchfdNuYCbsiv2rGiE\r\n" +
"a=rtpmap:0 PCMU/8000\r\n" +
"a=rtpmap:8 PCMA/8000\r\n" +
"a=rtpmap:107 SILK/8000\r\n" +
"a=rtpmap:109 SILK/16000\r\n" +
"a=rtpmap:113 OPUS/8000\r\n" +
"a=rtpmap:112 OPUS/16000\r\n" +
"a=rtpmap:111 OPUS/48000\r\n" +
"a=rtpmap:110 OPUS/48000/2\r\n" +
"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:pY0Gff78GmCQtTjjsQVd+HhYmkuOhMhotGBhHgkH\r\n" +
"a=rtcp-mux\r\n" +
"a=mid:audio\r\n" +
"m=video 10204 RTP/SAVPF 34 115 121 97 102\r\n" +
"a=candidate:1474096815 1 udp 2113994751 10.0.2.1 10242 typ host\r\n" +
"a=candidate:1474096815 2 udp 2113994751 10.0.2.1 10243 typ host\r\n" +
"a=ice-lite\r\n" +
"a=ice-ufrag:Lwksz+dcY9UOr1N7\r\n" +
"a=ice-pwd:54GeTBAchfdNuYCbsiv2rGiE\r\n" +
"a=rtpmap:34 H263/90000\r\n" +
"a=fmtp:34 QCIF=2;CIF=2;VGA=2;CIF4=2\r\n" +
"a=rtpmap:102 VP8/90000\r\n" +
"a=rtpmap:97 H264/90000\r\n" +
"a=rtpmap:121 MP4V-ES/90000\r\n" +
"a=rtpmap:115 H263-1998/90000\r\n" +
"a=fmtp:115 QCIF=2;CIF=2;VGA=2;CIF4=2;I=1;J=1;T=1\r\n" +
"a=fmtp:97 profile-level-id=42000d;packetization-mode=0;\r\n" +
"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:qNKCJp35xn3jaGKtuytvrPVQ1Xd0mQusxOxrzS0/\r\n" +
"a=rtcp-mux\r\n" +
"a=rtcp-fb:102 ccm fir\r\n" +
"a=rtcp-fb:102 nack\r\n" +
"a=mid:video\r\n");

    public static final String sdpResp2All= new String("v=0\r\n" +
"o=- 1807461654593550908 2 IN IP4 127.0.0.1\r\n" +
"s=-\r\n" +
"t=0 0\r\n" +
"a=group:BUNDLE audio video\r\n" +
"a=msid-semantic: WMS iP3E1Fzw7KYJlA41z0dEEIXCzsN3BHg4EKZ6\r\n" +
"m=audio 49400 RTP/SAVPF 110 0 8\r\n" +
"c=IN IP4 192.168.100.106\r\n" +
"a=rtcp:1 IN IP4 0.0.0.0\r\n" +
"a=candidate:2530088836 1 udp 2113937151 192.168.1.106 49400 typ host generation 0\r\n" +
"a=candidate:3355351182 1 udp 2113937151 10.0.2.1 58186 typ host generation 0\r\n" +
"a=candidate:1316314130 1 udp 2113937151 169.254.149.198 50405 typ host generation 0\r\n" +
"a=candidate:3628985204 1 tcp 1509957375 192.168.1.106 0 typ host generation 0\r\n" +
"a=candidate:2306696318 1 tcp 1509957375 10.0.2.1 0 typ host generation 0\r\n" +
"a=candidate:16163042 1 tcp 1509957375 169.254.149.198 0 typ host generation 0\r\n" +
"a=ice-ufrag:VThWp2PPdDGsfI9N\r\n" +
"a=ice-pwd:IfyRTOjYZgbm9f1rz+Ax6Qgu\r\n" +
"a=sendrecv\r\n" +
"a=mid:audio\r\n" +
"a=rtcp-mux\r\n" +
"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:+j1p80f5UWBnk3OBTov8j3w1wf7rjeEYwNJU6ROq\r\n" +
"a=rtpmap:110 opus/48000/2\r\n" +
"a=fmtp:110 minptime=10\r\n" +
"a=rtpmap:0 PCMU/8000\r\n" +
"a=rtpmap:8 PCMA/8000\r\n" +
"a=ssrc:556784522 cname:02pXsDmY1B93jiWK\r\n" +
"a=ssrc:556784522 msid:iP3E1Fzw7KYJlA41z0dEEIXCzsN3BHg4EKZ6 iP3E1Fzw7KYJlA41z0dEEIXCzsN3BHg4EKZ6a0\r\n" +
"a=ssrc:556784522 mslabel:iP3E1Fzw7KYJlA41z0dEEIXCzsN3BHg4EKZ6\r\n" +
"a=ssrc:556784522 label:iP3E1Fzw7KYJlA41z0dEEIXCzsN3BHg4EKZ6a0\r\n" +
"m=video 49402 RTP/SAVPF 102\r\n" +
"c=IN IP4 192.168.100.106\r\n" +
"a=rtcp:1 IN IP4 0.0.0.0\r\n" +
"a=candidate:2530088836 1 udp 2113937151 192.168.1.106 49900 typ host generation 0\r\n" +
"a=candidate:3355351182 1 udp 2113937151 10.0.2.1 58186 typ host generation 0\r\n" +
"a=candidate:1316314130 1 udp 2113937151 169.254.149.198 50405 typ host generation 0\r\n" +
"a=candidate:3628985204 1 tcp 1509957375 192.168.1.106 0 typ host generation 0\r\n" +
"a=candidate:2306696318 1 tcp 1509957375 10.0.2.1 0 typ host generation 0\r\n" +
"a=candidate:16163042 1 tcp 1509957375 169.254.149.198 0 typ host generation 0\r\n" +
"a=ice-ufrag:VThWp2PPdDGsfI9N\r\n" +
"a=ice-pwd:IfyRTOjYZgbm9f1rz+Ax6Qgu\r\n" +
"a=sendrecv\r\n" +
"a=mid:video\r\n" +
"a=rtcp-mux\r\n" +
"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:+j1p80f5UWBnk3OBTov8j3w1wf7rjeEYwNJU6ROq\r\n" +
"a=rtpmap:102 VP8/90000\r\n" +
"a=rtcp-fb:102 ccm fir\r\n" +
"a=rtcp-fb:102 nack\r\n" +
"a=ssrc:2928818856 cname:02pXsDmY1B93jiWK\r\n" +
"a=ssrc:2928818856 msid:iP3E1Fzw7KYJlA41z0dEEIXCzsN3BHg4EKZ6 iP3E1Fzw7KYJlA41z0dEEIXCzsN3BHg4EKZ6v0\r\n" +
"a=ssrc:2928818856 mslabel:iP3E1Fzw7KYJlA41z0dEEIXCzsN3BHg4EKZ6\r\n" +
"a=ssrc:2928818856 label:iP3E1Fzw7KYJlA41z0dEEIXCzsN3BHg4EKZ6v0\r\n");
*/

/*
    public static final String sdpWebRTC1 = new String("v=0\r\n" +
"o=test1 3608 657 IN IP4 10.0.2.3\r\n" +
"s=Talk\r\n" +
"c=IN IP4 121.96.234.119\r\n" +
"a=rtcp:59691 IN IP4 121.96.234.119\r\n" +
"b=AS:380\r\n" +
"t=0 0\r\n" +
"m=audio 56382 RTP/SAVPF 111 110 100 3 0 8 120 9 101\r\n" +
"a=candidate:186199869 1 udp 2113937151 192.168.1.101 59691 typ host generation 0\r\n" +
"a=candidate:186199869 2 udp 2113937151 192.168.1.101 59691 typ host generation 0\r\n" +
"a=candidate:3355351182 1 udp 2113937151 10.0.2.1 54564 typ host generation 0\r\n" +
"a=candidate:3355351182 2 udp 2113937151 10.0.2.1 54564 typ host generation 0\r\n" +
"a=candidate:1205546383 1 udp 2113937151 169.254.209.40 65465 typ host generation 0\r\n" +
"a=candidate:1205546383 2 udp 2113937151 169.254.209.40 65465 typ host generation 0\r\n" +
"a=candidate:2320574857 1 udp 1845501695 121.96.234.119 59691 typ srflx raddr 192.168.1.101 rport 59691 generation 0\r\n" +
"a=candidate:2320574857 2 udp 1845501695 121.96.234.119 59691 typ srflx raddr 192.168.1.101 rport 59691 generation 0\r\n" +
"a=candidate:1167774669 1 tcp 1509957375 192.168.1.101 59635 typ host generation 0\r\n" +
"a=candidate:1167774669 2 tcp 1509957375 192.168.1.101 59635 typ host generation 0\r\n" +
"a=candidate:2306696318 1 tcp 1509957375 10.0.2.1 59636 typ host generation 0\r\n" +
"a=candidate:2306696318 2 tcp 1509957375 10.0.2.1 59636 typ host generation 0\r\n" +
"a=candidate:156815743 1 tcp 1509957375 169.254.209.40 59637 typ host generation 0\r\n" +
"a=candidate:156815743 2 tcp 1509957375 169.254.209.40 59637 typ host generation 0\r\n" +
"a=remote-candidates:1 192.168.1.106 56382 2 192.168.1.107 56384\r\n" +
"a=ice-ufrag:Fv405CktnNiOEzcz\r\n" +
"a=ice-pwd:5AcgVg3kROTJKQTbf87B8kyb\r\n" +
"a=ice-options:google-ice\r\n" +
"a=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\n" +
"a=rtpmap:115 opus/48000/2\r\n" +
"a=fmtp:115 minptime=10\r\n" +
"a=rtpmap:111 speex/16000\r\n" +
"a=fmtp:111 vbr=on\r\n" +
"a=rtpmap:110 speex/8000\r\n" +
"a=fmtp:110 vbr=on\r\n" +
"a=rtpmap:100 iLBC/8000\r\n" +
"a=fmtp:100 mode=30\r\n" +
"a=rtpmap:121 SILK/24000\r\n" +
"a=rtpmap:120 SILK/16000\r\n" +
"a=rtpmap:9 G722/8000\r\n" +
"a=rtpmap:101 telephone-event/8000\r\n" +
"a=fmtp:101 0-11\r\n" +
"a=test\r\n" +
"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:DaJvLkMI6PZ+I/L9qEDnajmSZOAaos8SYooaH5vy\r\n" +
"a=crypto:2 AES_CM_128_HMAC_SHA1_32 inline:81YpxofxgSAD+aqFw+v0XTF55oRCVCixjiWecQl2\r\n" +
"a=rtcp-mux\r\n" +
"a=ssrc:4010711364 cname:1kMP6EA182dYHrwP\r\n" +
"a=ssrc:4010711364 msid:vXJRsL5ImjhcjEIXcIAYgQu8AMRBSCS1pOAG vXJRsL5ImjhcjEIXcIAYgQu8AMRBSCS1pOAGv0\r\n" +
"a=ssrc:4010711364 mslabel:vXJRsL5ImjhcjEIXcIAYgQu8AMRBSCS1pOAG\r\n" +
"a=ssrc:4010711364 label:vXJRsL5ImjhcjEIXcIAYgQu8AMRBSCS1pOAGv0\r\n"+
"m=video 7170 RTP/SAVPF 125\n" +
"c=IN IP4 192.168.1.105\r\n" +
"a=rtpmap:125 VP8/90000\r\n" +
"a=fmtp:125 some vp8 fmtp params\r\n" +
"a=imageattr:125 recv [x=[128:16:352],y=[96:16:288]] send [x=[128:16:352],y=[96:16:288]]\r\n" +
"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:8JDqLZa5PaSPamQDsQE4zbuOe6/vcZLSG6ixbd6S\r\n" +
"a=crypto:2 AES_CM_128_HMAC_SHA1_32 inline:1hILc1t4iErxlGsUM4eojcrQ313aXd6OEewSJeiy|2^20|1:4\r\n" +
"a=rtcp-fb:* nack pli\r\n" +
"a=rtcp-fb:104 ccm fir\r\n" +
"a=rtcp-mux\r\n" +
"a=ssrc:968476305 cname:1kMP6EA182dYHrwP\r\n" +
"a=ssrc:968476305 msid:vXJRsL5ImjhcjEIXcIAYgQu8AMRBSCS1pOAG vXJRsL5ImjhcjEIXcIAYgQu8AMRBSCS1pOAGv0\r\n" +
"a=ssrc:968476305 mslabel:vXJRsL5ImjhcjEIXcIAYgQu8AMRBSCS1pOAG\r\n" +
"a=ssrc:968476305 label:vXJRsL5ImjhcjEIXcIAYgQu8AMRBSCS1pOAGv0\r\n");
*/
/*
    public static final String sdpWebRTC2 = new String("v=0\r\n" +
"o=test1 3608 657 IN IP4 10.0.2.3\r\n" +
"s=Talk\r\n" +
"c=IN IP4 10.0.2.3\r\n" +
"b=AS:380\r\n" +
"t=0 0\r\n" +
"m=audio 7076 RTP/SAVPF 111 110 100 3 0 8 120 9 101\r\n" +
"a=rtcp:7076 IN IP4 10.0.2.3 \r\n" +
"a=candidate:2530088836 1 udp 2113937151 192.168.1.106 56382 typ host generation 0\r\n" +
"a=candidate:2530088836 2 udp 2113937151 192.168.1.106 56382 typ host generation 0\r\n" +
"a=candidate:2845672098 1 udp 2113937151 172.16.187.1 54652 typ host generation 0\r\n" +
"a=candidate:2845672098 2 udp 2113937151 172.16.187.1 54652 typ host generation 0\r\n" +
"a=candidate:3026868468 1 udp 2113937151 172.16.165.1 62390 typ host generation 0\r\n" +
"a=candidate:3026868468 2 udp 2113937151 172.16.165.1 62390 typ host generation 0\r\n" +
"a=candidate:3628985204 1 tcp 1509957375 192.168.1.106 55991 typ host generation 0\r\n" +
"a=candidate:3628985204 2 tcp 1509957375 192.168.1.106 55991 typ host generation 0\r\n" +
"a=candidate:3877535314 1 tcp 1509957375 172.16.187.1 55992 typ host generation 0\r\n" +
"a=candidate:3877535314 2 tcp 1509957375 172.16.187.1 55992 typ host generation 0\r\n" +
"a=candidate:4209615876 1 tcp 1509957375 172.16.165.1 55993 typ host generation 0\r\n" +
"a=candidate:4209615876 2 tcp 1509957375 172.16.165.1 55993 typ host generation 0\r\n" +
"a=remote-candidates:1 192.168.1.106 56382 2 192.168.1.107 56384\r\n" +
"a=ice-lite\r\n" +
"a=ice-ufrag:Fv405CktnNiOEzcz\r\n" +
"a=ice-pwd:5AcgVg3kROTJKQTbf87B8kyb\r\n" +
"a=ice-options:google-ice\r\n" +
"a=rtpmap:111 speex/16000\r\n" +
"a=fmtp:111 vbr=on\r\n" +
"a=rtpmap:110 speex/8000\r\n" +
"a=fmtp:110 vbr=on\r\n" +
"a=rtpmap:100 iLBC/8000\r\n" +
"a=fmtp:100 mode=30\r\n" +
"a=rtpmap:121 SILK/24000\r\n" +
"a=rtpmap:120 SILK/16000\r\n" +
"a=rtpmap:9 G722/8000\r\n" +
"a=rtpmap:101 telephone-event/8000\r\n" +
"a=fmtp:101 0-11\r\n" +
"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:DaJvLkMI6PZ+I/L9qEDnajmSZOAaos8SYooaH5vy\r\n" +
"a=crypto:2 AES_CM_128_HMAC_SHA1_32 inline:81YpxofxgSAD+aqFw+v0XTF55oRCVCixjiWecQl2\r\n" +
"a=rtcp-mux\r\n" +
"m=video 7170 RTP/SAVPF 125 104\n" +
"c=IN IP4 192.168.1.105\r\n" +
"a=rtpmap:125 VP8/90000\r\n" +
"a=imageattr:125 recv [x=[128:16:352],y=[96:16:288]] send [x=[128:16:352],y=[96:16:288]]\r\n" +
"a=rtpmap:104 H264/90000\r\n" +
"a=rtcp-fb:* nack pli\r\n" +
"a=rtcp-fb:104 ccm fir\r\n" +
"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:8JDqLZa5PaSPamQDsQE4zbuOe6/vcZLSG6ixbd6S\r\n" +
"a=crypto:2 AES_CM_128_HMAC_SHA1_32 inline:1hILc1t4iErxlGsUM4eojcrQ313aXd6OEewSJeiy|2^20|1:4\r\n" +
"a=ssrc:968476305 cname:1kMP6EA182dYHrwP\r\n" +
"a=ssrc:968476305 msid:vXJRsL5ImjhcjEIXcIAYgQu8AMRBSCS1pOAG vXJRsL5ImjhcjEIXcIAYgQu8AMRBSCS1pOAGv0\r\n" +
"a=ssrc:968476305 mslabel:vXJRsL5ImjhcjEIXcIAYgQu8AMRBSCS1pOAG\r\n" +
"a=ssrc:968476305 label:vXJRsL5ImjhcjEIXcIAYgQu8AMRBSCS1pOAGv0\r\n" +
"a=rtcp-mux\r\n");
*/
/*
public static final String sdpFirefox = new String("v=0\r\n" +
"o=Mozilla-SIPUA-22.0 20323 0 IN IP4 0.0.0.0\r\n"+
"s=SIP Call\r\n"+
"t=0 0\r\n" +
"a=ice-ufrag:a7e1a395\r\n" +
"a=ice-pwd:65f722e3c101a0b879d2ce2637db9ae0\r\n" +
"a=fingerprint:sha-256 E8:99:D8:CA:83:81:18:45:D1:8B:54:90:64:07:01:7A:F8:9C:36:CC:5B:E0:D8:D1:89:61:0E:FF:B4:61:EF:1E\r\n" +
"m=audio 52088 RTP/SAVPF 109 0 8 101\r\n" +
"c=IN IP4 172.16.0.56\r\n" +
"a=rtpmap:109 opus/48000/2\r\n" +
"a=ptime:20\r\n" +
"a=rtpmap:0 PCMU/8000\r\n" +
"a=rtpmap:8 PCMA/8000\r\n" +
"a=rtpmap:101 telephone-event/8000\r\n" +
"a=fmtp:101 0-15\r\n" +
"a=sendrecv\r\n" +
"a=candidate:0 1 UDP 2113667327 172.16.0.56 52088 typ host\r\n" +
"a=candidate:1 1 UDP 2112946431 172.16.187.1 57718 typ host\r\n" +
"a=candidate:2 1 UDP 2112487679 172.16.165.1 50384 typ host\r\n" +
"a=candidate:0 2 UDP 2113667326 172.16.0.56 49697 typ host\r\n" +
"a=candidate:1 2 UDP 2112946430 172.16.187.1 64290 typ host\r\n" +
"a=candidate:2 2 UDP 2112487678 172.16.165.1 56530 typ host\r\n" +
//"a=ice-ufrag:hi2GVHNey2MY3MpQ\r\n" +
//"a=ice-pwd:CS+fGd7xf+ygABm3BD8weDhY\r\n" +
//"a=ice-options:google-ice\r\n" +
//"a=fingerprint:sha-256 FE:D2:5E:AF:98:9D:4E:30:5B:2B:76:E8:2C:D0:F9:B2:E4:B3:DE:93:28:C5:E5:3D:D4:C7:23:F7:A5:FF:7E:76\r\n" +
"m=video 61213 RTP/SAVPF 120\r\n" +
"c=IN IP4 172.16.0.56\r\n" +
"a=rtpmap:120 VP8/90000\r\n" +
"a=sendrecv\r\n" +
"a=candidate:0 1 UDP 2113667327 172.16.0.56 61213 typ host\r\n" +
"a=candidate:1 1 UDP 2112946431 172.16.187.1 53780 typ host\r\n" +
"a=candidate:2 1 UDP 2112487679 172.16.165.1 60416 typ host\r\n" +
"a=candidate:0 2 UDP 2113667326 172.16.0.56 53569 typ host\r\n" +
"a=candidate:1 2 UDP 2112946430 172.16.187.1 64291 typ host\r\n" +
"a=candidate:2 2 UDP 2112487678 172.16.165.1 60326 typ host\r\n" +
//"a=ice-ufrag:hi2GVHNey2MY3MpQ\r\n" +
//"a=ice-pwd:CS+fGd7xf+ygABm3BD8weDhY\r\n" +
//"a=ice-options:google-ice\r\n" +
//"a=fingerprint:sha-256 FE:D2:5E:AF:98:9D:4E:30:5B:2B:76:E8:2C:D0:F9:B2:E4:B3:DE:93:28:C5:E5:3D:D4:C7:23:F7:A5:FF:7E:76\r\n" +
"");
*/

public static final String sdpWebRTCTURN = new String("v=0\r\n" +
"o=- 7500386360062901000 2 IN IP4 127.0.0.1\r\n" +
"s=Doubango Telecom - chrome\r\n" +
"t=0 0\r\n" +
"a=group:BUNDLE audio video\r\n" +
"a=msid-semantic: WMS 64Vn8uD8VFwXFZtANTEb5tYnEqDTs8LQToPF\r\n" +
"m=audio 58515 RTP/SAVPF 111 103 104 0 8 106 105 13 126\r\n" +
"c=IN IP4 192.168.1.44\r\n" +
"a=rtcp:58515 IN IP4 192.168.1.44\r\n" +
"a=candidate:3028854479 1 udp 2113937151 192.168.1.44 63501 typ host generation 0\r\n" +
"a=candidate:3028854479 2 udp 2113937151 192.168.1.44 63501 typ host generation 0\r\n" +
"a=candidate:2845672098 1 udp 2113937151 172.16.187.1 55720 typ host generation 0\r\n" +
"a=candidate:2845672098 2 udp 2113937151 172.16.187.1 55720 typ host generation 0\r\n" +
"a=candidate:3026868468 1 udp 2113937151 172.16.165.1 59583 typ host generation 0\r\n" +
"a=candidate:3026868468 2 udp 2113937151 172.16.165.1 59583 typ host generation 0\r\n" +
"a=candidate:4195047999 1 tcp 1509957375 192.168.1.44 0 typ host generation 0\r\n" +
"a=candidate:4195047999 2 tcp 1509957375 192.168.1.44 0 typ host generation 0\r\n" +
"a=candidate:3877535314 1 tcp 1509957375 172.16.187.1 0 typ host generation 0\r\n" +
"a=candidate:3877535314 2 tcp 1509957375 172.16.187.1 0 typ host generation 0\r\n" +
"a=candidate:4209615876 1 tcp 1509957375 172.16.165.1 0 typ host generation 0\r\n" +
"a=candidate:4209615876 2 tcp 1509957375 172.16.165.1 0 typ host generation 0\r\n" +
"a=candidate:1520572953 1 udp 33562367 192.168.1.44 58515 typ relay raddr 192.168.1.44 rport 50637 generation 0\r\n" +
"a=candidate:1520572953 2 udp 33562367 192.168.1.44 58515 typ relay raddr 192.168.1.44 rport 50637 generation 0\r\n" +
"a=ice-ufrag:NFloeGxdXFSJhEh4\r\n" +
"a=ice-pwd:KnYtFRt22fOdqVtYSWMbqbLC\r\n" +
"a=ice-options:google-ice\r\n" +
"a=fingerprint:sha-256 08:63:5C:CA:B0:B9:80:B4:FE:6D:55:DA:75:8D:C9:41:16:0F:F1:03:37:C9:46:AF:85:84:AB:9E:65:16:07:8C\r\n" +
"a=setup:actpass\r\n" +
"a=mid:audio\r\n" +
"a=extmap:1 urn:ietf:params:rtp-hdrext:ssrc-audio-level\r\n" +
"a=sendrecv\r\n" +
"a=rtcp-mux\r\n" +
"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:pm3rm7YvLY8QdXAdqYkbvbAQM0f5alVjbIBuO/Yx\r\n" +
"a=rtpmap:111 opus/48000/2\r\n" +
"a=fmtp:111 minptime=10\r\n" +
"a=rtpmap:103 ISAC/16000\r\n" +
"a=rtpmap:104 ISAC/32000\r\n" +
"a=rtpmap:0 PCMU/8000\r\n" +
"a=rtpmap:8 PCMA/8000\r\n" +
"a=rtpmap:106 CN/32000\r\n" +
"a=rtpmap:105 CN/16000\r\n" +
"a=rtpmap:13 CN/8000\r\n" +
"a=rtpmap:126 telephone-event/8000\r\n" +
"a=maxptime:60\r\n" +
"a=ssrc:1994629766 cname:q7r3EgtQSaGOqX0P\r\n" +
"a=ssrc:1994629766 msid:64Vn8uD8VFwXFZtANTEb5tYnEqDTs8LQToPF 64Vn8uD8VFwXFZtANTEb5tYnEqDTs8LQToPFa0\r\n" +
"a=ssrc:1994629766 mslabel:64Vn8uD8VFwXFZtANTEb5tYnEqDTs8LQToPF\r\n" +
"a=ssrc:1994629766 label:64Vn8uD8VFwXFZtANTEb5tYnEqDTs8LQToPFa0\r\n" +
"m=video 58515 RTP/SAVPF 100 116 117\r\n" +
"c=IN IP4 192.168.1.44\r\n" +
"a=rtcp:58515 IN IP4 192.168.1.44\r\n" +
"a=candidate:3028854479 1 udp 2113937151 192.168.1.44 63501 typ host generation 0\r\n" +
"a=candidate:3028854479 2 udp 2113937151 192.168.1.44 63501 typ host generation 0\r\n" +
"a=candidate:2845672098 1 udp 2113937151 172.16.187.1 55720 typ host generation 0\r\n" +
"a=candidate:2845672098 2 udp 2113937151 172.16.187.1 55720 typ host generation 0\r\n" +
"a=candidate:3026868468 1 udp 2113937151 172.16.165.1 59583 typ host generation 0\r\n" +
"a=candidate:3026868468 2 udp 2113937151 172.16.165.1 59583 typ host generation 0\r\n" +
"a=candidate:4195047999 1 tcp 1509957375 192.168.1.44 0 typ host generation 0\r\n" +
"a=candidate:4195047999 2 tcp 1509957375 192.168.1.44 0 typ host generation 0\r\n" +
"a=candidate:3877535314 1 tcp 1509957375 172.16.187.1 0 typ host generation 0\r\n" +
"a=candidate:3877535314 2 tcp 1509957375 172.16.187.1 0 typ host generation 0\r\n" +
"a=candidate:4209615876 1 tcp 1509957375 172.16.165.1 0 typ host generation 0\r\n" +
"a=candidate:4209615876 2 tcp 1509957375 172.16.165.1 0 typ host generation 0\r\n" +
"a=candidate:1520572953 1 udp 33562367 192.168.1.44 58515 typ relay raddr 192.168.1.44 rport 50637 generation 0\r\n" +
"a=candidate:1520572953 2 udp 33562367 192.168.1.44 58515 typ relay raddr 192.168.1.44 rport 50637 generation 0\r\n" +
"a=ice-ufrag:NFloeGxdXFSJhEh4\r\n" +
"a=ice-pwd:KnYtFRt22fOdqVtYSWMbqbLC\r\n" +
"a=ice-options:google-ice\r\n" +
"a=fingerprint:sha-256 08:63:5C:CA:B0:B9:80:B4:FE:6D:55:DA:75:8D:C9:41:16:0F:F1:03:37:C9:46:AF:85:84:AB:9E:65:16:07:8C\r\n" +
"a=setup:actpass\r\n" +
"a=mid:video\r\n" +
"a=extmap:2 urn:ietf:params:rtp-hdrext:toffset\r\n" +
"a=extmap:3 http://www.webrtc.org/experiments/rtp-hdrext/abs-send-time\r\n" +
"a=sendrecv\r\n" +
"a=rtcp-mux\r\n" +
"a=crypto:1 AES_CM_128_HMAC_SHA1_80 inline:pm3rm7YvLY8QdXAdqYkbvbAQM0f5alVjbIBuO/Yx\r\n" +
"a=rtpmap:100 VP8/90000\r\n" +
"a=rtcp-fb:100 ccm fir\r\n" +
"a=rtcp-fb:100 nack\r\n" +
"a=rtcp-fb:100 goog-remb\r\n" +
"a=rtpmap:116 red/90000\r\n" +
"a=rtpmap:117 ulpfec/90000\r\n" +
"a=ssrc:1369908970 cname:q7r3EgtQSaGOqX0P\r\n" +
"a=ssrc:1369908970 msid:64Vn8uD8VFwXFZtANTEb5tYnEqDTs8LQToPF 64Vn8uD8VFwXFZtANTEb5tYnEqDTs8LQToPFv0\r\n" +
"a=ssrc:1369908970 mslabel:64Vn8uD8VFwXFZtANTEb5tYnEqDTs8LQToPF\r\n" +
"a=ssrc:1369908970 label:64Vn8uD8VFwXFZtANTEb5tYnEqDTs8LQToPFv0\r\n");
}
