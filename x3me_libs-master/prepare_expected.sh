#!/bin/bash
# this script is executed from other scripts
# but it also can be used as a standalone script

source ./expectedconfig.sh

function clone_boost_expected
{
    if [[ ! -d ${BOOST_EXPECTED_DIR} ]]; then
        echo -ne "clonning ${BOOST_EXPECTED_DIR} ...\n"
        if git clone https://github.com/ptal/expected ${BOOST_EXPECTED_DIR}; then
            echo -ne "ok\n"
        else
            rm -rf ${BOOST_EXPECTED_DIR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

################################################################################

clone_boost_expected
