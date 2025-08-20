namespace RtpDrmPlayer.Native;

public static class DmaBufConst
{
    public const ulong DMA_BUF_IOCTL_SYNC = 0x40086200;
    public const uint DMA_BUF_SYNC_START = 0;
    public const uint DMA_BUF_SYNC_END = 4;
    public const uint DMA_BUF_SYNC_RW = 3;
}