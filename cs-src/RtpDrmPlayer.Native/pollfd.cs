using System;
using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)] public struct pollfd { public int fd; public short events; public short revents; }