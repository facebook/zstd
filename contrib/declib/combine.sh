#!/bin/bash

# Tool to bundle multiple C/C++ source files, inlining any includes.

# Common file roots
ROOTS="./"

# Files previously visited
FOUND=""

# Prints the script usage then exits
function usage {
  echo "Usage: $0 [-r <paths>] infile"
  echo "  -r file root search paths"
  echo "Example: $0 -r \"../my/path ../my/other\" in.c > out.c"
  exit 1
}

# Tests if list $1 has item $2
function list_has_item {
  local list="$1"
  local item="$2"
  if [[ $list =~ (^|[[:space:]]*)"$item"($|[[:space:]]*) ]]; then
    return 0
  fi
  return 1
}

# Adds the contents of $1 with any of its includes inlined
function add_file {
  # Match the path
  local file=
  for root in $ROOTS; do
    if test -f "$root/$1"; then
      file="$root/$1"
    fi
  done
  if [ "$file" != "" ]; then
    # Read the file
    local line
    while IFS= read -r line; do
      if [[ $line =~ ^[[:space:]]*\#[[:space:]]*include[[:space:]]*\"(.*)\".* ]]; then
        # We have an include directive
        local inc=${BASH_REMATCH[1]}
        if ! `list_has_item "$FOUND" "$inc"`; then
          # And we've not previously encountered it
          FOUND="$FOUND $inc"
          echo "/**** start inlining $inc ****/"
          add_file "$inc"
          echo "/**** ended inlining $inc ****/"
        else
          echo "/**** skipping file: $inc ****/"
        fi
      else
        # Otherwise write the source line
        echo "$line"
      fi
    done < "$file"
  else
    echo "#error Unable to find \"$1\""
  fi
}

while getopts ":r:" opts; do
  case $opts in
  r)
    ROOTS="$ROOTS $OPTARG"
    ;;
  *)
    usage
    ;;
  esac
done
shift $((OPTIND-1))

if [ "$1" != "" ]; then
  add_file $1
else
  usage
fi
