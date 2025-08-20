using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)] public struct v4l2_capability { [MarshalAs(UnmanagedType.ByValArray, SizeConst = 16)] public byte[] driver; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public byte[] card; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 32)] public byte[] bus_info; public uint version; public uint capabilities; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 4)] public uint[] reserved; }