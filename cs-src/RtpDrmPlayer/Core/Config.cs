using RtpDrmPlayer.Native;

namespace RtpDrmPlayer.Core;

// Аналог DecoderConfig из C++
public class DecoderConfig
{
    public string DevicePath { get; set; } = "/dev/video0";
    public uint Width { get; set; } = 1920;
    public uint Height { get; set; } = 1080;
    public uint InputCodec { get; set; } = V4L2Const.V4L2_PIX_FMT_H264;
    public uint OutputPixelFormat { get; set; } = V4L2Const.V4L2_PIX_FMT_YUV420;
    public int InputBufferCount { get; set; } = 6;
    public int OutputBufferCount { get; set; } = 4;
    public ulong DefaultInputBufferSize { get; set; } = 2UL * 1024 * 1024; // 2MB
}
