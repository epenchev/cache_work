#!/bin/bash
# this script is executed from other scripts
# but it also can be used as a standalone script

TAR_EXT=tar.gz
CTEMPLATE_DIR=ctemplate-2.3
CTEMPLATE_TAR=${CTEMPLATE_DIR}.${TAR_EXT}
CTEMPLATE_URI=https://xwis.net/ctemplate/${CTEMPLATE_TAR}

function download_ctemplate
{
    if [[ ! -d ${CTEMPLATE_DIR} ]] && [[ ! -f ${CTEMPLATE_TAR} ]]; then
        echo -ne "downloading ctemplate: ${CTEMPLATE_TAR} ...\n"
        if wget ${CTEMPLATE_URI}; then
            echo -ne "ok\n"
        else
            rm -f ${CTEMPLATE_TAR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

function extract_ctemplate
{
    if [[ ! -d ${CTEMPLATE_DIR} ]]; then
        echo -ne "extracting ctemplate: ${CTEMPLATE_TAR} ...\n"
        if tar xf ${CTEMPLATE_TAR} ; then
            echo -ne "ok\n"
        else # most likely a broken archive
            rm -f ${CTEMPLATE_TAR}
            rm -rf ${CTEMPLATE_DIR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

function build_ctemplate
{
    num_cores=$(cat /proc/cpuinfo|grep processor|wc -l)

    if [[ ! -d ${CTEMPLATE_DIR}/lib ]] || 
        [[ ! "$(ls -A ${CTEMPLATE_DIR}/lib/*.a)" ]]; then
        pushd ${PWD} > /dev/null
        cd ${CTEMPLATE_DIR}

        echo -ne "building ctemplate library ...\n"
        
        ./configure --enable-shared=no --prefix=${PWD}
        make clean
        make -j${num_cores}
        make check
        make install
        make clean

        popd > /dev/null
    fi
}

function remove_old_ctemplate
{
    for found in "ctemplate-"*; do
        if [[ -d ${found} ]] && [[ "${found}" != "${CTEMPLATE_DIR}" ]]; then
            echo -ne "removing old ctemplate dir: ${found} ...\n"
            rm -rf ${found}
        elif [[ -f ${found} ]] && [[ ${found} == ctemplate-*.${TAR_EXT} ]] && 
            [[ "${found}" != "${CTEMPLATE_TAR}" ]]; then
            echo -ne "removing old ctemplate tar: ${found} ...\n"
            rm -f ${found}
        fi
    done
}

#################################################################

download_ctemplate
extract_ctemplate
build_ctemplate
remove_old_ctemplate

