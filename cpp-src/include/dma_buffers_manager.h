#pragma once

#include "dmabuf_allocator.h"
#include <linux/videodev2.h>
#include <vector>
#include <memory>

class V4L2Device; // Forward declaration

class DmaBuffersManager {
public:
    DmaBuffersManager(std::shared_ptr<DmaBufAllocator> allocator, size_t count, v4l2_buf_type type);
    ~DmaBuffersManager();

    DmaBuffersManager(const DmaBuffersManager&) = delete;
    DmaBuffersManager& operator=(const DmaBuffersManager&) = delete;

    [[nodiscard]] bool allocate(size_t buffer_size);
    void deallocate();

    [[nodiscard]] size_t count() const { return count_; }
    [[nodiscard]] const DmaBufAllocator::DmaBufInfo& get_info(size_t index) const;
    [[nodiscard]] DmaBufAllocator::DmaBufInfo& get_info(size_t index);

    // Methods for managing buffer state
    [[nodiscard]] int get_free_buffer_index();
    void mark_in_use(size_t index);
    void mark_free(size_t index);
    void reset_usage();

    // Methods for interacting with the V4L2 device
    [[nodiscard]] bool requestOnDevice(V4L2Device& device);
    [[nodiscard]] bool releaseOnDevice(V4L2Device& device);

private:
    std::shared_ptr<DmaBufAllocator> allocator_;
    std::vector<DmaBufAllocator::DmaBufInfo> buffers_;
    const size_t count_;
    const v4l2_buf_type type_;
    std::vector<bool> in_use_;
    size_t current_buffer_ = 0;
};
