PROGRAM_FILES := ../../programs/fileio.c

TEST_INPUT := ../../lib
TEST_OUTPUT := randomDict
ARG :=

all: main testrun test clean

run: main rand clean

.PHONY: rand
rand:
	echo "Building a random dictionary with given arguments"
	./main $(ARG)


main: main.o random.o libzstd.a
	gcc main.o random.o libzstd.a -o main

main.o: main.c $(PROGRAM_FILES)
	gcc -c main.c $(PROGRAM_FILES) -I random.h -I ../../programs -I ../../lib/common -I ../../lib -I ../../lib/dictBuilder

random.o: random.c
	gcc -c random.c -I random.h -I ../../lib/common -I ../../lib/dictBuilder

libzstd.a:
	$(MAKE) -C ../../lib libzstd.a
	mv ../../lib/libzstd.a .

.PHONY: testrun
testrun: main
	echo "Run with $(TEST_INPUT) and $(TEST_OUTPUT) "
	./main in=$(TEST_INPUT) out=$(TEST_OUTPUT)
	zstd -be3 -D $(TEST_OUTPUT) -r $(TEST_INPUT) -q
	rm -f $(TEST_OUTPUT)

.PHONY: test
test: test.sh
	sh test.sh
	echo "Finish running test.sh"

.PHONY: clean
clean:
	rm -f libzstd.a main
	rm -f ../../lib/*/*.o
	rm -f ../../programs/*.o
	rm -f *.o
	echo "Cleaning is completed"
