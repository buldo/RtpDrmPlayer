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

// Используем DMA heaps для Raspberry Pi
#include <linux/dma-buf.h>
#include <linux/types.h>

// Определения DMA heap API (если не найдены в системе)
struct dma_heap_allocation_data {
    __u64 len;
    __u32 fd;
    __u32 fd_flags;
    __u64 heap_flags;
};

#define DMA_HEAP_IOCTL_ALLOC _IOWR('H', 0x0, struct dma_heap_allocation_data)

// Для DMA_BUF_SET_NAME используем системное определение или совместимую версию
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
        // Список DMA heap устройств для Raspberry Pi (по приоритету)
        const std::vector<const char*> heap_names = {
            "/dev/dma_heap/vidbuf_cached",  // Pi 5
            "/dev/dma_heap/linux,cma"       // Pi 4 и ниже
        };

        for (const char* name : heap_names) {
            dma_heap_fd = open(name, O_RDWR | O_CLOEXEC);
            if (dma_heap_fd >= 0) {
                std::cout << "Открыт DMA heap: " << name << std::endl;
                supported = true;
                return true;
            }
            std::cout << "Не удалось открыть " << name << ": " << strerror(errno) << std::endl;
        }

        std::cerr << "Не удалось открыть ни один DMA heap" << std::endl;
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
            std::cerr << "DMA-buf аллокатор не инициализирован" << std::endl;
            return buf_info;
        }

        // Проверяем размер на разумные пределы
        if (size == 0 || size > UINT32_MAX) {
            std::cerr << "Некорректный размер буфера: " << size << " (макс: " << UINT32_MAX << ")" << std::endl;
            return buf_info;
        }

        // Аллокация через DMA heap
        struct dma_heap_allocation_data heap_data = {};
        heap_data.len = size;
        heap_data.fd_flags = O_RDWR | O_CLOEXEC;
        heap_data.heap_flags = 0;

        int ret = ioctl(dma_heap_fd, DMA_HEAP_IOCTL_ALLOC, &heap_data);
        if (ret < 0) {
            std::cerr << "Ошибка DMA_HEAP_IOCTL_ALLOC: " << strerror(errno) << std::endl;
            return buf_info;
        }

        int dmabuf_fd = heap_data.fd;
        
        // Получаем фактический размер буфера
        struct stat stat_buf;
        size_t actual_size = size;
        if (fstat(dmabuf_fd, &stat_buf) == 0) {
            actual_size = stat_buf.st_size;
        }
        
        // Устанавливаем имя буфера для отладки (опционально)
        std::string name = "v4l2_decoder_buffer_" + std::to_string(actual_size);
        
        // Пытаемся установить имя, но игнорируем ошибки так как это не критично
        struct dma_buf_set_name_compat name_data = {};
        name_data.name_ptr = reinterpret_cast<__u64>(name.c_str());
        name_data.name_len = name.length();
        
        ret = ioctl(dmabuf_fd, DMA_BUF_SET_NAME_COMPAT, &name_data);
        if (ret < 0) {
            // Пробуем системный DMA_BUF_SET_NAME если есть
            ret = ioctl(dmabuf_fd, _IOW('b', 1, __u64), reinterpret_cast<__u64>(name.c_str()));
            if (ret < 0) {
                // Не критично, продолжаем без установки имени
                std::cout << "Предупреждение: не удалось установить имя DMA-buf (игнорируем)" << std::endl;
            }
        }

        buf_info.fd = dmabuf_fd;
        buf_info.size = actual_size;  // Используем фактический размер!
        buf_info.mapped_addr = nullptr; // Будет установлен в map()
        buf_info.handle = 0; // Не используется в DMA heaps

        std::cout << "Выделен DMA-buf: fd=" << buf_info.fd 
                  << ", size=" << buf_info.size << " bytes (запрошено " << size << ")" << std::endl;
        
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
            std::cerr << "Ошибка маппинга DMA-buf: " << strerror(errno) << std::endl;
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

// Реализация публичного интерфейса
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