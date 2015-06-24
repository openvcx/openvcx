#!/bin/sh

function getnosuffix {
  local VAR2=${1}
  VAR=${VAR2%.*}
}
function getfilename {
  #VAR=`basename ${1}`
  local VAR2=${1}
  VAR=${VAR2##*/}
}
function getdirname {
  VAR=`dirname ${1}`
}

function getvsxvermajmin {
  local FILE="./version/include/version.h"
  if [ ! -f ${FILE} ]; then
    FILE="./openvsx/version/include/version.h"
  fi
  VAR=`grep 'VSX_VERSION_MAJMIN ' ${FILE} | awk '{print $3}' | sed 's/\"//g'`
}

function getvsxbuildnum {
  local FILE="./version/include/build_info_ver.h"
  if [ ! -f ${FILE} ]; then
    FILE="./openvsx/version/include/build_info_ver.h"
  fi
  VAR=`grep 'BUILD_INFO_NUM' ${FILE} | awk '{print $3}' | sed 's/\"//g'`
}

function getvcxver {
  local FILE="./version/include/version.h"
  if [ ! -f ${FILE} ]; then
    FILE="./openvsx/version/include/version.h"
  fi
  VAR=`grep 'VCX_VERSION_BETA' ${FILE} | awk '{print $3}' | sed 's/\"//g'`
}

function funix2dos {
  local FILE=${1}
  awk 'sub("$", "\r")' ${FILE} > ${FILE}.win
  if [ -f ${FILE}.win ]; then
    mv ${FILE}.win ${FILE}
  fi
}

function have_config_val {
  local FILE=../../openvsx/makefiles/vsxconfig.mak
  if [ ! -f ${FILE} ]; then
    FILE=../../../openvsx/makefiles/vsxconfig.mak
  fi

  VAL=0 
  local RES=`grep ^$1 ${FILE}`
  if [ "${RES}" != "" ]; then
    VAL=`echo $RES | awk -F'=' '{print $2}'`
  fi
  if [ "${VAL}" != "1" ]; then
    VAL=0
  fi
  return $VAL
}

function have_xcode_define {
  local FILE=../../openvsx/makefiles/xcodeconfig.h
  if [ ! -f ${FILE} ]; then
    FILE=../../../openvsx/makefiles/xcodeconfig.h
  fi

  local CAP=$1

  VAL=`grep " $CAP " ${FILE} | grep -v '//' | tr -d ' ' | awk -F"${CAP}" '{print $2}'`
  if [ "${VAL}" != "1" ]; then
    VAL=0
  fi
  return $VAL
}

function getVersion {
  if [ ! -f "$1" ]; then
      echo ""
      echo "Unable to get version information from $1"
      echo ""
      VERSION="0.0.0.b0"
  else
      VERSION=`grep -o '[0-9].*' $1`
  fi
  VERMAJOR=`echo ${VERSION} | awk -F '.' '{print $1}'`
  VERMINOR=`echo ${VERSION} | awk -F '.' '{print $2}'`
  VERMINOR2=`echo ${VERSION} | awk -F '.' '{print $3}'`
  #VERMINOR2=`grep -o '[0-9].*.b' $1 | awk -F '.' '{print $3}'`
  local T1=`echo ${VERSION} | awk -F '.' '{print $4}'`
  local T2=( $T1 )
  local T3="${T2[0]}"
  BUILDNUM="${T3:1}"
}

function checkVersions {

  if [ ! -f "${1}" -o ! -f "${2}" ]; then
    RET=-1
  else 

    local SIPVERFILE="${1}"
    getVersion $SIPVERFILE
    local SIPVERSION=$VERSION
    local SIPVERMAJOR=$VERMAJOR
    local SIPVERMINOR=$VERMINOR
    local SIPVERMINOR2=$VERMINOR2
    local SIPBUILDNUM=$BUILDNUM

    local MEDIAVERFILE="${2}"
    getVersion $MEDIAVERFILE
    local MEDIAVERSION=$VERSION
    local MEDIAVERMAJOR=$VERMAJOR
    local MEDIAVERMINOR=$VERMINOR
    local MEDIAVERMINOR2=$VERMINOR2
    local MEDIABUILDNUM=$BUILDNUM

    RET=0

    if [ "$MEDIAVERMAJOR" -gt 0 -o "$MEDIAVERMINOR" -gt 0 ]; then
      if [ "$MEDIAVERMAJOR" -lt "$SIPVERMAJOR" -o \( "$MEDIAVERMAJOR" -eq "$SIPVERMAJOR" -a \
          \( "$MEDIAVERMINOR" -lt "$SIPVERMINOR" -o \( "$MEDIAVERMINOR" -eq "$SIPVERMINOR" -a \
          \( "$MEDIAVERMINOR2" -lt "$SIPVERMINOR2" -o \( "$MEDIAVERMINOR2" -eq "$SIPVERMINOR2" -a \
          "$MEDIABUILDNUM" -lt "$SIPBUILDNUM" \) \) \) \) \)  ]; then
        RET=1
      fi
    fi

  fi

}


