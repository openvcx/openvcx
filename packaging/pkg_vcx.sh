#!/bin/bash

. ./packaging/pkg_common.sh


function pkg_source {

  echo ""
  echo "Creating source tarball ..."
  which javadoc >/dev/null
  local RC=$?
  if [ -f ./${SRCDIR}/javadoc.sh -a ${RC} -eq 0 ]; then
    local CURDIR2=`pwd`
    cd ${SRCDIR}
    ./javadoc.sh >/dev/null
    cd ${CURDIR2}
  fi

  ZIPOUT=openvcx_src_${VSXVERSION}.tar.gz
  cp -R ${SRCDIR}/src ${OUT}/
  cp ${SRCDIR}/build_conference.xml ${OUT}/
  cp ${SRCDIR}/build_webrtcdemo.xml ${OUT}/
  cp ${SRCDIR}/build.sh ${OUT}/
  cp -R ${SRCDIR}/javadoc  ${OUT}/javadoc
  tar -czf ${ZIPOUT} ${OUT}/src ${OUT}/build_conference.xml ${OUT}/build_webrtcdemo.xml ${OUT}/build.sh ${OUT}/javadoc ${OUT}/version.txt
  rm ${OUT}/build_conference.xml
  rm ${OUT}/build_webrtcdemo.xml
  rm ${OUT}/build.sh
  echo "Created ${CURDIR}/${ZIPOUT}"
  echo "============================================================================================="
}

function do_md5 {
  local FILE_IN=$1
  local FILE_OUT=$2

  echo ""
  echo "Calculating package checksum ..."

  if [ `uname` == "Darwin" ]; then
    local MD5=md5
    local SUM=`${MD5} "${FILE_IN}" | awk -F'=' '{print $2}' | tr -d ' '`
  else
    local MD5=md5sum
    local SUM=`${MD5} "${FILE_IN}" | awk -F' ' '{print $1}'`
  fi

  if [ "${FILE_OUT}" != "" ]; then
    echo ${SUM} > ${FILE_OUT}
    echo "Created ${FILE_OUT} with MD5 sum: ${SUM}"
  else
    echo "MD5 sum is: ${SUM}"
  fi
  echo "============================================================================================="
}

function pkg_selfextracting {
  local TAR_FILE=$1
  local D=`dirname $VSX_BUNDLE_DIR`
  local VSX_ARCH=`basename $D`
  INSTALLER_FILE=openvcx_${VSX_ARCH}_${VSXVERSION}.b${VSXBUILDNUM}.bin
  local VSXFULLVER="${VSXVERSION}.b${VSXBUILDNUM}"
  local VCX_DIR="openvcx_${VSXFULLVER}"

  echo ""
  echo "Creating self-extracting ${CURDIR}/${INSTALLER_FILE} ..."

  echo "#!/bin/bash" > ${INSTALLER_FILE}
  echo "#  Installation Script for OpenVCX ${VSXFULLVER}.  This script should be run as a shell command." >> ${INSTALLER_FILE}
  echo "#  http://openvcx.com" >> ${INSTALLER_FILE}
  echo "echo \"\"" >> ${INSTALLER_FILE}
  echo "echo \"*************************************************************************************\"" >> ${INSTALLER_FILE}

  if [ "${VSX_ARCH}" == "${ARCH_MAC_X86_64}" ]; then
    echo "if [ \"\`uname\`\" != \"Darwin\" ]; then " >> ${INSTALLER_FILE}
    echo "echo \"Cannot install package for architecture ${VSX_ARCH} on \`uname\`\"" >> ${INSTALLER_FILE}
    echo "echo \"Aborting...\"; exit" >> ${INSTALLER_FILE}
    echo "fi" >> ${INSTALLER_FILE}
  elif [ "${VSX_ARCH}" == "${ARCH_LINUX_X86_32}" ]; then
    echo "if [ \"\`uname\`\" != \"Linux\" ]; then " >> ${INSTALLER_FILE}
    echo "echo \"Cannot install package for architecture ${VSX_ARCH} on \`uname\`\"" >> ${INSTALLER_FILE}
    echo "echo \"Aborting...\"; exit" >> ${INSTALLER_FILE}
    echo "fi" >> ${INSTALLER_FILE}
  else
    echo "Invalid OpenVSX architecture ${VSX_ARCH}"
    exit
  fi

  echo "echo \"Starting installation of OpenVCX ${VSXFULLVER} for $VSX_ARCH from http://openvcx.com\"" >> ${INSTALLER_FILE}
  echo "VCX_HOME=\"\"" >> ${INSTALLER_FILE}
  echo "while [ \"\${VCX_HOME}\" == \"\" ]; do" >> ${INSTALLER_FILE}
  echo "echo \"\"" >> ${INSTALLER_FILE}
  echo "echo \"Enter the directory to extract OpenVCX ${VSXFULLVER}\"" >> ${INSTALLER_FILE}
  echo "echo \"For eg.  > \`echo ~\`    Will extract to: \`echo ~\`/${VCX_DIR}\"" >> ${INSTALLER_FILE}
  echo "echo \"\"" >> ${INSTALLER_FILE}
  echo "echo \"Note:  You DO NOT need to be root to install or run OpenVCX.\"" >> ${INSTALLER_FILE}
  echo "echo \"\"" >> ${INSTALLER_FILE}
  echo "echo -n \">\"" >> ${INSTALLER_FILE}
  echo "read -r VCX_HOME" >> ${INSTALLER_FILE}
  echo "VCX_HOME=\"\`eval echo \${VCX_HOME}\`\"" >> ${INSTALLER_FILE}
  echo "if [ \"\${VCX_HOME}\" != \"\" -a -d \"\${VCX_HOME}\" -a ! -w \"\${VCX_HOME}\" ]; then" >> ${INSTALLER_FILE}
  echo "echo \"Error:  \`whoami\` does not have permission to write to \${VCX_HOME}\"" >> ${INSTALLER_FILE}
  echo "VCX_HOME=" >> ${INSTALLER_FILE}
  echo "elif [ \"\${VCX_HOME}\" != \"\" -a -d \"\${VCX_HOME}/${VCX_DIR}\" ]; then" >> ${INSTALLER_FILE}
  echo "echo \"The directory \${VCX_HOME}/${VCX_DIR} already exists.\"" >> ${INSTALLER_FILE}
  echo "echo -n \"Are you sure you want to overwrite the contents? y/n >\"" >> ${INSTALLER_FILE}
  echo "read -r CONT" >> ${INSTALLER_FILE}

  echo "if [ \"\$CONT\" != \"y\" ]; then" >> ${INSTALLER_FILE}
  echo "VCX_HOME=" >> ${INSTALLER_FILE}
  echo "elif [ -d \"\${VCX_HOME}/${VCX_DIR}\" -a ! -w \"\${VCX_HOME}/${VCX_DIR}\" ]; then" >> ${INSTALLER_FILE}
  echo "echo \"Error:  \`whoami\` does not have permission to write to \${VCX_HOME}/${VCX_DIR}\"" >> ${INSTALLER_FILE}
  echo "VCX_HOME=" >> ${INSTALLER_FILE}
  echo "fi" >> ${INSTALLER_FILE}

  echo "elif [ \"\${VCX_HOME}\" != \"\" -a ! -d \"\${VCX_HOME}\" ]; then" >> ${INSTALLER_FILE}
  echo "echo \"The directory \${VCX_HOME} does not exist.\"" >> ${INSTALLER_FILE}
  echo "echo -n \"Do you want to create it?  y/n >\"" >> ${INSTALLER_FILE}
  echo "read -r CONT" >> ${INSTALLER_FILE}

  echo "if [ \"\$CONT\" == \"y\" ]; then" >> ${INSTALLER_FILE}
  echo "mkdir -p \"\${VCX_HOME}\"" >> ${INSTALLER_FILE}
  echo "if [ \"\$?\" != 0 ]; then" >> ${INSTALLER_FILE}
  echo "echo \"Aborting...\"" >> ${INSTALLER_FILE}
  echo "exit" >> ${INSTALLER_FILE}
  echo "fi" >> ${INSTALLER_FILE}
  echo "echo \"Created \${VCX_HOME}\"" >> ${INSTALLER_FILE}
  echo "fi" >> ${INSTALLER_FILE}

  echo "if [ ! -d \"\${VCX_HOME}\" ]; then" >> ${INSTALLER_FILE}
  echo "VCX_HOME=" >> ${INSTALLER_FILE}
  echo "fi" >> ${INSTALLER_FILE}

  echo "fi" >> ${INSTALLER_FILE}

  echo "done" >> ${INSTALLER_FILE}
  echo "echo -n \"Unpacking archive to \${VCX_HOME}/${VCX_DIR}\" ... " >> ${INSTALLER_FILE}
  echo "sed -e '1,/^exit -11$/d' "\$0" | tar xzf - -C "\${VCX_HOME}" && if [ \$? != 0 ]; then echo "\"Aborting...\""; exit; fi && echo " Done." && echo \"\" && echo "\"*************************************************************************************\"" && echo "\"OpenVCX \(Video Conferencing Server\) is installed in \${VCX_HOME}/${VCX_DIR}\"" && echo "\"OpenVSX \(Video Streaming Server\) is installed in \${VCX_HOME}/${VCX_DIR}/openvsx\"" && echo "\"Please review the readme found in \${VCX_HOME}/${VCX_DIR}/README.txt\"" && echo \"\" && echo "\"Run \${VCX_HOME}/${VCX_DIR}/startvcx.sh to start OpenVCX.\"" && echo \"\" && CONT=\"\" && while [ \"\$CONT\" != \"y\" -a \"\$CONT\" != \"n\" ]; do echo -n "\"Do you want to run startvcx.sh to configure and start the OpenVCX SIP Server? y/n \>\"" && read -r CONT; done && test \"\$CONT\" = \"y\" && echo \"\" && echo \"cd into \${VCX_HOME}/${VCX_DIR}\" && cd \${VCX_HOME}/${VCX_DIR} && echo "Running ./startvcx.sh for first time configuration..." && echo "\"*************************************************************************************\"" && echo \"\" && ./startvcx.sh" >> ${INSTALLER_FILE}
  echo "exit -11" >> ${INSTALLER_FILE}
  cat $TAR_FILE >> ${INSTALLER_FILE}
  chmod u+x ${INSTALLER_FILE}
  INSTALLER_PATH="${CURDIR}/${INSTALLER_FILE}"
  echo "Created self-extracting ${INSTALLER_PATH}"
  echo "============================================================================================="

  if [ -f "${INSTALLER_FILE}.md5" ]; then
    rm ${INSTALLER_FILE}.md5
  fi
  do_md5 ${INSTALLER_FILE} ${INSTALLER_FILE}.md5
  
}

function copy_stunnel {
  local STUNNEL_ARCH=$1

  if [ ! -d ${OUT}/stunnel/$STUNNEL_ARCH ]; then
    mkdir -p ${OUT}/stunnel/$STUNNEL_ARCH/bin
  fi

  cp ${SRCDIR}/stunnel/$STUNNEL_ARCH/bin/stunnel_ws  ${OUT}/stunnel/$STUNNEL_ARCH/bin/
  cp ${SRCDIR}/stunnel/$STUNNEL_ARCH/stunnel.conf.ORIG ${OUT}/stunnel/$STUNNEL_ARCH/stunnel.conf
  cp ${SRCDIR}/stunnel/$STUNNEL_ARCH/stunnel.pem ${OUT}/stunnel/$STUNNEL_ARCH/
  cp ${SRCDIR}/stunnel/$STUNNEL_ARCH/stunnel-4.56_ws.patch ${OUT}/stunnel/$STUNNEL_ARCH/
  cp ${SRCDIR}/stunnel/$STUNNEL_ARCH/COPYING ${OUT}/stunnel/$STUNNEL_ARCH/
  cp ${SRCDIR}/stunnel/$STUNNEL_ARCH/COPYRIGHT.GPL ${OUT}/stunnel/$STUNNEL_ARCH/
}

getvsxvermajmin
VSXVERSION=$VAR
if [ "_" = _"${VSXVERSION}" ]; then
  echo "Unable to retrieve version string"
  exit 0
fi

getvsxbuildnum
VSXBUILDNUM=$VAR
if [ "_" = _"${VSXBUILDNUM}" ]; then
  echo "Unable to retrieve build number"
  exit 0
fi

getvcxver
VCXVER=$VAR

ARCH_LINUX_X86_32="linux_x86_32"
ARCH_MAC_X86_64="mac_x86_64"

if [ "$1" != "" ]; then
  VSX_BUNDLE_DIR=$1
  D=`dirname $VSX_BUNDLE_DIR`
  VSX_ARCH=`basename $D`
  ARCH=${VSX_ARCH}
  if [ "${ARCH}" == "" ]; then
    echo "Unable to determine ARCH from ${VSX_BUNDLE_DIR}"
    exit
  elif [ "${ARCH}" != "${ARCH_LINUX_X86_32}" -a "${ARCH}" != "${ARCH_MAC_X86_64}" ]; then
    echo "Unsupported OpenVSX architecture ${ARCH}"
    exit
  fi

  echo ""
  echo "Going to create self-extracting OpenVCX installation binary for $ARCH ..."
else
  echo "Going to create non self-extracting OpenVCX architecture independent tar.gz ..."
  ARCH=vcx
  echo "This method is deprecated.  You should use architecture specific bundled packaging of OpenVCX."
  exit 0
fi

SELFEXTRACTING_OUTPUT=$2

OUT="openvcx_${VSXVERSION}.b${VSXBUILDNUM}"
DIRNAME=${ARCH}

CWD=`pwd`
#getdirname $0

if [ ! -d ./dist/${DIRNAME} ]; then
  mkdir -p ./dist/${DIRNAME}
fi

cd ./dist/${DIRNAME}

if [ -d ./${OUT} ]; then
  rm -rf ./${OUT}
fi

if [ ! -d ${OUT}/webapps ]; then
  mkdir -p ${OUT}/webapps
fi

if [ ! -d ${OUT}/conf ]; then
  mkdir -p ${OUT}/conf
fi

if [ ! -d ${OUT}/conferences/auto ]; then
  mkdir -p ${OUT}/conferences/auto
fi

if [ ! -d ${OUT}/conf/dars ]; then
  mkdir -p ${OUT}/conf/dars
fi

if [ ! -d ${OUT}/media ]; then
  mkdir -p ${OUT}/media
fi

if [ ! -d ${OUT}/license ]; then
  mkdir -p ${OUT}/license
fi

ZIPOUT=openvcx_${VSXVERSION}.tar.gz
SIPMGRDIR=vcx/mobicents-sip
VSXDIR=../../openvsx
SRCDIR=../../${SIPMGRDIR}
SIPMGR_WAR=${SRCDIR}/webapps/sip-conference.war
WEBRTCDEMO_WAR=${SRCDIR}/webapps/webrtc-demo.war


if [ ! -e ${SIPMGR_WAR} ]; then
  echo "${SIPMGR_WAR} not found."
  exit 1
fi

mkdir ${OUT}/logs
mkdir ${OUT}/temp


if [ "${VSX_BUNDLE_DIR}" != "" ]; then
  echo "Copying OpenVSX from ${VSX_BUNDLE_DIR} to ${OUT}/ ..."
  cp -R ../../${VSX_BUNDLE_DIR} ${OUT}/
  if [ "$?" != "0" ]; then
    echo "Aborting..."
    exit 1 
  fi
  VSX_THIRDPARTY="$D/openvsxthirdparty_${VSX_ARCH}_${VSXVERSION}.tar.gz"
  echo "Extracting ${VSX_THIRDPARTY} to ./${OUT} ..."
  tar -xzf ../../${VSX_THIRDPARTY} -C ./${OUT}
fi

cp ${SIPMGR_WAR} ${OUT}/webapps/sip-conference.war
cp ${WEBRTCDEMO_WAR} ${OUT}/webapps/webrtc-demo.war
mkdir -p ${OUT}/bin
cp -R ${SRCDIR}/bin/* ${OUT}/bin/
cp ${SRCDIR}/conf/catalina.policy ${OUT}/conf/
cp ${SRCDIR}/conf/catalina.properties ${OUT}/conf/
cp ${SRCDIR}/conf/context.xml ${OUT}/conf/
cp ${SRCDIR}/conf/dars/* ${OUT}/conf/dars
cp ${SRCDIR}/conf/logging.properties ${OUT}/conf/
cp ${SRCDIR}/conf/mss-sip-stack.properties ${OUT}/conf/
cp ${SRCDIR}/conf/server.xml ${OUT}/conf/
cp ${SRCDIR}/conf/sip-conference.conf ${OUT}/conf/
touch ${OUT}/conf/start.env
cp ${SRCDIR}/conf/tomcat-users.xml ${OUT}/conf/
cp ${SRCDIR}/conf/web.xml ${OUT}/conf/
cp ${SRCDIR}/conferences/1234.conference ${OUT}/conferences
cp ${SRCDIR}/conferences/auto.template ${OUT}/conferences
cp ${SRCDIR}/media/* ${OUT}/media
mkdir -p ${OUT}/lib
cp -R ${SRCDIR}/lib/* ${OUT}/lib/
cp -R ${SRCDIR}/license  ${OUT}/
cp ${SRCDIR}/license/JAIN_SIP.LICENSE ${OUT}/license
cp ${SRCDIR}/license/LGPL-LICENSE.TXT ${OUT}/license
cp ${SRCDIR}/license/LICENSE ${OUT}/license
cp ${SRCDIR}/license/MOBICENTS-LICENSE.txt ${OUT}/license
cp ${SRCDIR}/license/NIST-CONTRIB-LICENSE.txt ${OUT}/license
cp ${SRCDIR}/license/NIST_SIP.LICENSE ${OUT}/license
cp ${SRCDIR}/license/NOTICE ${OUT}/license
cp ${VSXDIR}/scripts/pkg/vcx_readme.txt ${OUT}/README.txt
if [ "${VSX_BUNDLE_DIR}" == "" ]; then
  copy_stunnel "linux_x86_32"
  copy_stunnel "mac_x86_64"
else
  copy_stunnel "${VSX_ARCH}"
fi

rm ${OUT}/bin/*.in
rm ${OUT}/bin/*.pre

STARTENV=${OUT}/conf/start.env
echo "" > ${STARTENV}
echo "CATALINA_HOME=" >> ${STARTENV}
echo "VSX_HOME=" >> ${STARTENV}
echo "SDP_CONNECT_LOCATION=" >> ${STARTENV}

echo -n "OpenVCX Video Conferencing Server "${VSXVERSION} > ${OUT}/version.txt
echo -n ".b"${VSXBUILDNUM} >> ${OUT}/version.txt
echo -n " ${VCXVER}" >> ${OUT}/version.txt
echo  "" >> ${OUT}/version.txt

if [ "${VSX_BUNDLE_DIR}" != "" ]; then
  checkVersions ${OUT}/version.txt ${OUT}/openvsx/version.txt
  if [ ${RET} != 0 ]; then
    echo "Software version mismatch ${OUT}/version.txt ${OUT}/openvsx/version.txt"
    exit
  fi
fi

#export OUT=${OUT}
#export SRCDIR=${SRCDIR}
#$CWD/scripts/pkg/customize_readme.sh linux

CURDIR=`pwd`
cd ${OUT}
ln -s bin/startvcx.sh
cd ${CURDIR}

tar -czf ${ZIPOUT} ${OUT}/*
echo "Created ${CURDIR}/${ZIPOUT}"
echo "============================================================================================="

INSTALLER_PATH=""
if [ "${VSX_BUNDLE_DIR}" != "" ]; then
  #Create self extracting executable 
  pkg_selfextracting ${CURDIR}/${ZIPOUT}
fi

#Create the source package
pkg_source

cd ${CWD}

if [ "${INSTALLER_PATH}" != "" ]; then
  echo ""
  echo "Install by executing the self-extracting binary from the shell."
  echo "${INSTALLER_PATH}"
  if [ "${SELFEXTRACTING_OUTPUT}" != "" ]; then
    echo "${INSTALLER_PATH}" > "${SELFEXTRACTING_OUTPUT}"
  fi
fi
