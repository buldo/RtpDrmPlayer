using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)] public struct v4l2_format { public uint type; public v4l2_pix_mp fmt; }