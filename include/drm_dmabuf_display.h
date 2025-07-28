#pragma once

#include <memory>
#include <string>
#include <cstdint>

// TRUE Zero-Copy DRM/DMA-buf дисплей менеджер
class DrmDmaBufDisplayManager {
public:
    struct FrameInfo {
        void* data;         // Указатель на данные кадра
        int dma_fd;         // DMA-buf file descriptor (если доступен)
        uint32_t width;     // Ширина кадра
        uint32_t height;    // Высота кадра
        uint32_t format;    // Формат пикселей (fourcc)
        size_t size;        // Размер данных
        bool is_dmabuf;     // Флаг DMA-buf
    };

    DrmDmaBufDisplayManager();
    ~DrmDmaBufDisplayManager();
    
    bool initialize(uint32_t width, uint32_t height);
    bool displayFrame(const FrameInfo& frame);
    void cleanup() noexcept;
    std::string getDisplayInfo() const;
    
    // Специальные методы для DMA-buf
    bool setupZeroCopyBuffer(int dma_fd, uint32_t width, uint32_t height);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
