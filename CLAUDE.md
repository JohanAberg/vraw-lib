# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

## Build Commands

```bash
# Configure and build (from repo root)
mkdir build && cd build
cmake ..
make -j4

# Build with tests
cmake -DVRAW_BUILD_TESTS=ON ..
make -j4

# Run all tests
ctest
# Or run directly for verbose output
./test_variants

# Install
cmake --install . --prefix /usr/local
```

## Architecture

C++ library for reading/writing VRAW RAW video files. All public API is in the `vraw` namespace.

**Data Flow:**

```
Write: uint16_t pixels -> [LOG encode] -> [bit-pack] -> [LZ4 compress] -> file
Read:  file -> [LZ4 decompress] -> [bit-unpack] -> uint16_t pixels (LOG-encoded)
```

Note: Reader returns LOG-encoded data as-is. Use `decodeLog10Bit()`/`decodeLog12Bit()` to convert back to linear.

**Core Components:**
- `VrawWriter` - Writes frames with optional LOG encoding, bit-packing, and LZ4 compression
- `VrawReader` - Reads frames with random access via frame index table; handles decompression and unpacking
- `Encoding` - Log2 encode/decode for 10-bit (0-1023) and 12-bit (0-4095) pixel data

**File Format:**
- 512-byte header + frame data + optional audio (MAUD) + frame index table (MIDX)
- Frame header: 64 bytes (timestamp, exposure, white balance, etc.)
- Packing detection: `uncompressed_size < pixelCount * 2`

**Supported Variants (all tested):**
- Encodings: `LINEAR_10BIT`, `LOG2_10BIT`, `LINEAR_12BIT`, `LOG2_12BIT`
- Compression: None or LZ4
- Packing: 10-bit packed (5 bytes per 4 pixels) or 12-bit packed (3 bytes per 2 pixels)

**Dependencies:** LZ4 bundled in `src/lz4/` (no external dependency).

## Code Style

- C++17 standard
- Single include via `<vraw.h>`
- Types in `VrawTypes.h`: `BayerPattern`, `Encoding`, `Compression`, `FileHeader`, `FrameHeader`
- `LOG_8BIT` and `CINEON_10BIT` encoding types are reserved (not implemented)
