# This Makefile presumes libzstd is installed, using `sudo make install`

CFLAGS  ?= -O3
DEBUGFLAGS = -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow \
            -Wstrict-aliasing=1 -Wswitch-enum -Wdeclaration-after-statement \
            -Wstrict-prototypes -Wundef -Wpointer-arith -Wformat-security \
            -Wvla -Wformat=2 -Winit-self -Wfloat-equal -Wwrite-strings \
            -Wredundant-decls
CFLAGS  += $(DEBUGFLAGS) $(MOREFLAGS)
FLAGS    = $(CPPFLAGS) $(CFLAGS)

LDFLAGS += -lzstd

.PHONY: default all clean

default: all

all: main-ldm

main-ldm : util.c ldm.c main-ldm.c
	$(CC) $(CPPFLAGS) $(CFLAGS) $^ $(LDFLAGS) -o $@

clean:
	@rm -f core *.o tmp* result* *.ldm *.ldm.dec \
	main-ldm
	@echo Cleaning completed

