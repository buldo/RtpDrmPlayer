#include "v4l2_device.h"
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

V4L2Device::V4L2Device() = default;

V4L2Device::~V4L2Device() {
    close();
}

bool V4L2Device::open(std::string_view device_path) {
    if (is_open()) {
        std::cerr << "Device is already open" << std::endl;
        return false;
    }

    fd_ = ::open(device_path.data(), O_RDWR | O_NONBLOCK, 0);
    if (fd_ < 0) {
        std::cerr << "Error opening device " << device_path << ": " << strerror(errno) << std::endl;
        return false;
    }

    poll_fds_.resize(1);
    poll_fds_[0].fd = fd_;
    poll_fds_[0].events = POLLPRI; // Default for events

    std::cout << "Device " << device_path << " opened, fd=" << fd_ << std::endl;
    return true;
}

void V4L2Device::close() {
    if (is_open()) {
        if (::close(fd_) < 0) {
            std::cerr << "Error closing device: " << strerror(errno) << std::endl;
        }
        fd_ = -1;
        poll_fds_.clear();
        std::cout << "Device closed" << std::endl;
    }
}

bool V4L2Device::ioctl_helper(unsigned long request, void* arg, const char* request_name) {
    if (!is_open()) {
        std::cerr << "Device not open for ioctl " << request_name << std::endl;
        return false;
    }
    if (ioctl(fd_, request, arg) < 0) {
        std::cerr << "Error ioctl " << request_name << ": " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool V4L2Device::query_capability(v4l2_capability& cap) {
    return ioctl_helper(VIDIOC_QUERYCAP, &cap, "VIDIOC_QUERYCAP");
}

bool V4L2Device::set_format(v4l2_format& fmt) {
    return ioctl_helper(VIDIOC_S_FMT, &fmt, "VIDIOC_S_FMT");
}

bool V4L2Device::get_format(v4l2_format& fmt) {
    return ioctl_helper(VIDIOC_G_FMT, &fmt, "VIDIOC_G_FMT");
}

bool V4L2Device::set_control(const v4l2_control& ctrl) {
    // Create a copy as ioctl might modify the structure
    v4l2_control temp_ctrl = ctrl;
    return ioctl_helper(VIDIOC_S_CTRL, &temp_ctrl, "VIDIOC_S_CTRL");
}

bool V4L2Device::request_buffers(v4l2_requestbuffers& req) {
    return ioctl_helper(VIDIOC_REQBUFS, &req, "VIDIOC_REQBUFS");
}

bool V4L2Device::queue_buffer(v4l2_buffer& buf) {
    return ioctl_helper(VIDIOC_QBUF, &buf, "VIDIOC_QBUF");
}

bool V4L2Device::dequeue_buffer(v4l2_buffer& buf) {
    // This ioctl can return EAGAIN, which is not a fatal error
    if (!is_open()) {
        std::cerr << "Device not open for ioctl VIDIOC_DQBUF" << std::endl;
        return false;
    }
    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
        if (errno != EAGAIN) {
            std::cerr << "Error ioctl VIDIOC_DQBUF: " << strerror(errno) << std::endl;
        }
        return false;
    }
    return true;
}

bool V4L2Device::stream_on(enum v4l2_buf_type type) {
    return ioctl_helper(VIDIOC_STREAMON, &type, "VIDIOC_STREAMON");
}

bool V4L2Device::stream_off(enum v4l2_buf_type type) {
    return ioctl_helper(VIDIOC_STREAMOFF, &type, "VIDIOC_STREAMOFF");
}

bool V4L2Device::subscribe_to_events() {
    v4l2_event_subscription sub_eos = {};
    sub_eos.type = V4L2_EVENT_EOS;
    if (!subscribe_event(sub_eos)) return false;

    v4l2_event_subscription sub_source_change = {};
    sub_source_change.type = V4L2_EVENT_SOURCE_CHANGE;
    if (!subscribe_event(sub_source_change)) return false;

    std::cout << "✅ Subscribed to V4L2_EVENT_EOS and V4L2_EVENT_SOURCE_CHANGE events" << std::endl;
    return true;
}

bool V4L2Device::dequeue_event(v4l2_event& ev) {
    return ioctl_helper(VIDIOC_DQEVENT, &ev, "VIDIOC_DQEVENT");
}

bool V4L2Device::poll(short events, int timeout_ms) {
    if (!is_open()) {
        std::cerr << "Device not open for poll" << std::endl;
        return false;
    }
    poll_fds_[0].events = events;
    int ret = ::poll(poll_fds_.data(), poll_fds_.size(), timeout_ms);
    if (ret < 0) {
        std::cerr << "Poll error: " << strerror(errno) << std::endl;
        revents_ = 0;
        return false;
    }
    if (ret == 0) {
        // Timeout
        revents_ = 0;
        return true;
    }
    revents_ = poll_fds_[0].revents;
    return true;
}

bool V4L2Device::has_event() const {
    return revents_ & POLLPRI;
}

bool V4L2Device::has_error() const {
    return revents_ & POLLERR;
}

bool V4L2Device::is_ready_for_read() const {
    return revents_ & POLLIN;
}

bool V4L2Device::is_ready_for_write() const {
    return revents_ & POLLOUT;
}

bool V4L2Device::subscribe_event(v4l2_event_subscription& sub) {
    return ioctl_helper(VIDIOC_SUBSCRIBE_EVENT, &sub, "VIDIOC_SUBSCRIBE_EVENT");
}

bool V4L2Device::configure_decoder_formats(uint32_t width, uint32_t height, uint32_t in_pixel_format, uint32_t out_pixel_format) {
    struct v4l2_format fmt_in = {};
    fmt_in.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt_in.fmt.pix_mp.width = width;
    fmt_in.fmt.pix_mp.height = height;
    fmt_in.fmt.pix_mp.pixelformat = in_pixel_format;
    fmt_in.fmt.pix_mp.num_planes = 1;
    fmt_in.fmt.pix_mp.plane_fmt[0].sizeimage = 2 * 1024 * 1024; // 2MB for H.264
    if (!set_format(fmt_in)) {
        std::cerr << "❌ ERROR: Failed to set input format" << std::endl;
        return false;
    }
    std::cout << "Input format set: " << width << "x" << height << std::endl;

    struct v4l2_format fmt_out = {};
    fmt_out.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt_out.fmt.pix_mp.width = width;
    fmt_out.fmt.pix_mp.height = height;
    fmt_out.fmt.pix_mp.pixelformat = out_pixel_format;
    fmt_out.fmt.pix_mp.num_planes = 1;
    if (!set_format(fmt_out)) {
        std::cerr << "❌ ERROR: Failed to set output format" << std::endl;
        return false;
    }
    std::cout << "Output format set: " << width << "x" << height << std::endl;

    // Set the minimum number of capture buffers for minimum latency
    struct v4l2_control ctrl = {};
    ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
    ctrl.value = 1;
    if (!set_control(ctrl)) {
        std::cout << "⚠️ WARNING: Failed to set V4L2_CID_MIN_BUFFERS_FOR_CAPTURE=1. This may increase latency." << std::endl;
        // Not critical, continue
    } else {
        std::cout << "✅ Minimum output buffering set (MIN_BUFFERS_FOR_CAPTURE=1) for low latency" << std::endl;
    }

    return true;
}

bool V4L2Device::check_dma_buf_support() {
    // Try to request buffers in DMA-buf mode for testing
    struct v4l2_requestbuffers req = {};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    req.memory = V4L2_MEMORY_DMABUF;

    bool supported = request_buffers(req);

    std::cout << "Checking DMA-buf support: " << (supported ? "OK" : "FAIL")
              << std::endl;

    // Reset buffers after testing
    if (supported) {
        req.count = 0;
        (void)request_buffers(req);
    }

    return supported;
}

bool V4L2Device::initialize_for_decoding(std::string_view device_path) {
    std::cout << "Initializing V4L2 device for decoding: " << device_path << std::endl;

    if (!open(device_path)) {
        return false;
    }

    struct v4l2_capability cap;
    if (!query_capability(cap)) {
        close();
        return false;
    }

    std::cout << "Device: " << cap.card << "\nDriver: " << cap.driver << std::endl;

    if (!(cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE)) [[unlikely]] {
        std::cerr << "❌ ERROR: Device does not support V4L2_CAP_VIDEO_M2M_MPLANE" << std::endl;
        std::cerr << "Available capabilities: 0x" << std::hex << cap.capabilities << std::dec << std::endl;
        close();
        return false;
    }

    // Check for DMA-buf support in the V4L2 driver
    if (!check_dma_buf_support()) {
        std::cerr << "❌ CRITICAL ERROR: V4L2 driver does not support DMA-buf" << std::endl;
        std::cerr << "Possible reasons:" << std::endl;
        std::cerr << "  - Outdated driver" << std::endl;
        std::cerr << "  - Incorrect kernel configuration" << std::endl;
        close();
        return false;
    }

    std::cout << "Using DMA-buf buffers" << std::endl;

    // Subscribe to critical V4L2 events
    if (!subscribe_to_events()) {
        std::cerr << "⚠️ WARNING: Failed to subscribe to V4L2 events" << std::endl;
        // Continue operation - events are not critical for basic functionality
    }

    return true;
}
