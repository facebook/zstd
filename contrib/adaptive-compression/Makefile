
ZSTDDIR = ../../lib
ZSTDCOMMON_FILES := $(ZSTDDIR)/common/*.c
ZSTDCOMP_FILES   := $(ZSTDDIR)/compress/*.c
ZSTDDECOMP_FILES := $(ZSTDDIR)/decompress/*.c
ZSTD_FILES  := $(ZSTDDECOMP_FILES) $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES)

DEBUGFLAGS= -g -DZSTD_DEBUG=1
CPPFLAGS += -I$(ZSTDDIR) -I$(ZSTDDIR)/common -I$(ZSTDDIR)/compress \
            -I$(ZSTDDIR)/dictBuilder -I$(ZSTDDIR)/deprecated -I$(PRGDIR)
CFLAGS   ?= -O3
CFLAGS   += -Wall -Wextra -Wcast-qual -Wcast-align -Wshadow                 \
            -Wstrict-aliasing=1 -Wswitch-enum -Wdeclaration-after-statement \
            -Wstrict-prototypes -Wundef -Wformat-security                   \
            -Wvla -Wformat=2 -Winit-self -Wfloat-equal -Wwrite-strings      \
            -Wredundant-decls
CFLAGS   += $(DEBUGFLAGS)
CFLAGS   += $(MOREFLAGS)
FLAGS     = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS)

all: clean single multi
single: $(ZSTD_FILES) single.c
	$(CC) $(FLAGS) $^ -o $@

multi: $(ZSTD_FILES) multi.c
	$(CC) $(FLAGS) $^ -o $@

clean:
	@$(RM) -f single multi
	@$(RM) -rf *.dSYM
	@$(RM) -f tmp*
	@$(RM) -f tests/*.zst
	@$(RM) -f tests/tmp*
	@echo "finished cleaning"
