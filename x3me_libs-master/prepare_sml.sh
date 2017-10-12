#!/bin/bash
# this script is executed from other scripts
# but it also can be used as a standalone script

source ./smlconfig.sh

function clone_sml
{
    if [[ ! -d ${SML_DIR} ]]; then
        echo -ne "clonning ${SML_DIR} ...\n"
        if git clone https://github.com/boost-experimental/sml ${SML_DIR}; then
            echo -ne "ok\n"
        else
            rm -rf ${SML_DIR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

################################################################################

clone_sml
