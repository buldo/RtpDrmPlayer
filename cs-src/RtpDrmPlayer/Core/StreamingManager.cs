using System;
using RtpDrmPlayer.Native;

namespace RtpDrmPlayer.Core;

public class StreamingManager
{
    private readonly V4L2Device _dev; private State _st=State.Stopped; private enum State{Stopped,Starting,Active,Stopping,Error}
    public StreamingManager(V4L2Device d){ _dev=d; }
    public bool Start(){ if(_st==State.Active) return true; _st=State.Starting; if(!_dev.StreamOn(V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)) return false; if(!_dev.StreamOn(V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)) return false; _st=State.Active; return true; }
    public bool Stop(){ if(_st==State.Stopped) return true; _st=State.Stopping; _dev.StreamOff(V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE); _dev.StreamOff(V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE); _st=State.Stopped; return true; }
    public bool IsActive=>_st==State.Active;
}
