using System;
using RtpDrmPlayer.Native;

namespace RtpDrmPlayer.Core;

public class FrameProcessor
{
    private DrmDmaBufDisplayManager? _disp; private readonly DmaBuffersManager _out; private readonly Func<int,bool>? _setup; private uint _w; private uint _h; private int _decoded; private readonly bool[] _zeroCopyInit; private readonly object _lock=new();
    public FrameProcessor(DrmDmaBufDisplayManager? d,DmaBuffersManager o,uint w,uint h,Func<int,bool>? setup,int bufferCount){ _disp=d; _out=o; _w=w; _h=h; _setup=setup; _zeroCopyInit=new bool[bufferCount]; }
    public void UpdateDimensions(uint w,uint h){ _w=w; _h=h; }
    public void SetDisplay(DrmDmaBufDisplayManager? d){ lock(_lock) _disp=d; }
    private bool Validate(int idx, IntPtr mapped, int fd){ if(idx<0||idx>=_out.Count) return false; if(fd<0||mapped==IntPtr.Zero) return false; return true; }
    public bool ProcessDecoded(int idx, ulong used, uint format=0, int planeCount=1, uint[]? pitches=null, uint[]? offsets=null){ if(!_w.HasValue()||!_h.HasValue()) return false; if(idx<0||idx>=_out.Count) return false; var info=_out[idx]; if(!Validate(idx,info.Mapped,info.Fd)) return false; _decoded++; if(!_zeroCopyInit[idx]){ _setup?.Invoke(idx); _zeroCopyInit[idx]=true; }
        var disp=_disp; if(disp!=null){ if(format==0) format=V4L2Const.V4L2_PIX_FMT_YUV420; var fi=new DrmDmaBufDisplayManager.FrameInfo{ Data=info.Mapped,DmaFd=info.Fd,Width=_w,Height=_h,Format=format,DrmFormat=format,Size=used,IsDmaBuf=true,PlaneCount=planeCount,Pitches=pitches,Offsets=offsets}; disp.DisplayFrame(fi);} return true; }
    public int DecodedCount=>_decoded;
}

file static class FrameProcessorExt{ public static bool HasValue(this uint v)=> v!=0; }
