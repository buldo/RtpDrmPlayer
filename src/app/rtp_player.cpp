/**
 * @file rtp_player.cpp
 * @brief RTP Player для приема и декодирования H.264 RTP потока в реальном времени
 */

#include "v4l2_decoder.h"
#include "config.h"
#include "display_manager.h"
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
        // Создаем конфигурацию
        DecoderConfig config;
        config.device_path = device_path_;
        // Другие параметры остаются по умолчанию

        // Инициализируем V4L2 декодер
        decoder_ = std::make_unique<V4L2Decoder>();
        if (!decoder_->initialize(config)) {
            std::cerr << "Ошибка инициализации V4L2 декодера" << std::endl;
            return false;
        }

        // Настраиваем отображение
        if (!decoder_->setDisplay(V4L2Decoder::DisplayType::DRM_DMABUF)) {
            std::cerr << "Ошибка настройки дисплея" << std::endl;
            return false;
        }

        // Инициализируем RTP приёмник
        rtp_receiver_ = std::make_unique<UvgRTPReceiver>(local_ip_, local_port_);
        if (!rtp_receiver_->initialize()) {
            std::cerr << "Ошибка инициализации RTP приёмника" << std::endl;
            return false;
        }

        // Устанавливаем callback для обработки кадров
        rtp_receiver_->setFrameCallback([this](std::unique_ptr<H264Frame> frame) {
            this->onFrameReceived(std::move(frame));
        });

        std::cout << "RTP Player инициализирован: " << local_ip_ << ":" << local_port_ << std::endl;
        return true;
    }

    void start() {
        if (!rtp_receiver_) {
            std::cerr << "RTP приёмник не инициализирован" << std::endl;
            return;
        }

        running_ = true;
        
        // Запускаем поток декодирования
        decoder_thread_ = std::thread(&RTPPlayer::decoderLoop, this);

        // Устанавливаем real-time приоритет для потока декодера
        sched_param sch_params;
        sch_params.sched_priority = sched_get_priority_max(SCHED_FIFO);
        if (pthread_setschedparam(decoder_thread_.native_handle(), SCHED_FIFO, &sch_params) != 0) {
            std::cerr << "⚠️ ПРЕДУПРЕЖДЕНИЕ: Не удалось установить real-time приоритет для потока декодера. "
                      << "Запустите с sudo для лучшей производительности. Ошибка: " << strerror(errno) << std::endl;
        } else {
            std::cout << "✅ Установлен real-time приоритет (SCHED_FIFO) для потока декодера." << std::endl;
        }
        
        // Запускаем RTP приёмник
        if (!rtp_receiver_->start()) {
            std::cerr << "Ошибка запуска RTP приёмника" << std::endl;
            running_ = false;
            return;
        }
        
        std::cout << "RTP Player запущен, ожидание H.264 данных на " << local_ip_ << ":" << local_port_ << std::endl;
        std::cout << "Нажмите Enter для остановки..." << std::endl;
        std::cin.get();
        
        stop();
    }

    void stop() {
        running_ = false;
        
        // Останавливаем RTP приёмник
        if (rtp_receiver_) {
            rtp_receiver_->stop();
        }
        
        // Сигнализируем декодеру о завершении
        frame_condition_.notify_all();
        
        if (decoder_thread_.joinable()) {
            decoder_thread_.join();
        }
        
        std::cout << "RTP Player остановлен" << std::endl;
    }

    // Статистика
    int getDecodedFrames() const { return decoded_frames_; }

private:
    void onFrameReceived(std::unique_ptr<H264Frame> frame) {
        if (!frame || frame->data.empty()) {
            return;
        }

        // Проверяем наличие SPS/PPS в потоке, если еще не было
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
                            std::cout << "✅ SPS кадр получен (NALU type 7), декодер готов к работе!" << std::endl;
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
                frame_queue_.pop(); // Удаляем самый старый кадр, если очередь заполнена
            }
            frame_queue_.push(std::move(frame));
        }
        
        frame_condition_.notify_one();
    }

    void decoderLoop() {
        std::cout << "Запуск цикла декодирования с буферизацией (размер очереди: " << MAX_QUEUE_SIZE << ")..." << std::endl;
        
        // Ожидаем первый SPS кадр
        while (running_ && !has_sps_) {
            std::cout << "⏳ Ожидание SPS кадра..." << std::endl;
            std::this_thread::sleep_for(std::chrono::seconds(1));
        }

        while (running_) {
            std::unique_ptr<H264Frame> frame_to_decode;
            
            // Ждем новый кадр
            {
                std::unique_lock<std::mutex> lock(frame_mutex_);
                frame_condition_.wait(lock, [this] { return !frame_queue_.empty() || !running_; });
                
                if (!running_ && frame_queue_.empty()) {
                    break;
                }
                
                // Забираем кадр из очереди
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
                         std::cout << "✅ Первый кадр успешно декодирован и отображен!" << std::endl;
                    } else if (decoded_frames_ % 100 == 0) {
                        std::cout << "✅ Декодировано " << decoded_frames_ << " кадров" << std::endl;
                    }
                } else {
                    std::cout << "❌ Ошибка декодирования кадра (" << frame_to_decode->data.size() << " байт)" << std::endl;
                }
                
            } catch (const std::exception& e) {
                std::cerr << "Критическая ошибка в цикле декодирования: " << e.what() << std::endl;
            }
        }
        
        std::cout << "Цикл декодирования завершен" << std::endl;
    }
    
    // Конфигурация
    std::string device_path_;
    std::string local_ip_;
    uint16_t local_port_;
    
    // Компоненты
    std::unique_ptr<V4L2Decoder> decoder_;
    std::unique_ptr<UvgRTPReceiver> rtp_receiver_;
    
    // Потоки
    std::thread decoder_thread_;
    std::atomic<bool> running_;
    
    // Очередь кадров с ограниченным размером для сглаживания
    static constexpr size_t MAX_QUEUE_SIZE = 5;
    std::queue<std::unique_ptr<H264Frame>> frame_queue_;
    std::mutex frame_mutex_;
    std::condition_variable frame_condition_;
    
    // Статистика
    std::atomic<int> decoded_frames_;
    std::atomic<bool> has_sps_;
};

void printUsage(const char* program_name) {
    std::cout << "RTP Player - прием и декодирование H.264 RTP потока в реальном времени\n\n";
    std::cout << "Использование: " << program_name << " [опции]\n\n";
    std::cout << "Опции:\n";
    std::cout << "  -d, --device <device>   V4L2 устройство (по умолчанию: /dev/video10)\n";
    std::cout << "  -i, --ip <ip>          Локальный IP для прослушивания (по умолчанию: 0.0.0.0)\n";
    std::cout << "  -p, --port <port>      Локальный порт для RTP (по умолчанию: 5600)\n";
    std::cout << "  -h, --help             Показать эту справку\n\n";
    std::cout << "Примеры:\n";
    std::cout << "  " << program_name << " -p 5600                    # Слушать на порту 5600\n";
    std::cout << "  " << program_name << " -i 192.168.1.100 -p 8080  # Слушать на конкретном IP и порту\n";
}

int main(int argc, char* argv[]) {
    std::string device_path = "/dev/video10";
    std::string local_ip = "0.0.0.0";
    uint16_t local_port = 5600;
    
    // Парсинг аргументов командной строки
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
                std::cerr << "Ошибка: опция " << arg << " требует значения\n";
                return 1;
            }
        }
        else if ((arg == "-i") || (arg == "--ip")) {
            if (i + 1 < argc) {
                local_ip = argv[++i];
            } else {
                std::cerr << "Ошибка: опция " << arg << " требует значения\n";
                return 1;
            }
        }
        else if ((arg == "-p") || (arg == "--port")) {
            if (i + 1 < argc) {
                local_port = static_cast<uint16_t>(std::stoi(argv[++i]));
            } else {
                std::cerr << "Ошибка: опция " << arg << " требует значения\n";
                return 1;
            }
        }
        else {
            std::cerr << "Неизвестная опция: " << arg << "\n";
            printUsage(argv[0]);
            return 1;
        }
    }
    
    std::cout << "\n=== RTP Player для H.264 потока ===" << std::endl;
    std::cout << "V4L2 устройство: " << device_path << std::endl;
    std::cout << "Слушаем RTP на: " << local_ip << ":" << local_port << std::endl;
    std::cout << "=====================================" << std::endl << std::endl;
    
    try {
        RTPPlayer player(device_path, local_ip, local_port);
        
        if (!player.initialize()) {
            std::cerr << "Ошибка инициализации RTP Player" << std::endl;
            return 1;
        }
        
        player.start();
        
        std::cout << "Декодировано кадров: " << player.getDecodedFrames() << std::endl;
        
    } catch (const std::exception& e) {
        std::cerr << "Исключение: " << e.what() << std::endl;
        return 1;
    }
    
    return 0;
}
