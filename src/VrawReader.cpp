/**
 * VRAW Library - Video Reader Implementation
 */

#include "VrawReader.h"
#include "lz4.h"
#include <cstring>
#include <algorithm>
#include <cstdio>

#ifdef __ANDROID__
#include <android/log.h>
#define LOG_TAG "VrawReader"
#define LOGI(...) __android_log_print(ANDROID_LOG_INFO, LOG_TAG, __VA_ARGS__)
#define LOGE(...) __android_log_print(ANDROID_LOG_ERROR, LOG_TAG, __VA_ARGS__)
#else
#define LOGI(...) do { fprintf(stderr, "[VRAW INFO] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#define LOGE(...) do { fprintf(stderr, "[VRAW ERROR] "); fprintf(stderr, __VA_ARGS__); fprintf(stderr, "\n"); } while(0)
#endif

// 64-bit file operations for large file support (>2GB)
// On Windows, long is 32-bit, so we need _fseeki64/_ftelli64
#ifdef _WIN32
#include <io.h>
inline int fseek64(FILE* stream, int64_t offset, int origin) {
    return _fseeki64(stream, offset, origin);
}
inline int64_t ftell64(FILE* stream) {
    return _ftelli64(stream);
}
#else
// On Unix/Linux/macOS, use standard functions (off_t is 64-bit with _FILE_OFFSET_BITS=64)
inline int fseek64(FILE* stream, int64_t offset, int origin) {
    return fseeko(stream, static_cast<off_t>(offset), origin);
}
inline int64_t ftell64(FILE* stream) {
    return static_cast<int64_t>(ftello(stream));
}
#endif

namespace vraw {

// Forward declarations for unpacking functions
static void unpackFrame10Bit(const uint8_t* src, uint32_t srcBytes, std::vector<uint8_t>& dst, uint32_t pixelCount);
static void unpackFrame12Bit(const uint8_t* src, uint32_t srcBytes, std::vector<uint8_t>& dst, uint32_t pixelCount);

// Internal file structures
#pragma pack(push, 1)

struct SimpleFrameHeader {
    uint64_t timestamp_us;
    uint32_t frame_number;
    uint32_t compressed_size;
    uint32_t uncompressed_size;
    float iso;
    float exposure_time_ms;
    float white_balance_r;
    float white_balance_g;
    float white_balance_b;
    float focal_length;
    float aperture;
    float focus_distance;
    uint16_t dynamic_black_level[4];
    uint8_t reserved[4];
};

struct SimpleFileHeader {
    char magic[4];
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint8_t bayer_pattern;
    uint8_t encoding;
    uint8_t compression;
    uint8_t reserved1;
    uint16_t black_level[4];
    uint16_t white_level;
    uint16_t reserved2;
    uint32_t frame_count;
    uint64_t index_offset;

    uint32_t native_width;
    uint32_t native_height;
    uint32_t binning_num;
    uint32_t binning_den;

    uint8_t has_audio;
    uint8_t audio_channels;
    uint8_t audio_bit_depth;
    uint8_t reserved3;
    uint32_t audio_sample_rate;
    uint64_t audio_offset;
    uint64_t audio_start_time_us;

    uint8_t has_timecode;
    uint8_t timecode_format;
    uint8_t timecode_fps;
    uint8_t timecode_drop_frame;
    uint32_t timecode_start_frame;
    uint8_t timecode_hours;
    uint8_t timecode_minutes;
    uint8_t timecode_seconds;
    uint8_t timecode_frames;
    uint8_t reserved_tc[4];

    int32_t sensor_orientation;
    uint8_t reserved[408];
};

struct AudioStreamHeaderRaw {
    char magic[4];
    uint32_t version;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bit_depth;
    uint64_t sample_count;
    uint64_t start_timestamp_us;
    uint8_t reserved[32];
};

#pragma pack(pop)

static const int FILE_HEADER_SIZE = 512;
static const int FRAME_HEADER_SIZE = 64;

VrawReader::VrawReader()
    : file_(nullptr),
      isPacked_(false),
      usingFd_(false),
      fd_(-1) {
    memset(&fileHeader_, 0, sizeof(fileHeader_));
}

VrawReader::~VrawReader() {
    close();
}

bool VrawReader::open(const std::string& path) {
    if (file_) {
        close();
    }

    file_ = fopen(path.c_str(), "rb");
    if (!file_) {
        LOGE("Failed to open file: %s", path.c_str());
        return false;
    }

    filePath_ = path;
    usingFd_ = false;
    fd_ = -1;

    if (!readFileHeader()) {
        LOGE("Failed to read file header: %s", path.c_str());
        close();
        return false;
    }

    if (!readIndexTable()) {
        // Try building sequential index
        if (!buildSequentialIndex()) {
            LOGE("Failed to build frame index: %s", path.c_str());
            close();
            return false;
        }
    }

    if (!validateIndex()) {
        if (!buildSequentialIndex()) {
            LOGE("Failed to validate frame index: %s", path.c_str());
            close();
            return false;
        }
    }

    LOGI("Opened: %s (%ux%u, %u frames)", path.c_str(),
         fileHeader_.width, fileHeader_.height, fileHeader_.frameCount);
    return true;
}

bool VrawReader::openWithFd(int fd, const std::string& displayPath) {
    if (file_) {
        close();
    }

    if (fd < 0) {
        LOGE("Invalid file descriptor: %d", fd);
        return false;
    }

    file_ = fdopen(fd, "rb");
    if (!file_) {
        LOGE("Failed to fdopen descriptor: %d", fd);
        return false;
    }

    filePath_ = displayPath;
    usingFd_ = true;
    fd_ = fd;

    if (!readFileHeader()) {
        LOGE("Failed to read file header: %s", displayPath.c_str());
        close();
        return false;
    }

    if (!readIndexTable()) {
        // Try building sequential index
        if (!buildSequentialIndex()) {
            LOGE("Failed to build frame index: %s", displayPath.c_str());
            close();
            return false;
        }
    }

    if (!validateIndex()) {
        if (!buildSequentialIndex()) {
            LOGE("Failed to validate frame index: %s", displayPath.c_str());
            close();
            return false;
        }
    }

    LOGI("Opened fd=%d: %s (%ux%u, %u frames)", fd, displayPath.c_str(),
         fileHeader_.width, fileHeader_.height, fileHeader_.frameCount);
    return true;
}

void VrawReader::close() {
    if (file_) {
        fclose(file_);
        file_ = nullptr;
    }
    frameIndex_.clear();
    filePath_.clear();
    usingFd_ = false;
    fd_ = -1;
}

bool VrawReader::readFileHeader() {
    SimpleFileHeader raw;

    fseek64(file_, 0, SEEK_SET);
    if (fread(&raw, sizeof(raw), 1, file_) != 1) {
        return false;
    }

    // Accept both VRAW (new) and MRAW (legacy) magic
    if (memcmp(raw.magic, "VRAW", 4) != 0 && memcmp(raw.magic, "MRAW", 4) != 0) {
        return false;
    }

    fileHeader_.version = raw.version;
    fileHeader_.width = raw.width;
    fileHeader_.height = raw.height;
    fileHeader_.bayerPattern = static_cast<BayerPattern>(raw.bayer_pattern);
    fileHeader_.encoding = static_cast<Encoding>(raw.encoding);
    fileHeader_.compression = static_cast<Compression>(raw.compression);

    for (int i = 0; i < 4; i++) {
        fileHeader_.blackLevel[i] = raw.black_level[i];
    }
    fileHeader_.whiteLevel = raw.white_level;
    fileHeader_.frameCount = raw.frame_count;
    fileHeader_.indexOffset = raw.index_offset;

    if (raw.version >= 2) {
        fileHeader_.nativeWidth = raw.native_width;
        fileHeader_.nativeHeight = raw.native_height;
        fileHeader_.binningNum = raw.binning_num > 0 ? raw.binning_num : 1;
        fileHeader_.binningDen = raw.binning_den > 0 ? raw.binning_den : 1;

        fileHeader_.hasAudio = raw.has_audio != 0;
        fileHeader_.audioChannels = raw.audio_channels;
        fileHeader_.audioBitDepth = raw.audio_bit_depth;
        fileHeader_.audioSampleRate = raw.audio_sample_rate;
        fileHeader_.audioOffset = raw.audio_offset;
        fileHeader_.audioStartTimeUs = raw.audio_start_time_us;

        fileHeader_.hasTimecode = raw.has_timecode != 0;
        if (fileHeader_.hasTimecode) {
            fileHeader_.timecode.hours = raw.timecode_hours;
            fileHeader_.timecode.minutes = raw.timecode_minutes;
            fileHeader_.timecode.seconds = raw.timecode_seconds;
            fileHeader_.timecode.frames = raw.timecode_frames;
            fileHeader_.timecode.fps = raw.timecode_fps;
            fileHeader_.timecode.dropFrame = raw.timecode_drop_frame != 0;
            fileHeader_.timecode.format = raw.timecode_format;
        }

        fileHeader_.sensorOrientation = raw.sensor_orientation;
    } else {
        fileHeader_.nativeWidth = raw.width;
        fileHeader_.nativeHeight = raw.height;
        fileHeader_.binningNum = 1;
        fileHeader_.binningDen = 1;
        fileHeader_.hasAudio = false;
        fileHeader_.hasTimecode = false;
        fileHeader_.sensorOrientation = 0;
    }

    return true;
}

bool VrawReader::readIndexTable() {
    if (fileHeader_.indexOffset == 0 || fileHeader_.frameCount == 0) {
        return false;
    }

    // Use 64-bit seek for large file support (files >2GB)
    fseek64(file_, static_cast<int64_t>(fileHeader_.indexOffset), SEEK_SET);

    frameIndex_.resize(fileHeader_.frameCount);
    for (uint32_t i = 0; i < fileHeader_.frameCount; i++) {
        if (fread(&frameIndex_[i], sizeof(uint64_t), 1, file_) != 1) {
            frameIndex_.clear();
            return false;
        }
    }

    return true;
}

bool VrawReader::buildSequentialIndex() {
    frameIndex_.clear();

    // Use 64-bit file operations for large file support (>2GB)
    fseek64(file_, 0, SEEK_END);
    int64_t fileLen = ftell64(file_);

    int64_t pos = FILE_HEADER_SIZE;
    uint32_t count = 0;

    while (pos + FRAME_HEADER_SIZE <= fileLen && count < fileHeader_.frameCount) {
        // Read frame header BEFORE adding to index to validate completeness
        fseek64(file_, pos, SEEK_SET);
        SimpleFrameHeader fh;
        if (fread(&fh, sizeof(fh), 1, file_) != 1) {
            break;
        }

        uint32_t dataSize = (fh.compressed_size > 0) ? fh.compressed_size : fh.uncompressed_size;
        if (dataSize <= 0) {
            break;  // Invalid or partial frame - stop here
        }

        // Verify complete frame data exists in file before adding to index
        if (pos + FRAME_HEADER_SIZE + dataSize > fileLen) {
            break;  // Partial frame data - don't include this frame
        }

        // Frame is complete - add to index
        frameIndex_.push_back(static_cast<uint64_t>(pos));

        pos += FRAME_HEADER_SIZE + dataSize;
        count++;
    }

    return !frameIndex_.empty();
}

bool VrawReader::validateIndex() {
    if (frameIndex_.empty()) {
        return false;
    }

    // Use 64-bit file operations for large file support (>2GB)
    fseek64(file_, 0, SEEK_END);
    int64_t fileLen = ftell64(file_);

    for (uint64_t offset : frameIndex_) {
        if (offset < FILE_HEADER_SIZE || offset >= static_cast<uint64_t>(fileLen)) {
            return false;
        }
    }

    return true;
}

VrawReader::Frame VrawReader::readFrame(uint32_t frameNumber) {
    Frame result;
    result.valid = false;

    if (!file_ || frameNumber >= frameIndex_.size()) {
        return result;
    }

    uint64_t frameOffset = frameIndex_[frameNumber];
    // Use 64-bit seek for large file support (>2GB)
    fseek64(file_, static_cast<int64_t>(frameOffset), SEEK_SET);

    SimpleFrameHeader fh;
    if (fread(&fh, sizeof(fh), 1, file_) != 1) {
        return result;
    }

    result.header.timestampUs = fh.timestamp_us;
    result.header.frameNumber = fh.frame_number;
    result.header.compressedSize = fh.compressed_size;
    result.header.uncompressedSize = fh.uncompressed_size;
    result.header.iso = fh.iso;
    result.header.exposureTimeMs = fh.exposure_time_ms;
    result.header.whiteBalanceR = fh.white_balance_r;
    result.header.whiteBalanceG = fh.white_balance_g;
    result.header.whiteBalanceB = fh.white_balance_b;
    result.header.focalLength = fh.focal_length;
    result.header.aperture = fh.aperture;
    result.header.focusDistance = fh.focus_distance;
    for (int i = 0; i < 4; i++) {
        result.header.dynamicBlackLevel[i] = fh.dynamic_black_level[i];
    }

    // Determine data size and format
    uint32_t pixelCount = fileHeader_.width * fileHeader_.height;
    uint32_t fullFrameSize = pixelCount * 2;  // 16-bit samples

    bool isCompressed = (fh.compressed_size > 0 && fileHeader_.compression != Compression::NONE);
    uint32_t dataSize = (fh.compressed_size > 0) ? fh.compressed_size : fh.uncompressed_size;

    // Detect packing: if uncompressed size is less than full frame size, data is packed
    bool isPacked = (fh.uncompressed_size > 0 && fh.uncompressed_size < fullFrameSize);

    if (dataSize == 0) {
        return result;
    }

    // Read pixel data
    std::vector<uint8_t> rawData(dataSize);
    if (fread(rawData.data(), 1, dataSize, file_) != dataSize) {
        return result;
    }

    // Decompress if needed
    std::vector<uint8_t> decompressedData;
    const uint8_t* frameData = rawData.data();
    uint32_t frameDataSize = dataSize;

    if (isCompressed && fh.uncompressed_size > 0) {
        decompressedData.resize(fh.uncompressed_size);
        int decompressed = LZ4_decompress_safe(
            reinterpret_cast<const char*>(rawData.data()),
            reinterpret_cast<char*>(decompressedData.data()),
            dataSize,
            fh.uncompressed_size
        );
        if (decompressed < 0) {
            return result;
        }
        frameData = decompressedData.data();
        frameDataSize = fh.uncompressed_size;
    }

    isPacked_ = isPacked;

    // Unpack bit-packed data to 16-bit samples
    bool is12Bit = (fileHeader_.encoding == Encoding::LOG2_12BIT ||
                    fileHeader_.encoding == Encoding::LINEAR_12BIT);

    if (isPacked) {
        if (is12Bit) {
            unpackFrame12Bit(frameData, frameDataSize, result.pixelData, pixelCount);
        } else {
            unpackFrame10Bit(frameData, frameDataSize, result.pixelData, pixelCount);
        }
    } else if (isCompressed) {
        result.pixelData = std::move(decompressedData);
    } else {
        result.pixelData = std::move(rawData);
    }

    result.valid = true;
    return result;
}

bool VrawReader::readFrameHeader(uint32_t frameNumber, FrameHeader& header) {
    if (!file_ || frameNumber >= frameIndex_.size()) {
        return false;
    }

    uint64_t frameOffset = frameIndex_[frameNumber];
    // Use 64-bit seek for large file support (>2GB)
    if (fseek64(file_, static_cast<int64_t>(frameOffset), SEEK_SET) != 0) {
        return false;
    }

    // Read only the 64-byte frame header (no pixel data)
    SimpleFrameHeader fh;
    if (fread(&fh, sizeof(fh), 1, file_) != 1) {
        return false;
    }

    // Copy to public header structure
    header.timestampUs = fh.timestamp_us;
    header.frameNumber = fh.frame_number;
    header.compressedSize = fh.compressed_size;
    header.uncompressedSize = fh.uncompressed_size;
    header.iso = fh.iso;
    header.exposureTimeMs = fh.exposure_time_ms;
    header.whiteBalanceR = fh.white_balance_r;
    header.whiteBalanceG = fh.white_balance_g;
    header.whiteBalanceB = fh.white_balance_b;
    header.focalLength = fh.focal_length;
    header.aperture = fh.aperture;
    header.focusDistance = fh.focus_distance;
    for (int i = 0; i < 4; i++) {
        header.dynamicBlackLevel[i] = fh.dynamic_black_level[i];
    }

    return true;
}

bool VrawReader::readAudio(AudioHeader& header, std::vector<int16_t>& samples) {
    if (!file_ || !fileHeader_.hasAudio || fileHeader_.audioOffset == 0) {
        return false;
    }

    // Use 64-bit seek for large file support (>2GB)
    fseek64(file_, static_cast<int64_t>(fileHeader_.audioOffset), SEEK_SET);

    AudioStreamHeaderRaw ash;
    if (fread(&ash, sizeof(ash), 1, file_) != 1) {
        return false;
    }

    if (memcmp(ash.magic, "MAUD", 4) != 0) {
        return false;
    }

    header.sampleRate = ash.sample_rate;
    header.channels = ash.channels;
    header.bitDepth = ash.bit_depth;
    header.sampleCount = ash.sample_count;
    header.startTimestampUs = ash.start_timestamp_us;

    uint64_t totalSamples = ash.sample_count * ash.channels;
    samples.resize(totalSamples);

    if (fread(samples.data(), sizeof(int16_t), totalSamples, file_) != totalSamples) {
        samples.clear();
        return false;
    }

    return true;
}

// Unpack 10-bit packed data to 16-bit samples
static void unpackFrame10Bit(const uint8_t* src, uint32_t srcBytes, std::vector<uint8_t>& dst, uint32_t pixelCount) {
    dst.resize(pixelCount * 2);
    uint16_t* dstPtr = reinterpret_cast<uint16_t*>(dst.data());

    uint32_t bitBuffer = 0;
    int bitCount = 0;
    uint32_t srcIdx = 0;
    uint32_t pixelIdx = 0;

    while (pixelIdx < pixelCount && srcIdx < srcBytes) {
        while (bitCount < 10 && srcIdx < srcBytes) {
            bitBuffer |= static_cast<uint32_t>(src[srcIdx++]) << bitCount;
            bitCount += 8;
        }
        if (bitCount >= 10) {
            dstPtr[pixelIdx++] = bitBuffer & 0x3FF;
            bitBuffer >>= 10;
            bitCount -= 10;
        }
    }
}

// Unpack 12-bit packed data to 16-bit samples
static void unpackFrame12Bit(const uint8_t* src, uint32_t srcBytes, std::vector<uint8_t>& dst, uint32_t pixelCount) {
    dst.resize(pixelCount * 2);
    uint16_t* dstPtr = reinterpret_cast<uint16_t*>(dst.data());

    uint32_t srcIdx = 0;
    uint32_t pixelIdx = 0;

    while (pixelIdx < pixelCount && srcIdx + 2 < srcBytes) {
        uint8_t b0 = src[srcIdx++];
        uint8_t b1 = src[srcIdx++];
        uint8_t b2 = src[srcIdx++];

        dstPtr[pixelIdx++] = (static_cast<uint16_t>(b0) << 4) | ((b1 >> 4) & 0x0F);
        if (pixelIdx < pixelCount) {
            dstPtr[pixelIdx++] = (static_cast<uint16_t>(b1 & 0x0F) << 8) | b2;
        }
    }

    // Handle remaining byte pair if odd pixel count
    if (pixelIdx < pixelCount && srcIdx + 1 < srcBytes) {
        uint8_t b0 = src[srcIdx++];
        uint8_t b1 = src[srcIdx++];
        dstPtr[pixelIdx++] = (static_cast<uint16_t>(b0) << 4) | ((b1 >> 4) & 0x0F);
    }
}

} // namespace vraw
