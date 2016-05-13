# Makefile for example of using zstd wrapper for zlib
#
# make - compiles statically and dynamically linked examples/example.c
# make test testdll - compiles and runs statically and dynamically linked examples/example.c
# make LOC=-DZWRAP_USE_ZSTD=1 - compiles statically and dynamically linked examples/example.c with zstd compression turned on


# Paths to static and dynamic zlib and zstd libraries
ifneq (,$(filter Windows%,$(OS)))
STATICLIB = ../../zlib/libz.a ../lib/libzstd.a
IMPLIB    = ../../zlib/libz.dll.a ../lib/libzstd.a
else
STATICLIB = -static -lz ../lib/libzstd.a
IMPLIB    = -lz ../lib/libzstd.a
endif

ZLIBWRAPPER_PATH = .
EXAMPLE_PATH = examples/
CC = gcc
CFLAGS = $(LOC) -I../lib/common -I$(ZLIBWRAPPER_PATH) -O3 -Wall -std=gnu89
LDFLAGS = $(LOC)
RM = rm -f


all: example example_d

test: example
	./example

testdll: example_d
	./example_d

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<

example: $(EXAMPLE_PATH)/example.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o
	$(CC) $(LDFLAGS) -o $@ $(EXAMPLE_PATH)/example.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o $(STATICLIB)

example_d: $(EXAMPLE_PATH)/example.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o
	$(CC) $(LDFLAGS) -o $@ $(EXAMPLE_PATH)/example.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o $(IMPLIB)

$(EXAMPLE_PATH)/example.o: $(EXAMPLE_PATH)/example.c
	$(CC) $(CFLAGS) -I. -c -o $@ $(EXAMPLE_PATH)/example.c

$(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o: $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.c $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.h
	$(CC) $(CFLAGS) -I. -c -o $@ $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.c

clean:
	-$(RM) $(ZLIBWRAPPER_PATH)/*.o
	-$(RM) $(EXAMPLE_PATH)/*.o
	-$(RM) *.o
	-$(RM) *.exe
	-$(RM) foo.gz

