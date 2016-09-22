# Makefile for example of using zstd wrapper for zlib
#
# make - compiles statically and dynamically linked examples
# make LOC=-DZWRAP_USE_ZSTD=1 - compiles statically and dynamically linked examples with zstd compression turned on
# make test test_d - runs statically and dynamically linked examples


# Paths to static and dynamic zlib and zstd libraries
# Use "make ZLIBDIR=path/to/zlib" to select a path to library
ifdef ZLIBDIR
STATICLIB = $(ZLIBDIR)/libz.a ../lib/libzstd.a
IMPLIB    = $(ZLIBDIR)/libz.dll.a ../lib/libzstd.a
else
STATICLIB = -static -lz ../lib/libzstd.a
IMPLIB    = -lz ../lib/libzstd.a
ZLIBDIR   = .
endif

ZLIBWRAPPER_PATH = .
EXAMPLE_PATH = examples
PROGRAMS_PATH = ../programs
CC ?= gcc
CFLAGS = $(LOC) -I$(PROGRAMS_PATH) -I../lib -I../lib/common -I$(ZLIBWRAPPER_PATH) -I$(ZLIBDIR) -O3 -std=gnu90
CFLAGS += -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wswitch-enum -Wdeclaration-after-statement -Wstrict-prototypes -Wundef
LDFLAGS = $(LOC)
RM = rm -f


all: clean fitblk example example_d zwrapbench

test: example fitblk
	./example
	./fitblk 10240 <../zstd_compression_format.md
	./fitblk 40960 <../zstd_compression_format.md

test_d: example_d
	./example_d

test_zstd: example_zstd fitblk_zstd zwrapbench
	./example_zstd
	./fitblk_zstd 10240 <../zstd_compression_format.md
	./fitblk_zstd 40960 <../zstd_compression_format.md
	./zwrapbench ../zstd_compression_format.md

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<
 
example: $(EXAMPLE_PATH)/example.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o
	$(CC) $(LDFLAGS) -o $@ $(EXAMPLE_PATH)/example.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o $(STATICLIB)

example_d: $(EXAMPLE_PATH)/example.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o
	$(CC) $(LDFLAGS) -o $@ $(EXAMPLE_PATH)/example.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o $(IMPLIB)

example_zstd: $(EXAMPLE_PATH)/example.o $(ZLIBWRAPPER_PATH)/zstdTurnedOn_zlibwrapper.o
	$(CC) $(LDFLAGS)  -o $@ $(EXAMPLE_PATH)/example.o $(ZLIBWRAPPER_PATH)/zstdTurnedOn_zlibwrapper.o $(STATICLIB)

fitblk: $(EXAMPLE_PATH)/fitblk.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o
	$(CC) $(LDFLAGS) -o $@ $(EXAMPLE_PATH)/fitblk.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o $(STATICLIB)

fitblk_zstd: $(EXAMPLE_PATH)/fitblk.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o
	$(CC) $(LDFLAGS) -o $@ $(EXAMPLE_PATH)/fitblk.o $(ZLIBWRAPPER_PATH)/zstdTurnedOn_zlibwrapper.o $(STATICLIB)

zwrapbench: $(EXAMPLE_PATH)/zwrapbench.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o $(PROGRAMS_PATH)/datagen.o
	$(CC) $(LDFLAGS) -o $@ $^ $(STATICLIB)

$(EXAMPLE_PATH)/zwrapbench.o: $(EXAMPLE_PATH)/zwrapbench.c

$(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o: $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.c $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.h
	$(CC) $(CFLAGS) -I. -c -o $@ $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.c

$(ZLIBWRAPPER_PATH)/zstdTurnedOn_zlibwrapper.o: $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.c $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.h
	$(CC) $(CFLAGS) -DZWRAP_USE_ZSTD=1 -I. -c -o $@ $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.c

clean:
	-$(RM) $(ZLIBWRAPPER_PATH)/*.o $(EXAMPLE_PATH)/*.o *.o *.exe foo.gz example example_d example_zstd fitblk fitblk_zstd
	@echo Cleaning completed
