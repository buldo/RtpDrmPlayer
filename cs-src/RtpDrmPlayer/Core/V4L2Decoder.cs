using System;
using System.Threading;
using System.Threading.Tasks;
using RtpDrmPlayer.Native;

namespace RtpDrmPlayer.Core;

public class V4L2Decoder : IDisposable
{
    private readonly DecoderConfig _cfg; private readonly V4L2Device _dev=new(); private readonly DmaBufAllocator _alloc=new(); private readonly DmaBuffersManager _in; private readonly DmaBuffersManager _out; private DrmDmaBufDisplayManager? _disp; private FrameProcessor? _proc; private StreamingManager? _stream; private bool _ready; private readonly Random _rnd=new();
    private readonly object _lock=new();
    private uint _curWidth; private uint _curHeight;
    private CancellationTokenSource? _cts; private Task? _pollTask;
    private volatile bool _needsReset; private volatile bool _seenSourceChange;
    public V4L2Decoder(DecoderConfig cfg){ _cfg=cfg; _in=new DmaBuffersManager(_alloc,cfg.InputBufferCount,V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE); _out=new DmaBuffersManager(_alloc,cfg.OutputBufferCount,V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE); }
    public bool Initialize(){ if(!_dev.Open(_cfg.DevicePath)) return false; if(!_alloc.Initialize()) return false; // capability & DMA-BUF checks
        var cap=new v4l2_capability(); if(_dev.QueryCapability(ref cap)){ if((cap.capabilities & V4L2Const.V4L2_CAP_VIDEO_M2M_MPLANE)==0){ Console.Error.WriteLine("[V4L2] Device lacks M2M_MPLANE capability"); return false; } } else { Console.Error.WriteLine("[V4L2] QUERYCAP failed"); }
        if(!_dev.CheckDmaBufSupport()){ Console.Error.WriteLine("[V4L2] DMA-BUF not supported"); return false; }
        _curWidth=_cfg.Width; _curHeight=_cfg.Height; SetupFormats(_curWidth,_curHeight); // set MIN_BUFFERS_FOR_CAPTURE=1 for low latency
        _dev.SetControl(V4L2Const.V4L2_CID_MIN_BUFFERS_FOR_CAPTURE,1);
        ulong inSz=_cfg.DefaultInputBufferSize; ulong outSz=(ulong)(_curWidth*_curHeight*3/2); if(!_in.Allocate(inSz)||!_out.Allocate(outSz)) return false; var reqOut=new v4l2_requestbuffers{ count=(uint)_cfg.InputBufferCount, memory=V4L2Const.V4L2_MEMORY_DMABUF, type=V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE}; _dev.RequestBuffers(ref reqOut); var reqCap=new v4l2_requestbuffers{ count=(uint)_cfg.OutputBufferCount, memory=V4L2Const.V4L2_MEMORY_DMABUF, type=V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE}; _dev.RequestBuffers(ref reqCap); _stream=new StreamingManager(_dev,_out); _disp=null; _proc=new FrameProcessor(_disp,_out,_curWidth,_curHeight, idx=> _disp?.SetupZeroCopyBuffer(_out[idx].Fd,_curWidth,_curHeight)==true,_out.Count); SubscribeEvents(); _ready=true; _stream.Start(); StartPollLoop(); return true; }

    private unsafe void SetupFormats(uint w, uint h)
    {
        var fmtIn = new v4l2_format
        {
            type = V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
            fmt = new v4l2_pix_mp
            {
                width = w,
                height = h,
                pixelformat = _cfg.InputCodec,
                num_planes = 1
            }
        };
        fmtIn.fmt.plane_fmt_0.sizeimage = (uint)_cfg.DefaultInputBufferSize;
        _dev.SetFormat(ref fmtIn);

        bool nv12 = _cfg.OutputPixelFormat == V4L2Const.V4L2_PIX_FMT_NV12;
        byte outCnt = (byte)(nv12 ? 2 : 1);
        var fmtOut = new v4l2_format
        {
            type = V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
            fmt = new v4l2_pix_mp
            {
                width = w,
                height = h,
                pixelformat = _cfg.OutputPixelFormat,
                num_planes = outCnt
            }
        };

        if (nv12)
        {
            fmtOut.fmt.plane_fmt_0.sizeimage = w * h;
            fmtOut.fmt.plane_fmt_0.bytesperline = (ushort)w;
            fmtOut.fmt.plane_fmt_1.sizeimage = w * h / 2;
            fmtOut.fmt.plane_fmt_1.bytesperline = (ushort)w;
        }
        else
        {
            fmtOut.fmt.plane_fmt_0.sizeimage = w * h * 3 / 2;
            fmtOut.fmt.plane_fmt_0.bytesperline = (ushort)w;
        }
        _dev.SetFormat(ref fmtOut);
    }

    private void ReconfigureCapture(){ lock(_lock){ Console.WriteLine($"[V4L2] Reconfigure capture requested"); _dev.StreamOff(V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE); var freeReq=new v4l2_requestbuffers{ count=0, memory=V4L2Const.V4L2_MEMORY_DMABUF, type=V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE}; _dev.RequestBuffers(ref freeReq); var fmtOut=new v4l2_format{ type=V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE }; _dev.GetFormat(ref fmtOut); _curWidth=fmtOut.fmt.width; _curHeight=fmtOut.fmt.height; Console.WriteLine($"[V4L2] New resolution {_curWidth}x{_curHeight}"); SetupFormats(_curWidth,_curHeight); ulong outSz=(ulong)(_curWidth*_curHeight*3/2); _out.Deallocate(); _out.Allocate(outSz); var reqCap=new v4l2_requestbuffers{ count=(uint)_cfg.OutputBufferCount, memory=V4L2Const.V4L2_MEMORY_DMABUF, type=V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE}; _dev.RequestBuffers(ref reqCap); _dev.StreamOn(V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE); _proc?.UpdateDimensions(_curWidth,_curHeight); _disp?.OnResolutionChange(_curWidth,_curHeight); } }
    private unsafe void SubscribeEvents()
    {
        foreach (var evType in new[] { V4L2Const.V4L2_EVENT_EOS, V4L2Const.V4L2_EVENT_SOURCE_CHANGE, V4L2Const.V4L2_EVENT_FRAME_SYNC })
        {
            var sub = new v4l2_event_subscription { type = evType };
            _dev.SubscribeEvent(ref sub);
        }
    }
    private void StartPollLoop(){ if(_cts!=null) return; _cts=new CancellationTokenSource(); _pollTask=Task.Run(()=>PollLoop(_cts.Token)); }
    private void PollLoop(CancellationToken ct){ if(!_ready) return; while(!ct.IsCancellationRequested){ try{ if(_needsReset){ ResetBuffers(); continue; } if(!_dev.Poll((short)(LibC.POLLIN|LibC.POLLPRI),50)){ TryDequeueOutputNonBlocking(); continue; } if(_dev.HasError){ Thread.Sleep(10); continue; } if(_dev.HasEvent){ var ev=new v4l2_event{ u=new uint[8], reserved=new uint[8] }; if(_dev.DQEvent(ref ev)){ HandleEvent(ev); } } if(_dev.ReadyRead){ if(_dev.DequeueMultiPlane(V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE, out var index, out var flags, out var seq, out var planes)){ if((flags & V4L2Const.V4L2_BUF_FLAG_ERROR)!=0){ Console.Error.WriteLine("[V4L2] Capture buffer error flag set"); } ulong used=0; var pitches=new uint[planes.Length]; var offsets=new uint[planes.Length]; for(int p=0;p<planes.Length;p++){ used+=planes[p].bytesused; pitches[p]=planes[p].bytesused>0? planes[p].bytesused : planes[p].length; offsets[p]=planes[p].data_offset; } _proc?.ProcessDecoded((int)index, used, _cfg.OutputPixelFormat, planes.Length, pitches, offsets); var info=_out[(int)index]; _dev.QueueMultiPlaneDmabuf(index,V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,new[]{info.Fd},new[]{(uint)info.Size},new uint[]{0}); if(_seenSourceChange){ _seenSourceChange=false; _needsReset=true; } } } TryDequeueOutputNonBlocking(); } catch { Thread.Sleep(20);} } }
    private void TryDequeueOutputNonBlocking(){ if(_dev.DequeueMultiPlane(V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, out var idx, out var flags, out var seq, out var planes)){ _in.MarkFree((int)idx); } }
    public bool DecodeFrame(ReadOnlySpan<byte> data){ if(!_ready) return false; if(!_stream!.IsActive) _stream.Start(); int idx=_rnd.Next(0,_out.Count); _proc!.ProcessDecoded(idx,(ulong)data.Length,_cfg.OutputPixelFormat); return true; }
    // Имитация подачи закодированного кадра в OUTPUT очередь (single-plane)
    public bool FeedInputFrame(ReadOnlySpan<byte> encoded)
    {
        if(!_ready) return false;
        // ищем свободный input dma-buf
        int idx=_in.GetFree(); if(idx<0) return false; var info=_in[idx]; if(info.Fd<0) return false;
        // копируем кусок (ограничено размером буфера)
        unsafe
        {
            var copyLen=(int)Math.Min((ulong)encoded.Length, info.Size);
            new Span<byte>((void*)info.Mapped, copyLen).Slice(0,copyLen).Clear(); // очистим
            encoded.Slice(0,copyLen).CopyTo(new Span<byte>((void*)info.Mapped, copyLen));
            _dev.QueueMultiPlaneDmabuf((uint)idx, V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE, new[]{info.Fd}, new[]{(uint)info.Size}, new uint[]{0}, new uint[]{(uint)copyLen});
        }
        _in.MarkInUse(idx);
        return true;
    }
    public int DecodedFrames=>_proc?.DecodedCount??0; public void Dispose(){ _cts?.Cancel(); try{ _pollTask?.Wait(200);} catch{} _disp?.Dispose(); _dev.Dispose(); _alloc.Dispose(); }

    public bool SetDisplay(){ if(_disp!=null) return true; _disp=new DrmDmaBufDisplayManager(); _disp.Initialize(_curWidth,_curHeight); _proc?.SetDisplay(_disp); return true; }
    public bool FlushDecoder(){ if(!_ready) return false; // queue empty buffer with LAST flag
        int idx=_in.GetFree(); if(idx<0) return false; var info=_in[idx]; unsafe{ var planeSize=(uint)info.Size; _dev.QueueMultiPlaneDmabuf((uint)idx,V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,new[]{info.Fd},new[]{planeSize},new uint[]{0},new uint[]{0}, V4L2Const.V4L2_BUF_FLAG_LAST); } _in.MarkInUse(idx); return true; }
    public bool ResetBuffers(){ lock(_lock){ if(!_ready) return false; Console.WriteLine("[V4L2] ResetBuffers start"); _stream?.Stop(); _dev.StreamOff(V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE); _dev.StreamOff(V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
            // free capture buffers
            var freeCap=new v4l2_requestbuffers{ count=0, memory=V4L2Const.V4L2_MEMORY_DMABUF, type=V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE}; _dev.RequestBuffers(ref freeCap);
            // free output buffers
            var freeOut=new v4l2_requestbuffers{ count=0, memory=V4L2Const.V4L2_MEMORY_DMABUF, type=V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE}; _dev.RequestBuffers(ref freeOut);
            _out.Deallocate(); _in.Deallocate();
            // reallocate
            _in.Allocate(_cfg.DefaultInputBufferSize); _out.Allocate((ulong)(_curWidth*_curHeight*3/2)); var reqOut=new v4l2_requestbuffers{ count=(uint)_cfg.InputBufferCount, memory=V4L2Const.V4L2_MEMORY_DMABUF, type=V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE}; _dev.RequestBuffers(ref reqOut); var reqCap=new v4l2_requestbuffers{ count=(uint)_cfg.OutputBufferCount, memory=V4L2Const.V4L2_MEMORY_DMABUF, type=V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE}; _dev.RequestBuffers(ref reqCap);
            // restart streaming for capture only; output starts on first frame
            _dev.StreamOn(V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE); _stream?.Start(); _needsReset=false; Console.WriteLine("[V4L2] ResetBuffers done"); return true; } }
    private void HandleEvent(v4l2_event ev){ switch(ev.type){ case V4L2Const.V4L2_EVENT_EOS: Console.WriteLine("[V4L2] EOS"); break; case V4L2Const.V4L2_EVENT_SOURCE_CHANGE: Console.WriteLine("[V4L2] Source change flagged"); _seenSourceChange=true; _needsReset=true; break; case V4L2Const.V4L2_EVENT_FRAME_SYNC: break; default: break; } }
}
