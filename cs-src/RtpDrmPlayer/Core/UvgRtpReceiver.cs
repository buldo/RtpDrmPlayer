using System;
using System.Threading;
using System.Threading.Tasks;

namespace RtpDrmPlayer.Core;

public class UvgRtpReceiver : IDisposable
{
    public record H264Frame(byte[] Data,uint Timestamp);
    public delegate void FrameCallback(H264Frame frame);
    private FrameCallback? _cb; private CancellationTokenSource? _cts; private Task? _worker; private uint _ts;
    public UvgRtpReceiver(string ip, ushort port){}
    public bool Initialize()=>true;
    public void SetFrameCallback(FrameCallback cb)=>_cb=cb;
    public bool Start(){ if(_worker!=null) return true; _cts=new CancellationTokenSource(); _worker=Task.Run(()=>Loop(_cts.Token)); return true; }
    private async Task Loop(CancellationToken ct){ var rnd=new Random(); while(!ct.IsCancellationRequested){ var buf=new byte[1024]; buf[0]=0;buf[1]=0;buf[2]=0;buf[3]=1; buf[4]=0x67; rnd.NextBytes(buf.AsSpan(5)); _cb?.Invoke(new H264Frame(buf,_ts++)); await Task.Delay(40,ct); } }
    public void Stop(){ _cts?.Cancel(); try{ _worker?.Wait(); }catch{} _worker=null; _cts?.Dispose(); }
    public void Dispose()=>Stop();
}
