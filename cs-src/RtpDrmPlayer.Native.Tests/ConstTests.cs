using System;
using System.IO;
using Xunit;
using RtpDrmPlayer.Native;
using RtpDrmPlayer.Native.Tests.Interop;

namespace RtpDrmPlayer.Native.Tests;

public class ConstTests
{
    private static readonly Lazy<NativeLoader.ExportedConsts> _native = new(() =>
    {
        var dir = Path.Combine(AppContext.BaseDirectory, "native");
        NativeLoader.BuildNativeLibrary(dir);
        return NativeLoader.Load();
    });

    private NativeLoader.ExportedConsts N => _native.Value;

    [Fact] public void V4L2_CAP_VIDEO_M2M_MPLANE_Match() => Assert.Equal(N.V4L2_CAP_VIDEO_M2M_MPLANE, V4L2Const.V4L2_CAP_VIDEO_M2M_MPLANE);
    [Fact] public void V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE_Match() => Assert.Equal(N.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
    [Fact] public void V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE_Match() => Assert.Equal(N.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
    [Fact] public void V4L2_MEMORY_DMABUF_Match() => Assert.Equal(N.V4L2_MEMORY_DMABUF, V4L2Const.V4L2_MEMORY_DMABUF);
    [Fact] public void V4L2_EVENT_EOS_Match() => Assert.Equal(N.V4L2_EVENT_EOS, V4L2Const.V4L2_EVENT_EOS);
    [Fact] public void V4L2_EVENT_SOURCE_CHANGE_Match() => Assert.Equal(N.V4L2_EVENT_SOURCE_CHANGE, V4L2Const.V4L2_EVENT_SOURCE_CHANGE);
    [Fact] public void V4L2_EVENT_FRAME_SYNC_Match() => Assert.Equal(N.V4L2_EVENT_FRAME_SYNC, V4L2Const.V4L2_EVENT_FRAME_SYNC);
    [Fact] public void V4L2_EVENT_SRC_CH_RESOLUTION_Match() => Assert.Equal(N.V4L2_EVENT_SRC_CH_RESOLUTION, V4L2Const.V4L2_EVENT_SRC_CH_RESOLUTION);
    [Fact] public void V4L2_CID_MIN_BUFFERS_FOR_CAPTURE_Match() => Assert.Equal(N.V4L2_CID_MIN_BUFFERS_FOR_CAPTURE, V4L2Const.V4L2_CID_MIN_BUFFERS_FOR_CAPTURE);
    [Fact] public void V4L2_BUF_FLAG_ERROR_Match() => Assert.Equal(N.V4L2_BUF_FLAG_ERROR, V4L2Const.V4L2_BUF_FLAG_ERROR);
    [Fact] public void V4L2_BUF_FLAG_LAST_Match() => Assert.Equal(N.V4L2_BUF_FLAG_LAST, V4L2Const.V4L2_BUF_FLAG_LAST);
    [Fact] public void VIDIOC_QUERYCAP_Match() => Assert.Equal(N.VIDIOC_QUERYCAP, V4L2Const.VIDIOC_QUERYCAP);
    [Fact] public void VIDIOC_G_FMT_Match() => Assert.Equal(N.VIDIOC_G_FMT, V4L2Const.VIDIOC_G_FMT);
    [Fact] public void VIDIOC_S_FMT_Match() => Assert.Equal(N.VIDIOC_S_FMT, V4L2Const.VIDIOC_S_FMT);
    [Fact] public void VIDIOC_REQBUFS_Match() => Assert.Equal(N.VIDIOC_REQBUFS, V4L2Const.VIDIOC_REQBUFS);
    [Fact] public void VIDIOC_QBUF_Match() => Assert.Equal(N.VIDIOC_QBUF, V4L2Const.VIDIOC_QBUF);
    [Fact] public void VIDIOC_DQBUF_Match() => Assert.Equal(N.VIDIOC_DQBUF, V4L2Const.VIDIOC_DQBUF);
    [Fact] public void VIDIOC_STREAMON_Match() => Assert.Equal(N.VIDIOC_STREAMON, V4L2Const.VIDIOC_STREAMON);
    [Fact] public void VIDIOC_STREAMOFF_Match() => Assert.Equal(N.VIDIOC_STREAMOFF, V4L2Const.VIDIOC_STREAMOFF);
    [Fact] public void VIDIOC_S_CTRL_Match() => Assert.Equal(N.VIDIOC_S_CTRL, V4L2Const.VIDIOC_S_CTRL);
    [Fact] public void VIDIOC_SUBSCRIBE_EVENT_Match() => Assert.Equal(N.VIDIOC_SUBSCRIBE_EVENT, V4L2Const.VIDIOC_SUBSCRIBE_EVENT);
    [Fact] public void VIDIOC_DQEVENT_Match() => Assert.Equal(N.VIDIOC_DQEVENT, V4L2Const.VIDIOC_DQEVENT);
    [Fact] public void DMA_BUF_IOCTL_SYNC_Match() => Assert.Equal(N.DMA_BUF_IOCTL_SYNC, DmaBufConst.DMA_BUF_IOCTL_SYNC);
    [Fact] public void PIX_FMT_H264_Match() => Assert.Equal(N.V4L2_PIX_FMT_H264, V4L2Const.V4L2_PIX_FMT_H264);
    [Fact] public void PIX_FMT_YUV420_Match() => Assert.Equal(N.V4L2_PIX_FMT_YUV420, V4L2Const.V4L2_PIX_FMT_YUV420);
    [Fact] public void DMA_BUF_SYNC_START_Match() => Assert.Equal(N.DMA_BUF_SYNC_START, DmaBufConst.DMA_BUF_SYNC_START);
    [Fact] public void DMA_BUF_SYNC_END_Match() => Assert.Equal(N.DMA_BUF_SYNC_END, DmaBufConst.DMA_BUF_SYNC_END);
    [Fact] public void DMA_BUF_SYNC_RW_Match() => Assert.Equal(N.DMA_BUF_SYNC_RW, DmaBufConst.DMA_BUF_SYNC_RW);
    [Fact] public void O_RDWR_Match() => Assert.Equal(N.O_RDWR_, LibC.O_RDWR);
    [Fact] public void O_NONBLOCK_Match() => Assert.Equal(N.O_NONBLOCK_, LibC.O_NONBLOCK);
    [Fact] public void O_CLOEXEC_Match() => Assert.Equal(N.O_CLOEXEC_, LibC.O_CLOEXEC);
    [Fact] public void PROT_READ_Match() => Assert.Equal(N.PROT_READ_, LibC.PROT_READ);
    [Fact] public void PROT_WRITE_Match() => Assert.Equal(N.PROT_WRITE_, LibC.PROT_WRITE);
    [Fact] public void MAP_SHARED_Match() => Assert.Equal(N.MAP_SHARED_, LibC.MAP_SHARED);
    [Fact] public void POLLIN_Match() => Assert.Equal(N.POLLIN_, LibC.POLLIN);
    [Fact] public void POLLPRI_Match() => Assert.Equal(N.POLLPRI_, LibC.POLLPRI);
    [Fact] public void POLLOUT_Match() => Assert.Equal(N.POLLOUT_, LibC.POLLOUT);
    [Fact] public void POLLERR_Match() => Assert.Equal(N.POLLERR_, LibC.POLLERR);
}
