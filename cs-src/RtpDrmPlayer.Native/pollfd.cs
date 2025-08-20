using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

[StructLayout(LayoutKind.Sequential)]
public struct PollFd
{
    public int Fd;
    public short Events;
    public short Revents;
}