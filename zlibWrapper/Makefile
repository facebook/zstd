# Makefile for example of using zstd wrapper for zlib
#
# make - compiles statically and dynamically linked examples
# make LOC=-DZWRAP_USE_ZSTD=1 - compiles statically and dynamically linked examples with zstd compression turned on
# make test test_d - runs statically and dynamically linked examples


# Paths to static and dynamic zlib and zstd libraries
# Use "make ZLIB_LIBRARY=path/to/zlib" to select a path to library
ZLIB_LIBRARY ?= -lz

ZSTDLIBDIR = ../lib
ZSTDLIBRARY = $(ZSTDLIBDIR)/libzstd.a
ZLIBWRAPPER_PATH = .
EXAMPLE_PATH = examples
PROGRAMS_PATH = ../programs
CC ?= gcc
CFLAGS ?= -O3 
CFLAGS += $(LOC) -I$(PROGRAMS_PATH) -I$(ZSTDLIBDIR) -I$(ZSTDLIBDIR)/common -I$(ZLIBWRAPPER_PATH) -std=gnu90
CFLAGS += -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow -Wswitch-enum -Wdeclaration-after-statement -Wstrict-prototypes -Wundef
LDFLAGS = $(LOC)
RM = rm -f


all: clean fitblk example example_d zwrapbench

test: example fitblk example_zstd fitblk_zstd zwrapbench
	./example
	./example_zstd
	./fitblk 10240 <../zstd_compression_format.md
	./fitblk 40960 <../zstd_compression_format.md
	./fitblk_zstd 10240 <../zstd_compression_format.md
	./fitblk_zstd 40960 <../zstd_compression_format.md
	./zwrapbench -qb1e5 ../zstd_compression_format.md
	./zwrapbench -qb1e5B1K ../zstd_compression_format.md

test_d: example_d
	./example_d

valgrindTest: VALGRIND = LD_LIBRARY_PATH=$(ZSTDLIBDIR) valgrind --track-origins=yes --leak-check=full --error-exitcode=1
valgrindTest: ZSTDLIBRARY = $(ZSTDLIBDIR)/libzstd.so
valgrindTest: clean example fitblk example_zstd fitblk_zstd zwrapbench
	@echo "\n ---- valgrind tests ----"
	$(VALGRIND) ./example
	$(VALGRIND) ./example_zstd
	$(VALGRIND) ./fitblk 10240 <../zstd_compression_format.md
	$(VALGRIND) ./fitblk 40960 <../zstd_compression_format.md
	$(VALGRIND) ./fitblk_zstd 10240 <../zstd_compression_format.md
	$(VALGRIND) ./fitblk_zstd 40960 <../zstd_compression_format.md
	$(VALGRIND) ./zwrapbench -qb1e5 ../zstd_compression_format.md
	$(VALGRIND) ./zwrapbench -qb1e5B1K ../zstd_compression_format.md

.c.o:
	$(CC) $(CFLAGS) -c -o $@ $<
 
example: $(EXAMPLE_PATH)/example.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o $(ZSTDLIBRARY)
	$(CC) $(LDFLAGS) -o $@ $(EXAMPLE_PATH)/example.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o $(ZSTDLIBRARY) $(ZLIB_LIBRARY)

example_zstd: $(EXAMPLE_PATH)/example.o $(ZLIBWRAPPER_PATH)/zstdTurnedOn_zlibwrapper.o $(ZSTDLIBRARY)
	$(CC) $(LDFLAGS)  -o $@ $(EXAMPLE_PATH)/example.o $(ZLIBWRAPPER_PATH)/zstdTurnedOn_zlibwrapper.o $(ZSTDLIBRARY) $(ZLIB_LIBRARY)

fitblk: $(EXAMPLE_PATH)/fitblk.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o $(ZSTDLIBRARY)
	$(CC) $(LDFLAGS) -o $@ $(EXAMPLE_PATH)/fitblk.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o $(ZSTDLIBRARY) $(ZLIB_LIBRARY)

fitblk_zstd: $(EXAMPLE_PATH)/fitblk.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o $(ZSTDLIBRARY)
	$(CC) $(LDFLAGS) -o $@ $(EXAMPLE_PATH)/fitblk.o $(ZLIBWRAPPER_PATH)/zstdTurnedOn_zlibwrapper.o $(ZSTDLIBRARY) $(ZLIB_LIBRARY)

zwrapbench: $(EXAMPLE_PATH)/zwrapbench.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o $(PROGRAMS_PATH)/datagen.o $(ZSTDLIBRARY)
	$(CC) $(LDFLAGS) -o $@ $(EXAMPLE_PATH)/zwrapbench.o $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o $(PROGRAMS_PATH)/datagen.o $(ZSTDLIBRARY) $(ZLIB_LIBRARY)

$(EXAMPLE_PATH)/zwrapbench.o: $(EXAMPLE_PATH)/zwrapbench.c

$(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.o: $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.c $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.h
	$(CC) $(CFLAGS) -I. -c -o $@ $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.c

$(ZLIBWRAPPER_PATH)/zstdTurnedOn_zlibwrapper.o: $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.c $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.h
	$(CC) $(CFLAGS) -DZWRAP_USE_ZSTD=1 -I. -c -o $@ $(ZLIBWRAPPER_PATH)/zstd_zlibwrapper.c

$(ZSTDLIBDIR)/libzstd.a:
	$(MAKE) -C $(ZSTDLIBDIR) all

$(ZSTDLIBDIR)/libzstd.so:
	$(MAKE) -C $(ZSTDLIBDIR) all

clean:
	-$(RM) $(ZLIBWRAPPER_PATH)/*.o $(EXAMPLE_PATH)/*.o *.o *.exe foo.gz example example_d example_zstd fitblk fitblk_zstd zwrapbench
	@echo Cleaning completed
