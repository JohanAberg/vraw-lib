# VRAW Library

A standalone C++ library for reading and writing VRAW video files.

VRAW is a RAW video format designed for high-quality camera capture with support for:
- 10-bit or 12-bit pixel depth
- Optional LOG2 encoding for better dynamic range
- Optional LZ4 compression
- Audio stream support (PCM16)
- Timecode and orientation metadata

## Building

```bash
mkdir build && cd build
cmake ..
make
```

### Build Options

- `VRAW_BUILD_EXAMPLES` - Build example programs (default: ON)
- `VRAW_BUILD_TESTS` - Build tests (default: OFF)

### Install

```bash
cmake --install . --prefix /usr/local
```

## Usage

### Writing VRAW Files

```cpp
#include <vraw.h>

vraw::VrawWriter writer;

// Initialize with 12-bit encoding and LZ4 compression
uint16_t blackLevel[4] = {256, 256, 256, 256};
writer.init(1920, 1080, "output.vraw",
            vraw::Encoding::LINEAR_12BIT,
            false,  // packing
            true,   // compression
            vraw::BayerPattern::RGGB,
            blackLevel,
            4095);  // white level

writer.start();

// Write frames
for (int i = 0; i < frameCount; i++) {
    writer.submitFrame(frameData, timestampUs);
}

writer.stop();
```

### Reading VRAW Files

```cpp
#include <vraw.h>

vraw::VrawReader reader;
reader.open("input.vraw");

auto header = reader.getFileHeader();
printf("Resolution: %dx%d\n", header.width, header.height);
printf("Frames: %d\n", reader.getFrameCount());

// Read frames
for (uint32_t i = 0; i < reader.getFrameCount(); i++) {
    auto frame = reader.readFrame(i);
    if (frame.valid) {
        // Process frame.pixelData
    }
}

reader.close();
```

### Audio Support

```cpp
// Writing audio
writer.enableAudio(48000, 2);  // 48kHz stereo
writer.start();
writer.submitAudio(audioSamples, sampleCount, timestampUs);
writer.stop();

// Reading audio
vraw::AudioHeader audioHeader;
std::vector<int16_t> samples;
if (reader.readAudio(audioHeader, samples)) {
    // Process audio samples
}
```

## File Format

### Header Structure (512 bytes)

| Offset | Size | Field | Description |
|--------|------|-------|-------------|
| 0 | 4 | magic | "VRAW" |
| 4 | 4 | version | Format version (2) |
| 8 | 4 | width | Frame width |
| 12 | 4 | height | Frame height |
| 16 | 1 | bayer_pattern | 0=RGGB, 1=GRBG, 2=GBRG, 3=BGGR |
| 17 | 1 | encoding | Encoding type |
| 18 | 1 | compression | Compression type |
| 20-27 | 8 | black_level | Per-channel black levels |
| 28 | 2 | white_level | White level |
| 32 | 4 | frame_count | Number of frames |
| 36 | 8 | index_offset | Offset to frame index |
| 44-59 | 16 | native/binning | Native dimensions and binning |
| 60-83 | 24 | audio | Audio stream info |
| 84-99 | 16 | timecode | Timecode info |
| 100 | 4 | orientation | Sensor orientation (degrees) |

### Frame Structure

Each frame consists of:
- 64-byte frame header (timestamp, metadata)
- Pixel data (raw, packed, or compressed)

### Tools

The library includes command-line tools:

- `vraw_info` - Display information about VRAW files
- `vraw_example` - Example read/write program

## License

Part of the Vertigo project - https://github.com/JohanAberg/vertigo
