using System;
using System.Collections.Generic;
using System.IO;
using System.Runtime.InteropServices;
using RtpDrmPlayer.Native;

namespace RtpDrmPlayer.Core;

public class DrmDmaBufDisplayManager : IDisposable
{
    public class FrameInfo {
        public IntPtr Data; public int DmaFd; public uint Width; public uint Height;
        public uint Format; public ulong Size; public bool IsDmaBuf;
        public uint DrmFormat; // fourcc для DRM (может отличаться, но обычно совпадает)
        public int PlaneCount; public uint[]? Pitches; public uint[]? Offsets; public uint[]? Sizes;
    }

    private int _drmFd = -1;
    private readonly Dictionary<int,uint> _dmaToGem = new();
    private readonly Dictionary<uint,uint> _gemToFb = new(); // GEM handle -> FB id
    private bool _initialized;
    private bool _modesetDone;
    private uint _crtcId; private uint _connectorId; private uint _currentFb; private DrmConst.drm_mode_modeinfo _mode;

    private bool Sync(int fd, bool start)
    {
        if(fd<0) return false; var sync = new dma_buf_sync{ flags = (start? DmaBufConst.DMA_BUF_SYNC_START : DmaBufConst.DMA_BUF_SYNC_END) | DmaBufConst.DMA_BUF_SYNC_RW};
        unsafe { return LibC.ioctl(fd, DmaBufConst.DMA_BUF_IOCTL_SYNC, (IntPtr)(&sync))==0; }
    }

    public bool Initialize(uint w,uint h)
    {
        if (_initialized) return true;
        // Перебираем /dev/dri/cardN
        for(int card=0; card<10; card++)
        {
            string path=$"/dev/dri/card{card}";
            if(!File.Exists(path)) continue;
            int fd=LibC.open(path, LibC.O_RDWR | LibC.O_CLOEXEC, 0);
            if(fd<0) continue;
            _drmFd=fd; Console.WriteLine($"[DRM] opened {path} fd={fd}");
            if(PerformModesetDiscovery(w,h)) break; // success or at least resource read
        }
        if(_drmFd<0) Console.WriteLine("[DRM] no DRM devices opened, running in stub mode");
        _initialized=true; return true;
    }

    private unsafe bool PerformModesetDiscovery(uint reqW,uint reqH)
    {
        if(_drmFd<0) return false;
        try
        {
            var res=new DrmConst.drm_mode_card_res();
            if(LibC.ioctl(_drmFd,DrmConst.DRM_IOCTL_MODE_GETRESOURCES,(IntPtr)(&res))!=0){ Console.WriteLine("[DRM] GETRESOURCES fail"); return false; }
            if(res.count_connectors==0||res.count_crtcs==0){ Console.WriteLine("[DRM] no connectors/crtcs on device"); return false; }
            // allocate id arrays
            int connCount=(int)res.count_connectors; int crtcCount=(int)res.count_crtcs; int encCount=(int)res.count_encoders;
            uint[] connIds=new uint[connCount]; uint[] crtcIds=new uint[crtcCount]; uint[] encIds = encCount>0? new uint[encCount] : Array.Empty<uint>();
            fixed(uint* pConn=connIds) fixed(uint* pCrtc=crtcIds) fixed(uint* pEnc=encIds){
                res.connector_id_ptr=(ulong)(IntPtr)pConn; res.crtc_id_ptr=(ulong)(IntPtr)pCrtc; if(encCount>0) res.encoder_id_ptr=(ulong)(IntPtr)pEnc;
                if(LibC.ioctl(_drmFd,DrmConst.DRM_IOCTL_MODE_GETRESOURCES,(IntPtr)(&res))!=0){ Console.WriteLine("[DRM] GETRESOURCES second fail"); return false; }
            }
            for(int ci=0; ci<connCount; ci++)
            {
                var gc=new DrmConst.drm_mode_get_connector{ connector_id=connIds[ci] };
                if(LibC.ioctl(_drmFd,DrmConst.DRM_IOCTL_MODE_GETCONNECTOR,(IntPtr)(&gc))!=0) continue;
                if(gc.count_modes==0) continue; // skip disconnected
                // allocate arrays for modes & encoders
                var modes=new DrmConst.drm_mode_modeinfo[gc.count_modes];
                uint[] cEncIds = gc.count_encoders>0? new uint[gc.count_encoders] : Array.Empty<uint>();
                fixed(DrmConst.drm_mode_modeinfo* pModes=modes) fixed(uint* pCEnc=cEncIds){
                    gc.modes_ptr=(ulong)(IntPtr)pModes; if(cEncIds.Length>0) gc.encoders_ptr=(ulong)(IntPtr)pCEnc;
                    if(LibC.ioctl(_drmFd,DrmConst.DRM_IOCTL_MODE_GETCONNECTOR,(IntPtr)(&gc))!=0) continue;
                }
                // choose mode: exact size else first
                var chosen=modes[0];
                for(int m=0;m<modes.Length;m++){ if(modes[m].hdisplay==reqW && modes[m].vdisplay==reqH){ chosen=modes[m]; break; } }
                // find encoder & crtc bit that matches
                uint selectedCrtc=0; uint selectedEnc=0;
                foreach(var encId in cEncIds){ var ge=new DrmConst.drm_mode_get_encoder{ encoder_id=encId }; if(LibC.ioctl(_drmFd,DrmConst.DRM_IOCTL_MODE_GETENCODER,(IntPtr)(&ge))!=0) continue; if(ge.possible_crtcs==0) continue; // bitmask referencing index in res.crtcs
                    for(int bi=0; bi<crtcIds.Length; bi++){ if(((ge.possible_crtcs>>bi)&1u)!=0){ selectedCrtc=crtcIds[bi]; selectedEnc=encId; break; } }
                    if(selectedCrtc!=0) break; }
                if(selectedCrtc==0){ // fallback: first global crtc
                    if(crtcIds.Length>0) selectedCrtc=crtcIds[0];
                }
                if(selectedCrtc==0) continue;
                _connectorId=connIds[ci]; _crtcId=selectedCrtc; _mode=chosen; _modesetDone=true;
                Console.WriteLine($"[DRM] Selected card fd={_drmFd} conn={_connectorId} enc={selectedEnc} crtc={_crtcId} mode={_mode.hdisplay}x{_mode.vdisplay}");
                return true;
            }
        }
        catch(Exception ex){ Console.WriteLine($"[DRM] discovery exception {ex.Message}"); }
        return false;
    }

    public unsafe bool DisplayFrame(FrameInfo f)
    {
        if (!_initialized) return false;
        if (_drmFd < 0 || !f.IsDmaBuf)
        {
            Console.WriteLine($"[DRM STUB] display fd={f.DmaFd} size={f.Size}");
            return true;
        }
    if (!_dmaToGem.TryGetValue(f.DmaFd, out var gem))
        {
            if (!ImportDmaBuf(f.DmaFd, out gem))
            {
                Console.WriteLine($"[DRM] import failed fd={f.DmaFd}");
                return false;
            }
        }
    // sync start/end around hypothetical GPU use
        Sync(f.DmaFd,true);
    if(f.PlaneCount>0 && f.Pitches!=null && f.Offsets!=null){
            string planes=""; for(int i=0;i<f.PlaneCount;i++){ var pitch=f.Pitches.Length>i?f.Pitches[i]:0; var off=f.Offsets.Length>i?f.Offsets[i]:0; planes += $" [p{i} off={off} pitch={pitch}]"; }
            Console.WriteLine($"[DRM] would display GEM={gem} fd={f.DmaFd} {f.Width}x{f.Height} fmt=0x{f.Format:x} drm=0x{f.DrmFormat:x} planes={f.PlaneCount}{planes}");
        } else {
            Console.WriteLine($"[DRM] would display GEM={gem} fd={f.DmaFd} {f.Width}x{f.Height} fmt=0x{f.Format:x}");
        }
        if(_modesetDone)
        {
            // Убедимся что есть FB для текущего GEM
            if(!_gemToFb.TryGetValue(gem, out var fbId))
            {
                var fb = new DrmConst.drm_mode_fb_cmd2{ fb_id=0, width=f.Width, height=f.Height, pixel_format=f.DrmFormat!=0?f.DrmFormat:f.Format, flags=0, handles=new uint[4], pitches=new uint[4], offsets=new uint[4], modifier=new ulong[4] };
                fb.handles[0]=gem; fb.pitches[0]= f.Pitches!=null && f.Pitches.Length>0? f.Pitches[0]: f.Width; fb.offsets[0]=0;
                if(f.PlaneCount>1){ fb.handles[1]=gem; fb.pitches[1]= f.Pitches![1]; fb.offsets[1]= f.Offsets![1]; }
                if(LibC.ioctl(_drmFd,DrmConst.DRM_IOCTL_MODE_ADDFB2,(IntPtr)(&fb))!=0){ Console.WriteLine($"[DRM] AddFB2 failed errno={Marshal.GetLastPInvokeError()}"); }
                else { fbId=fb.fb_id; _gemToFb[gem]=fbId; Console.WriteLine($"[DRM] AddFB2 fb={fbId} for GEM={gem}"); }
            }
            if(fbId!=0)
            {
                if(_currentFb==0)
                {
                    // Первый кадр – делаем SETCRTC
                    var crtc=new DrmConst.drm_mode_crtc{ crtc_id=_crtcId, fb_id=fbId, x=0,y=0, mode_valid=1, mode=_mode};
                    if(LibC.ioctl(_drmFd,DrmConst.DRM_IOCTL_MODE_SETCRTC,(IntPtr)(&crtc))!=0){ Console.WriteLine($"[DRM] SETCRTC failed errno={Marshal.GetLastPInvokeError()}"); }
                    else { _currentFb=fbId; Console.WriteLine("[DRM] SETCRTC ok"); }
                }
                else if(fbId!=_currentFb)
                {
                    var flip=new DrmConst.drm_mode_crtc_page_flip{ crtc_id=_crtcId, fb_id=fbId, flags=0, reserved=0, user_data=0};
                    if(LibC.ioctl(_drmFd,DrmConst.DRM_IOCTL_MODE_PAGE_FLIP,(IntPtr)(&flip))==0) _currentFb=fbId;
                }
            }
        }
        Sync(f.DmaFd,false);
        return true;
    }

    public void OnResolutionChange(uint w,uint h)
    {
        if(_drmFd<0) return; Console.WriteLine($"[DRM] resolution change -> reset display {w}x{h}");
        // Удаляем все FB
        foreach(var fb in _gemToFb.Values){ uint id=fb; unsafe { LibC.ioctl(_drmFd,DrmConst.DRM_IOCTL_MODE_RMFB,(IntPtr)(&id)); } }
    _gemToFb.Clear(); _currentFb=0; _modesetDone=false; PerformModesetDiscovery(w,h); // повторим выбор ресурсов
    }

    public bool SetupZeroCopyBuffer(int fd,uint w,uint h)
    {
        if (_drmFd < 0)
        {
            Console.WriteLine($"[DRM STUB] zero-copy fd={fd} {w}x{h}");
            return true;
        }
        if (_dmaToGem.ContainsKey(fd)) return true;
        if (!ImportDmaBuf(fd, out var gem)) return false;
        Console.WriteLine($"[DRM] zero-copy imported fd={fd} -> GEM={gem}");
        return true;
    }

    public string GetDisplayInfo() => _drmFd < 0 ? "DRM STUB" : $"DRM fd={_drmFd} imported={_dmaToGem.Count}";

    private bool ImportDmaBuf(int dmaFd, out uint gemHandle)
    {
        gemHandle = 0;
        if (_drmFd < 0) return false;
        var prime = new DrmConst.drm_prime_handle { fd = dmaFd, flags = 0, handle = 0 };
        int ret;
        unsafe
        {
            ret = LibC.ioctl(_drmFd, DrmConst.DRM_IOCTL_PRIME_FD_TO_HANDLE, new IntPtr(&prime));
        }
        if (ret != 0)
        {
            Console.WriteLine($"[DRM] PRIME_FD_TO_HANDLE errno={Marshal.GetLastPInvokeError()}");
            return false;
        }
        gemHandle = prime.handle;
        _dmaToGem[dmaFd] = gemHandle;
        return true;
    }

    private void CloseGem(uint handle)
    {
        if (_drmFd < 0) return;
        var close = new DrmConst.drm_gem_close { handle = handle };
        unsafe
        {
            LibC.ioctl(_drmFd, DrmConst.DRM_IOCTL_GEM_CLOSE, new IntPtr(&close));
        }
    }

    public void Dispose()
    {
    if(_currentFb!=0){ uint id=_currentFb; unsafe{ LibC.ioctl(_drmFd,DrmConst.DRM_IOCTL_MODE_RMFB,(IntPtr)(&id)); } _currentFb=0; }
    if(_gemToFb.Count>0){ foreach(var kv in _gemToFb){ uint id=kv.Value; unsafe{ LibC.ioctl(_drmFd,DrmConst.DRM_IOCTL_MODE_RMFB,(IntPtr)(&id)); } } _gemToFb.Clear(); }
        foreach (var kv in _dmaToGem)
        {
            CloseGem(kv.Value);
        }
        _dmaToGem.Clear();
        if (_drmFd >= 0)
        {
            LibC.close(_drmFd);
            _drmFd = -1;
        }
    }
}
