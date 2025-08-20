using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

// struct v4l2_plane { __u32 bytesused; __u32 length; union { __u32 mem_offset; unsigned long userptr; __s32 fd; } m; __u32 data_offset; __u32 reserved[11]; };
[StructLayout(LayoutKind.Explicit)]
public unsafe struct v4l2_plane
{
    [FieldOffset(0)]
    public uint bytesused;
    [FieldOffset(4)]
    public uint length;
    
    [FieldOffset(8)]
    public int m_fd;
    [FieldOffset(8)]
    public ulong m_userptr;

    [FieldOffset(16)]
    public uint data_offset;
    
    [FieldOffset(20)]
    public fixed uint reserved[11];
}
