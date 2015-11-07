#!/bin/bash

#
# This is a wrapper script used to start ${VSX_HOME}/bin/vsxbin
#

function checkvsx {
  VSX_HOME=$1
  VSX_PATH="${VSX_HOME}/${VSXBIN}"
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
    VSXBIN="bin/${VSXBIN}"
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
    echo "Unable to find your VSX installation directory."
    echo "Please set VSX_HOME or run this script from your OpenVSX installation directory."
    exit 0
  fi
}

VSXBIN="vsxbin"
VSX_PATH=""

findvsx
if [ "${VSX_HOME}" != "" ]; then
  cd ${VSX_HOME}
  VSX_HOME=`pwd`
fi


if [ -d ${VSX_HOME}/lib ]; then
  if [ `uname` = "Darwin" ]; then
    export DYLD_LIBRARY_PATH="${VSX_HOME}/lib"
  else
    export LD_LIBRARY_PATH="${VSX_HOME}/lib"
  fi

fi

#echo ./${VSXBIN} "$@"
./${VSXBIN} "$@"
