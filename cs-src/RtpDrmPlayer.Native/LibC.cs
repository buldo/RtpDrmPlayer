using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

public static class LibC
{
    public const int O_RDWR = 0x0002;
    public const int O_NONBLOCK = 0x800;
    public const int O_CLOEXEC = 0x80000;
    [DllImport("libc", SetLastError = true, EntryPoint = "open", CharSet = CharSet.Ansi)]
    public static extern int open(string pathname, int flags, int mode = 0);
    [DllImport("libc", SetLastError = true)] public static extern int close(int fd);
    [DllImport("libc", SetLastError = true)] public static extern int ioctl(int fd, ulong request, IntPtr arg);
    [DllImport("libc", SetLastError = true)] public static extern IntPtr mmap(IntPtr addr, UIntPtr length, int prot, int flags, int fd, IntPtr offset);
    [DllImport("libc", SetLastError = true)] public static extern int munmap(IntPtr addr, UIntPtr length);
    [DllImport("libc", SetLastError = true)] public static extern int poll([In, Out] PollFd[] fds, uint nfds, int timeout);
    public const int PROT_READ = 0x1; public const int PROT_WRITE = 0x2; public const int MAP_SHARED = 0x01;
    public const short POLLIN = 0x0001; public const short POLLPRI = 0x0002; public const short POLLOUT = 0x0004; public const short POLLERR = 0x0008;
}