#pragma once

#include "v4l2_decoder.h" // For V4L2Decoder::DisplayType
#include <linux/videodev2.h>
#include <memory>
#include <vector>
#include <functional>

// Forward declarations
class DrmDmaBufDisplayManager;
class DmaBuffersManager;

class FrameProcessor {
public:
    using ZeroCopySetupCallback = std::function<void(unsigned int)>;

    FrameProcessor(
        DrmDmaBufDisplayManager* display_manager,
        DmaBuffersManager* output_buffers,
        uint32_t& frame_width,
        uint32_t& frame_height,
        V4L2Decoder::DisplayType& display_type,
        std::vector<bool>& zero_copy_initialized,
        int& decoded_frame_count,
        ZeroCopySetupCallback cb = nullptr
    );

    // Returns true if the buffer should be re-queued
    [[nodiscard]] bool processDecodedFrame(const v4l2_buffer& out_buf);

    // Update the DisplayManager pointer
    void setDisplayManager(DrmDmaBufDisplayManager* display_manager);

private:
    [[nodiscard]] bool validateOutputBuffer(const v4l2_buffer& out_buf) const;
    [[nodiscard]] bool displayFrame(const v4l2_buffer& out_buf);
    void setupZeroCopyBuffer(unsigned int index);

    DrmDmaBufDisplayManager* display_manager_;
    DmaBuffersManager* output_buffers_;
    uint32_t& frame_width_;
    uint32_t& frame_height_;
    V4L2Decoder::DisplayType& display_type_;
    std::vector<bool>& zero_copy_initialized_;
    int& decoded_frame_count_;
    ZeroCopySetupCallback zero_copy_setup_callback_;
};
