
LIBS_RELPATH=../x3me_libs
XKMOD_RELPATH=../xkmod
X3ME_LIBS_RELPATH=$(LIBS_RELPATH)/x3me_libs

include $(LIBS_RELPATH)/boosthttpconfig.sh
include $(LIBS_RELPATH)/expectedconfig.sh
include $(LIBS_RELPATH)/fmtconfig.sh
include $(LIBS_RELPATH)/httpparserconfig.sh
include $(LIBS_RELPATH)/protobufconfig.sh
include $(LIBS_RELPATH)/smlconfig.sh
include $(LIBS_RELPATH)/sparsehashconfig.sh

# Note that the -I and -isystem directories are searched before the system ones
PROJECT_COMPILER_FLAGS=\
					   -isystem$(LIBS_RELPATH)/$(BOOST_HTTP_INCLUDE_DIR) \
					   -isystem$(LIBS_RELPATH)/$(BOOST_EXPECTED_INCLUDE_DIR) \
					   -isystem$(LIBS_RELPATH)/$(FMT_INCLUDE_DIR) \
					   -isystem$(LIBS_RELPATH)/$(HTTP_PARSER_INCLUDE_DIR) \
					   -isystem$(LIBS_RELPATH)/$(PROTOBUF_INCLUDE_DIR) \
					   -isystem$(LIBS_RELPATH)/$(SML_INCLUDE_DIR) \
					   -isystem$(LIBS_RELPATH)/$(SPARSEHASH_INCLUDE_DIR) \
			 		   -I$(XKMOD_RELPATH) \
					   -I.

PROJECT_LINKER_FLAGS= \
					 -L$(LIBS_RELPATH)/$(FMT_LIB_DIR) \
					 -L$(LIBS_RELPATH)/$(HTTP_PARSER_LIB_DIR) \
					 -L$(LIBS_RELPATH)/$(PROTOBUF_LIB_DIR) \
					 -rdynamic \
					 -lboost_filesystem \
					 -lboost_program_options \
					 -lboost_system \
					 -lboost_thread \
					 -lcap \
					 -lcrypto \
					 -lfmt \
					 -lhttp_parser \
					 -lpcre \
					 -lprotobuf \
					 -lz

PROJECT_BINARY=xproxy

PROJECT_CPP_FILES=\
				  $(wildcard *.cpp) \
				  $(wildcard cache/*.cpp) \
				  $(wildcard http/*.cpp) \
				  $(wildcard mgmt/*.cpp) \
				  $(wildcard net/*.cpp) \
				  $(wildcard plgns/*.cpp) \
				  $(wildcard xlog/*.cpp) \
				  $(wildcard xutils/*.cpp) \
				  $(X3ME_LIBS_RELPATH)/build_utils.cpp \
				  $(X3ME_LIBS_RELPATH)/http/http_conn.cpp \
				  $(X3ME_LIBS_RELPATH)/http/http_server.cpp \
				  $(X3ME_LIBS_RELPATH)/http/http_server_base.cpp \
				  $(X3ME_LIBS_RELPATH)/json_rpc/json_rpc_handler.cpp \
				  $(X3ME_LIBS_RELPATH)/json_rpc/json_rpc_server.cpp \
				  $(X3ME_LIBS_RELPATH)/stacktrace.cpp \
				  $(X3ME_LIBS_RELPATH)/sys_utils.cpp \
				  $(X3ME_LIBS_RELPATH)/x3me_assert.cpp \
				  $(XKMOD_RELPATH)/xbpctrl/usrland/netlink_conn.cpp

asan=
app_test=
ifeq ($(asan),1)
PROJECT_COMPILER_FLAGS:=$(PROJECT_COMPILER_FLAGS) \
						-fno-omit-frame-pointer \
						-fsanitize=address
PROJECT_LINKER_FLAGS:=-lasan $(PROJECT_LINKER_FLAGS)
else ifeq ($(app_test),1)
PROJECT_COMPILER_FLAGS:=$(PROJECT_COMPILER_FLAGS) \
						-fno-omit-frame-pointer \
						-fsanitize=address \
						-DX3ME_APP_TEST
PROJECT_LINKER_FLAGS:=-lasan $(PROJECT_LINKER_FLAGS)
PROJECT_BINARY=xproxy_app_test
endif

include $(LIBS_RELPATH)/common.mk
