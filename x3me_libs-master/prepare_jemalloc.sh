#!/bin/bash
# this script is executed from other scripts
# but it also can be used as a standalone script

source ./jemallocconfig.sh

JEMALLOC_TAR_EXT=tar.bz2
JEMALLOC_TAR=${JEMALLOC_DIR}.${JEMALLOC_TAR_EXT}
JEMALLOC_URI=https://github.com/jemalloc/jemalloc/releases/download/${JEMALLOC_VER}/jemalloc-${JEMALLOC_VER}.tar.bz2

function download_jemalloc
{
    if [[ ! -d ${JEMALLOC_DIR} ]] && [[ ! -f ${JEMALLOC_TAR} ]]; then
        echo -ne "downloading jemalloc: ${JEMALLOC_TAR} ...\n"
        if wget ${JEMALLOC_URI} -O ${JEMALLOC_TAR}; then
            echo -ne "ok\n"
        else
            rm -f ${JEMALLOC_TAR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

function extract_jemalloc
{
    if [[ ! -d ${JEMALLOC_DIR} ]]; then
        echo -ne "extracting jemalloc: ${JEMALLOC_TAR} ...\n"
        if tar xf ${JEMALLOC_TAR} ; then
            echo -ne "ok\n"
        else # most likely a broken archive
            rm -f ${JEMALLOC_TAR}
            rm -rf ${JEMALLOC_DIR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

function build_jemalloc
{
    if [[ ! -d ${JEMALLOC_LIB_DIR} ]] || 
        [[ ! "$(ls -A ${JEMALLOC_LIB_DIR}/*.so)" ]]; then
        pushd ${PWD} > /dev/null
        cd ${JEMALLOC_DIR}

        echo -ne "building jemalloc library ...\n"

        ./configure --prefix=${PWD}/${JEMALLOC_INSTALL_DIR} \
                    --disable-stats --disable-fill \
                    --disable-valgrind --disable-experimental
        make clean
        make
        #make check
        make install
        make distclean

        popd > /dev/null
    fi
}

function remove_old_jemalloc
{
    for found in "jemalloc-"*; do
        if [[ -d ${found} ]] && [[ "${found}" != "${JEMALLOC_DIR}" ]]; then
            echo -ne "removing old jemalloc dir: ${found} ...\n"
            rm -rf ${found}
        elif [[ -f ${found} ]] && 
            [[ ${found} == jemalloc-*.${JEMALLOC_TAR_EXT} ]] && 
            [[ "${found}" != "${JEMALLOC_TAR}" ]]; then
            echo -ne "removing old jemalloc tar: ${found} ...\n"
            rm -f ${found}
        fi
    done
}

#################################################################

download_jemalloc
extract_jemalloc
build_jemalloc
remove_old_jemalloc
