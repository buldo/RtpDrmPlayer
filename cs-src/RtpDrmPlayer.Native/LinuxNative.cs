using System;
using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

public static class LibC
{
    public const int O_RDWR = 0x0002;
    public const int O_NONBLOCK = 0x800;
    public const int O_CLOEXEC = 0x80000;
    [DllImport("libc", SetLastError = true, EntryPoint = "open", CharSet = CharSet.Ansi)]
    public static extern int open(string pathname, int flags, int mode = 0);
    [DllImport("libc", SetLastError = true)] public static extern int close(int fd);
    [DllImport("libc", SetLastError = true)] public static extern int ioctl(int fd, ulong request, IntPtr arg);
    [DllImport("libc", SetLastError = true)] public static extern IntPtr mmap(IntPtr addr, UIntPtr length, int prot, int flags, int fd, IntPtr offset);
    [DllImport("libc", SetLastError = true)] public static extern int munmap(IntPtr addr, UIntPtr length);
    [DllImport("libc", SetLastError = true)] public static extern int poll([In, Out] pollfd[] fds, uint nfds, int timeout);
    public const int PROT_READ = 0x1; public const int PROT_WRITE = 0x2; public const int MAP_SHARED = 0x01;
    public const short POLLIN = 0x0001; public const short POLLPRI = 0x0002; public const short POLLOUT = 0x0004; public const short POLLERR = 0x0008;
}

[StructLayout(LayoutKind.Sequential)] public struct pollfd { public int fd; public short events; public short revents; }

public static class V4L2Const
{
    private static uint FCC(char a, char b, char c, char d) => (uint)(byte)a | ((uint)(byte)b << 8) | ((uint)(byte)c << 16) | ((uint)(byte)d << 24);
    public static readonly uint V4L2_PIX_FMT_H264 = FCC('H','2','6','4');
    public static readonly uint V4L2_PIX_FMT_YUV420 = FCC('Y','U','1','2');
    public const uint V4L2_CAP_VIDEO_M2M_MPLANE = 0x00004000; public const uint V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE = 9; public const uint V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE = 10; public const uint V4L2_MEMORY_DMABUF = 4; public const uint V4L2_EVENT_EOS = 5; public const uint V4L2_EVENT_SOURCE_CHANGE = 6; public const uint V4L2_EVENT_FRAME_SYNC = 7; public const uint V4L2_EVENT_SRC_CH_RESOLUTION = 0x00000001; public const uint V4L2_CID_MIN_BUFFERS_FOR_CAPTURE = 0x0098092c; public const uint V4L2_BUF_FLAG_ERROR = 0x0040; public const uint V4L2_BUF_FLAG_LAST = 0x4000;
    public const ulong VIDIOC_QUERYCAP = 0x80685600; public const ulong VIDIOC_G_FMT = 0xc0cc5604; public const ulong VIDIOC_S_FMT = 0xc0cc5605; public const ulong VIDIOC_REQBUFS = 0xc0145608; public const ulong VIDIOC_QBUF = 0xc058560f; public const ulong VIDIOC_DQBUF = 0xc0585611; public const ulong VIDIOC_STREAMON = 0x40045612; public const ulong VIDIOC_STREAMOFF = 0x40045613; public const ulong VIDIOC_S_CTRL = 0xc008561c; public const ulong VIDIOC_SUBSCRIBE_EVENT = 0x4020565a; public const ulong VIDIOC_DQEVENT = 0x80805659;
}

[StructLayout(LayoutKind.Sequential)] public struct v4l2_capability { [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)] public byte[] driver; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public byte[] card; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public byte[] bus_info; public uint version; public uint capabilities; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public uint[] reserved; }
[StructLayout(LayoutKind.Sequential)] public struct v4l2_requestbuffers { public uint count; public uint type; public uint memory; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)] public uint[] reserved; }
[StructLayout(LayoutKind.Sequential)] public struct v4l2_pix_mp { public uint width; public uint height; public uint pixelformat; public uint field; public uint colorspace; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public uint[] plane_sizes; }
[StructLayout(LayoutKind.Sequential)] public struct v4l2_format { public uint type; public v4l2_pix_mp fmt; }
[StructLayout(LayoutKind.Sequential)] public struct v4l2_buffer { public uint index; public uint type; public uint bytesused; public uint flags; public uint field; public ulong timestamp_sec; public ulong timestamp_usec; public uint timecode_type; public uint sequence; public uint memory; public IntPtr m_planes; public uint length; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)] public uint[] reserved2; }
[StructLayout(LayoutKind.Sequential)] public struct v4l2_control { public uint id; public int value; }
[StructLayout(LayoutKind.Sequential)] public struct v4l2_event_subscription { public uint type; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 5)] public uint[] reserved; }
[StructLayout(LayoutKind.Sequential)] public struct v4l2_event { public uint type; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)] public uint[] u; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)] public uint[] reserved; }
public static class DmaBufConst { public const ulong DMA_BUF_IOCTL_SYNC = 0x40086200; public const uint DMA_BUF_SYNC_START = 1<<0; public const uint DMA_BUF_SYNC_END = 1<<1; public const uint DMA_BUF_SYNC_RW = 1<<2; }
[StructLayout(LayoutKind.Sequential)] public struct dma_buf_sync { public uint flags; public uint pad; }
