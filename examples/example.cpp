/**
 * VRAW Library - Example Usage
 *
 * This example demonstrates how to write and read VRAW video files.
 */

#include <vraw.h>
#include <iostream>
#include <vector>
#include <cstring>

// Generate a test frame with a gradient pattern
std::vector<uint16_t> generateTestFrame(uint32_t width, uint32_t height, uint32_t frameNum) {
    std::vector<uint16_t> frame(width * height);
    for (uint32_t y = 0; y < height; y++) {
        for (uint32_t x = 0; x < width; x++) {
            // Create a moving gradient pattern
            uint32_t value = ((x + frameNum * 10) + (y + frameNum * 5)) % 4096;
            frame[y * width + x] = static_cast<uint16_t>(value);
        }
    }
    return frame;
}

int main(int argc, char* argv[]) {
    std::cout << "VRAW Library v" << vraw::getVersion() << std::endl;
    std::cout << std::endl;

    const uint32_t width = 1920;
    const uint32_t height = 1080;
    const uint32_t frameCount = 10;
    const char* filename = "test_output.vraw";

    if (argc > 1) {
        filename = argv[1];
    }

    // === WRITING ===
    std::cout << "Writing " << frameCount << " frames to " << filename << "..." << std::endl;

    vraw::VrawWriter writer;

    // Initialize with 12-bit linear encoding and LZ4 compression
    uint16_t blackLevel[4] = {256, 256, 256, 256};
    if (!writer.init(width, height, filename,
                     vraw::Encoding::LINEAR_12BIT,  // encoding
                     false,                          // packing
                     true,                           // compression
                     vraw::BayerPattern::RGGB,      // bayer pattern
                     blackLevel,                     // black level
                     4095,                           // white level
                     0,                              // orientation
                     width, height)) {               // native dimensions
        std::cerr << "Failed to initialize writer" << std::endl;
        return 1;
    }

    if (!writer.start()) {
        std::cerr << "Failed to start recording" << std::endl;
        return 1;
    }

    for (uint32_t i = 0; i < frameCount; i++) {
        auto frame = generateTestFrame(width, height, i);
        uint64_t timestamp = i * 41667;  // ~24fps in microseconds

        if (!writer.submitFrame(frame.data(), timestamp)) {
            std::cerr << "Failed to write frame " << i << std::endl;
            return 1;
        }

        std::cout << "  Frame " << i << " written" << std::endl;
    }

    if (!writer.stop()) {
        std::cerr << "Failed to stop recording" << std::endl;
        return 1;
    }

    std::cout << "Wrote " << writer.getFrameCount() << " frames, "
              << writer.getBytesWritten() << " bytes" << std::endl;
    std::cout << std::endl;

    // === READING ===
    std::cout << "Reading back from " << filename << "..." << std::endl;

    vraw::VrawReader reader;
    if (!reader.open(filename)) {
        std::cerr << "Failed to open file for reading" << std::endl;
        return 1;
    }

    const auto& header = reader.getFileHeader();
    std::cout << "  Version: " << header.version << std::endl;
    std::cout << "  Resolution: " << header.width << "x" << header.height << std::endl;
    std::cout << "  Encoding: " << static_cast<int>(header.encoding) << std::endl;
    std::cout << "  Compression: " << static_cast<int>(header.compression) << std::endl;
    std::cout << "  Frame count: " << reader.getFrameCount() << std::endl;
    std::cout << "  Orientation: " << header.sensorOrientation << "°" << std::endl;

    if (header.hasTimecode) {
        std::cout << "  Timecode: "
                  << static_cast<int>(header.timecode.hours) << ":"
                  << static_cast<int>(header.timecode.minutes) << ":"
                  << static_cast<int>(header.timecode.seconds) << ":"
                  << static_cast<int>(header.timecode.frames)
                  << " @ " << static_cast<int>(header.timecode.fps) << "fps"
                  << std::endl;
    }

    std::cout << std::endl;

    // Read first frame
    auto frame = reader.readFrame(0);
    if (frame.valid) {
        std::cout << "  Frame 0: " << frame.pixelData.size() << " bytes, "
                  << "timestamp=" << frame.header.timestampUs << "us"
                  << std::endl;
    } else {
        std::cerr << "Failed to read frame 0" << std::endl;
    }

    reader.close();

    std::cout << std::endl;
    std::cout << "Done!" << std::endl;

    return 0;
}
