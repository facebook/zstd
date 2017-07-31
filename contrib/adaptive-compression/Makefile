
ZSTDDIR = ../../lib
PRGDIR  = ../../programs
ZSTDCOMMON_FILES := $(ZSTDDIR)/common/*.c
ZSTDCOMP_FILES   := $(ZSTDDIR)/compress/*.c
ZSTDDECOMP_FILES := $(ZSTDDIR)/decompress/*.c
ZSTD_FILES  := $(ZSTDDECOMP_FILES) $(ZSTDCOMMON_FILES) $(ZSTDCOMP_FILES)

MULTITHREAD_LDFLAGS = -pthread
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
FLAGS     = $(CPPFLAGS) $(CFLAGS) $(LDFLAGS) $(MULTITHREAD_LDFLAGS)

all: adapt datagen

adapt: $(ZSTD_FILES) adapt.c
	$(CC) $(FLAGS) $^ -o $@

adapt-debug: $(ZSTD_FILES) adapt.c
	$(CC) $(FLAGS) -DDEBUG_MODE=2 $^ -o adapt

datagen : $(PRGDIR)/datagen.c datagencli.c
	$(CC)      $(FLAGS) $^ -o $@

test-adapt-correctness: datagen adapt
	@./test-correctness.sh
	@echo "test correctness complete"

test-adapt-performance: datagen adapt
	@./test-performance.sh
	@echo "test performance complete"

clean:
	@$(RM) -f adapt datagen
	@$(RM) -rf *.dSYM
	@$(RM) -f tmp*
	@$(RM) -f tests/*.zst
	@$(RM) -f tests/tmp*
	@echo "finished cleaning"
