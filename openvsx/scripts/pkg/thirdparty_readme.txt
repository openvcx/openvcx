====================================================================
    Package Overview 
====================================================================


  This add-on package contains public domain third-party encoder / decoder
  implementations packaged as shared library objects which may be
  dynamically linked by OpenVSX.  If applicable, care should be taken to
  comply with any licensing restrictions of the third-party software
  within your organization.


====================================================================
    Package Contents
====================================================================


  This package contains third-party encoder/decoder libraries which may 
  be used by OpenVSX.

  lib/libx264.so          - libx264 GPL H.264 encoder implementation.

  This shared library is provided here only for your convenience 
  when evaluating OpenVSX.  You can build the library yourself by 
  downloading the full source from:
  http://www.videolan.org/developers/x264.html

  lib/libSKP_SILK_SDK.so  - Skype SILK audio encoder / decoder implementation.

  This shared library is provided here only for your convenience when evaluating
  OpenVSX.  To use the Skype SILK encoder / decoder 
  implementation within your organization you should obtain the SkypeKit 
  from https://dev.skype.com/skypekit and build the source yourself.

  lib/libfaac.so  - AAC audio encoder implementation.

  This shared library is provided here only for your convenience when evaluating
  OpenVSX.  To use the AAC encoder implementation within your 
  organization you should ensure you comply with any patent licenses governing
  the AAC codec.  You can build the library yourself by downloading the full source 
  from: kwkhttp://www.audiocoding.com/faac.html


====================================================================
    Getting Started 
====================================================================


  You can unzip or copy the entire package contents into the OpenVSX root 
  directory of the OpenVSX download bundle.  Alternatively, you can copy
  the individual library components into the OpenVSX lib directory.

  To enable OpenVSX to utilize the x264 video encoder copy libx264.so into 
  the OpenVSX lib directory.

  To enable OpenVSX to utilize the SILK audio codec copy libSKP_SILK_SDK.so 
  into the OpenVSX lib directory.

  To enable OpenVSX to utilize the faac AAC audio encoder copy libfaac.so 
  into the OpenVSX lib directory.





