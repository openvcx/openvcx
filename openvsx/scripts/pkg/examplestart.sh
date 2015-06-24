#!/bin/bash

#
# This is an example on how to script OpenVSX.
# You can use this script to build command line arguments controlling vsx.
#

function checkvsx {
  VSX_HOME=$1
  VSX_PATH="${VSX_HOME}/${VSX}"
  if [ ! -f "${VSX_PATH}" ]; then
    VSX_PATH=""
  fi
}

function findvsx {
  if [ "${VSX_PATH}" == "" ]; then
    checkvsx .
    if [ "${VSX_PATH}" != "" ]; then
      PWD=`pwd`
      VSX_HOME=`dirname ${PWD}`
    fi
    VSX="bin/${VSX}"
  fi
  if [ "${VSX_PATH}" == "" ]; then
    checkvsx ${VSX_HOME} 
  fi
  if [ "${VSX_PATH}" == "" ]; then
    checkvsx .
  fi
  if [ "${VSX_PATH}" == "" ]; then
    checkvsx `dirname $0`
  fi
  if [ "${VSX_PATH}" == "" ]; then
    checkvsx `dirname $VSX_HOME`
  fi
  if [ "${VSX_PATH}" == "" ]; then
    checkvsx `dirname $VSX_HOME`
  fi
  if [ "${VSX_PATH}" == "" ]; then
    echo "Unable to find your OpenVSX (Video Streaming Processor) installation directory.  Please set VSX_HOME or run this script from your VSX installation directory. "
    exit 0
  fi
}

VSX="vsx"
VSX_PATH=""

findvsx
if [ "${VSX_HOME}" != "" ]; then
  cd ${VSX_HOME}
  VSX_HOME=`pwd`
fi

LITE_VERSION=`cat ${VSX_HOME}/version.txt | grep 'Lite'`

echo ""
echo "This is an example on how to script OpenVSX."
echo "You can use this script to build command line arguments controlling vsx."
echo ""

VERBOSE="--verbose"

VERBOSE_OPT=${VERBOSE}
HTTPLIVE_OPT="--httplivedir=html/httplive --httplivechunk=5 --httplive=8080"
FLVLIVE_OPT="--flvlive=8080"
RTMP_OPT="--rtmp=1935"
LIVE_OPT="--live=8080"
CAPTURE_OPT="--capture=udp://127.0.0.1:41394 --filter=\"type=m2t\""
RTSP_OPT="--rtsp=1554"
STREAM_OPT="--stream=rtp://127.0.0.1:5004,5006 --transport=rtp ${RTSP_OPT}"

if [ "${LITE_VERSION}" == "" ]; then
    XCODE_OPT_VIDEO="vc=h264,vb=400,vt=20,y=320,vf=1,vp=66,"
    XCODE_OPT_AUDIO="ac=aac,ab=32000,ar=22050,as=1"
    if [ "${XCODE_OPT_VIDEO}" != "" -o "${XCODE_OPT_AUDIO}" != "" ]; then
        XCODE_OPT="--xcode=\"${XCODE_OPT_VIDEO}${XCODE_OPT_AUDIO}\""
    fi
fi

ALL_OPT="${VERBOSE_OPT} ${CAPTURE_OPT} ${HTTPLIVE_OPT} ${RTMP_OPT} ${FLVLIVE_OPT} \
         ${LIVE_OPT} ${STREAM_OPT} ${XCODE_OPT}"
echo  ./${VSX} ${ALL_OPT}
./${VSX} ${ALL_OPT}
