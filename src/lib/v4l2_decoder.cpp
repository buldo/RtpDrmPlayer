#include "config.h"
#include "v4l2_decoder.h"
#include "v4l2_device.h"
#include "dmabuf_allocator.h"
#include "dma_buffers_manager.h"
#include "drm_dmabuf_display.h"
#include "frame_processor.h"
#include "streaming_manager.h"
#include <iostream>
#include <fstream>
#include <vector>
#include <cstring>
#include <unistd.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <linux/videodev2.h>
#include <cerrno>
#include <poll.h>
#include <memory>
#include <string_view>
#include <span>
#include <chrono>
#include <linux/dma-buf.h>


class V4L2DecoderImpl {
    DecoderConfig config_;
    std::unique_ptr<V4L2Device> device_;
    
    // For DMA-buf buffers
    std::shared_ptr<DmaBufAllocator> dmabuf_allocator;
    std::unique_ptr<DmaBuffersManager> input_buffers_;
    std::unique_ptr<DmaBuffersManager> output_buffers_;
   
    // Tracking Zero-Copy initialization for each buffer
    std::vector<bool> zero_copy_initialized;

    // For display
    std::unique_ptr<DrmDmaBufDisplayManager> display_manager;
    V4L2Decoder::DisplayType display_type = V4L2Decoder::DisplayType::NONE;
    uint32_t frame_width = 0;
    uint32_t frame_height = 0;
    
    // Frame handler
    std::unique_ptr<FrameProcessor> frame_processor_;

    // Streaming manager
    std::unique_ptr<StreamingManager> streaming_manager_;

    int decoded_frame_count = 0;
    
    // Decoder initialization flag
    bool decoder_ready = false;
    bool needs_reset = false;
    
public:
    V4L2DecoderImpl() : 
        device_(std::make_unique<V4L2Device>())
    {
        dmabuf_allocator = std::make_shared<DmaBufAllocator>();
    }
    ~V4L2DecoderImpl() { cleanup(); }

    [[nodiscard]] int getDecodedFrameCount() const noexcept { return decoded_frame_count; }

    [[nodiscard]] bool initialize(const DecoderConfig& config) {
        config_ = config;
        
        input_buffers_ = std::make_unique<DmaBuffersManager>(dmabuf_allocator, config_.input_buffer_count, V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
        output_buffers_ = std::make_unique<DmaBuffersManager>(dmabuf_allocator, config_.output_buffer_count, V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);

        streaming_manager_ = std::make_unique<StreamingManager>(*device_, *output_buffers_);

        if (!device_->initialize_for_decoding(config_.device_path)) {
            return false;
        }
        
        // Initialize DMA-buf allocator
        std::cout << "Initializing DMA-buf allocator..." << std::endl;
        if (!dmabuf_allocator->initialize()) {
            std::cerr << "❌ CRITICAL ERROR: Failed to initialize DMA-buf allocator" << std::endl;
            std::cerr << "Check for /dev/dma_heap/vidbuf_cached" << std::endl;
            device_->close();
            return false;
        }
        
        frame_processor_ = std::make_unique<FrameProcessor>(
            display_manager.get(), 
            output_buffers_.get(), 
            frame_width, 
            frame_height, 
            display_type, 
            zero_copy_initialized, 
            decoded_frame_count,
            // Lambda function for Zero-Copy setup
            [this](unsigned int buffer_index) {
                if (buffer_index >= zero_copy_initialized.size() || zero_copy_initialized[buffer_index]) {
                    return;
                }

                // Use dynamic_cast to check if our DisplayManager is the required type
                auto* dmabuf_display = dynamic_cast<DrmDmaBufDisplayManager*>(display_manager.get());
                if (dmabuf_display) {
                    const auto& buffer_info = output_buffers_->get_info(buffer_index);
                    if (dmabuf_display->setupZeroCopyBuffer(buffer_info.fd, frame_width, frame_height)) {
                        zero_copy_initialized[buffer_index] = true;
                        std::cout << "✅ Zero-copy buffer " << buffer_index << " configured via callback" << std::endl;
                    }
                }
            }
        );

        return setupFormats() && setupBuffers();
    }

    void handleV4L2Events() {
        struct v4l2_event ev = {};
        while (device_->dequeue_event(ev)) {
            switch (ev.type) {
                case V4L2_EVENT_SOURCE_CHANGE:
                    std::cout << "🔄 V4L2_EVENT_SOURCE_CHANGE received" << std::endl;
                    if (ev.u.src_change.changes & V4L2_EVENT_SRC_CH_RESOLUTION) {
                        std::cout << "  📐 Resolution change, IGNORE reset" << std::endl;
                        // needs_reset = true; // Отключено по запросу
                    }
                    break;
                    
                case V4L2_EVENT_EOS:
                    std::cout << "🔚 V4L2_EVENT_EOS received - end of stream" << std::endl;
                    break;
                    
                case V4L2_EVENT_FRAME_SYNC:
                    std::cout << "🎬 V4L2_EVENT_FRAME_SYNC received - frame ready" << std::endl;
                    break;
                    
                default:
                    std::cout << "❓ Unknown V4L2 event: " << ev.type << std::endl;
                    break;
            }
        }
    }

    [[nodiscard]] bool setupFormats() {
        if (!device_->configure_decoder_formats(config_.width, config_.height, config_.input_codec, config_.output_pixel_format)) {
            return false;
        }

        // Get actual frame size after format setup
        struct v4l2_format fmt_out = {};
        fmt_out.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (!device_->get_format(fmt_out)) {
            std::cerr << "❌ ОШИБКА: Не удалось получить выходной формат после настройки" << std::endl;
            return false;
        }
        
        // Save frame size for display
        frame_width = fmt_out.fmt.pix_mp.width;
        frame_height = fmt_out.fmt.pix_mp.height;
        
        // Initialize display if already configured
        if (display_manager && display_type != V4L2Decoder::DisplayType::NONE) {
            std::cout << "Initializing display " << frame_width << "x" << frame_height << std::endl;
            if (!display_manager->initialize(frame_width, frame_height)) {
                std::cerr << "Display initialization error" << std::endl;
                display_manager.reset();
                return false;
            }
            std::cout << "Display initialized: " << display_manager->getDisplayInfo() << std::endl;
        } else {
            std::cout << "Display not initialized: display_manager=" << (display_manager ? "present" : "absent") 
                      << ", display_type=" << (int)display_type << std::endl;
        }
        
        std::cout << "Output format set: YUV420, " << fmt_out.fmt.pix_mp.width << "x" << fmt_out.fmt.pix_mp.height << std::endl;
        return true;
    }

    [[nodiscard]] bool setDisplay() {
        display_type = V4L2Decoder::DisplayType::DRM_DMABUF;
        
        std::cout << "Настройка дисплея: TRUE Zero-Copy DMA-buf" << std::endl;
        
        display_manager = std::make_unique<DrmDmaBufDisplayManager>();
        
        // Если frame_width и frame_height уже известны, инициализируем дисплей
        if (frame_width > 0 && frame_height > 0) {
            if (!display_manager->initialize(frame_width, frame_height)) {
                std::cerr << "Ошибка инициализации дисплея" << std::endl;
                display_manager.reset();
                return false;
            }
            std::cout << "Дисплей инициализирован: " << display_manager->getDisplayInfo() << std::endl;
        }
        
        // Обновляем display_manager в frame_processor
        if (frame_processor_) {
            frame_processor_->setDisplayManager(display_manager.get());
        }
        
        return true;
    }

    [[nodiscard]] bool setupBuffers() {
        return setupDmaBufs();
    }

    [[nodiscard]] bool setupDmaBufs() {
        // Полностью DMA-buf подход
        
        zero_copy_initialized.assign(output_buffers_->count(), false);
        
        // Получаем размеры буферов от V4L2
        struct v4l2_format fmt_out = {};
        fmt_out.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        if (!device_->get_format(fmt_out)) {
            std::cerr << "Ошибка получения output формата" << std::endl;
            return false;
        }
        
        struct v4l2_format fmt_cap = {};
        fmt_cap.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        if (!device_->get_format(fmt_cap)) {
            std::cerr << "Ошибка получения capture формата" << std::endl;
            return false;
        }
        
        size_t input_buffer_size = fmt_out.fmt.pix_mp.plane_fmt[0].sizeimage;
        size_t output_buffer_size = fmt_cap.fmt.pix_mp.plane_fmt[0].sizeimage;
        
        if (input_buffer_size == 0) {
            input_buffer_size = config_.default_input_buffer_size;
        }
        if (output_buffer_size == 0) {
            output_buffer_size = config_.width * config_.height * 3 / 2; // Для YUV420
        }
        
        std::cout << "Размеры буферов: input=" << input_buffer_size 
                  << ", output=" << output_buffer_size << std::endl;

        // 1. ВХОДНЫЕ буферы - DMA-buf
        if (!input_buffers_->allocate(input_buffer_size)) {
            return false;
        }
        if (!input_buffers_->requestOnDevice(*device_)) {
            return false;
        }
        
        // 2. ВЫХОДНЫЕ буферы - DMA-buf
        if (!output_buffers_->allocate(output_buffer_size)) {
            return false;
        }
        for (size_t i = 0; i < output_buffers_->count(); ++i) {
            auto& info = output_buffers_->get_info(i);
            if (info.mapped_addr && info.size > 0) {
                uint8_t* buffer = static_cast<uint8_t*>(info.mapped_addr);
                size_t y_size = frame_width * frame_height;
                size_t uv_size = y_size / 2;
                std::memset(buffer, 16, y_size);
                std::memset(buffer + y_size, 128, uv_size);
            }
        }
        if (!output_buffers_->requestOnDevice(*device_)) {
            return false;
        }
        
        std::cout << "DMA-buf буферы настроены: " << input_buffers_->count() << " входных, " 
                  << output_buffers_->count() << " выходных" << std::endl;
        return true;
    }

    // FFmpeg-style streaming state management
    
    [[nodiscard]] bool startStreaming() {
        return streaming_manager_->start();
    }

private:
    [[nodiscard]] bool queueOutputBuffers() {
        std::cout << "Постановка в очередь " << output_buffers_->count() << " выходных буферов..." << std::endl;

        for (unsigned int i = 0; i < output_buffers_->count(); ++i) {
            if (!queueOutputBuffer(i)) {
                std::cerr << "❌ Ошибка постановки в очередь буфера " << i << std::endl;
                return false;
            }
        }
        return true;
    }

    [[nodiscard]] bool queueOutputBuffer(unsigned int index) {
        if (index >= output_buffers_->count()) {
            std::cerr << "❌ Недопустимый индекс буфера: " << index << std::endl;
            return false;
        }

        struct v4l2_buffer buf = {};
        struct v4l2_plane plane = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_DMABUF;
        buf.index = index;
        buf.m.planes = &plane;
        buf.length = 1;
        plane.m.fd = output_buffers_->get_info(index).fd;
        plane.length = output_buffers_->get_info(index).size;

        if (!device_->queue_buffer(buf)) {
            std::cerr << "❌ VIDIOC_QBUF для буфера " << index << " не удалось" << std::endl;
            return false;
        }
        return true;
    }

    [[nodiscard]] bool enableStreaming() {
        // Enable output streaming first (input buffers)
        if (!device_->stream_on(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)) {
            std::cerr << "❌ VIDIOC_STREAMON для входа не удалось" << std::endl;
            return false;
        }

        // Enable capture streaming (output buffers)
        if (!device_->stream_on(V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) {
            std::cerr << "❌ VIDIOC_STREAMON для выхода не удалось" << std::endl;

            // Rollback input streaming on failure
            (void)device_->stream_off(V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
            return false;
        }

        return true;
    }

public:
    [[nodiscard]] bool stopStreaming() {
        return streaming_manager_->stop();
    }

    [[nodiscard]] bool decodeData(const uint8_t* data, size_t size) {
        if (!data || size == 0) {
            std::cerr << "❌ ОШИБКА: Недопустимые входные данные (data=" << (void*)data 
                      << ", size=" << size << ")" << std::endl;
            return false;
        }

        if (!device_->is_open()) {
            std::cerr << "❌ КРИТИЧЕСКАЯ ОШИБКА: Декодер не инициализирован" << std::endl;
            return false;
        }

        // Проверяем, не нужен ли сброс из-за V4L2 события
        if (needs_reset) {
            std::cout << "🚀 Выполнение сброса из-за V4L2_EVENT_SOURCE_CHANGE..." << std::endl;
            if (!resetBuffers()) {
                std::cerr << "❌ Ошибка при сбросе буферов после изменения источника" << std::endl;
                return false;
            }
            if (!streaming_manager_->start()) {
                std::cerr << "❌ Ошибка при перезапуске стриминга после сброса" << std::endl;
                return false;
            }
            needs_reset = false; // Сбрасываем флаг
            std::cout << "✅ Сброс и перезапуск стриминга выполнены успешно" << std::endl;
        }

        // uvgRTP предоставляет полные кадры - обработка NAL не требуется.
        // Декодер считается готовым, если он был инициализирован.
        if (!decoder_ready) {
            decoder_ready = true;
            std::cout << "✅ Декодер готов к приему данных" << std::endl;
        }

        // Запускаем стриминг, если еще не запущен
        if (!streaming_manager_->is_active()) {
            if (!streaming_manager_->start()) {
                std::cerr << "Ошибка запуска стриминга" << std::endl;
                return false;
            }
            std::cout << "🚀 Стриминг запущен" << std::endl;
        }

        // Освобождаем завершенные входные буферы
        struct v4l2_buffer dq_buf_in = {};
        struct v4l2_plane dq_plane_in = {};
        dq_buf_in.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        dq_buf_in.memory = V4L2_MEMORY_DMABUF;
        dq_buf_in.m.planes = &dq_plane_in;
        dq_buf_in.length = 1;
        while (device_->dequeue_buffer(dq_buf_in)) {
            input_buffers_->mark_free(dq_buf_in.index);
        }

        int buffer_to_use = input_buffers_->get_free_buffer_index();
        
        if (buffer_to_use == -1) {
            // Если нет свободных буферов, попробуем подождать один с небольшим таймаутом
            if (device_->poll(POLLOUT | POLLERR, 20) && device_->is_ready_for_write()) {
                 if (device_->dequeue_buffer(dq_buf_in)) {
                    input_buffers_->mark_free(dq_buf_in.index);
                    buffer_to_use = dq_buf_in.index;
                    std::cout << "✅ Освобожден входной буфер " << buffer_to_use << " после ожидания" << std::endl;
                }
            }
        }

        if (buffer_to_use == -1) {
            std::cerr << "❌ ОШИБКА: Нет свободных входных буферов!" << std::endl;
            return false;
        }
        
        // Validate buffer bounds
        if (buffer_to_use >= static_cast<int>(input_buffers_->count()) || buffer_to_use < 0) {
            std::cerr << "❌ КРИТИЧЕСКАЯ ОШИБКА: Недопустимый индекс буфера: " << buffer_to_use << std::endl;
            return false;
        }

        if (!input_buffers_->get_info(buffer_to_use).mapped_addr) {
            std::cerr << "❌ КРИТИЧЕСКАЯ ОШИБКА: Указатель буфера NULL для индекса " << buffer_to_use << std::endl;
            return false;
        }

        // --- DMA-BUF Synchronization START ---
        struct dma_buf_sync sync_start = {};
        sync_start.flags = DMA_BUF_SYNC_START | DMA_BUF_SYNC_RW;
        if (ioctl(input_buffers_->get_info(buffer_to_use).fd, DMA_BUF_IOCTL_SYNC, &sync_start) < 0) {
            std::cerr << "⚠️ ПРЕДУПРЕЖДЕНИЕ: Не удалось выполнить DMA_BUF_IOCTL_SYNC_START - "
                      << strerror(errno) << " (код: " << errno << ")" << std::endl;
            // Продолжаем, но это может привести к повреждению данных
        }

        size_t chunk_size = std::min(size, input_buffers_->get_info(buffer_to_use).size);
        if (chunk_size == 0) {
            std::cerr << "❌ ОШИБКА: Размер данных для копирования равен 0" << std::endl;
            return false;
        }

        std::memcpy(input_buffers_->get_info(buffer_to_use).mapped_addr, data, chunk_size);

        // --- DMA-BUF Synchronization END ---
        struct dma_buf_sync sync_end = {};
        sync_end.flags = DMA_BUF_SYNC_END | DMA_BUF_SYNC_RW;
        if (ioctl(input_buffers_->get_info(buffer_to_use).fd, DMA_BUF_IOCTL_SYNC, &sync_end) < 0) {
            std::cerr << "⚠️ ПРЕДУПРЕЖДЕНИЕ: Не удалось выполнить DMA_BUF_IOCTL_SYNC_END - "
                      << strerror(errno) << " (код: " << errno << ")" << std::endl;
        }

        struct v4l2_buffer buf = {};
        struct v4l2_plane plane = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buf.memory = V4L2_MEMORY_DMABUF;
        buf.index = buffer_to_use;
        buf.m.planes = &plane;
        buf.length = 1;
        plane.m.fd = input_buffers_->get_info(buffer_to_use).fd;
        plane.bytesused = chunk_size;
        plane.length = input_buffers_->get_info(buffer_to_use).size; // Указываем полный размер буфера
        
        if (!device_->queue_buffer(buf)) {
            std::cerr << "❌ ОШИБКА: Не удалось поставить буфер в очередь (буфер " << buffer_to_use 
                      << ")" << std::endl;
            return false;
        }
        input_buffers_->mark_in_use(buffer_to_use);

        // --- Извлекаем готовые кадры ---
        if (!decoder_ready) {
            std::cout << "⏭️ Данные отправлены, ожидание готовности декодера" << std::endl;
            return true;
        }

        bool frames_processed;
        do {
            frames_processed = false;
            if (!device_->poll(POLLIN | POLLPRI | POLLERR, 0)) { // Таймаут 0 для неблокирующей проверки
                break;
            }

            if (device_->has_event()) { handleV4L2Events(); }
            if (device_->has_error()) { 
                std::cerr << "❌ POLLERR" << std::endl; 
                needs_reset = true; // Устанавливаем флаг для сброса
                return false; 
            }
            
            if (device_->is_ready_for_read()) {
                struct v4l2_buffer out_buf = {};
                struct v4l2_plane out_plane = {};
                out_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
                out_buf.memory = V4L2_MEMORY_DMABUF;
                out_buf.m.planes = &out_plane;
                out_buf.length = 1;
                
                if (device_->dequeue_buffer(out_buf)) {
                    if (frame_processor_->processDecodedFrame(out_buf)) {
                        requeueOutputBuffer(out_buf);
                    }
                    frames_processed = true;
                } else {
                    // EAGAIN - это нормально, просто данных пока нет
                    break;
                }
            }
        } while (frames_processed);

        return true;
    }

    [[nodiscard]] bool flushDecoder() {
        if (!device_->is_open()) {
            return false;
        }
        
        std::cout << "🔄 Принудительная очистка буферов декодера..." << std::endl;

        // 1. Найти свободный входной буфер
        int flush_buffer_idx = input_buffers_->get_free_buffer_index();

        // 2. Если свободных нет, попытаться освободить один
        if (flush_buffer_idx == -1) {
            std::cout << "Нет свободных входных буферов, пытаемся освободить..." << std::endl;
            struct v4l2_buffer dq_buf = {};
            struct v4l2_plane dq_plane = {};
            dq_buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
            dq_buf.memory = V4L2_MEMORY_MMAP;
            dq_buf.m.planes = &dq_plane;
            dq_buf.length = 1;
            
            if (device_->dequeue_buffer(dq_buf)) {
                input_buffers_->mark_free(dq_buf.index);
                flush_buffer_idx = dq_buf.index;
                std::cout << "✅ Освобожден буфер " << flush_buffer_idx << std::endl;
            } else {
                std::cerr << "❌ Не удалось освободить входной буфер для flush" << std::endl;
                return false;
            }
        }
        
        // 3. Отправляем пустой буфер с флагом последнего для очистки декодера
        struct v4l2_buffer buf = {};
        struct v4l2_plane plane = {};
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buf.memory = V4L2_MEMORY_DMABUF;
        buf.index = flush_buffer_idx;
        buf.m.planes = &plane;
        buf.length = 1;
        buf.flags = V4L2_BUF_FLAG_LAST; // Флаг конца потока
        plane.m.fd = input_buffers_->get_info(flush_buffer_idx).fd;
        plane.bytesused = 0; // Пустые данные для flush
        
        if (!device_->queue_buffer(buf)) {
            std::cerr << "❌ Ошибка отправки flush буфера" << std::endl;
            return false;
        }
        input_buffers_->mark_in_use(flush_buffer_idx); // Помечаем буфер как используемый
        
        // Проверяем выходные кадры после flush
        int attempts = 0;
        while (attempts < 20) {
            if (!device_->poll(POLLIN | POLLPRI | POLLERR, 50)) {
                attempts++;
                continue;
            }
            
            if (device_->has_event()) {
                handleV4L2Events();
            }
            
            if (device_->has_error()) {
                std::cerr << "❌ POLLERR при flush" << std::endl;
                return false; // Выходим с ошибкой
            }
            
            if (device_->is_ready_for_read()) {
                struct v4l2_buffer out_buf = {};
                struct v4l2_plane out_plane = {};
                out_buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
                out_buf.memory = V4L2_MEMORY_DMABUF;
                out_buf.m.planes = &out_plane;
                out_buf.length = 1;
                
                if (device_->dequeue_buffer(out_buf)) {
                    if (frame_processor_->processDecodedFrame(out_buf)) {
                         if (!requeueOutputBuffer(out_buf)) {
                            std::cerr << "❌ Не удалось вернуть flush выходной буфер" << std::endl;
                        }
                    }
                    attempts = 0; // Сброс счетчика при получении кадра
                } else {
                    attempts++;
                }
            }
        }
        
        std::cout << "✅ Очистка буферов завершена" << std::endl;
        return true;
    }

    [[nodiscard]] bool resetBuffers() {
        if (!device_->is_open()) {
            std::cerr << "❌ Декодер не инициализирован" << std::endl;
            return false;
        }

        std::cout << "🔄 ПЕРЕЗАГРУЗКА: Полный сброс V4L2 буферов..." << std::endl;

        // Останавливаем стриминг
        if (streaming_manager_ && streaming_manager_->is_active()) {
            streaming_manager_->stop();
        }
        streaming_manager_->set_inactive();

        // Освобождаем буферы
        if (input_buffers_) {
            (void)input_buffers_->releaseOnDevice(*device_);
        }
        if (output_buffers_) {
            (void)output_buffers_->releaseOnDevice(*device_);
        }

        // Wait for any pending operations to complete
        usleep(50000); // 50ms

        // Reset buffer tracking state first
        if (input_buffers_) {
            input_buffers_->reset_usage();
        }

        // КРИТИЧНО: Освобождаем старые DMA-buf буферы (входные и выходные)
        if (input_buffers_) {
            input_buffers_->deallocate();
        }
        if (output_buffers_) {
            output_buffers_->deallocate();
        }

        // Сбрасываем состояние zero-copy ПОСЛЕ очистки буферов
        zero_copy_initialized.clear();

        // Очищаем MMAP буферы для входных данных - больше не нужно
        /*
        for (auto& mmap_buf : input_mmap_buffers) {
            if (mmap_buf.ptr && mmap_buf.ptr != MAP_FAILED) {
                if (munmap(mmap_buf.ptr, mmap_buf.size) < 0) {
                    std::cerr << "❌ Ошибка munmap в resetBuffers: " << strerror(errno) << std::endl;
                }
                mmap_buf.ptr = nullptr;
                mmap_buf.size = 0;
            }
        }
        input_mmap_buffers.clear();
        */

        // Ждем дольше для освобождения DMA-buf памяти
        usleep(200000); // 200ms

        // Пересоздаем буферы
        if (!setupBuffers()) {
            std::cerr << "❌ Ошибка пересоздания буферов" << std::endl;
            return false;
        }

        std::cout << "✅ Буферы успешно сброшены и пересозданы" << std::endl;
        return true;
    }

private:
    [[nodiscard]] bool requeueOutputBuffer(const v4l2_buffer& out_buf) {
        struct v4l2_buffer requeue_buf = out_buf;
        struct v4l2_plane requeue_plane = {};
        requeue_buf.m.planes = &requeue_plane;
        requeue_plane.m.fd = output_buffers_->get_info(out_buf.index).fd;
        requeue_plane.length = output_buffers_->get_info(out_buf.index).size;

        if (!device_->queue_buffer(requeue_buf)) {
            std::cerr << "❌ КРИТИЧЕСКАЯ ОШИБКА: Не удалось вернуть буфер " << out_buf.index << std::endl;
            return false;
        }
        return true;
    }

public:
    void cleanup() noexcept {
        std::cout << "Завершение работы V4L2 декодера..." << std::endl;
        
        if (!device_->is_open()) return;
        
        // First stop streaming to prevent further operations
        if (streaming_manager_ && streaming_manager_->is_active()) {
            (void)streaming_manager_->stop();
        }

        // Release V4L2 buffers first
        if (input_buffers_) {
            (void)input_buffers_->releaseOnDevice(*device_);
        }
        if (output_buffers_) {
            (void)output_buffers_->releaseOnDevice(*device_);
        }

        // Очищаем DMA-buf буферы (входные и выходные)
        if (dmabuf_allocator) {
            std::cout << "Освобождение DMA-buf буферов..." << std::endl;
            if (input_buffers_) {
                input_buffers_->deallocate();
            }
            if (output_buffers_) {
                output_buffers_->deallocate();
            }
            zero_copy_initialized.clear();
        }

        // Cleanup display manager before closing device
        if (display_manager) {
            display_manager.reset();
        }

        // Close device file descriptor
        device_->close();

        // Reset all state variables
        decoder_ready = false;
        needs_reset = false;
        frame_width = 0;
        frame_height = 0;

        std::cout << "V4L2 декодер завершен. Декодировано кадров: " << decoded_frame_count << std::endl;
    }
};


// Реализация интерфейса V4L2Decoder
V4L2Decoder::V4L2Decoder() : impl(std::make_unique<V4L2DecoderImpl>()) {}
V4L2Decoder::~V4L2Decoder() = default;

bool V4L2Decoder::initialize(const DecoderConfig& config) { return impl->initialize(config); }
bool V4L2Decoder::setDisplay() { return impl->setDisplay(); }
bool V4L2Decoder::decodeData(const uint8_t* data, size_t size) { return impl->decodeData(data, size); }
bool V4L2Decoder::flushDecoder() { return impl->flushDecoder(); }
bool V4L2Decoder::resetBuffers() { return impl->resetBuffers(); }
int V4L2Decoder::getDecodedFrameCount() const { return impl->getDecodedFrameCount(); }
