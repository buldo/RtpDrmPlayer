#pragma once

#include "v4l2_decoder.h" // Для V4L2Decoder::DisplayType
#include <linux/videodev2.h>
#include <memory>
#include <vector>
#include <functional>

// Forward declarations
class DisplayManager;
class DmaBuffersManager;

class FrameProcessor {
public:
    using ZeroCopySetupCallback = std::function<void(unsigned int)>;

    FrameProcessor(
        DisplayManager* display_manager,
        DmaBuffersManager* output_buffers,
        uint32_t& frame_width,
        uint32_t& frame_height,
        V4L2Decoder::DisplayType& display_type,
        std::vector<bool>& zero_copy_initialized,
        int& decoded_frame_count,
        ZeroCopySetupCallback cb = nullptr
    );

    // Возвращает true, если буфер должен быть повторно поставлен в очередь
    [[nodiscard]] bool processDecodedFrame(const v4l2_buffer& out_buf);

    // Обновление указателя на DisplayManager
    void setDisplayManager(DisplayManager* display_manager);

private:
    [[nodiscard]] bool validateOutputBuffer(const v4l2_buffer& out_buf) const;
    [[nodiscard]] bool displayFrame(const v4l2_buffer& out_buf);
    void setupZeroCopyBuffer(unsigned int index);

    DisplayManager* display_manager_;
    DmaBuffersManager* output_buffers_;
    uint32_t& frame_width_;
    uint32_t& frame_height_;
    V4L2Decoder::DisplayType& display_type_;
    std::vector<bool>& zero_copy_initialized_;
    int& decoded_frame_count_;
    ZeroCopySetupCallback zero_copy_setup_callback_;
};
