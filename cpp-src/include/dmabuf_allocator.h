#pragma once

#include <memory>
#include <vector>
#include <string_view>

/**
 * @brief DMA-buf allocator for V4L2 decoder
 * 
 * Provides the ability to use DMA-buf for efficient
 * buffer sharing between different devices without data copying.
 */
class DmaBufAllocator {
public:
    struct DmaBufInfo {
        int fd = -1;           // DMA-buf file descriptor
        void* mapped_addr = nullptr;  // Memory address (if mapped)
        size_t size = 0;       // Buffer size
        uint32_t handle = 0;   // Driver handle (if needed)
    };

    DmaBufAllocator();
    ~DmaBufAllocator();

    /**
     * @brief Allocator initialization
     * @param device_path path to device (e.g., /dev/dri/card0)
     * @return true on success
     */
    [[nodiscard]] bool initialize(std::string_view device_path = "/dev/dri/card0");

    /**
     * @brief Allocate a DMA-buf buffer
     * @param size buffer size in bytes (max 4GB)
     * @return information about the allocated buffer
     */
    [[nodiscard]] DmaBufInfo allocate(size_t size);

    /**
     * @brief Deallocate a DMA-buf buffer
     * @param buf_info information about the buffer to deallocate
     */
    void deallocate(const DmaBufInfo& buf_info);

    /**
     * @brief Map a DMA-buf into the process's address space
     * @param buf_info buffer information
     * @return true on success
     */
    [[nodiscard]] bool map(DmaBufInfo& buf_info);

    /**
     * @brief Unmap a DMA-buf
     * @param buf_info buffer information
     */
    void unmap(DmaBufInfo& buf_info);

    /**
     * @brief Check for DMA-buf support
     * @return true if DMA-buf is supported
     */
    [[nodiscard]] bool isSupported() const;

    // Disallow copying
    DmaBufAllocator(const DmaBufAllocator&) = delete;
    DmaBufAllocator& operator=(const DmaBufAllocator&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
