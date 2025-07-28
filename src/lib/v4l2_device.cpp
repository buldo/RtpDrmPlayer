#include "v4l2_device.h"
#include <iostream>
#include <cstring>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>

V4L2Device::V4L2Device() = default;

V4L2Device::~V4L2Device() {
    close();
}

bool V4L2Device::open(std::string_view device_path) {
    if (is_open()) {
        std::cerr << "Устройство уже открыто" << std::endl;
        return false;
    }

    fd_ = ::open(device_path.data(), O_RDWR | O_NONBLOCK, 0);
    if (fd_ < 0) {
        std::cerr << "Ошибка открытия устройства " << device_path << ": " << strerror(errno) << std::endl;
        return false;
    }

    poll_fds_.resize(1);
    poll_fds_[0].fd = fd_;
    poll_fds_[0].events = POLLPRI; // По умолчанию для событий

    std::cout << "Устройство " << device_path << " открыто, fd=" << fd_ << std::endl;
    return true;
}

void V4L2Device::close() {
    if (is_open()) {
        if (::close(fd_) < 0) {
            std::cerr << "Ошибка закрытия устройства: " << strerror(errno) << std::endl;
        }
        fd_ = -1;
        poll_fds_.clear();
        std::cout << "Устройство закрыто" << std::endl;
    }
}

bool V4L2Device::ioctl_helper(unsigned long request, void* arg, const char* request_name) {
    if (!is_open()) {
        std::cerr << "Устройство не открыто для ioctl " << request_name << std::endl;
        return false;
    }
    if (ioctl(fd_, request, arg) < 0) {
        std::cerr << "Ошибка ioctl " << request_name << ": " << strerror(errno) << std::endl;
        return false;
    }
    return true;
}

bool V4L2Device::query_capability(v4l2_capability& cap) {
    return ioctl_helper(VIDIOC_QUERYCAP, &cap, "VIDIOC_QUERYCAP");
}

bool V4L2Device::set_format(v4l2_format& fmt) {
    return ioctl_helper(VIDIOC_S_FMT, &fmt, "VIDIOC_S_FMT");
}

bool V4L2Device::get_format(v4l2_format& fmt) {
    return ioctl_helper(VIDIOC_G_FMT, &fmt, "VIDIOC_G_FMT");
}

bool V4L2Device::set_control(const v4l2_control& ctrl) {
    // Создаем копию, так как ioctl может изменять структуру
    v4l2_control temp_ctrl = ctrl;
    return ioctl_helper(VIDIOC_S_CTRL, &temp_ctrl, "VIDIOC_S_CTRL");
}

bool V4L2Device::request_buffers(v4l2_requestbuffers& req) {
    return ioctl_helper(VIDIOC_REQBUFS, &req, "VIDIOC_REQBUFS");
}

bool V4L2Device::queue_buffer(v4l2_buffer& buf) {
    return ioctl_helper(VIDIOC_QBUF, &buf, "VIDIOC_QBUF");
}

bool V4L2Device::dequeue_buffer(v4l2_buffer& buf) {
    // Этот ioctl может возвращать EAGAIN, что не является фатальной ошибкой
    if (!is_open()) {
        std::cerr << "Устройство не открыто для ioctl VIDIOC_DQBUF" << std::endl;
        return false;
    }
    if (ioctl(fd_, VIDIOC_DQBUF, &buf) < 0) {
        if (errno != EAGAIN) {
            std::cerr << "Ошибка ioctl VIDIOC_DQBUF: " << strerror(errno) << std::endl;
        }
        return false;
    }
    return true;
}

bool V4L2Device::stream_on(enum v4l2_buf_type type) {
    return ioctl_helper(VIDIOC_STREAMON, &type, "VIDIOC_STREAMON");
}

bool V4L2Device::stream_off(enum v4l2_buf_type type) {
    return ioctl_helper(VIDIOC_STREAMOFF, &type, "VIDIOC_STREAMOFF");
}

bool V4L2Device::subscribe_to_events() {
    v4l2_event_subscription sub_eos = {};
    sub_eos.type = V4L2_EVENT_EOS;
    if (!subscribe_event(sub_eos)) return false;

    v4l2_event_subscription sub_source_change = {};
    sub_source_change.type = V4L2_EVENT_SOURCE_CHANGE;
    if (!subscribe_event(sub_source_change)) return false;

    std::cout << "✅ Подписка на события V4L2_EVENT_EOS и V4L2_EVENT_SOURCE_CHANGE" << std::endl;
    return true;
}

bool V4L2Device::dequeue_event(v4l2_event& ev) {
    return ioctl_helper(VIDIOC_DQEVENT, &ev, "VIDIOC_DQEVENT");
}

bool V4L2Device::poll(short events, int timeout_ms) {
    if (!is_open()) {
        std::cerr << "Устройство не открыто для poll" << std::endl;
        return false;
    }
    poll_fds_[0].events = events;
    int ret = ::poll(poll_fds_.data(), poll_fds_.size(), timeout_ms);
    if (ret < 0) {
        std::cerr << "Ошибка poll: " << strerror(errno) << std::endl;
        revents_ = 0;
        return false;
    }
    if (ret == 0) {
        // Timeout
        revents_ = 0;
        return true;
    }
    revents_ = poll_fds_[0].revents;
    return true;
}

bool V4L2Device::has_event() const {
    return revents_ & POLLPRI;
}

bool V4L2Device::has_error() const {
    return revents_ & POLLERR;
}

bool V4L2Device::is_ready_for_read() const {
    return revents_ & POLLIN;
}

bool V4L2Device::is_ready_for_write() const {
    return revents_ & POLLOUT;
}

bool V4L2Device::subscribe_event(v4l2_event_subscription& sub) {
    return ioctl_helper(VIDIOC_SUBSCRIBE_EVENT, &sub, "VIDIOC_SUBSCRIBE_EVENT");
}

bool V4L2Device::configure_decoder_formats(uint32_t width, uint32_t height, uint32_t in_pixel_format, uint32_t out_pixel_format) {
    struct v4l2_format fmt_in = {};
    fmt_in.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt_in.fmt.pix_mp.width = width;
    fmt_in.fmt.pix_mp.height = height;
    fmt_in.fmt.pix_mp.pixelformat = in_pixel_format;
    fmt_in.fmt.pix_mp.num_planes = 1;
    fmt_in.fmt.pix_mp.plane_fmt[0].sizeimage = 2 * 1024 * 1024; // 2MB for H.264
    if (!set_format(fmt_in)) {
        std::cerr << "❌ ОШИБКА: Не удалось установить входной формат" << std::endl;
        return false;
    }
    std::cout << "Входной формат установлен: " << width << "x" << height << std::endl;

    struct v4l2_format fmt_out = {};
    fmt_out.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt_out.fmt.pix_mp.width = width;
    fmt_out.fmt.pix_mp.height = height;
    fmt_out.fmt.pix_mp.pixelformat = out_pixel_format;
    fmt_out.fmt.pix_mp.num_planes = 1;
    if (!set_format(fmt_out)) {
        std::cerr << "❌ ОШИБКА: Не удалось установить выходной формат" << std::endl;
        return false;
    }
    std::cout << "Выходной формат установлен: " << width << "x" << height << std::endl;

    // Устанавливаем минимальное количество буферов для захвата для минимальной задержки
    struct v4l2_control ctrl = {};
    ctrl.id = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
    ctrl.value = 1;
    if (!set_control(ctrl)) {
        std::cout << "⚠️ ПРЕДУПРЕЖДЕНИЕ: Не удалось установить V4L2_CID_MIN_BUFFERS_FOR_CAPTURE=1. Это может увеличить задержку." << std::endl;
        // Не критично, продолжаем
    } else {
        std::cout << "✅ Установлена минимальная буферизация на выходе (MIN_BUFFERS_FOR_CAPTURE=1) для низкой задержки" << std::endl;
    }

    return true;
}

bool V4L2Device::check_dma_buf_support() {
    // Пытаемся запросить буферы в режиме DMA-buf для тестирования
    struct v4l2_requestbuffers req = {};
    req.count = 1;
    req.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    req.memory = V4L2_MEMORY_DMABUF;

    bool supported = request_buffers(req);

    std::cout << "Проверка поддержки DMA-buf: " << (supported ? "OK" : "FAIL")
              << std::endl;

    // Сбрасываем буферы после тестирования
    if (supported) {
        req.count = 0;
        (void)request_buffers(req);
    }

    return supported;
}

bool V4L2Device::initialize_for_decoding(std::string_view device_path) {
    std::cout << "Инициализация V4L2 устройства для декодирования: " << device_path << std::endl;

    if (!open(device_path)) {
        return false;
    }

    struct v4l2_capability cap;
    if (!query_capability(cap)) {
        close();
        return false;
    }

    std::cout << "Устройство: " << cap.card << "\nДрайвер: " << cap.driver << std::endl;

    if (!(cap.capabilities & V4L2_CAP_VIDEO_M2M_MPLANE)) [[unlikely]] {
        std::cerr << "❌ ОШИБКА: Устройство не поддерживает V4L2_CAP_VIDEO_M2M_MPLANE" << std::endl;
        std::cerr << "Доступные возможности: 0x" << std::hex << cap.capabilities << std::dec << std::endl;
        close();
        return false;
    }

    // Проверяем поддержку DMA-buf в V4L2 драйвере
    if (!check_dma_buf_support()) {
        std::cerr << "❌ КРИТИЧЕСКАЯ ОШИБКА: V4L2 драйвер не поддерживает DMA-buf" << std::endl;
        std::cerr << "Возможные причины:" << std::endl;
        std::cerr << "  - Устаревший драйвер" << std::endl;
        std::cerr << "  - Неправильная конфигурация ядра" << std::endl;
        close();
        return false;
    }

    std::cout << "Используем DMA-buf буферы" << std::endl;

    // Подписываемся на критические V4L2 события
    if (!subscribe_to_events()) {
        std::cerr << "⚠️ ПРЕДУПРЕЖДЕНИЕ: Не удалось подписаться на V4L2 события" << std::endl;
        // Продолжаем работу - события не критичны для базовой функциональности
    }

    return true;
}
