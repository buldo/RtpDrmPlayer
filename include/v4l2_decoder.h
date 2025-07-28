#pragma once

#include "config.h"
#include <memory>
#include <string_view>

class V4L2DecoderImpl;
class DmaBufAllocator;

/**
 * @brief V4L2 Hardware H.264 Decoder
 * 
 * This class provides an interface for hardware-accelerated H.264 video decoding
 * via the Linux V4L2 API. It uses DMA-buf buffers for efficient data exchange
 * and supports Memory-to-Memory operations with multiplanar buffers.
 */
class V4L2Decoder {
public:
    enum class DisplayType {
        NONE,       // No display
        DRM_DMABUF  // TRUE Zero-Copy via DMA-buf
    };

private:
    std::unique_ptr<V4L2DecoderImpl> impl;

public:
    V4L2Decoder();
    ~V4L2Decoder();

    [[nodiscard]] bool initialize(const DecoderConfig& config);
    [[nodiscard]] bool setDisplay();
    [[nodiscard]] bool decodeData(const uint8_t* data, size_t size);
    [[nodiscard]] bool flushDecoder();  // Force flush decoder buffers
    [[nodiscard]] bool resetBuffers();  // Full reset and recreation of buffers
    [[nodiscard]] int getDecodedFrameCount() const;

    V4L2Decoder(const V4L2Decoder&) = delete;
    V4L2Decoder& operator=(const V4L2Decoder&) = delete;
};
