#!/bin/bash
#
# This script is called by OpenVCX to get the fingerprint of an X.509 certificate file.
# [ path of .pem certificate ] [ optional output fingerprint file path ] [ optional digest ]
#
# For eg.
# openssl x509 -noout -in cert.pem -fingerprint -sha256
#

OPENSSL=openssl
CERTPATH=$1
OUTPATH=$2
DIGEST=$3

if [ "${OUTPATH}" != "" ]; then
  if [ -f "${OUTPATH}" ]; then
    rm "${OUTPATH}" 
  fi
fi

${OPENSSL} version >/dev/null
RC=$?
if [ ${RC} != 0 ]; then
  echo "OpenSSL binary file ${OPENSSL} does not exist."
  exit
fi

if [ ! -f "${CERTPATH}" ]; then
  echo "Certificate path ${CERTPATH} does not exist."
  exit
fi

if [ "${DIGEST}" == "" ]; then
  DIGEST="sha256"
fi

OUTPUT=`${OPENSSL} x509 -noout -in ${CERTPATH} -fingerprint -${DIGEST}`
FINGERPRINT=`echo "${OUTPUT}" | awk -F '=' '{print $2}'`

if [ "${OUTPATH}" != "" ]; then
  echo $FINGERPRINT > ${OUTPATH}
else
  echo $FINGERPRINT 
fi
