
LIBS_RELPATH=../../../x3me_libs

include $(LIBS_RELPATH)/protobufconfig.sh

cpp_out:
	$(LIBS_RELPATH)/$(PROTOBUF_INCLUDE_DIR)/protoc --cpp_out=./ ./pb_stats.proto

js_out:
	$(LIBS_RELPATH)/$(PROTOBUF_INCLUDE_DIR)/protoc --js_out=./ ./pb_stats.proto

desc:
	$(LIBS_RELPATH)/$(PROTOBUF_INCLUDE_DIR)/protoc \
		--descriptor_set_out=pb_stats.desc --include_imports pb_stats.proto

clean:
	rm -f pb_stats.pb.h pb_stats.pb.cc pb_stats.desc

.PHONY: proto desc clean
