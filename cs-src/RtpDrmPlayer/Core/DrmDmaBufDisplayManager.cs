using System;

namespace RtpDrmPlayer.Core;

public class DrmDmaBufDisplayManager : IDisposable
{
    public class FrameInfo { public IntPtr Data; public int DmaFd; public uint Width; public uint Height; public uint Format; public ulong Size; public bool IsDmaBuf; }
    public bool Initialize(uint w,uint h){ Console.WriteLine($"[DRM STUB] init {w}x{h}"); return true; }
    public bool DisplayFrame(FrameInfo f){ Console.WriteLine($"[DRM STUB] display fd={f.DmaFd} size={f.Size}"); return true; }
    public bool SetupZeroCopyBuffer(int fd,uint w,uint h){ Console.WriteLine($"[DRM STUB] zero-copy fd={fd} {w}x{h}"); return true; }
    public string GetDisplayInfo()=>"DRM STUB"; public void Dispose(){}
}
