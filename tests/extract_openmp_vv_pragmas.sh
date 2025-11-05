#!/bin/bash
#
# extract_openmp_vv_pragmas.sh - Extract OpenMP pragmas from OpenMP_VV suite
#
# This script extracts OpenMP pragmas from the OpenMP Validation & Verification
# test suite and saves them as individual test files for our parser.
#
# IMPORTANT: This script MUST be run with LLVM toolchain (default: LLVM 20):
#   CC=clang-20 CXX=clang++-20 FC=flang-20 ./extract_openmp_vv_pragmas.sh
#
# To use a different LLVM version:
#   LLVM_VERSION=21 CC=clang-21 CXX=clang++-21 FC=flang-21 ./extract_openmp_vv_pragmas.sh
#
# The script will:
# 1. Wipe the tests/openmp_vv/ directory completely
# 2. Clone/update the OpenMP_VV repository
# 3. Extract pragmas from all C/C++/Fortran files using LLVM preprocessors
# 4. Save each pragma to a separate file in tests/openmp_vv/
# 5. Update this script's header with the OpenMP_VV commit hash and date
# 6. Report the total number of pragmas extracted
#
# Extracted from OpenMP_VV repository:
# Commit: 49ad6cbd5281c461447c3c18272e4371496b8981
# Date:   2025-10-23 13:16:43
#
# Last extraction: 2025-11-05 21:25:59
#

set -euo pipefail

# Configuration
LLVM_VERSION="${LLVM_VERSION:-20}"  # Default to LLVM 20, can be overridden
REPO_URL="https://github.com/OpenMP-Validation-and-Verification/OpenMP_VV"
REPO_PATH="build/openmp_vv"
TESTS_DIR="tests"
OUTPUT_DIR="tests/openmp_vv"
REQUIRED_MIN_PRAGMAS=6687

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
NC='\033[0m' # No Color

# Check if LLVM compilers are available and being used
echo "========================================="
echo "  OpenMP_VV Pragma Extraction"
echo "========================================="
echo ""

# Verify LLVM toolchain
echo "Verifying LLVM ${LLVM_VERSION} toolchain..."
REQUIRED_CC="clang-${LLVM_VERSION}"
REQUIRED_CXX="clang++-${LLVM_VERSION}"
REQUIRED_FC="flang-${LLVM_VERSION}"
REQUIRED_CLANG_FORMAT="clang-format-${LLVM_VERSION}"

# Check if compilers are set correctly
if [ "${CC:-}" != "$REQUIRED_CC" ] && [ "${CC:-}" != "clang" ]; then
    echo -e "${RED}ERROR: CC must be set to ${REQUIRED_CC}${NC}"
    echo "Usage: CC=${REQUIRED_CC} CXX=${REQUIRED_CXX} FC=${REQUIRED_FC} $0"
    echo "Or:    LLVM_VERSION=21 CC=clang-21 CXX=clang++-21 FC=flang-21 $0"
    exit 1
fi

if [ "${CXX:-}" != "$REQUIRED_CXX" ] && [ "${CXX:-}" != "clang++" ]; then
    echo -e "${RED}ERROR: CXX must be set to ${REQUIRED_CXX}${NC}"
    echo "Usage: CC=${REQUIRED_CC} CXX=${REQUIRED_CXX} FC=${REQUIRED_FC} $0"
    exit 1
fi

if [ "${FC:-}" != "$REQUIRED_FC" ] && [ "${FC:-}" != "flang" ]; then
    echo -e "${RED}ERROR: FC must be set to ${REQUIRED_FC}${NC}"
    echo "Usage: CC=${REQUIRED_CC} CXX=${REQUIRED_CXX} FC=${REQUIRED_FC} $0"
    exit 1
fi

# Verify executables exist
for tool in "$REQUIRED_CC" "$REQUIRED_CXX" "$REQUIRED_FC" "$REQUIRED_CLANG_FORMAT"; do
    if ! command -v "$tool" >/dev/null 2>&1; then
        echo -e "${RED}ERROR: Required tool '$tool' not found${NC}"
        echo "Please install LLVM ${LLVM_VERSION} toolchain"
        exit 1
    fi
done

echo -e "${GREEN}✓${NC} LLVM ${LLVM_VERSION} toolchain verified"
echo "  C:   ${REQUIRED_CC}"
echo "  C++: ${REQUIRED_CXX}"
echo "  FC:  ${REQUIRED_FC}"
echo "  clang-format: ${REQUIRED_CLANG_FORMAT}"
echo ""

# Wipe output directory
echo "Wiping output directory: $OUTPUT_DIR"
rm -rf "$OUTPUT_DIR"
mkdir -p "$OUTPUT_DIR"
echo -e "${GREEN}✓${NC} Output directory ready"
echo ""

# Clone or update OpenMP_VV repository
if [ ! -d "$REPO_PATH" ]; then
    echo "Cloning OpenMP_VV repository..."
    mkdir -p "$(dirname "$REPO_PATH")"
    git clone --depth 1 "$REPO_URL" "$REPO_PATH" || {
        echo -e "${RED}Failed to clone OpenMP_VV${NC}"
        exit 1
    }
    echo -e "${GREEN}✓${NC} Cloned successfully"
else
    echo "Updating existing OpenMP_VV repository..."
    cd "$REPO_PATH"
    git fetch origin || {
        echo -e "${YELLOW}Warning: Failed to fetch updates${NC}"
    }
    git reset --hard origin/main || git reset --hard origin/master || {
        echo -e "${YELLOW}Warning: Failed to reset to latest${NC}"
    }
    cd - >/dev/null
    echo -e "${GREEN}✓${NC} Updated successfully"
fi
echo ""

# Get commit information
echo "Reading OpenMP_VV repository information..."
cd "$REPO_PATH"
COMMIT_HASH=$(git rev-parse HEAD)
COMMIT_DATE=$(git log -1 --format=%cd --date=format:'%Y-%m-%d %H:%M:%S')
cd - >/dev/null
echo -e "${GREEN}✓${NC} Commit: $COMMIT_HASH"
echo -e "${GREEN}✓${NC} Date:   $COMMIT_DATE"
echo ""

# Find all source files
echo "Finding source files in $REPO_PATH/$TESTS_DIR..."
mapfile -t source_files < <(find "$REPO_PATH/$TESTS_DIR" -type f \( \
    -name "*.c" -o -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" -o \
    -name "*.f" -o -name "*.for" -o -name "*.f90" -o -name "*.f95" -o -name "*.f03" -o \
    -name "*.F" -o -name "*.F90" -o -name "*.F95" -o -name "*.F03" \
    \) | sort)
echo "Found ${#source_files[@]} source files"
echo ""

# Extract pragmas
echo "Extracting pragmas..."
total_pragmas=0

for file in "${source_files[@]}"; do
    # Detect file type
    ext="${file##*.}"
    lower_ext="${ext,,}"
    is_fortran=0
    case "$lower_ext" in
        f|for|f90|f95|f03)
            is_fortran=1
            ;;
    esac

    # Set up include paths
    include_flags=()
    if [ -d "$REPO_PATH/ompvv" ]; then
        include_flags+=("-I" "$REPO_PATH/ompvv")
    fi
    include_flags+=("-I" "$REPO_PATH")
    include_flags+=("-I" "$(dirname "$file")")

    # Preprocess based on file type
    pragmas=()
    if [ $is_fortran -eq 1 ]; then
        # Fortran file
        preprocessed=$("$REQUIRED_FC" -E -P -fopenmp "${include_flags[@]}" "$file" 2>/dev/null || true)

        if [ -z "$preprocessed" ]; then
            continue
        fi

        # Extract Fortran directives and merge continuations
        raw_pragmas=()
        IFS=$'\n' read -r -d '' -a raw_pragmas < <(echo "$preprocessed" | grep -iE '^[[:space:]]*[!cC*]\$omp' || true; printf '\0')

        if [ ${#raw_pragmas[@]} -gt 0 ]; then
            current=""
            for line in "${raw_pragmas[@]}"; do
                trimmed=$(echo "$line" | sed 's/^[[:space:]]*//;s/[[:space:]]*$//')
                lower=$(echo "$trimmed" | tr '[:upper:]' '[:lower:]')
                prefix="${lower:0:6}"

                if [ "$prefix" = '!$omp&' ] || [ "$prefix" = 'c$omp&' ] || [ "$prefix" = '*$omp&' ]; then
                    remainder="${trimmed:6}"
                    remainder=$(echo "$remainder" | sed 's/^[[:space:]]*//')
                    current=$(echo "$current" | sed 's/[[:space:]]*&$//')
                    current="${current}${remainder}"
                else
                    if [ -n "$current" ]; then
                        pragmas+=("$current")
                    fi
                    current="$trimmed"
                fi

                if [[ "$current" == *"&" ]]; then
                    current=$(echo "$current" | sed 's/[[:space:]]*&$//')
                fi
            done

            if [ -n "$current" ]; then
                pragmas+=("$current")
            fi
        fi
    else
        # C/C++ file
        preprocessor="$REQUIRED_CC"
        case "$lower_ext" in
            cpp|cc|cxx)
                preprocessor="$REQUIRED_CXX"
                ;;
        esac

        preprocessed=$("$preprocessor" -E -P -CC -fopenmp "${include_flags[@]}" "$file" 2>/dev/null || true)

        if [ -z "$preprocessed" ]; then
            continue
        fi

        # Extract pragmas
        mapfile -t pragmas < <(echo "$preprocessed" | grep -E '^[[:space:]]*#pragma[[:space:]]+omp' || true)
    fi

    # Save all pragmas to a file matching upstream structure
    if [ ${#pragmas[@]} -gt 0 ]; then
        # Get relative path from upstream tests directory
        relative_path="${file#$REPO_PATH/$TESTS_DIR/}"

        # Create output path preserving directory structure
        output_file="$OUTPUT_DIR/$relative_path"
        output_dir=$(dirname "$output_file")

        # Create directory structure if it doesn't exist
        mkdir -p "$output_dir"

        # Write all pragmas to single file (one per line)
        for pragma in "${pragmas[@]}"; do
            echo "$pragma" >> "$output_file"
            total_pragmas=$((total_pragmas + 1))
        done
    fi
done

echo ""
echo "========================================="
echo "  Extraction Results"
echo "========================================="
echo ""
echo "Total pragmas extracted: $total_pragmas"
echo "Output directory:        $OUTPUT_DIR"
echo ""

# Check if we got enough pragmas
if [ $total_pragmas -lt $REQUIRED_MIN_PRAGMAS ]; then
    echo -e "${RED}ERROR: Only extracted $total_pragmas pragmas (expected at least $REQUIRED_MIN_PRAGMAS)${NC}"
    echo -e "${RED}This indicates the extraction did not work correctly.${NC}"
    echo -e "${RED}Make sure you are using LLVM 20 toolchain!${NC}"
    exit 1
fi

echo -e "${GREEN}✓${NC} Successfully extracted $total_pragmas pragmas (minimum $REQUIRED_MIN_PRAGMAS required)"
echo ""

# Update this script's header with commit information
echo "Updating script header with extraction metadata..."
SCRIPT_PATH="${BASH_SOURCE[0]}"
EXTRACTION_DATE=$(date '+%Y-%m-%d %H:%M:%S')

# Use sed to update the header comments
sed -i "s|^# Commit: .*|# Commit: $COMMIT_HASH|" "$SCRIPT_PATH"
sed -i "s|^# Date:   .*|# Date:   $COMMIT_DATE|" "$SCRIPT_PATH"
sed -i "s|^# Last extraction: .*|# Last extraction: $EXTRACTION_DATE|" "$SCRIPT_PATH"

echo -e "${GREEN}✓${NC} Script header updated"
echo ""

echo "========================================="
echo -e "${GREEN}✓ Extraction complete!${NC}"
echo "========================================="
echo ""
echo "Next steps:"
echo "  1. Verify tests pass: cd build && ctest ."
echo "  2. Commit the extracted pragmas: git add $OUTPUT_DIR && git commit"
echo ""
