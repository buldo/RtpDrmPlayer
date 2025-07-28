#include "drm_dmabuf_display.h"
#include "dmabuf_allocator.h"
#include <iostream>
#include <fcntl.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/ioctl.h>
#include <cerrno>
#include <cstring>
#include <vector>
#include <algorithm>
#include <chrono>
#include <xf86drm.h>
#include <xf86drmMode.h>
#include <drm_fourcc.h>
#include <linux/videodev2.h>

class DrmDmaBufDisplayManager::Impl {
public:
    struct ZeroCopyBuffer {
        int dma_fd = -1;        // DMA-buf file descriptor
        uint32_t fb_id = 0;     // DRM framebuffer ID
        uint32_t handle = 0;    // DRM handle
        void* mapped_addr = nullptr;
        size_t size = 0;
    };

    int drm_fd = -1;
    drmModeRes* resources = nullptr;
    drmModeConnector* connector = nullptr;
    drmModeEncoder* encoder = nullptr;
    drmModeCrtc* crtc = nullptr;
    drmModeModeInfo* mode = nullptr;
    uint32_t connector_id = 0;
    uint32_t crtc_id = 0;
    
    uint32_t width = 0;
    uint32_t height = 0;
    
    std::vector<ZeroCopyBuffer> zero_copy_buffers;
    DmaBufAllocator dmabuf_allocator;
    
    uint32_t frame_count = 0;
    
    bool initializeDrm() {
        std::cout << "Initializing TRUE Zero-Copy DRM/DMA-buf display..." << std::endl;
        
        // Search for the correct DRM device with mode setting support
        std::string found_device;
        for (int card = 0; card < 4; card++) {
            std::string device = "/dev/dri/card" + std::to_string(card);
            
            int test_fd = open(device.c_str(), O_RDWR);
            if (test_fd < 0) {
                continue;  // Device not available
            }
            
            // Check for mode setting support
            drmModeRes* test_resources = drmModeGetResources(test_fd);
            if (!test_resources) {
                close(test_fd);
                continue;  // Device does not support mode setting
            }
            
            // Found suitable device
            std::cout << "Found DRM device with mode setting: " << device << std::endl;
            drm_fd = test_fd;
            found_device = device;
            
            // Free test resources
            drmModeFreeResources(test_resources);
            break;
        }
        
        if (drm_fd < 0) {
            std::cerr << "No DRM device with mode setting support found" << std::endl;
            return false;
        }
        
        // Initialize DMA-buf allocator with found device
        if (!dmabuf_allocator.initialize(found_device)) {
            std::cerr << "DMA-buf allocator initialization error" << std::endl;
            return false;
        }
        
        // Get DRM resources
        resources = drmModeGetResources(drm_fd);
        if (!resources) {
            std::cerr << "Error getting DRM resources: " << strerror(errno) << std::endl;
            return false;
        }
        
        std::cout << "DRM resources: " << resources->count_connectors << " connectors, " 
                  << resources->count_crtcs << " CRTC" << std::endl;
        
        return findDisplay();
    }
    
    bool findDisplay() {
        // Search for connected display
        for (int i = 0; i < resources->count_connectors; i++) {
            connector = drmModeGetConnector(drm_fd, resources->connectors[i]);
            if (!connector) continue;
            
            std::cout << "Connector " << i << ": state=" 
                      << (connector->connection == DRM_MODE_CONNECTED ? "connected" : "disconnected")
                      << ", modes=" << connector->count_modes << std::endl;
            
            if (connector->connection == DRM_MODE_CONNECTED && connector->count_modes > 0) {
                connector_id = connector->connector_id;
                
                // Search for 1080p mode
                for (int j = 0; j < connector->count_modes; j++) {
                    drmModeModeInfo* current_mode = &connector->modes[j];
                    std::cout << "  Mode " << j << ": " << current_mode->hdisplay << "x" 
                              << current_mode->vdisplay << "@" << current_mode->vrefresh << "Hz" << std::endl;
                    
                    if (current_mode->hdisplay == 1920 && current_mode->vdisplay == 1080) {
                        mode = current_mode;
                        std::cout << "  ✓ 1080p mode found!" << std::endl;
                        break;
                    }
                }
                
                if (!mode && connector->count_modes > 0) {
                    mode = &connector->modes[0];
                    std::cout << "Using first available mode: " << mode->hdisplay 
                              << "x" << mode->vdisplay << "@" << mode->vrefresh << "Hz" << std::endl;
                }
                break; // Found suitable connector, exit
            }
            
            // Free connector if not suitable
            drmModeFreeConnector(connector);
            connector = nullptr;
        }
        
        if (!connector || !mode) {
            std::cerr << "No suitable display found" << std::endl;
            return false;
        }
        
        return findEncoder();
    }
    
    bool findEncoder() {
        // Find encoder
        if (connector->encoder_id) {
            encoder = drmModeGetEncoder(drm_fd, connector->encoder_id);
        }
        
        if (!encoder) {
            for (int i = 0; i < resources->count_encoders; i++) {
                encoder = drmModeGetEncoder(drm_fd, resources->encoders[i]);
                if (encoder) break;
            }
        }
        
        if (!encoder) {
            std::cerr << "Error getting encoder" << std::endl;
            return false;
        }
        
        // Get CRTC
        if (encoder->crtc_id) {
            crtc = drmModeGetCrtc(drm_fd, encoder->crtc_id);
            crtc_id = encoder->crtc_id;
        } else {
            // Search for free CRTC
            for (int i = 0; i < resources->count_crtcs; i++) {
                if (encoder->possible_crtcs & (1 << i)) {
                    crtc_id = resources->crtcs[i];
                    crtc = drmModeGetCrtc(drm_fd, crtc_id);
                    if (crtc) break; // Add check for successful CRTC acquisition
                }
            }
        }
        
        if (!crtc) {
            std::cerr << "Error getting CRTC" << std::endl;
            return false;
        }
        
        std::cout << "Selected mode: " << mode->hdisplay << "x" << mode->vdisplay 
                  << "@" << mode->vrefresh << "Hz" << std::endl;
        
        return true;
    }
    
    bool setupZeroCopyBuffer(int dma_fd, uint32_t w, uint32_t h) {
        std::cout << "Setting up TRUE zero-copy buffer: " << w << "x" << h 
                  << ", DMA-fd=" << dma_fd << std::endl;
        
        if (dma_fd < 0) {
            std::cerr << "Invalid DMA-buf FD: " << dma_fd << std::endl;
            return false;
        }
        
        // Check buffer size validity
        if (w == 0 || h == 0 || w > 8192 || h > 8192) {
            std::cerr << "Invalid buffer size: " << w << "x" << h << std::endl;
            return false;
        }
        
        // Check if this buffer is already set up
        for (const auto& buf : zero_copy_buffers) {
            if (buf.dma_fd == dma_fd) {
                std::cout << "Buffer with DMA-fd=" << dma_fd << " already set up, skipping" << std::endl;
                return true;
            }
        }
        
        // Import DMA-buf into DRM
        uint32_t handle = 0;
        if (drmPrimeFDToHandle(drm_fd, dma_fd, &handle) < 0) {
            std::cerr << "Error importing DMA-buf into DRM: " << strerror(errno) << std::endl;
            return false;
        }
        
        std::cout << "DMA-buf imported into DRM, handle=" << handle << std::endl;
        
        // Calculate layout for YUV420 with overflow check
        uint64_t y_size_64 = static_cast<uint64_t>(w) * h;
        if (y_size_64 > UINT32_MAX) {
            std::cerr << "Y plane size too large: " << y_size_64 << std::endl;
            drmCloseBufferHandle(drm_fd, handle);
            return false;
        }
        
        uint32_t y_size = static_cast<uint32_t>(y_size_64);
        uint32_t uv_size = y_size / 4;
        
        // Create framebuffer with YUV420 layout
        uint32_t handles[4] = {handle, handle, handle, 0};
        uint32_t pitches[4] = {w, w/2, w/2, 0};
        uint32_t offsets[4] = {0, y_size, y_size + uv_size, 0};
        
        uint32_t fb_id = 0;
        if (drmModeAddFB2(drm_fd, w, h, DRM_FORMAT_YUV420,
                          handles, pitches, offsets, &fb_id, 0) < 0) {
            std::cerr << "Error creating YUV420 framebuffer: " << strerror(errno) << std::endl;
            drmCloseBufferHandle(drm_fd, handle);
            return false;
        }
        
        // Save zero-copy buffer
        ZeroCopyBuffer buffer = {};
        buffer.dma_fd = dma_fd;
        buffer.fb_id = fb_id;
        buffer.handle = handle;
        buffer.mapped_addr = nullptr;
        buffer.size = y_size + uv_size * 2;
        
        zero_copy_buffers.push_back(buffer);
        
        std::cout << "TRUE zero-copy buffer created: fb_id=" << fb_id 
                  << ", size=" << buffer.size << " bytes" << std::endl;
        
        return true;
    }
    
    bool displayZeroCopyFrame(int dma_fd) {
        std::cout << "DrmDmaBufDisplayManager::Impl::displayZeroCopyFrame for DMA-fd " << dma_fd << std::endl;
        auto start_time = std::chrono::high_resolution_clock::now();
        
        // Search for matching buffer
        ZeroCopyBuffer* buffer = nullptr;
        for (auto& buf : zero_copy_buffers) {
            if (buf.dma_fd == dma_fd) {
                buffer = &buf;
                break;
            }
        }
        
        if (!buffer) {
            std::cerr << "Buffer not found for DMA-fd " << dma_fd << std::endl;
            return false;
        }
        
        if (drmModeSetCrtc(drm_fd, crtc_id, buffer->fb_id, 0, 0,
                          &connector_id, 1, mode) != 0) {
            std::cerr << "TRUE zero-copy display error: " << strerror(errno) << std::endl;
            return false;
        }
        
        auto end_time = std::chrono::high_resolution_clock::now();
        auto duration = std::chrono::duration_cast<std::chrono::microseconds>(end_time - start_time);
        
        std::cout << "TRUE ZERO-COPY display: " << duration.count() 
                  << " us (NO copying!)" << std::endl;
                
        return true;
    }
    
    void cleanup() noexcept {
        std::cout << "Cleaning up DRM resources..." << std::endl;
        
        // Clean up zero-copy buffers
        for (auto& buffer : zero_copy_buffers) {
            if (buffer.fb_id > 0) {
                if (drmModeRmFB(drm_fd, buffer.fb_id) < 0) {
                    std::cerr << "Warning: error removing framebuffer " << buffer.fb_id 
                              << ": " << strerror(errno) << std::endl;
                }
            }
            if (buffer.handle > 0) {
                if (drmCloseBufferHandle(drm_fd, buffer.handle) < 0) {
                    std::cerr << "Warning: error closing buffer handle " << buffer.handle 
                              << ": " << strerror(errno) << std::endl;
                }
            }
        }
        zero_copy_buffers.clear();
        
        // Clean up DRM resources
        if (crtc) {
            drmModeFreeCrtc(crtc);
            crtc = nullptr;
        }
        if (encoder) {
            drmModeFreeEncoder(encoder);
            encoder = nullptr;
        }
        if (connector) {
            drmModeFreeConnector(connector);
            connector = nullptr;
        }
        if (resources) {
            drmModeFreeResources(resources);
            resources = nullptr;
        }
        if (drm_fd >= 0) {
            close(drm_fd);
            drm_fd = -1;
        }
        
        std::cout << "DRM resource cleanup complete" << std::endl;
    }
};

DrmDmaBufDisplayManager::DrmDmaBufDisplayManager() : impl_(std::make_unique<Impl>()) {}

DrmDmaBufDisplayManager::~DrmDmaBufDisplayManager() = default;

bool DrmDmaBufDisplayManager::initialize(uint32_t width, uint32_t height) {
    impl_->width = width;
    impl_->height = height;
    
    return impl_->initializeDrm();
}

bool DrmDmaBufDisplayManager::setupZeroCopyBuffer(int dma_fd, uint32_t width, uint32_t height) {
    return impl_->setupZeroCopyBuffer(dma_fd, width, height);
}

bool DrmDmaBufDisplayManager::displayFrame(const FrameInfo& frame) {
    if (frame.is_dmabuf && frame.dma_fd >= 0) {
        // TRUE ZERO-COPY path
        return impl_->displayZeroCopyFrame(frame.dma_fd);
    } else {
        std::cerr << "DrmDmaBufDisplayManager requires DMA-buf frames!" << std::endl;
        return false;
    }
}

void DrmDmaBufDisplayManager::cleanup() noexcept {
    impl_->cleanup();
}

std::string DrmDmaBufDisplayManager::getDisplayInfo() const {
    if (impl_->mode) {
        return "TRUE Zero-Copy DRM/DMA-buf: " + std::to_string(impl_->mode->hdisplay) + "x" + 
               std::to_string(impl_->mode->vdisplay) + "@" + std::to_string(impl_->mode->vrefresh) + "Hz";
    }
    return "TRUE Zero-Copy DRM/DMA-buf (not initialized)";
}
