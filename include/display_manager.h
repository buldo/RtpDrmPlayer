#pragma once

#include <memory>
#include <string>
#include <cstdint>

// Абстрактный интерфейс для отображения кадров
class DisplayManager {
public:
    enum class DisplayType {
        DRM_DMABUF  // TRUE Zero-Copy через DMA-buf
    };

    struct FrameInfo {
        void* data;         // Указатель на данные кадра
        int dma_fd;         // DMA-buf file descriptor (если доступен)
        uint32_t width;     // Ширина кадра
        uint32_t height;    // Высота кадра
        uint32_t format;    // Формат пикселей (fourcc)
        size_t size;        // Размер данных
        bool is_dmabuf;     // Флаг DMA-buf
    };

    virtual ~DisplayManager() = default;
    
    // Инициализация дисплея
    virtual bool initialize(uint32_t width, uint32_t height) = 0;
    
    // Отображение кадра
    virtual bool displayFrame(const FrameInfo& frame) = 0;
    
    // Очистка ресурсов
    virtual void cleanup() noexcept = 0;
    
    // Проверка поддержки типа дисплея
    static bool isSupported(DisplayType type) noexcept;
    
    // Создание менеджера дисплея
    static std::unique_ptr<DisplayManager> create(DisplayType type);
    
    // Получение информации о дисплее
    virtual std::string getDisplayInfo() const = 0;
};

// TRUE Zero-Copy DRM/DMA-buf дисплей менеджер
class DrmDmaBufDisplayManager : public DisplayManager {
public:
    DrmDmaBufDisplayManager();
    ~DrmDmaBufDisplayManager() override;
    
    bool initialize(uint32_t width, uint32_t height) override;
    bool displayFrame(const FrameInfo& frame) override;
    void cleanup() noexcept override;
    std::string getDisplayInfo() const override;
    
    // Специальные методы для DMA-buf
    bool setupZeroCopyBuffer(int dma_fd, uint32_t width, uint32_t height);

private:
    class Impl;
    std::unique_ptr<Impl> impl_;
};
