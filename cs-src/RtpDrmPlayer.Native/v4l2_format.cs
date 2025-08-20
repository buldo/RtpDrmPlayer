using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct v4l2_format
{
    public uint type;
    public v4l2_pix_mp fmt;
}