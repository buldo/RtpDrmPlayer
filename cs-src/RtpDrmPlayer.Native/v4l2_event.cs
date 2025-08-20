using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)] public struct v4l2_event { public uint type; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)] public uint[] u; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 8)] public uint[] reserved; }