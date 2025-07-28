#include "frame_processor.h"
#include "display_manager.h"
#include "dma_buffers_manager.h"
#include <iostream>
#include <algorithm> // Для std::min

FrameProcessor::FrameProcessor(
    DisplayManager* display_manager,
    DmaBuffersManager* output_buffers,
    uint32_t& frame_width,
    uint32_t& frame_height,
    V4L2Decoder::DisplayType& display_type,
    std::vector<bool>& zero_copy_initialized,
    int& decoded_frame_count,
    ZeroCopySetupCallback cb)
    : display_manager_(display_manager),
      output_buffers_(output_buffers),
      frame_width_(frame_width),
      frame_height_(frame_height),
      display_type_(display_type),
      zero_copy_initialized_(zero_copy_initialized),
      decoded_frame_count_(decoded_frame_count),
      zero_copy_setup_callback_(cb) {}

bool FrameProcessor::processDecodedFrame(const v4l2_buffer& out_buf) {
    if (!validateOutputBuffer(out_buf)) {
        return true; // Возвращаем true, чтобы плохой буфер был перепоставлен в очередь
    }

    const auto& out_plane = out_buf.m.planes[0];
    decoded_frame_count_++;
    std::cout << "✅ Кадр #" << decoded_frame_count_ << " (буфер " << out_buf.index 
              << ", размер: " << out_plane.bytesused << ")" << std::endl;

    // DEBUG: Проверяем состояние перед отображением
    std::cout << "  [Debug] Проверка перед отображением: display_manager=" 
              << (display_manager_ ? "есть" : "нет")
              << ", width=" << frame_width_ << ", height=" << frame_height_ << std::endl;

    if (display_manager_ && frame_width_ > 0 && frame_height_ > 0) {
        if (!displayFrame(out_buf)) {
            std::cerr << "⚠️ Ошибка отображения кадра " << decoded_frame_count_ << std::endl;
        }
    }

    return true; // Всегда возвращаем true, чтобы буфер был перепоставлен
}

bool FrameProcessor::validateOutputBuffer(const v4l2_buffer& out_buf) const {
    if (out_buf.index >= output_buffers_->count()) {
        std::cerr << "❌ Недопустимый индекс буфера: " << out_buf.index 
                  << " >= " << output_buffers_->count() << std::endl;
        return false;
    }

    if (output_buffers_->get_info(out_buf.index).fd < 0 || !output_buffers_->get_info(out_buf.index).mapped_addr) {
        std::cerr << "❌ Недопустимый DMA-buf буфер " << out_buf.index << std::endl;
        return false;
    }

    if (out_buf.flags & V4L2_BUF_FLAG_ERROR) {
        std::cerr << "⚠️ Буфер " << out_buf.index << " содержит ошибки декодирования" << std::endl;
        return false;
    }

    return true;
}

bool FrameProcessor::displayFrame(const v4l2_buffer& out_buf) {
    std::cout << "FrameProcessor::displayFrame для буфера " << out_buf.index << std::endl;
    const auto& out_plane = out_buf.m.planes[0];
    uint8_t* buffer = static_cast<uint8_t*>(output_buffers_->get_info(out_buf.index).mapped_addr);
    size_t min_expected_size = frame_width_ * frame_height_ * 3 / 2;

    if (out_plane.bytesused < min_expected_size / 2) {
        std::cerr << "⚠️ Буфер " << out_buf.index << " слишком мал: " 
                  << out_plane.bytesused << " < " << min_expected_size/2 << std::endl;
        return false;
    }

    __sync_synchronize();

    bool has_content = false;
    for (size_t i = 0; i < std::min<size_t>(1024, out_plane.bytesused); i += 64) {
        if (buffer[i] != 16 || buffer[i+1] != 16) {
            has_content = true;
            break;
        }
    }

    if (!has_content) {
        std::cerr << "⚠️ Буфер " << out_buf.index << " содержит только инициализированные данные" << std::endl;
        return false;
    }

    DisplayManager::FrameInfo frame_info = {
        output_buffers_->get_info(out_buf.index).mapped_addr,
        output_buffers_->get_info(out_buf.index).fd,
        frame_width_,
        frame_height_,
        V4L2_PIX_FMT_YUV420,
        out_plane.bytesused,
        true
    };

    if (display_type_ == V4L2Decoder::DisplayType::DRM_DMABUF) {
        setupZeroCopyBuffer(out_buf.index);
    }

    bool success = display_manager_->displayFrame(frame_info);
    if (!success) {
        std::cerr << "❌ display_manager_->displayFrame не удался" << std::endl;
    }
    return success;
}

void FrameProcessor::setupZeroCopyBuffer(unsigned int index) {
    std::cout << "FrameProcessor::setupZeroCopyBuffer для буфера " << index << std::endl;
    if (zero_copy_setup_callback_) {
        zero_copy_setup_callback_(index);
    }
}

void FrameProcessor::setDisplayManager(DisplayManager* display_manager) {
    display_manager_ = display_manager;
}
