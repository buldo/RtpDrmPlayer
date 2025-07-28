#pragma once

#include <linux/videodev2.h>
#include <poll.h>
#include <string_view>
#include <vector>

/**
 * @class V4L2Device
 * @brief Encapsulates low-level interaction with a V4L2 device.
 *
 * This class provides a thin wrapper around ioctl calls for V4L2,
 * managing the file descriptor and performing basic device operations.
 */
class V4L2Device {
public:
    V4L2Device();
    ~V4L2Device();

    // Disallow copying and assignment
    V4L2Device(const V4L2Device&) = delete;
    V4L2Device& operator=(const V4L2Device&) = delete;

    [[nodiscard]] bool open(std::string_view device_path);
    void close();
    [[nodiscard]] bool is_open() const { return fd_ >= 0; }
    [[nodiscard]] int fd() const { return fd_; }

    [[nodiscard]] bool query_capability(v4l2_capability& cap);
    [[nodiscard]] bool set_format(v4l2_format& fmt);
    [[nodiscard]] bool get_format(v4l2_format& fmt);
    [[nodiscard]] bool set_control(const v4l2_control& ctrl);
    [[nodiscard]] bool request_buffers(v4l2_requestbuffers& req);
    [[nodiscard]] bool queue_buffer(v4l2_buffer& buf);
    [[nodiscard]] bool dequeue_buffer(v4l2_buffer& buf);
    [[nodiscard]] bool stream_on(enum v4l2_buf_type type);
    [[nodiscard]] bool stream_off(enum v4l2_buf_type type);
    [[nodiscard]] bool subscribe_to_events();
    [[nodiscard]] bool dequeue_event(v4l2_event& ev);
    [[nodiscard]] bool configure_decoder_formats(uint32_t width, uint32_t height, uint32_t in_pixel_format, uint32_t out_pixel_format);
    [[nodiscard]] bool initialize_for_decoding(std::string_view device_path);

    // Poll-related methods
    [[nodiscard]] bool poll(short events, int timeout_ms);
    [[nodiscard]] bool has_event() const;
    [[nodiscard]] bool has_error() const;
    [[nodiscard]] bool is_ready_for_read() const;
    [[nodiscard]] bool is_ready_for_write() const;

private:
    [[nodiscard]] bool ioctl_helper(unsigned long request, void* arg, const char* request_name);
    [[nodiscard]] bool subscribe_event(v4l2_event_subscription& sub);
    [[nodiscard]] bool check_dma_buf_support();

    int fd_ = -1;
    std::vector<pollfd> poll_fds_;
    short revents_ = 0;
};
