#!/bin/bash
# this script is executed from other scripts
# but it also can be used as a standalone script

source ./libutpconfig.sh

function clone_libutp
{
    if [[ ! -d ${LIBUTP_DIR} ]]; then
        echo -ne "clonning ${LIBUTP_DIR} ...\n"
        if git clone https://github.com/bittorrent/libutp ${LIBUTP_DIR}; then
            echo -ne "ok\n"
        else
            rm -rf ${LIBUTP_DIR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

function build_libutp
{
    if [[ ! -f "${LIBUTP_DIR}/libutp.a" ]]; then
        pushd ${PWD} > /dev/null
        cd ${LIBUTP_DIR}

        echo -ne "building libutp library ...\n"

        make clean
        make libutp.a

        popd > /dev/null
    fi
}

################################################################################

clone_libutp
build_libutp
