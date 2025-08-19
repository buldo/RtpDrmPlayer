/**
 * @file uvgrtp_receiver.cpp
 * @brief Implementation of an RTP receiver based on the uvgRTP library
 */

#include "uvgrtp_receiver.h"
#include <iostream>
#include <cstring>

UvgRTPReceiver::UvgRTPReceiver(const std::string& local_ip, uint16_t local_port)
    : local_ip_(local_ip), local_port_(local_port), stream_(nullptr),
      running_(false), initialized_(false) {
}

UvgRTPReceiver::~UvgRTPReceiver() {
    stop();
}

bool UvgRTPReceiver::initialize() {
    try {
        // Create uvgRTP context
        ctx_ = std::make_unique<uvgrtp::context>();

        // Create session
        session_ = std::unique_ptr<uvgrtp::session>(ctx_->create_session(local_ip_));
        if (!session_) {
            std::cerr << "Failed to create uvgRTP session" << std::endl;
            return false;
        }

        // Create media stream for H.264 reception (bind to local port)
        int flags = RCE_RECEIVE_ONLY | RCE_FRAGMENT_GENERIC;
        stream_ = session_->create_stream(local_port_, RTP_FORMAT_H264, flags);
        if (!stream_) {
            std::cerr << "Failed to create uvgRTP media stream" << std::endl;
            return false;
        }

        // uvgRTP v3.1.6+ automatically enables defragmentation for H.264
        std::cout << "uvgRTP media stream created with automatic defragmentation" << std::endl;

        // Install hook to receive READY frames (already defragmented)
        if (stream_->install_receive_hook(this, frameReceiveHook) != RTP_OK) {
            std::cerr << "Failed to install receive hook" << std::endl;
            return false;
        }

        initialized_ = true;
        std::cout << "uvgRTP receiver initialized successfully on " 
                  << local_ip_ << ":" << local_port_ << std::endl;

        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception during uvgRTP initialization: " << e.what() << std::endl;
        return false;
    }
}

void UvgRTPReceiver::setFrameCallback(FrameCallback callback) {
    frame_callback_ = callback;
}

bool UvgRTPReceiver::start() {
    if (!initialized_) {
        std::cerr << "UvgRTPReceiver not initialized" << std::endl;
        return false;
    }

    if (running_) {
        std::cout << "UvgRTPReceiver already running" << std::endl;
        return true;
    }

    try {

        running_ = true;
        std::cout << "uvgRTP receiver started" << std::endl;
        return true;
    } catch (const std::exception& e) {
        std::cerr << "Exception during uvgRTP start: " << e.what() << std::endl;
        return false;
    }
}

void UvgRTPReceiver::stop() {
    if (!running_) {
        return;
    }

    running_ = false;

    try {
        if (session_ && stream_) {
            session_->destroy_stream(stream_);
            stream_ = nullptr;
        }

        session_.reset();
        ctx_.reset();

        std::cout << "uvgRTP receiver stopped" << std::endl;
    } catch (const std::exception& e) {
        std::cerr << "Exception during uvgRTP stop: " << e.what() << std::endl;
    }

    initialized_ = false;
}

void UvgRTPReceiver::frameReceiveHook(void* arg, uvgrtp::frame::rtp_frame* frame) {
    if (!arg || !frame) {
        return;
    }

    UvgRTPReceiver* receiver = static_cast<UvgRTPReceiver*>(arg);
    receiver->processFrame(frame);
}

void UvgRTPReceiver::processFrame(uvgrtp::frame::rtp_frame* frame) {
    if (!running_ || !frame_callback_) {
        // Deallocate frame if it's not going to be processed
        if (frame) {
            (void)uvgrtp::frame::dealloc_frame(frame);
        }
        return;
    }

    try {
        // Update statistics
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.packets_received++;
            stats_.bytes_received += frame->payload_len;
            stats_.frames_completed++;
        }

        // Create H264Frame and pass it forward
        auto h264_frame = std::make_unique<H264Frame>();
        h264_frame->timestamp = frame->header.timestamp;
        h264_frame->data.assign(frame->payload, frame->payload + frame->payload_len);

        std::cout << "✅ Frame received: " << h264_frame->data.size() 
                  << " bytes, timestamp: " << h264_frame->timestamp << std::endl;

        if (frame_callback_) {
            frame_callback_(std::move(h264_frame));
        }

    } catch (const std::exception& e) {
        std::cerr << "Exception in processFrame: " << e.what() << std::endl;
        std::lock_guard<std::mutex> lock(stats_mutex_);
        stats_.frames_dropped++;
    }
    
    // Always free frame memory after processing
    if (frame) {
        (void)uvgrtp::frame::dealloc_frame(frame);
    }
}

UvgRTPReceiver::Statistics UvgRTPReceiver::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void UvgRTPReceiver::resetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = Statistics{};
}
