#pragma once

#include <memory>
#include <string>
#include <cstdint>

// TRUE Zero-Copy DRM/DMA-buf display manager
class DrmDmaBufDisplayManager {
public:
    struct FrameInfo {
        void* data;         // Pointer to frame data
        int dma_fd;         // DMA-buf file descriptor (if available)
        uint32_t width;     // Frame width
        uint32_t height;    // Frame height
        uint32_t format;    // Pixel format (fourcc)
        size_t size;        // Data size
        bool is_dmabuf;     // DMA-buf flag
    };

    DrmDmaBufDisplayManager();
    ~DrmDmaBufDisplayManager();
    
    bool initialize(uint32_t width, uint32_t height);
    bool displayFrame(const FrameInfo& frame);
    void cleanup() noexcept;
    std::string getDisplayInfo() const;
    
    // Special methods for DMA-buf
    bool setupZeroCopyBuffer(int dma_fd, uint32_t width, uint32_t height);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
