namespace RtpDrmPlayer.Native;

public static class DmaBufConst
{
    public const ulong DMA_BUF_IOCTL_SYNC = 0x40086200;
    public const uint DMA_BUF_SYNC_START = 1<<0;
    public const uint DMA_BUF_SYNC_END = 1<<1;
    public const uint DMA_BUF_SYNC_RW = 1<<2;
}