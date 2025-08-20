using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Explicit)]
public unsafe struct v4l2_format
{
    [FieldOffset(0)]
    public uint type;

    [FieldOffset(8)]
    public v4l2_pix_mp fmt;
    
    [FieldOffset(8)]
    private fixed byte raw_data[200];
}