/**
 * @file rtp_receiver.cpp
 * @brief Implementation of RTP receiver with H.264 depayloading
 */

#include "rtp_receiver.h"
#include <iostream>
#include <cstring>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <unistd.h>
#include <errno.h>
#include <fcntl.h>
#include <chrono>
#include <thread>

RTPReceiver::RTPReceiver(const std::string& local_ip, uint16_t local_port)
    : local_ip_(local_ip), local_port_(local_port), socket_fd_(-1),
      running_(false), frame_started_(false), current_timestamp_(0),
      expected_sequence_(0), fragment_started_(false),
      fragment_nalu_type_(0), fragment_nri_(0),
      sps_received_(false), waiting_for_idr_(true) {
}

RTPReceiver::~RTPReceiver() {
    stop();
}

bool RTPReceiver::initialize() {
    // Create UDP socket
    socket_fd_ = socket(AF_INET, SOCK_DGRAM, 0);
    if (socket_fd_ < 0) {
        std::cerr << "❌ Socket creation error: " << strerror(errno) << std::endl;
        return false;
    }
    
    // Allow address reuse
    int opt = 1;
    if (setsockopt(socket_fd_, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0) {
        std::cerr << "⚠️ Warning: failed to set SO_REUSEADDR" << std::endl;
    }
    
    // Configure address for binding
    struct sockaddr_in server_addr;
    memset(&server_addr, 0, sizeof(server_addr));
    server_addr.sin_family = AF_INET;
    server_addr.sin_port = htons(local_port_);
    
    if (local_ip_ == "0.0.0.0") {
        server_addr.sin_addr.s_addr = INADDR_ANY;
    } else {
        if (inet_pton(AF_INET, local_ip_.c_str(), &server_addr.sin_addr) <= 0) {
            std::cerr << "❌ Invalid IP address: " << local_ip_ << std::endl;
            close(socket_fd_);
            socket_fd_ = -1;
            return false;
        }
    }
    
    // Bind socket
    if (bind(socket_fd_, (struct sockaddr*)&server_addr, sizeof(server_addr)) < 0) {
        std::cerr << "❌ Socket bind error to " << local_ip_ << ":" << local_port_ 
                  << " - " << strerror(errno) << std::endl;
        close(socket_fd_);
        socket_fd_ = -1;
        return false;
    }
    
    std::cout << "✅ RTP socket initialized: " << local_ip_ << ":" << local_port_ << std::endl;
    return true;
}

void RTPReceiver::setFrameCallback(FrameCallback callback) {
    frame_callback_ = callback;
}

bool RTPReceiver::start() {
    if (socket_fd_ < 0) {
        std::cerr << "❌ Socket not initialized" << std::endl;
        return false;
    }
    
    if (running_) {
        std::cout << "⚠️ RTP receiver already running" << std::endl;
        return true;
    }
    
    running_ = true;
    receive_thread_ = std::thread(&RTPReceiver::receiveLoop, this);
    
    std::cout << "🚀 RTP receiver started" << std::endl;
    return true;
}

void RTPReceiver::stop() {
    if (!running_) {
        return;
    }
    
    running_ = false;
    
    if (receive_thread_.joinable()) {
        receive_thread_.join();
    }
    
    if (socket_fd_ >= 0) {
        close(socket_fd_);
        socket_fd_ = -1;
    }
    
    std::cout << "🛑 RTP receiver stopped" << std::endl;
}

void RTPReceiver::receiveLoop() {
    uint8_t buffer[MAX_PACKET_SIZE];
    struct sockaddr_in client_addr;
    socklen_t client_len = sizeof(client_addr);
    
    std::cout << "📡 Starting RTP packet reception..." << std::endl;
    
    // Variables for rate control
    auto last_frame_time = std::chrono::steady_clock::now();
    int packets_in_current_frame = 0;
    const int MAX_PACKETS_PER_FRAME = 100; // Maximum packets per frame
    const auto FRAME_INTERVAL = std::chrono::milliseconds(33); // ~30 FPS
    
    while (running_) {
        ssize_t received = recvfrom(socket_fd_, buffer, MAX_PACKET_SIZE, 0,
                                   (struct sockaddr*)&client_addr, &client_len);
        
        if (received < 0) {
            if (errno == EINTR || errno == EAGAIN) {
                continue;
            }
            std::cerr << "❌ Packet reception error: " << strerror(errno) << std::endl;
            break;
        }
        
        if (received == 0) {
            continue;
        }
        
        // Reception rate control to prevent decoder overload
        auto now = std::chrono::steady_clock::now();
        if (now - last_frame_time >= FRAME_INTERVAL) {
            packets_in_current_frame = 0;
            last_frame_time = now;
        }
        
        packets_in_current_frame++;
        if (packets_in_current_frame > MAX_PACKETS_PER_FRAME) {
            // Слишком много packets - делаем небольшую паузу
            std::this_thread::sleep_for(std::chrono::microseconds(100));
        }
        
        // Обрабатываем RTP пакет
        if (!processRTPPacket(buffer, received)) {
            std::cerr << "⚠️ Ошибка обработки RTP пакета" << std::endl;
        }
        
        // Обновляем статистику
        {
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.packets_received++;
            stats_.bytes_received += received;
        }
    }
    
    std::cout << "📡 Цикл приёма RTP завершён" << std::endl;
}

bool RTPReceiver::processRTPPacket(const uint8_t* data, size_t size) {
    if (size < sizeof(RTPHeader)) {
        return false;
    }
    
    // Парсим RTP заголовок
    const RTPHeader* rtp_header = reinterpret_cast<const RTPHeader*>(data);
    
    // Конвертируем из network byte order
    uint16_t sequence = ntohs(rtp_header->sequence_number);
    uint32_t timestamp = ntohl(rtp_header->timestamp);
    
    // Проверяем версию RTP
    if (rtp_header->getVersion() != 2) {
        return false;
    }
    
    // Проверяем payload type (должен быть H.264)
    uint8_t payload_type = rtp_header->getPayloadType();
    if (payload_type != H264_PAYLOAD_TYPE && payload_type != 97 && payload_type != 98) {
        // Принимаем распространённые динамические типы для H.264
        return false;
    }
    
    // Вычисляем payload size
    size_t header_size = sizeof(RTPHeader);
    header_size += rtp_header->getCSRCCount() * 4; // CSRC список
    
    if (rtp_header->hasExtension()) {
        if (size < header_size + 4) return false;
        const uint16_t* ext_header = reinterpret_cast<const uint16_t*>(data + header_size + 2);
        header_size += 4 + ntohs(*ext_header) * 4;
    }
    
    if (size <= header_size) {
        return false;
    }
    
    const uint8_t* payload = data + header_size;
    size_t payload_size = size - header_size;
    
    // Обрабатываем H.264 payload
    return processH264Payload(*rtp_header, payload, payload_size);
}

bool RTPReceiver::processH264Payload(const RTPHeader& rtp_header, const uint8_t* payload, size_t payload_size) {
    if (payload_size == 0) {
        return false;
    }
    
    // Парсим NAL Unit заголовок
    const H264NALUHeader* nalu_header = reinterpret_cast<const H264NALUHeader*>(payload);
    uint8_t nalu_type = nalu_header->getType();
    
    uint32_t timestamp = ntohl(rtp_header.timestamp);
    uint16_t sequence = ntohs(rtp_header.sequence_number);
    
    std::cout << "📦 RTP seq=" << sequence << ", ts=" << timestamp 
              << ", " << getNALUTypeName(nalu_type) << " (" << payload_size << " bytes)" << std::endl;
    
    // Для FU-A packets нужно извлечь реальный тип NALU
    uint8_t actual_nalu_type = nalu_type;
    if (nalu_type == NALU_TYPE_FU_A && payload_size >= 2) {
        const H264FUHeader* fu_header = reinterpret_cast<const H264FUHeader*>(payload + 1);
        actual_nalu_type = fu_header->getType();
    }
    
    // Отслеживаем состояние потока для правильного декодирования
    if (actual_nalu_type == NALU_TYPE_SPS) {
        sps_received_ = true;
        waiting_for_idr_ = true; // После SPS всегда ждём I-кадр
        std::cout << "🔧 Получен SPS - ожидаем I-кадр для начала декодирования" << std::endl;
    } else if (actual_nalu_type == NALU_TYPE_IDR && sps_received_) {
        waiting_for_idr_ = false;
        std::cout << "🚀 Получен I-кадр после SPS - готовы к декодированию!" << std::endl;
    }
    
    // Пропускаем кадры до receivedия SPS+I-frame последовательности
    if (waiting_for_idr_ && actual_nalu_type != NALU_TYPE_SPS && actual_nalu_type != NALU_TYPE_PPS) {
        std::cout << "⏭️ Пропуск до SPS+I-frame" << std::endl;
        return true; // Пакет обработан, но кадр не собираем
    }
    
    // Обрабатываем в зависимости от типа NAL Unit
    switch (nalu_type) {
        case NALU_TYPE_STAP_A:
            // Aggregated packets
            handleAggregatedNALUs(rtp_header, payload, payload_size);
            break;
            
        case NALU_TYPE_FU_A:
            // Fragmented packets
            handleFragmentedNALU(rtp_header, payload, payload_size);
            break;
            
        case NALU_TYPE_SLICE:
        case NALU_TYPE_IDR:
        case NALU_TYPE_SPS:
        case NALU_TYPE_PPS:
        case NALU_TYPE_SEI:
        case NALU_TYPE_AUD:
            // Single NALU
            handleSingleNALU(rtp_header, payload, payload_size);
            break;
            
        default:
            // Неизвестный тип, обрабатываем как single NALU
            handleSingleNALU(rtp_header, payload, payload_size);
            break;
    }
    
    // Проверяем завершение кадра по marker bit ИЛИ по смене timestamp
    bool should_complete_frame = rtp_header.getMarker();
    
    // Дополнительная логика завершения кадра по смене timestamp для всех типов packets
    if (!should_complete_frame && frame_started_ && current_timestamp_ != 0 && current_timestamp_ != timestamp) {
        std::cout << "🔄 Принудительное завершение кадра: смена timestamp " 
                  << current_timestamp_ << " -> " << timestamp << std::endl;
        should_complete_frame = true;
    }
    
    if (should_complete_frame) {
        auto frame = completeFrame();
        if (frame && frame_callback_) {
            uint32_t completed_timestamp = frame->timestamp;  // Сохраняем timestamp перед move
            frame_callback_(std::move(frame));
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.frames_completed++;
            std::cout << "✅ Кадр завершен и отправлен в декодер (timestamp=" << completed_timestamp << ")" << std::endl;
        }
    }
    
    return true;
}

void RTPReceiver::handleSingleNALU(const RTPHeader& rtp_header, const uint8_t* nalu_data, size_t nalu_size) {
    uint32_t timestamp = ntohl(rtp_header.timestamp);
    
    // Если это new кадр, завершаем предыдущий
    if (frame_started_ && current_timestamp_ != timestamp) {
        auto frame = completeFrame();
        if (frame && frame_callback_) {
            frame_callback_(std::move(frame));
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.frames_completed++;
        }
    }
    
    // Начинаем new кадр если нужно
    if (!frame_started_ || current_timestamp_ != timestamp) {
        frame_buffer_.clear();
        frame_started_ = true;
        current_timestamp_ = timestamp;
    }
    
    // Добавляем NAL Unit в кадр
    addNALUToFrame(nalu_data, nalu_size);
}

void RTPReceiver::handleFragmentedNALU(const RTPHeader& rtp_header, const uint8_t* payload, size_t payload_size) {
    if (payload_size < 2) {
        return;
    }
    
    const H264NALUHeader* nalu_header = reinterpret_cast<const H264NALUHeader*>(payload);
    const H264FUHeader* fu_header = reinterpret_cast<const H264FUHeader*>(payload + 1);
    
    uint32_t timestamp = ntohl(rtp_header.timestamp);
    bool start = fu_header->getStartBit();
    bool end = fu_header->getEndBit();
    uint8_t nalu_type = fu_header->getType();
    uint8_t nri = nalu_header->getNRI();
    
    if (start) {
        // Начало фрагментированного NAL Unit
        if (frame_started_ && current_timestamp_ != timestamp) {
            // Завершаем предыдущий кадр
            auto frame = completeFrame();
            if (frame && frame_callback_) {
                frame_callback_(std::move(frame));
                std::lock_guard<std::mutex> lock(stats_mutex_);
                stats_.frames_completed++;
            }
        }
        
        if (!frame_started_ || current_timestamp_ != timestamp) {
            frame_buffer_.clear();
            frame_started_ = true;
            current_timestamp_ = timestamp;
        }
        
        fragment_started_ = true;
        fragment_nalu_type_ = nalu_type;
        fragment_nri_ = nri;
        
        // Добавляем start code и восстановленный NAL Unit заголовок
        const uint8_t start_code[] = {0x00, 0x00, 0x00, 0x01};
        frame_buffer_.insert(frame_buffer_.end(), start_code, start_code + 4);
        
        uint8_t reconstructed_header = (nri << 5) | nalu_type;
        frame_buffer_.push_back(reconstructed_header);
        
        // Добавляем payload без FU заголовков
        frame_buffer_.insert(frame_buffer_.end(), payload + 2, payload + payload_size);
        
    } else if (fragment_started_ && fragment_nalu_type_ == nalu_type) {
        // Продолжение фрагментированного NAL Unit
        frame_buffer_.insert(frame_buffer_.end(), payload + 2, payload + payload_size);
    }
    
    if (end) {
        fragment_started_ = false;
    }
}

void RTPReceiver::handleAggregatedNALUs(const RTPHeader& rtp_header, const uint8_t* payload, size_t payload_size) {
    if (payload_size < 1) {
        return;
    }
    
    uint32_t timestamp = ntohl(rtp_header.timestamp);
    
    // Если это new кадр, завершаем предыдущий
    if (frame_started_ && current_timestamp_ != timestamp) {
        auto frame = completeFrame();
        if (frame && frame_callback_) {
            frame_callback_(std::move(frame));
            std::lock_guard<std::mutex> lock(stats_mutex_);
            stats_.frames_completed++;
        }
    }
    
    if (!frame_started_ || current_timestamp_ != timestamp) {
        frame_buffer_.clear();
        frame_started_ = true;
        current_timestamp_ = timestamp;
    }
    
    // Пропускаем STAP-A заголовок
    size_t offset = 1;
    
    // Обрабатываем каждый NAL Unit в пакете
    while (offset + 2 < payload_size) {
        // Читаем size NAL Unit (2 bytesа, network byte order)
        uint16_t nalu_size = ntohs(*reinterpret_cast<const uint16_t*>(payload + offset));
        offset += 2;
        
        if (offset + nalu_size > payload_size) {
            break;
        }
        
        // Добавляем NAL Unit в кадр
        addNALUToFrame(payload + offset, nalu_size);
        
        offset += nalu_size;
    }
}

void RTPReceiver::addNALUToFrame(const uint8_t* nalu_data, size_t nalu_size) {
    if (nalu_size == 0 || frame_buffer_.size() + nalu_size + 4 > MAX_FRAME_SIZE) {
        return;
    }
    
    // Добавляем start code
    const uint8_t start_code[] = {0x00, 0x00, 0x00, 0x01};
    frame_buffer_.insert(frame_buffer_.end(), start_code, start_code + 4);
    
    // Добавляем NAL Unit
    frame_buffer_.insert(frame_buffer_.end(), nalu_data, nalu_data + nalu_size);
}

std::unique_ptr<H264Frame> RTPReceiver::completeFrame() {
    if (!frame_started_ || frame_buffer_.empty()) {
        return nullptr;
    }
    
    auto frame = std::make_unique<H264Frame>();
    frame->data = std::move(frame_buffer_);
    frame->timestamp = current_timestamp_;
    
    frame_buffer_.clear();
    frame_started_ = false;
    fragment_started_ = false;
    
    return frame;
}

std::string RTPReceiver::getNALUTypeName(uint8_t type) const {
    switch (type) {
        case NALU_TYPE_SLICE: return "P-frame";
        case NALU_TYPE_IDR: return "I-frame";
        case NALU_TYPE_SPS: return "SPS";
        case NALU_TYPE_PPS: return "PPS";
        case NALU_TYPE_SEI: return "SEI";
        case NALU_TYPE_AUD: return "AUD";
        case NALU_TYPE_STAP_A: return "STAP-A";
        case NALU_TYPE_FU_A: return "FU-A";
        default: return "NALU-" + std::to_string(type);
    }
}

RTPReceiver::Statistics RTPReceiver::getStatistics() const {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    return stats_;
}

void RTPReceiver::resetStatistics() {
    std::lock_guard<std::mutex> lock(stats_mutex_);
    stats_ = Statistics{};
}
