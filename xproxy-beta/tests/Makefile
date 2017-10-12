LIBS_RELPATH=../../x3me_libs
X3ME_LIBS_RELPATH=$(LIBS_RELPATH)/x3me_libs

include $(LIBS_RELPATH)/boosthttpconfig.sh
include $(LIBS_RELPATH)/expectedconfig.sh
include $(LIBS_RELPATH)/fmtconfig.sh
include $(LIBS_RELPATH)/httpparserconfig.sh
include $(LIBS_RELPATH)/smlconfig.sh
include $(LIBS_RELPATH)/sparsehashconfig.sh

PROJECT_COMPILER_FLAGS=\
					   -DX3ME_TEST \
					   -isystem $(LIBS_RELPATH)/$(BOOST_HTTP_INCLUDE_DIR) \
					   -isystem $(LIBS_RELPATH)/$(BOOST_EXPECTED_INCLUDE_DIR) \
					   -isystem $(LIBS_RELPATH)/$(FMT_INCLUDE_DIR) \
					   -isystem $(LIBS_RELPATH)/$(HTTP_PARSER_INCLUDE_DIR) \
					   -isystem $(LIBS_RELPATH)/$(SML_INCLUDE_DIR) \
					   -isystem$(LIBS_RELPATH)/$(SPARSEHASH_INCLUDE_DIR) \
					   -include $(PRECOMPILED_HEADER) \
					   -I. \
					   -I.. \
					   -fno-omit-frame-pointer \
					   -fsanitize=address

PROJECT_LINKER_FLAGS=\
					 -L$(LIBS_RELPATH)/$(FMT_LIB_DIR) \
					 -L$(LIBS_RELPATH)/$(HTTP_PARSER_LIB_DIR) \
					 -lboost_unit_test_framework \
					 -lboost_system \
					 -lfmt \
					 -lhttp_parser \
					 -lasan \
					 -lcrypto \
					 -lpcre \
					 -lz

PROJECT_BINARY=tests.bin

PROJECT_CPP_FILES=\
				  $(wildcard *.cpp) \
				  $(wildcard cache/*.cpp) \
				  $(wildcard http/*.cpp) \
				  $(wildcard plgns/*.cpp) \
				  ../id_tag.cpp \
				  ../cache/agg_writer.cpp \
				  ../cache/agg_write_block.cpp \
				  ../cache/agg_write_meta.cpp \
				  ../cache/aio_service.cpp \
				  ../cache/aio_task_queue.cpp \
				  ../cache/aligned_data_ptr.cpp \
				  ../cache/buffer.cpp \
				  ../cache/cache_error.cpp \
				  ../cache/cache_fs_operations.cpp \
				  ../cache/cache_key.cpp \
				  ../cache/disk_reader.cpp \
				  ../cache/frag_write_buff.cpp \
				  ../cache/fs_table.cpp \
				  ../cache/fs_metadata.cpp \
				  ../cache/fs_metadata_hdr.cpp \
				  ../cache/object_read_handle.cpp \
				  ../cache/object_write_handle.cpp \
				  ../cache/range_elem.cpp \
				  ../cache/range_vector.cpp \
				  ../cache/read_buffers.cpp \
				  ../cache/read_transaction.cpp \
				  ../cache/task_md_sync.cpp \
				  ../cache/volume_fd.cpp \
				  ../cache/volume_info.cpp \
				  ../cache/write_buffers.cpp \
				  ../cache/write_transaction.cpp \
				  ../http/http_date.cpp \
				  ../http/http_stats.cpp \
				  ../http/http_trans.cpp \
				  ../http/interesting_hdrs.cpp \
				  ../plgns/cache_url.cpp \
				  ../xlog/async_channel.cpp \
				  ../xlog/async_channel_impl.cpp \
				  ../xlog/log_file.cpp \
				  ../xlog/log_target.cpp \
				  ../xlog/log_target_impl.cpp \
				  ../xlog/shared_queue.cpp \
				  ../xutils/io_buff.cpp \
				  ../xutils/io_buff_reader.cpp \
				  ../xutils/pcrex.cpp \
				  $(X3ME_LIBS_RELPATH)/sys_utils.cpp \
				  $(X3ME_LIBS_RELPATH)/x3me_assert.cpp

include $(LIBS_RELPATH)/common.mk

test:
	./$(PROJECT_BINARY) --log_level=error --report_level=short -t disk_reader_tests

.PHONY: test
