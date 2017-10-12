LIBS_RELPATH=../../../x3me_libs
X3ME_LIBS_RELPATH=$(LIBS_RELPATH)/x3me_libs

include $(LIBS_RELPATH)/fmtconfig.sh

PROJECT_COMPILER_FLAGS=\
					   -isystem$(LIBS_RELPATH)/$(FMT_INCLUDE_DIR) \
					   -I. \
					   -I../..
# -fsanitize=thread

# -ltsan
PROJECT_LINKER_FLAGS=\
					 -L$(LIBS_RELPATH)/$(FMT_LIB_DIR) \
					 -lboost_system \
					 -lfmt

PROJECT_BINARY=speed_test.bin

PROJECT_CPP_FILES=\
				  $(wildcard *.cpp) \
				  $(wildcard ../../xlog/*.cpp) \
				  $(X3ME_LIBS_RELPATH)/sys_utils.cpp

include $(LIBS_RELPATH)/common.mk
