INCLUDE_FILES=include/building.h include/longest_common_substr.h include/planning.h include/util.h include/valuation.h
EXE_FILE=bin/server

all:
	make clean
	mkdir bin obj
	make exe

exe: $(EXE_FILE)

$(EXE_FILE): obj/server.o
	g++ -o $(EXE_FILE) -L/usr/local/opt/proj/lib -lproj -L/usr/local/opt/curl/lib -lcurl -L/usr/local/opt/sqlite/lib \
		-lsqlite3 obj/server.o
	chmod ugo+x $(EXE_FILE)

obj/server.o: $(INCLUDE_FILES)
	g++ -I/usr/local/opt/proj/include -I/usr/local/opt/curl/include -I/usr/local/opt/nlohmann-json/include \
		-I/usr/local/opt/sqlite/include -c -o obj/server.o -std=c++11 server.cpp 

clean:
	@ echo "Removing generated files"
	- rm -rf obj/*
	- rm -rf bin/*
