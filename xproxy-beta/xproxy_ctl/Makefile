
LIBS_RELPATH=../../x3me_libs
X3ME_LIBS_RELPATH=$(LIBS_RELPATH)/x3me_libs

include $(LIBS_RELPATH)/urdlconfig.sh

PROJECT_COMPILER_FLAGS=\
					   -isystem$(LIBS_RELPATH)/$(URDL_INCLUDE_DIR) \
					   -I.

PROJECT_LINKER_FLAGS= \
					 -rdynamic \
					 -lboost_program_options \
					 -lboost_system

PROJECT_BINARY=xproxy_ctl

PROJECT_CPP_FILES=\
				  $(wildcard *.cpp) \
				  $(X3ME_LIBS_RELPATH)/build_utils.cpp \
				  $(X3ME_LIBS_RELPATH)/stacktrace.cpp \

include $(LIBS_RELPATH)/common.mk
