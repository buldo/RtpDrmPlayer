using System;
using System.Runtime.InteropServices;

namespace RtpDrmPlayer.Native;

// Минимальный набор DRM ioctls/структур для PRIME импортов
public static class DrmConst
{
    // _IOC битовые поля (совпадают с linux/uapi)
    private const int IOC_NRBITS = 8;
    private const int IOC_TYPEBITS = 8;
    private const int IOC_SIZEBITS = 14;
    private const int IOC_DIRBITS = 2;

    private const int IOC_NRSHIFT = 0;
    private const int IOC_TYPESHIFT = IOC_NRSHIFT + IOC_NRBITS;
    private const int IOC_SIZESHIFT = IOC_TYPESHIFT + IOC_TYPEBITS;
    private const int IOC_DIRSHIFT = IOC_SIZESHIFT + IOC_SIZEBITS;

    private const int IOC_NONE = 0;
    private const int IOC_WRITE = 1;
    private const int IOC_READ = 2;

    private static uint _IOC(int dir, int type, int nr, int size) => (uint)((dir << IOC_DIRSHIFT) | (type << IOC_TYPESHIFT) | (nr << IOC_NRSHIFT) | (size << IOC_SIZESHIFT));
    private static uint _IOWR(int type, int nr, int size) => _IOC(IOC_READ | IOC_WRITE, type, nr, size);
    private static uint _IOW(int type, int nr, int size) => _IOC(IOC_WRITE, type, nr, size);

    public const int DRM_IOCTL_BASE = (int)'d';
    public const int DRM_COMMAND_BASE = 0x40; // offset для vendor команд

    // Команды (индексы) из drm.h
    public const int DRM_PRIME_FD_TO_HANDLE = 0x2d; // + DRM_COMMAND_BASE
    public const int DRM_IOCTL_GEM_CLOSE_IDX = 0x09; // без DRM_COMMAND_BASE

    // Структуры
    [StructLayout(LayoutKind.Sequential)]
    public struct drm_prime_handle
    {
        public uint handle; // (in/out) GEM handle
        public uint flags;  // currently unused for FD_TO_HANDLE (set 0)
        public int fd;      // (in) dma-buf fd
    }

    [StructLayout(LayoutKind.Sequential)]
    public struct drm_gem_close
    {
        public uint handle; // handle to close
        public uint pad;    // reserved
    }

    public static readonly uint DRM_IOCTL_PRIME_FD_TO_HANDLE = _IOWR(DRM_IOCTL_BASE, DRM_COMMAND_BASE + DRM_PRIME_FD_TO_HANDLE, Marshal.SizeOf<drm_prime_handle>());
    public static readonly uint DRM_IOCTL_GEM_CLOSE = _IOW(DRM_IOCTL_BASE, DRM_IOCTL_GEM_CLOSE_IDX, Marshal.SizeOf<drm_gem_close>());

    // Mode setting (legacy) ioctl indices (from drm_mode.h)
    private const int DRM_IOCTL_MODE_GETRESOURCES_IDX = 0xA0;
    private const int DRM_IOCTL_MODE_GETCRTC_IDX = 0xA1;
    private const int DRM_IOCTL_MODE_SETCRTC_IDX = 0xA2;
    private const int DRM_IOCTL_MODE_GETENCODER_IDX = 0xA6;
    private const int DRM_IOCTL_MODE_GETCONNECTOR_IDX = 0xA7;
    private const int DRM_IOCTL_MODE_RMFB_IDX = 0xAF;
    private const int DRM_IOCTL_MODE_ADDFB2_IDX = 0xB8;
    private const int DRM_IOCTL_MODE_PAGE_FLIP_IDX = 0xB0;

    // FourCC helpers for DRM formats (subset)
    private static uint FOURCC(char a,char b,char c,char d) => (uint)(byte)a | ((uint)(byte)b<<8) | ((uint)(byte)c<<16) | ((uint)(byte)d<<24);
    public static readonly uint DRM_FORMAT_NV12 = FOURCC('N','V','1','2');
    public static readonly uint DRM_FORMAT_YUV420 = FOURCC('Y','U','1','2'); // YU12/I420

    [StructLayout(LayoutKind.Sequential)] public struct drm_mode_card_res
    {
        public ulong fb_id_ptr; public ulong crtc_id_ptr; public ulong connector_id_ptr; public ulong encoder_id_ptr;
        public uint count_fbs; public uint count_crtcs; public uint count_connectors; public uint count_encoders;
        public uint min_width; public uint max_width; public uint min_height; public uint max_height;
    }

    [StructLayout(LayoutKind.Sequential)] public struct drm_mode_modeinfo
    {
        public uint clock; public ushort hdisplay; public ushort hsync_start; public ushort hsync_end; public ushort htotal; public ushort hskew; public ushort vdisplay; public ushort vsync_start; public ushort vsync_end; public ushort vtotal; public ushort vscan; public uint vrefresh; public uint flags; public uint type;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=32)] public byte[] name;
    }

    [StructLayout(LayoutKind.Sequential)] public struct drm_mode_get_connector
    {
        public uint connector_id; public uint encoder_id; public uint connector_type; public uint connector_type_id;
        public uint connection; public uint mm_width; public uint mm_height; public uint subpixel;
        public uint count_modes; public uint count_props; public uint count_encoders;
        public ulong modes_ptr; public ulong props_ptr; public ulong prop_values_ptr; public ulong encoders_ptr;
    }

    [StructLayout(LayoutKind.Sequential)] public struct drm_mode_get_encoder
    {
        public uint encoder_id; public uint encoder_type; public uint crtc_id; public uint possible_crtcs; public uint possible_clones;
    }

    [StructLayout(LayoutKind.Sequential)] public struct drm_mode_crtc
    {
        public uint set_connectors_ptr_low; // we will pack pointer (low 32 bits not used separately)
        public uint count_connectors;
        public uint crtc_id; public uint fb_id; public uint x; public uint y; public uint gamma_size; public uint mode_valid;
        public drm_mode_modeinfo mode;
    }

    [StructLayout(LayoutKind.Sequential)] public struct drm_mode_fb_cmd2
    {
        public uint fb_id; public uint width; public uint height; public uint pixel_format; public uint flags;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=4)] public uint[] handles;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=4)] public uint[] pitches;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=4)] public uint[] offsets;
        [MarshalAs(UnmanagedType.ByValArray, SizeConst=4)] public ulong[] modifier;
    }

    [StructLayout(LayoutKind.Sequential)] public struct drm_mode_crtc_page_flip
    {
        public uint crtc_id; public uint fb_id; public uint flags; public uint reserved; public ulong user_data;
    }

    public static readonly uint DRM_IOCTL_MODE_GETRESOURCES = _IOWR(DRM_IOCTL_BASE, DRM_IOCTL_MODE_GETRESOURCES_IDX, Marshal.SizeOf<drm_mode_card_res>());
    public static readonly uint DRM_IOCTL_MODE_GETCRTC = _IOWR(DRM_IOCTL_BASE, DRM_IOCTL_MODE_GETCRTC_IDX, Marshal.SizeOf<drm_mode_crtc>());
    public static readonly uint DRM_IOCTL_MODE_SETCRTC = _IOWR(DRM_IOCTL_BASE, DRM_IOCTL_MODE_SETCRTC_IDX, Marshal.SizeOf<drm_mode_crtc>());
    public static readonly uint DRM_IOCTL_MODE_GETENCODER = _IOWR(DRM_IOCTL_BASE, DRM_IOCTL_MODE_GETENCODER_IDX, Marshal.SizeOf<drm_mode_get_encoder>());
    public static readonly uint DRM_IOCTL_MODE_GETCONNECTOR = _IOWR(DRM_IOCTL_BASE, DRM_IOCTL_MODE_GETCONNECTOR_IDX, Marshal.SizeOf<drm_mode_get_connector>());
    public static readonly uint DRM_IOCTL_MODE_ADDFB2 = _IOWR(DRM_IOCTL_BASE, DRM_IOCTL_MODE_ADDFB2_IDX, Marshal.SizeOf<drm_mode_fb_cmd2>());
    public static readonly uint DRM_IOCTL_MODE_RMFB = _IOWR(DRM_IOCTL_BASE, DRM_IOCTL_MODE_RMFB_IDX, Marshal.SizeOf<uint>());
    public static readonly uint DRM_IOCTL_MODE_PAGE_FLIP = _IOWR(DRM_IOCTL_BASE, DRM_IOCTL_MODE_PAGE_FLIP_IDX, Marshal.SizeOf<drm_mode_crtc_page_flip>());
}
