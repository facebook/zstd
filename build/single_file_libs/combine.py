#!/usr/bin/env python3

# Tool to bundle multiple C/C++ source files, inlining any includes.
# 
# Author: Carl Woffenden, Numfum GmbH (this script is released under a CC0 license/Public Domain)

import argparse, os, re, sys

from pathlib import Path

# File roots when searching (equivalent to -I paths for the compiler).
roots = set()

# File Path objects previously inlined.
found = set()

# Destination file object (or stdout if no output file was supplied).
destn = None

# Regex to handle the following type of file includes:
# 
#	#include "file"
#	  #include "file"
#	#  include "file"
#	#include   "file"
#	#include "file" // comment
#	#include "file" // comment with quote "
# 
# And all combinations of, as well as ignoring the following:
# 
#	#include <file>
#	//#include "file"
#	/*#include "file"*/
# 
# We don't try to catch errors since the compiler will do this (and the code is
# expected to be valid before processing) and we don't care what follows the
# file (whether it's a valid comment or not, since anything after the quoted
# string is ignored)
# 
include_regex = re.compile(r'^\s*#\s*include\s*"(.+?)"')

# Simple tests to prove include_regex's cases.
# 
def test_match_include():
	if (include_regex.match('#include "file"')   and
		include_regex.match('  #include "file"') and
		include_regex.match('#  include "file"') and
		include_regex.match('#include   "file"') and
		include_regex.match('#include "file" // comment')):
			if (not include_regex.match('#include <file>')   and
				not include_regex.match('//#include "file"') and
				not include_regex.match('/*#include "file"*/')):
					found = include_regex.match('#include "file" // "')
					if (found and found.group(1) == 'file'):
						print('#include match valid')
						return True
	return False

# Regex to handle "#pragma once" in various formats:
# 
#	#pragma once
#	  #pragma once
#	#  pragma once
#	#pragma   once
#	#pragma once // comment
# 
# Ignoring commented versions, same as include_regex.
# 
pragma_regex = re.compile(r'^\s*#\s*pragma\s*once\s*')

# Simple tests to prove pragma_regex's cases.
# 
def text_match_pragma():
	if (pragma_regex.match('#pragma once')   and
		pragma_regex.match('  #pragma once') and
		pragma_regex.match('#  pragma once') and
		pragma_regex.match('#pragma   once') and
		pragma_regex.match('#pragma once // comment')):
			if (not pragma_regex.match('//#pragma once') and
				not pragma_regex.match('/*#pragma once*/')):
					print('#pragma once match valid')
					return True
	return False

# Finds 'file'. First the currently processing file's 'parent' path is looked at
# for a match, followed by the list of 'root', returning a valid Path in
# canonical form. If no match is found None is returned.
# 
def resolve_include(parent: Path, file: str):
	found = parent.joinpath(file).resolve();
	if (found.is_file()):
		return found
	for root in roots:
		found = root.joinpath(file).resolve()
		if (found.is_file()):
			return found
	return None

# Writes 'line' to the open file 'destn' (or stdout).
# 
def write_line(line):
	print(line, file=destn)

# Logs 'line' to stderr.
# 
def log_line(line):
	print(line, file=sys.stderr)

def add_file(file):
	if (isinstance(file, Path) and file.is_file()):
		log_line(f'Processing: {file}')
		with file.open('r') as opened:
			for line in opened:
				line = line.rstrip('\n')
				match_include = include_regex.match(line);
				if (match_include):
					inc_name = match_include.group(1)
					resolved = resolve_include(file.parent, inc_name)
					if (resolved not in found):
						# The file was not previously encountered
						found.add(resolved)
						write_line(f'/**** start inlining {inc_name} ****/')
						add_file(resolved)
						write_line(f'/**** ended inlining {inc_name} ****/')
					else:
						write_line(f'/**** skipping file: {inc_name} ****/')
				else:
					if (not pragma_regex.match(line)):
						write_line(line)
	else:
		log_line(f'Error: Unable to find: {file}')
	

parser = argparse.ArgumentParser(description='Amalgamate Tool', epilog=f'example: {sys.argv[0]} -r ../my/path -r ../other/path -o out.c in.c')
parser.add_argument('-r', '--root', action='append', type=Path, help='file root search path')
parser.add_argument('-x', '--exclude',  action='append', help='file to completely exclude from inlining')
parser.add_argument('-k', '--keep', action='append', help='file to exclude from inlining but keep the include directive')
parser.add_argument('-p', '--pragma', action='store_true', default=False, help='keep any "#pragma once" directives (removed by default)')
parser.add_argument('-o', '--output', type=argparse.FileType('w'), help='output file (otherwise stdout)')
parser.add_argument('input', type=Path, help='input file')
args = parser.parse_args()

# Resolve all of the root paths upfront (we'll halt here on invalid roots)
if (args.root is not None):
	for path in args.root:
		roots.add(path.resolve(strict=True))

try:
	if (args.output is None):
		destn = sys.stdout
	else:
		destn = args.output
	add_file(args.input)
finally:
	if (destn is not None):
		destn.close()
