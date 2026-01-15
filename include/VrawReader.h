/**
 * VRAW Library - Video Reader
 *
 * Standalone C++ library for reading VRAW/MRAW video files.
 * https://github.com/JohanAberg/vraw-lib
 */

#ifndef VRAW_READER_H
#define VRAW_READER_H

#include "VrawTypes.h"
#include <string>
#include <vector>
#include <cstdio>
#include <memory>

namespace vraw {

/**
 * VrawReader - Read RAW video frames from VRAW format files.
 *
 * Example usage:
 *   VrawReader reader;
 *   if (reader.open("input.vraw")) {
 *       auto header = reader.getFileHeader();
 *       for (uint32_t i = 0; i < header.frameCount; i++) {
 *           auto frame = reader.readFrame(i);
 *           // process frame.pixelData
 *       }
 *   }
 *   reader.close();
 */
class VrawReader {
public:
    // Frame data returned by readFrame()
    struct Frame {
        FrameHeader header;
        std::vector<uint8_t> pixelData;
        bool valid = false;
    };

    VrawReader();
    ~VrawReader();

    // Disable copy
    VrawReader(const VrawReader&) = delete;
    VrawReader& operator=(const VrawReader&) = delete;

    /**
     * Open an VRAW file for reading.
     *
     * @param path Path to .vraw file
     * @return true on success
     */
    bool open(const std::string& path);

    /**
     * Open from a file descriptor (for Android SAF/USB storage).
     *
     * @param fd File descriptor to read from
     * @param displayPath Path for logging purposes only
     * @return true on success
     */
    bool openWithFd(int fd, const std::string& displayPath);

    /**
     * Close the file.
     */
    void close();

    /**
     * Check if file is open.
     */
    bool isOpen() const { return file_ != nullptr; }

    /**
     * Get file header information.
     */
    const FileHeader& getFileHeader() const { return fileHeader_; }

    /**
     * Get frame count.
     */
    uint32_t getFrameCount() const { return static_cast<uint32_t>(frameIndex_.size()); }

    /**
     * Get frame dimensions.
     */
    uint32_t getWidth() const { return fileHeader_.width; }
    uint32_t getHeight() const { return fileHeader_.height; }
    uint32_t getNativeWidth() const { return fileHeader_.nativeWidth; }
    uint32_t getNativeHeight() const { return fileHeader_.nativeHeight; }

    /**
     * Read a frame by index.
     *
     * @param frameNumber Frame index (0-based)
     * @return Frame data (check .valid flag)
     */
    Frame readFrame(uint32_t frameNumber);

    /**
     * Read audio data if present.
     *
     * @param header Output audio header
     * @param samples Output audio samples
     * @return true if audio was read successfully
     */
    bool readAudio(AudioHeader& header, std::vector<int16_t>& samples);

    /**
     * Check if file has audio.
     */
    bool hasAudio() const { return fileHeader_.hasAudio; }

    /**
     * Get sensor orientation in degrees.
     */
    int32_t getSensorOrientation() const { return fileHeader_.sensorOrientation; }

    /**
     * Check if data is bit-packed.
     */
    bool isPacked() const { return isPacked_; }

private:
    bool readFileHeader();
    bool readIndexTable();
    bool buildSequentialIndex();
    bool validateIndex();

    FILE* file_;
    std::string filePath_;
    FileHeader fileHeader_;
    std::vector<uint64_t> frameIndex_;
    bool isPacked_;
    bool usingFd_;
    int fd_;
};

} // namespace vraw

#endif // VRAW_READER_H
