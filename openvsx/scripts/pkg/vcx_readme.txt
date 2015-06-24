====================================================================
              Thank you for downloading OpenVCX 
                (Video Conferencing Server)
====================================================================


  OpenVCX is a SIP (Session Initiation Protocol) based Video Conferencing
  Server.  OpenVCX requires the OpenVSX (Video Streaming Server) media back-end 
  for voice and video media processing.  With OpenVCX you can bridge multiple 
  SIP video calling clients into a single video conference.  


====================================================================
    Package Contents
====================================================================

  README.txt            - This readme file
  bin/                  - Application server scripts
  conf/                 - Configuration files
  conferences/          - SIP Conference definition files 
  lib/                  - JAR libraries 
  license/              - Software license content 
  logs/                 - SIP server output logs 
  media/                - Media announcements and chimes 
  startvcx.sh           - Startup and control script
  openvsx/             - OpenVSX (Video Streaming Processor) bundled installation 
  stunnel/              - SSL Tunnelling application 
  temp/                 - Temporary file storage location
  version.txt           - Software version file
  webapps/              - SIP Server war application path 


====================================================================
    Getting Started with the Server
====================================================================


1. OpenVCX requires minimal setup.  Launch OpenVCX from the system shell by 
   invoking the startvcx.sh script found in the base installation directory.

   cd [ OpenVCX  INSTALL DIR ]; 
   ./startvcx.sh

2. If your OpenVCX distribution does not come with a bundled 
   OpenVSX (Video Streaming Server) distribution you will be asked to set VSX_HOME 
   to the directory of the full path of the OpenVSX installation.  If you have not 
   downloaded and installed OpenVSX you can download it 
   from http://openvcx.com/download.php

3. OpenVCX runs as a java process using the Mobicents JAIN-SIP framework.  
   If you do not have the Java Platform JRE 6 (v1.6) or higher installed 
   you can download and install it for your operating system from here:
   http://www.oracle.com/technetwork/java/javase/downloads/index.html 

4. You will be asked to set SDP_CONNECT_LOCATION to the external IP
   address or full hostname of your server.  This address needs to
   be accessible by any video media clients and should itself not be 
   behind a NAT.  If you are testing on your localized private network 
   you can set this to the IP of your network interface. 

5. The OpenVCX Video Conferencing Server application archive is located in 
   webapps/sip-conference.war.  
   You can edit the OpenVCX configuration file conf/sip-conference.conf.  

   The server listens for TCP and UDP SIP requests on 0.0.0.0:5060 and
   Websocket on 0.0.0.0:5062.  

   To change the listener edit conf/server.xml.

   Log output is sent to logs/sip-conference.log.  


====================================================================
    Placing a Video Call
====================================================================


1.  You will need a standards based VoIP SIP video client such as
    X-Lite, Linphone, Bria, Vippie, WebRTC, etc...  

    An in-browser WebRTC client demo is included at 
    http://[INSTALL IP]:8090/webrtc-demo/ .  


2.  In your SIP video client, set the outbound SIP proxy to the IP 
    or hostname of the server running OpenVCX.   OpenVCX is able to
    handle SIP registrations but it does not act like a SIP Registrar
    so you can use any username and password in your SIP video client.


3.  Manually create a conference definition file in the directory 
    conference/.  The file name should match the SIP username of the 
    conference virtual endpoint and the contents can be empty.  A sample 
    file is provided in conference/1234.conference.  You can connect to 
    this conference by dialing a SIP URI such as 1234@mydomain.com.


4.  If you are using WebRTC by accessing the page at 
    http://[INSTALL IP]:8090/webrtc-demo/ just click on 'Register' to 
    automatically provision a personal conference SIP URI.  The number 
    should appear in the 'Conference SIP URI' field.  Calling this SIP 
    URI will make your live stream available to viewing from any web 
    device at the published link shown at near the top of the page.
    You can then bridge other video clients to the same conference number.
    

====================================================================
    Deployment Guide
====================================================================


  OpenVCX acts as a Video MCU (Multipoint Control Unit) allowing 
  multiple calling parties to interconnect together.  The server
  can be used to bridge codec mis-matches, perform format adaptation, 
  and bitrate adaptation between two or more calling parties.  The 
  server can also be used to host a video or audio-only conference 
  between multiple calling parties.

  OpenVCX can be deployed into an IMS architecture as an MRFP (Media
  Resource Function Processor).  As an MRFP, the server  handles all 
  media interaction for IMS calling clients.

  OpenVCX is a java component handling all signalling messages.  The
  OpenVSX back-end handles all video and audio media streams.  OpenVCX is 
  designed to run in the context of the Mobicents SIP Server 
  JSLEE 1.1 (JSR 240) Application Server Sip Servlets framework 
  available either for JBoss or Apache Tomcat.  The OpenVCX component 
  can be migrated to your own Application Server Framework by 
  copying the following content:
 
      bin/cleanup.sh
      bin/certfingerprint.sh
      bin/startvcx.sh
      conf/sip-conference.conf
      conferences/
      media/
      $__DIRNAME_MEDIA__@/
      version.txt
      webapps/sip-conference.war

  The WebRTC demo application is found in 

      webapps/webrtc-demo.war

  OpenVCX should be deployed on a publicly accessible server IP.
  OpenVCX listens for SIP messages on UDP/TCP port 5060 and Websocket 
  port 5062 (Configurable in conf/server.xml).  The Websocket protocol 
  is used by the WebRTC video client.  RTP Media services are provided 
  within the port range specified in conf/sip-ngconference.conf 
  'rtp.port.start' through 'rtp.port.end'.  These ports should be open 
  in any firewall and publicly accessible by SIP calling clients.

  OpenVCX supports ICE and and STUN.   OpenVCX does not itself act as a STUN 
  server for the purposes of a client's discovery of it's own IP and port 
  binding prior to generating an SDP.  A third party public STUN server 
  listening on port 3478 should be used by a calling client for public 
  IP discovery.  OpenVCX responds to STUN connectivity checks and issues 
  its own STUN connectivity checks following an ICE-Lite server 
  implementation.  It is recommended that ICE and STUN are enabled in 
  the calling client configuration.

  OpenVCX uses a conference definition file framework to establish
  available conferences.  Upon receiving a SIP INVITE, OpenVCX looks for
  the virtual conference SIP URI in the conference/ directory.  An Example 
  syntax is provided in the file conference/1234.conference.   A 
  third-party provisioning application should be used to manage 
  available conference SIP URIs and conference properties. 


====================================================================
    Troubleshooting 
====================================================================


  If you are having trouble establishing a SIP video call you will
  need to inspect the logs for any errors or warnings.
 
  OpenVCX produces log output in the 'logs/' directory within the the root 
  installation directory.  The log file containing any runtime error
  or informational output is 'logs/sip-ngconference.log'.

  To change the log output verbosity edit lib/log4j.xml and increase any
  relevant log level ouput value such as: ERROR, WARN, INFO, DEBUG, ALL.

  OpenVCX uses a dedicated OpenVSX process to service all media streams
  for a conference.  OpenVSX is started from the script located in
  VSX_HOME/bin/startconf.sh.  Each conference will have a dedicated
  log file using the following naming scheme:

  VSX_HOME/log/log_[ conference user ][ domain ]_[date].log

  For eg.,  dialing the virtual conference SIP URI 
  '1234@domain.com' should result in the following log file
  path being created:

  VSX_HOME/log/log_1234mydomain.com_Mon-Jan-01-13:54:41.log


  If you are testing the server and client together on the same system
  you may need to set 'icePreferHost=true' in the respective conference
  definition file to prevent using global ICE candidates. 
 

  To enable logging of SIP messages you may need to edit lib/log4j.xml 
  and set the priority for "gov.nist" to value="INFO" or higher.  SIP 
  messages content will be stored in the file logs/mss-jsip-messages.xml.



====================================================================
    Licensing
====================================================================

  OpenVCX is released under the Apache License, Version 2.0

    http://www.apache.org/licenses/LICENSE-2.0

  If you require product support, a support and maintenance agreement, or an 
  alternate license for commercial use please contact openvcx@gmail.com

  OpenVCX may link to third-party libraries distributed under the
  license(s) found in the 'license/' directory of the downloaded
  package.

  OpenVCX may link to libraries which are licensed under the LGPL (Lesser
  Gnu Public License).  OpenVCX itself is not licensed under the terms
  of the LGPL and can legally operate in conjunction with such
  third-party code.  The OpenVCX license complies with the terms of the
  LGPL, particularly section 6.


====================================================================
    For help and documentation visit the OpenVCX knowledge base at 
    http://openvcx.com/howtovcx.php
====================================================================


