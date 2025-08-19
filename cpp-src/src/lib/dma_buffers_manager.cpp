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
        std::cerr << "Error requesting " << (type_ == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE ? "input" : "output") 
                  << " DMA-buf buffers" << std::endl;
        return false;
    }
    return true;
}

bool DmaBuffersManager::releaseOnDevice(V4L2Device& device) {
    struct v4l2_requestbuffers req = {};
    req.count = 0;
    req.type = type_;
    req.memory = V4L2_MEMORY_DMABUF;
    // We don't check the result as this is done during cleanup
    (void)device.request_buffers(req);
    return true;
}

bool DmaBuffersManager::allocate(size_t buffer_size) {
    if (!allocator_) {
        std::cerr << "DmaBufAllocator not initialized" << std::endl;
        return false;
    }

    deallocate(); // Free old buffers before allocating new ones
    buffers_.resize(count_);
    in_use_.assign(count_, false);
    current_buffer_ = 0;

    for (size_t i = 0; i < count_; ++i) {
        buffers_[i] = allocator_->allocate(buffer_size);
        if (buffers_[i].fd < 0) {
            std::cerr << "Error allocating DMA-buf buffer " << i << std::endl;
            deallocate(); // Cleanup in case of error
            return false;
        }
        if (!allocator_->map(buffers_[i])) {
            std::cerr << "Error mapping DMA-buf buffer " << i << std::endl;
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
            // We don't increment current_buffer_ here, so we can "peek" at
            // a free buffer without occupying it.
            return idx;
        }
    }
    return -1; // No free buffers
}

void DmaBuffersManager::mark_in_use(size_t index) {
    if (index < count_) {
        in_use_[index] = true;
        // Update current_buffer_ only when the buffer is actually used
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
