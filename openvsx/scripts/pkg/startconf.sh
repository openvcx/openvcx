#!/bin/bash

#
# This script is called by the SIP conferencing server to launch a media processor video conferencing process.
# [ unique instance id] [ PIP HTTP control port ] [ xcode configuration ] [ additional command line args ]
# startconf.sh instance_1234 8080 "vx=640,vy=480,vfr=30" "none"
#

#ulimit -c unlimited

MAX_STREAMS=100
MAX_RTSP=50
MAX_HTTP=${MAX_STREAMS}
MAX_RTP=${MAX_STREAMS}
XCODE_ARGS=""
XCODE_ARGS_PADDING="padAspectRatio=1,padColorRGB=0x000000"
XCODE_ARGS_DIMENSIONS="vx=320,vy=240"
XCODE_ARGS_FPS="vfr=15"
XCODE_ARGS_DEFAULT="ar=16000,"


function make_xcode_opt_vid_h264 {
  BITRATE_MULTIPLIER=.040
  XCODE_ARGS_DEFAULT+="vc=h264,vp=77,vf=3,vgop=2800,vlh=9"
}
function make_xcode_opt_vid_record_h264 {
  xcode_get_val "${XCODE_ARGS}" "vx"
  VX="${VAL}"
  if [ "${VX}" != "" ]; then
    if [ ${VX} -le 320 ]; then
      BITRATE_MULTIPLIER=.045
    else
      BITRATE_MULTIPLIER=.040
    fi
  else
    BITRATE_MULTIPLIER=.045
  fi
  XCODE_ARGS_DEFAULT+="vc=h264,vp=77,vf=3,vgop=2800,vlh=9"
}

function make_xcode_opt_vid_vp8 {
  BITRATE_MULTIPLIER=.050
  XCODE_ARGS_DEFAULT+="vc=vp8,vth=1,vthd=1,vlh=1,vf=4,vgop=1800"
}
function make_xcode_opt_vid_record_vp8 {
  BITRATE_MULTIPLIER=.060
  XCODE_ARGS_DEFAULT+="vc=vp8,vf=4,vgop=2800"
}

function make_xcode_opt_vid_mpg4 {
  BITRATE_MULTIPLIER=.060
  XCODE_ARGS_DEFAULT+="vc=mpg4,vp=0,vth=1,vthd=1,vf=4,vgop=1800"
}
function make_xcode_opt_vid_record_mpg4 {
  BITRATE_MULTIPLIER=.060
  XCODE_ARGS_DEFAULT+="vc=mpg4,vp=0,vgop=2800"
}

function make_xcode_opt_vid_h263p {
  BITRATE_MULTIPLIER=.070
  XCODE_ARGS_DEFAULT+="vc=h263p,vth=1,vthd=1,vgop=1800"
}

function make_xcode_opt_vid_h263 {
  BITRATE_MULTIPLIER=.070
  XCODE_ARGS_DEFAULT+="vc=h263,vth=1,vthd=1,vgop=1800"
}

function make_xcode_opt_vid_yuv420p {
  XCODE_ARGS_DEFAULT+=""
}


function make_xcode_bitrate {
  xcode_to_array "${XCODE_ARGS}"

  xcode_get_val2  "vb"
  local BITRATE=${VAL}

  if [ "${VAL}" == "" ]; then
    xcode_get_val2 "vx"
    VX="${VAL}"
    xcode_get_val2 "vy"
    VY="${VAL}"
    if [ "${VX}" == "" -a "${VY}" == "" ]; then
      echo "Need a resolution to create a bitrate"
      exit 0
    elif [ "${BITRATE_MULTIPLIER}" == "" ]; then
      echo "Need a bitrate multiplier to create a bitrate"
      exit 0
    fi
    # multiply the # of macro-blocks by the codec specific bitrate factor 
    local MB_TOT=`expr ${VX} \* ${VY} / 16`
    BITRATE=`echo $MB_TOT ${BITRATE_MULTIPLIER} | awk '{printf "%d", $1 * $2 }'`
  fi

  VB_TOL=`echo $BITRATE | awk '{printf "%d", $1 * .101}'`
  VBV_MAX=`echo $BITRATE | awk '{printf "%d", $1 * 1.25}'`
  VBV_BUF=${VBV_MAX}

  XCODE_BITRATE=",vb=${BITRATE},vbt=${VB_TOL},vbvmax=${VBV_MAX},vbvbuf=${VBV_BUF}"
}

function xcode_to_array {
  COUNT=0
  ARGS_AR=(`echo "$1" | tr -s ',' ' '` )
  for ARG in ${ARGS_AR[@]}; do
    KEY=`echo ${ARG} | awk -F '=' '{print $1}'`
    VAL=`echo ${ARG} | awk -F '=' '{print $2}'`
    XCODE_ARRAY_KEYS[$COUNT]=${KEY}
    XCODE_ARRAY_VALS[$COUNT]=${VAL}
    let COUNT=COUNT+1
  done
}

function xcode_get_val2 {
  VAL=""
  local INDEX=0
  local COUNT=${#XCODE_ARRAY_KEYS[@]}
  while [ ${INDEX} -lt ${COUNT} ]; do
    KEY=${XCODE_ARRAY_KEYS[$INDEX]}
    if [ "${KEY}" == "${1}" ]; then
      VAL=${XCODE_ARRAY_VALS[$INDEX]}
      break
    elif [ "${2}" != "" -a  "${KEY}" == "${2}" ]; then
      VAL=${XCODE_ARRAY_VALS[$INDEX]}
      break
    fi
    let INDEX=INDEX+1
  done
}

function xcode_get_val {
  VAL=""

  local ARGS_AR=(`echo "$1" | tr -s ',' ' '` )
  for ARG in ${ARGS_AR[@]}; do
    KEY=`echo ${ARG} | awk -F '=' '{print $1}'`
    if [ "${KEY}" == "${2}" ]; then
      VAL=`echo ${ARG} | awk -F '=' '{print $2}'`
      break
    elif [ "${3}" != "" -a  "${KEY}" == "${3}" ]; then
      VAL=`echo ${ARG} | awk -F '=' '{print $2}'`
      break
    fi

  done
}

function cmdline_get_val {
  VAL=""
  local ARG=""
  local _KEY=""
  local ARGS_AR=(`echo "$1" | tr -s ',' ' '` )

  for ARG in ${ARGS_AR[@]}; do
    _KEY=`echo ${ARG} | awk -F '=' '{print $1}'`
    if [ "${_KEY}" == "$2" ]; then
      VAL=`echo ${ARG} | awk -F '=' '{print $2}'`
      break
    fi
  done
}

function cmdline_have_key {
  KEY=""
  VAL=""
  local ARG=""
  local ARGS_AR=(`echo "$1" | tr -s ',' ' '` )

  for ARG in ${ARGS_AR[@]}; do
    _KEY=`echo ${ARG} | awk -F '=' '{print $1}'`
    _VAL=`echo ${ARG} | awk -F '=' '{print $2}'`
    if [ "${_KEY}" == "$2" ]; then
      KEY="${_KEY}"
      VAL="${_VAL}"
      break
    fi
  done

}

function cmdline_remove_key {
  CMDLINE=""
  local _KEY=""
  local _VAL=""
  local ARG=""
  local ARGS_AR=(`echo "$1" | tr -s ',' ' '` )

  for ARG in ${ARGS_AR[@]}; do
    _KEY=`echo ${ARG} | awk -F '=' '{print $1}'`
    if [ "${_KEY}" != "$2" ]; then
      CMDLINE+=" ${_KEY}"
      _VAL=`echo ${ARG} | awk -F '=' '{print $2}'`
      if [ "${_VAL}" != "" ]; then
        CMDLINE+="=${_VAL}"
      fi
    fi
  done
}

function xcode_override {

  if [ "${2}" == "" ]; then
    XCODE_ARGS="${1}"
    return
  fi

  VAL=""
  XCODE_ARGS=""
  local ARGS_AR_BASE=(`echo "${1}" | tr -s ',' ' '` )
  local ARGS_AR_OVERRIDE=(`echo "${2}" | tr -s ',' ' '` )
  local COUNT=0
  local INDEX=0
  for ARG_OVERRIDE in ${ARGS_AR_OVERRIDE[@]}; do
    KEY_OVERRIDE[$COUNT]=`echo ${ARG_OVERRIDE} | awk -F '=' '{print $1}'`
    VAL_OVERRIDE[$COUNT]=`echo ${ARG_OVERRIDE} | awk -F '=' '{print $2}'`
    FOUND[$COUNT]=0
    let COUNT=COUNT+1
  done

  for ARG in ${ARGS_AR_BASE[@]}; do
    KEY=`echo ${ARG} | awk -F '=' '{print $1}'`
    VAL=`echo ${ARG} | awk -F '=' '{print $2}'`
    INDEX=0
    while [ ${INDEX} -lt ${COUNT} ]; do
      if [ "${KEY_OVERRIDE[$INDEX]}" == "${KEY}" ]; then
        VAL="${VAL_OVERRIDE[$INDEX]}"
        FOUND[$INDEX]=1
        break
      fi
      let INDEX=INDEX+1
    done

    XCODE_ARGS+="${KEY}=${VAL},"
  done

  if [ "${3}" != "" ]; then
    return
  fi

  INDEX=0
  while [ ${INDEX} -lt ${COUNT} ]; do
    if [ ${FOUND[$INDEX]} != 1 ]; then
      XCODE_ARGS+="${KEY_OVERRIDE[$INDEX]}=${VAL_OVERRIDE[$INDEX]},"
    fi
    let INDEX=INDEX+1
  done

}

function update_default_meta_ignore {
  if [ ! -f "$1" ]; then
      echo "ignore=$2" > "$1"
  elif [ "`grep "^ignore=$2" "$1"`" == "" ]; then
      echo "ignore=$2" >> "$1"
  fi
}

function update_default_meta {
  # Add the media file to the ignore list of the directory
  local RECORD_DIR="`dirname $1`"
  local RECORD_BASENAME="`basename $1`"
  local RECORD_DEFAULT_META="${RECORD_DIR}/default.meta"

  update_default_meta_ignore  "${RECORD_DEFAULT_META}" "${RECORD_BASENAME}"
}

function post_cleanup {
  if [ -f "${PID_FILE}" ]; then
      rm "${PID_FILE}"
  fi
  if [ "${HAVE_HTTPLIVE}" != "" -a -d ./"${HTTPLIVEDIR}" ]; then
      rm -rf ./"${HTTPLIVEDIR}"
  fi

  if [ "${RECORD_FILE}" != "" -a -f "${RECORD_FILE}" ]; then
      # prevent the .mkv version from showing up in the media web portal exposed dir list
      update_default_meta "${RECORD_FILE}"
      local RECORD_MP4="${RECORD_NAME}.mp4"
      if [ -f "${FFMPEG}" -a ! -f "${RECORD_MP4}" ]; then
          echo ${FFMPEG} -v info -i "${RECORD_FILE}" -acodec copy -vcodec copy -movflags faststart "${RECORD_MP4}" >> ${LOG_STDOUT} 2>&1
          ${FFMPEG} -v info -i "${RECORD_FILE}" -acodec copy -vcodec copy -movflags faststart "${RECORD_MP4}" >> ${LOG_STDOUT} 2>&1
      fi
  fi
}

function kill_process {
    local PID_FILE=${1}
    if [ -f "${PID_FILE}" ]; then
        PID=`cat $PID_FILE`
        if [ "${PID}" != "" ]; then
            local IS_RUNNING=`ps -efw | grep ${INSTANCEID}.pid | grep ${RUNPATH} | grep ${PID} | grep -v grep`
            if [ "${IS_RUNNING}" != "" ]; then
                echo "kill ${PID}"
                kill ${PID}
                sleep 1
                IS_RUNNING=`ps -efw | grep ${INSTANCEID}.pid | grep ${RUNPATH} | grep ${PID} | grep -v grep`
                if [ "${IS_RUNNING}" != "" ]; then
                    echo "kill -9 ${PID}"
                    kill -9 ${PID}
                fi
            fi
        fi
    fi
}


#
# Proper execution starts here
#

INSTANCEID=$1
PIP_PORT=$2

XCODE_ARGS_CMDLINE=$3
if [ "${XCODE_ARGS_CMDLINE}" == "none" ]; then
  XCODE_ARGS_CMDLINE=""
fi
EXTRA_ARGS=$4
if [ "${EXTRA_ARGS}" == "none" ]; then
  EXTRA_ARGS=""
fi


DIR=`dirname $0`
DIR=`dirname $DIR`
cd ${DIR}

if [ "${INSTANCEID}" == "" ]; then
  echo "Invalid arguments."
  exit
fi

LOG_FILE=log/log_${INSTANCEID}.log
LOG_STDOUT=log/log_${INSTANCEID}.stdout
CONFERENCE_NAME=`echo $INSTANCEID | awk -F '_' '{print $1}'`

FFMPEG=./bin/ffmpeg_tn
RECORDING_BASE_DIR=recording
PID_FILE="tmp/${INSTANCEID}.pid"
RUNPATH="bin/vsxbin"
if [ ! -f "${RUNPATH}" ]; then
  RUNPATH="bin/vsx"
fi

if [ "${PIP_PORT}" == "kill" ]; then
    kill_process ${PID_FILE}
    exit 0
fi

if [ "${LOG_STDOUT}" != "" ]; then
  date > ${LOG_STDOUT}
  echo $0 $@ >> ${LOG_STDOUT}
fi


HTTPSTREAMING_PORT="${PIP_PORT}"
RTSP_PORT=`echo ${PIP_PORT} | awk '{printf "%d", $1 + 1}'`

if [ -d ./lib ]; then
  if [ `uname` = "Darwin" ]; then
    export DYLD_LIBRARY_PATH=`pwd`/lib
  else
    export LD_LIBRARY_PATH=`pwd`/lib
  fi
fi



#
# Get the xcode video codec parameter
#
xcode_get_val "${XCODE_ARGS_CMDLINE}" "vc" "videoCodec"
VID_CODEC=${VAL}

#
# Validate the video codec, and default to "yuv420p" if unknown
#
if [ "${VID_CODEC}" == "h263+" ]; then
  VID_CODEC = "h263p";
elif [ "${VID_CODEC}" != "h263" -a "${VID_CODEC}" != "h263p" -a "${VID_CODEC}" != "h264" -a "${VID_CODEC}" != "mpg4" -a "${VID_CODEC}" != "vp8" ]; then
  VID_CODEC="yuv420p"
fi

#Setup conference output recording
cmdline_have_key "${EXTRA_ARGS}" "record"
if [ "${KEY}" == "record" ]; then
  RECORD_NAME="${VAL}"
  RECORD_DIR="`dirname ${RECORD_NAME}`"
  RECORD_DEFAULT_META="${RECORD_DIR}/default.meta"

  cmdline_remove_key "${EXTRA_ARGS}" "record"
  EXTRA_ARGS="${CMDLINE}"

  # If there is no encoder codec set, default to h264
  if [ "${VID_CODEC}" == "yuv420p" ]; then
    VID_CODEC="h264"
    xcode_override "${XCODE_ARGS_CMDLINE}" "vc=${VID_CODEC}"
  fi

  if [ ! -d "${RECORD_DIR}" ]; then
    mkdir -p "${RECORD_DIR}"
  fi

  update_default_meta_ignore  "${RECORD_DEFAULT_META}" "httplive"

  #if [ "${VID_CODEC}" == "h264" ]; then
  #  RECORD_FILE="${RECORD_NAME}.flv"
  #  ARGS_RECORD="--flvrecord=${RECORD_FILE}"
  #elif [ "${VID_CODEC}" == "vp8" ]; then
    RECORD_FILE="${RECORD_NAME}.mkv"
    ARGS_RECORD="--mkvrecord=${RECORD_FILE}"
    RECORD_BASENAME="`basename ${RECORD_FILE}`"
    #EXTRA_ARGS+=" --mkvrecordavsync=.7"
    update_default_meta_ignore  "${RECORD_DEFAULT_META}" "${RECORD_BASENAME}"
  #fi
  #ARGS_AVSYNC="--avsync=1.0"

  #
  # Set the default codec specific xcode configuration
  #
  make_xcode_opt_vid_record_$VID_CODEC
  HAVE_XCODE_OPT="1"

fi


#
# Setup video layout
#
cmdline_have_key "${EXTRA_ARGS}" "--layout"
if [ "${KEY}" == "--layout" ]; then
  cmdline_remove_key "${EXTRA_ARGS}" "--layout"
  EXTRA_ARGS="${CMDLINE}"
  EXTRA_ARGS+=" "--layout="${VAL}"
fi

#
# Setup live stream output
#
cmdline_have_key "${EXTRA_ARGS}" "--tslive"
HAVE_TSLIVE=${KEY}
cmdline_have_key "${EXTRA_ARGS}" "--flvlive"
HAVE_FLVLIVE=${KEY}
cmdline_have_key "${EXTRA_ARGS}" "--mkvlive"
HAVE_MKVLIVE=${KEY}
cmdline_have_key "${EXTRA_ARGS}" "--httplive"
HAVE_HTTPLIVE=${KEY}
cmdline_have_key "${EXTRA_ARGS}" "--rtsp"
HAVE_RTSP=${KEY}
cmdline_have_key "${EXTRA_ARGS}" "--live"
HAVE_LIVE=${KEY}

cmdline_have_key "${EXTRA_ARGS}" "broadcast"
if [ "${KEY}" == "broadcast" ]; then
  cmdline_remove_key "${EXTRA_ARGS}" "broadcast"
  EXTRA_ARGS="${CMDLINE}"

  if [ "${VAL}" == "auto" ]; then
    # If there is no encoder codec set, default to h264
    if [ "${VID_CODEC}" == "yuv420p" ]; then
      VID_CODEC="h264"
      if [ "${XCODE_ARGS_CMDLINE}" == "" ]; then
        XCODE_ARGS="vc=${VID_CODEC}"
      else
        xcode_override "${XCODE_ARGS_CMDLINE}" "vc=${VID_CODEC}"
      fi
    fi
  fi

   if [ "${HAVE_TSLIVE}" == "" ]; then
     HAVE_TSLIVE="--tslive"
     if [ "${MAX_HTTP}" != "" ]; then
       HAVE_TSLIVE+=" --tslivemax=${MAX_HTTP}"
     fi
     EXTRA_ARGS+=" ${HAVE_TSLIVE}"
   fi
   if [ "${HAVE_FLVLIVE}" == "" ]; then
     HAVE_FLVLIVE="--flvlive"
     if [ "${MAX_HTTP}" != "" ]; then
       HAVE_FLVLIVE+=" --flvlivemax=${MAX_HTTP}"
     fi
     EXTRA_ARGS+=" ${HAVE_FLVLIVE}"
   fi
   if [ "${HAVE_MKVLIVE}" == "" ]; then
     HAVE_MKVLIVE="--mkvlive"
     if [ "${MAX_HTTP}" != "" ]; then
       HAVE_MKVLIVE+=" --mkvlivemax=${MAX_HTTP}"
     fi
     EXTRA_ARGS+=" ${HAVE_MKVLIVE}"
     #EXTRA_ARGS+=" --mkvavsync=.4"
   fi
   if [ "${HAVE_HTTPLIVE}" == "" ]; then
     HAVE_HTTPLIVE="--httplive"
     if [ "${MAX_HTTP}" != "" ]; then
       HAVE_HTTPLIVE+=" --httplivemax=${MAX_HTTP}"
     fi
     EXTRA_ARGS+=" ${HAVE_HTTPLIVE}"
   fi
   if [ "${HAVE_RTSP}" == "" ]; then
     HAVE_RTSP="--rtsp=${RTSP_PORT}"
     if [ "${MAX_RTSP}" != "" ]; then
       HAVE_RTSP+=" --rtspmax=${MAX_RTSP}"
     fi
     EXTRA_ARGS+=" ${HAVE_RTSP}"
   fi
   if [ "${HAVE_LIVE}" == "" ]; then
     HAVE_LIVE="--live"
     if [ "${MAX_HTTP}" != "" ]; then
       HAVE_LIVE+=" --livemax=${MAX_HTTP}"
     fi
     EXTRA_ARGS+=" ${HAVE_LIVE}"
   fi

fi

if [ "${HAVE_HTTPLIVE}" != "" ]; then
  cmdline_have_key "${EXTRA_ARGS}" "--httplivechunk"
  if [ "${KEY}" == "" ]; then
    EXTRA_ARGS+=" --httplivechunk=5"
  fi
  cmdline_have_key "${EXTRA_ARGS}" "--httplivedir"
  if [ "${KEY}" == "" ]; then
    HTTPLIVEDIR="${RECORDING_BASE_DIR}/${CONFERENCE_NAME}/httplive"
    if [ ! -d "${HTTPLIVEDIR}" ]; then
      mkdir -p "${HTTPLIVEDIR}"
    fi
    EXTRA_ARGS+=" --httplivedir=${HTTPLIVEDIR}"
  fi
fi

if [ "${HAVE_XCODE_OPT}" == "" ]; then
  #
  # Set the default codec specific xcode configuration
  #
  make_xcode_opt_vid_$VID_CODEC
  HAVE_XCODE_OPT="1"
fi

XCODE_ARGS_DEFAULT+=",${XCODE_ARGS_PADDING},${XCODE_ARGS_DIMENSIONS},${XCODE_ARGS_FPS}"


if [ "${VID_CODEC}" == "yuv420p" ]; then
  #
  # There will be no shared encoder instance
  #
  xcode_override "${XCODE_ARGS_DEFAULT}" "${XCODE_ARGS_CMDLINE}" "yuv420p"
  XCODE_ARGS_DEFAULT=${XCODE_ARGS}
  xcode_override "${XCODE_ARGS_DEFAULT}" "vc=yuv420p"

else


  #
  # override any default xcode configuration parameters with those given in the command line
  #
  xcode_override "${XCODE_ARGS_DEFAULT}" "${XCODE_ARGS_CMDLINE}" ""

  #
  # Set a reasonable bitrate given the input codec and dimensions
  #
  make_xcode_bitrate
  XCODE_ARGS+="${XCODE_BITRATE}"

fi

#
# Turn off any stream output types depending on output codecs
#
if [ "${HAVE_TSLIVE}" != "" -a "${VID_CODEC}" != "h264" -a "${VID_CODEC}" != "mpg4" ]; then
  cmdline_remove_key "${EXTRA_ARGS}" "--tslive"
  EXTRA_ARGS="${CMDLINE}"
  HAVE_TSLIVE=""
 fi
if [ "${HAVE_FLVLIVE}" != "" -a "${VID_CODEC}" != "h264" ]; then
  cmdline_remove_key "${EXTRA_ARGS}" "--flvlive"
  EXTRA_ARGS="${CMDLINE}"
  HAVE_FLVLIVE=""
fi
if [ "${HAVE_HTTPLIVE}" != "" -a "${VID_CODEC}" != "h264" ]; then
  cmdline_remove_key "${EXTRA_ARGS}" "--httplive"
  EXTRA_ARGS="${CMDLINE}"
  HAVE_HTTPLIVE=""
fi
if [ "${HAVE_MKVLIVE}" != "" -a "${VID_CODEC}" != "h264" -a "${VID_CODEC}" != "mpg4" ]; then
  cmdline_remove_key "${EXTRA_ARGS}" "--mkvlive"
  EXTRA_ARGS="${CMDLINE}"
  HAVE_MKVLIVE=""
fi

#
# Get the xcode audio codec parameter
#
xcode_get_val "${XCODE_ARGS_CMDLINE}" "ac" "audioCodec"
AUD_CODEC=${VAL}
xcode_override "${XCODE_ARGS}" "as=1"

if [ "${HAVE_TSLIVE}" != "" -o "${HAVE_FLVLIVE}" -o "${HAVE_MKVLIVE}" -o "${HAVE_HTTPLIVE}" != "" -o "${HAVE_RTSP}" ]; then
  HAVE_OUTPUT=1

  if [ "${MAX_HTTP}" != "" ]; then
      EXTRA_ARGS+=" --httpmax=${MAX_HTTP}"
  fi
fi

if [ "${MAX_RTP}" != "" ]; then
    EXTRA_ARGS+=" --rtpmax=${MAX_RTP}"
fi

if [ "${AUD_CODEC}" == "" ]; then

  if [ "${VID_CODEC}" == "yuv420p" ]; then
    xcode_override "${XCODE_ARGS}" "ac=pcm"
  elif [ "${HAVE_OUTPUT}" != "" -o "${ARGS_RECORD}" != "" ]; then
    xcode_override "${XCODE_ARGS}" "ac=aac"
  else
    xcode_override "${XCODE_ARGS}" "ac=g711a"
  fi

fi

#function demoscroll {
#  xcode_override "${XCODE_ARGS}" "padColorRGB=0xffffff"
#  xcode_override "${XCODE_ARGS}" "padBottom=40"
#  EXTRA_ARGS+=" --pipconf=piptest2.conf"
#}
#demoscroll

ARGS_DEFAULT="--rtcprr=2.0"
ARGS_DEFAULT+=" --rembxmitmaxrate=600Kbs"
#Use --rembxmitforce=1 to force sender bw into range even during good network conditions
#ARGS_DEFAULT+=" --rembxmitminrate=90Kbps --rembxmitmaxrate=100Kbs --rembxmitforce=1 --debug-remb"
#ARGS_DEFAULT+=" --rembxmit=0"
#ARGS_DEFAULT+=" --mixerself=1 --nackxmit=0 --rembxmit=0 --maxrtpvideodelay=500"
#ARGS_DEFAULT+=" --mixerself=1"
#ARGS_DEFAULT+=" --debug-dtls --debug-net"
OPT_VERBOSE="--verbose"
OPT_INPUT="--conference"
#OPT_STUN="--stunrespond"
OPT_XCODE="--xcode=\"${XCODE_ARGS}\""
OPT_EXTRA="${ARGS_DEFAULT} ${EXTRA_ARGS} ${ARGS_RECORD} ${ARGS_AVSYNC}"
OPT_PID="--pid=${PID_FILE}"
OPT_PIP="--piphttp=${PIP_PORT} --piphttpmax=4"
OPT_CONFIGHTTP="--config=${PIP_PORT}"
OPT_STATUSHTTP="--status=${PIP_PORT}"
OPT_LOG="--logfile=${LOG_FILE} --logfilemaxsize=10MB"

#echo  ./${RUNPATH} ${OPT_PID} ${OPT_VERBOSE} ${OPT_INPUT} ${OPT_STUN} ${OPT_PIP} ${OPT_CONFIGHTTP} ${OPT_STATUSHTTP} ${OPT_EXTRA} ${OPT_XCODE} ${OPT_LOG}; post_cleanup; sleep 40;exit

#
# Start a media processor  process to service the video conference
#
if [ "${LOG_STDOUT}" != "" ]; then
  date >> ${LOG_STDOUT}
  echo ./${RUNPATH} ${OPT_PID} ${OPT_VERBOSE} ${OPT_INPUT} ${OPT_STUN} ${OPT_PIP} ${OPT_CONFIGHTTP} ${OPT_STATUSHTTP} ${OPT_EXTRA} ${OPT_XCODE} ${OPT_LOG} >> ${LOG_STDOUT}
  echo "PWD=`pwd`" >> ${LOG_STDOUT}
  ./${RUNPATH} ${OPT_PID} ${OPT_VERBOSE} ${OPT_INPUT} ${OPT_STUN}  ${OPT_PIP} ${OPT_CONFIGHTTP} ${OPT_STATUSHTTP} ${OPT_EXTRA} ${OPT_XCODE} ${OPT_LOG} >> ${LOG_STDOUT} 2>&1
  date >>${LOG_STDOUT}
else
  ./${RUNPATH} ${OPT_PID} ${OPT_VERBOSE} ${OPT_INPUT} ${OPT_STUN} ${OPT_PIP} ${OPT_CONFIGHTTP} ${OPT_STATUSHTTP} ${OPT_EXTRA} ${OPT_XCODE} ${OPT_LOG} 
fi

post_cleanup

