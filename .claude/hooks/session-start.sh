#!/bin/bash
# SessionStart hook for MediaInspector
# This script runs when a Claude Code session starts or resumes
# Handles both local (macOS) and cloud (Ubuntu) environments

set -e

echo "=== MediaInspector Session Context ==="

# Detect environment
if [[ "$OSTYPE" == "darwin"* ]]; then
    PLATFORM="macos"
    echo "Platform: macOS (local)"
elif [[ -f /etc/os-release ]]; then
    PLATFORM="linux"
    . /etc/os-release
    echo "Platform: Linux ($NAME $VERSION_ID)"
else
    PLATFORM="unknown"
    echo "Platform: Unknown"
fi

# Function to check if a command exists
command_exists() {
    command -v "$1" >/dev/null 2>&1
}

# Install dependencies for Linux/Cloud environment
install_linux_deps() {
    echo "Checking/installing dependencies for Linux..."

    # Check if we need to install anything
    NEED_INSTALL=false

    if ! command_exists cmake; then NEED_INSTALL=true; fi
    if ! command_exists ninja; then NEED_INSTALL=true; fi
    if ! command_exists qmake6 && ! command_exists qmake; then NEED_INSTALL=true; fi
    if ! pkg-config --exists Qt6Core 2>/dev/null; then NEED_INSTALL=true; fi

    if [ "$NEED_INSTALL" = true ]; then
        echo "Installing build dependencies..."

        # Update package lists
        sudo apt-get update -qq

        # Install base development tools
        sudo apt-get install -y -qq \
            build-essential \
            cmake \
            ninja-build \
            git \
            pkg-config \
            wget \
            curl \
            ca-certificates

        # Install Qt6 (Ubuntu 22.04+ has Qt6, but may need backports for 6.5+)
        sudo apt-get install -y -qq \
            qt6-base-dev \
            qt6-base-private-dev \
            qt6-declarative-dev \
            qt6-shader-baker \
            qt6-tools-dev \
            qml6-module-qtquick \
            qml6-module-qtquick-controls \
            qml6-module-qtquick-layouts \
            qml6-module-qtquick-window \
            libqt6opengl6-dev \
            libgl1-mesa-dev \
            libglu1-mesa-dev \
            libegl1-mesa-dev \
            libxkbcommon-dev \
            libwayland-dev \
            libxcb1-dev \
            libx11-xcb-dev \
            || echo "Some Qt6 packages may not be available"

        # Install FFmpeg
        sudo apt-get install -y -qq \
            ffmpeg \
            libavcodec-dev \
            libavformat-dev \
            libavutil-dev \
            libswscale-dev \
            libavfilter-dev \
            || echo "FFmpeg installation skipped"

        # Install OpenColorIO
        sudo apt-get install -y -qq \
            libopencolorio-dev \
            || echo "OpenColorIO not available"

        # Install OpenEXR
        sudo apt-get install -y -qq \
            libopenexr-dev \
            || echo "OpenEXR not available"

        # Install LZ4
        sudo apt-get install -y -qq \
            liblz4-dev

        # Install Vulkan (for compute shaders)
        sudo apt-get install -y -qq \
            libvulkan-dev \
            vulkan-tools \
            mesa-vulkan-drivers \
            spirv-tools \
            || echo "Vulkan packages skipped"

        # Install Python development (for bindings)
        sudo apt-get install -y -qq \
            python3-dev \
            pybind11-dev \
            || echo "Python dev packages skipped"

        echo "Dependencies installed."
    else
        echo "Dependencies already installed."
    fi
}

# Set environment variables
setup_environment() {
    if [ -n "$CLAUDE_ENV_FILE" ]; then
        if [ "$PLATFORM" = "macos" ]; then
            # Qt paths for macOS (local development)
            if [ -d "/Users/johan/Qt/6.8.2/macos" ]; then
                echo 'export QT_DIR="/Users/johan/Qt/6.8.2/macos"' >> "$CLAUDE_ENV_FILE"
                echo 'export PATH="$QT_DIR/bin:$PATH"' >> "$CLAUDE_ENV_FILE"
            fi
        else
            # Linux paths
            echo 'export QT_QPA_PLATFORM=offscreen' >> "$CLAUDE_ENV_FILE"
            echo 'export QT_LOGGING_RULES="qt.rhi.*=false"' >> "$CLAUDE_ENV_FILE"
        fi

        # Default execution mode for v2_timeline_test
        echo 'export MI_EXECUTION_MODE=hybrid' >> "$CLAUDE_ENV_FILE"
    fi
}

# Check build status
check_build_status() {
    cd "$CLAUDE_PROJECT_DIR" 2>/dev/null || return

    # Check build directory
    if [ -d "cmake-build-release" ]; then
        echo "Build directory: cmake-build-release (exists)"
        BUILD_DIR="cmake-build-release"
    elif [ -d "build" ]; then
        echo "Build directory: build (exists)"
        BUILD_DIR="build"
    else
        echo "Build directory: NOT FOUND - run cmake to configure"
        BUILD_DIR=""
    fi

    # Check test binary
    if [ -n "$BUILD_DIR" ] && [ -x "$BUILD_DIR/test_v2_pipeline" ]; then
        echo "Tests: test_v2_pipeline binary found"
    else
        echo "Tests: test_v2_pipeline NOT FOUND - build required"
    fi

    # Git status summary
    if [ -d ".git" ]; then
        BRANCH=$(git branch --show-current 2>/dev/null)
        AHEAD=$(git rev-list --count @{u}..HEAD 2>/dev/null || echo "0")
        BEHIND=$(git rev-list --count HEAD..@{u} 2>/dev/null || echo "0")
        DIRTY=$(git status --porcelain 2>/dev/null | wc -l | tr -d ' ')
        echo "Git: $BRANCH (ahead: $AHEAD, behind: $BEHIND, uncommitted: $DIRTY)"
    fi
}

# Main execution
if [ "$PLATFORM" = "linux" ]; then
    install_linux_deps
fi

setup_environment
check_build_status

echo "=== Session Ready ==="
exit 0
