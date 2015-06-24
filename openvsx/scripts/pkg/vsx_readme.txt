====================================================================
         Thank you for downloading OpenVSX!
====================================================================


  OpenVSX is a real-time video streaming server for use 
  in live broadcast environments.  It has many applications ranging from
  IPTV, streaming services in CDNs, Video VoIP, video conferencing, and 
  much more.

  OpenVSX can be used in the following modes of operation:

1. - OpenVCX (Video Conferencing Exchange). OpenVCX is a SIP 
     (Session Initiation Protocol) based video conferencing server for 
     Video VoIP (Voice over IP). OpenVCX leverages OpenVSX to provide all  
     media related functionality.  OpenVCX is packaged together with the 
     OpenVSX download bundle.

2. - OpenVSX Web Portal (Media Web Portal).  OpenVSX Web Portal is a web portal front-end for 
     hosting live video feeds and stored media content.  OpenVSX Web Portal selectively 
     launches OpenVSX media processes to handle transcoding and format adaptation 
     requests based on client device type identification.  The OpenVSX Web Portal 
     is launched by invoking bin/startvsxwp.sh 

2. - Scripting Mode.  OpenVSX is started from the command line to process a 
     single streaming task such as transcoding and reformatting one or more live 
     video streams.  OpenVSX can be launched by editing bin/examplestart.sh


====================================================================
    Package Contents
====================================================================


  bin/vsx               - OpenVSX wrapper startup script

  bin/vsxbin            - OpenVSX binary executable program

  bin/vsxwpbin          - OpenVSX Web Portal.  vsxbin is used to service
                          client media requests and can selectively launch
                          and control OpenVSX child processes.  

  bin/startvsx.sh       - Script used to start / stop the OpenVSX Web Portal 
                          Service.

  bin/examplestart.sh   - Example script used to show how to create an OpenVSX 
                          commandline for streaming.

  lib/libvsx.so         - OpenVSX shared library.             

  lib/libxcode.so       - Transcoder interface library.             

  etc/vsx.conf          - OpenVSX Configuration file.

  etc/vsxwp.conf        - OpenVSX Web Portal configuration file.

  etc/devices.conf      - Basic device profile configuration for client lookup 
                          based on User-Agent.

  etc/pip.conf          - Example PIP (Picture In Picture) Configuration script.

  etc/xcode.conf        - Example transcoder configuration file.


====================================================================
    Getting Started with OpenVSX 
====================================================================


1. OpenVSX is typically run from the command line.  To see the supported
   help options you would do the following from a system shell:

       cd PATH_OF_OpenVSX_INSTALLATION;
       ./bin/vsx -h 

   For usage information on stream output options:
       ./bin/vsx -h stream

2. Example script OpenVSX take a look at bin/examplestart.sh to see how to 
   control OpenVSX behavior.

       cd PATH_OF_OpenVSX_INSTALLATION;
       ./bin/examplestart.sh

   This example script launches OpenVSX to listen for an IPTV MPEG-2 TS
   video feed on port 41394 and converts it to H.264/AAC suitable
   for playback on Android, iOS, and Flash.  To view the video output
   connect a web browser to http://127.0.0.1:8080/live
   
3. For detailed information on usage take a look at the User Guide
   available online at http://openvcx.com/vsxuserguide.php .

====================================================================
    Getting Started with OpenVSX Web Portal 
====================================================================


1. After unpacking the OpenVSX software package contents to anywhere 
   on your system, edit the file etc/vsxwp.conf and set the 
   line 'media=' to the directory where to look for your digital 
   media files (.mp4, .flv, .m2t).  The line 'listen=' is by 
   default set to be accessed only from localhost.  To access 
   NGWP from a remote client set this to 'listen=0.0.0.0:8080'

2. Start OpenVSX Web Portal by running './bin/startvsxwp.sh start' from the 
   command line.  './bin/startvsxwp.sh stop' is used to stop the 
   service.

3. Read the OpenVSX Web Portal User Guide located in doc/OpenVSX Web Portal_userguide.pdf
   to learn how to create configuration meta-files to automatically
   adapt, format, and transcode any live and static media contents.



====================================================================
    Licensing
====================================================================

  OpenVSX is released under the Affero General Public License
  AGPL Version 3.

    http://www.gnu.org/licenses/agpl.html

  If you require product support, a support and maintenance agreement, or an 
  alternate license better suited for commercial use and distribution please
  contact openvcx@gmail.com

  OpenVSX may link to third-party libraries distributed under the
  license(s) found in the 'doc/' directory of the downloaded package.

  OpenVSX may link to libraries which are licensed under the LGPL (Lesser
  Gnu Public License).  OpenVSX itself is not licensed under the terms
  of the LGPL and can legally operate in conjunction with such
  third-party code.  The OpenVSX license complies with the terms of the
  LGPL, particularly section 6.



====================================================================
    Third Party Software:
====================================================================


  The OpenVSX distribution which you obtained may be used
  in conjunction with third-party encoders and decoder codec implementations
  included in the add-on package @__THIRDPARTY_PACKAGE_NAME__@

  The add-on package contains public domain third-party encoder / decoder 
  implementations packaged as shared library objects which may be 
  dynamically linked by OpenVSX.  If applicable, care should be taken to 
  comply with any licensing restrictions of the third-party software 
  within your organization.


====================================================================
    Troubleshooting:
====================================================================


  =====  Failure to find library dependancies

  ./bin/vsxbin: error while loading shared libraries: libvsx.so: 
  cannot open shared object file: No such file or directory

  ./bin/vsxbin: error while loading shared libraries: libxcode.so: 
  cannot open shared object file: No such file or directory

  This means that the system library loader cannot find the OpenVSX shared
  libs.  The wrapper startus script bin/vsx should set the correct
  library path for your operating system.  If it fails, or if you are
  calling vsxbin directly, the following command tells the shell where 
  to look for shared libraries from the OpenVSX home directory:
 
  on Linux:

  export LD_LIBRARY_PATH=./lib

  on Mac / Darwin:

  export DYLD_LIBRARY_PATH=./lib


  =====  Library permissions

  ./bin/vsxbin: error while loading shared libraries: libvsx.so:
  cannot restore segment prot after reloc: Permission denied

  ./bin/vsxbin: error while loading shared libraries: libxcode.so:
  cannot restore segment prot after reloc: Permission denied

  On some systems certain kernel security extensions may prohibit 
  the shared libraries from loading correctly.  To override this do:

  chcon -t texrel_shlib_t lib/libvsx.so
  chcon -t texrel_shlib_t lib/libxcode.so


  =====  Running on 64bit linux hosts

  The OpenVSX binary distribution should be able to run on most major flavors of 
  32 or 64 bit linux distributions based on x86 architecture.  For 64bit 
  systems you will need to have installed 32bit library support for your 
  system.

  On CentOS / Fedora / RedHat systems you can typically do this with:

  sudo yum install ia32-libs

  or

  sudo yum install glibc.i686  

  On Ubuntu / Debian systems you can typically do this with:

  sudo apt-get install ia32-libs

  If vsx still fails to start you should check if all library dependancies
  are fulfilled with the following command: 

  ldd ./bin/vsxbin


  =====  Unable to find license file 

  If you have just installed the license file 'license.dat' into 
  'etc/license.dat' but OpenVSX fails to read the license ensure that
  you have the entry 'license=etc/license.dat' in the OpenVSX configuration
  file 'etc/vsxwp.conf'.  This may occur if you are starting OpenVSX by
  doubleclicking from File Explorer.


====================================================================
    Help and Support:
====================================================================

   For Help and Support visit http://openvcx.com/howtovsx.php and
   use the contact form to ask an expert.
   

