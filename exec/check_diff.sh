#!/bin/sh

# Simple program for comparision.
# This program just runs diff for comparision between
# program and expected output.

TESTIN = "$1"
PROGRAM = "$2"
TESTOUT = "$3"

diff --strip-trailing-cr -Z -b -B "$PROGRAM" "$TESTOUT"
EXITCODE = $?
[[ $EXITCODE -gt 1 ]] && exit -1 # diff error
[[ $EXITCODE -ne 0 ]] && exit 1 # Wrong answer

diff --strip-trailing-cr "$PROGRAM" "$TESTOUT"
EXITCODE = $?
[[ $EXITCODE -gt 1 ]] && exit -1 # diff error
[[ $EXITCODE -ne 0 ]] && exit 2 # Presentation error

exit 0 # Accepted