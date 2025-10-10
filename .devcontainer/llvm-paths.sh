#!/usr/bin/env bash
# LLVM path helper
# This script configures environment variables for LLVM 21. It prefers a local
# development LLVM tree under $HOME/Projects/llvm-21 if present, otherwise it
# falls back to the system installation under /usr/lib/llvm-21.

LLVM="$HOME/Projects/llvm-21"
if [ -d "$LLVM" ]; then
    LLVM_PATH="$LLVM/llvm_install"
else
    LLVM_PATH="/usr/lib/llvm-21"
fi

export LLVM
export LLVM_PATH
export LLVM_SRC="$LLVM/llvm_src"
export LLVM_BUILD="$LLVM/llvm_build"

# Paths
export PATH="$LLVM_PATH/bin:$PATH"

if [ -z "${LD_LIBRARY_PATH:-}" ]; then
    export LD_LIBRARY_PATH="$LLVM_PATH/libexec:$LLVM_PATH/lib:$LLVM_PATH/lib/x86_64-unknown-linux-gnu"
else
    export LD_LIBRARY_PATH="$LLVM_PATH/libexec:$LLVM_PATH/lib:$LLVM_PATH/lib/x86_64-unknown-linux-gnu:$LD_LIBRARY_PATH"
fi

if [ -z "${LIBRARY_PATH:-}" ]; then
    export LIBRARY_PATH="$LLVM_PATH/libexec:$LLVM_PATH/lib:$LLVM_PATH/lib/x86_64-linux-gnu"
else
    export LIBRARY_PATH="$LLVM_PATH/libexec:$LLVM_PATH/lib:$LLVM_PATH/lib/x86_64-linux-gnu:$LIBRARY_PATH"
fi

export MANPATH="$LLVM_PATH/share/man:${MANPATH:-}"

if [ -z "${C_INCLUDE_PATH:-}" ]; then
    export C_INCLUDE_PATH="$LLVM_PATH/include"
else
    export C_INCLUDE_PATH="$LLVM_PATH/include:$C_INCLUDE_PATH"
fi

if [ -z "${CPLUS_INCLUDE_PATH:-}" ]; then
    export CPLUS_INCLUDE_PATH="$LLVM_PATH/include"
else
    export CPLUS_INCLUDE_PATH="$LLVM_PATH/include:$CPLUS_INCLUDE_PATH"
fi
