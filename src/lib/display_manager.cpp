#include "display_manager.h"
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include <string>
#include <xf86drm.h>
#include <xf86drmMode.h>

// Check display type support
bool DisplayManager::isSupported(DisplayType type) noexcept {
    switch (type) {
        case DisplayType::DRM_DMABUF: {
            // Проверяем поддержку DMA-buf, ищем устройство с mode setting
            for (int card = 0; card < 4; card++) {
                std::string device = "/dev/dri/card" + std::to_string(card);
                int fd = open(device.c_str(), O_RDWR);
                if (fd >= 0) {
                    // Проверяем, поддерживает ли устройство mode setting
                    drmModeRes* resources = drmModeGetResources(fd);
                    bool has_modesetting = (resources != nullptr);
                    if (resources) {
                        drmModeFreeResources(resources);
                    }
                    close(fd);
                    
                    if (has_modesetting) {
                        return true;  // Найдено устройство с mode setting
                    }
                }
            }
            return false;
        }
        
        default:
            return false;
    }
}

// Создание менеджера дисплея
std::unique_ptr<DisplayManager> DisplayManager::create(DisplayType type) {
    if (!isSupported(type)) {
        std::cerr << "Тип дисплея не поддерживается" << std::endl;
        return nullptr;
    }
    
    switch (type) {
        case DisplayType::DRM_DMABUF:
            return std::make_unique<DrmDmaBufDisplayManager>();
        
        default:
            return nullptr;
    }
}
