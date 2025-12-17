INCLUDE_FILES=include/building.h include/planning.h include/util.h include/valuation.h include/building_shape.h include/sqlitedb.h
OBJ_FILES=obj/util.o obj/building_shape.o obj/building.o obj/valuation.o obj/planning.o obj/vector_tile.pb.o
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
	make refresh
	make exe
	make bin/vector_tile_test

# EXE
exe: $(EXE_FILE)
$(EXE_FILE): obj/server.o $(OBJ_FILES)
	g++ -o $(EXE_FILE) $(EXTERNAL_LIBS) $(CXX_STD) $(OBJ_FILES) obj/server.o
	chmod ugo+x $(EXE_FILE)
obj/server.o: $(INCLUDE_FILES) src/server.cpp
	g++ -c -o obj/server.o $(CXX_STD) src/server.cpp 

# OBJ_FILES
obj/util.o: include/util.h src/util.cpp
	g++ -c -o obj/util.o $(CXX_STD) src/util.cpp
obj/building_shape.o: include/building_shape.h src/building_shape.cpp
	g++ -c -o obj/building_shape.o $(CXX_STD) src/building_shape.cpp
obj/building.o: include/building.h src/building.cpp
	g++ -c -o obj/building.o $(CXX_STD) src/building.cpp
obj/valuation.o: include/valuation.h src/valuation.cpp
	g++ -c -o obj/valuation.o $(CXX_STD) src/valuation.cpp
obj/planning.o: include/planning.h src/planning.cpp
	g++ -c -o obj/planning.o $(CXX_STD) src/planning.cpp
obj/sqlitedb.o: include/sqlitedb.h src/sqlitedb.cpp
	g++ -c -o obj/sqlitedb.o $(CXX_STD) src/sqlitedb.cpp
obj/vector_tile.pb.o: src/tiles/vector_tile.pb.cc
	g++ -c -o obj/vector_tile.pb.o src/tiles/vector_tile.pb.cc $(CXX_STD)


# Tiles test
TILES_TEST_INCLUDE=include/util.h include/building_shape.h
TILES_TEST_OBJ=obj/util.o obj/building_shape.o obj/vector_tile.pb.o obj/vector_tile_test.o

bin/vector_tile_test: $(TILES_TEST_OBJ)
	g++ -o bin/vector_tile_test $(CXX_STD) $(EXTERNAL_LIBS) $(TILES_TEST_OBJ)
	chmod ugo+x bin/vector_tile_test
obj/vector_tile_test.o: src/tiles/vector_tile_test.cpp $(TILES_TEST_INCLUDE)
	g++ -c -o obj/vector_tile_test.o src/tiles/vector_tile_test.cpp $(CXX_STD)

init:
	mkdir bin obj

clean:
	@ echo "Removing generated files"
	- rm -rf obj
	- rm -rf bin

refresh:
	make clean
	make init
