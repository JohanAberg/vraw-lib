/**
 * vraw_info - Display information about VRAW files
 *
 * Usage: vraw_info <file.vraw>
 */

#include <vraw.h>
#include <iostream>
#include <iomanip>

const char* encodingToString(vraw::Encoding enc) {
    switch (enc) {
        case vraw::Encoding::LINEAR_10BIT: return "LINEAR_10BIT";
        case vraw::Encoding::LOG2_10BIT: return "LOG2_10BIT";
        case vraw::Encoding::LOG_8BIT: return "LOG_8BIT (reserved)";
        case vraw::Encoding::CINEON_10BIT: return "CINEON_10BIT (reserved)";
        case vraw::Encoding::LOG2_12BIT: return "LOG2_12BIT";
        case vraw::Encoding::LINEAR_12BIT: return "LINEAR_12BIT";
        default: return "UNKNOWN";
    }
}

const char* compressionToString(vraw::Compression comp) {
    switch (comp) {
        case vraw::Compression::NONE: return "None";
        case vraw::Compression::LZ4_FAST: return "LZ4 Fast";
        case vraw::Compression::LZ4_BALANCED: return "LZ4 Balanced";
        case vraw::Compression::LZ4_HIGH: return "LZ4 High";
        default: return "Unknown";
    }
}

const char* bayerToString(vraw::BayerPattern bp) {
    switch (bp) {
        case vraw::BayerPattern::RGGB: return "RGGB";
        case vraw::BayerPattern::GRBG: return "GRBG";
        case vraw::BayerPattern::GBRG: return "GBRG";
        case vraw::BayerPattern::BGGR: return "BGGR";
        default: return "Unknown";
    }
}

int main(int argc, char* argv[]) {
    if (argc < 2) {
        std::cerr << "Usage: vraw_info <file.vraw>" << std::endl;
        return 1;
    }

    vraw::VrawReader reader;
    if (!reader.open(argv[1])) {
        std::cerr << "Error: Failed to open " << argv[1] << std::endl;
        return 1;
    }

    const auto& h = reader.getFileHeader();

    std::cout << "VRAW File: " << argv[1] << std::endl;
    std::cout << std::string(60, '=') << std::endl;
    std::cout << std::endl;

    std::cout << "Format Information:" << std::endl;
    std::cout << "  Version:        " << h.version << std::endl;
    std::cout << "  Encoding:       " << encodingToString(h.encoding) << std::endl;
    std::cout << "  Compression:    " << compressionToString(h.compression) << std::endl;
    std::cout << "  Bayer Pattern:  " << bayerToString(h.bayerPattern) << std::endl;
    std::cout << std::endl;

    std::cout << "Resolution:" << std::endl;
    std::cout << "  Effective:      " << h.width << " x " << h.height << std::endl;
    std::cout << "  Native:         " << h.nativeWidth << " x " << h.nativeHeight << std::endl;
    std::cout << "  Binning:        " << h.binningNum << ":" << h.binningDen << std::endl;
    std::cout << std::endl;

    std::cout << "Sensor Levels:" << std::endl;
    std::cout << "  Black Level:    [" << h.blackLevel[0] << ", " << h.blackLevel[1]
              << ", " << h.blackLevel[2] << ", " << h.blackLevel[3] << "]" << std::endl;
    std::cout << "  White Level:    " << h.whiteLevel << std::endl;
    std::cout << std::endl;

    std::cout << "Content:" << std::endl;
    std::cout << "  Frame Count:    " << reader.getFrameCount() << std::endl;
    std::cout << "  Orientation:    " << h.sensorOrientation << "°" << std::endl;
    std::cout << std::endl;

    if (h.hasTimecode) {
        std::cout << "Timecode:" << std::endl;
        std::cout << "  Start:          "
                  << std::setfill('0')
                  << std::setw(2) << static_cast<int>(h.timecode.hours) << ":"
                  << std::setw(2) << static_cast<int>(h.timecode.minutes) << ":"
                  << std::setw(2) << static_cast<int>(h.timecode.seconds)
                  << (h.timecode.dropFrame ? ";" : ":")
                  << std::setw(2) << static_cast<int>(h.timecode.frames)
                  << std::endl;
        std::cout << "  Frame Rate:     " << static_cast<int>(h.timecode.fps) << " fps"
                  << (h.timecode.dropFrame ? " (drop-frame)" : " (non-drop)") << std::endl;
        std::cout << std::endl;
    }

    if (h.hasAudio) {
        std::cout << "Audio:" << std::endl;
        std::cout << "  Sample Rate:    " << h.audioSampleRate << " Hz" << std::endl;
        std::cout << "  Channels:       " << static_cast<int>(h.audioChannels) << std::endl;
        std::cout << "  Bit Depth:      " << static_cast<int>(h.audioBitDepth) << " bit" << std::endl;
        std::cout << std::endl;
    }

    // Read first frame for additional info
    if (reader.getFrameCount() > 0) {
        auto frame = reader.readFrame(0);
        if (frame.valid) {
            std::cout << "First Frame:" << std::endl;
            std::cout << "  Timestamp:      " << frame.header.timestampUs << " µs" << std::endl;
            std::cout << "  Data Size:      " << frame.pixelData.size() << " bytes" << std::endl;
            std::cout << "  ISO:            " << frame.header.iso << std::endl;
            std::cout << "  Exposure:       " << frame.header.exposureTimeMs << " ms" << std::endl;
            std::cout << "  White Balance:  R=" << frame.header.whiteBalanceR
                      << " G=" << frame.header.whiteBalanceG
                      << " B=" << frame.header.whiteBalanceB << std::endl;
            std::cout << std::endl;
        }
    }

    reader.close();
    return 0;
}
