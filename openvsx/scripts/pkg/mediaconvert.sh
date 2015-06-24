#!/bin/bash

#
# This is an example script to convert static media files using ffmpeg and OpenVSX
#

function getnosuffix {
  VAR2=${1}
  VAR=${VAR2%.*}
}
function getdirname {
  VAR=`dirname ${1}`
}


#
# Print banner
#
echo " "
echo "This script uses ffmpeg and OpenVSX to encode your video to MPEG-4 Part 10 / H.264 AVC"
echo "===================================================================="
echo " "

INPM2T=$1
if [ _"${INPM2T}" = "_" ]; then
  echo "Please specify an input video file to convert"
  exit 1
fi

if [ ! -e "${INPM2T}" ]; then
  echo "${INPM2T} does not exist."
  exit 1
fi

if [ _"${VSX}" = "_" ]; then
  VSX=../bin/vsx
fi
if [ _"${FFMPEG}" = "_" ]; then
  FFMPEG=ffmpeg
fi

NICE="nice -n19"

CWD=`pwd`
getdirname $0

cd ${VAR}


if [ ! -e "${FFMPEG}" ]; then
  FFMPEG2=`which ${FFMPEG}`
  if [ ! -e "${FFMPEG2}" ]; then
    FFMPEG2=${FFMPEG}
  fi
  if [ ! -e "${FFMPEG2}" ]; then
    echo "Cannot find ${FFMPEG}"
    echo "ffmpeg may not have been bundled with your OpenVSX distribution due to license concerns." 
    echo "To use a non-default location set the FFMPEG environment variable to your ffmpeg binary."
  
    echo "Visit http://www.ffmpeg.org to download ffmpeg for your operating system"
    echo "Ensure ffmpeg is compiled with '--enable-libx264 --enable-libfaac'"
    exit 1
  fi
  FFMPEG=${FFMPEG2}
fi;

if [ _"`${FFMPEG} 2>&1 | grep 'configuration' | grep 'libfaac'`" = "_" ]; then
  if [ _"`./${FFMPEG} 2>&1 | grep 'configuration' | grep 'libfaac'`" = "_" ]; then
    echo "It seems that ${FFMPEG} has not been built with required libfaac"
    exit 1
  else
    FFMPEG=./${FFMPEG}
  fi
fi
if [ _"`${FFMPEG} 2>&1 | grep 'configuration' | grep 'libx264'`" = "_" ]; then
  echo "It seems that ${FFMPEG} has not been built with required libx264"
  exit 1
fi

if [ ! -e "${VSX}" ]; then
  echo "Cannot find ${VSX}"
  echo "To use a non-default location set the VSX environment variable to your VSX binary."
  echo -n "Press y to proceed without OpenVSX: "
  read -r RES
  if [ _"$RES" = "_y" ]; then
    VSX=""
  else
    exit 1
  fi;
fi

VIDCODECOPT="-vcodec libx264"

#default video resolution
#VIDRESOLUTIONOPT="-s 1280x720"

#default h.264 bandwidth
#VIDBWOPT="-vb 2800k -bt 200k"

#default tv broadcast fps (from interlace of 59.94)
#VIDFPSOPT="-r 29.97"

#VIDBWOPT="-cqp 24"

X264PRESETS_BASELINE="-coder 0 -flags +loop -cmp +chroma -partitions +parti8x8+parti4x4+partp8x8+partb8x8 -me_method umh -subq 8 -me_range 16 -g 150 -keyint_min 90 -sc_threshold 40 -i_qfactor 0.71  -qcomp 0.6 -qmin 10 -qmax 51 -qdiff 4 -refs 3 -directpred 3 -trellis 1 -flags2 -wpred-dct8x8+fastpskip+mbtree"

#presets from normal.ffpreset (CAVLC)
#X264PRESETS_NORMAL="-flags +loop -cmp +chroma -partitions +parti8x8+parti4x4+partp8x8+partb8x8 -me_method hex -subq 6 -me_range 16 -g 150 -keyint_min 29 -sc_threshold 40 -i_qfactor 0.71 -b_strategy 1 -qcomp 0.6 -qmin 10 -qmax 51 -qdiff 4 -bf 3 -refs 3 -directpred 3 -trellis 0 -flags2 +wpred+dct8x8+fastpskip+mbtree -wpredp 2" 

X264PRESETS_NORMAL="-coder 1 -flags +loop -cmp +chroma -partitions +parti8x8+parti4x4+partp8x8+partb8x8 -me_method hex -subq 6 -me_range 16 -g 150 -keyint_min 29 -sc_threshold 40 -i_qfactor 0.71 -b_strategy 1 -qcomp 0.6 -qmin 10 -qmax 51 -qdiff 4 -bf 3 -refs 3 -directpred 3 -trellis 0 -flags2 +wpred+dct8x8+fastpskip+mbtree -wpredp 2" 

X264PRESENTS_MAINPROFILE="-coder 1 -flags +loop -cmp +chroma -partitions +parti8x8+parti4x4+partp8x8+partb8x8 -me_method umh -subq 8 -me_range 16 -g 150 -keyint_min 29 -sc_threshold 40 -i_qfactor 0.71 -b_strategy 2 -qcomp 0.6 -qmin 10 -qmax 51 -qdiff 4 -bf 3 -refs 4 -directpred 3 -trellis 1 -flags2 +wpred+mixed_refs+fastpskip+mbtree -wpredp 2"

#presets from hq.ffpreset (coder 1 CABAC)
X264PRESETS_HQ="-coder 1 -flags +loop -cmp +chroma -partitions +parti8x8+parti4x4+partp8x8+partb8x8 -me_method umh -subq 8 -me_range 16 -g 150 -keyint_min 29 -sc_threshold 40 -i_qfactor 0.71 -b_strategy 2 -qcomp 0.6 -qmin 10 -qmax 51 -qdiff 4 -bf 3 -refs 4 -directpred 3 -trellis 1 -flags2 +wpred+mixed_refs+dct8x8+fastpskip+mbtree -wpredp 2" 

X264_PRESETS_MAX_QMAX=39
X264PRESETS_MAX="-coder 1 -flags +loop -cmp +chroma -partitions +parti8x8+parti4x4+partp8x8+partb8x8 -me_method esa -subq 8 -me_range 16 -g 150 -keyint_min 29 -sc_threshold 40 -i_qfactor 0.71 -b_strategy 2 -qcomp 0.6 -qmin 10 -qmax "${X264_PRESETS_MAX_QMAX}" -qdiff 4 -bf 3 -refs 4 -directpred 3 -trellis 2 -flags2 +wpred+mixed_refs+dct8x8-fastpskip+mbtree -wpredp 2" 

#presets from fastfirstpass.ffpreset
X264PRESETS_FAST="-coder 1 -flags +loop -cmp +chroma -partitions +parti8x8+parti4x4+partp8x8+partp4x4+partb8x8 -me_method dia -subq 2 -me_range 16 -g 150 -keyint_min 29 -sc_threshold 40 -i_qfactor 0.71 -b_strategy 1 -qcomp 0.6 -qmin 10 -qmax 51 -qdiff 4 -bf 3 -refs 1 -directpred 3 -trellis 0 -flags2 -bpyramid-wpred-mixed_refs-dct8x8+fastpskip+mbtree -wpredp 2" 

#profile aac_main seems to encode occasionally choppy/garbled audio in current config
#AUDCODECOPT="-acodec libfaac -profile aac_main"
AUDCODECOPT="-acodec libfaac -profile aac_low"

#augment default volume from 256 default
AUDVOLUMEOPT="-vol 512"

#sync vid to audio using mp4 stts 
AUDSYNCOPT="-async 1"

#stero audio
AUDCHANNELSOPT="-ac 2"

# 2 x 64k aac output channels
AUDBWOPT="-ab 128k"

THREADOPT=""

function getresolution {

  VIDRESOLUTIONOPT=""
  RES=0
  while [ "$RES" -lt 1 -o "$RES" -gt 8 ];  do
  #echo "1 - Keep original resolution"
  echo "2 - 1920x1080"
  echo "3 - 1440x810"
  echo "4 - 1280x720"
  echo "5 - 960x540"
  echo "6 - 800x450"
  echo "7 - 640x360"
  echo "8 - 480x270"
  echo -n "Please specify an output resolution: "
  read -r RES 
  if [ _"$RES" = "_" ]; then
    RES=0
  elif [ _"$RES" = "_1" ]; then
    VIDRESOLUTIONOPT=""
    VIDBWOPT=""
  elif [ _"$RES" = "_2" ]; then
    VIDRESOLUTIONOPT="-s 1920x1080"
    VRATES="2800k 3200k 3600k 4000k 4400k 4800k 5200k 5600k 6000k 6400k 6800k 7200k 7600k"
    VTOL="500k"
    echo "Using stereo audio output"
  elif [ _"$RES" = "_3" ]; then
    VIDRESOLUTIONOPT="-s 1440x810"
    VRATES="2400k 2600k 2800k 3000k 3200k 3400k 3600k 3800k 4000k 4200k 4400k 4600k 4800k"
    VTOL="300k"
    echo "Using stereo audio output"
  elif [ _"$RES" = "_4" ]; then
    VIDRESOLUTIONOPT="-s 1280x720"
    VRATES="1500k 1700k 1900k 2100k 2300k 2500k 2700k 2900k 3100k 3300k 3500k 3700k 3900k"
    VTOL="200k"
    echo "Using stereo audio output"
  elif [ _"$RES" = "_5" ]; then
    VIDRESOLUTIONOPT="-s 960x540"
    VRATES="750k 850k 950k 1050k 1150k 1250k 1350k 1450k 1550k 1650k 1750k 1850k 1950k 2150k"
    VTOL="150k"
    echo "Using stereo audio output"
  elif [ _"$RES" = "_6" ]; then
    VIDRESOLUTIONOPT="-s 800x450"
    VRATES="500k 600k 700k 800k 900k 1000k 1100k 1200k 1300k 1400k 1500k"
    VTOL="90k"
    echo "Using stereo audio output"
  elif [ _"$RES" = "_7" ]; then
    VIDRESOLUTIONOPT="-s 640x360"
    VRATES="300k 350k 400k 450k 500k 550k 600k 650k 700k 750k 800k 850k"
    VTOL="50k"
    AUDCHANNELSOPT="-ac 1"
    AUDBWOPT="-ab 64k"
    echo "Using mono audio output"
  elif [ _"$RES" = "_8" ]; then
    VIDRESOLUTIONOPT="-s 480x270"
    VRATES="240k 260k 280k 300k 320k 340k 360k 380k 400k 420k 440k 460k 480k 500k"
    VTOL="30k"
    AUDCHANNELSOPT="-ac 1"
    AUDBWOPT="-ab 64k"
    echo "Using mono audio output"
  fi
  done

  echo " "
  if [ _"$RES" = "_1" ]; then
    echo "Using default output bitrate.  It is recommended to explictily specfiy a video resolutionusing to ensure '-vb' video bitrate is passed to the encoder." 
  fi
  
}

function getfps {

  VIDFPSOPT="-r 29.97"
  RES=0
  while [ "$RES" -lt 1 -o "$RES" -gt 5 ];  do
  echo "1 - Keep original frame rate"
  echo "2 - 59.94"
  echo "3 - 29.97"
  echo "4 - 30"
  echo "5 - 24"
  echo -n "Please specify an output frame rate: "
  read -r RES 
  if [ _"$RES" = "_" ]; then
    RES=0
  elif [ _"$RES" = "_1" ]; then
    VIDFPSOPT=""
  elif [ _"$RES" = "_2" ]; then
    VIDFPSOPT="-r 59.94"
  elif [ _"$RES" = "_3" ]; then
    VIDFPSOPT="-r 29.97"
  elif [ _"$RES" = "_4" ]; then
    VIDFPSOPT="-r 30"
  elif [ _"$RES" = "_5" ]; then
    VIDFPSOPT="-r 24"
  fi
  done

  echo " "
}

function getcbrvrate {
  RES=0
  while true; do
    IDX=1
    for ARG in ${VRATES} ; do
      echo "${IDX} - ${ARG}"
      let IDX=$IDX+1
    done
    echo -n "Please specify a video bitrate: "
    read -r RES 
   if [ _"$RES" = "_" ]; then
     RES=0
   fi
   if [ "$RES" -gt 0 -a "$RES" -lt "${IDX}" ]; then
    IDX=1
     for ARG in ${VRATES} ; do
       if [ "${IDX}" -eq "${RES}" ]; then
         break;
       fi
       let IDX=$IDX+1
     done
    VIDBWOPT="-vb ${ARG} -bt ${VTOL}"
     break 
   fi
  done
}

function getvbrvrate {
  RES=0
  while true; do
    echo "Please specify an H.264 quantizer parameter"
    echo "The encoder may adjust this parameter slightly to accomodate for "
    echo "high motion frames.  This value is based on a logarithmic scale "
    echo "in increments of 6, the lowest value being the highest quality. "
    echo "Recommended: 21 - high quality.  31 - mediaum quality "
    echo -n "Range [1-51]:"
    read -r RES 
    if [ _"$RES" = "_" ]; then
      RES=0
    elif [ "$RES" -gt 1 -o "$RES" -lt 51 ]; then
      echo $RES
      VIDBWOPT="-crf "${RES}
      break 
    fi
  done
}

function getvrate {

  RES=0
  while [ "$RES" -lt 1 -o "$RES" -gt 2 ];  do
  echo "1 - Constant bit rate (recommended for streaming)"
  echo "2 - Variable bit rate (recommended for highest quality)"
  echo -n "Please specify the encoding rate control method: "
  read -r RES 
  if [ _"$RES" = "_" ]; then
    RES=0
  elif [ _"$RES" = "_1" ]; then
    echo " "
    getcbrvrate
    break
  elif [ _"$RES" = "_2" ]; then
    echo " "
    getvbrvrate
    break
  fi
  done

  echo " "

}

function getqual {

  X264PRESETS=${X264PRESETS_HQ}
  RES=0
  while [ "$RES" -lt 1 -o "$RES" -gt 4 ];  do
  echo "1 - Caterpillar (Highest Quality qmax <= "${X264_PRESETS_MAX_QMAX}")"
  echo "2 - Cow (High Quality)"
  echo "3 - Cat (Good Quality)"
  echo "4 - Cheetah (Medium Quality)"
  echo -n "Please specify an encoding video quality / time tradeoff: "
  read -r RES 
  if [ _"$RES" = "_" ]; then
    RES=0
  elif [ _"$RES" = "_1" ]; then
    X264PRESETS=${X264PRESETS_MAX}
  elif [ _"$RES" = "_2" ]; then
    X264PRESETS=${X264PRESETS_HQ}
  elif [ _"$RES" = "_3" ]; then
    X264PRESETS=${X264PRESETS_NORMAL}
  elif [ _"$RES" = "_4" ]; then
    X264PRESETS=${X264PRESETS_FAST}
  fi
  done

  echo " "
}

function getthreadopt {
  CPUINFO="/proc/cpuinfo"
  THREADOPT=""

  if [ _"`uname`" == "_Darwin" ]; then

    echo "Defaulting to 4 threads"
    THREADOPT=" -threads 4"

  elif [ _"`uname`" == "_Linux" ]; then

    if [ -f ${CPUINFO} ]; then
      CNT=`cat ${CPUINFO} | grep processor | wc -l`
      let CNT=${CNT}*2
      if [ ${CNT} -ge 2 ]; then
        THREADOPT=" -threads "${CNT}
      fi
    fi 

  fi

}

${FFMPEG} -i ${INPM2T} 
echo " "
echo "===================================================================="
getresolution
echo " "
getfps
echo " "
getqual
echo " "
getvrate
echo " "

getthreadopt

VIDOPT="${VIDCODECOPT} ${VIDRESOLUTIONOPT} ${VIDBWOPT} ${VIDFPSOPT} ${THREADOPT} ${X264PRESETS}"
AUDOPT="${AUDCODECOPT} ${AUDCHANNELSOPT} ${AUDBWOPT} ${AUDSYNCOPT} ${AUDVOLUMEOPT}"
#echo ${VIDOPT} ${AUDOPT}


OUTMP4=$2
if [ _"${OUTMP4}" = "_" ]; then
  getnosuffix ${INPM2T}
  OUTMP4=${VAR}.mp4
  if [ -e "${OUTMP4}" ]; then
    echo "Output ${OUTMP4} already exists.  Specify explicit output argument to overwrite"
    exit 1
  fi
fi

TMPOUT=${OUTMP4}"_tmp"
TMPOUT_A=${TMPOUT}.aac
TMPOUT_V=${TMPOUT}.h264
TMPOUT_STTS=${TMPOUT}.stts
TMPOUT_V_MP4=${TMPOUT}.mp4


echo "Will create ${OUTMP4}"

echo ${NICE} ${FFMPEG} -i ${INPM2T} ${AUDOPT} ${VIDOPT} -f mp4 ${TMPOUT_V_MP4}
${NICE} ${FFMPEG} -i ${INPM2T} ${AUDOPT} ${VIDOPT} -f mp4 ${TMPOUT_V_MP4}

if [ ! -e "${TMPOUT_V_MP4}" ]; then
  echo "Unable to create ${TMPOUT_V_MP4}"
  exit 1
fi

if [ _"${VSX}" != "_" ]; then

  echo ${VSX} --dump=${TMPOUT_V_MP4} --stts 
  ${NICE} ${VSX} --dump=${TMPOUT_V_MP4} --stts > ${TMPOUT_STTS}
  echo ${VSX} --extract=${TMPOUT_V_MP4} --out=${TMPOUT}
  ${NICE} ${VSX} --extract=${TMPOUT_V_MP4} --out=${TMPOUT}
  rm ${TMPOUT_V_MP4}

  echo ${VSX} --create=${TMPOUT_V} --stts=${TMPOUT_STTS} --out=${OUTMP4}
  ${NICE} ${VSX} --create=${TMPOUT_V} --stts=${TMPOUT_STTS} --out=${OUTMP4}
  echo ${VSX} --create=${TMPOUT_A} --stts=${TMPOUT_STTS} --out=${OUTMP4}
  ${NICE} ${VSX} --create=${TMPOUT_A} --stts=${TMPOUT_STTS} --out=${OUTMP4}

  rm ${TMPOUT_V}
  rm ${TMPOUT_A}
  rm ${TMPOUT_STTS}

else
  mv ${TMPOUT_V_MP4} ${OUTMP4}
  echo "Created "${OUTMP4}
fi

exit 0
