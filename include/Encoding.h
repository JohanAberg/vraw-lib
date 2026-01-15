/**
 * VRAW Library - Log Encoding Functions
 *
 * Standalone C++ library for VRAW video files.
 * https://github.com/JohanAberg/vertigo
 */

#ifndef VRAW_ENCODING_H
#define VRAW_ENCODING_H

#include <cstdint>

namespace vraw {

/**
 * Encode a single pixel using Log2 10-bit encoding.
 *
 * @param pixel Linear pixel value
 * @param blackLevel Black level to subtract
 * @param whiteLevel White level (sensor saturation)
 * @return Encoded 10-bit value (0-1023)
 */
uint16_t encodePixelLog10Bit(uint16_t pixel, uint16_t blackLevel, uint16_t whiteLevel);

/**
 * Encode a single pixel using Log2 12-bit encoding.
 *
 * @param pixel Linear pixel value
 * @param blackLevel Black level to subtract
 * @param whiteLevel White level (sensor saturation)
 * @return Encoded 12-bit value (0-4095)
 */
uint16_t encodePixelLog12Bit(uint16_t pixel, uint16_t blackLevel, uint16_t whiteLevel);

/**
 * Encode an array of pixels using Log2 10-bit encoding.
 *
 * @param input Linear pixel array
 * @param output Encoded pixel array (must be pre-allocated)
 * @param pixelCount Number of pixels to encode
 * @param blackLevel Black level to subtract
 * @param whiteLevel White level (sensor saturation)
 */
void encodeLog10Bit(const uint16_t* input, uint16_t* output,
                    uint32_t pixelCount, uint16_t blackLevel,
                    uint16_t whiteLevel);

/**
 * Encode an array of pixels using Log2 12-bit encoding.
 *
 * @param input Linear pixel array
 * @param output Encoded pixel array (must be pre-allocated)
 * @param pixelCount Number of pixels to encode
 * @param blackLevel Black level to subtract
 * @param whiteLevel White level (sensor saturation)
 */
void encodeLog12Bit(const uint16_t* input, uint16_t* output,
                    uint32_t pixelCount, uint16_t blackLevel,
                    uint16_t whiteLevel);

/**
 * Decode a single pixel from Log2 10-bit encoding.
 *
 * @param encoded Encoded 10-bit value
 * @param blackLevel Black level to add back
 * @param whiteLevel White level
 * @return Linear pixel value
 */
uint16_t decodePixelLog10Bit(uint16_t encoded, uint16_t blackLevel, uint16_t whiteLevel);

/**
 * Decode a single pixel from Log2 12-bit encoding.
 *
 * @param encoded Encoded 12-bit value
 * @param blackLevel Black level to add back
 * @param whiteLevel White level
 * @return Linear pixel value
 */
uint16_t decodePixelLog12Bit(uint16_t encoded, uint16_t blackLevel, uint16_t whiteLevel);

} // namespace vraw

#endif // VRAW_ENCODING_H
