#!/bin/bash
#
# test_single_pragma.sh - Test pragma file(s)
#
# This script validates that a pragma file can be successfully parsed.
# Handles both single-pragma and multi-pragma files.
#
# Usage: ./test_single_pragma.sh <pragma_file> <ompp_binary>
#
# Exit codes:
#   0: All pragmas parsed successfully
#   1: One or more pragmas failed to parse
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

# Run ompp on the file (handles multi-pragma files automatically)
OUTPUT=$("$OMPP_BIN" --no-normalize "$PRAGMA_FILE" 2>&1)

# Count NULL outputs (indicates parse failures)
NULL_COUNT=$(echo "$OUTPUT" | grep -c "^NULL$" || true)

if [ "$NULL_COUNT" -gt 0 ]; then
    echo "Parse failed: $NULL_COUNT pragma(s) returned NULL"
    echo "$OUTPUT"
    exit 1
fi

# Check if stderr contains actual error messages
if echo "$OUTPUT" | grep -E "^error:" > /dev/null; then
    echo "Parse error detected"
    echo "$OUTPUT"
    exit 1
fi

# If we got here, all pragmas parsed successfully
exit 0
