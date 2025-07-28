#include "dma_buffers_manager.h"
#include "v4l2_device.h"
#include <iostream>
#include <optional>
#include <linux/videodev2.h>

DmaBuffersManager::DmaBuffersManager(std::shared_ptr<DmaBufAllocator> allocator, size_t count, v4l2_buf_type type)
    : allocator_(std::move(allocator)), count_(count), type_(type) {
    buffers_.reserve(count_);
    in_use_.resize(count_, false);
}

DmaBuffersManager::~DmaBuffersManager() {
    deallocate();
}

bool DmaBuffersManager::requestOnDevice(V4L2Device& device) {
    struct v4l2_requestbuffers req = {};
    req.count = count_;
    req.type = type_;
    req.memory = V4L2_MEMORY_DMABUF;
    if (!device.request_buffers(req)) {
        std::cerr << "Ошибка запроса " << (type_ == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ? "входных" : "выходных") 
                  << " DMA-buf буферов" << std::endl;
        return false;
    }
    return true;
}

bool DmaBuffersManager::releaseOnDevice(V4L2Device& device) {
    struct v4l2_requestbuffers req = {};
    req.count = 0;
    req.type = type_;
    req.memory = V4L2_MEMORY_DMABUF;
    // Мы не проверяем результат, так как это делается при очистке
    (void)device.request_buffers(req);
    return true;
}

bool DmaBuffersManager::allocate(size_t buffer_size) {
    if (!allocator_) {
        std::cerr << "DmaBufAllocator не инициализирован" << std::endl;
        return false;
    }

    deallocate(); // Освобождаем старые буферы перед выделением новых
    buffers_.resize(count_);
    in_use_.assign(count_, false);
    current_buffer_ = 0;

    for (size_t i = 0; i < count_; ++i) {
        buffers_[i] = allocator_->allocate(buffer_size);
        if (buffers_[i].fd < 0) {
            std::cerr << "Ошибка выделения DMA-buf буфера " << i << std::endl;
            deallocate(); // Очистка в случае ошибки
            return false;
        }
        if (!allocator_->map(buffers_[i])) {
            std::cerr << "Ошибка маппинга DMA-buf буфера " << i << std::endl;
            deallocate();
            return false;
        }
    }
    return true;
}

void DmaBuffersManager::deallocate() {
    if (!allocator_) return;

    for (auto& dmabuf : buffers_) {
        if (dmabuf.mapped_addr) {
            allocator_->unmap(dmabuf);
            dmabuf.mapped_addr = nullptr;
        }
        if (dmabuf.fd >= 0) {
            allocator_->deallocate(dmabuf);
            dmabuf.fd = -1;
        }
    }
    buffers_.clear();
    in_use_.clear();
}

const DmaBufAllocator::DmaBufInfo& DmaBuffersManager::get_info(size_t index) const {
    return buffers_.at(index);
}

DmaBufAllocator::DmaBufInfo& DmaBuffersManager::get_info(size_t index) {
    return buffers_.at(index);
}

int DmaBuffersManager::get_free_buffer_index() {
    for (size_t i = 0; i < count_; ++i) {
        size_t idx = (current_buffer_ + i) % count_;
        if (!in_use_[idx]) {
            // Не инкрементируем current_buffer_ здесь, чтобы можно было "подсмотреть"
            // свободный буфер, не занимая его.
            return idx;
        }
    }
    return -1; // Нет свободных буферов
}

void DmaBuffersManager::mark_in_use(size_t index) {
    if (index < count_) {
        in_use_[index] = true;
        // Обновляем current_buffer_ только когда буфер действительно используется
        if (index == (current_buffer_ % count_)) {
             current_buffer_ = (index + 1) % count_;
        }
    }
}

void DmaBuffersManager::mark_free(size_t index) {
    if (index < count_) {
        in_use_[index] = false;
    }
}

void DmaBuffersManager::reset_usage() {
    in_use_.assign(count_, false);
    current_buffer_ = 0;
}
