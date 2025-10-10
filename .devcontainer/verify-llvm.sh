#!/usr/bin/env bash
set -euo pipefail

echo "Verifying LLVM 21 installation"

CLANG=$(command -v clang-21 || true)
CLANGD=$(command -v clangd-21 || true)
LLVM_CONFIG=$(command -v llvm-config-21 || true)

echo "clang-21: ${CLANG:-not found}"
echo "clangd-21: ${CLANGD:-not found}"
echo "llvm-config-21: ${LLVM_CONFIG:-not found}"

if [[ -n "$CLANG" ]]; then
  echo "clang-21 --version:"; clang-21 --version || true
fi
if [[ -n "$CLANGD" ]]; then
  echo "clangd-21 --version:"; clangd-21 --version || true
fi
if [[ -n "$LLVM_CONFIG" ]]; then
  echo "llvm-config-21 --version:"; llvm-config-21 --version || true
  echo "llvm-config-21 --libdir:"; llvm-config-21 --libdir || true
fi

echo "LD_LIBRARY_PATH=${LD_LIBRARY_PATH:-<not set>}"

echo "Done verification. If tools aren't found, check the devcontainer build logs."
