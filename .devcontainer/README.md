# Dev Container for ompparser (LLVM 21)

This devcontainer installs LLVM 21 from the official LLVM APT repository using the official install script (https://apt.llvm.org/llvm.sh) with the "all" option. It's tuned for Ubuntu 24.04 base image and sets PATH and library paths so tools like `clang-21`, `clangd-21`, and `llvm-config-21` are available.

How it works
- The Dockerfile uses `ubuntu:24.04` as base.
- It downloads and runs the official `llvm.sh` script as root to install LLVM 21 with `all` packages.
- Environment variables (PATH, LD_LIBRARY_PATH, LIBRARY_PATH, CMAKE_PREFIX_PATH) are set in the image to include `/usr/lib/llvm-21`.
- `/etc/ld.so.conf.d/llvm-21.conf` is added and `ldconfig` is run so dynamic loader finds LLVM libraries.

Usage
1. Open this repository in GitHub Codespaces or VS Code Remote - Containers.
2. The devcontainer will build automatically. After build the `postCreateCommand` runs `.devcontainer/verify-llvm.sh` to show installed tool versions.

Troubleshooting
- If the build fails when running `llvm.sh`, check build logs: it may be a network or apt key issue.
- If tools aren't found in the container shell, ensure the container was rebuilt (not just reopened) since ENV changes occur at build time.
