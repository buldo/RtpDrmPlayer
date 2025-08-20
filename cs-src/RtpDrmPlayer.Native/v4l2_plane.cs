using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

// Упрощённое отображение v4l2_plane (используем только fd/data_offset). Размер и порядок должны соответствовать ядру для m.fd пути.
// struct v4l2_plane { __u32 bytesused; __u32 length; union { __u32 mem_offset; unsigned long userptr; __s32 fd; } m; __u32 data_offset; __u32 reserved[11]; };
[StructLayout(LayoutKind.Sequential)]
public struct v4l2_plane
{
    public uint bytesused; // bytes actually used
    public uint length;    // total length of the plane (size)
    public int m_fd;       // using union field fd
    public uint data_offset; // offset to data (for compressed formats)
    [MarshalAs(UnmanagedType.ByValArray, SizeConst = 11)] public uint[] reserved; // keep size
}
