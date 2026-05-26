# ==============================================================================
# GenesisEmu - Build and Test Environment
# ==============================================================================
# This Dockerfile provides a complete, isolated C++20 toolchain.
# It is designed to compile the Core Domain (emulation logic), run TDD unit 
# tests, and compile the Outer Hexagon (SDL2 Adapters) without installing 
# any dependencies directly on the Windows/WSL host.
# ==============================================================================

# Use Ubuntu 22.04 LTS as the stable base image
FROM ubuntu:22.04

# Prevent interactive prompts during apt installations (e.g., timezone config)
ENV DEBIAN_FRONTEND=noninteractive

# Update package lists and install essential build tools
RUN apt-get update && apt-get install -y \
    # Core C++ Toolchain
    build-essential \
    g++-12 \
    gcc-12 \
    cmake \
    ninja-build \
    gdb \
    # GoogleTest for TDD (Test-Driven Development)
    libgtest-dev \
    # SDL2 libraries for the Outer Hexagon (Video, Audio, Input Adapters)
    libsdl2-dev \
    libsdl2-ttf-dev \
    # Utility tools
    git \
    valgrind \
    && rm -rf /var/lib/apt/lists/*

# Set GCC/G++ 12 as the default compiler (supports C++20 features)
RUN update-alternatives --install /usr/bin/gcc gcc /usr/bin/gcc-12 100 \
    && update-alternatives --install /usr/bin/g++ g++ /usr/bin/g++-12 100

# Build and install GoogleTest globally inside the container
# This allows our CMake configuration to find GTest instantly for our TDD loop
WORKDIR /usr/src/gtest
RUN cmake CMakeLists.txt \
    && make \
    && cp lib/*.a /usr/lib/

# Set the working directory for our emulator source code
# The host machine's source code will be mounted here via docker-compose
WORKDIR /app

# The default command simply keeps the container alive if run detached,
# but we will normally override this via docker-compose commands (e.g., `make test`)
CMD ["/bin/bash"]