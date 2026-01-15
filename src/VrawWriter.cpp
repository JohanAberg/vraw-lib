/**
 * VRAW Library - Video Writer Implementation
 */

#include "VrawWriter.h"
#include "Encoding.h"
#include "lz4.h"
#include <cstring>
#include <ctime>
#include <algorithm>
#include <cstdio>

namespace vraw {

// Internal file structures (must match format spec)
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
    char magic[4];              // "VRAW"
    uint32_t version;           // 2
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

    // V2 fields
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

struct AudioStreamHeader {
    char magic[4];              // "MAUD"
    uint32_t version;
    uint32_t sample_rate;
    uint16_t channels;
    uint16_t bit_depth;
    uint64_t sample_count;
    uint64_t start_timestamp_us;
    uint8_t reserved[32];
};

#pragma pack(pop)

VrawWriter::VrawWriter()
    : outputFile_(nullptr),
      isRecording_(false),
      width_(0),
      height_(0),
      nativeWidth_(0),
      nativeHeight_(0),
      encoding_(Encoding::LINEAR_12BIT),
      compression_(Compression::NONE),
      writePacked_(false),
      useCompression_(false),
      binningNum_(1),
      binningDen_(1),
      frameNumber_(0),
      bytesWritten_(0),
      blackLevel_{64, 64, 64, 64},
      whiteLevel_(4095),
      sensorOrientation_(0),
      audioEnabled_(false),
      audioSampleRate_(48000),
      audioChannels_(2),
      audioStartTime_(0) {
}

VrawWriter::~VrawWriter() {
    if (isRecording_) {
        stop();
    }
    if (outputFile_) {
        fclose(outputFile_);
        outputFile_ = nullptr;
    }
}

bool VrawWriter::init(uint32_t width, uint32_t height, const std::string& outputPath,
                      Encoding encoding, bool usePacking, bool useCompression,
                      BayerPattern bayerPattern,
                      const uint16_t* blackLevel, uint16_t whiteLevel,
                      int32_t sensorOrientation,
                      uint32_t nativeWidth, uint32_t nativeHeight) {
    if (outputFile_) {
        return false;
    }
    if (width == 0 || height == 0 || outputPath.empty()) {
        return false;
    }

    width_ = width;
    height_ = height;
    outputPath_ = outputPath;
    encoding_ = encoding;

    if (blackLevel != nullptr) {
        for (int i = 0; i < 4; ++i) {
            blackLevel_[i] = blackLevel[i];
        }
    } else {
        blackLevel_[0] = blackLevel_[1] = blackLevel_[2] = blackLevel_[3] = 64;
    }
    whiteLevel_ = whiteLevel;
    sensorOrientation_ = sensorOrientation;

    nativeWidth_ = (nativeWidth > 0) ? nativeWidth : width;
    nativeHeight_ = (nativeHeight > 0) ? nativeHeight : height;

    if (nativeWidth > width && nativeHeight > height) {
        binningNum_ = 1;
        binningDen_ = nativeWidth / width;
    } else {
        binningNum_ = 1;
        binningDen_ = 1;
    }

    writePacked_ = usePacking;
    useCompression_ = useCompression;
    compression_ = useCompression ? Compression::LZ4_FAST : Compression::NONE;

    frameNumber_ = 0;
    bytesWritten_ = 0;

    outputFile_ = fopen(outputPath.c_str(), "wb");
    if (!outputFile_) {
        return false;
    }

    // Write file header
    SimpleFileHeader fh;
    memset(&fh, 0, sizeof(fh));
    memcpy(fh.magic, "VRAW", 4);
    fh.version = 2;
    fh.width = width;
    fh.height = height;
    fh.bayer_pattern = static_cast<uint8_t>(bayerPattern);
    fh.encoding = static_cast<uint8_t>(encoding_);
    fh.compression = static_cast<uint8_t>(compression_);
    fh.black_level[0] = blackLevel_[0];
    fh.black_level[1] = blackLevel_[1];
    fh.black_level[2] = blackLevel_[2];
    fh.black_level[3] = blackLevel_[3];
    fh.white_level = whiteLevel_;
    fh.frame_count = 0;
    fh.index_offset = 0;

    fh.native_width = nativeWidth_;
    fh.native_height = nativeHeight_;
    fh.binning_num = binningNum_;
    fh.binning_den = binningDen_;

    fh.has_audio = 0;
    fh.audio_channels = 2;
    fh.audio_bit_depth = 16;
    fh.audio_sample_rate = 48000;
    fh.audio_offset = 0;
    fh.audio_start_time_us = 0;

    fh.has_timecode = 1;
    fh.timecode_format = 0;
    fh.timecode_fps = 24;
    fh.timecode_drop_frame = 0;
    fh.timecode_start_frame = 0;

    time_t now = time(nullptr);
    struct tm* local = localtime(&now);
    fh.timecode_hours = local->tm_hour;
    fh.timecode_minutes = local->tm_min;
    fh.timecode_seconds = local->tm_sec;
    fh.timecode_frames = 0;

    fh.sensor_orientation = sensorOrientation_;

    if (fwrite(&fh, sizeof(SimpleFileHeader), 1, outputFile_) != 1) {
        fclose(outputFile_);
        outputFile_ = nullptr;
        return false;
    }
    bytesWritten_ = sizeof(SimpleFileHeader);

    return true;
}

bool VrawWriter::start() {
    if (!outputFile_ || isRecording_) {
        return false;
    }
    isRecording_ = true;
    frameNumber_ = 0;
    frameOffsets_.clear();
    return true;
}

bool VrawWriter::submitFrame(const uint16_t* data,
                             uint64_t timestampUs,
                             float whiteBalanceR,
                             float whiteBalanceG,
                             float whiteBalanceB,
                             const uint16_t* dynamicBlackLevel) {
    if (!isRecording_ || !outputFile_ || !data) {
        return false;
    }

    const uint32_t pixelCount = width_ * height_;

    // Apply log encoding if required
    const uint16_t* dataToWrite = data;
    if (encoding_ == Encoding::LOG2_10BIT || encoding_ == Encoding::LOG2_12BIT) {
        ensureEncodedCapacity(pixelCount);
        uint16_t avgBlackLevel = (blackLevel_[0] + blackLevel_[1] + blackLevel_[2] + blackLevel_[3]) / 4;

        if (encoding_ == Encoding::LOG2_12BIT) {
            encodeLog12Bit(data, encodedBuffer_.data(), pixelCount, avgBlackLevel, whiteLevel_);
        } else {
            encodeLog10Bit(data, encodedBuffer_.data(), pixelCount, avgBlackLevel, whiteLevel_);
        }
        dataToWrite = encodedBuffer_.data();
    }

    uint64_t frame_offset = bytesWritten_;
    frameOffsets_.push_back(frame_offset);

    SimpleFrameHeader fh = {};
    fh.timestamp_us = timestampUs;
    fh.frame_number = frameNumber_++;
    fh.uncompressed_size = pixelCount * 2;
    uint32_t payloadBytes = fh.uncompressed_size;
    const uint8_t* dataToWriteBytes = reinterpret_cast<const uint8_t*>(dataToWrite);

    // Bit-packing
    if (writePacked_) {
        if (encoding_ == Encoding::LOG2_12BIT || encoding_ == Encoding::LINEAR_12BIT) {
            payloadBytes = packFrame12Bit(dataToWrite, pixelCount);
        } else {
            payloadBytes = packFrame10Bit(dataToWrite, pixelCount);
        }
        dataToWriteBytes = packedBuffer_.data();
        fh.uncompressed_size = payloadBytes;
    }

    // LZ4 compression
    if (useCompression_) {
        ensureCompressedCapacity(payloadBytes);
        fh.uncompressed_size = payloadBytes;

        int maxCompressedSize = LZ4_compressBound(payloadBytes);
        int compressedSize = LZ4_compress_default(
            reinterpret_cast<const char*>(dataToWriteBytes),
            reinterpret_cast<char*>(compressedBuffer_.data()),
            payloadBytes,
            maxCompressedSize
        );

        if (compressedSize > 0 && static_cast<uint32_t>(compressedSize) < payloadBytes) {
            fh.compressed_size = compressedSize;
            dataToWriteBytes = compressedBuffer_.data();
            payloadBytes = compressedSize;
        } else {
            fh.compressed_size = 0;
        }
    } else {
        fh.compressed_size = writePacked_ ? payloadBytes : 0;
    }

    fh.iso = 100.0f;
    fh.exposure_time_ms = 16.67f;
    fh.white_balance_r = whiteBalanceR;
    fh.white_balance_g = whiteBalanceG;
    fh.white_balance_b = whiteBalanceB;

    if (dynamicBlackLevel) {
        for (int i = 0; i < 4; ++i) {
            fh.dynamic_black_level[i] = dynamicBlackLevel[i];
        }
    } else {
        for (int i = 0; i < 4; ++i) {
            fh.dynamic_black_level[i] = blackLevel_[i];
        }
    }

    if (fwrite(&fh, sizeof(SimpleFrameHeader), 1, outputFile_) != 1) {
        return false;
    }
    bytesWritten_ += sizeof(SimpleFrameHeader);

    if (fwrite(dataToWriteBytes, 1, payloadBytes, outputFile_) != payloadBytes) {
        return false;
    }
    bytesWritten_ += payloadBytes;

    return true;
}

bool VrawWriter::stop() {
    if (!outputFile_ || !isRecording_) {
        return false;
    }

    uint32_t frame_count = frameNumber_;

    // Write audio stream if enabled
    uint64_t audio_offset = 0;
    if (audioEnabled_ && !audioBuffer_.empty()) {
        audio_offset = bytesWritten_;

        AudioStreamHeader ash = {};
        memcpy(ash.magic, "MAUD", 4);
        ash.version = 1;
        ash.sample_rate = audioSampleRate_;
        ash.channels = audioChannels_;
        ash.bit_depth = 16;
        ash.sample_count = audioBuffer_.size() / audioChannels_;
        ash.start_timestamp_us = audioStartTime_;

        if (fwrite(&ash, sizeof(AudioStreamHeader), 1, outputFile_) != 1) {
            return false;
        }
        bytesWritten_ += sizeof(AudioStreamHeader);

        const size_t audioBytes = audioBuffer_.size() * sizeof(int16_t);
        if (fwrite(audioBuffer_.data(), 1, audioBytes, outputFile_) != audioBytes) {
            return false;
        }
        bytesWritten_ += audioBytes;

        // Update file header with audio info
        fseek(outputFile_, offsetof(SimpleFileHeader, has_audio), SEEK_SET);
        uint8_t has_audio = 1;
        fwrite(&has_audio, 1, 1, outputFile_);

        fseek(outputFile_, offsetof(SimpleFileHeader, audio_offset), SEEK_SET);
        fwrite(&audio_offset, sizeof(uint64_t), 1, outputFile_);

        fseek(outputFile_, offsetof(SimpleFileHeader, audio_start_time_us), SEEK_SET);
        fwrite(&audioStartTime_, sizeof(uint64_t), 1, outputFile_);

        fseek(outputFile_, 0, SEEK_END);
    }

    // Write index table
    uint64_t index_offset = bytesWritten_;
    for (uint64_t offset : frameOffsets_) {
        if (fwrite(&offset, sizeof(uint64_t), 1, outputFile_) != 1) {
            return false;
        }
        bytesWritten_ += sizeof(uint64_t);
    }

    // Write index header
    char index_magic[4] = {'M', 'I', 'D', 'X'};
    fwrite(index_magic, 4, 1, outputFile_);
    bytesWritten_ += 4;
    fwrite(&frame_count, sizeof(uint32_t), 1, outputFile_);
    bytesWritten_ += 4;
    uint8_t padding[8] = {0};
    fwrite(padding, 8, 1, outputFile_);
    bytesWritten_ += 8;

    // Update file header
    fseek(outputFile_, 32, SEEK_SET);
    fwrite(&frame_count, sizeof(uint32_t), 1, outputFile_);
    fwrite(&index_offset, sizeof(uint64_t), 1, outputFile_);

    fflush(outputFile_);
    isRecording_ = false;

    return true;
}

bool VrawWriter::flush() {
    if (!outputFile_) {
        return false;
    }
    return fflush(outputFile_) == 0;
}

bool VrawWriter::enableAudio(uint32_t sampleRate, uint16_t channels) {
    if (isRecording_) {
        return false;
    }

    audioEnabled_ = true;
    audioSampleRate_ = sampleRate;
    audioChannels_ = channels;
    audioStartTime_ = 0;
    audioBuffer_.clear();
    audioBuffer_.reserve(sampleRate * channels * 10);

    return true;
}

bool VrawWriter::submitAudio(const int16_t* samples, uint32_t sampleCount, uint64_t timestampUs) {
    if (!isRecording_ || !audioEnabled_ || !samples || sampleCount == 0) {
        return false;
    }

    if (audioStartTime_ == 0) {
        audioStartTime_ = timestampUs;
    }

    const uint32_t totalSamples = sampleCount * audioChannels_;
    audioBuffer_.insert(audioBuffer_.end(), samples, samples + totalSamples);

    return true;
}

uint64_t VrawWriter::getAudioSampleCount() const {
    return audioEnabled_ ? (audioBuffer_.size() / audioChannels_) : 0;
}

bool VrawWriter::ensurePackedCapacity(uint32_t packedBytes) {
    if (packedBuffer_.size() < packedBytes) {
        packedBuffer_.resize(packedBytes);
    }
    return true;
}

bool VrawWriter::ensureEncodedCapacity(uint32_t pixelCount) {
    if (encodedBuffer_.size() < pixelCount) {
        encodedBuffer_.resize(pixelCount);
    }
    return true;
}

bool VrawWriter::ensureCompressedCapacity(uint32_t uncompressedSize) {
    int maxSize = LZ4_compressBound(uncompressedSize);
    if (compressedBuffer_.size() < static_cast<size_t>(maxSize)) {
        compressedBuffer_.resize(maxSize);
    }
    return true;
}

uint32_t VrawWriter::packFrame10Bit(const uint16_t* src, uint32_t pixelCount) {
    const uint32_t packedBytes = (pixelCount * 10 + 7) / 8;
    ensurePackedCapacity(packedBytes);
    uint32_t outIdx = 0;
    uint32_t bitBuffer = 0;
    int bitCount = 0;
    uint8_t* dst = packedBuffer_.data();

    for (uint32_t i = 0; i < pixelCount; ++i) {
        uint32_t sample = src[i] & 0x3FF;
        bitBuffer |= sample << bitCount;
        bitCount += 10;
        while (bitCount >= 8) {
            dst[outIdx++] = bitBuffer & 0xFF;
            bitBuffer >>= 8;
            bitCount -= 8;
        }
    }
    if (bitCount > 0) {
        dst[outIdx++] = bitBuffer & 0xFF;
    }
    if (outIdx < packedBytes) {
        memset(dst + outIdx, 0, packedBytes - outIdx);
    }

    return packedBytes;
}

uint32_t VrawWriter::packFrame12Bit(const uint16_t* src, uint32_t pixelCount) {
    const uint32_t packedBytes = (pixelCount * 3 + 1) / 2;
    ensurePackedCapacity(packedBytes);
    uint32_t outIdx = 0;
    uint8_t* dst = packedBuffer_.data();

    for (uint32_t i = 0; i < pixelCount; i += 2) {
        uint16_t pixel1 = src[i] & 0xFFF;

        if (i + 1 < pixelCount) {
            uint16_t pixel2 = src[i + 1] & 0xFFF;
            dst[outIdx++] = (pixel1 >> 4) & 0xFF;
            dst[outIdx++] = ((pixel1 & 0xF) << 4) | ((pixel2 >> 8) & 0xF);
            dst[outIdx++] = pixel2 & 0xFF;
        } else {
            dst[outIdx++] = (pixel1 >> 4) & 0xFF;
            dst[outIdx++] = (pixel1 & 0xF) << 4;
        }
    }

    if (outIdx < packedBytes) {
        memset(dst + outIdx, 0, packedBytes - outIdx);
    }

    return packedBytes;
}

} // namespace vraw
