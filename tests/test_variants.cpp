/**
 * VRAW Library - Read/Write Variant Tests
 *
 * Tests all combinations of encoding, compression, and packing.
 */

#include <vraw.h>
#include <Encoding.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>
#include <string>
#include <vector>

struct TestConfig {
    const char* name;
    vraw::Encoding encoding;
    bool compression;
    bool packing;
    uint16_t maxValue;
    int tolerance;  // Allowed difference for lossy encodings
};

static const TestConfig testConfigs[] = {
    // LINEAR_10BIT variants
    {"LINEAR_10BIT, no compression, no packing", vraw::Encoding::LINEAR_10BIT, false, false, 1023, 0},
    {"LINEAR_10BIT, no compression, packing",    vraw::Encoding::LINEAR_10BIT, false, true,  1023, 0},
    {"LINEAR_10BIT, LZ4 compression, no packing", vraw::Encoding::LINEAR_10BIT, true,  false, 1023, 0},
    {"LINEAR_10BIT, LZ4 compression, packing",   vraw::Encoding::LINEAR_10BIT, true,  true,  1023, 0},

    // LOG2_10BIT variants
    {"LOG2_10BIT, no compression, no packing",   vraw::Encoding::LOG2_10BIT, false, false, 1023, 4},
    {"LOG2_10BIT, no compression, packing",      vraw::Encoding::LOG2_10BIT, false, true,  1023, 4},
    {"LOG2_10BIT, LZ4 compression, no packing",  vraw::Encoding::LOG2_10BIT, true,  false, 1023, 4},
    {"LOG2_10BIT, LZ4 compression, packing",     vraw::Encoding::LOG2_10BIT, true,  true,  1023, 4},

    // LINEAR_12BIT variants
    {"LINEAR_12BIT, no compression, no packing", vraw::Encoding::LINEAR_12BIT, false, false, 4095, 0},
    {"LINEAR_12BIT, no compression, packing",    vraw::Encoding::LINEAR_12BIT, false, true,  4095, 0},
    {"LINEAR_12BIT, LZ4 compression, no packing", vraw::Encoding::LINEAR_12BIT, true,  false, 4095, 0},
    {"LINEAR_12BIT, LZ4 compression, packing",   vraw::Encoding::LINEAR_12BIT, true,  true,  4095, 0},

    // LOG2_12BIT variants
    {"LOG2_12BIT, no compression, no packing",   vraw::Encoding::LOG2_12BIT, false, false, 4095, 8},
    {"LOG2_12BIT, no compression, packing",      vraw::Encoding::LOG2_12BIT, false, true,  4095, 8},
    {"LOG2_12BIT, LZ4 compression, no packing",  vraw::Encoding::LOG2_12BIT, true,  false, 4095, 8},
    {"LOG2_12BIT, LZ4 compression, packing",     vraw::Encoding::LOG2_12BIT, true,  true,  4095, 8},
};

static const int NUM_TESTS = sizeof(testConfigs) / sizeof(testConfigs[0]);

// Test parameters
static const uint32_t TEST_WIDTH = 64;
static const uint32_t TEST_HEIGHT = 48;
static const uint32_t PIXEL_COUNT = TEST_WIDTH * TEST_HEIGHT;

// Generate deterministic test pattern
static void generateTestData(std::vector<uint16_t>& data, uint16_t maxValue) {
    data.resize(PIXEL_COUNT);
    for (uint32_t i = 0; i < PIXEL_COUNT; i++) {
        // Gradient pattern with some variation
        uint32_t x = i % TEST_WIDTH;
        uint32_t y = i / TEST_WIDTH;
        uint32_t value = (x * maxValue / TEST_WIDTH + y * 17) % (maxValue + 1);
        // Ensure we stay above black level for LOG encoding tests
        if (value < 100) value = 100;
        data[i] = static_cast<uint16_t>(value);
    }
}

// Compare data with tolerance
static bool compareData(const uint16_t* original, const uint16_t* decoded,
                        uint32_t count, int tolerance, int& maxDiff) {
    maxDiff = 0;
    for (uint32_t i = 0; i < count; i++) {
        int diff = std::abs(static_cast<int>(original[i]) - static_cast<int>(decoded[i]));
        if (diff > maxDiff) maxDiff = diff;
        if (diff > tolerance) {
            return false;
        }
    }
    return true;
}

static bool runTest(const TestConfig& config, int testNum) {
    printf("  [%2d/16] %-45s ", testNum, config.name);
    fflush(stdout);

    std::string testFile = "/tmp/vraw_test_" + std::to_string(testNum) + ".vraw";

    // Generate test data
    std::vector<uint16_t> originalData;
    generateTestData(originalData, config.maxValue);

    // Write test file
    {
        vraw::VrawWriter writer;
        uint16_t blackLevel[4] = {64, 64, 64, 64};

        if (!writer.init(TEST_WIDTH, TEST_HEIGHT, testFile,
                         config.encoding, config.packing, config.compression,
                         vraw::BayerPattern::RGGB, blackLevel, config.maxValue)) {
            printf("FAIL (init)\n");
            return false;
        }

        if (!writer.start()) {
            printf("FAIL (start)\n");
            return false;
        }

        // Write 3 frames to test consistency
        for (int frame = 0; frame < 3; frame++) {
            if (!writer.submitFrame(originalData.data(), frame * 33333, 1.0f, 1.0f, 1.0f)) {
                printf("FAIL (submitFrame %d)\n", frame);
                return false;
            }
        }

        if (!writer.stop()) {
            printf("FAIL (stop)\n");
            return false;
        }
    }

    // Read and verify
    {
        vraw::VrawReader reader;

        if (!reader.open(testFile)) {
            printf("FAIL (open)\n");
            std::remove(testFile.c_str());
            return false;
        }

        // Verify header
        const auto& header = reader.getFileHeader();
        if (header.width != TEST_WIDTH || header.height != TEST_HEIGHT) {
            printf("FAIL (dimensions %ux%u)\n", header.width, header.height);
            reader.close();
            std::remove(testFile.c_str());
            return false;
        }

        if (reader.getFrameCount() != 3) {
            printf("FAIL (frame count %u)\n", reader.getFrameCount());
            reader.close();
            std::remove(testFile.c_str());
            return false;
        }

        // Verify all frames
        for (uint32_t frame = 0; frame < 3; frame++) {
            auto result = reader.readFrame(frame);
            if (!result.valid) {
                printf("FAIL (readFrame %u)\n", frame);
                reader.close();
                std::remove(testFile.c_str());
                return false;
            }

            // Check data size
            size_t expectedSize = PIXEL_COUNT * sizeof(uint16_t);
            if (result.pixelData.size() != expectedSize) {
                printf("FAIL (frame %u size %zu, expected %zu)\n",
                       frame, result.pixelData.size(), expectedSize);
                reader.close();
                std::remove(testFile.c_str());
                return false;
            }

            // Get read data
            const uint16_t* readData = reinterpret_cast<const uint16_t*>(result.pixelData.data());

            // For LOG encoding, decode back to linear before comparison
            std::vector<uint16_t> decodedData;
            if (config.encoding == vraw::Encoding::LOG2_10BIT ||
                config.encoding == vraw::Encoding::LOG2_12BIT) {
                decodedData.resize(PIXEL_COUNT);
                uint16_t blackLevel = 64;
                if (config.encoding == vraw::Encoding::LOG2_12BIT) {
                    vraw::decodeLog12Bit(readData, decodedData.data(), PIXEL_COUNT,
                                         blackLevel, config.maxValue);
                } else {
                    vraw::decodeLog10Bit(readData, decodedData.data(), PIXEL_COUNT,
                                         blackLevel, config.maxValue);
                }
                readData = decodedData.data();
            }

            // Compare pixel data
            int maxDiff = 0;
            if (!compareData(originalData.data(), readData, PIXEL_COUNT, config.tolerance, maxDiff)) {
                printf("FAIL (data mismatch, maxDiff=%d, tolerance=%d)\n", maxDiff, config.tolerance);
                reader.close();
                std::remove(testFile.c_str());
                return false;
            }
        }

        reader.close();
    }

    // Cleanup
    std::remove(testFile.c_str());

    printf("PASS\n");
    return true;
}

static bool runAudioTest() {
    printf("  [AUDIO] Audio write/read round-trip                  ");
    fflush(stdout);

    std::string testFile = "/tmp/vraw_test_audio.vraw";

    // Generate test audio (1 second of 48kHz stereo sine wave)
    const uint32_t sampleRate = 48000;
    const uint16_t channels = 2;
    const uint32_t sampleCount = sampleRate;  // 1 second
    std::vector<int16_t> originalAudio(sampleCount * channels);

    for (uint32_t i = 0; i < sampleCount; i++) {
        double t = static_cast<double>(i) / sampleRate;
        int16_t left = static_cast<int16_t>(std::sin(t * 440.0 * 2.0 * 3.14159) * 16000);
        int16_t right = static_cast<int16_t>(std::sin(t * 880.0 * 2.0 * 3.14159) * 16000);
        originalAudio[i * 2] = left;
        originalAudio[i * 2 + 1] = right;
    }

    // Generate minimal video data
    std::vector<uint16_t> videoData(TEST_WIDTH * TEST_HEIGHT, 1000);

    // Write file with audio
    {
        vraw::VrawWriter writer;
        uint16_t blackLevel[4] = {64, 64, 64, 64};

        if (!writer.init(TEST_WIDTH, TEST_HEIGHT, testFile,
                         vraw::Encoding::LINEAR_12BIT, false, false,
                         vraw::BayerPattern::RGGB, blackLevel, 4095)) {
            printf("FAIL (init)\n");
            return false;
        }

        if (!writer.enableAudio(sampleRate, channels)) {
            printf("FAIL (enableAudio)\n");
            return false;
        }

        if (!writer.start()) {
            printf("FAIL (start)\n");
            return false;
        }

        // Write one frame
        if (!writer.submitFrame(videoData.data(), 0, 1.0f, 1.0f, 1.0f)) {
            printf("FAIL (submitFrame)\n");
            return false;
        }

        // Write audio
        if (!writer.submitAudio(originalAudio.data(), sampleCount, 0)) {
            printf("FAIL (submitAudio)\n");
            return false;
        }

        if (!writer.stop()) {
            printf("FAIL (stop)\n");
            return false;
        }
    }

    // Read and verify audio
    {
        vraw::VrawReader reader;

        if (!reader.open(testFile)) {
            printf("FAIL (open)\n");
            std::remove(testFile.c_str());
            return false;
        }

        if (!reader.hasAudio()) {
            printf("FAIL (no audio flag)\n");
            reader.close();
            std::remove(testFile.c_str());
            return false;
        }

        vraw::AudioHeader audioHeader;
        std::vector<int16_t> readAudio;

        if (!reader.readAudio(audioHeader, readAudio)) {
            printf("FAIL (readAudio)\n");
            reader.close();
            std::remove(testFile.c_str());
            return false;
        }

        if (audioHeader.sampleRate != sampleRate ||
            audioHeader.channels != channels ||
            audioHeader.sampleCount != sampleCount) {
            printf("FAIL (audio header mismatch)\n");
            reader.close();
            std::remove(testFile.c_str());
            return false;
        }

        if (readAudio.size() != originalAudio.size()) {
            printf("FAIL (audio size %zu, expected %zu)\n", readAudio.size(), originalAudio.size());
            reader.close();
            std::remove(testFile.c_str());
            return false;
        }

        // Compare audio samples
        for (size_t i = 0; i < originalAudio.size(); i++) {
            if (readAudio[i] != originalAudio[i]) {
                printf("FAIL (audio sample %zu: %d vs %d)\n", i, readAudio[i], originalAudio[i]);
                reader.close();
                std::remove(testFile.c_str());
                return false;
            }
        }

        reader.close();
    }

    std::remove(testFile.c_str());
    printf("PASS\n");
    return true;
}

int main() {
    printf("\nVRAW Library Test Suite\n");
    printf("=======================\n\n");
    printf("Testing %d encoding/compression/packing combinations:\n\n", NUM_TESTS);

    int passed = 0;
    int failed = 0;

    for (int i = 0; i < NUM_TESTS; i++) {
        if (runTest(testConfigs[i], i + 1)) {
            passed++;
        } else {
            failed++;
        }
    }

    printf("\n");

    // Run audio test
    if (runAudioTest()) {
        passed++;
    } else {
        failed++;
    }

    printf("\n");
    printf("Results: %d passed, %d failed\n", passed, failed);
    printf("\n");

    return failed > 0 ? 1 : 0;
}
