
SHELL := /bin/bash

################################################################################

CPP_FILES=\
	main.cpp \
	tests_array_view.cpp \
	tests_convert.cpp \
	tests_bencode.cpp \
	tests_binary_stream.cpp \
	tests_bitfield.cpp \
	tests_encode.cpp \
	tests_decode.cpp \
	tests_infohash.cpp \
	tests_infohash_view.cpp \
	tests_inplace_fn.cpp \
	tests_locked.cpp \
	tests_math_funcs.cpp \
	tests_mem_fn_delegate.cpp \
	tests_peer_id.cpp \
	tests_peer_id_view.cpp \
	tests_rcu_resource.cpp \
	tests_short_alloc.cpp \
	tests_stack_buffer.cpp \
	tests_stack_string.cpp \
	tests_string_builder.cpp \
	tests_synchronized.cpp \
	tests_tagged_ptr.cpp \
	tests_typelist.cpp \
	tests_x3me_assert.cpp \
	tests_x3me_assert_NDEBUG.cpp \
	common.cpp \
	../bitfield.cpp \
	../x3me_assert.cpp

################################################################################

OBJ_FILES=$(CPP_FILES:.cpp=.o)
DEP_FILES=$(CPP_FILES:.cpp=.d)

################################################################################
# GCC seems to be giving a wrong warning -Wnomissing-field-initializer
# for such initialization of std::array - arr{};

CXX=g++
CXXFLAGS=-c -MMD -O3 -std='c++1z' \
		 -Wall -Wextra -Wno-missing-field-initializers \
		 -fno-omit-frame-pointer -fsanitize=address \
		 -isystem ../../boost_1_61_0/ -pthread
LDFLAGS=-lasan -L../../boost_1_61_0/lib/debug/lib -lcurl -pthread
BINARY=tests

################################################################################

all: $(BINARY)

# the targets are executed by separate/child make processes to ensure
# correct multiprocessor build (-j option)
rebuild:
	$(MAKE) clean
	$(MAKE) all

clean:
	rm -f $(DEP_FILES) $(OBJ_FILES) $(BINARY)

run:
	./$(BINARY) --log_level=error --report_level=short --show_progress=yes

################################################################################

# ensure that if some of the included files is changed we are going to rebuild
# the needed file(s)
-include $(DEP_FILES)

# compile each of the object files if needed
%.o: %.cpp
	$(CXX) $(CXXFLAGS) -o $@ $<

# link the all object files into the final executable
$(BINARY): $(OBJ_FILES)
	$(CXX) -o $@ $(OBJ_FILES) $(LDFLAGS)

################################################################################

.PHONY: all rebuild clean run
