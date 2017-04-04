
IFLAGS := -isystem include/ -I ../include/ -I ../lib/zstd/ -isystem googletest/googletest/include

SOURCES := $(wildcard ../lib/zstd/*.c)
OBJECTS := $(patsubst %.c,%.o,$(SOURCES))

ARFLAGS := rcs
CXXFLAGS += -std=c++11
CFLAGS += -g -O0
CPPFLAGS += $(IFLAGS)

../lib/zstd/libzstd.a: $(OBJECTS)
	$(AR) $(ARFLAGS) $@ $^

UserlandTest: UserlandTest.cpp ../lib/zstd/libzstd.a
	$(CXX) $(CXXFLAGS) $(CFLAGS) $(CPPFLAGS) $^ googletest/build/googlemock/gtest/libgtest.a googletest/build/googlemock/gtest/libgtest_main.a -o $@

# Install googletest
.PHONY: googletest
googletest:
	@$(RM) -rf googletest
	@git clone https://github.com/google/googletest
	@mkdir -p googletest/build
	@cd googletest/build && cmake .. && $(MAKE)

clean:
	$(RM) -f *.{o,a} ../lib/zstd/*.{o,a}
