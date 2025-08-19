#include "streaming_manager.h"
#include "v4l2_device.h"
#include "dma_buffers_manager.h"
#include <iostream>
#include <unistd.h> // For usleep

StreamingManager::StreamingManager(V4L2Device& device, DmaBuffersManager& output_buffers)
    : device_(device), output_buffers_(output_buffers) {}

StreamingManager::~StreamingManager() {
    if (state_ == State::ACTIVE) {
        stop();
    }
}

bool StreamingManager::is_active() const {
    return state_ == State::ACTIVE;
}

void StreamingManager::set_inactive() {
    state_ = State::STOPPED;
}

bool StreamingManager::start() {
    if (state_ == State::ACTIVE) {
        std::cout << "Streaming is already active" << std::endl;
        return true;
    }

    state_ = State::STARTING;

    if (!queueOutputBuffers()) {
        state_ = State::ERROR;
        return false;
    }

    if (!enableStreaming()) {
        state_ = State::ERROR;
        return false;
    }

    state_ = State::ACTIVE;
    std::cout << "✅ Streaming started successfully" << std::endl;
    return true;
}

bool StreamingManager::stop() {
    if (state_ == State::STOPPED) {
        return true;
    }

    state_ = State::STOPPING;

    (void)device_.stream_off(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    (void)device_.stream_off(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

    state_ = State::STOPPED;

    usleep(10000); // 10ms

    std::cout << "✅ Streaming stopped" << std::endl;
    return true;
}

bool StreamingManager::queueOutputBuffers() {
    std::cout << "Queuing " << output_buffers_.count() << " output buffers..." << std::endl;

    for (unsigned int i = 0; i < output_buffers_.count(); ++i) {
        if (!queueOutputBuffer(i)) {
            std::cerr << "❌ Error queuing buffer " << i << std::endl;
            return false;
        }
    }
    return true;
}

bool StreamingManager::queueOutputBuffer(unsigned int index) {
    if (index >= output_buffers_.count()) {
        std::cerr << "❌ Invalid buffer index: " << index << std::endl;
        return false;
    }

    struct v4l2_buffer buf = {};
    struct v4l2_plane plane = {};
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    buf.memory = V4L2_MEMORY_DMABUF;
    buf.index = index;
    buf.m.planes = &plane;
    buf.length = 1;
    plane.m.fd = output_buffers_.get_info(index).fd;
    plane.length = output_buffers_.get_info(index).size;

    if (!device_.queue_buffer(buf)) {
        std::cerr << "❌ VIDIOC_QBUF for buffer " << index << " failed" << std::endl;
        return false;
    }
    return true;
}

bool StreamingManager::enableStreaming() {
    if (!device_.stream_on(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)) {
        std::cerr << "❌ VIDIOC_STREAMON for input failed" << std::endl;
        return false;
    }

    if (!device_.stream_on(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) {
        std::cerr << "❌ VIDIOC_STREAMON for output failed" << std::endl;
        (void)device_.stream_off(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
        return false;
    }

    return true;
}
