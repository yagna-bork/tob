INCLUDE_FILES=include/building.h include/longest_common_substr.h include/planning.h include/util.h include/valuation.h
EXE_FILE=bin/server
CXX_STD=-std=c++17
ABSL_LOG_INTERNAL_LIBS=-labsl_log_internal_check_op -labsl_log_internal_conditions \
						 -labsl_log_internal_fnmatch -labsl_log_internal_format \
						 -labsl_log_internal_globals -labsl_log_internal_log_sink_set \
						 -labsl_log_internal_message -labsl_log_internal_nullguard \
						 -labsl_log_internal_proto -labsl_log_internal_structured_proto
PROTOBUF_LIBS=-L/usr/local/opt/protobuf/lib -lprotobuf-lite -lprotobuf \
			  -L/usr/local/opt/abseil/lib $(ABSL_LOG_INTERNAL_LIBS)
EXTERNAL_LIBS=-L/usr/local/opt/proj/lib -lproj -L/usr/local/opt/curl/lib -lcurl -L/usr/local/opt/sqlite/lib -lsqlite3 \
			  $(PROTOBUF_LIBS)

all:
	make clean
	make init
	make exe
	make bin/vector_tile_test

exe: $(EXE_FILE)

$(EXE_FILE): obj/server.o
	g++ -o $(EXE_FILE) $(EXTERNAL_LIBS) obj/server.o
	chmod ugo+x $(EXE_FILE)

obj/server.o: $(INCLUDE_FILES)
	g++ -c -o obj/server.o $(CXX_STD) server.cpp 

bin/vector_tile_test: obj/vector_tile_test.o obj/vector_tile_pb.o
	g++ -o bin/vector_tile_test $(CXX_STD) $(EXTERNAL_LIBS) obj/vector_tile_test.o obj/vector_tile_pb.o
	chmod ugo+x bin/vector_tile_test

obj/vector_tile_pb.o: tiles/vector_tile.pb.cc
	g++ -c -o obj/vector_tile_pb.o tiles/vector_tile.pb.cc $(CXX_STD)

obj/vector_tile_test.o: tiles/vector_tile_test.cpp tiles/vector_tile.pb.cc include/util.h include/building_shape.h
	g++ -c -o obj/vector_tile_test.o tiles/vector_tile_test.cpp $(CXX_STD)

init:
	mkdir bin obj

clean:
	@ echo "Removing generated files"
	- rm -rf obj
	- rm -rf bin
