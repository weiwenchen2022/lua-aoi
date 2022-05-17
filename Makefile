CC = gcc
CFLAGS = $(LUA_INC:%=-I%) -Wall -Wshadow -Wextra \
	 -Wimplicit -O2 -ggdb3 -fpic
LUA_INC = /usr/include/lua

LD = gcc
LDFLAGS = -O -shared -fpic

.PHONY : all
all : \
	aoi.so \
	# test \
	# testmap

aoi.so : lua-aoi.o aoi.o
	$(LD) $(LDFLAGS) -o $@ $^

test : test.o aoi.o
	$(CC) $(CFLAGS) -o $@ $^

testmap : testmap.o
	$(CC) $(CFLAGS) -o $@ $^

# List of dependencies
aoi.o : aoi.c aoi.h
test.o : test.c aoi.h
testmap.o : testmap.c aoi.c aoi.h

.PHONY : clean
clean :
	rm -f aoi.so test testmap *.o
