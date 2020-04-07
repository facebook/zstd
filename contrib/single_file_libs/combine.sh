#!/bin/sh -e

# Tool to bundle multiple C/C++ source files, inlining any includes.
# 
# Note: this POSIX-compliant script is many times slower than the original bash
# implementation (due to the grep calls) but it runs and works everywhere.
# 
# TODO: ROOTS, FOUND, etc., as arrays (since they fail on paths with spaces)
# TODO: revert to Bash-only regex (the grep ones being too slow)
# 
# Author: Carl Woffenden, Numfum GmbH (this script is released under a CC0 license/Public Domain)

# Common file roots
ROOTS="."

# -x option excluded includes
XINCS=""

# -k option includes to keep as include directives
KINCS=""

# Files previously visited
FOUND=""

# Optional destination file (empty string to write to stdout)
DESTN=""

# Whether the "#pragma once" directives should be written to the output
PONCE=0

# Prints the script usage then exits
usage() {
  echo "Usage: $0 [-r <path>] [-x <header>] [-k <header>] [-o <outfile>] infile"
  echo "  -r file root search path"
  echo "  -x file to completely exclude from inlining"
  echo "  -k file to exclude from inlining but keep the include directive"
  echo "  -p keep any '#pragma once' directives (removed by default)"
  echo "  -o output file (otherwise stdout)"
  echo "Example: $0 -r ../my/path - r ../other/path -o out.c in.c"
  exit 1
}

# Tests that the grep implementation works as expected (older OSX grep fails)
test_grep() {
  if ! echo '#include "foo"' | grep -Eq '^\s*#\s*include\s*".+"'; then
    echo "Aborting: the grep implementation fails to parse include lines"
    exit 1
  fi
}

# Tests if list $1 has item $2 (returning zero on a match)
list_has_item() {
  if echo "$1" | grep -Eq "(^|\s*)$2(\$|\s*)"; then
    return 0
  else
    return 1
  fi
}

# Adds a new line with the supplied arguments to $DESTN (or stdout)
write_line() {
  if [ -n "$DESTN" ]; then
    printf '%s\n' "$@" >> "$DESTN"
  else
    printf '%s\n' "$@"
  fi
}

# Adds the contents of $1 with any of its includes inlined
add_file() {
  # Match the path
  local file=
  for root in $ROOTS; do
    if [ -f "$root/$1" ]; then
      file="$root/$1"
    fi
  done
  if [ -n "$file" ]; then
    if [ -n "$DESTN" ]; then
      # Log but only if not writing to stdout
      echo "Processing: $file"
    fi
    # Read the file
    local line=
    while IFS= read -r line; do
      if echo "$line" | grep -Eq '^\s*#\s*include\s*".+"'; then
        # We have an include directive so strip the (first) file
        local inc=$(echo "$line" | grep -Eo '".*"' | grep -Eo '\w*(\.?\w+)+' | head -1)
        if list_has_item "$XINCS" "$inc"; then
          # The file was excluded so error if the source attempts to use it
          write_line "#error Using excluded file: $inc"
        else
          if ! list_has_item "$FOUND" "$inc"; then
            # The file was not previously encountered
            FOUND="$FOUND $inc"
            if list_has_item "$KINCS" "$inc"; then
              # But the include was flagged to keep as included
              write_line "/**** *NOT* inlining $inc ****/"
              write_line "$line"
            else
              # The file was neither excluded nor seen before so inline it
              write_line "/**** start inlining $inc ****/"
              add_file "$inc"
              write_line "/**** ended inlining $inc ****/"
            fi
          else
            write_line "/**** skipping file: $inc ****/"
          fi
        fi
      else
        # Skip any 'pragma once' directives, otherwise write the source line
        local write=$PONCE
        if [ $write -eq 0 ]; then
          if echo "$line" | grep -Eqv '^\s*#\s*pragma\s*once\s*'; then
            write=1
          fi
        fi
        if [ $write -ne 0 ]; then
          write_line "$line"
        fi
      fi
    done < "$file"
  else
    write_line "#error Unable to find \"$1\""
  fi
}

while getopts ":r:x:k:po:" opts; do
  case $opts in
  r)
    ROOTS="$ROOTS $OPTARG"
    ;;
  x)
    XINCS="$XINCS $OPTARG"
    ;;
  k)
    KINCS="$KINCS $OPTARG"
    ;;
  p)
    PONCE=1
    ;;
  o)
    DESTN="$OPTARG"
    ;;
  *)
    usage
    ;;
  esac
done
shift $((OPTIND-1))

if [ -n "$1" ]; then
  if [ -f "$1" ]; then
    if [ -n "$DESTN" ]; then
      printf "" > "$DESTN"
    fi
    test_grep
    add_file $1
  else
    echo "Input file not found: \"$1\""
    exit 1
  fi
else
  usage
fi
exit 0
