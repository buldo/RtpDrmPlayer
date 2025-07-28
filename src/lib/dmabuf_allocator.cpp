#include "dmabuf_allocator.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <sys/stat.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <vector>

// Use DMA heaps for Raspberry Pi
#include <linux/dma-buf.h>
#include <linux/types.h>

// DMA heap API definitions (if not found in the system)
struct dma_heap_allocation_data {
    __u64 len;
    __u32 fd;
    __u32 fd_flags;
    __u64 heap_flags;
};

#define DMA_HEAP_IOCTL_ALLOC _IOWR('H', 0x0, struct dma_heap_allocation_data)

// For DMA_BUF_SET_NAME, use the system definition or a compatible version
#ifndef DMA_BUF_SET_NAME_COMPAT
struct dma_buf_set_name_compat {
    __u64 name_ptr;
    __u32 name_len;
};
#define DMA_BUF_SET_NAME_COMPAT _IOW('b', 1, struct dma_buf_set_name_compat)
#endif

class DmaBufAllocator::Impl {
public:
    int dma_heap_fd = -1;
    bool supported = false;

    bool initialize(std::string_view device_path) {
        // List of DMA heap devices for Raspberry Pi (by priority)
        const std::vector<const char*> heap_names = {
            "/dev/dma_heap/vidbuf_cached",  // Pi 5
            "/dev/dma_heap/linux,cma"       // Pi 4 and below
        };

        for (const char* name : heap_names) {
            dma_heap_fd = open(name, O_RDWR | O_CLOEXEC);
            if (dma_heap_fd >= 0) {
                std::cout << "Opened DMA heap: " << name << std::endl;
                supported = true;
                return true;
            }
            std::cout << "Failed to open " << name << ": " << strerror(errno) << std::endl;
        }

        std::cerr << "Failed to open any DMA heap" << std::endl;
        supported = false;
        return false;
    }

    ~Impl() {
        if (dma_heap_fd >= 0) {
            close(dma_heap_fd);
        }
    }

    DmaBufAllocator::DmaBufInfo allocate(size_t size) {
        DmaBufAllocator::DmaBufInfo buf_info = {};
        
        if (!supported) {
            std::cerr << "DMA-buf allocator not initialized" << std::endl;
            return buf_info;
        }

        // Check size for reasonable limits
        if (size == 0 || size > UINT32_MAX) {
            std::cerr << "Invalid buffer size: " << size << " (max: " << UINT32_MAX << ")" << std::endl;
            return buf_info;
        }

        // Allocation via DMA heap
        struct dma_heap_allocation_data heap_data = {};
        heap_data.len = size;
        heap_data.fd_flags = O_RDWR | O_CLOEXEC;
        heap_data.heap_flags = 0;

        int ret = ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data);
        if (ret < 0) {
            std::cerr << "DMA_HEAP_IOCTL_ALLOC error: " << strerror(errno) << std::endl;
            return buf_info;
        }

        int dmabuf_fd = heap_data.fd;
        
        // Get the actual buffer size
        struct stat stat_buf;
        size_t actual_size = size;
        if (fstat(dmabuf_fd, &stat_buf) == 0) {
            actual_size = stat_buf.st_size;
        }
        
        // Set buffer name for debugging (optional)
        std::string name = "v4l2_decoder_buffer_" + std::to_string(actual_size);
        
        // Try to set the name, but ignore errors as it's not critical
        struct dma_buf_set_name_compat name_data = {};
        name_data.name_ptr = reinterpret_cast<__u64>(name.c_str());
        name_data.name_len = name.length();
        
        ret = ioctl(dmabuf_fd, DMA_BUF_SET_NAME_COMPAT, &name_data);
        if (ret < 0) {
            // Try the system DMA_BUF_SET_NAME if available
            ret = ioctl(dmabuf_fd, _IOW('b', 1, __u64), reinterpret_cast<__u64>(name.c_str()));
            if (ret < 0) {
                // Not critical, continue without setting the name
                std::cout << "Warning: failed to set DMA-buf name (ignoring)" << std::endl;
            }
        }

        buf_info.fd = dmabuf_fd;
        buf_info.size = actual_size;  // Use the actual size!
        buf_info.mapped_addr = nullptr; // Will be set in map()
        buf_info.handle = 0; // Not used with DMA heaps

        std::cout << "Allocated DMA-buf: fd=" << buf_info.fd 
                  << ", size=" << buf_info.size << " bytes (requested " << size << ")" << std::endl;
        
        return buf_info;
    }

    void deallocate(const DmaBufAllocator::DmaBufInfo& buf_info) {
        if (buf_info.fd >= 0) {
            close(buf_info.fd);
        }
    }

    bool map(DmaBufAllocator::DmaBufInfo& buf_info) {
        if (buf_info.fd < 0) {
            return false;
        }

        void* addr = mmap(nullptr, buf_info.size, PROT_READ | PROT_WRITE, 
                         MAP_SHARED, buf_info.fd, 0);
        
        if (addr == MAP_FAILED) {
            std::cerr << "Error mapping DMA-buf: " << strerror(errno) << std::endl;
            return false;
        }

        buf_info.mapped_addr = addr;
        return true;
    }

    void unmap(DmaBufAllocator::DmaBufInfo& buf_info) {
        if (buf_info.mapped_addr != nullptr) {
            munmap(buf_info.mapped_addr, buf_info.size);
            buf_info.mapped_addr = nullptr;
        }
    }
};

// Public interface implementation
DmaBufAllocator::DmaBufAllocator() : impl_(std::make_unique<Impl>()) {}

DmaBufAllocator::~DmaBufAllocator() = default;

bool DmaBufAllocator::initialize(std::string_view device_path) {
    return impl_->initialize(device_path);
}

DmaBufAllocator::DmaBufInfo DmaBufAllocator::allocate(size_t size) {
    return impl_->allocate(size);
}

void DmaBufAllocator::deallocate(const DmaBufInfo& buf_info) {
    impl_->deallocate(buf_info);
}

bool DmaBufAllocator::map(DmaBufInfo& buf_info) {
    return impl_->map(buf_info);
}

void DmaBufAllocator::unmap(DmaBufInfo& buf_info) {
    impl_->unmap(buf_info);
}

bool DmaBufAllocator::isSupported() const {
    return impl_->supported;
}