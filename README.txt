====================================================================
      Thank you for downloading OpenVCX Video Streaming Software
====================================================================

====================================================================
    Package Contents 
====================================================================


  openvsx/              - OpenVSX (Video Streaming Processor) source

  vcx/                  - OpenVCX (SIP Video Conferencing Server) source

  packaging/            - Scripts for packaging and installation.

  third-party/          - Third party sources for OpenVSX (Video Streaming Processor)

  LICENSE.txt           - OpenVCX License 

  README.txt            - OpenVCX Source Package Readme 
  
  configure             - Build environment configuration script


====================================================================
    Prerequisites
====================================================================


  Supported OS: 
    - Mac OSX, 
    - Linux x86 (with 32bit application support)

  gcc 4.x or higher
  Java 6 JDK (v1.6) or higher 
  Apache Ant 


====================================================================
    How-to Build sources 
====================================================================


  ./configure            - Configure the build environment and create makefiles.
                           Do './configure -h' to view available configure options.

  make                   - Compile all OpenVSX and OpenVCX sources.

  make installer-build   - Create a self-extracting executable which will install
                           both OpenVCX and OpenVSX.

  To install on your system run the self-extracting executable from a shell.  The install script 
  will perform first-time configuration.  You do not need root privileges to install or run.

  ./dist/linux_x86_32/openvcx_linux_x86_32_v1.x.x.bxxx.bin

  'make openvsx' is used to build OpenVSX (Video Streaming Processor) source 
  found in vsx/.

  'make openvcx' is used to build OpenVCX (Video Conferencing Server) source 
  found in vcx/mobicents-sip/.  
  This is the same as calling the ant build script 'build.sh deploy all' 
  found in vcx/mobicents-sip/ to build OpenVCX java source.

  Use 'packaging/pkg_vsx_linux_x86_32.sh' or 'packaging/pkg_vsx_mac_x86_64.sh'
  to manually create an OpenVSX (Video Streaming Processor) tarball package.

  Use packaging/pkg_all.sh to create a self-extracting binary for 
  either Linux or Mac OSX which will install both OpenVCX and OpenVSX.


====================================================================
    How-to run OpenVCX SIP Video Conferencing Server
====================================================================


====================================================================
    Troubleshooting:
====================================================================


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


====================================================================

For questions, comments, or to request support, pleae email  openvcx@gmail.com

