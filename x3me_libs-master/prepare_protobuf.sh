#!/bin/bash
# this script is executed from other scripts
# but it also can be used as a standalone script

source ./protobufconfig.sh

PROTOBUF_TAR=protobuf-${PROTOBUF_VER}.tar.gz
PROTOBUF_URI=https://github.com/google/protobuf/releases/download/v${PROTOBUF_VER}/${PROTOBUF_TAR}

function download_protobuf
{
    if [[ ! -d ${PROTOBUF_DIR} ]] && [[ ! -f ${PROTOBUF_TAR} ]]; then
        echo -ne "downloading protobuf: ${PROTOBUF_TAR} ...\n"
        if wget ${PROTOBUF_URI} -O ${PROTOBUF_TAR}; then
            echo -ne "ok\n"
        else
            rm -f ${PROTOBUF_TAR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

function extract_protobuf
{
    if [[ ! -d ${PROTOBUF_DIR} ]]; then
        echo -ne "extracting protobuf: ${PROTOBUF_TAR} ...\n"
        if tar xf ${PROTOBUF_TAR} ; then
            echo -ne "ok\n"
        else # most likely a broken archive
            rm -f ${PROTOBUF_TAR}
            rm -rf ${PROTOBUF_DIR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

function build_protobuf
{
    num_cores=$(cat /proc/cpuinfo|grep processor|wc -l)

    if [[ ! -d ${PROTOBUF_LIB_DIR} ]] || 
        [[ ! "$(ls -A ${PROTOBUF_LIB_DIR}/*.a)" ]]; then
        pushd ${PWD} > /dev/null
        cd ${PROTOBUF_DIR}

        echo -ne "building protobuf library ...\n"

        ./autogen.sh

        ./configure --disable-shared

        make clean
        make -j${num_cores}
        make check -j${num_cores}

        popd > /dev/null
    fi
}

function remove_old_protobuf
{
    for found in "protobuf-"*; do
        if [[ -d ${found} ]] && [[ "${found}" != "${PROTOBUF_DIR}" ]]; then
            echo -ne "removing old protobuf dir: ${found} ...\n"
            rm -rf ${found}
        elif [[ -f ${found} ]] && 
            [[ ${found} == protobuf-*.tar.gz ]] && 
            [[ "${found}" != "${PROTOBUF_TAR}" ]]; then
            echo -ne "removing old protobuf tar: ${found} ...\n"
            rm -f ${found}
        fi
    done
}

#################################################################

download_protobuf
extract_protobuf
build_protobuf
remove_old_protobuf
