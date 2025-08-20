using System;

using RtpDrmPlayer.Native;

namespace RtpDrmPlayer.Core;

public class V4L2Decoder : IDisposable
{
    private readonly DecoderConfig _cfg; private readonly V4L2Device _dev=new(); private readonly DmaBufAllocator _alloc=new(); private readonly DmaBuffersManager _in; private readonly DmaBuffersManager _out; private DrmDmaBufDisplayManager? _disp; private FrameProcessor? _proc; private StreamingManager? _stream; private bool _ready; private readonly Random _rnd=new();
    public V4L2Decoder(DecoderConfig cfg){ _cfg=cfg; _in=new DmaBuffersManager(_alloc,cfg.InputBufferCount,V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE); _out=new DmaBuffersManager(_alloc,cfg.OutputBufferCount,V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE); }
    public bool Initialize(){ if(!_dev.Open(_cfg.DevicePath)) return false; if(!_alloc.Initialize()) return false; var fmtIn=new v4l2_format{ type=V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, fmt=new v4l2_pix_mp{ width=_cfg.Width,height=_cfg.Height,pixelformat=_cfg.InputCodec,plane_sizes=new uint[4] } }; _dev.SetFormat(ref fmtIn); var fmtOut=new v4l2_format{ type=V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, fmt=new v4l2_pix_mp{ width=_cfg.Width,height=_cfg.Height,pixelformat=_cfg.OutputPixelFormat,plane_sizes=new uint[4] } }; _dev.SetFormat(ref fmtOut); ulong inSz=_cfg.DefaultInputBufferSize; ulong outSz=(ulong)(_cfg.Width*_cfg.Height*3/2); if(!_in.Allocate(inSz)||!_out.Allocate(outSz)) return false; _stream=new StreamingManager(_dev); _disp=new DrmDmaBufDisplayManager(); _disp.Initialize(_cfg.Width,_cfg.Height); _proc=new FrameProcessor(_disp,_out,_cfg.Width,_cfg.Height, idx=> _disp.SetupZeroCopyBuffer(_out[idx].Fd,_cfg.Width,_cfg.Height)); _ready=true; return true; }
    public bool DecodeFrame(ReadOnlySpan<byte> data){ if(!_ready) return false; if(!_stream!.IsActive) _stream.Start(); int idx=_rnd.Next(0,_out.Count); _proc!.ProcessDecoded(idx,(ulong)data.Length); return true; }
    public int DecodedFrames=>_proc?.DecodedCount??0; public void Dispose(){ _disp?.Dispose(); _dev.Dispose(); _alloc.Dispose(); }
}
