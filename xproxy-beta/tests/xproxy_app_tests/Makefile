LIBS_RELPATH=../../../x3me_libs
X3ME_LIBS_RELPATH=$(LIBS_RELPATH)/x3me_libs

PROJECT_COMPILER_FLAGS=-pthread
PROJECT_LINKER_FLAGS=\
					 -pthread \
					 -lboost_system \
					 -lboost_unit_test_framework

PROJECT_BINARY=tests.bin

PROJECT_CPP_FILES=\
				  $(wildcard *.cpp) \

include $(LIBS_RELPATH)/common.mk

run:
	./$(PROJECT_BINARY) --log_level=error --report_level=short

.PHONY: run
