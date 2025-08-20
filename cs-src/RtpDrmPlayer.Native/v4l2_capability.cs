using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct v4l2_capability
{
    public fixed byte driver[16];
    public fixed byte card[32];
    public fixed byte bus_info[32];
    public uint version;
    public uint capabilities;
    public fixed uint reserved[4];
}