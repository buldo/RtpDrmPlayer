/**
 * @file rtp_player.cpp
 * @brief RTP Player for receiving and decoding H.264 RTP stream in real-time
 */

#include "v4l2_decoder.h"
#include "config.h"
#include "uvgrtp_receiver.h"
#include <iostream>
#include <thread>
#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <queue>
#include <mutex>
#include <condition_variable>
#include <chrono>
#include <cstdint>
#include <sched.h>
#include <cstring>

class RTPPlayer {
public:
    RTPPlayer(const std::string& device_path, const std::string& local_ip, uint16_t local_port)
        : device_path_(device_path), local_ip_(local_ip), local_port_(local_port), 
          running_(false), decoded_frames_(0), has_sps_(false) {}

    ~RTPPlayer() {
        stop();
    }

    bool initialize() {
        // Create configuration
        DecoderConfig config;
        config.device_path = device_path_;
        // Other parameters remain default

        // Initialize V4L2 decoder
        decoder_ = std::make_unique<V4L2Decoder>();
        if (!decoder_->initialize(config)) {
            std::cerr << "Error initializing V4L2 decoder" << std::endl;
            return false;
        }

        // Configure display
        if (!decoder_->setDisplay()) {
            std::cerr << "Error configuring display" << std::endl;
            return false;
        }

        // Initialize RTP receiver
        rtp_receiver_ = std::make_unique<UvgRTPReceiver>(local_ip_, local_port_);
        if (!rtp_receiver_->initialize()) {
            std::cerr << "Error initializing RTP receiver" << std::endl;
            return false;
        }

        // Set callback for frame processing
        rtp_receiver_->setFrameCallback([this](std::unique_ptr<H264Frame> frame) {
            this->onFrameReceived(std::move(frame));
        });

        std::cout << "RTP Player initialized: " << local_ip_ << ":" << local_port_ << std::endl;
        return true;
    }

    void start() {
        if (!rtp_receiver_) {
            std::cerr << "RTP receiver not initialized" << std::endl;
            return;
        }

        running_ = true;
        
        // Start the decoding thread
        decoder_thread_ = std::thread(&RTPPlayer::decoderLoop, this);

        // Set real-time priority for the decoder thread
        sched_param sch_params;
        sch_params.sched_priority = sched_get_priority_max(SCHED_FIFO);
        if (pthread_setschedparam(decoder_thread_.native_handle(), SCHED_FIFO, &sch_params) != 0) {
            std::cerr << "⚠️ WARNING: Failed to set real-time priority for the decoder thread. "
                      << "Run with sudo for better performance. Error: " << strerror(errno) << std::endl;
        } else {
            std::cout << "✅ Real-time priority (SCHED_FIFO) set for the decoder thread." << std::endl;
        }
        
        // Start the RTP receiver
        if (!rtp_receiver_->start()) {
            std::cerr << "Error starting RTP receiver" << std::endl;
            running_ = false;
            return;
        }
        
        std::cout << "RTP Player started, waiting for H.264 data on " << local_ip_ << ":" << local_port_ << std::endl;
        std::cout << "Press Enter to stop..." << std::endl;
        std::cin.get();
        
        stop();
    }

    void stop() {
        running_ = false;
        
        // Stop the RTP receiver
        if (rtp_receiver_) {
            rtp_receiver_->stop();
        }
        
        // Signal the decoder to terminate
        frame_condition_.notify_all();
        
        if (decoder_thread_.joinable()) {
            decoder_thread_.join();
        }
        
        std::cout << "RTP Player stopped" << std::endl;
    }

    // Statistics
    int getDecodedFrames() const { return decoded_frames_; }

private:
    void onFrameReceived(std::unique_ptr<H264Frame> frame) {
        if (!frame || frame->data.empty()) {
            return;
        }

        // Check for SPS/PPS in the stream if not already found
        if (!has_sps_) {
            const auto& data = frame->data;
            for (size_t i = 0; i + 3 < data.size(); ) {
                size_t start_code_len = 0;
                if (i + 4 < data.size() && data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x00 && data[i+3] == 0x01) {
                    start_code_len = 4;
                } else if (data[i] == 0x00 && data[i+1] == 0x00 && data[i+2] == 0x01) {
                    start_code_len = 3;
                }

                if (start_code_len > 0) {
                    size_t nalu_header_pos = i + start_code_len;
                    if (nalu_header_pos < data.size()) {
                        uint8_t nalu_type = data[nalu_header_pos] & 0x1F;
                        if (nalu_type == 7) { // SPS
                            std::cout << "✅ SPS frame received (NALU type 7), decoder is ready to work!" << std::endl;
                            has_sps_ = true;
                            break; 
                        }
                    }
                    i += start_code_len;
                } else {
                    i++;
                }
            }
        }

        {
            std::lock_guard<std::mutex> lock(frame_mutex_);
            if (frame_queue_.size() >= MAX_QUEUE_SIZE) {
                frame_queue_.pop(); // Remove the oldest frame if the queue is full
            }
            frame_queue_.push(std::move(frame));
        }
        
        frame_condition_.notify_one();
    }

    void decoderLoop() {
        std::cout << "Starting decoding loop with buffering (queue size: " << MAX_QUEUE_SIZE << ")..." << std::endl;
        
        // Wait for the first SPS frame
        while (running_ && !has_sps_) {
            std::cout << "⏳ Waiting for SPS frame..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (running_) {
            std::unique_ptr<H264Frame> frame_to_decode;
            
            // Wait for a new frame
            {
                std::unique_lock<std::mutex> lock(frame_mutex_);
                frame_condition_.wait(lock, [this] { return !frame_queue_.empty() || !running_; });
                
                if (!running_ && frame_queue_.empty()) {
                    break;
                }
                
                // Take a frame from the queue
                frame_to_decode = std::move(frame_queue_.front());
                frame_queue_.pop();
            }
            
            if (!frame_to_decode) {
                continue;
            }
            
            try {
                if (decoder_->decodeData(frame_to_decode->data.data(), frame_to_decode->data.size())) {
                    decoded_frames_++;
                    
                    if (decoded_frames_ == 1) {
                         std::cout << "✅ First frame successfully decoded and displayed!" << std::endl;
                    } else if (decoded_frames_ % 100 == 0) {
                        std::cout << "✅ Decoded " << decoded_frames_ << " frames" << std::endl;
                    }
                } else {
                    std::cout << "❌ Error decoding frame (" << frame_to_decode->data.size() << " bytes)" << std::endl;
                }
                
            } catch (const std::exception& e) {
                std::cerr << "Critical error in decoding loop: " << e.what() << std::endl;
            }
        }
        
        std::cout << "Decoding loop finished" << std::endl;
    }
    
    // Configuration
    std::string device_path_;
    std::string local_ip_;
    uint16_t local_port_;
    
    // Components
    std::unique_ptr<V4L2Decoder> decoder_;
    std::unique_ptr<UvgRTPReceiver> rtp_receiver_;
    
    // Threads
    std::thread decoder_thread_;
    std::atomic<bool> running_;
    
    // Frame queue with limited size for smoothing
    static constexpr size_t MAX_QUEUE_SIZE = 5;
    std::queue<std::unique_ptr<H264Frame>> frame_queue_;
    std::mutex frame_mutex_;
    std::condition_variable frame_condition_;
    
    // Statistics
    std::atomic<int> decoded_frames_;
    std::atomic<bool> has_sps_;
};

void printUsage(const char* program_name) {
    std::cout << "RTP Player - real-time H.264 RTP stream reception and decoding\n\n";
    std::cout << "Usage: " << program_name << " [options]\n\n";
    std::cout << "Options:\n";
    std::cout << "  -d, --device <device>   V4L2 device (default: /dev/video10)\n";
    std::cout << "  -i, --ip <ip>          Local IP to listen on (default: 0.0.0.0)\n";
    std::cout << "  -p, --port <port>      Local port for RTP (default: 5600)\n";
    std::cout << "  -h, --help             Show this help\n\n";
    std::cout << "Examples:\n";
    std::cout << "  " << program_name << " -p 5600                    # Listen on port 5600\n";
    std::cout << "  " << program_name << " -i 192.168.1.100 -p 8080  # Listen on a specific IP and port\n";
}

int main(int argc, char* argv[]) {
    std::string device_path = "/dev/video10";
    std::string local_ip = "0.0.0.0";
    uint16_t local_port = 5600;
    
    // Parse command line arguments
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if ((arg == "-h") || (arg == "--help")) {
            printUsage(argv[0]);
            return 0;
        }
        else if ((arg == "-d") || (arg == "--device")) {
            if (i + 1 < argc) {
                device_path = argv[++i];
            } else {
                std::cerr << "Error: option " << arg << " requires a value\n";
                return 1;
            }
        }
        else if ((arg == "-i") || (arg == "--ip")) {
            if (i + 1 < argc) {
                local_ip = argv[++i];
            } else {
                std::cerr << "Error: option " << arg << " requires a value\n";
                return 1;
            }
        }
        else if ((arg == "-p") || (arg == "--port")) {
            if (i + 1 < argc) {
                local_port = static_cast<uint16_t>(std::stoi(argv[++i]));
            } else {
                std::cerr << "Error: option " << arg << " requires a value\n";
                return 1;
            }
        }
        else {
            std::cerr << "Unknown option: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    
    std::cout << "\n=== RTP Player for H.264 stream ===" << std::endl;
    std::cout << "V4L2 device: " << device_path << std::endl;
    std::cout << "Listening for RTP on: " << local_ip << ":" << local_port << std::endl;
    std::cout << "=====================================" << std::endl << std::endl;
    
    try {
        RTPPlayer player(device_path, local_ip, local_port);
        
        if (!player.initialize()) {
            std::cerr << "RTP Player initialization failed" << std::endl;
            return 1;
        }
        
        player.start();
        
        std::cout << "Decoded frames: " << player.getDecodedFrames() << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
