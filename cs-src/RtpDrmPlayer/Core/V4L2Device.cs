using System;
using System.Runtime.InteropServices;
using RtpDrmPlayer.Native;

namespace RtpDrmPlayer.Core;

public sealed class V4L2Device : IDisposable
{
    private int _fd = -1; private pollfd[] _poll = Array.Empty<pollfd>(); private short _revents;
    public bool IsOpen => _fd >= 0;
    public bool Open(string path){ if(IsOpen) return false; _fd=LibC.open(path,LibC.O_RDWR|LibC.O_NONBLOCK,0); if(_fd<0){ Console.Error.WriteLine("open fail"); return false;} _poll=new[]{ new pollfd{ fd=_fd, events=LibC.POLLPRI }}; return true; }
    public void Close(){ if(!IsOpen) return; LibC.close(_fd); _fd=-1; _poll=Array.Empty<pollfd>(); }
    private bool Ioctl(ulong req, IntPtr arg){ if(!IsOpen) return false; if(LibC.ioctl(_fd,req,arg)!=0) return false; return true; }
    public unsafe bool SetFormat(ref v4l2_format f){ fixed(v4l2_format* p=&f) return Ioctl(V4L2Const.VIDIOC_S_FMT,(IntPtr)p);}    
    public unsafe bool GetFormat(ref v4l2_format f){ fixed(v4l2_format* p=&f) return Ioctl(V4L2Const.VIDIOC_G_FMT,(IntPtr)p);}    
    public unsafe bool StreamOn(uint t){ uint x=t; return Ioctl(V4L2Const.VIDIOC_STREAMON,(IntPtr)(&x)); }
    public unsafe bool StreamOff(uint t){ uint x=t; return Ioctl(V4L2Const.VIDIOC_STREAMOFF,(IntPtr)(&x)); }
    public bool Poll(short ev,int to){ if(!IsOpen) return false; _poll[0].events=ev; var r=LibC.poll(_poll,(uint)_poll.Length,to); if(r<0){ _revents=0; return false;} _revents=_poll[0].revents; return true; }
    public bool ReadyRead=> (_revents & LibC.POLLIN)!=0; public bool ReadyWrite=> (_revents & LibC.POLLOUT)!=0; public bool HasEvent=> (_revents & LibC.POLLPRI)!=0; public bool HasError=> (_revents & LibC.POLLERR)!=0;
    public void Dispose()=>Close();
}
