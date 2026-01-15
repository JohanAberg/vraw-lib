/**
 * VRAW Library - Video Writer
 *
 * Standalone C++ library for writing VRAW video files.
 * https://github.com/JohanAberg/vertigo
 */

#ifndef VRAW_WRITER_H
#define VRAW_WRITER_H

#include "VrawTypes.h"
#include <string>
#include <vector>
#include <cstdio>
#include <memory>

namespace vraw {

/**
 * VrawWriter - Write RAW video frames to VRAW format files.
 *
 * Example usage:
 *   VrawWriter writer;
 *   writer.init(1920, 1080, "output.vraw");
 *   writer.start();
 *   writer.submitFrame(frameData, timestampUs);
 *   writer.stop();
 */
class VrawWriter {
public:
    VrawWriter();
    ~VrawWriter();

    // Disable copy
    VrawWriter(const VrawWriter&) = delete;
    VrawWriter& operator=(const VrawWriter&) = delete;

    /**
     * Initialize the writer with resolution and output path.
     *
     * @param width Frame width in pixels
     * @param height Frame height in pixels
     * @param outputPath Path to output .vraw file
     * @param encoding Pixel encoding type (default: LINEAR_12BIT)
     * @param usePacking Use bit-packing to reduce file size
     * @param useCompression Use LZ4 compression
     * @param bayerPattern Sensor Bayer pattern
     * @param blackLevel Per-channel black levels (array of 4, or nullptr for default)
     * @param whiteLevel Sensor white level
     * @param sensorOrientation Orientation in degrees (0, 90, 180, 270)
     * @param nativeWidth Full sensor width (0 = same as width)
     * @param nativeHeight Full sensor height (0 = same as height)
     * @return true on success
     */
    bool init(uint32_t width, uint32_t height, const std::string& outputPath,
              Encoding encoding = Encoding::LINEAR_12BIT,
              bool usePacking = false,
              bool useCompression = true,
              BayerPattern bayerPattern = BayerPattern::RGGB,
              const uint16_t* blackLevel = nullptr,
              uint16_t whiteLevel = 4095,
              int32_t sensorOrientation = 0,
              uint32_t nativeWidth = 0,
              uint32_t nativeHeight = 0);

    /**
     * Start recording frames.
     */
    bool start();

    /**
     * Submit a frame for writing.
     *
     * @param data Pointer to pixel data (16-bit per pixel)
     * @param timestampUs Frame timestamp in microseconds
     * @param whiteBalanceR Red white balance multiplier
     * @param whiteBalanceG Green white balance multiplier
     * @param whiteBalanceB Blue white balance multiplier
     * @param dynamicBlackLevel Per-frame black level (or nullptr)
     * @return true on success
     */
    bool submitFrame(const uint16_t* data,
                     uint64_t timestampUs,
                     float whiteBalanceR = 1.0f,
                     float whiteBalanceG = 1.0f,
                     float whiteBalanceB = 1.0f,
                     const uint16_t* dynamicBlackLevel = nullptr);

    /**
     * Stop recording and finalize the file.
     */
    bool stop();

    /**
     * Check if currently recording.
     */
    bool isRecording() const { return isRecording_; }

    /**
     * Get number of frames written.
     */
    uint32_t getFrameCount() const { return frameNumber_; }

    /**
     * Get total bytes written.
     */
    uint64_t getBytesWritten() const { return bytesWritten_; }

    /**
     * Flush buffered data to disk.
     */
    bool flush();

    /**
     * Enable audio recording.
     *
     * @param sampleRate Audio sample rate (e.g., 48000)
     * @param channels Number of channels (1=mono, 2=stereo)
     */
    bool enableAudio(uint32_t sampleRate = 48000, uint16_t channels = 2);

    /**
     * Submit audio samples.
     *
     * @param samples PCM16 audio samples (interleaved for stereo)
     * @param sampleCount Number of samples PER CHANNEL
     * @param timestampUs Timestamp of first sample
     */
    bool submitAudio(const int16_t* samples, uint32_t sampleCount, uint64_t timestampUs);

    /**
     * Get total audio samples captured.
     */
    uint64_t getAudioSampleCount() const;

private:
    bool ensurePackedCapacity(uint32_t packedBytes);
    bool ensureEncodedCapacity(uint32_t pixelCount);
    bool ensureCompressedCapacity(uint32_t uncompressedSize);
    uint32_t packFrame10Bit(const uint16_t* src, uint32_t pixelCount);
    uint32_t packFrame12Bit(const uint16_t* src, uint32_t pixelCount);

    FILE* outputFile_;
    bool isRecording_;
    uint32_t width_;
    uint32_t height_;
    uint32_t nativeWidth_;
    uint32_t nativeHeight_;
    std::string outputPath_;
    Encoding encoding_;
    Compression compression_;
    bool writePacked_;
    bool useCompression_;
    uint32_t binningNum_;
    uint32_t binningDen_;
    uint32_t frameNumber_;
    uint64_t bytesWritten_;
    std::vector<uint64_t> frameOffsets_;
    std::vector<uint8_t> packedBuffer_;
    std::vector<uint16_t> encodedBuffer_;
    std::vector<uint8_t> compressedBuffer_;

    uint16_t blackLevel_[4];
    uint16_t whiteLevel_;
    int32_t sensorOrientation_;

    // Audio state
    bool audioEnabled_;
    uint32_t audioSampleRate_;
    uint16_t audioChannels_;
    uint64_t audioStartTime_;
    std::vector<int16_t> audioBuffer_;
};

} // namespace vraw

#endif // VRAW_WRITER_H
