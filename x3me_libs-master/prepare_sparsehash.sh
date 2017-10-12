#!/bin/bash
# this script is executed from other scripts
# but it also can be used as a standalone script

source ./sparsehashconfig.sh

function clone_sparsehash
{
    if [[ ! -d ${SPARSEHASH_DIR} ]]; then
        echo -ne "clonning ${SPARSEHASH_DIR} ...\n"
        if git clone https://github.com/sparsehash/sparsehash ${SPARSEHASH_DIR}; then
            echo -ne "ok\n"
        else
            rm -rf ${SPARSEHASH_DIR}
            echo -ne "failed\n"
            exit
        fi
    fi
}

function build_sparsehash
{
    # build the tests and run them
    if [[ ! -f "${SPARSEHASH_DIR}/Makefile" ]]; then
        pushd ${PWD} > /dev/null
        cd ${SPARSEHASH_DIR}

        echo -ne "building sparsehash library ...\n"

        ./configure
        make -j4
        make check

        popd > /dev/null
    fi
}

################################################################################

clone_sparsehash
build_sparsehash
