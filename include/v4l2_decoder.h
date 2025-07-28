#pragma once

#include "config.h"
#include <memory>
#include <string_view>

class V4L2DecoderImpl;
class DmaBufAllocator;
class DisplayManager;

/**
 * @brief V4L2 Hardware H.264 Decoder
 * 
 * Этот класс предоставляет интерфейс для аппаратного декодирования H.264 видео
 * через Linux V4L2 API. Использует DMA-buf буферы для эффективного обмена данными
 * и поддерживает Memory-to-Memory операции с multiplanar буферами.
 */
class V4L2Decoder {
public:
    enum class DisplayType {
        NONE,       // Без отображения
        DRM_DMABUF  // TRUE Zero-Copy через DMA-buf
    };

private:
    std::unique_ptr<V4L2DecoderImpl> impl;

public:
    V4L2Decoder();
    ~V4L2Decoder();

    [[nodiscard]] bool initialize(const DecoderConfig& config);
    [[nodiscard]] bool setDisplay(DisplayType display_type);
    [[nodiscard]] bool decodeData(const uint8_t* data, size_t size);
    [[nodiscard]] bool flushDecoder();  // Принудительная очистка буферов декодера
    [[nodiscard]] bool resetBuffers();  // Полный сброс и пересоздание буферов
    [[nodiscard]] int getDecodedFrameCount() const;

    V4L2Decoder(const V4L2Decoder&) = delete;
    V4L2Decoder& operator=(const V4L2Decoder&) = delete;
};
