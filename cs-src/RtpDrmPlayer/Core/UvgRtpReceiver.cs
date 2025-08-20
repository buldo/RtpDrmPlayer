using System;
using System.Threading;
using System.Threading.Tasks;

namespace RtpDrmPlayer.Core;

public class UvgRtpReceiver : IDisposable
{
    public record H264Frame(byte[] Data,uint Timestamp,bool IsKeyframe);
    public delegate void FrameCallback(H264Frame frame);
    private FrameCallback? _cb; private CancellationTokenSource? _cts; private Task? _worker; private uint _ts;
    private readonly string _ip; private readonly ushort _port; private ulong _frames; private ulong _bytes; private ulong _keyframes;
    public UvgRtpReceiver(string ip, ushort port){ _ip=ip; _port=port; }
    public bool Initialize()=>true; // TODO: заменить на реальный uvgRTP binding
    public void SetFrameCallback(FrameCallback cb)=>_cb=cb;
    public bool Start(){ if(_worker!=null) return true; _cts=new CancellationTokenSource(); _worker=Task.Run(()=>Loop(_cts.Token)); return true; }
    private async Task Loop(CancellationToken ct){ var rnd=new Random(); while(!ct.IsCancellationRequested){ bool key= (_ts%30)==0; if(key){ // Сформировать последовательность SPS(7), PPS(8), IDR(5)
                byte[] sps=BuildNal(rnd,7,32); byte[] pps=BuildNal(rnd,8,16); byte[] idr=BuildNal(rnd,5,3000);
                var buf=new byte[sps.Length+pps.Length+idr.Length]; Buffer.BlockCopy(sps,0,buf,0,sps.Length); Buffer.BlockCopy(pps,0,buf,sps.Length,pps.Length); Buffer.BlockCopy(idr,0,buf,sps.Length+pps.Length,idr.Length);
                _frames++; _bytes+=(ulong)buf.Length; _keyframes++; _cb?.Invoke(new H264Frame(buf,_ts++,true));
            } else { byte[] slice=BuildNal(rnd,1,1200); _frames++; _bytes+=(ulong)slice.Length; _cb?.Invoke(new H264Frame(slice,_ts++,false)); }
            await Task.Delay(33,ct); } }

    private static byte[] BuildNal(Random rnd, byte nalType, int payload){ var buf=new byte[payload+5]; buf[0]=0;buf[1]=0;buf[2]=0;buf[3]=1; buf[4]=(byte)(nalType & 0x1F); if(payload>0) rnd.NextBytes(buf.AsSpan(5)); return buf; }
    public (ulong frames,ulong bytes,ulong keyframes) GetStats()=> (_frames,_bytes,_keyframes);
    public void Stop(){ _cts?.Cancel(); try{ _worker?.Wait(200);}catch{} _worker=null; _cts?.Dispose(); }
    public void Dispose()=>Stop();
}
