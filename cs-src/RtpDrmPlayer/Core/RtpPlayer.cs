using System;
using System.Collections.Concurrent;
using System.Threading;
using System.Threading.Tasks;

namespace RtpDrmPlayer.Core;

public class RtpPlayer : IDisposable
{
    private readonly DecoderConfig _cfg; private V4L2Decoder? _dec; private UvgRtpReceiver? _rx; private readonly BlockingCollection<UvgRtpReceiver.H264Frame> _q=new(5); private CancellationTokenSource? _cts; private Task? _loop; private bool _hasSps; private bool _hasPps;
    public RtpPlayer(string device,string ip,ushort port){ _cfg=new DecoderConfig{ DevicePath=device }; }
    public bool Initialize(){ _dec=new V4L2Decoder(_cfg); if(!_dec.Initialize()) return false; _rx=new UvgRtpReceiver("0.0.0.0",5600); if(!_rx.Initialize()) return false; _rx.SetFrameCallback(f=>OnFrame(f)); return true; }
    private void OnFrame(UvgRtpReceiver.H264Frame f){
        var nals = H264AnnexBParser.Parse(f.Data);
        foreach(var nal in nals){
            switch(nal.Type){
                case 7: if(!_hasSps){ _hasSps=true; System.Console.WriteLine("SPS detected"); } break;
                case 8: if(!_hasPps){ _hasPps=true; System.Console.WriteLine("PPS detected"); } break;
                case 5: if(!_hasSps||!_hasPps) System.Console.WriteLine("IDR before SPS/PPS"); break;
            }
        }
        if(!(_hasSps && _hasPps)) return;
        if(!_q.TryAdd(f)){ _q.TryTake(out _); _q.TryAdd(f);} }
    public void Start(){ _cts=new CancellationTokenSource(); _dec?.Start(); _loop=Task.Run(()=>Loop(_cts.Token)); _rx?.Start(); }
    private void Loop(CancellationToken ct){ while(!ct.IsCancellationRequested){ if(!_q.TryTake(out var f,100)) continue; _dec?.FeedInputFrame(f.Data); } }
    public void Stop(){ _rx?.Stop(); _cts?.Cancel(); try{ _loop?.Wait(); }catch{} }
    public void Dispose(){ Stop(); _dec?.Dispose(); _rx?.Dispose(); }
}
