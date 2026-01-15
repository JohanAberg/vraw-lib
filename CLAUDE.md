# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure and build
mkdir build && cd build
cmake ..
make -j4

# Build with tests enabled
cmake -DVRAW_BUILD_TESTS=ON ..
make

# Install
cmake --install . --prefix /usr/local
```

## Architecture

C++ library for reading/writing VRAW RAW video files. All public API is in the `vraw` namespace.

**Core Components:**
- `VrawWriter` - Writes frames to .vraw files with optional LZ4 compression and LOG encoding
- `VrawReader` - Reads frames with random access via frame index table
- `Encoding` - Log2 encode/decode functions for 10-bit and 12-bit pixel data

**File Format:**
- 512-byte header + frame data + optional audio + frame index table
- Supports 10/12-bit linear or LOG2 encoding, optional LZ4 compression
- Audio: PCM16 interleaved

**Dependencies:** LZ4 is bundled in `src/lz4/` (compiled directly, no external dependency).

## Code Style

- C++17 standard
- Single include via `<vraw.h>` pulls in all headers
- Types defined in `VrawTypes.h` (enums: `BayerPattern`, `Encoding`, `Compression`)
