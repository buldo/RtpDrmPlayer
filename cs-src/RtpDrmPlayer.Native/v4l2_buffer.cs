using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)] public struct v4l2_buffer { public uint index; public uint type; public uint bytesused; public uint flags; public uint field; public ulong timestamp_sec; public ulong timestamp_usec; public uint timecode_type; public uint sequence; public uint memory; public IntPtr m_planes; public uint length; [MarshalAs(UnmanagedType.ByValArray, SizeConst = 2)] public uint[] reserved2; }