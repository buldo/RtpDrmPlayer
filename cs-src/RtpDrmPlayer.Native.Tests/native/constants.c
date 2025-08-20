#include <linux/videodev2.h>
#include <linux/dma-buf.h>
#include <drm/drm.h>
#include <stdint.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <poll.h>

struct exported_consts {
    uint32_t V4L2_CAP_VIDEO_M2M_MPLANE_;
    uint32_t V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE_;
    uint32_t V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE_;
    uint32_t V4L2_MEMORY_DMABUF_;
    uint32_t V4L2_EVENT_EOS_;
    uint32_t V4L2_EVENT_SOURCE_CHANGE_;
    uint32_t V4L2_EVENT_FRAME_SYNC_;
    uint32_t V4L2_EVENT_SRC_CH_RESOLUTION_;
    uint32_t V4L2_CID_MIN_BUFFERS_FOR_CAPTURE_;
    uint32_t V4L2_BUF_FLAG_ERROR_;
    uint32_t V4L2_BUF_FLAG_LAST_;
    unsigned long VIDIOC_QUERYCAP_;
    unsigned long VIDIOC_G_FMT_;
    unsigned long VIDIOC_S_FMT_;
    unsigned long VIDIOC_REQBUFS_;
    unsigned long VIDIOC_QBUF_;
    unsigned long VIDIOC_DQBUF_;
    unsigned long VIDIOC_STREAMON_;
    unsigned long VIDIOC_STREAMOFF_;
    unsigned long VIDIOC_S_CTRL_;
    unsigned long VIDIOC_SUBSCRIBE_EVENT_;
    unsigned long VIDIOC_DQEVENT_;
    unsigned long DMA_BUF_IOCTL_SYNC_;
    unsigned long DRM_IOCTL_PRIME_FD_TO_HANDLE_;
    unsigned long DRM_IOCTL_GEM_CLOSE_;
    unsigned long DRM_IOCTL_MODE_GETRESOURCES_;
    unsigned long DRM_IOCTL_MODE_GETCRTC_;
    unsigned long DRM_IOCTL_MODE_SETCRTC_;
    unsigned long DRM_IOCTL_MODE_GETENCODER_;
    unsigned long DRM_IOCTL_MODE_GETCONNECTOR_;
    unsigned long DRM_IOCTL_MODE_ADDFB2_;
    unsigned long DRM_IOCTL_MODE_RMFB_;
    unsigned long DRM_IOCTL_MODE_PAGE_FLIP_;
    /* Pixel formats */
    uint32_t V4L2_PIX_FMT_H264_;
    uint32_t V4L2_PIX_FMT_YUV420_;
    uint32_t V4L2_PIX_FMT_NV12_;
    /* DMA BUF sync flags */
    uint32_t DMA_BUF_SYNC_START_;
    uint32_t DMA_BUF_SYNC_END_;
    uint32_t DMA_BUF_SYNC_RW_;
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
    uint32_t SIZE_v4l2_pix_format_mplane;
};

#ifdef __cplusplus
extern "C" {
#endif

__attribute__((visibility("default"))) struct exported_consts get_exported_consts() {
    struct exported_consts c = {
        .V4L2_CAP_VIDEO_M2M_MPLANE_ = V4L2_CAP_VIDEO_M2M_MPLANE,
        .V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE_ = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
        .V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE_ = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
        .V4L2_MEMORY_DMABUF_ = V4L2_MEMORY_DMABUF,
        .V4L2_EVENT_EOS_ = V4L2_EVENT_EOS,
        .V4L2_EVENT_SOURCE_CHANGE_ = V4L2_EVENT_SOURCE_CHANGE,
        .V4L2_EVENT_FRAME_SYNC_ = V4L2_EVENT_FRAME_SYNC,
        .V4L2_EVENT_SRC_CH_RESOLUTION_ = V4L2_EVENT_SRC_CH_RESOLUTION,
        .V4L2_CID_MIN_BUFFERS_FOR_CAPTURE_ = V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,
        .V4L2_BUF_FLAG_ERROR_ = V4L2_BUF_FLAG_ERROR,
        .V4L2_BUF_FLAG_LAST_ = V4L2_BUF_FLAG_LAST,
        .VIDIOC_QUERYCAP_ = VIDIOC_QUERYCAP,
        .VIDIOC_G_FMT_ = VIDIOC_G_FMT,
        .VIDIOC_S_FMT_ = VIDIOC_S_FMT,
        .VIDIOC_REQBUFS_ = VIDIOC_REQBUFS,
        .VIDIOC_QBUF_ = VIDIOC_QBUF,
        .VIDIOC_DQBUF_ = VIDIOC_DQBUF,
        .VIDIOC_STREAMON_ = VIDIOC_STREAMON,
        .VIDIOC_STREAMOFF_ = VIDIOC_STREAMOFF,
        .VIDIOC_S_CTRL_ = VIDIOC_S_CTRL,
        .VIDIOC_SUBSCRIBE_EVENT_ = VIDIOC_SUBSCRIBE_EVENT,
        .VIDIOC_DQEVENT_ = VIDIOC_DQEVENT,
        .DMA_BUF_IOCTL_SYNC_ = DMA_BUF_IOCTL_SYNC,
        .DRM_IOCTL_PRIME_FD_TO_HANDLE_ = DRM_IOCTL_PRIME_FD_TO_HANDLE,
        .DRM_IOCTL_GEM_CLOSE_ = DRM_IOCTL_GEM_CLOSE,
        .DRM_IOCTL_MODE_GETRESOURCES_ = DRM_IOCTL_MODE_GETRESOURCES,
        .DRM_IOCTL_MODE_GETCRTC_ = DRM_IOCTL_MODE_GETCRTC,
        .DRM_IOCTL_MODE_SETCRTC_ = DRM_IOCTL_MODE_SETCRTC,
        .DRM_IOCTL_MODE_GETENCODER_ = DRM_IOCTL_MODE_GETENCODER,
        .DRM_IOCTL_MODE_GETCONNECTOR_ = DRM_IOCTL_MODE_GETCONNECTOR,
        .DRM_IOCTL_MODE_ADDFB2_ = DRM_IOCTL_MODE_ADDFB2,
        .DRM_IOCTL_MODE_RMFB_ = DRM_IOCTL_MODE_RMFB,
        .DRM_IOCTL_MODE_PAGE_FLIP_ = DRM_IOCTL_MODE_PAGE_FLIP,
    .V4L2_PIX_FMT_H264_ = V4L2_PIX_FMT_H264,
    .V4L2_PIX_FMT_YUV420_ = V4L2_PIX_FMT_YUV420,
    .V4L2_PIX_FMT_NV12_ = V4L2_PIX_FMT_NV12,
    .DMA_BUF_SYNC_START_ = DMA_BUF_SYNC_START,
    .DMA_BUF_SYNC_END_ = DMA_BUF_SYNC_END,
    .DMA_BUF_SYNC_RW_ = DMA_BUF_SYNC_RW,
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
    .SIZE_v4l2_pix_format_mplane = sizeof(struct v4l2_pix_format_mplane),
    };
    return c;
}

#ifdef __cplusplus
}
#endif
