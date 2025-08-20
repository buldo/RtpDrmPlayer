#include <linux/videodev2.h>
#include <linux/dma-buf.h>
#include <stdint.h>

struct exported_consts {
    uint32_t V4L2_CAP_VIDEO_M2M_MPLANE;
    uint32_t V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    uint32_t V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    uint32_t V4L2_MEMORY_DMABUF;
    uint32_t V4L2_EVENT_EOS;
    uint32_t V4L2_EVENT_SOURCE_CHANGE;
    uint32_t V4L2_EVENT_FRAME_SYNC;
    uint32_t V4L2_EVENT_SRC_CH_RESOLUTION;
    uint32_t V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
    uint32_t V4L2_BUF_FLAG_ERROR;
    uint32_t V4L2_BUF_FLAG_LAST;
    unsigned long VIDIOC_QUERYCAP;
    unsigned long VIDIOC_G_FMT;
    unsigned long VIDIOC_S_FMT;
    unsigned long VIDIOC_REQBUFS;
    unsigned long VIDIOC_QBUF;
    unsigned long VIDIOC_DQBUF;
    unsigned long VIDIOC_STREAMON;
    unsigned long VIDIOC_STREAMOFF;
    unsigned long VIDIOC_S_CTRL;
    unsigned long VIDIOC_SUBSCRIBE_EVENT;
    unsigned long VIDIOC_DQEVENT;
    unsigned long DMA_BUF_IOCTL_SYNC;
    /* Pixel formats */
    uint32_t V4L2_PIX_FMT_H264;
    uint32_t V4L2_PIX_FMT_YUV420;
    /* DMA BUF sync flags */
    uint32_t DMA_BUF_SYNC_START;
    uint32_t DMA_BUF_SYNC_END;
    uint32_t DMA_BUF_SYNC_RW;
    /* Libc flags */
    int O_RDWR_;
    int O_NONBLOCK_;
    int O_CLOEXEC_;
    int PROT_READ_;
    int PROT_WRITE_;
    int MAP_SHARED_;
    short POLLIN_;
    short POLLPRI_;
    short POLLOUT_;
    short POLLERR_;
};

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((visibility("default"))) struct exported_consts get_exported_consts() {
    struct exported_consts c = {
        .V4L2_CAP_VIDEO_M2M_MPLANE = V4L2_CAP_VIDEO_M2M_MPLANE,
        .V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        .V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
        .V4L2_MEMORY_DMABUF = V4L2_MEMORY_DMABUF,
        .V4L2_EVENT_EOS = V4L2_EVENT_EOS,
        .V4L2_EVENT_SOURCE_CHANGE = V4L2_EVENT_SOURCE_CHANGE,
        .V4L2_EVENT_FRAME_SYNC = V4L2_EVENT_FRAME_SYNC,
        .V4L2_EVENT_SRC_CH_RESOLUTION = V4L2_EVENT_SRC_CH_RESOLUTION,
        .V4L2_CID_MIN_BUFFERS_FOR_CAPTURE = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
        .V4L2_BUF_FLAG_ERROR = V4L2_BUF_FLAG_ERROR,
        .V4L2_BUF_FLAG_LAST = V4L2_BUF_FLAG_LAST,
        .VIDIOC_QUERYCAP = VIDIOC_QUERYCAP,
        .VIDIOC_G_FMT = VIDIOC_G_FMT,
        .VIDIOC_S_FMT = VIDIOC_S_FMT,
        .VIDIOC_REQBUFS = VIDIOC_REQBUFS,
        .VIDIOC_QBUF = VIDIOC_QBUF,
        .VIDIOC_DQBUF = VIDIOC_DQBUF,
        .VIDIOC_STREAMON = VIDIOC_STREAMON,
        .VIDIOC_STREAMOFF = VIDIOC_STREAMOFF,
        .VIDIOC_S_CTRL = VIDIOC_S_CTRL,
        .VIDIOC_SUBSCRIBE_EVENT = VIDIOC_SUBSCRIBE_EVENT,
        .VIDIOC_DQEVENT = VIDIOC_DQEVENT,
        .DMA_BUF_IOCTL_SYNC = DMA_BUF_IOCTL_SYNC,
    .V4L2_PIX_FMT_H264 = V4L2_PIX_FMT_H264,
    .V4L2_PIX_FMT_YUV420 = V4L2_PIX_FMT_YUV420,
    .DMA_BUF_SYNC_START = DMA_BUF_SYNC_START,
    .DMA_BUF_SYNC_END = DMA_BUF_SYNC_END,
    .DMA_BUF_SYNC_RW = DMA_BUF_SYNC_RW,
    .O_RDWR_ = O_RDWR,
    .O_NONBLOCK_ = O_NONBLOCK,
    .O_CLOEXEC_ = O_CLOEXEC,
    .PROT_READ_ = PROT_READ,
    .PROT_WRITE_ = PROT_WRITE,
    .MAP_SHARED_ = MAP_SHARED,
    .POLLIN_ = POLLIN,
    .POLLPRI_ = POLLPRI,
    .POLLOUT_ = POLLOUT,
    .POLLERR_ = POLLERR,
    };
    return c;
}

#ifdef __cplusplus
}
#endif
