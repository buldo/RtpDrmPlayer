using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)] public struct v4l2_event_subscription { public uint type; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 5)] public uint[] reserved; }