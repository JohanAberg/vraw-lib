/**
 * VRAW Library - Type Definitions
 *
 * Standalone C++ library for reading and writing VRAW video files.
 * https://github.com/JohanAberg/vertigo
 */

#ifndef VRAW_TYPES_H
#define VRAW_TYPES_H

#include <cstdint>

namespace vraw {

// Bayer pattern types
enum class BayerPattern : uint8_t {
    RGGB = 0,
    GRBG = 1,
    GBRG = 2,
    BGGR = 3
};

// Encoding types
enum class Encoding : uint8_t {
    LINEAR_10BIT = 0,    // Raw 10-bit linear
    LOG2_10BIT = 1,      // LOG2 encoded 10-bit
    LOG_8BIT = 2,        // Reserved for future use
    CINEON_10BIT = 3,    // Reserved for future use
    LOG2_12BIT = 4,      // LOG2 encoded 12-bit
    LINEAR_12BIT = 5     // Raw 12-bit linear (default)
};

// Compression types
enum class Compression : uint8_t {
    NONE = 0,
    LZ4_FAST = 1,
    LZ4_BALANCED = 2,
    LZ4_HIGH = 3
};

// Timecode structure
struct Timecode {
    uint8_t hours;
    uint8_t minutes;
    uint8_t seconds;
    uint8_t frames;
    uint8_t fps;
    bool dropFrame;
    uint8_t format;  // 0=SMPTE, 1=LTC, 2=EBU
};

// File header information
struct FileHeader {
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t nativeWidth;
    uint32_t nativeHeight;
    BayerPattern bayerPattern;
    Encoding encoding;
    Compression compression;
    uint16_t blackLevel[4];
    uint16_t whiteLevel;
    uint32_t frameCount;
    uint64_t indexOffset;
    uint32_t binningNum;
    uint32_t binningDen;
    int32_t sensorOrientation;
    bool hasTimecode;
    Timecode timecode;
    bool hasAudio;
    uint8_t audioChannels;
    uint8_t audioBitDepth;
    uint32_t audioSampleRate;
    uint64_t audioOffset;
    uint64_t audioStartTimeUs;
};

// Frame header information
struct FrameHeader {
    uint64_t timestampUs;
    uint32_t frameNumber;
    uint32_t compressedSize;
    uint32_t uncompressedSize;
    float iso;
    float exposureTimeMs;
    float whiteBalanceR;
    float whiteBalanceG;
    float whiteBalanceB;
    float focalLength;
    float aperture;
    float focusDistance;
    uint16_t dynamicBlackLevel[4];
};

// Audio stream header
struct AudioHeader {
    uint32_t sampleRate;
    uint16_t channels;
    uint16_t bitDepth;
    uint64_t sampleCount;
    uint64_t startTimestampUs;
};

} // namespace vraw

#endif // VRAW_TYPES_H
