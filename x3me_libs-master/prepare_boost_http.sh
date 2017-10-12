#!/bin/bash
# this script is executed from other scripts
# but it also can be used as a standalone script

source ./boosthttpconfig.sh

function clone_boost_http
{
    if [[ ! -d ${BOOST_HTTP_DIR} ]]; then
        echo -ne "clonning ${BOOST_HTTP_DIR} ...\n"
        if git clone https://github.com/BoostGSoC14/boost.http ${BOOST_HTTP_DIR}; then
            echo -ne "ok\n"
        else
            rm -rf ${BOOST_HTTP_DIR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

################################################################################

clone_boost_http
