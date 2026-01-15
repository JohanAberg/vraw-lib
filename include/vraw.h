/**
 * VRAW Library - Main Header
 *
 * Standalone C++ library for reading and writing VRAW video files.
 *
 * VRAW is a RAW video format designed for high-quality camera capture:
 * - 10-bit or 12-bit pixel depth
 * - Optional LOG2 encoding for better dynamic range
 * - Optional LZ4 compression
 * - Audio stream support (PCM16)
 * - Timecode and orientation metadata
 *
 * File format specification:
 * - 512-byte file header
 * - Frame data (64-byte header + pixel data per frame)
 * - Optional audio stream
 * - Frame index table at end
 *
 * https://github.com/JohanAberg/vertigo
 *
 * Usage:
 *   #include <vraw.h>
 *
 *   // Writing
 *   vraw::VrawWriter writer;
 *   writer.init(1920, 1080, "output.vraw");
 *   writer.start();
 *   writer.submitFrame(data, timestamp);
 *   writer.stop();
 *
 *   // Reading
 *   vraw::VrawReader reader;
 *   reader.open("input.vraw");
 *   auto frame = reader.readFrame(0);
 *   reader.close();
 */

#ifndef VRAW_H
#define VRAW_H

#include "VrawTypes.h"
#include "VrawWriter.h"
#include "VrawReader.h"
#include "Encoding.h"

// Library version
#define VRAW_VERSION_MAJOR 2
#define VRAW_VERSION_MINOR 0
#define VRAW_VERSION_PATCH 0

namespace vraw {

/**
 * Get library version string.
 */
inline const char* getVersion() {
    return "2.0.0";
}

} // namespace vraw

#endif // VRAW_H
