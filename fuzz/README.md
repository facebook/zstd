# Fuzzing

Each fuzzing target can be built with multiple engines.

## LibFuzzer

You can install `libFuzzer` with `make libFuzzer`. Then you can make each target
with `make target LDFLAGS=-L. CC=clang CXX=clang++`.

## AFL

The regression driver also serves as a binary for `afl-fuzz`. You can make each
target with one of these commands:

```
make target-regression CC=afl-clang CXX=afl-clang++
AFL_MSAN=1 make target-regression-msan CC=afl-clang CXX=afl-clang++
AFL_ASAN=1 make target-regression-uasan CC=afl-clang CXX=afl-clang++
```

Then run as `./target @@`.

## Regression Testing

Each fuzz target has a corpus checked into the repo under `fuzz/corpora/`.
You can run regression tests on the corpora to ensure that inputs which
previously exposed bugs still pass. You can make these targets to run the
regression tests with different sanitizers.

```
make regression-test
make regression-test-msan
make regression-test-uasan
```
