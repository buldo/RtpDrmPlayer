using System;
using RtpDrmPlayer.Native;

namespace RtpDrmPlayer.Core;

public class FrameProcessor
{
    private readonly DrmDmaBufDisplayManager? _disp; private readonly DmaBuffersManager _out; private readonly Func<int,bool>? _setup; private readonly uint _w; private readonly uint _h; private int _decoded;
    public FrameProcessor(DrmDmaBufDisplayManager? d,DmaBuffersManager o,uint w,uint h,Func<int,bool>? setup){ _disp=d; _out=o; _w=w; _h=h; _setup=setup; }
    public bool ProcessDecoded(int idx, ulong used){ if(idx<0||idx>=_out.Count) return false; var info=_out[idx]; if(info.Fd<0||info.Mapped==IntPtr.Zero) return false; _decoded++; _setup?.Invoke(idx); if(_disp!=null){ var fi=new DrmDmaBufDisplayManager.FrameInfo{ Data=info.Mapped,DmaFd=info.Fd,Width=_w,Height=_h,Format=V4L2Const.V4L2_PIX_FMT_YUV420,Size=used,IsDmaBuf=true}; _disp.DisplayFrame(fi);} return true; }
    public int DecodedCount=>_decoded;
}
