/**
 * @file rtp_receiver.h
 * @brief Custom RTP receiver implementation with H.264 depayloading
 */

#pragma once

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
 * @brief RTP header (RFC 3550)
 */
struct RTPHeader {
    uint8_t version_p_x_cc;     // Version(2) + Padding(1) + Extension(1) + CSRC Count(4)
    uint8_t m_pt;               // Marker(1) + Payload Type(7)
    uint16_t sequence_number;    // Sequence number
    uint32_t timestamp;         // Timestamp
    uint32_t ssrc;              // Synchronization source identifier
    
    // Methods for extracting fields
    uint8_t getVersion() const { return (version_p_x_cc >> 6) & 0x03; }
    bool hasPadding() const { return (version_p_x_cc >> 5) & 0x01; }
    bool hasExtension() const { return (version_p_x_cc >> 4) & 0x01; }
    uint8_t getCSRCCount() const { return version_p_x_cc & 0x0F; }
    bool getMarker() const { return (m_pt >> 7) & 0x01; }
    uint8_t getPayloadType() const { return m_pt & 0x7F; }
} __attribute__((packed));

/**
 * @brief H.264 NAL Unit header (RFC 6184)
 */
struct H264NALUHeader {
    uint8_t f_nri_type;         // F(1) + NRI(2) + Type(5)
    
    bool getFBit() const { return (f_nri_type >> 7) & 0x01; }
    uint8_t getNRI() const { return (f_nri_type >> 5) & 0x03; }
    uint8_t getType() const { return f_nri_type & 0x1F; }
} __attribute__((packed));

/**
 * @brief H.264 FU-A header for fragmented NAL units
 */
struct H264FUHeader {
    uint8_t s_e_r_type;         // S(1) + E(1) + R(1) + Type(5)
    
    bool getStartBit() const { return (s_e_r_type >> 7) & 0x01; }
    bool getEndBit() const { return (s_e_r_type >> 6) & 0x01; }
    bool getReservedBit() const { return (s_e_r_type >> 5) & 0x01; }
    uint8_t getType() const { return s_e_r_type & 0x1F; }
} __attribute__((packed));

/**
 * @brief H.264 NAL Unit types
 */
enum H264NALUnitType {
    NALU_TYPE_UNSPECIFIED = 0,
    NALU_TYPE_SLICE = 1,        // P-frame slice
    NALU_TYPE_DPA = 2,
    NALU_TYPE_DPB = 3,
    NALU_TYPE_DPC = 4,
    NALU_TYPE_IDR = 5,          // I-frame slice
    NALU_TYPE_SEI = 6,          // Supplemental Enhancement Information
    NALU_TYPE_SPS = 7,          // Sequence Parameter Set
    NALU_TYPE_PPS = 8,          // Picture Parameter Set
    NALU_TYPE_AUD = 9,          // Access Unit Delimiter
    NALU_TYPE_EOSEQ = 10,
    NALU_TYPE_EOSTREAM = 11,
    NALU_TYPE_FILL = 12,
    // 13-23: Reserved
    NALU_TYPE_STAP_A = 24,      // Single-time aggregation packet
    NALU_TYPE_STAP_B = 25,
    NALU_TYPE_MTAP16 = 26,
    NALU_TYPE_MTAP24 = 27,
    NALU_TYPE_FU_A = 28,        // Fragmentation unit A
    NALU_TYPE_FU_B = 29         // Fragmentation unit B
};

/**
 * @brief Assembled H.264 frame
 */
struct H264Frame {
    std::vector<uint8_t> data;
    uint32_t timestamp;
    uint16_t sequence_start;
    uint16_t sequence_end;
    std::chrono::steady_clock::time_point received_time;
    
    H264Frame() : timestamp(0), sequence_start(0), sequence_end(0) {
        received_time = std::chrono::steady_clock::now();
    }
};

/**
 * @brief RTP receiver with H.264 depayloading
 */
class RTPReceiver {
public:
    using FrameCallback = std::function<void(std::unique_ptr<H264Frame>)>;
    
    RTPReceiver(const std::string& local_ip = "0.0.0.0", uint16_t local_port = 5600);
    ~RTPReceiver();
    
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
    void receiveLoop();
    bool processRTPPacket(const uint8_t* data, size_t size);
    bool processH264Payload(const RTPHeader& rtp_header, const uint8_t* payload, size_t payload_size);
    
    void handleSingleNALU(const RTPHeader& rtp_header, const uint8_t* nalu_data, size_t nalu_size);
    void handleFragmentedNALU(const RTPHeader& rtp_header, const uint8_t* payload, size_t payload_size);
    void handleAggregatedNALUs(const RTPHeader& rtp_header, const uint8_t* payload, size_t payload_size);
    
    std::unique_ptr<H264Frame> completeFrame();
    void addNALUToFrame(const uint8_t* nalu_data, size_t nalu_size);
    
    std::string getNALUTypeName(uint8_t type) const;
    
    // Network parameters
    std::string local_ip_;
    uint16_t local_port_;
    int socket_fd_;
    
    // Receive thread
    std::thread receive_thread_;
    std::atomic<bool> running_;
    
    // Callback for completed frames
    FrameCallback frame_callback_;
    
    // Buffer for frame assembly
    std::vector<uint8_t> frame_buffer_;
    bool frame_started_;
    uint32_t current_timestamp_;
    uint16_t expected_sequence_;
    
    // Fragmentation state
    bool fragment_started_;
    uint8_t fragment_nalu_type_;
    uint8_t fragment_nri_;
    
    // Stream state for proper decoding
    bool sps_received_;     // SPS received
    bool waiting_for_idr_;  // Waiting for I-frame after SPS
    
    // Statistics
    mutable std::mutex stats_mutex_;
    Statistics stats_;
    
    // Constants
    static constexpr size_t MAX_PACKET_SIZE = 2048;
    static constexpr size_t MAX_FRAME_SIZE = 1024 * 1024; // 1MB
    static constexpr uint8_t H264_PAYLOAD_TYPE = 96; // Standard dynamic type for H.264
};
