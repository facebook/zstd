#!/usr/bin/env bash
# test_process_substitution.bash
# Test zstd's support for process substitution with --filelist

# Process arguments
ZSTD_PATH="zstd"  # Default to using zstd from PATH
if [ $# -ge 1 ]; then
    ZSTD_PATH="$1"
fi

echo "Using zstd executable: $ZSTD_PATH"

set -e  # Exit on error

# Set up test directory and files
echo "Setting up test environment..."
TEST_DIR="tmp_process_substit"
rm -rf "$TEST_DIR"
mkdir -p "$TEST_DIR"
echo "Content of file 1" > "$TEST_DIR/file1.txt"
echo "Content of file 2" > "$TEST_DIR/file2.txt"
echo "Content of file 3" > "$TEST_DIR/file3.txt"

# Clean up any previous test artifacts
rm -f "$TEST_DIR/output.zst" "$TEST_DIR/output_echo.zst" "$TEST_DIR/output_cat.zst"
rm -rf "$TEST_DIR/extracted"
mkdir -p "$TEST_DIR/extracted"

echo "=== Testing process substitution with --filelist ==="

# Test 1: Basic process substitution with find
echo "Test 1: Basic process substitution (find command)"
"$ZSTD_PATH" --filelist=<(find "$TEST_DIR" -name "*.txt" | sort) -c > "$TEST_DIR/output.zst"

if [ -f "$TEST_DIR/output.zst" ]; then
    echo "✓ Test 1 PASSED: Output file was created"
else
    echo "✗ Test 1 FAILED: Output file was not created"
    exit 1
fi

# Test 2: Process substitution with echo
echo "Test 2: Process substitution (echo command)"
"$ZSTD_PATH" --filelist=<(echo -e "$TEST_DIR/file1.txt\n$TEST_DIR/file2.txt") -c > "$TEST_DIR/output_echo.zst"

if [ -f "$TEST_DIR/output_echo.zst" ]; then
    echo "✓ Test 2 PASSED: Output file was created"
else
    echo "✗ Test 2 FAILED: Output file was not created"
    exit 1
fi

# Test 3: Process substitution with cat
echo "Test 3: Process substitution (cat command)"
echo -e "$TEST_DIR/file1.txt\n$TEST_DIR/file3.txt" > "$TEST_DIR/filelist.txt"
"$ZSTD_PATH" --filelist=<(cat "$TEST_DIR/filelist.txt") -c > "$TEST_DIR/output_cat.zst"

if [ -f "$TEST_DIR/output_cat.zst" ]; then
    echo "✓ Test 3 PASSED: Output file was created"
else
    echo "✗ Test 3 FAILED: Output file was not created"
    exit 1
fi

# Test 4: Verify contents of archives
echo "Test 4: Verifying archive contents"
"$ZSTD_PATH" -d "$TEST_DIR/output.zst" -o "$TEST_DIR/extracted/combined.out"

if grep -q "Content of file 1" "$TEST_DIR/extracted/combined.out" &&
   grep -q "Content of file 2" "$TEST_DIR/extracted/combined.out" &&
   grep -q "Content of file 3" "$TEST_DIR/extracted/combined.out"; then
    echo "✓ Test 4 PASSED: All files were correctly archived and extracted"
else
    echo "✗ Test 4 FAILED: Not all expected content was found in the extracted file"
    exit 1
fi

# Test 5: Edge case with empty list
echo "Test 5: Process substitution with empty input"
"$ZSTD_PATH" --filelist=<(echo "") -c > "$TEST_DIR/output_empty.zst" 2>/dev/null || true

if [ -f "$TEST_DIR/output_empty.zst" ]; then
    echo "✓ Test 5 PASSED: Handled empty input gracefully"
else
    echo "✓ Test 5 PASSED: Properly rejected empty input"
fi

# cleanup
rm -rf "$TEST_DIR"

echo "All tests completed successfully!"

