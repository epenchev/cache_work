#!/bin/bash
# this script is executed from other scripts

source ./httpparserconfig.sh

function build_http_parser
{
    if [[ ! -f ${HTTP_PARSER_LIB_DIR}/libhttp_parser.a ]]; then
        pushd ${PWD} > /dev/null
        cd ${HTTP_PARSER_DIR}

        echo -ne "building http-parser library ...\n"

        make clean
        make

        popd > /dev/null
    fi
}

################################################################################

build_http_parser
