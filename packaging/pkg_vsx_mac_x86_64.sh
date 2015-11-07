#!/bin/bash

. ./packaging/pkg_common.sh

getvsxvermajmin
VSXVERSION=$VAR
if [ "_" = _"${VSXVERSION}" ]; then
  echo "Unable to retrieve version string"
  exit 1
fi

getvsxbuildnum
VSXBUILDNUM=$VAR
if [ "_" = _"${VSXBUILDNUM}" ]; then
  echo "Unable to retrieve build number"
  exit 1
fi


SELFEXTRACTING_OUTPUT=$1
ARCH=mac_x86_64
DIRNAME=${ARCH}
OUTIN=vsx
OUT=vsx
OUTDIR=openvsx

if [ -f "bin_${DIRNAME}/${OUT}lite" ]; then
  LITE_VERSION="lite"
else
  LITE_VERSION=""
fi


CWD=`pwd`
#getdirname $0

if [ ! -d ./dist/${DIRNAME} ]; then
  mkdir -p ./dist/${DIRNAME}
fi

cd ./dist/${DIRNAME}


if [ -d ./${OUTDIR} ]; then
  rm -rf ./${OUTDIR}
fi

if [ ! -d ${OUTDIR} ]; then
  mkdir ${OUTDIR}
fi
if [ ! -d ${OUTDIR}/bin ]; then
  mkdir ${OUTDIR}/bin
fi
if [ ! -d ${OUTDIR}/lib ]; then
  mkdir ${OUTDIR}/lib
fi
if [ ! -d ${OUTDIR}/etc ]; then
  mkdir ${OUTDIR}/etc
fi
if [ ! -d ${OUTDIR}/tmp ]; then
  mkdir ${OUTDIR}/tmp
fi
if [ ! -d ${OUTDIR}/doc ]; then
  mkdir ${OUTDIR}/doc
fi
if [ ! -d ${OUTDIR}/html ]; then
  mkdir ${OUTDIR}/html
fi
if [ ! -d ${OUTDIR}/html/img ]; then
  mkdir ${OUTDIR}/html/img
fi
if [ ! -d ${OUTDIR}/html/rsrc ]; then
  mkdir ${OUTDIR}/html/rsrc
fi
if [ ! -d ${OUTDIR}/recording ]; then
  mkdir ${OUTDIR}/recording
fi
if [ ! -d ${OUTDIR}/log ]; then
  mkdir ${OUTDIR}/log
fi


SRCDIR=../../${OUTDIR}
ZIPOUT=${OUTDIR}${LITE_VERSION}_${ARCH}_${VSXVERSION}.tar.gz
EXENAME=${OUT}bin
EXEMGR=${OUT}-wpbin
EXEIPCNAME=${OUT}-srv
EXEXCODENAME=${OUT}-xcode
EXELIB=${SRCDIR}/bin_${ARCH}/lib${OUT}${LITE_VERSION}.so
FFMPEGTN=ffmpeg_tn_${ARCH}
EXESRCPATH=${SRCDIR}/bin_${ARCH}
EXEIPCSRCPATH=${SRCDIR}/binipc_${ARCH}
EXESRC=${EXESRCPATH}/${EXENAME}${LITE_VERSION}
EXEMGRSRC=${EXESRCPATH}/${EXEMGR}${LITE_VERSION}
EXEIPCSRC=${EXEIPCSRCPATH}/${EXENAME}${LITE_VERSION}
EXEXCODESRC=${SRCDIR}/xcoder/bin_${ARCH}/${EXEXCODENAME}
EXEXCODELIB=${SRCDIR}/xcoder/bin_${ARCH}/libxcode.so
LIBCRYPTO=libcrypto.1.0.0.dylib
LIBCRYPTOPATH=${SRCDIR}/../third-party/${ARCH}/openssl-1.0.1e/lib/${LIBCRYPTO}
LIBSSL=libssl.1.0.0.dylib
LIBSSLPATH=${SRCDIR}/../third-party/${ARCH}/openssl-1.0.1e/lib/${LIBSSL}
LIBX264SO=libx264.so
LIBX264PATH=${SRCDIR}/../third-party/${ARCH}/libx264
LIBSILKSO=libSKP_SILK_SDK.so
LIBSILKPATH=${SRCDIR}/../third-party/${ARCH}/silk/${LIBSILKSO}
LIBFAACSO=libfaac.so
LIBFAACPATH=${SRCDIR}/../third-party/${ARCH}/faac-1.28/${LIBFAACSO}
LIBOPUSSO=libopus.0.dylib
LIBOPUSSO_FLOATPOINT=libopus_float.0.dylib
LIBOPUSSO_FIXEDPOINT=libopus_fixedpoint.0.dylib
LIBOPUSPATH=${SRCDIR}/../third-party/${ARCH}/opus-1.0.3/lib


if [ ! -e ${EXESRC} ]; then
  echo "${EXESRC} not found."
  exit 1
fi

if [ ! -e ${EXEMGRSRC} ]; then
  echo "${EXEMGRSRC} not found."
  exit 1
fi

#if [ ! -e ${EXEIPCSRC} ]; then
#  echo "${EXEIPCSRC} not found."
#  exit 1
#fi

cp ${EXESRC} ${OUTDIR}/bin/${EXENAME}
cp ${EXEMGRSRC} ${OUTDIR}/bin/${EXEMGR}
#cp ${EXESRCPATH}/../include/${OUTIN}lib.h ${OUTDIR}/include/${OUT}lib.h
#cp ${EXEIPCSRC} ${OUTDIR}/bin/${EXEIPCNAME}
#cp ${EXEXCODESRC} ${OUTDIR}/bin/${EXEXCODENAME}
cp ${EXELIB} ${OUTDIR}/lib/


#cp ${SRCDIR}/scripts/pkg/broadcastctrl.sh ${OUTDIR}/bin/broadcastctrl.sh
cp ${SRCDIR}/scripts/pkg/examplestart.sh ${OUTDIR}/bin/examplestart.sh
cp ${SRCDIR}/scripts/pkg/${OUTIN}wrapper.sh ${OUTDIR}/bin/${OUT}wrapper.sh
cp ${SRCDIR}/scripts/pkg/start${OUTIN}wp.sh ${OUTDIR}/bin/start${OUT}wp.sh
cp ${SRCDIR}/scripts/pkg/${OUTIN}child.sh ${OUTDIR}/bin/${OUT}child.sh
cp ${SRCDIR}/etc/${OUTIN}${ARCH}.conf ${OUTDIR}/etc/${OUT}.conf
cp ${SRCDIR}/etc/${OUTIN}wp${ARCH}.conf ${OUTDIR}/etc/${OUT}wp.conf
cp ${SRCDIR}/etc/devices.conf ${OUTDIR}/etc/devices.conf
cp ${SRCDIR}/etc/pip.conf ${OUTDIR}/etc/pip.conf
cp ${SRCDIR}/etc/xcode.conf ${OUTDIR}/etc/xcode.conf
cp ${SRCDIR}/etc/cacert.pem ${OUTDIR}/etc/cacert.pem
cp ${SRCDIR}/etc/dtlscert.pem ${OUTDIR}/etc/dtlscert.pem
cp ${SRCDIR}/etc/privkey.pem ${OUTDIR}/etc/privkey.pem
cp ${SRCDIR}/etc/dtlskey.pem ${OUTDIR}/etc/dtlskey.pem
cp ${SRCDIR}/html/*.html ${OUTDIR}/html/
#cp ${SRCDIR}/html/*.js ${OUTDIR}/html/
#cp ${SRCDIR}/html/*.css ${OUTDIR}/html/
cp -R ${SRCDIR}/html/img/* ${OUTDIR}/html/img/
cp -R ${SRCDIR}/html/rsrc/* ${OUTDIR}/html/rsrc/
cp -R ${SRCDIR}/scripts/pkg/${FFMPEGTN} ${OUTDIR}/bin/ffmpeg_tn
cp ${SRCDIR}/scripts/pkg/${OUTIN}_license.txt ${OUTDIR}/LICENSE.txt
cp -R ${SRCDIR}/scripts/pkg/mediaconvert.sh ${OUTDIR}/bin/mediaconvert.sh
mkdir ${OUTDIR}/html/httplive
mkdir ${OUTDIR}/html/dash
if [ "${LITE_VERSION}" == "" ]; then
  cp ${SRCDIR}/scripts/pkg/startconf.sh ${OUTDIR}/bin/startconf.sh
  cp ${EXEXCODELIB} ${OUTDIR}/lib/
  cp ${LIBCRYPTOPATH} ${OUTDIR}/lib/
  cp ${LIBSSLPATH} ${OUTDIR}/lib/

  have_xcode_define "XCODE_HAVE_VP8"
  if [ "$?" == "1" ]; then
    cp ${SRCDIR}/../third-party/${ARCH}/libvpx/LICENSE ${OUTDIR}/doc/vpx_license.txt
  fi
  have_xcode_define "XCODE_HAVE_VORBIS"
  if [ "$?" == "1" ]; then
    cp ${SRCDIR}/../third-party/${ARCH}/libvorbis-1.3.2/COPYING ${OUTDIR}/doc/vorbis_license.txt
  fi
  have_xcode_define "XCODE_HAVE_OPUS"
  if [ "$?" == "1" ]; then
    cp ${SRCDIR}/../third-party/${ARCH}/opus-1.0.3/COPYING ${OUTDIR}/doc/opus_license.txt
  fi
  have_xcode_define "XCODE_HAVE_SPEEX"
  if [ "$?" == "1" ]; then
    cp ${SRCDIR}/../third-party/${ARCH}/speex-1.2rc1/COPYING ${OUTDIR}/doc/speex_license.txt
  fi
  have_config_val "CFG_HAVE_SRTP"
  if [ "$?" == "1" ]; then
    cp ${SRCDIR}/../third-party/${ARCH}/srtp/LICENSE  ${OUTDIR}/doc/srtp_license.txt
  fi
  have_config_val "CFG_HAVE_PIP_AUDIO"
  if [ "$?" == "1" ]; then
    cp ${SRCDIR}/../third-party/${ARCH}/webrtc/LICENSE  ${OUTDIR}/doc/webrtc_license.txt
  fi
  have_config_val "CFG_OPENSSL_SSL"
  if [ "$?" == "1" ]; then
    cp ${SRCDIR}/../third-party/${ARCH}/openssl-1.0.1e/LICENSE ${OUTDIR}/doc/openssl_license.txt
  fi
  have_config_val "CFG_HAVE_TRANSCODING"
  if [ "$?" == "1" ]; then
    cp ${SRCDIR}/../third-party/${ARCH}/ffmpeg-0.10.2/COPYING.LGPLv2.1 ${OUTDIR}/doc/ffmpeg_license.txt
  fi

  have_config_val "CFG_XCODE_OPUS_DLOPEN"
  if [ "${VAL}" -eq 1 ]; then
    cp ${LIBOPUSPATH}/${LIBOPUSSO_FLOATPOINT} ${OUTDIR}/lib/${LIBOPUSSO_FLOATPOINT}
    cp ${LIBOPUSPATH}/${LIBOPUSSO_FIXEDPOINT} ${OUTDIR}/lib/${LIBOPUSSO_FIXEDPOINT}
    CWDTMP=`pwd`
    cd ${OUTDIR}/lib/
    ln -s ${LIBOPUSSO_FLOATPOINT} ${LIBOPUSSO}
    cd ${CWDTMP}
  fi

  echo "OpenVSX "${VSXVERSION}".b"${VSXBUILDNUM} > ${OUTDIR}/version.txt
else
  echo "OpenVSX Lite "${VSXVERSION}".b"${VSXBUILDNUM}"-Lite" > ${OUTDIR}/version.txt
fi

#${SRCDIR}/scripts/pkg/customize_readme.sh mac ${SRCDIR}/scripts/pkg/${OUT}${LITE_VERSION}_readme.txt ${OUTDIR}/README.txt
ZIP_THIRDPARTY="${OUTDIR}thirdparty_${ARCH}_${VSXVERSION}.tar.gz"
sed -e "s/@__THIRDPARTY_PACKAGE_NAME__@/$ZIP_THIRDPARTY/g" "${SRCDIR}/scripts/pkg/${OUTIN}${LITE_VERSION}_readme.txt" > "${OUTDIR}/README.txt"

mv ${OUTDIR}/bin/${OUT}wrapper.sh ${OUTDIR}/bin/${OUT}

tar -czf ${ZIPOUT} ${OUTDIR}/*
if [ "$?" != 0 ]; then
  exit $?
fi
INSTALLER_PATH="`pwd`/${ZIPOUT}"
echo ""
echo "Created ${INSTALLER_PATH}"
echo "============================================================================================="
if [ "${SELFEXTRACTING_OUTPUT}" != "" ]; then
  echo "${INSTALLER_PATH}" > "${SELFEXTRACTING_OUTPUT}"
fi

#if [ -e "${EXESRC}_g" ]; then
#  cp ${EXESRC}_g ${OUTDIR}/bin/${EXENAME}${LITE_VERSION}_g
#fi

if [ "${LITE_VERSION}" == "" ]; then
  # Create the package consisting of any third-party licensed libraries
  #ZIPOUT=${OUTDIR}thirdparty_${ARCH}_${VSXVERSION}.tar.gz
  ZIPOUT="${ZIP_THIRDPARTY}"
  ZIPUT_SO=
  cp ${LIBX264PATH}/${LIBX264SO} ${OUTDIR}/lib/${LIBX264SO}
  ZIPOUT_SO+=" ${OUTDIR}/lib/${LIBX264SO}"
  cp ${LIBSILKPATH} ${OUTDIR}/lib/${LIBSILKSO}
  ZIPOUT_SO+=" ${OUTDIR}/lib/${LIBSILKSO}"
  cp ${LIBFAACPATH} ${OUTDIR}/lib/${LIBFAACSO}
  ZIPOUT_SO+=" ${OUTDIR}/lib/${LIBFAACSO}"
#  have_config_val "CFG_XCODE_OPUS_DLOPEN"
#  if [ "${VAL}" -eq 1 ]; then
#    cp ${LIBOPUSPATH}/${LIBOPUSSO} ${OUTDIR}/lib/${LIBOPUSSO}
#    ZIPOUT_SO+=" ${OUTDIR}/lib/${LIBOPUSSO}"
#  fi
  cp ${LIBX264PATH}/COPYING ${OUTDIR}/doc/x264_license.txt
  cp ${SRCDIR}/scripts/pkg/thirdparty_readme.txt ${OUTDIR}/README-THIRDPARTY.txt
  tar -czf ${ZIPOUT} ${ZIPOUT_SO} ${OUTDIR}/doc/x264_license.txt ${OUTDIR}/README-THIRDPARTY.txt
  RES=$?
  rm ${OUTDIR}/lib/${LIBX264SO}
  rm ${OUTDIR}/lib/${LIBSILKSO}
  rm ${OUTDIR}/lib/${LIBFAACSO}
#  if [ -f "${OUTDIR}/lib/${LIBOPUSSO}" ]; then
#    rm ${OUTDIR}/lib/${LIBOPUSSO}
#  fi
  rm ${OUTDIR}/doc/x264_license.txt
  rm ${OUTDIR}/README-THIRDPARTY.txt
  if [ "${RES}" != 0 ]; then
    exit ${RES}
  fi
  echo ""
  echo "Created "`pwd`"/${ZIPOUT}"
  echo "============================================================================================="
fi

cd ${CWD}
