#!/bin/bash
# this script is executed from other scripts
# but it also can be used as a standalone script

source ./sociconfig.sh

SOCI_TAR_EXT=tar.gz
SOCI_TAR=${SOCI_DIR}.${SOCI_TAR_EXT}
SOCI_URI=http://sourceforge.net/projects/soci/files/soci/${SOCI_DIR}/${SOCI_TAR}/download

function download_soci
{
    if [[ ! -d ${SOCI_DIR} ]] && [[ ! -f ${SOCI_TAR} ]]; then
        echo -ne "downloading soci: ${SOCI_TAR} ...\n"
        if wget ${SOCI_URI} -O ${SOCI_TAR}; then
            echo -ne "ok\n"
        else
            rm -f ${SOCI_TAR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

function extract_soci
{
    if [[ ! -d ${SOCI_DIR} ]]; then
        echo -ne "extracting soci: ${SOCI_TAR} ...\n"
        if tar xf ${SOCI_TAR} ; then
            echo -ne "ok\n"
        else # most likely a broken archive
            rm -f ${SOCI_TAR}
            rm -rf ${SOCI_DIR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

function build_soci
{
    num_cores=$(cat /proc/cpuinfo|grep processor|wc -l)

    if [[ ! -d ${SOCI_LIB_DIR} ]] || 
        [[ ! "$(ls -A ${SOCI_LIB_DIR}/*.a)" ]]; then
        pushd ${PWD} > /dev/null
        cd ${SOCI_DIR}

        echo -ne "building soci library ...\n"
        
        rm -f CMakeCache.txt
        cmake -DCMAKE_BUILD_TYPE=RELEASE \
            -DCMAKE_INSTALL_PREFIX=${PWD} \
            -DSOCI_STATIC=ON \
            -DSOCI_TESTS=OFF \
            -DWITH_BOOST=ON \
            -DWITH_MYSQL=ON
        make clean
        make -j${num_cores}
        make install
        make clean
        rm -f CMakeCache.txt
        # Remove all dynamic libraries so that the applications
        # can easily link to the static versions
        rm -f lib64/*.so* 

        popd > /dev/null
    fi
}

function remove_old_soci
{
    for found in "soci-"*; do
        if [[ -d ${found} ]] && [[ "${found}" != "${SOCI_DIR}" ]]; then
            echo -ne "removing old soci dir: ${found} ...\n"
            rm -rf ${found}
        elif [[ -f ${found} ]] && [[ ${found} == soci-*.${SOCI_TAR_EXT} ]] && 
            [[ "${found}" != "${SOCI_TAR}" ]]; then
            echo -ne "removing old soci tar: ${found} ...\n"
            rm -f ${found}
        fi
    done
}

#################################################################

download_soci
extract_soci
build_soci
remove_old_soci

