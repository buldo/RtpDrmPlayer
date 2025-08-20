using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct v4l2_requestbuffers
{
    public uint count;
    public uint type;
    public uint memory;
    public fixed uint reserved[2];
}