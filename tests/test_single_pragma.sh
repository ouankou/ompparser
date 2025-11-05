#!/bin/bash
#
# test_single_pragma.sh - Test a single extracted pragma file
#
# This script validates that a pragma file can be successfully parsed.
# It reads the pragma from the file and passes it through the parser.
#
# Usage: ./test_single_pragma.sh <pragma_file> <ompp_binary>
#
# Exit codes:
#   0: Parse succeeded
#   1: Parse failed or NULL output
#

set -euo pipefail

if [ $# -lt 2 ]; then
    echo "Usage: $0 <pragma_file> <ompp_binary>"
    exit 1
fi

PRAGMA_FILE="$1"
OMPP_BIN="$2"

if [ ! -f "$PRAGMA_FILE" ]; then
    echo "Error: Pragma file not found: $PRAGMA_FILE"
    exit 1
fi

if [ ! -x "$OMPP_BIN" ]; then
    echo "Error: ompp binary not found or not executable: $OMPP_BIN"
    exit 1
fi

# Run ompp and capture output
OUTPUT=$("$OMPP_BIN" --no-normalize "$PRAGMA_FILE" 2>&1)

# Check if output contains "NULL" which indicates parse failure
if echo "$OUTPUT" | grep -q "^NULL$"; then
    echo "Parse failed: NULL output"
    echo "$OUTPUT"
    exit 1
fi

# Check if stderr contains actual error messages (not just the word "error" in pragmas)
if echo "$OUTPUT" | grep -E "^error:" > /dev/null; then
    echo "Parse error detected"
    echo "$OUTPUT"
    exit 1
fi

# If we got here, parsing succeeded
exit 0
