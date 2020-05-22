#!/usr/bin/env python3

# ################################################################
# Copyright (c) 2016-2020, Facebook, Inc.
# All rights reserved.
#
# This source code is licensed under both the BSD-style license (found in the
# LICENSE file in the root directory of this source tree) and the GPLv2 (found
# in the COPYING file in the root directory of this source tree).
# You may select, at your option, one of the above-listed licenses.
# ################################################################

import datetime
import enum
import glob
import os
import sys

YEAR = datetime.datetime.now().year

YEAR_STR = str(YEAR)

ROOT = os.path.join(os.path.dirname(__file__), "..")

RELDIRS = [
    "doc",
    "examples",
    "lib",
    "programs",
    "tests",
]

DIRS = [os.path.join(ROOT, d) for d in RELDIRS]

class File(enum.Enum):
    C = 1
    H = 2
    MAKE = 3
    PY = 4

SUFFIX = {
    File.C: ".c",
    File.H: ".h",
    File.MAKE: "Makefile",
    File.PY: ".py",
}

# License should certainly be in the first 10 KB.
MAX_BYTES = 10000
MAX_LINES = 50

LICENSE_LINES = [
    "This source code is licensed under both the BSD-style license (found in the",
    "LICENSE file in the root directory of this source tree) and the GPLv2 (found",
    "in the COPYING file in the root directory of this source tree).",
    "You may select, at your option, one of the above-listed licenses.",
]

COPYRIGHT_EXCEPTIONS = {
    # From zstdmt
    "threading.c",
    "threading.h",
    # From divsufsort
    "divsufsort.c",
    "divsufsort.h",
}

LICENSE_EXCEPTIONS = {
    # From divsufsort
    "divsufsort.c",
    "divsufsort.h",
}


def valid_copyright(lines):
    for line in lines:
        line = line.strip()
        if "Copyright" not in line:
            continue
        if "present" in line:
            return (False, f"Copyright line '{line}' contains 'present'!")
        if "Facebook, Inc" not in line:
            return (False, f"Copyright line '{line}' does not contain 'Facebook, Inc'")
        if YEAR_STR not in line:
            return (False, f"Copyright line '{line}' does not contain {YEAR}")
        if " (c) " not in line:
            return (False, f"Copyright line '{line}' does not contain ' (c) '!")
        return (True, "")
    return (False, "Copyright not found!")


def valid_license(lines):
    for b in range(len(lines)):
        if LICENSE_LINES[0] not in lines[b]:
            continue
        for l in range(len(LICENSE_LINES)):
            if LICENSE_LINES[l] not in lines[b + l]:
                message = f"""Invalid license line found starting on line {b + l}!
Expected: '{LICENSE_LINES[l]}'
Actual: '{lines[b + l]}'"""
                return (False, message)
        return (True, "")
    return (False, "License not found!")


def valid_file(filename):
    with open(filename, "r") as f:
        lines = f.readlines(MAX_BYTES)
    lines = lines[:min(len(lines), MAX_LINES)]
                
    ok = True
    if os.path.basename(filename) not in COPYRIGHT_EXCEPTIONS:
        c_ok, c_msg = valid_copyright(lines)
        if not c_ok:
            print(f"{filename}: {c_msg}")
            ok = False
    if os.path.basename(filename) not in LICENSE_EXCEPTIONS:
        l_ok, l_msg = valid_license(lines)
        if not l_ok:
            print(f"{filename}: {l_msg}")
            ok = False
    return ok


def main():
    invalid_files = []
    for directory in DIRS:
        for suffix in SUFFIX.values():
            files = set(glob.glob(f"{directory}/*{suffix}"))
            files |= set(glob.glob(f"{directory}/**/*{suffix}"))
            for filename in files:
                if not valid_file(filename):
                    invalid_files.append(filename)
    if len(invalid_files) > 0:
        print(f"Invalid files: {invalid_files}")
    else:
        print("Pass!")
    return len(invalid_files)

if __name__ == "__main__":
    sys.exit(main())