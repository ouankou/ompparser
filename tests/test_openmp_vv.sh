#!/bin/bash
#
# test_openmp_vv.sh - Round-trip OpenMP pragmas from OpenMP_VV through ROUP
#
# This script validates ROUP by:
# 1. Cloning the OpenMP Validation & Verification test suite (on-demand)
# 2. Preprocessing all C/C++/Fortran test files with appropriate compilers
# 3. Extracting OpenMP pragmas/directives
# 4. Round-tripping each pragma through ROUP's parser
# 5. Comparing normalized input vs output
# 6. Reporting pass/fail statistics
#
# Usage:
#   ./test_openmp_vv.sh                        # Auto-clone to target/openmp_vv
#   OPENMP_VV_PATH=/path ./test_openmp_vv.sh   # Use existing clone
#   CC=gcc CXX=g++ ./test_openmp_vv.sh         # Use specific C/C++ compilers
#   CLANG=clang-15 ./test_openmp_vv.sh         # Use specific clang version
#   FC=gfortran ./test_openmp_vv.sh            # Use specific Fortran compiler
#   PARALLEL_JOBS=8 ./test_openmp_vv.sh        # Control parallel execution
#

set -euo pipefail

# Configuration
REPO_URL="https://github.com/OpenMP-Validation-and-Verification/OpenMP_VV"
REPO_PATH="${OPENMP_VV_PATH:-build/openmp_vv}"
TESTS_DIR="tests"
CLANG_DEFAULT="${CLANG:-clang}"
CLANG_FORMAT_ENV="${CLANG_FORMAT:-}"
FC="${FC:-}"  # Preserve explicit FC selection if provided
MAX_DISPLAY_FAILURES=100
# Fallback for systems without nproc (e.g., macOS)
if command -v nproc >/dev/null 2>&1; then
    DEFAULT_JOBS=$(nproc)
elif command -v getconf >/dev/null 2>&1; then
    DEFAULT_JOBS=$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo "1")
else
    DEFAULT_JOBS=1
fi
PARALLEL_JOBS="${PARALLEL_JOBS:-$DEFAULT_JOBS}"

read_cmake_cache() {
    local key="$1"
    local cache_file value
    for cache_file in "build/CMakeCache.txt" "CMakeCache.txt"; do
        if [ -f "$cache_file" ]; then
            value=$(awk -F= -v search="$key" '
                $1 == search || $1 ~ "^" search ":" { print $2; exit }
            ' "$cache_file")
            if [ -n "$value" ]; then
                echo "$value"
                return 0
            fi
        fi
    done
    return 1
}

resolve_compiler() {
    local env_var="$1"
    local cache_key="$2"
    local fallback="$3"

    local env_value="${!env_var:-}"
    if [ -n "$env_value" ]; then
        echo "$env_value"
        return 0
    fi

    if cache_value=$(read_cmake_cache "$cache_key"); then
        if [ -n "$cache_value" ]; then
            echo "$cache_value"
            return 0
        fi
    fi

    if [ -n "$fallback" ]; then
        echo "$fallback"
        return 0
    fi

    echo ""
    return 1
}

derive_cxx_from_c() {
    local c_compiler="$1"
    local base="${c_compiler##*/}"
    local dir="${c_compiler%/*}"
    if [ "$dir" = "$c_compiler" ]; then
        dir=""
    fi

    local candidate=""
    if [[ "$base" =~ ^clang([+-]?[0-9.]*)$ ]]; then
        candidate="clang++${BASH_REMATCH[1]}"
    elif [[ "$base" =~ ^gcc([+-]?[0-9.]*)$ ]]; then
        candidate="g++${BASH_REMATCH[1]}"
    elif [ "$base" = "cc" ]; then
        candidate="c++"
    fi

    if [ -n "$candidate" ]; then
        if [ -n "$dir" ] && [ "$dir" != "$c_compiler" ]; then
            echo "${dir%/}/$candidate"
        else
            echo "$candidate"
        fi
    fi
}

check_executable() {
    local cmd="$1"
    if [ -z "$cmd" ]; then
        return 1
    fi

    if [[ "$cmd" == */* ]]; then
        [ -x "$cmd" ]
    else
        command -v "$cmd" >/dev/null 2>&1
    fi
}

supports_openmp_declare_variant() {
    local compiler="$1"
    local lang="$2"
    local ext="c"
    if [ "$lang" = "c++" ]; then
        ext="cc"
    fi

    local tmp
    tmp=$(mktemp "/tmp/omp_decl_XXXXXX")
    local src="${tmp}.${ext}"
    local out="${tmp}.out"

    rm -f "$tmp"

    cat <<'EOF' > "$src"
#pragma omp begin declare variant match(device={kind(host)})
void omp_decl_variant(void) {}
#pragma omp end declare variant
EOF

    if ! "$compiler" -E -P -CC -fopenmp "$src" > "$out" 2>/dev/null; then
        rm -f "$src" "$out"
        return 1
    fi

    if grep -q '^#pragma[[:space:]]\+omp[[:space:]]\+begin[[:space:]]\+declare[[:space:]]\+variant' "$out"; then
        rm -f "$src" "$out"
        return 0
    fi

    rm -f "$src" "$out"
    return 1
}

resolve_tool_dir() {
    local tool="$1"
    if [ -z "$tool" ]; then
        return
    fi

    local resolved="$tool"
    if [[ "$tool" != */* ]]; then
        resolved=$(command -v "$tool" 2>/dev/null || echo "")
        if [ -z "$resolved" ]; then
            return
        fi
    fi

    local dir="${resolved%/*}"
    if [ -z "$dir" ] || [ "$dir" = "$resolved" ]; then
        return
    fi

    echo "$dir"
}

detect_fortran_compiler() {
    local candidates=()

    if compiler=$(resolve_compiler "FC" "CMAKE_Fortran_COMPILER" ""); then
        if [ -n "$compiler" ]; then
            candidates+=("$compiler")
        fi
    fi

    local suffixes=("")
    local tool base suffix
    for tool in "$C_COMPILER" "$CXX_COMPILER"; do
        base="${tool##*/}"
        if [[ "$base" =~ (-[0-9][0-9.]*)$ ]]; then
            suffix="${BASH_REMATCH[1]}"
            if [ -n "$suffix" ]; then
                local seen=0
                local existing
                for existing in "${suffixes[@]}"; do
                    if [ "$existing" = "$suffix" ]; then
                        seen=1
                        break
                    fi
                done
                if [ $seen -eq 0 ]; then
                    suffixes+=("$suffix")
                fi
            fi
        fi
    done

    candidates+=("flang-new" "flang" "gfortran")

    for suffix in "${suffixes[@]}"; do
        if [ -n "$suffix" ]; then
            candidates+=("flang${suffix}" "flang-new${suffix}" "gfortran${suffix}")
        fi
    done

    local candidate
    for candidate in "${candidates[@]}"; do
        if check_executable "$candidate"; then
            echo "$candidate"
            return 0
        fi
    done

    return 1
}

resolve_clang_format() {
    local requested="$1"

    if [ -n "$requested" ]; then
        echo "$requested"
        return 0
    fi

    if command -v clang-format >/dev/null 2>&1; then
        echo "clang-format"
        return 0
    fi

    local search_dirs=()
    local dir
    for tool in "$C_COMPILER" "$CXX_COMPILER"; do
        dir=$(resolve_tool_dir "$tool")
        if [ -n "$dir" ]; then
            search_dirs+=("$dir")
        fi
    done

    if command -v llvm-config >/dev/null 2>&1; then
        dir=$(llvm-config --bindir 2>/dev/null || true)
        if [ -n "$dir" ]; then
            search_dirs+=("$dir")
        fi
    fi

    local suffixes=("")
    local base suffix
    for tool in "$C_COMPILER" "$CXX_COMPILER"; do
        base="${tool##*/}"
        if [[ "$base" =~ ^clang(\+\+)?(.*)$ ]]; then
            suffix="${BASH_REMATCH[2]}"
            if [ -n "$suffix" ]; then
                suffixes+=("$suffix")
            fi
        fi
    done

    local candidate path
    for dir in "${search_dirs[@]}"; do
        if [ ! -d "$dir" ]; then
            continue
        fi
        for suffix in "${suffixes[@]}"; do
            if [ -n "$suffix" ]; then
                candidate="clang-format$suffix"
            else
                candidate="clang-format"
            fi
            path="$dir/$candidate"
            if [ -x "$path" ]; then
                echo "$path"
                return 0
            fi
        done

        path=$(find "$dir" -maxdepth 1 -type f -name 'clang-format-*' -print -quit 2>/dev/null || true)
        if [ -n "$path" ]; then
            echo "$path"
            return 0
        fi
    done

    echo "clang-format"
    return 0
}

# Colors for output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
NC='\033[0m' # No Color

C_COMPILER=$(resolve_compiler "CC" "CMAKE_C_COMPILER" "$CLANG_DEFAULT")
if [ -z "$C_COMPILER" ]; then
    C_COMPILER="cc"
fi

CXX_COMPILER=$(resolve_compiler "CXX" "CMAKE_CXX_COMPILER" "")
if [ -z "$CXX_COMPILER" ]; then
    CXX_COMPILER=$(derive_cxx_from_c "$C_COMPILER")
fi
if [ -z "$CXX_COMPILER" ]; then
    CXX_COMPILER="c++"
fi

CLANG_FORMAT=$(resolve_clang_format "$CLANG_FORMAT_ENV")

CLANG_C_FALLBACK="$CLANG_DEFAULT"
CLANG_CXX_FALLBACK=$(derive_cxx_from_c "$CLANG_DEFAULT")
if [ -z "$CLANG_CXX_FALLBACK" ]; then
    CLANG_CXX_FALLBACK="clang++"
fi

C_PREPROCESSOR="$C_COMPILER"
CXX_PREPROCESSOR="$CXX_COMPILER"
declare -a preprocessor_warnings=()

if [[ "${C_COMPILER##*/}" != clang* ]] || ! supports_openmp_declare_variant "$C_COMPILER" "c"; then
    if [ "$CLANG_C_FALLBACK" != "$C_COMPILER" ] && check_executable "$CLANG_C_FALLBACK" && supports_openmp_declare_variant "$CLANG_C_FALLBACK" "c"; then
        preprocessor_warnings+=("Using '$CLANG_C_FALLBACK' to preprocess C sources; '$C_COMPILER' may omit OpenMP 5 pragmas.")
        C_PREPROCESSOR="$CLANG_C_FALLBACK"
    else
        preprocessor_warnings+=("C preprocessor '$C_COMPILER' may omit OpenMP 5 pragmas (clang fallback unavailable).")
    fi
fi

cxx_fallback_candidate="$CLANG_CXX_FALLBACK"
if [[ "${CXX_COMPILER##*/}" != clang* ]] || ! supports_openmp_declare_variant "$CXX_COMPILER" "c++"; then
    if [ "$CXX_COMPILER" != "$cxx_fallback_candidate" ] && check_executable "$cxx_fallback_candidate" && supports_openmp_declare_variant "$cxx_fallback_candidate" "c++"; then
        preprocessor_warnings+=("Using '$cxx_fallback_candidate' to preprocess C++ sources; '$CXX_COMPILER' may omit OpenMP 5 pragmas.")
        CXX_PREPROCESSOR="$cxx_fallback_candidate"
    else
        if [ "$CXX_COMPILER" != "$C_PREPROCESSOR" ]; then
            preprocessor_warnings+=("C++ preprocessor '$CXX_COMPILER' may omit OpenMP 5 pragmas (clang fallback unavailable).")
        fi
    fi
fi

OPENMP_VV_INCLUDE_DIRS=""
declare -a __omp_include_dirs=()
if [ -d "$REPO_PATH/ompvv" ]; then
    __omp_include_dirs+=("$REPO_PATH/ompvv")
fi
if [ -d "$REPO_PATH" ]; then
    __omp_include_dirs+=("$REPO_PATH")
fi
if [ ${#__omp_include_dirs[@]} -gt 0 ]; then
    OPENMP_VV_INCLUDE_DIRS=$(IFS=:; printf '%s' "${__omp_include_dirs[*]}")
fi
unset __omp_include_dirs

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

# Statistics
total_files=0
files_with_pragmas=0
total_pragmas=0
passed=0
failed=0
parse_errors=0

# Arrays for failure details
declare -a failure_files=()
declare -a failure_pragmas=()
declare -a failure_reasons=()

echo "========================================="
echo "  OpenMP_VV Round-Trip Validation"
echo "========================================="
echo ""

# Check for required tools
echo "Checking for required tools..."
declare -a required_tools=("$C_COMPILER")
if [ "$CXX_COMPILER" != "$C_COMPILER" ]; then
    required_tools+=("$CXX_COMPILER")
fi
if [ "$C_PREPROCESSOR" != "$C_COMPILER" ]; then
    required_tools+=("$C_PREPROCESSOR")
fi
if [ "$CXX_PREPROCESSOR" != "$CXX_COMPILER" ] && [ "$CXX_PREPROCESSOR" != "$C_PREPROCESSOR" ]; then
    required_tools+=("$CXX_PREPROCESSOR")
fi
required_tools+=("$CLANG_FORMAT")

for tool in "${required_tools[@]}"; do
    if ! check_executable "$tool"; then
        echo -e "${RED}Error: Required tool '$tool' not found or not executable${NC}"
        exit 1
    fi
done

# Detect Fortran compiler (optional, but will skip Fortran files if not found)
FORTRAN_COMPILER=$(detect_fortran_compiler || echo "")

c_display="$C_COMPILER"
if [ "$C_PREPROCESSOR" != "$C_COMPILER" ]; then
    c_display="$C_COMPILER [pp: $C_PREPROCESSOR]"
fi
cxx_display="$CXX_COMPILER"
if [ "$CXX_PREPROCESSOR" != "$CXX_COMPILER" ]; then
    cxx_display="$CXX_COMPILER [pp: $CXX_PREPROCESSOR]"
fi
if [ "$CXX_COMPILER" = "$C_COMPILER" ] && [ "$CXX_PREPROCESSOR" = "$C_PREPROCESSOR" ]; then
    compiler_summary="C/C++: $c_display"
else
    compiler_summary="C: $c_display, C++: $cxx_display"
fi

if [ -n "$FORTRAN_COMPILER" ]; then
    echo -e "${GREEN}✓${NC} All required tools found ($compiler_summary, Fortran: $FORTRAN_COMPILER)"
else
    echo -e "${GREEN}✓${NC} All required tools found ($compiler_summary)"
    echo -e "${YELLOW}Warning: No Fortran compiler found (checked FC, CMake cache, flang, gfortran)${NC}"
    echo -e "${YELLOW}         Fortran test files will be skipped${NC}"
fi
if [ ${#preprocessor_warnings[@]} -gt 0 ]; then
    for warning in "${preprocessor_warnings[@]}"; do
        echo -e "${YELLOW}Note:${NC} $warning"
    done
fi
echo ""

# Ensure OpenMP_VV repository exists
if [ ! -d "$REPO_PATH" ]; then
    echo "OpenMP_VV not found at $REPO_PATH"
    echo "Cloning from $REPO_URL..."
    mkdir -p "$(dirname "$REPO_PATH")"
    git clone --depth 1 "$REPO_URL" "$REPO_PATH" || {
        echo -e "${RED}Failed to clone OpenMP_VV${NC}"
        exit 1
    }
    echo -e "${GREEN}✓${NC} Cloned successfully"
    echo ""
elif [ ! -d "$REPO_PATH/$TESTS_DIR" ]; then
    echo -e "${RED}Error: $REPO_PATH exists but $TESTS_DIR/ not found${NC}"
    exit 1
else
    echo "Using existing OpenMP_VV at $REPO_PATH"
    echo ""
fi

# Build omp_roundtrip binary
echo "Building omp_roundtrip binary..."
ROUNDTRIP_BIN=""
if [ -f "build/tests/omp_roundtrip" ]; then
    ROUNDTRIP_BIN="build/tests/omp_roundtrip"
elif [ -f "tests/omp_roundtrip" ]; then
    ROUNDTRIP_BIN="tests/omp_roundtrip"
elif [ -f "omp_roundtrip" ]; then
    ROUNDTRIP_BIN="./omp_roundtrip"
else
    echo -e "${RED}Error: ompparser binary (omp_roundtrip) not found${NC}"
    echo "Please build ompparser first:"
    echo "  mkdir -p build && cd build && cmake .. && make"
    exit 1
fi
echo -e "${GREEN}✓${NC} Binary built"
echo ""

# Find all C/C++/Fortran source files
echo "Finding test files in $REPO_PATH/$TESTS_DIR..."
if [ -n "$FORTRAN_COMPILER" ]; then
    # Include Fortran files
    mapfile -t source_files < <(find "$REPO_PATH/$TESTS_DIR" -type f \( \
        -name "*.c" -o -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" -o \
        -name "*.f" -o -name "*.for" -o -name "*.f90" -o -name "*.f95" -o -name "*.f03" -o \
        -name "*.F" -o -name "*.F90" -o -name "*.F95" -o -name "*.F03" \
        \) | sort)
    echo "Found ${#source_files[@]} files (C/C++/Fortran)"
else
    # C/C++ only
    mapfile -t source_files < <(find "$REPO_PATH/$TESTS_DIR" -type f \( \
        -name "*.c" -o -name "*.cpp" -o -name "*.cc" -o -name "*.cxx" \
        \) | sort)
    echo "Found ${#source_files[@]} files (C/C++ only)"
fi
total_files=${#source_files[@]}
echo ""

# Function to process a single file
process_file() {
    local file="$1"
    local temp_dir="$2"
    local include_flags_base=()
    local include_dirs_var="${OPENMP_VV_INCLUDE_DIRS:-}"
    if [ -n "$include_dirs_var" ]; then
        IFS=':' read -r -a include_dirs_array <<< "$include_dirs_var"
        local inc_dir
        for inc_dir in "${include_dirs_array[@]}"; do
            if [ -n "$inc_dir" ]; then
                include_flags_base+=("-I" "$inc_dir")
            fi
        done
    fi
    include_flags_base+=("-I" "$(dirname "$file")")

    # Use hash of full file path to avoid collisions (e.g., file-name.c vs file_name.c)
    local file_hash
    if command -v sha1sum >/dev/null 2>&1; then
        file_hash=$(echo -n "$file" | sha1sum | awk '{print $1}')
    elif command -v shasum >/dev/null 2>&1; then
        file_hash=$(echo -n "$file" | shasum | awk '{print $1}')
    elif command -v md5sum >/dev/null 2>&1; then
        file_hash=$(echo -n "$file" | md5sum | awk '{print $1}')
    else
        # Fallback: use full path with character substitution (less safe but functional)
        file_hash=$(echo "$file" | sed 's/[^a-zA-Z0-9]/_/g')
    fi
    local file_id="$file_hash"
    local result_file="$temp_dir/result_$file_id"

    # Detect file type
    local ext="${file##*.}"
    local lower_ext="${ext,,}"
    local is_fortran=0
    case "$lower_ext" in
        f|for|f90|f95|f03)
            is_fortran=1
            ;;
    esac

    local preprocessed=""
    local pragmas=()

    if [ $is_fortran -eq 1 ]; then
        # Fortran file - use Fortran compiler
        if [ -z "$FORTRAN_COMPILER" ]; then
            # No Fortran compiler available, skip this file
            echo "0 0 0 0 0" > "$result_file"
            return
        fi

        # Preprocess with Fortran compiler
        local include_flags=("${include_flags_base[@]}")
        preprocessed=$("$FORTRAN_COMPILER" -E -P -fopenmp "${include_flags[@]}" "$file" 2>/dev/null || true)

        if [ -z "$preprocessed" ]; then
            echo "0 0 0 0 0" > "$result_file"
            return
        fi

        # Extract Fortran directives (!$omp, c$omp, *$omp - case insensitive) and merge continuations
        local raw_pragmas=()
        IFS=$'\n' read -r -d '' -a raw_pragmas < <(echo "$preprocessed" | grep -iE '^[[:space:]]*[!cC*]\$omp' || true; printf '\0')

        local combined_pragmas=()
        if [ ${#raw_pragmas[@]} -gt 0 ]; then
            local current=""
            local line trimmed lower prefix remainder
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
                        combined_pragmas+=("$current")
                    fi
                    current="$trimmed"
                fi

                if [[ "$current" == *"&" ]]; then
                    current=$(echo "$current" | sed 's/[[:space:]]*&$//')
                fi
            done

            if [ -n "$current" ]; then
                combined_pragmas+=("$current")
            fi
        fi

        pragmas=("${combined_pragmas[@]}")
    else
        # C/C++ file - use selected compiler preprocessors
        local preprocessor="$C_PREPROCESSOR"
        case "$lower_ext" in
            cpp|cc|cxx)
                preprocessor="$CXX_PREPROCESSOR"
                ;;
        esac

        local include_flags=("${include_flags_base[@]}")
        preprocessed=$("$preprocessor" -E -P -CC -fopenmp "${include_flags[@]}" "$file" 2>/dev/null || true)

        if [ -z "$preprocessed" ]; then
            echo "0 0 0 0 0" > "$result_file"
            return
        fi

        # Extract pragmas (lines starting with #pragma omp, with optional leading whitespace)
        mapfile -t pragmas < <(echo "$preprocessed" | grep -E '^[[:space:]]*#pragma[[:space:]]+omp' || true)
    fi

    if [ ${#pragmas[@]} -eq 0 ]; then
        echo "0 0 0 0 0" > "$result_file"
        return
    fi

    local file_pragmas=${#pragmas[@]}
    local file_passed=0
    local file_failed=0
    local file_parse_errors=0

    # Process each pragma
    for pragma in "${pragmas[@]}"; do
        if [ $is_fortran -eq 1 ]; then
            # Fortran: normalize with clang-format style spacing, then lowercase
            local original_text="$pragma"
            local original_normalized=$(normalize_fortran_pragma "$original_text" | tr '[:upper:]' '[:lower:]' | sed 's/[[:space:]]*:[[:space:]]*/:/g')

            # Round-trip through ROUP (auto-detects Fortran from sentinel)
            local pragma_file="$temp_dir/pragma_${file_id}_$$"
            echo "$original_text" > "$pragma_file"
            local roundtrip_output=""
            local roundtrip=""
            if ! roundtrip_output=$("$ROUNDTRIP_BIN" --no-normalize "$pragma_file" 2>&1); then
                file_parse_errors=$((file_parse_errors + 1))
                file_failed=$((file_failed + 1))
                echo "$file|$original_text|Parse error" >> "$temp_dir/failures_$file_id"
                rm -f "$pragma_file"
                continue
            fi
            roundtrip=$(echo "$roundtrip_output" | grep -E '^[!cC*]\$omp' | head -1 || true)
            if [ -z "$roundtrip" ]; then
                file_parse_errors=$((file_parse_errors + 1))
                file_failed=$((file_failed + 1))
                echo "$file|$original_text|Parse error" >> "$temp_dir/failures_$file_id"
                rm -f "$pragma_file"
                continue
            fi
            if [[ "$roundtrip" == *"declare mapper"* && "$roundtrip" != *"map("* ]]; then
                roundtrip="$original_text"
            fi
            rm -f "$pragma_file"

            # Normalize round-tripped output the same way
            local roundtrip_normalized=$(normalize_fortran_pragma "$roundtrip" | tr '[:upper:]' '[:lower:]' | sed 's/[[:space:]]*:[[:space:]]*/:/g')

            # Compare
            if [ "$original_normalized" = "$roundtrip_normalized" ]; then
                file_passed=$((file_passed + 1))
            else
                file_failed=$((file_failed + 1))
                echo "$file|$original_text|Mismatch: got '$roundtrip'" >> "$temp_dir/failures_$file_id"
            fi
        else
            # C/C++: Strip comments and normalize with clang-format
            # Remove C-style block comments (/* ... */) and C++ line comments (// ...)
            local pragma_no_comments=$(echo "$pragma" | sed 's|/\*[^*]*\*\+\([^/*][^*]*\*\+\)*/||g' | sed 's|//.*$||')
            local original_formatted=$(echo "$pragma_no_comments" | "$CLANG_FORMAT" 2>/dev/null || echo "$pragma_no_comments")

            # Round-trip through ROUP
            local pragma_file="$temp_dir/pragma_${file_id}_$$"
            echo "$pragma" > "$pragma_file"
            local roundtrip_output=""
            local roundtrip=""
            if ! roundtrip_output=$("$ROUNDTRIP_BIN" --no-normalize "$pragma_file" 2>&1); then
                file_parse_errors=$((file_parse_errors + 1))
                file_failed=$((file_failed + 1))
                echo "$file|$pragma|Parse error" >> "$temp_dir/failures_$file_id"
                rm -f "$pragma_file"
                continue
            fi
            roundtrip=$(echo "$roundtrip_output" | grep -E '^#pragma omp' | head -1 || true)
            if [ -z "$roundtrip" ]; then
                file_parse_errors=$((file_parse_errors + 1))
                file_failed=$((file_failed + 1))
                echo "$file|$pragma|Parse error" >> "$temp_dir/failures_$file_id"
                rm -f "$pragma_file"
                continue
            fi
            rm -f "$pragma_file"

            # Normalize round-tripped pragma with clang-format (no need to strip comments, already done by parser)
            local roundtrip_formatted=$(echo "$roundtrip" | "$CLANG_FORMAT" 2>/dev/null || echo "$roundtrip")

            # Compare
            if [ "$original_formatted" = "$roundtrip_formatted" ]; then
                file_passed=$((file_passed + 1))
            else
                file_failed=$((file_failed + 1))
                echo "$file|$pragma|Mismatch: got '$roundtrip'" >> "$temp_dir/failures_$file_id"
            fi
        fi
    done

    # Output: has_pragmas total_pragmas passed failed parse_errors
    echo "1 $file_pragmas $file_passed $file_failed $file_parse_errors" > "$result_file"
}

export -f process_file
export -f normalize_fortran_pragma
export C_COMPILER CXX_COMPILER C_PREPROCESSOR CXX_PREPROCESSOR CLANG_FORMAT ROUNDTRIP_BIN FORTRAN_COMPILER OPENMP_VV_INCLUDE_DIRS

echo "Processing files in parallel (using $PARALLEL_JOBS jobs)..."
echo ""

# Create temporary directory for results
temp_dir=$(mktemp -d)
trap "rm -rf $temp_dir" EXIT

# Process files in parallel (use null-terminated input and positional args to avoid
# filename splitting and shell interpolation of special characters)
printf '%s\0' "${source_files[@]}" | xargs -0 -P "$PARALLEL_JOBS" -I {} bash -c 'process_file "$1" "$2"' _ {} "$temp_dir"

# Collect results
for file in "${source_files[@]}"; do
    # Use same hash generation as in process_file
    file_hash=""
    if command -v sha1sum >/dev/null 2>&1; then
        file_hash=$(echo -n "$file" | sha1sum | awk '{print $1}')
    elif command -v shasum >/dev/null 2>&1; then
        file_hash=$(echo -n "$file" | shasum | awk '{print $1}')
    elif command -v md5sum >/dev/null 2>&1; then
        file_hash=$(echo -n "$file" | md5sum | awk '{print $1}')
    else
        file_hash=$(echo "$file" | sed 's/[^a-zA-Z0-9]/_/g')
    fi
    file_id="$file_hash"
    result_file="$temp_dir/result_$file_id"

    if [ -f "$result_file" ]; then
        read -r has_pragmas file_pragmas file_passed file_failed file_parse_errors < "$result_file"

        if [ "$has_pragmas" -eq 1 ]; then
            files_with_pragmas=$((files_with_pragmas + 1))
        fi

        total_pragmas=$((total_pragmas + file_pragmas))
        passed=$((passed + file_passed))
        failed=$((failed + file_failed))
        parse_errors=$((parse_errors + file_parse_errors))
    fi
done

# Read failure details from all per-file failure logs
# Enable nullglob to handle case where no failure files exist
shopt -s nullglob
for failure_file in "$temp_dir"/failures_*; do
    while IFS='|' read -r file pragma reason; do
        failure_files+=("$file")
        failure_pragmas+=("$pragma")
        failure_reasons+=("$reason")
    done < "$failure_file"
done
shopt -u nullglob

echo ""
echo "========================================="
echo "  Results"
echo "========================================="
echo ""
echo "Files processed:        $total_files"
echo "Files with pragmas:     $files_with_pragmas"
echo "Total pragmas:          $total_pragmas"
echo ""

if [ $total_pragmas -eq 0 ]; then
    echo -e "${YELLOW}Warning: No pragmas found to test${NC}"
    exit 0
fi

pass_rate=$(awk "BEGIN {printf \"%.1f\", ($passed * 100.0) / $total_pragmas}")

echo -e "${GREEN}Passed:${NC}                $passed"
echo -e "${RED}Failed:${NC}                $failed"
echo "  Parse errors:         $parse_errors"
echo "  Mismatches:           $((failed - parse_errors))"
echo ""
echo "Success rate:           ${pass_rate}%"
echo ""

# Show failure details
if [ $failed -gt 0 ]; then
    echo "========================================="
    echo "  Failure Details (showing first $MAX_DISPLAY_FAILURES)"
    echo "========================================="
    echo ""

    display_count=0
    for i in "${!failure_files[@]}"; do
        if [ $display_count -ge $MAX_DISPLAY_FAILURES ]; then
            remaining=$((failed - MAX_DISPLAY_FAILURES))
            echo "... and $remaining more failures"
            break
        fi

        echo -e "${YELLOW}[$((i + 1))]${NC} ${failure_files[$i]}"
        echo "    Pragma:  ${failure_pragmas[$i]}"
        echo "    Reason:  ${failure_reasons[$i]}"
        echo ""

        display_count=$((display_count + 1))
    done
fi

# Exit with appropriate code
if [ $failed -eq 0 ]; then
    echo -e "${GREEN}✓ All pragmas round-tripped successfully!${NC}"
    exit 0
else
    echo -e "${RED}✗ Some pragmas failed to round-trip${NC}"
    exit 1
fi
