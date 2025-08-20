using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using RtpDrmPlayer.Native;

namespace RtpDrmPlayer.Core;

/// <summary>
/// Аллокатор DMA-буферов для видеодекодера V4L2.
/// Предоставляет возможность использовать DMA-buf для эффективного 
/// обмена буферами между различными устройствами без копирования данных.
/// </summary>
public class DmaBufAllocator : IDisposable
{
    /// <summary>
    /// Информация о выделенном DMA-буфере, соответствующая C++ структуре
    /// </summary>
    public class DmaBufInfo
    {
        public int Fd = -1;                // Файловый дескриптор DMA-buf
        public IntPtr Mapped = IntPtr.Zero; // Адрес отображения в памяти
        public ulong Size = 0;             // Размер буфера
        public uint Handle = 0;            // Дополнительный идентификатор (при необходимости)
    }

    private int _heapFd = -1;
    private bool _supported;
    private readonly List<DmaBufInfo> _allocatedBuffers = new();
    
    // Константа для DMA Heap IOCTL
    private const ulong DMA_HEAP_IOCTL_ALLOC = 0xC0184800;
    
    [StructLayout(LayoutKind.Sequential)] 
    private struct dma_heap_allocation_data 
    { 
        public ulong len;          // Размер запрашиваемого буфера
        public uint fd;            // Возвращаемый fd
        public uint fd_flags;      // Флаги открытия fd
        public ulong heap_flags;   // Дополнительные флаги
    }

    /// <summary>
    /// Инициализирует аллокатор DMA-буферов
    /// </summary>
    /// <returns>true при успешной инициализации</returns>
    public bool initialize()
    {
        // Пробуем открыть один из доступных DMA Heap устройств
        Console.WriteLine("Initializing DmaBufAllocator...");
        
        string[] paths = {
            "/dev/dma_heap/vidbuf_cached",
            "/dev/dma_heap/linux,cma",
            "/dev/dma_heap/system" // Добавляем системный heap на случай, если другие недоступны
        };
        
        foreach (var path in paths)
        {
            Console.WriteLine($"Trying DMA heap: {path}");
            _heapFd = LibC.open(path, LibC.O_RDWR | LibC.O_CLOEXEC, 0);
            
            if (_heapFd >= 0)
            {
                _supported = true;
                Console.WriteLine($"✅ Successfully opened DMA heap: {path}");
                return true;
            }
            
            Console.WriteLine($"Failed to open DMA heap: {path}, errno: {Marshal.GetLastWin32Error()}");
        }
        
        Console.WriteLine("❌ No DMA heap devices available");
        return false;
    }

    /// <summary>
    /// Выделяет DMA-буфер указанного размера
    /// </summary>
    /// <param name="size">Размер буфера в байтах</param>
    /// <returns>Информация о выделенном буфере</returns>
    public DmaBufInfo allocate(ulong size)
    {
        if (!_supported)
        {
            Console.WriteLine("❌ DMA-BUF allocation not supported");
            return new DmaBufInfo();
        }
        
        var allocationData = new dma_heap_allocation_data
        {
            len = size,
            fd_flags = (uint)(LibC.O_RDWR | LibC.O_CLOEXEC),
            heap_flags = 0 // Нет специальных флагов
        };
        
        Console.WriteLine($"Allocating DMA buffer of size {size} bytes");
        
        unsafe
        {
            if (LibC.ioctl(_heapFd, DMA_HEAP_IOCTL_ALLOC, (IntPtr)(&allocationData)) != 0)
            {
                Console.WriteLine($"❌ DMA-BUF allocation failed, errno: {Marshal.GetLastWin32Error()}");
                return new DmaBufInfo();
            }
        }
        
        var bufInfo = new DmaBufInfo
        {
            Fd = (int)allocationData.fd,
            Size = size
        };
        
        _allocatedBuffers.Add(bufInfo);
        Console.WriteLine($"✅ Allocated DMA buffer with fd {bufInfo.Fd}, size {size}");
        
        return bufInfo;
    }

    /// <summary>
    /// Отображает DMA-буфер в адресное пространство процесса
    /// </summary>
    /// <param name="bufInfo">Информация о буфере</param>
    /// <returns>true при успешном отображении</returns>
    public bool map(DmaBufInfo bufInfo)
    {
        if (bufInfo.Fd < 0)
        {
            Console.WriteLine("❌ Cannot map buffer with invalid fd");
            return false;
        }
        
        Console.WriteLine($"Mapping DMA buffer fd {bufInfo.Fd}, size {bufInfo.Size}");
        
        var mappedPtr = LibC.mmap(
            IntPtr.Zero,                          // Адрес не имеет значения (автоматически)
            (UIntPtr)bufInfo.Size,                // Размер отображения
            LibC.PROT_READ | LibC.PROT_WRITE,    // Чтение и запись
            LibC.MAP_SHARED,                     // Совместное использование отображения
            bufInfo.Fd,                          // Файловый дескриптор DMA буфера
            IntPtr.Zero                          // Смещение = 0
        );
        
        if (mappedPtr == (IntPtr)(-1))
        {
            Console.WriteLine($"❌ Failed to map DMA buffer, errno: {Marshal.GetLastWin32Error()}");
            return false;
        }
        
        bufInfo.Mapped = mappedPtr;
        Console.WriteLine($"✅ Mapped DMA buffer to address {mappedPtr}");
        
        return true;
    }

    /// <summary>
    /// Отменяет отображение DMA-буфера
    /// </summary>
    /// <param name="bufInfo">Информация о буфере</param>
    public void unmap(DmaBufInfo bufInfo)
    {
        if (bufInfo.Mapped != IntPtr.Zero)
        {
            Console.WriteLine($"Unmapping DMA buffer at address {bufInfo.Mapped}");
            
            LibC.munmap(bufInfo.Mapped, (UIntPtr)bufInfo.Size);
            bufInfo.Mapped = IntPtr.Zero;
            
            Console.WriteLine("✅ Buffer unmapped");
        }
    }

    /// <summary>
    /// Освобождает DMA-буфер
    /// </summary>
    /// <param name="bufInfo">Информация о буфере</param>
    public void deallocate(DmaBufInfo bufInfo)
    {
        // Сначала отменяем отображение, если оно есть
        unmap(bufInfo);
        
        // Затем закрываем файловый дескриптор
        if (bufInfo.Fd >= 0)
        {
            Console.WriteLine($"Closing DMA buffer fd {bufInfo.Fd}");
            
            LibC.close(bufInfo.Fd);
            bufInfo.Fd = -1;
            
            Console.WriteLine("✅ Buffer deallocated");
        }
    }

    /// <summary>
    /// Проверяет поддержку DMA-буферов
    /// </summary>
    /// <returns>true, если DMA-буферы поддерживаются</returns>
    public bool isSupported() => _supported;

    /// <summary>
    /// Освобождает все ресурсы
    /// </summary>
    public void Dispose()
    {
        Console.WriteLine("Disposing DmaBufAllocator");
        
        // Освобождаем все буферы
        foreach (var buffer in _allocatedBuffers)
        {
            deallocate(buffer);
        }
        
        // Закрываем файловый дескриптор DMA Heap
        if (_heapFd >= 0)
        {
            Console.WriteLine($"Closing DMA heap fd {_heapFd}");
            LibC.close(_heapFd);
            _heapFd = -1;
        }
    }
    
    // Методы совместимости с предыдущей версией C#
    public bool Initialize() => initialize();
    public DmaBufInfo Allocate(ulong size) => allocate(size);
    public bool Map(DmaBufInfo info) => map(info);
    public void Unmap(DmaBufInfo info) => unmap(info);
    public void Deallocate(DmaBufInfo info) => deallocate(info);
}
