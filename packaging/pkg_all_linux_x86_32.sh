#!/bin/sh

ARCH=linux_x86_32

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

./package/pkg_all.sh ${ARCH}
