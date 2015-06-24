#!/bin/bash

ARCH=$1
SELFEXTRACTING_OUTPUT=$2
if [ "${1}" = "" -a "${2}" = "" ]; then
  ARCH="unknown"
fi

if [ "${ARCH}" == "unknown" ]; then
  if [ `uname` == "Darwin" ]; then
    ARCH=mac_x86_64
  else
    ARCH=linux_x86_32
  fi
fi

if [ "${ARCH}" != "mac_x86_64" -a "${ARCH}" != "linux_x86_32" ]; then
  echo "Unknown system architecture ${ARCH}"
  exit
else
  echo "Using system architecture ${ARCH}"
fi

if [ "${SELFEXTRACTING_OUTPUT}" != "" ]; then
  SELFEXTRACTING_OUTPUT="`pwd`/${SELFEXTRACTING_OUTPUT}"
fi

if [ ! -d ./openvsx ]; then
  cd ../
  if [ ! -d ./openvsx ]; then
    cd ../
  fi
fi
if [ ! -d ./openvsx ]; then
  echo "Please run from root project directory!"
  exit
fi

SCRIPT_NAME=pkg_vsx_${ARCH}.sh
SCRIPT_PKG="./packaging/$SCRIPT_NAME"

if [ "${SCRIPT_PKG}" != "" ]; then
  echo ""
  echo "Running ${SCRIPT_PKG} ${SELFEXTRACTING_OUTPUT}"
  ${SCRIPT_PKG} "${SELFEXTRACTING_OUTPUT}"
  if [ "$?" != 0 ]; then
    exit 
  fi
fi

