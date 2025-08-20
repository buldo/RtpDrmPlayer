namespace RtpDrmPlayer.Native;

public static class V4L2Const
{
    private static uint FCC(char a, char b, char c, char d) => (uint)(byte)a | ((uint)(byte)b << 8) | ((uint)(byte)c << 16) | ((uint)(byte)d << 24);
    public static readonly uint V4L2_PIX_FMT_H264 = FCC('H','2','6','4');
    public static readonly uint V4L2_PIX_FMT_YUV420 = FCC('Y','U','1','2');
    public static readonly uint V4L2_PIX_FMT_NV12 = FCC('N','V','1','2');

    public const uint V4L2_CAP_VIDEO_M2M_MPLANE = 0x00004000;
    public const uint V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE = 9;
    public const uint V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE = 10;
    public const uint V4L2_MEMORY_MMAP = 1;
    public const uint V4L2_MEMORY_USERPTR = 2;
    public const uint V4L2_MEMORY_OVERLAY = 3;
    public const uint V4L2_MEMORY_DMABUF = 4;
    public const uint V4L2_EVENT_EOS = 5;
    public const uint V4L2_EVENT_SOURCE_CHANGE = 6;
    public const uint V4L2_EVENT_FRAME_SYNC = 7;
    public const uint V4L2_EVENT_SRC_CH_RESOLUTION = 0x00000001;
    public const uint V4L2_CID_MIN_BUFFERS_FOR_CAPTURE = 0x0098092c;
    public const uint V4L2_BUF_FLAG_ERROR = 0x0040;
    public const uint V4L2_BUF_FLAG_LAST = 0x4000;
    public const ulong VIDIOC_QUERYCAP = 0x80685600;
    public const ulong VIDIOC_G_FMT = 0xc0cc5604;
    public const ulong VIDIOC_S_FMT = 0xc0cc5605;
    public const ulong VIDIOC_REQBUFS = 0xc0145608;
    public const ulong VIDIOC_QBUF = 0xc058560f;
    public const ulong VIDIOC_DQBUF = 0xc0585611;
    public const ulong VIDIOC_STREAMON = 0x40045612;
    public const ulong VIDIOC_STREAMOFF = 0x40045613;
    public const ulong VIDIOC_S_CTRL = 0xc008561c;
    public const ulong VIDIOC_SUBSCRIBE_EVENT = 0x4020565a;
    public const ulong VIDIOC_DQEVENT = 0x80805659;
}