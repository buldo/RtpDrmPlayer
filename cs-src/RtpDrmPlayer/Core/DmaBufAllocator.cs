using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using RtpDrmPlayer.Native;

namespace RtpDrmPlayer.Core;

public class DmaBufAllocator : IDisposable
{
    public class DmaBufInfo { public int Fd=-1; public IntPtr Mapped=IntPtr.Zero; public ulong Size; }
    private int _heapFd=-1; private bool _supported; private readonly List<DmaBufInfo> _all=new();
    private const ulong DMA_HEAP_IOCTL_ALLOC=0xC0184800;
    [StructLayout(LayoutKind.Sequential)] private struct dma_heap_allocation_data { public ulong len; public uint fd; public uint fd_flags; public ulong heap_flags; }
    public bool Initialize(){ foreach(var p in new[]{"/dev/dma_heap/vidbuf_cached","/dev/dma_heap/linux,cma"}){ _heapFd=LibC.open(p,LibC.O_RDWR|LibC.O_CLOEXEC,0); if(_heapFd>=0){ _supported=true; return true; } } return false; }
    public DmaBufInfo Allocate(ulong size){ if(!_supported) return new(); var d=new dma_heap_allocation_data{ len=size, fd_flags=(uint)(LibC.O_RDWR|LibC.O_CLOEXEC)}; unsafe{ if(LibC.ioctl(_heapFd,DMA_HEAP_IOCTL_ALLOC,(IntPtr)(&d))!=0) return new(); } var info=new DmaBufInfo{Fd=(int)d.fd,Size=size}; _all.Add(info); return info; }
    public bool Map(DmaBufInfo info){ if(info.Fd<0) return false; var ptr=LibC.mmap(IntPtr.Zero,(UIntPtr)info.Size,LibC.PROT_READ|LibC.PROT_WRITE,LibC.MAP_SHARED,info.Fd,IntPtr.Zero); if(ptr==(IntPtr)(-1)) return false; info.Mapped=ptr; return true; }
    public void Unmap(DmaBufInfo info){ if(info.Mapped!=IntPtr.Zero){ LibC.munmap(info.Mapped,(UIntPtr)info.Size); info.Mapped=IntPtr.Zero; } }
    public void Deallocate(DmaBufInfo info){ Unmap(info); if(info.Fd>=0){ LibC.close(info.Fd); info.Fd=-1; } }
    public void Dispose(){ foreach(var b in _all) Deallocate(b); if(_heapFd>=0) LibC.close(_heapFd); }
}
