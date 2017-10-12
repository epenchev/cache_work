# this script is executed from other scripts

# boost dot version
BOOST_DVER=1.61.0
# this file is included from both shell script and make file. 
# so we can't use ${BOOST_DVER//./_}
# boost underscore version
BOOST_UVER=1_61_0

BOOST_DIR=boost_${BOOST_UVER}

BOOST_DEBUG_LIBS_RELPATH0=lib/debug
BOOST_RELEASE_LIBS_RELPATH0=lib/release
BOOST_DEBUG_LIBS_RELPATH=${BOOST_DEBUG_LIBS_RELPATH0}/lib
BOOST_RELEASE_LIBS_RELPATH=${BOOST_RELEASE_LIBS_RELPATH0}/lib
