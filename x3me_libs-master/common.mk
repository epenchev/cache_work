# this makefile is common for all x3me c++ projects

SHELL := /bin/bash

include $(LIBS_RELPATH)/boostconfig.sh # set the needed variables

BOOST_RELPATH=$(LIBS_RELPATH)/$(BOOST_DIR)

# these warnings will be fixed soon
TEMPORARY_DISABLED_WARNINGS=-Wno-missing-field-initializers
CXXFLAGS=-c -MMD $(COMPILER_FLAGS) -std='c++1z' -Wall -Wextra -Winvalid-pch \
    $(TEMPORARY_DISABLED_WARNINGS) \
    -isystem "/usr/include/mysql" -isystem "$(BOOST_RELPATH)" -I"$(LIBS_RELPATH)" \
    -pthread $(PROJECT_COMPILER_FLAGS)
LDFLAGS=$(LINKER_FLAGS) -pthread $(PROJECT_LINKER_FLAGS) \
	-Xlinker --defsym=X3ME_BUILD_DATE=$(shell date -u +'%Y%m%d') \
	-Xlinker --defsym=X3ME_BUILD_TIME=$(shell date -u +'%-H%M%S') \
	-Xlinker --defsym=X3ME_GIT_HASH=0x$(shell git rev-parse --short=7 HEAD || echo 1)

PRECOMPILED_HEADER=precompiled.h
PRECOMPILED_HEADER_D=$(PRECOMPILED_HEADER).d

cxx=
ifeq ($(cxx),clang)
CXX=clang++
CXXFLAGS:=-include $(PRECOMPILED_HEADER) $(CXXFLAGS)
PRECOMPILED_HEADER_PCH=$(PRECOMPILED_HEADER).pch
else # GCC
CXX=g++
PRECOMPILED_HEADER_PCH=$(PRECOMPILED_HEADER).gch
endif

OBJ_FILES=$(PROJECT_CPP_FILES:.cpp=.o)

DEP_FILES=$(OBJ_FILES:.o=.d) $(PRECOMPILED_HEADER_D)

################################################################################

#-flto is not used currently because the linker plugin for itbrings the symbol
#'__warn_memset_zero_len' which causes false positive linker warning
#'warning: memset used with constant zero length parameter; 
#this could be due to transposed parameters'
release: COMPILER_FLAGS=-O3
#-flto=jobserver
release: LINKER_FLAGS=-Wl,--strip-all -L$(BOOST_RELPATH)/$(BOOST_RELEASE_LIBS_RELPATH)
release: $(PROJECT_BINARY)

debug: COMPILER_FLAGS=-g -Og
debug: LINKER_FLAGS=-L$(BOOST_RELPATH)/$(BOOST_DEBUG_LIBS_RELPATH)
debug: $(PROJECT_BINARY)

# the targets are executed by separate/child make processes to ensure
# correct multiprocessor build (-j option)
all:
	$(MAKE) prepare_boost 
	$(MAKE) prepare_boost_http
	$(MAKE) prepare_expected
	$(MAKE) prepare_http_parser
	$(MAKE) prepare_jemalloc 
	$(MAKE) prepare_libfmt
	$(MAKE) prepare_libutp
	$(MAKE) prepare_protobuf
	$(MAKE) prepare_soci 
	$(MAKE) prepare_sparsehash
	$(MAKE) release

rebuild:
	$(MAKE) clean
	$(MAKE) release

rebuild_debug:
	$(MAKE) clean
	$(MAKE) debug

clean:
	rm -f $(PRECOMPILED_HEADER_PCH) $(DEP_FILES) $(OBJ_FILES) $(PROJECT_BINARY)

prepare_boost:
	pushd $(PWD) > /dev/null; \
	cd $(LIBS_RELPATH); \
	./prepare_boost.sh; \
	popd > /dev/null

prepare_boost_http:
	pushd $(PWD) > /dev/null; \
	cd $(LIBS_RELPATH); \
	./prepare_boost_http.sh; \
	popd > /dev/null

prepare_expected:
	pushd $(PWD) > /dev/null; \
	cd $(LIBS_RELPATH); \
	./prepare_expected.sh; \
	popd > /dev/null

prepare_http_parser:
	pushd $(PWD) > /dev/null; \
	cd $(LIBS_RELPATH); \
	./prepare_http_parser.sh; \
	popd > /dev/null

prepare_jemalloc:
	pushd $(PWD) > /dev/null; \
	cd $(LIBS_RELPATH); \
	./prepare_jemalloc.sh; \
	popd > /dev/null

prepare_libfmt:
	pushd $(PWD) > /dev/null; \
	cd $(LIBS_RELPATH); \
	./prepare_libfmt.sh; \
	popd > /dev/null

prepare_libutp:
	pushd $(PWD) > /dev/null; \
	cd $(LIBS_RELPATH); \
	./prepare_libutp.sh; \
	popd > /dev/null

prepare_protobuf:
	pushd $(PWD) > /dev/null; \
	cd $(LIBS_RELPATH); \
	./prepare_protobuf.sh; \
	popd > /dev/null

prepare_soci:
	pushd $(PWD) > /dev/null; \
	cd $(LIBS_RELPATH); \
	./prepare_soci.sh; \
	popd > /dev/null

prepare_sparsehash:
	pushd $(PWD) > /dev/null; \
	cd $(LIBS_RELPATH); \
	./prepare_sparsehash.sh; \
	popd > /dev/null

################################################################################

# ensure that if some of the included files is changed we are going to rebuild
# the needed .o and/or .gch file(s)
-include $(DEP_FILES)

# compile the precompiled header if needed
$(PRECOMPILED_HEADER_PCH): $(PRECOMPILED_HEADER)
	$(CXX) -x c++-header $(CXXFLAGS) -o $@ $<

# ensure that the precompiled header will be compiled first and also
# if the precompiled header is changed 
# all object files are going to be compiled again
$(OBJ_FILES): $(PRECOMPILED_HEADER_PCH)

# compile each of the object files if needed
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

# link the all object files into the final executable
$(PROJECT_BINARY): $(OBJ_FILES)
	+$(CXX) -o $@ $(OBJ_FILES) $(LDFLAGS)

################################################################################

.PHONY: \
	release \
	debug \
	all \
	rebuild \
	rebuild_debug \
	clean \
	prepare_boost \
	prepare_boost_http \
	prepare_expected \
	prepare_http_parser \
	prepare_jemalloc \
	prepare_libfmt \
	prepare_libutp \
	prepare_protobuf \
	prepare_soci \
	prepare_sparsehash
