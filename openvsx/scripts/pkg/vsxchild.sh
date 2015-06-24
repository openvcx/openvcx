#!/bin/bash
#
# This script is automatically called by vsx-wp to launch a vsx media process
#
# vsxchild.sh [ instance id ] [ input path ] [ device profile name ] [ streaming methods ] [ xcode config ] [ deprecated --in=... ]
#

INSTANCEID=$1
MEDIAPATH=$2

if [ "$MEDIAPATH" == "conference" ]; then
  DEVICENAME="conference"
  INCAPTURE="--conference"
  METHODS=$3
  XCODE_ARGS=$4
  if [ "${XCODE_ARGS}" == "none" ]; then
    XCODE_ARGS=""
  fi
  EXTRA_ARGS=$5
  if [ "${EXTRA_ARGS}" == "none" ]; then
    EXTRA_ARGS=""
  fi
  OPT_EXTRA="--rtcprr=2.0 ${EXTRA_ARGS}"
  LOG_FILE=log/log_${INSTANCEID}.log
  #LOG_STDOUT=log/log_${INSTANCEID}.stdout
  LOG_STDOUT=/dev/null
  OPT_CONF=""
else
  DEVICENAME=$3
  METHODS=$4
  XCODE_ARGS=$5
  INCAPTURE=$6
  OPT_CONF="--conf=etc/vsx.conf"
  OPT_EXTRA="--statusmax=2"
fi

OPT_VERBOSE="-v"
OPT_METHOD=
OPT_XCODE=
PID_FILE="tmp/${INSTANCEID}.pid"
RUNPATH="bin/vsxbin"
if [ ! -f "${RUNPATH}" ]; then
  RUNPATH="bin/vsx"
fi
HTTPLIVE_HOME="html/httplive"
DASH_HOME="html/dash"
if [ "${LOG_FILE}" == "" ]; then
  LOG_FILE=log/logout_${INSTANCEID}.log
fi
if [ "${LOG_STDOUT}" == "" ]; then
  LOG_STDOUT=/dev/null
fi
DIR_HTTPLIVE=
DIR_DASH=
XCODE_ARGS_DEV=""
DEV_UNKNOWN=

#XCODE_ARGS_COMMON="vgmin=2000,vgmax=3000,"

function make_xcode_opt_ipad {
  XCODE_ARGS_DEV+=""
}

function make_xcode_opt_iphone4 {
  XCODE_ARGS_DEV+=""
}

function make_xcode_opt_iphone3 {
  XCODE_ARGS_DEV+=""
}

function make_xcode_opt_android {
  XCODE_ARGS_DEV+=""
}

function make_xcode_opt_vlc {
  XCODE_ARGS_DEV+=""
}

function make_xcode_opt_unknown {
  XCODE_ARGS_DEV+=""
}
function make_xcode_opt_conference {
  # will use command line args to override any default values defined here
  XCODE_ARGS_DEV=${XCODE_ARGS}
  XCODE_ARGS="vc=h264,vb=300,vbt=30,vbvbuf=400,vbvmax=300,vp=66,vth=1,vlh=1,vf=4,vgmin=1200,vgmax=1800,vx=320,vy=240,vfr=15,ac=aac,ar=16000,as=1,padAspectRatio=1,padColorRGB=0x000000"
}

function make_xcode_opt {
  OPT_XCODE=""

  if [ "${DEV_UNKNOWN}" == "" ]; then
    make_xcode_opt_$DEVICENAME
  fi

  ARGS_AR_DEV=(`echo "${XCODE_ARGS_DEV}" | tr -s ',' ' '` )
  ARGS_AR=(`echo "${XCODE_ARGS}" | tr -s ',' ' '` )

  # replace command specific xcode args with the ones from make_xcode_opt_... 
  for ARG in ${ARGS_AR[@]}; do
    KEY=`echo ${ARG} | awk -F '=' '{print $1}'`
    VAL=`echo ${ARG} | awk -F '=' '{print $2}'`

    for ARG_DEV in ${ARGS_AR_DEV[@]}; do
      KEY_DEV=`echo ${ARG_DEV} | awk -F '=' '{print $1}'`
      if [ "${KEY_DEV}" = "${KEY}" ]; then
        VAL=`echo ${ARG_DEV} | awk -F '=' '{print $2}'`
        break
      fi
    done

    OPT_XCODE+=${KEY}"="${VAL}","
  done

  # check if any extra line xcode args are given
  for ARG_DEV in ${ARGS_AR_DEV[@]}; do
    KEY_DEV=`echo ${ARG_DEV} | awk -F '=' '{print $1}'`
    FOUND=0
    for ARG in ${ARGS_AR[@]}; do
      KEY=`echo ${ARG} | awk -F '=' '{print $1}'`
      if [ "${KEY_DEV}" = "${KEY}" ]; then
        FOUND=1
        break
      fi
    done
    if [ "$FOUND" = "0" ]; then
      VAL=`echo ${ARG_DEV} | awk -F '=' '{print $2}'`
      OPT_XCODE+=${KEY_DEV}"="${VAL}","
    fi
  done

  if [ "${OPT_XCODE}" != "" ]; then
    OPT_XCODE="--xcode=\"${OPT_XCODE}\""
  fi

}

function make_dev_opt {

  AR_METHODS=(`echo "${METHODS}" | tr -s ',' ' '` )
  for ARG in ${AR_METHODS[@]}; do

    METHOD=`echo ${ARG} | awk -F '=' '{print $1}'`
    LISTENER=`echo ${ARG} | awk -F '=' '{print $2}'`

    if [ "${LISTENER}" != "" ]; then
      LISTENER="=${LISTENER}"
    fi

    if [ "${METHOD}" = "httplive" ]; then
      OPT_METHOD+=" --httplive${LISTENER} "
      DIR_HTTPLIVE="${HTTPLIVE_HOME}/${INSTANCEID}"
      OPT_METHOD+=" --httplivechunk=5 --httplivedir=${DIR_HTTPLIVE}"
    elif [ "${METHOD}" = "dash" ]; then
      OPT_METHOD+=" --dash${LISTENER}"
      DIR_DASH="${DASH_HOME}/${INSTANCEID}"
      OPT_METHOD+=" --dashdir=${DIR_DASH}"
    elif [ "${METHOD}" = "tslive" ]; then
      OPT_METHOD+=" --tslive${LISTENER}"
    elif [ "${METHOD}" = "flvlive" ]; then
      OPT_METHOD+=" --flvlive${LISTENER}"
    elif [ "${METHOD}" = "mkvlive" ]; then
      OPT_METHOD+=" --mkvlive${LISTENER}"
    elif [ "${METHOD}" = "rtsp" ]; then
      OPT_METHOD+=" --rtsp${LISTENER}"
    elif [ "${METHOD}" = "rtmp" ]; then
      OPT_METHOD+=" --rtmp${LISTENER}"
    elif [ "${METHOD}" = "live" ]; then
      OPT_METHOD+=" --live${LISTENER}"
    elif [ "${METHOD}" = "status" ]; then
      OPT_METHOD+=" --status${LISTENER}"
    elif [ "${METHOD}" = "pip" ]; then
      OPT_METHOD+=" --piphttp${LISTENER} --piphttpmax=4 --tslive"
    fi

  done

}

function verify_dev_method {
  AR_DEVICES="ipad iphone4 iphone3 android vlc unknown conference"

  FOUND=0
  for ARG in ${AR_DEVICES[@]}; do
    if [ "${DEVICENAME}" = "${ARG}" ]; then
      FOUND=1
      break
    fi 
  done

  if [ "$FOUND" = "0" ]; then
    echo "Unrecognized device name $DEVICENAME" 
    DEV_UNKNOWN=1
  fi;

}

function pre_create {

  if [ "${DIR_HTTPLIVE}" != "" -a ! -d "${DIR_HTTPLIVE}" ]; then
    mkdir "${DIR_HTTPLIVE}"
  fi

  if [ "${DIR_DASH}" != "" -a ! -d "${DIR_DASH}" ]; then
    mkdir "${DIR_DASH}"
  fi
}

function post_cleanup {
  if [ -d "${DIR_HTTPLIVE}" ]; then
    rm -r "${DIR_HTTPLIVE}"
  fi
  if [ -d "${DIR_DASH}" ]; then
    rm -r "${DIR_DASH}"
  fi
  if [ -f "${PID_FILE}" ]; then
    rm "${PID_FILE}"
  fi
}

DIR=`dirname $0`
DIR=`dirname $DIR`
cd ${DIR}

if [ -d ./lib ]; then
  if [ `uname` = "Darwin" ]; then
    export DYLD_LIBRARY_PATH=`pwd`/lib
  else
    export LD_LIBRARY_PATH=`pwd`/lib
  fi
fi


if [ "${INSTANCEID}" == "" -o "${DEVICENAME}" == "" ]; then
  echo "Invalid arguments."
  exit
fi


if [ "${INCAPTURE}" == "" -a ! -f "${MEDIAPATH}" ]; then
  echo "${MEDIAPATH} does not exist."
  exit
fi

verify_dev_method "${DEVICENAME}"
make_dev_opt
make_xcode_opt "${XCODE_ARGS}"


if [ "${INCAPTURE}" == "" ]; then
  OPT_INPUT="--in=\"${MEDIAPATH}\""
else
  OPT_INPUT="${INCAPTURE}"
fi

pre_create

#date >${LOG_STDOUT}
echo "Running ./${RUNPATH} --pid=${PID_FILE} ${OPT_VERBOSE} ${OPT_CONF} ${OPT_METHOD} ${OPT_INPUT} ${OPT_EXTRA} ${OPT_XCODE} --log=${LOG_FILE}" 
./${RUNPATH} --pid=${PID_FILE} ${OPT_VERBOSE} ${OPT_CONF} ${OPT_METHOD} ${OPT_INPUT} ${OPT_EXTRA} ${OPT_XCODE} --log=${LOG_FILE} >${LOG_STDOUT} 2>&1
#date >>${LOG_FILE}

post_cleanup

