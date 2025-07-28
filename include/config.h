#pragma once

#include <string>
#include <cstdint>
#include <linux/videodev2.h>

// Structure for storing all decoder settings
struct DecoderConfig {
    // Path to V4L2 device
    std::string device_path = "/dev/video0";

    // Video parameters
    uint32_t width = 1920;
    uint32_t height = 1080;
    uint32_t input_codec = V4L2_PIX_FMT_H264;
    uint32_t output_pixel_format = V4L2_PIX_FMT_YUV420;

    // Number of buffers
    size_t input_buffer_count = 6;
    size_t output_buffer_count = 4;

    // Default input buffer size if not reported by the driver
    size_t default_input_buffer_size = 2 * 1024 * 1024; // 2MB
};
