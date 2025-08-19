/**
 * @file uvgrtp_receiver.h
 * @brief RTP receiver based on the uvgRTP library with automatic defragmentation
 */

#pragma once

#include <uvgrtp/lib.hh>
#include <cstdint>
#include <vector>
#include <memory>
#include <functional>
#include <thread>
#include <atomic>
#include <mutex>
#include <string>
#include <chrono>

/**
 * @brief A complete H.264 frame (already defragmented by uvgRTP)
 */
struct H264Frame {
    std::vector<uint8_t> data;  // Full frame, ready for decoding
    uint32_t timestamp;
    std::chrono::steady_clock::time_point received_time;

    H264Frame() : timestamp(0) {
        received_time = std::chrono::steady_clock::now();
    }
};

/**
 * @brief RTP receiver based on uvgRTP with automatic defragmentation.
 * uvgRTP automatically reassembles fragmented RTP packets into complete H.264 frames.
 */
class UvgRTPReceiver {
public:
    using FrameCallback = std::function<void(std::unique_ptr<H264Frame>)>;

    UvgRTPReceiver(const std::string& local_ip = "0.0.0.0", uint16_t local_port = 5600);
    ~UvgRTPReceiver();

    bool initialize();
    void setFrameCallback(FrameCallback callback);

    bool start();
    void stop();

    bool isRunning() const { return running_; }

    // Statistics
    struct Statistics {
        uint64_t packets_received = 0;
        uint64_t bytes_received = 0;
        uint64_t frames_completed = 0;
        uint64_t packets_lost = 0;
        uint64_t frames_dropped = 0;
    };

    Statistics getStatistics() const;
    void resetStatistics();

private:
    static void frameReceiveHook(void* arg, uvgrtp::frame::rtp_frame* frame);
    void processFrame(uvgrtp::frame::rtp_frame* frame);

    // uvgRTP objects
    std::unique_ptr<uvgrtp::context> ctx_;
    std::unique_ptr<uvgrtp::session> session_;
    uvgrtp::media_stream* stream_;

    // Network parameters
    std::string local_ip_;
    uint16_t local_port_;

    // State
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;

    // Callback for complete frames
    FrameCallback frame_callback_;

    // Statistics
    mutable std::mutex stats_mutex_;
    Statistics stats_;
};
