using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)] public struct v4l2_requestbuffers { public uint count; public uint type; public uint memory; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)] public uint[] reserved; }