using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)]
public struct v4l2_pix_mp
{
	public uint width;
	public uint height;
	public uint pixelformat;
	public uint field;
	public uint colorspace;
	[MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)] public v4l2_plane_pix_format[] plane_fmt;
	public byte num_planes;
	public byte flags;
	public byte ycbcr_enc;
	public byte quantization;
	public byte xfer_func;
	[MarshalAs(UnmanagedType.ByValArray, SizeConst = 7)] public byte[] reserved;
}