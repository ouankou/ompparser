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

# Tools (can be overridden via environment)
CLANG_FORMAT=${CLANG_FORMAT:-clang-format}

# Normalize Fortran pragma to match clang-format style spacing
normalize_fortran_pragma() {
    local pragma="$1"
    echo "$pragma" | \
        sed 's/,\([^[:space:]]\)/, \1/g' | \
        sed 's/[[:space:]]*(/(/g' | \
        sed 's/( /(/g' | \
        sed 's/\([[:alnum:]_]\)[[:space:]]\+(/\1(/g' | \
        sed 's/[[:space:]]*,[[:space:]]*/,/g' | \
        sed 's/[[:space:]][[:space:]]*/ /g' | \
        sed 's/[[:space:]]*>=/>=/g' | \
        sed 's/[[:space:]]*<=/<=/g' | \
        sed 's/[[:space:]]*==/==/g' | \
        sed 's/[[:space:]]*=>/=>/g' | \
        sed 's/[[:space:]]*>[[:space:]]*/>/g' | \
        sed 's/[[:space:]]*<[[:space:]]*/</g' | \
        sed 's/[[:space:]]*=[[:space:]]*/=/g' | \
        sed 's/[[:space:]]*-[[:space:]]*/-/g' | \
        sed 's/[[:space:]]*+[[:space:]]*/+/g' | \
        sed 's/[[:space:]]*\*[[:space:]]*/\*/g' | \
        sed 's/[[:space:]]*\/[[:space:]]*/\//g' | \
        sed 's/)[[:space:]]\+/)/g' | \
        sed 's/ )/)/g' | \
        sed 's/[[:space:]]*)/)/g' | \
        sed 's/[[:space:]]*\.\([[:alnum:]_]\{1,\}\)\.[[:space:]]*/.\1./g' | \
        sed 's/if(/if (/' | \
        sed 's/^[[:space:]]*//;s/[[:space:]]*$//'
}

# Format C/C++ pragmas using clang-format when available
format_c_pragmas() {
    local text="$1"
    if command -v "$CLANG_FORMAT" >/dev/null 2>&1; then
        echo "$text" | "$CLANG_FORMAT" -assume-filename=prim.c 2>/dev/null || echo "$text"
    else
        echo "$text"
    fi
}

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

# Determine language from extension
is_fortran=0
case "${PRAGMA_FILE##*.}" in
    f|for|f90|f95|f03|F|F90|F95|F03)
        is_fortran=1
        ;;
esac

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

# Extract original pragmas from the input file
if [ $is_fortran -eq 1 ]; then
    mapfile -t original_pragmas < <(grep -iE '^[[:space:]]*[!cC*]\$omp' "$PRAGMA_FILE" || true)
else
    mapfile -t original_pragmas < <(grep -E '^[[:space:]]*#pragma[[:space:]]+omp' "$PRAGMA_FILE" || true)
fi

# Extract round-tripped pragmas from ompp output
mapfile -t roundtrip_pragmas < <(echo "$OUTPUT" | grep -E '^#pragma omp|^[!cC*]\$omp' || true)

# If no pragmas were found, nothing to compare
if [ ${#original_pragmas[@]} -eq 0 ] && [ ${#roundtrip_pragmas[@]} -eq 0 ]; then
    exit 0
fi

# Mismatched pragma counts indicate an error
if [ ${#original_pragmas[@]} -ne ${#roundtrip_pragmas[@]} ]; then
    echo "Mismatch: expected ${#original_pragmas[@]} pragmas, got ${#roundtrip_pragmas[@]}"
    echo "$OUTPUT"
    exit 1
fi

# Normalize pragmas for comparison
if [ $is_fortran -eq 1 ]; then
    orig_norm=$(printf "%s\n" "${original_pragmas[@]}" | while IFS= read -r line; do
        normalize_fortran_pragma "$line" | tr '[:upper:]' '[:lower:]' | sed 's/[[:space:]]*:[[:space:]]*/:/g'
    done)
    roundtrip_norm=$(printf "%s\n" "${roundtrip_pragmas[@]}" | while IFS= read -r line; do
        normalize_fortran_pragma "$line" | tr '[:upper:]' '[:lower:]' | sed 's/[[:space:]]*:[[:space:]]*/:/g'
    done)
else
    # Strip comments before formatting
    orig_clean=$(printf "%s\n" "${original_pragmas[@]}" | sed 's|/\*[^*]*\*\+\([^/*][^*]*\*\+\)*/||g' | sed 's|//.*$||')
    roundtrip_clean=$(printf "%s\n" "${roundtrip_pragmas[@]}" | sed 's|/\*[^*]*\*\+\([^/*][^*]*\*\+\)*/||g' | sed 's|//.*$||')
    orig_norm=$(format_c_pragmas "$orig_clean")
    roundtrip_norm=$(format_c_pragmas "$roundtrip_clean")
fi

# Compare normalized results
if [ "$orig_norm" != "$roundtrip_norm" ]; then
    echo "Mismatch detected for $PRAGMA_FILE"
    echo "Expected:"
    echo "$orig_norm"
    echo "Got:"
    echo "$roundtrip_norm"
    exit 1
fi

# If we got here, all pragmas parsed and round-tripped successfully
exit 0
