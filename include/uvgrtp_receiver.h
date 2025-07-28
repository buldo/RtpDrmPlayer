/**
 * @file uvgrtp_receiver.h
 * @brief RTP приёмник на основе uvgRTP библиотеки с автоматической дефрагментацией
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
 * @brief Готовый H.264 кадр (уже дефрагментированный uvgRTP)
 */
struct H264Frame {
    std::vector<uint8_t> data;  // Полный кадр, готовый для декодирования
    uint32_t timestamp;
    std::chrono::steady_clock::time_point received_time;

    H264Frame() : timestamp(0) {
        received_time = std::chrono::steady_clock::now();
    }
};

/**
 * @brief RTP приёмник на основе uvgRTP с автоматической дефрагментацией
 * uvgRTP автоматически собирает фрагментированные RTP пакеты в полные H.264 кадры
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

    // Статистика
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

    // uvgRTP объекты
    std::unique_ptr<uvgrtp::context> ctx_;
    std::unique_ptr<uvgrtp::session> session_;
    uvgrtp::media_stream* stream_;

    // Сетевые параметры
    std::string local_ip_;
    uint16_t local_port_;

    // Состояние
    std::atomic<bool> running_;
    std::atomic<bool> initialized_;

    // Callback для готовых кадров
    FrameCallback frame_callback_;

    // Статистика
    mutable std::mutex stats_mutex_;
    Statistics stats_;
};
