using System;
using System.IO;
using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native.Tests.Interop;

internal static class NativeLoader
{
    private const string LibName = "libtestconsts.so";

    [StructLayout(LayoutKind.Sequential)]
    internal struct ExportedConsts
    {
        public uint V4L2_CAP_VIDEO_M2M_MPLANE;
        public uint V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        public uint V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        public uint V4L2_MEMORY_DMABUF;
        public uint V4L2_EVENT_EOS;
        public uint V4L2_EVENT_SOURCE_CHANGE;
        public uint V4L2_EVENT_FRAME_SYNC;
        public uint V4L2_EVENT_SRC_CH_RESOLUTION;
        public uint V4L2_CID_MIN_BUFFERS_FOR_CAPTURE;
        public uint V4L2_BUF_FLAG_ERROR;
        public uint V4L2_BUF_FLAG_LAST;
        public ulong VIDIOC_QUERYCAP;
        public ulong VIDIOC_G_FMT;
        public ulong VIDIOC_S_FMT;
        public ulong VIDIOC_REQBUFS;
        public ulong VIDIOC_QBUF;
        public ulong VIDIOC_DQBUF;
        public ulong VIDIOC_STREAMON;
        public ulong VIDIOC_STREAMOFF;
        public ulong VIDIOC_S_CTRL;
        public ulong VIDIOC_SUBSCRIBE_EVENT;
        public ulong VIDIOC_DQEVENT;
        public ulong DMA_BUF_IOCTL_SYNC;
    public ulong DRM_IOCTL_PRIME_FD_TO_HANDLE;
    public ulong DRM_IOCTL_GEM_CLOSE;
    public ulong DRM_IOCTL_MODE_GETRESOURCES;
    public ulong DRM_IOCTL_MODE_GETCRTC;
    public ulong DRM_IOCTL_MODE_SETCRTC;
    public ulong DRM_IOCTL_MODE_GETENCODER;
    public ulong DRM_IOCTL_MODE_GETCONNECTOR;
    public ulong DRM_IOCTL_MODE_ADDFB2;
    public ulong DRM_IOCTL_MODE_RMFB;
    public ulong DRM_IOCTL_MODE_PAGE_FLIP;
    public uint V4L2_PIX_FMT_H264;
    public uint V4L2_PIX_FMT_YUV420;
    public uint V4L2_PIX_FMT_NV12;
    public uint DMA_BUF_SYNC_START;
    public uint DMA_BUF_SYNC_END;
    public uint DMA_BUF_SYNC_RW;
    public int O_RDWR_;
    public int O_NONBLOCK_;
    public int O_CLOEXEC_;
    public int PROT_READ_;
    public int PROT_WRITE_;
    public int MAP_SHARED_;
    public short POLLIN_;
    public short POLLPRI_;
    public short POLLOUT_;
    public short POLLERR_;
    public uint SIZE_v4l2_pix_format_mplane;
    }

    [DllImport(LibName, EntryPoint = "get_exported_consts")]
    private static extern ExportedConsts get_exported_consts();

    internal static ExportedConsts Load() => get_exported_consts();

    internal static string BuildNativeLibrary(string sourceDir)
    {
        var soPath = Path.Combine(sourceDir, LibName);
        if(File.Exists(soPath)) return soPath;
        var cFile = Path.Combine(sourceDir, "constants.c");
        var args = $"-shared -fPIC -O2 -o {LibName} {cFile}";
        var proc = System.Diagnostics.Process.Start(new System.Diagnostics.ProcessStartInfo
        {
            FileName = "gcc",
            Arguments = args,
            WorkingDirectory = sourceDir,
            RedirectStandardError = true,
            RedirectStandardOutput = true,
            UseShellExecute = false
        });
        proc!.WaitForExit();
        if(proc.ExitCode != 0)
        {
            throw new Exception($"gcc failed: {proc.ExitCode}\n{proc.StandardError.ReadToEnd()}");
        }
        return soPath;
    }
}
