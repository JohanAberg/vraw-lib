/**
 * VRAW Library - Log Encoding Implementation
 */

#include "Encoding.h"
#include <cmath>
#include <algorithm>

// NEON support for ARM processors
#if defined(__ARM_NEON) || defined(__aarch64__)
#include <arm_neon.h>
#define HAS_NEON 1
#else
#define HAS_NEON 0
#endif

namespace vraw {

uint16_t encodePixelLog10Bit(uint16_t pixel, uint16_t blackLevel, uint16_t whiteLevel) {
    int32_t linear = static_cast<int32_t>(pixel) - static_cast<int32_t>(blackLevel);
    if (linear <= 0) return 0;

    float normalized = static_cast<float>(linear) / static_cast<float>(whiteLevel - blackLevel);
    normalized = std::min(1.0f, std::max(0.0f, normalized));

    float encoded = std::log2f(normalized * 1023.0f + 1.0f) / std::log2f(1024.0f);
    uint16_t result = static_cast<uint16_t>(encoded * 1023.0f + 0.5f);
    return std::min(result, static_cast<uint16_t>(1023));
}

uint16_t encodePixelLog12Bit(uint16_t pixel, uint16_t blackLevel, uint16_t whiteLevel) {
    int32_t linear = static_cast<int32_t>(pixel) - static_cast<int32_t>(blackLevel);
    if (linear <= 0) return 0;

    float normalized = static_cast<float>(linear) / static_cast<float>(whiteLevel - blackLevel);
    normalized = std::min(1.0f, std::max(0.0f, normalized));

    float encoded = std::log2f(normalized * 4095.0f + 1.0f) / std::log2f(4096.0f);
    uint16_t result = static_cast<uint16_t>(encoded * 4095.0f + 0.5f);
    return std::min(result, static_cast<uint16_t>(4095));
}

void encodeLog10Bit(const uint16_t* input, uint16_t* output,
                    uint32_t pixelCount, uint16_t blackLevel,
                    uint16_t whiteLevel) {
#if HAS_NEON
    const uint32_t vec_count = pixelCount / 8;

    const float inv_range = 1.0f / static_cast<float>(whiteLevel - blackLevel);
    const float log2_1024_inv = 1.0f / std::log2f(1024.0f);

    const uint16x8_t v_black = vdupq_n_u16(blackLevel);
    const float32x4_t v_inv_range = vdupq_n_f32(inv_range);
    const float32x4_t v_1023 = vdupq_n_f32(1023.0f);
    const float32x4_t v_1 = vdupq_n_f32(1.0f);

    for (uint32_t i = 0; i < vec_count; i++) {
        uint16x8_t pixels = vld1q_u16(input + i * 8);
        uint16x8_t linear = vqsubq_u16(pixels, v_black);

        uint32x4_t linear_lo_u32 = vmovl_u16(vget_low_u16(linear));
        uint32x4_t linear_hi_u32 = vmovl_u16(vget_high_u16(linear));
        float32x4_t linear_lo = vcvtq_f32_u32(linear_lo_u32);
        float32x4_t linear_hi = vcvtq_f32_u32(linear_hi_u32);

        float32x4_t norm_lo = vminq_f32(vmulq_f32(linear_lo, v_inv_range), v_1);
        float32x4_t norm_hi = vminq_f32(vmulq_f32(linear_hi, v_inv_range), v_1);

        float32x4_t x_lo = vmlaq_f32(v_1, norm_lo, v_1023);
        float32x4_t x_hi = vmlaq_f32(v_1, norm_hi, v_1023);

        float temp_lo[4], temp_hi[4];
        vst1q_f32(temp_lo, x_lo);
        vst1q_f32(temp_hi, x_hi);

        uint16_t results[8];
        for (int j = 0; j < 4; j++) {
            float log_val = std::log2f(temp_lo[j]) * log2_1024_inv;
            results[j] = static_cast<uint16_t>(log_val * 1023.0f + 0.5f);
            results[j] = std::min(results[j], static_cast<uint16_t>(1023));
        }
        for (int j = 0; j < 4; j++) {
            float log_val = std::log2f(temp_hi[j]) * log2_1024_inv;
            results[4 + j] = static_cast<uint16_t>(log_val * 1023.0f + 0.5f);
            results[4 + j] = std::min(results[4 + j], static_cast<uint16_t>(1023));
        }

        vst1q_u16(output + i * 8, vld1q_u16(results));
    }

    for (uint32_t i = vec_count * 8; i < pixelCount; i++) {
        output[i] = encodePixelLog10Bit(input[i], blackLevel, whiteLevel);
    }
#else
    for (uint32_t i = 0; i < pixelCount; i++) {
        output[i] = encodePixelLog10Bit(input[i], blackLevel, whiteLevel);
    }
#endif
}

void encodeLog12Bit(const uint16_t* input, uint16_t* output,
                    uint32_t pixelCount, uint16_t blackLevel,
                    uint16_t whiteLevel) {
#if HAS_NEON
    const uint32_t vec_count = pixelCount / 8;

    const float inv_range = 1.0f / static_cast<float>(whiteLevel - blackLevel);
    const float log2_4096_inv = 1.0f / std::log2f(4096.0f);

    const uint16x8_t v_black = vdupq_n_u16(blackLevel);
    const float32x4_t v_inv_range = vdupq_n_f32(inv_range);
    const float32x4_t v_4095 = vdupq_n_f32(4095.0f);
    const float32x4_t v_1 = vdupq_n_f32(1.0f);

    for (uint32_t i = 0; i < vec_count; i++) {
        uint16x8_t pixels = vld1q_u16(input + i * 8);
        uint16x8_t linear = vqsubq_u16(pixels, v_black);

        uint32x4_t linear_lo_u32 = vmovl_u16(vget_low_u16(linear));
        uint32x4_t linear_hi_u32 = vmovl_u16(vget_high_u16(linear));
        float32x4_t linear_lo = vcvtq_f32_u32(linear_lo_u32);
        float32x4_t linear_hi = vcvtq_f32_u32(linear_hi_u32);

        float32x4_t norm_lo = vminq_f32(vmulq_f32(linear_lo, v_inv_range), v_1);
        float32x4_t norm_hi = vminq_f32(vmulq_f32(linear_hi, v_inv_range), v_1);

        float32x4_t x_lo = vmlaq_f32(v_1, norm_lo, v_4095);
        float32x4_t x_hi = vmlaq_f32(v_1, norm_hi, v_4095);

        float temp_lo[4], temp_hi[4];
        vst1q_f32(temp_lo, x_lo);
        vst1q_f32(temp_hi, x_hi);

        uint16_t results[8];
        for (int j = 0; j < 4; j++) {
            float log_val = std::log2f(temp_lo[j]) * log2_4096_inv;
            results[j] = static_cast<uint16_t>(log_val * 4095.0f + 0.5f);
            results[j] = std::min(results[j], static_cast<uint16_t>(4095));
        }
        for (int j = 0; j < 4; j++) {
            float log_val = std::log2f(temp_hi[j]) * log2_4096_inv;
            results[4 + j] = static_cast<uint16_t>(log_val * 4095.0f + 0.5f);
            results[4 + j] = std::min(results[4 + j], static_cast<uint16_t>(4095));
        }

        vst1q_u16(output + i * 8, vld1q_u16(results));
    }

    for (uint32_t i = vec_count * 8; i < pixelCount; i++) {
        output[i] = encodePixelLog12Bit(input[i], blackLevel, whiteLevel);
    }
#else
    for (uint32_t i = 0; i < pixelCount; i++) {
        output[i] = encodePixelLog12Bit(input[i], blackLevel, whiteLevel);
    }
#endif
}

uint16_t decodePixelLog10Bit(uint16_t encoded, uint16_t blackLevel, uint16_t whiteLevel) {
    float normalized = static_cast<float>(encoded) / 1023.0f;
    float linear = (std::exp2f(normalized * std::log2f(1024.0f)) - 1.0f) / 1023.0f;
    linear = std::min(1.0f, std::max(0.0f, linear));

    int32_t value = static_cast<int32_t>(linear * (whiteLevel - blackLevel) + blackLevel + 0.5f);
    return static_cast<uint16_t>(std::min(std::max(value, 0), 65535));
}

uint16_t decodePixelLog12Bit(uint16_t encoded, uint16_t blackLevel, uint16_t whiteLevel) {
    float normalized = static_cast<float>(encoded) / 4095.0f;
    float linear = (std::exp2f(normalized * std::log2f(4096.0f)) - 1.0f) / 4095.0f;
    linear = std::min(1.0f, std::max(0.0f, linear));

    int32_t value = static_cast<int32_t>(linear * (whiteLevel - blackLevel) + blackLevel + 0.5f);
    return static_cast<uint16_t>(std::min(std::max(value, 0), 65535));
}

void decodeLog10Bit(const uint16_t* input, uint16_t* output,
                    uint32_t pixelCount, uint16_t blackLevel,
                    uint16_t whiteLevel) {
    for (uint32_t i = 0; i < pixelCount; i++) {
        output[i] = decodePixelLog10Bit(input[i], blackLevel, whiteLevel);
    }
}

void decodeLog12Bit(const uint16_t* input, uint16_t* output,
                    uint32_t pixelCount, uint16_t blackLevel,
                    uint16_t whiteLevel) {
    for (uint32_t i = 0; i < pixelCount; i++) {
        output[i] = decodePixelLog12Bit(input[i], blackLevel, whiteLevel);
    }
}

} // namespace vraw
