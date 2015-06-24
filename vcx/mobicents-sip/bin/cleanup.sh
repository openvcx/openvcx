#!/bin/bash
#
# This script is used by OpenVCX to perform periodic cleanup duties
# It can be called from cron or startvcx.sh
#


SIP_CONFERENCE="sip-conference"
SIP_CONFERENCE_CONF="./conf/${SIP_CONFERENCE}.conf"
EXPIRE_DAYS=$1

if [ "${EXPIRE_DAYS}" == "" ]; then
    EXPIRE_DAYS=7
fi

EXPIRE_DAYS_RECORD=${EXPIRE_DAYS}
EXPIRE_DAYS_LIVE=${EXPIRE_DAYS}

PWD=`pwd`
TEMP_DIR="`grep '^conference.temp.dir' "${SIP_CONFERENCE_CONF}" | awk -F'=' '{print $2}'`"

if [ "${TEMP_DIR}" == "" -o ! -d "${TEMP_DIR}" ]; then
    TEMP_DIR="./temp"
fi

if [ ! -d "${TEMP_DIR}" ]; then
    echo "${TEMP_DIR} does not exist"
    exit
fi

echo "Cleaning up temporary files in ${TEMP_DIR}"
find "${TEMP_DIR}" -path '*record' -mtime +${EXPIRE_DAYS_RECORD} -exec rm \{\} \;
find "${TEMP_DIR}" -path '*live' -mtime +${EXPIRE_DAYS_LIVE} -exec rm \{\} \;
