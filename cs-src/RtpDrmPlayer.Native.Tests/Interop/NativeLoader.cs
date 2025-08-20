using System;
using System.Diagnostics;
using System.IO;
using System.Reflection;
using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native.Tests.Interop;

internal static class NativeLoader
{
    private const string LibName = "libtestconsts.so";

    [StructLayout(LayoutKind.Sequential)]
    public struct ExportedConsts
    {
        public uint V4L2_CAP_VIDEO_M2M_MPLANE_;
        public uint V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE_;
        public uint V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE_;
        public uint V4L2_MEMORY_DMABUF_;
        public uint V4L2_EVENT_EOS_;
        public uint V4L2_EVENT_SOURCE_CHANGE_;
        public uint V4L2_EVENT_FRAME_SYNC_;
        public uint V4L2_EVENT_SRC_CH_RESOLUTION_;
        public uint V4L2_CID_MIN_BUFFERS_FOR_CAPTURE_;
        public uint V4L2_BUF_FLAG_ERROR_;
        public uint V4L2_BUF_FLAG_LAST_;
        public ulong VIDIOC_QUERYCAP_;
        public ulong VIDIOC_G_FMT_;
        public ulong VIDIOC_S_FMT_;
        public ulong VIDIOC_REQBUFS_;
        public ulong VIDIOC_QBUF_;
        public ulong VIDIOC_DQBUF_;
        public ulong VIDIOC_STREAMON_;
        public ulong VIDIOC_STREAMOFF_;
        public ulong VIDIOC_S_CTRL_;
        public ulong VIDIOC_SUBSCRIBE_EVENT_;
        public ulong VIDIOC_DQEVENT_;
        public ulong DMA_BUF_IOCTL_SYNC_;
    public ulong DRM_IOCTL_PRIME_FD_TO_HANDLE_;
    public ulong DRM_IOCTL_GEM_CLOSE_;
    public ulong DRM_IOCTL_MODE_GETRESOURCES_;
    public ulong DRM_IOCTL_MODE_GETCRTC_;
    public ulong DRM_IOCTL_MODE_SETCRTC_;
    public ulong DRM_IOCTL_MODE_GETENCODER_;
    public ulong DRM_IOCTL_MODE_GETCONNECTOR_;
    public ulong DRM_IOCTL_MODE_ADDFB2_;
    public ulong DRM_IOCTL_MODE_RMFB_;
    public ulong DRM_IOCTL_MODE_PAGE_FLIP_;
    public uint V4L2_PIX_FMT_H264_;
    public uint V4L2_PIX_FMT_YUV420_;
    public uint V4L2_PIX_FMT_NV12_;
    public uint DMA_BUF_SYNC_START_;
    public uint DMA_BUF_SYNC_END_;
    public uint DMA_BUF_SYNC_RW_;
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
    public uint SIZE_v4l2_buffer;
    public uint SIZE_v4l2_plane;
    public uint SIZE_v4l2_format;
    public uint SIZE_v4l2_plane_pix_format;
    public uint SIZE_v4l2_event_subscription;
    public uint SIZE_v4l2_event;
    public uint SIZE_v4l2_requestbuffers;
    public uint SIZE_v4l2_capability;
    public uint SIZE_v4l2_control;
    }

    private static readonly Lazy<ExportedConsts> _instance = new(Load);

    public static ExportedConsts Instance => _instance.Value;

    private static ExportedConsts Load()
    {
        BuildNativeLibrary();
        return get_exported_consts();
    }

    internal static void BuildNativeLibrary()
    {
        var assemblyLocation = Assembly.GetExecutingAssembly().Location;
        var assemblyDirectory = Path.GetDirectoryName(assemblyLocation);

        // Find the source constants.c file relative to the assembly
        var sourceFile = Path.GetFullPath(Path.Combine(assemblyDirectory, "..", "..", "..", "native", "constants.c"));
        if (!File.Exists(sourceFile))
        {
            throw new FileNotFoundException($"Original native source file not found: {sourceFile}");
        }

        // Create a 'native' directory in the output folder and copy the source file there
        var nativeDir = Path.Combine(assemblyDirectory, "native");
        Directory.CreateDirectory(nativeDir);
        var sourcePathInOutput = Path.Combine(nativeDir, "constants.c");
        File.Copy(sourceFile, sourcePathInOutput, true);

        var outputPath = Path.Combine(assemblyDirectory, "libtestconsts.so");

        var process = new Process
        {
            StartInfo = new ProcessStartInfo
            {
                FileName = "/usr/bin/gcc",
                Arguments = $"-shared -fPIC -o {outputPath} {sourcePathInOutput}",
                RedirectStandardOutput = true,
                RedirectStandardError = true,
                UseShellExecute = false,
                CreateNoWindow = true,
            }
        };

        process.Start();
        process.WaitForExit();

        if (process.ExitCode != 0)
        {
            var error = process.StandardError.ReadToEnd();
            throw new Exception($"Failed to build native library: {error}");
        }
    }

    [DllImport("libtestconsts.so", EntryPoint = "get_exported_consts")]
    private static extern ExportedConsts get_exported_consts();
}
