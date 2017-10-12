#!/bin/bash
# this script is executed from other scripts
# but it also can be used as a standalone script

source ./fmtconfig.sh

function build_libfmt
{
    if [[ ! -f "${FMT_LIB_DIR}/libfmt.a" ]]; then
        pushd ${PWD} > /dev/null
        if [[ ! -d "${FMT_BUILD_DIR}" ]]; then
            mkdir ${FMT_BUILD_DIR}
        fi    
        cd ${FMT_BUILD_DIR}

        echo -ne "building fmt library ...\n"

        cmake ../
        make clean
        make

        popd > /dev/null
    fi
}

################################################################################

build_libfmt
