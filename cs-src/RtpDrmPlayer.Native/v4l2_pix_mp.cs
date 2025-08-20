using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)] public struct v4l2_pix_mp { public uint width; public uint height; public uint pixelformat; public uint field; public uint colorspace; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public uint[] plane_sizes; }