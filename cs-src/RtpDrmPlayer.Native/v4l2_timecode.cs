using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct v4l2_timecode
{
    public uint type;
    public uint flags;
    public byte frames;
    public byte seconds;
    public byte minutes;
    public byte hours;
    public fixed byte userbits[4];
}
