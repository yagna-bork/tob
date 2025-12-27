INCLUDE_FILES=include/building.h include/planning.h include/util.h include/valuation.h include/building_shape.h include/sqlitedb.h include/httplib.h

OBJ_FILES=obj/util.o obj/building_shape.o obj/building.o obj/valuation.o obj/planning.o obj/httplib.o obj/vector_tile.pb.o

EXE_FILE=bin/server
CXX_STD=-std=c++17

EXTERNAL_LIBS=`pkg-config --libs absl_base protobuf sqlite3 proj libcurl`
# For some reason macports doesn't install proj into regular include path
PROJ_INCLUDE=`pkg-config --cflags-only-I proj`

all:
	make refresh
	make exe
	make bin/vector_tile_test

init:
	mkdir bin obj

clean:
	@ echo "Removing generated files"
	- rm -rf obj
	- rm -rf bin

refresh:
	make clean
	make init

# EXE
exe $(EXE_FILE): obj/server.o $(OBJ_FILES)
	g++ -o $(EXE_FILE) $(EXTERNAL_LIBS) $(CXX_STD) $(OBJ_FILES) obj/server.o
	chmod ugo+x $(EXE_FILE)
obj/server.o: $(INCLUDE_FILES) src/server.cpp
	g++ -c -o obj/server.o $(CXX_STD) src/server.cpp $(PROJ_INCLUDE)

# OBJ_FILES
obj/util.o: include/util.h src/util.cpp
	g++ -c -o obj/util.o $(CXX_STD) src/util.cpp $(PROJ_INCLUDE)
obj/building_shape.o: include/building_shape.h src/building_shape.cpp
	g++ -c -o obj/building_shape.o $(CXX_STD) src/building_shape.cpp $(PROJ_INCLUDE)
obj/building.o: include/building.h src/building.cpp
	g++ -c -o obj/building.o $(CXX_STD) src/building.cpp $(PROJ_INCLUDE)
obj/valuation.o: include/valuation.h src/valuation.cpp
	g++ -c -o obj/valuation.o $(CXX_STD) src/valuation.cpp $(PROJ_INCLUDE)
obj/planning.o: include/planning.h src/planning.cpp
	g++ -c -o obj/planning.o $(CXX_STD) src/planning.cpp $(PROJ_INCLUDE)
obj/sqlitedb.o: include/sqlitedb.h src/sqlitedb.cpp
	g++ -c -o obj/sqlitedb.o $(CXX_STD) src/sqlitedb.cpp $(PROJ_INCLUDE)
obj/httplib.o: include/httplib.h src/httplib.cpp
	g++ -c -o obj/httplib.o $(CXX_STD) src/httplib.cpp
obj/vector_tile.pb.o: src/tiles/vector_tile.pb.cc
	g++ -c -o obj/vector_tile.pb.o src/tiles/vector_tile.pb.cc $(CXX_STD)


# Tiles test
TILES_TEST_INCLUDE=include/util.h include/building_shape.h
TILES_TEST_OBJ=obj/util.o obj/building_shape.o obj/vector_tile.pb.o obj/vector_tile_test.o

vector_test bin/vector_tile_test: $(TILES_TEST_OBJ)
	g++ -o bin/vector_tile_test $(CXX_STD) $(EXTERNAL_LIBS) $(TILES_TEST_OBJ)
	chmod ugo+x bin/vector_tile_test
obj/vector_tile_test.o: src/tiles/vector_tile_test.cpp $(TILES_TEST_INCLUDE)
	g++ -c -o obj/vector_tile_test.o src/tiles/vector_tile_test.cpp $(CXX_STD) \
		$(PROJ_INCLUDE)

# Visualiser
visualiser bin/visualiser: src/visualiser/visualiser.cpp
	g++ -o bin/visualiser --std=c++17 `pkg-config --libs SDL3-ttf protobuf` \
		src/visualiser/visualiser.cpp src/tiles/vector_tile.pb.cc
	chmod ugo+x bin/visualiser
