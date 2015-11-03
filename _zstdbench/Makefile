#BUILD_SYSTEM = linux
BUILD_ARCH = 64-bit

ifeq ($(BUILD_SYSTEM),linux)
	DEFINES += -DFREEARC_UNIX
	LDFLAGS = -lrt
	GCC		= gcc
	GPP		= g++
else
#	DEFINES = -march=core2 -march=nocona -march=k8 -march=native
	DEFINES += -DFREEARC_WIN
	LDFLAGS	= -lshell32 -lole32 -loleaut32 
	GCC		= gcc.exe
	GPP		= g++.exe
endif


ifeq ($(BUILD_ARCH),64-bit)
	DEFINES	+= -D__x86_64__
	LDFLAGS	+= -static  -L C:\Aplikacje\win-builds64\lib
else
	LDFLAGS	+= -static  -L C:\Aplikacje\win-builds32\lib
endif


#DEFINES		+= -DBENCH_REMOVE_XXX
DEFINES		+= -I. -DFREEARC_INTEL_BYTE_ORDER -D_UNICODE -DUNICODE
CODE_FLAGS  = -Wno-unknown-pragmas -Wno-sign-compare -Wno-conversion
OPT_FLAGS   = -fomit-frame-pointer -fstrict-aliasing -fforce-addr -ffast-math

#BUILD_TYPE = debug

ifeq ($(BUILD_TYPE),debug)
	OPT_FLAGS += -g
else
	OPT_FLAGS += -O3 -DNDEBUG
endif

CFLAGS = $(CODE_FLAGS) $(OPT_FLAGS) $(DEFINES)


all: zstdbench


ZSTD_FILES = ../lib/fse.o ../lib/huff0.o ../lib/zstd.o ../lib/zstdhc.o 


zstdbench: $(ZSTD_FILES) zstdbench.o compressors.o
	$(GPP) $^ -o $@ $(LDFLAGS)

.c.o:
	$(GCC) $(CFLAGS) $< -std=c99 -c -o $@

.cc.o:
	$(GPP) $(CFLAGS) $< -c -o $@

.cpp.o:
	$(GPP) $(CFLAGS) $< -c -o $@

clean:
	rm -f *.o ../lib/*.o *.exe
