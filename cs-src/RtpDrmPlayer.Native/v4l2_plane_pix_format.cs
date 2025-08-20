using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct v4l2_plane_pix_format
{
    public uint sizeimage;
    public ushort bytesperline;
    public fixed ushort reserved[7];
}
