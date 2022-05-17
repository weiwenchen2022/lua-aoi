CC ?= gcc
CFLAGS = -g -O2 -Wall -I$(LUA_INC)
SHARED := -fPIC --shared

LUA_INC = /usr/include/lua

.PHONY : all
all : \
	aoi.so \
	test \
	testmap

aoi.so : lua-aoi.c aoi.c
	$(CC) $(CFLAGS) $(SHARED) -I$(LUA_INC) -o $@ $^

test : test.o aoi.o
	$(CC) $(CFLAGS) -o $@ $^

testmap : testmap.o
	$(CC) $(CFLAGS) -o $@ $^

testmap.o : testmap.c aoi.c aoi.h

test.o : test.c aoi.h
aoi.o : aoi.c aoi.h

.PHONY : clean
clean :
	rm -f aoi.so test testmap *.o
