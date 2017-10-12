#!/bin/bash
# this script is executed from other scripts
# but it also can be used as a standalone script

source ./boostconfig.sh

BOOST_TAR_EXT=tar.bz2  
BOOST_TAR=${BOOST_DIR}.${BOOST_TAR_EXT}
BOOST_URI=http://sourceforge.net/projects/boost/files/boost/${BOOST_DVER}/${BOOST_TAR}

function download_boost
{
    if [[ ! -d ${BOOST_DIR} ]] && [[ ! -f ${BOOST_TAR} ]]; then
        echo -ne "downloading boost: ${BOOST_TAR} ...\n"
        if wget ${BOOST_URI} -O ${BOOST_TAR}; then
            echo -ne "ok\n"
        else
            rm -f ${BOOST_TAR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

function extract_boost
{
    if [[ ! -d ${BOOST_DIR} ]]; then
        echo -ne "extracting boost: ${BOOST_TAR} ...\n"
        if tar -xjf ${BOOST_TAR} ; then
            echo -ne "ok\n"
        else # most likely a broken archive
            rm -f ${BOOST_TAR}
            rm -rf ${BOOST_DIR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

function build_boost
{
    boost_build_exe=./b2
    zlib_path=${PWD}/zlib
    num_cores=$(cat /proc/cpuinfo|grep processor|wc -l)

    pushd ${PWD} > /dev/null
    cd ${BOOST_DIR}
    if [[ ! -f ${boost_build_exe} ]]; then 
        echo -ne "bootstraping boost ...\n"
        ./bootstrap.sh
    fi
    if [[ ! -d ${BOOST_DEBUG_LIBS_RELPATH} ]] || 
        [[ ! "$(ls -A ${BOOST_DEBUG_LIBS_RELPATH})" ]]; then
        echo -ne "building boost libraries debug ...\n"
        ${boost_build_exe} \
        variant=debug link=static runtime-link=shared threading=multi \
        threadapi=pthread \
        -j${num_cores} -q -sZLIB_SOURCE=${zlib_path} -sNO_BZIP2=1 \
        --without-python --stagedir=${PWD}/${BOOST_DEBUG_LIBS_RELPATH0}
    fi
    if [[ ! -d ${BOOST_RELEASE_LIBS_RELPATH} ]] || 
        [[ ! "$(ls -A ${BOOST_RELEASE_LIBS_RELPATH})" ]]; then
        echo -ne "building boost libraries release ...\n"
        ${boost_build_exe} \
        variant=release link=static runtime-link=shared threading=multi \
        threadapi=pthread \
        -j${num_cores} -q -sZLIB_SOURCE=${zlib_path} -sNO_BZIP2=1 \
        --without-python --stagedir=${PWD}/${BOOST_RELEASE_LIBS_RELPATH0}
    fi
    popd > /dev/null
}

function remove_old_boost 
{
    for found in "boost_"*; do
        if [[ -d ${found} ]] && [[ "${found}" != "${BOOST_DIR}" ]]; then
            echo -ne "removing old boost dir: ${found} ...\n"
            rm -rf ${found}
        elif [[ -f ${found} ]] && 
            [[ ${found} == boost_*.${BOOST_TAR_EXT} ]] && 
            [[ "${found}" != "${BOOST_TAR}" ]]; then
            echo -ne "removing old boost tar: ${found} ...\n"
            rm -f ${found}
        fi
    done
}

#################################################################

download_boost
extract_boost
build_boost
remove_old_boost
