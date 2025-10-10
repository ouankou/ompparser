# Use Ubuntu 24.04 LTS base image
FROM ubuntu:24.04

ARG DEBIAN_FRONTEND=noninteractive

# Install prerequisites
RUN apt-get update && apt-get install -y --no-install-recommends \
    ca-certificates \
    curl \
    gnupg \
    lsb-release \
    software-properties-common \
    build-essential \
    git \
    python3 \
    python3-pip \
    wget \
    sudo \
 && rm -rf /var/lib/apt/lists/*

# Create a non-root user 'vscode' (matching codespaces defaults)
ARG USERNAME=vscode
ARG USER_UID=1000
ARG USER_GID=$USER_UID
# Ensure a 'vscode' passwd entry exists. If the requested UID/GID are taken,
# find the next available UID/GID (starting from the requested values) and
# create a group/user using those IDs. This avoids using `-o` while ensuring
# that a user named 'vscode' exists for Codespaces tooling.
RUN if getent passwd $USERNAME >/dev/null 2>&1; then \
            echo "user $USERNAME already exists"; \
        else \
            # find available GID
            GID=$USER_GID; \
            while getent group $GID >/dev/null 2>&1; do GID=$((GID+1)); done; \
            groupadd --gid $GID $USERNAME; \
            # find available UID
            UID=$USER_UID; \
            while id -u $UID >/dev/null 2>&1; do UID=$((UID+1)); done; \
            useradd -u $UID -g $GID -m -s /bin/bash $USERNAME; \
            echo "Created user $USERNAME with UID=$UID GID=$GID"; \
        fi \
 && echo "$USERNAME ALL=(ALL) NOPASSWD:ALL" > /etc/sudoers.d/$USERNAME \
 && chmod 0440 /etc/sudoers.d/$USERNAME

WORKDIR /home/$USERNAME

# Download and run official LLVM install script to install LLVM 21 (all option)
# Script: https://apt.llvm.org/llvm.sh
RUN curl -fsSL https://apt.llvm.org/llvm.sh -o /tmp/llvm.sh \
 && chmod +x /tmp/llvm.sh \
 # Run as root to allow apt install inside script (avoid sudo so installer runs reliably)
 && bash /tmp/llvm.sh 21 all \
 && rm -f /tmp/llvm.sh

# Set environment variables for LLVM 21
ENV LLVM_PREFIX=/usr/lib/llvm-21
ENV PATH="$LLVM_PREFIX/bin:${PATH}"
ENV LD_LIBRARY_PATH="${LLVM_PREFIX}/lib"
ENV LIBRARY_PATH="${LLVM_PREFIX}/lib"
ENV CMAKE_PREFIX_PATH="${LLVM_PREFIX}"

# Ensure linker can find libraries system-wide
USER root
RUN echo "/usr/lib/llvm-21/lib" > /etc/ld.so.conf.d/llvm-21.conf \
 && ldconfig

# Sanity check: print user/group info to help debug builds
RUN echo "--- user/group info ---" && id || true && getent passwd $USERNAME || true && getent group $USER_GID || true

# Install profile script so interactive/login shells pick up LLVM variables
# COPY from repository path; Docker build context is the repo root so use the
# relative path to the file inside the repo.
COPY --chown=root:root .devcontainer/llvm-paths.sh /etc/profile.d/llvm-21.sh
RUN chmod 644 /etc/profile.d/llvm-21.sh

# Sanity check: list the installed profile script
RUN echo "--- profile.d contents ---" && ls -la /etc/profile.d/ && echo "--- llvm script ---" && sed -n '1,200p' /etc/profile.d/llvm-21.sh || true

USER $USERNAME

# Minimal entrypoint
CMD ["bash"]
