#!/bin/bash

#
# This script is used to start/stop the OpenVSX Video Streaming Processor Web Portal component
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
      local PWD=`pwd`
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
    echo "Unable to find your OpenVSX (Video Streaming Processor) installation directory."
    echo "Please set VSX_HOME or run this script from your OpenVSX installation directory."
    exit 0
  fi
}


VSX="vsx-wpbin"
PROG=${VSX}
PROG_DESCR="OpenVSX Media Web Portal"
VSX_PATH=""
RESTART=1

findvsx
if [ "${VSX_HOME}" != "" ]; then
  cd "${VSX_HOME}"
  VSX_HOME=`pwd`
fi

if [ -d ${VSX_HOME}/lib ]; then
  if [ `uname` = "Darwin" ]; then
    export DYLD_LIBRARY_PATH="${VSX_HOME}/lib"
  else
    export LD_LIBRARY_PATH="${VSX_HOME}/lib"
  fi

fi

PROGSERVER_OUT=/dev/null
PROGPATH="${VSX_HOME}/bin/${PROG}"  


function getpid {
  local ARG=$1
  PID=""
  PIDPARENT=""

  local PSRES=`ps -efw | grep ${ARG} | grep -v grep`
  local PSRESC=`echo ${PSRES} | awk -F "${ARG}" '{print $2}' | head -c1`

  if [ "${PSRESC}" == "" -o "${PSRESC}" == " " ]; then
    PID=`echo ${PSRES} | awk '{print $2}'`
    PIDPARENT=`echo ${PSRES} | awk '{print $3}'`
  fi
  #echo "PID:$PID"
  #echo "PIDPARENT:$PIDPARENT"

}

function killpid {
  local SIG=$1
  local PID=$2

  if [ "${PID}" != "" ]; then
    #echo kill -${SIG} ${PID}
    kill -${SIG} ${PID}
  fi

}

function killproc {
  local ARG=$1
  if [ "${ARG}" == "" ]; then
    return 
  fi
  getpid ${ARG}
  if [ "${PID}" == "" ]; then
    return 
  fi

  echo "Stopping ${PROG_DESCR}"
  killpid "TERM" "${PIDPARENT}" 
  killpid "INT" "${PID}" 
  sleep 1
  getpid ${ARG} 
  if [ "${PID}" == "" ]; then
    return 
  fi

  killpid "TERM" "${PIDPARENT}" 
  killpid "TERM" "${PID}"
  sleep 1
  getpid ${ARG} 
  if [ "${PID}" == "" ]; then
    return 
  fi

  echo "kill -9 ${ARG}"
  killpid "KILL" "${PIDPARENT}"
  killpid "KILL" "${PID}"
  getpid ${ARG}
  if [ "${bPID}" == "" ]; then
    return 
  fi

  echo "Unable to kill ${ARG}"
}

function killprog {
  killproc ${PROG}
}

function sighandler_restart {
  echo "${PROG_DESCR} caught signal.  Exiting."
  RESTART=-1
}

function watchdogprog {
  local PROGPATH=$1
  local _ARGS=$2
  local PROGSERVER_OUT=$3

  #trap  sighandler_restart SIGTERM SIGINT SIGKILL
  #if [ "$?" != "0" ]; then
  #  RESTART=0
  #fi

  while [ "${RESTART}" -ge 0 ]; do

    if [ "${RESTART}" -gt 1 ]; then
      echo ""
      echo "Restarting ${PROG_DESCR}"
    fi

    ${PROGPATH} ${_ARGS} > ${PROGSERVER_OUT}

    if [ "${RESTART}" -ge 1 ]; then
      let RESTART=RESTART+1
      sleep 1
    fi 

  done

}

function startprog {

  local CONF="${VSX_HOME}/etc/vsxwp.conf"
  local MEDIADIR=`grep 'media=' "${CONF}" | grep -v '#' | awk -F'=' '{print $2}'`

  if [ "${MEDIADIR}" == "" ]; then
      echo "Media directory has not been set."
      echo "Please specify your media directory by modifying the line 'media=' in '${CONF}'"
      exit 0
  elif [ ! -d "${MEDIADIR}" ]; then
      echo "Media directory ${MEDIADIR} does not exist."
      exit 0
  fi

  #ulimit -c unlimited

  getpid ${PROG}
  PROGPID=${PID}

  if [ "${PROGPID}" != "" ]; then
      echo "${PROG_DESCR} is already running"
      exit 0
  fi

  echo -n "Starting ${PROG_DESCR}"
  if [ "${ARGS}" != "" ]; then
    echo -n " with arguments ${ARGS}"
  fi
  echo ""

  watchdogprog "${PROGPATH}" "${ARGS}" "${PROGSERVER_OUT}"

  #  local LISTEN=`grep 'listen=' "${CONF}" | grep -v '#' | awk -F'=' '{print $2}' | tr '\n' ' '`
  #  echo "${PROG_DESCR} is listening on ${LISTEN}"

}

function statusprog {
  getpid $PROG
  PROGPID=$PID
}

case "$1" in

  start)

    trap  sighandler_restart SIGTERM SIGINT SIGKILL

    shift
    ARGS=$@

    startprog sync

    ;;

  #start)

    #trap  sighandler_restart SIGTERM SIGINT SIGKILL

#    SCRIPT_NAME=$0
#    shift
#    ARGS=$@

#    if [ ! -f "${SCRIPT_NAME}" ]; then
#      SCRIPT_NAME="`basename ${SCRIPT_NAME}`"
#      if [ ! -f "${SCRIPT_NAME}" ]; then
#        SCRIPT_NAME="bin/${SCRIPT_NAME}"
#      fi
#    fi
#    "${SCRIPT_NAME}" "startfg" "${ARGS}" &

#    ;;

  restart)

    killprog

    shift
    ARGS=$@

    startprog
    ;;

  stop)

    killprog

    ;;

  status)

    getpid $PROG
    PROGPID=$PID
    if [ "${PROGPID}" != "" ]; then
        echo "${PROG_DESCR} is running."
    else
        echo "${PROG_DESCR} not running."
    fi

    ;;

   *)
    echo "${PROG_DESCR} Control"
    echo "Usage: $0 { start | stop | restart | status } < optional arguments >"
    exit 1
esac


exit 0

