using System;
using System.Collections.Generic;

namespace RtpDrmPlayer.Core;

public class DmaBuffersManager
{
    private readonly DmaBufAllocator _alloc; private readonly int _count; private readonly List<DmaBufAllocator.DmaBufInfo> _buffers=new(); private readonly bool[] _inUse; private int _cur;
    public DmaBuffersManager(DmaBufAllocator a,int count,uint type){ _alloc=a; _count=count; _inUse=new bool[count]; }
    public bool Allocate(ulong size){ Deallocate(); for(int i=0;i<_count;i++){ var b=_alloc.Allocate(size); if(b.Fd<0||!_alloc.Map(b)){ Deallocate(); return false;} _buffers.Add(b);} return true; }
    public void Deallocate(){ foreach(var b in _buffers) _alloc.Deallocate(b); _buffers.Clear(); Array.Clear(_inUse); _cur=0; }
    public int Count=>_count; public DmaBufAllocator.DmaBufInfo this[int i]=>_buffers[i];
    public int GetFree(){ for(int i=0;i<_count;i++){ int idx=(_cur+i)%_count; if(!_inUse[idx]) return idx;} return -1; }
    public void MarkInUse(int idx){ if(idx<0||idx>=_count) return; _inUse[idx]=true; if(idx==(_cur%_count)) _cur=(idx+1)%_count; }
    public void MarkFree(int idx){ if(idx<0||idx>=_count) return; _inUse[idx]=false; }
}
