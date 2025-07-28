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
     * @return true при успехе
     */
    [[nodiscard]] bool initialize(std::string_view device_path = "/dev/dri/card0");

    /**
     * @brief Выделение DMA-buf буфера
     * @param size размер буфера в байтах (максимум 4GB)
     * @return информация о выделенном буфере
     */
    [[nodiscard]] DmaBufInfo allocate(size_t size);

    /**
     * @brief Освобождение DMA-buf буфера
     * @param buf_info информация о буфере для освобождения
     */
    void deallocate(const DmaBufInfo& buf_info);

    /**
     * @brief Маппинг DMA-buf в адресное пространство процесса
     * @param buf_info информация о буфере
     * @return true при успехе
     */
    [[nodiscard]] bool map(DmaBufInfo& buf_info);

    /**
     * @brief Размаппинг DMA-buf
     * @param buf_info информация о буфере
     */
    void unmap(DmaBufInfo& buf_info);

    /**
     * @brief Проверка поддержки DMA-buf
     * @return true если DMA-buf поддерживается
     */
    [[nodiscard]] bool isSupported() const;

    // Запрет копирования
    DmaBufAllocator(const DmaBufAllocator&) = delete;
    DmaBufAllocator& operator=(const DmaBufAllocator&) = delete;

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
