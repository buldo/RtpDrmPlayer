using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)]
public struct v4l2_plane_pix_format
{
    public uint sizeimage;
    public ushort bytesperline;
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 7)] public ushort[] reserved;
}
