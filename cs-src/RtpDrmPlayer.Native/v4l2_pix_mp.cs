using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)]
public unsafe struct v4l2_pix_mp
{
	public uint width;
	public uint height;
	public uint pixelformat;
	public uint field;
	public uint colorspace;
	public v4l2_plane_pix_format plane_fmt_0;
	public v4l2_plane_pix_format plane_fmt_1;
	public v4l2_plane_pix_format plane_fmt_2;
	public v4l2_plane_pix_format plane_fmt_3;
	public v4l2_plane_pix_format plane_fmt_4;
	public v4l2_plane_pix_format plane_fmt_5;
	public v4l2_plane_pix_format plane_fmt_6;
	public v4l2_plane_pix_format plane_fmt_7;
	public byte num_planes;
	public byte flags;
	public byte ycbcr_enc;
	public byte quantization;
	public byte xfer_func;
	public fixed byte reserved[7];
}