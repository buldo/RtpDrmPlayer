using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Explicit)]
public unsafe struct v4l2_buffer
{
    [FieldOffset(0)]
    public uint index;
    [FieldOffset(4)]
    public uint type;
    [FieldOffset(8)]
    public uint bytesused;
    [FieldOffset(12)]
    public uint flags;
    [FieldOffset(16)]
    public uint field;
    [FieldOffset(24)]
    public long timestamp_sec;
    [FieldOffset(32)]
    public long timestamp_usec;
    [FieldOffset(40)]
    public v4l2_timecode timecode;
    [FieldOffset(56)]
    public uint sequence;
    [FieldOffset(60)]
    public uint memory;
    [FieldOffset(64)]
    public IntPtr m_planes;
    [FieldOffset(72)]
    public uint length;
    [FieldOffset(76)]
    public uint reserved2;
    [FieldOffset(80)]
    public uint reserved;
}