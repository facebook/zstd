
LINUX := ../linux
LINUX_ZSTDLIB := $(LINUX)/lib/zstd

CPPFLAGS += -I$(LINUX)/include -I$(LINUX_ZSTDLIB) -Iinclude -DNDEBUG
# Don't poison the workspace, it currently doesn't work with static allocation and workspace reuse
CPPFLAGS += -DZSTD_ASAN_DONT_POISON_WORKSPACE

LINUX_ZSTD_MODULE     := $(wildcard $(LINUX_ZSTDLIB)/*.c)
LINUX_ZSTD_COMMON     := $(wildcard $(LINUX_ZSTDLIB)/common/*.c) 
LINUX_ZSTD_COMPRESS   := $(wildcard $(LINUX_ZSTDLIB)/compress/*.c) 
LINUX_ZSTD_DECOMPRESS := $(wildcard $(LINUX_ZSTDLIB)/decompress/*.c) 
LINUX_ZSTD_FILES      := $(LINUX_ZSTD_MODULE) $(LINUX_ZSTD_COMMON) $(LINUX_ZSTD_COMPRESS) $(LINUX_ZSTD_DECOMPRESS)
LINUX_ZSTD_OBJECTS    := $(LINUX_ZSTD_FILES:.c=.o)

liblinuxzstd.a: $(LINUX_ZSTD_OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

test: test.c liblinuxzstd.a
	$(CC) $(LDFLAGS) $(CPPFLAGS) $(CFLAGS) $^ -o $@

static_test: static_test.c
	$(CC) $(LDFLAGS) $(CPPFLAGS) $(CFLAGS) $^ -o $@

run-test: test static_test
	./macro-test.sh
	./test
	./static_test

.PHONY:
clean:
	$(RM) -f $(LINUX_ZSTDLIB)/**/*.o
	$(RM) -f *.o *.a
	$(RM) -f test
