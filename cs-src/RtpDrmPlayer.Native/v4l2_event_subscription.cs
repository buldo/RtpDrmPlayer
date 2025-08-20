using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct v4l2_event_subscription
{
    public uint type;
    public uint id;
    public uint flags;
    public fixed uint reserved[5];
}