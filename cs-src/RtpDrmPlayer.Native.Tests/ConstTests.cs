using Xunit;
using RtpDrmPlayer.Native;
using System;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;
using RtpDrmPlayer.Native.Tests.Interop;

namespace RtpDrmPlayer.Native.Tests;

public class ConstTests
{
    private static readonly Lazy<NativeLoader.ExportedConsts> _native = new(() =>
    {
        NativeLoader.BuildNativeLibrary();
        return NativeLoader.Instance;
    });

    private NativeLoader.ExportedConsts N => _native.Value;

    [Fact]
    public void V4L2_CAP_VIDEO_M2M_MPLANE_Match() => Assert.Equal(V4L2Const.V4L2_CAP_VIDEO_M2M_MPLANE, N.V4L2_CAP_VIDEO_M2M_MPLANE_);
    [Fact]
    public void V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE_Match() => Assert.Equal(V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, N.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE_);
    [Fact]
    public void V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE_Match() => Assert.Equal(V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, N.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE_);
    [Fact]
    public void V4L2_MEMORY_DMABUF_Match() => Assert.Equal(V4L2Const.V4L2_MEMORY_DMABUF, N.V4L2_MEMORY_DMABUF_);
    [Fact]
    public void V4L2_EVENT_EOS_Match() => Assert.Equal(V4L2Const.V4L2_EVENT_EOS, N.V4L2_EVENT_EOS_);
    [Fact]
    public void V4L2_EVENT_SOURCE_CHANGE_Match() => Assert.Equal(V4L2Const.V4L2_EVENT_SOURCE_CHANGE, N.V4L2_EVENT_SOURCE_CHANGE_);
    [Fact]
    public void V4L2_EVENT_FRAME_SYNC_Match() => Assert.Equal(V4L2Const.V4L2_EVENT_FRAME_SYNC, N.V4L2_EVENT_FRAME_SYNC_);
    [Fact]
    public void V4L2_EVENT_SRC_CH_RESOLUTION_Match() => Assert.Equal(V4L2Const.V4L2_EVENT_SRC_CH_RESOLUTION, N.V4L2_EVENT_SRC_CH_RESOLUTION_);
    [Fact]
    public void V4L2_CID_MIN_BUFFERS_FOR_CAPTURE_Match() => Assert.Equal(V4L2Const.V4L2_CID_MIN_BUFFERS_FOR_CAPTURE, N.V4L2_CID_MIN_BUFFERS_FOR_CAPTURE_);
    [Fact]
    public void V4L2_BUF_FLAG_ERROR_Match() => Assert.Equal(V4L2Const.V4L2_BUF_FLAG_ERROR, N.V4L2_BUF_FLAG_ERROR_);
    [Fact]
    public void V4L2_BUF_FLAG_LAST_Match() => Assert.Equal(V4L2Const.V4L2_BUF_FLAG_LAST, N.V4L2_BUF_FLAG_LAST_);
    [Fact]
    public void VIDIOC_QUERYCAP_Match() => Assert.Equal(V4L2Const.VIDIOC_QUERYCAP, N.VIDIOC_QUERYCAP_);
    [Fact]
    public void VIDIOC_G_FMT_Match() => Assert.Equal(V4L2Const.VIDIOC_G_FMT, N.VIDIOC_G_FMT_);
    [Fact]
    public void VIDIOC_S_FMT_Match() => Assert.Equal(V4L2Const.VIDIOC_S_FMT, N.VIDIOC_S_FMT_);
    [Fact]
    public void VIDIOC_REQBUFS_Match() => Assert.Equal(V4L2Const.VIDIOC_REQBUFS, N.VIDIOC_REQBUFS_);
    [Fact]
    public void VIDIOC_QBUF_Match() => Assert.Equal(V4L2Const.VIDIOC_QBUF, N.VIDIOC_QBUF_);
    [Fact]
    public void VIDIOC_DQBUF_Match() => Assert.Equal(V4L2Const.VIDIOC_DQBUF, N.VIDIOC_DQBUF_);
    [Fact]
    public void VIDIOC_STREAMON_Match() => Assert.Equal(V4L2Const.VIDIOC_STREAMON, N.VIDIOC_STREAMON_);
    [Fact]
    public void VIDIOC_STREAMOFF_Match() => Assert.Equal(V4L2Const.VIDIOC_STREAMOFF, N.VIDIOC_STREAMOFF_);
    [Fact]
    public void VIDIOC_S_CTRL_Match() => Assert.Equal(V4L2Const.VIDIOC_S_CTRL, N.VIDIOC_S_CTRL_);
    [Fact]
    public void VIDIOC_SUBSCRIBE_EVENT_Match() => Assert.Equal(V4L2Const.VIDIOC_SUBSCRIBE_EVENT, N.VIDIOC_SUBSCRIBE_EVENT_);
    [Fact]
    public void VIDIOC_DQEVENT_Match() => Assert.Equal(V4L2Const.VIDIOC_DQEVENT, N.VIDIOC_DQEVENT_);
    [Fact]
    public void DMA_BUF_IOCTL_SYNC_Match() => Assert.Equal(DmaBufConst.DMA_BUF_IOCTL_SYNC, N.DMA_BUF_IOCTL_SYNC_);
    [Fact]
    public void DRM_IOCTL_PRIME_FD_TO_HANDLE_Match() => Assert.Equal(DrmConst.DRM_IOCTL_PRIME_FD_TO_HANDLE, N.DRM_IOCTL_PRIME_FD_TO_HANDLE_);
    [Fact]
    public void DRM_IOCTL_GEM_CLOSE_Match() => Assert.Equal(DrmConst.DRM_IOCTL_GEM_CLOSE, N.DRM_IOCTL_GEM_CLOSE_);
    [Fact]
    public void DRM_IOCTL_MODE_GETRESOURCES_Match() => Assert.Equal(DrmConst.DRM_IOCTL_MODE_GETRESOURCES, N.DRM_IOCTL_MODE_GETRESOURCES_);
    [Fact]
    public void DRM_IOCTL_MODE_GETCRTC_Match() => Assert.Equal(DrmConst.DRM_IOCTL_MODE_GETCRTC, N.DRM_IOCTL_MODE_GETCRTC_);
    [Fact]
    public void DRM_IOCTL_MODE_SETCRTC_Match() => Assert.Equal(DrmConst.DRM_IOCTL_MODE_SETCRTC, N.DRM_IOCTL_MODE_SETCRTC_);
    [Fact]
    public void DRM_IOCTL_MODE_GETENCODER_Match() => Assert.Equal(DrmConst.DRM_IOCTL_MODE_GETENCODER, N.DRM_IOCTL_MODE_GETENCODER_);
    [Fact]
    public void DRM_IOCTL_MODE_GETCONNECTOR_Match() => Assert.Equal(DrmConst.DRM_IOCTL_MODE_GETCONNECTOR, N.DRM_IOCTL_MODE_GETCONNECTOR_);
    [Fact]
    public void DRM_IOCTL_MODE_ADDFB2_Match() => Assert.Equal(DrmConst.DRM_IOCTL_MODE_ADDFB2, N.DRM_IOCTL_MODE_ADDFB2_);
    [Fact]
    public void DRM_IOCTL_MODE_RMFB_Match() => Assert.Equal(DrmConst.DRM_IOCTL_MODE_RMFB, N.DRM_IOCTL_MODE_RMFB_);
    [Fact]
    public void DRM_IOCTL_MODE_PAGE_FLIP_Match() => Assert.Equal(DrmConst.DRM_IOCTL_MODE_PAGE_FLIP, N.DRM_IOCTL_MODE_PAGE_FLIP_);
    [Fact]
    public void V4L2_PIX_FMT_H264_Match() => Assert.Equal(V4L2Const.V4L2_PIX_FMT_H264, N.V4L2_PIX_FMT_H264_);
    [Fact]
    public void V4L2_PIX_FMT_YUV420_Match() => Assert.Equal(V4L2Const.V4L2_PIX_FMT_YUV420, N.V4L2_PIX_FMT_YUV420_);
    [Fact]
    public void V4L2_PIX_FMT_NV12_Match() => Assert.Equal(V4L2Const.V4L2_PIX_FMT_NV12, N.V4L2_PIX_FMT_NV12_);
    [Fact]
    public void DMA_BUF_SYNC_START_Match() => Assert.Equal(DmaBufConst.DMA_BUF_SYNC_START, N.DMA_BUF_SYNC_START_);
    [Fact]
    public void DMA_BUF_SYNC_END_Match() => Assert.Equal(DmaBufConst.DMA_BUF_SYNC_END, N.DMA_BUF_SYNC_END_);
    [Fact]
    public void DMA_BUF_SYNC_RW_Match() => Assert.Equal(DmaBufConst.DMA_BUF_SYNC_RW, N.DMA_BUF_SYNC_RW_);
    [Fact]
    public void O_RDWR_Match() => Assert.Equal(LibC.O_RDWR, N.O_RDWR_);
    [Fact]
    public void O_NONBLOCK_Match() => Assert.Equal(LibC.O_NONBLOCK, N.O_NONBLOCK_);
    [Fact]
    public void O_CLOEXEC_Match() => Assert.Equal(LibC.O_CLOEXEC, N.O_CLOEXEC_);
    [Fact]
    public void PROT_READ_Match() => Assert.Equal(LibC.PROT_READ, N.PROT_READ_);
    [Fact]
    public void PROT_WRITE_Match() => Assert.Equal(LibC.PROT_WRITE, N.PROT_WRITE_);
    [Fact]
    public void MAP_SHARED_Match() => Assert.Equal(LibC.MAP_SHARED, N.MAP_SHARED_);
    [Fact]
    public void POLLIN_Match() => Assert.Equal(LibC.POLLIN, N.POLLIN_);
    [Fact]
    public void POLLPRI_Match() => Assert.Equal(LibC.POLLPRI, N.POLLPRI_);
    [Fact]
    public void POLLOUT_Match() => Assert.Equal(LibC.POLLOUT, N.POLLOUT_);
    [Fact]
    public void POLLERR_Match() => Assert.Equal(LibC.POLLERR, N.POLLERR_);
    [Fact]
    public void SIZE_v4l2_pix_format_mplane_Match() => Assert.Equal((uint)Marshal.SizeOf<v4l2_pix_mp>(), N.SIZE_v4l2_pix_format_mplane);
    [Fact]
    public void SIZE_v4l2_buffer_Match() => Assert.Equal((uint)Marshal.SizeOf<v4l2_buffer>(), N.SIZE_v4l2_buffer);
    [Fact]
    public void SIZE_v4l2_plane_Match() => Assert.Equal((uint)Marshal.SizeOf<v4l2_plane>(), N.SIZE_v4l2_plane);
    [Fact]
    public void SIZE_v4l2_format_Match() => Assert.Equal((uint)Marshal.SizeOf<v4l2_format>(), N.SIZE_v4l2_format);
    [Fact]
    public void SIZE_v4l2_plane_pix_format_Match() => Assert.Equal((uint)Marshal.SizeOf<v4l2_plane_pix_format>(), N.SIZE_v4l2_plane_pix_format);
    [Fact]
    public void SIZE_v4l2_event_subscription_Match() => Assert.Equal((uint)Marshal.SizeOf<v4l2_event_subscription>(), N.SIZE_v4l2_event_subscription);
    [Fact]
    public void SIZE_v4l2_event_Match() => Assert.Equal((uint)Marshal.SizeOf<v4l2_event>(), N.SIZE_v4l2_event);
    [Fact]
    public void SIZE_v4l2_requestbuffers_Match() => Assert.Equal((uint)Marshal.SizeOf<v4l2_requestbuffers>(), N.SIZE_v4l2_requestbuffers);
    [Fact]
    public void SIZE_v4l2_capability_Match() => Assert.Equal((uint)Marshal.SizeOf<v4l2_capability>(), N.SIZE_v4l2_capability);
    [Fact]
    public void SIZE_v4l2_control_Match() => Assert.Equal((uint)Marshal.SizeOf<v4l2_control>(), N.SIZE_v4l2_control);
}
