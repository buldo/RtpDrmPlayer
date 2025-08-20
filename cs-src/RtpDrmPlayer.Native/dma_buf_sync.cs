using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)]
public struct dma_buf_sync
{
    public uint flags; // DMA_BUF_SYNC_* | direction
}