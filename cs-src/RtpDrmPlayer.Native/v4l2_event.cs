using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Explicit)]
public unsafe struct v4l2_event
{
    [FieldOffset(0)]
    public uint type;

    [FieldOffset(8)]
    public fixed byte u_data[64];

    [FieldOffset(80)]
    public long timestamp_ts_sec;
    [FieldOffset(88)]
    public long timestamp_ts_nsec;

    [FieldOffset(96)]
    public uint id;

    [FieldOffset(104)]
    public fixed uint reserved[8];
}