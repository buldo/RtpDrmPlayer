using System;
using System.Collections.Generic;
using System.Runtime.CompilerServices;
using RtpDrmPlayer.Native;

namespace RtpDrmPlayer.Core;

/// <summary>
/// Управляет буферами DMA для V4L2 устройства.
/// Полностью соответствует реализации DmaBuffersManager из C++ версии.
/// </summary>
public class DmaBuffersManager : IDisposable
{
    private readonly DmaBufAllocator _allocator;
    private readonly List<DmaBufAllocator.DmaBufInfo> _buffers = new();
    private readonly bool[] _inUse;
    private readonly int _count;
    private readonly uint _type;
    private int _currentBuffer;
    private bool _disposed;

    public DmaBuffersManager(DmaBufAllocator allocator, int count, uint type)
    {
        Console.WriteLine($"Creating DmaBuffersManager with {count} buffers of type {type}");
        _allocator = allocator;
        _count = count;
        _type = type;
        _inUse = new bool[count];
        _currentBuffer = 0;
    }

    ~DmaBuffersManager()
    {
        Dispose(false);
    }

    public void Dispose()
    {
        Dispose(true);
        GC.SuppressFinalize(this);
    }

    protected virtual void Dispose(bool disposing)
    {
        if (_disposed)
        {
            return;
        }

        if (disposing)
        {
            // Освобождаем управляемые ресурсы
        }

        // Освобождаем неуправляемые ресурсы
        deallocate();

        _disposed = true;
    }

    /// <summary>
    /// Выделяет buffers_.size() буферов указанного размера
    /// </summary>
    /// <param name="bufferSize">Размер каждого буфера в байтах</param>
    /// <returns>true, если все буферы успешно выделены</returns>
    public bool allocate(ulong bufferSize)
    {
        Console.WriteLine($"Allocating {_count} buffers of size {bufferSize} bytes");
        
        // Освобождаем существующие буферы перед выделением новых
        deallocate();
        
        for (int i = 0; i < _count; i++)
        {
            var bufInfo = _allocator.Allocate(bufferSize);
            if (bufInfo.Fd < 0)
            {
                Console.WriteLine($"Failed to allocate buffer {i}");
                deallocate();
                return false;
            }
            
            if (!_allocator.Map(bufInfo))
            {
                Console.WriteLine($"Failed to map buffer {i}");
                deallocate();
                return false;
            }
            
            _buffers.Add(bufInfo);
            Console.WriteLine($"Allocated buffer {i} with fd {bufInfo.Fd}, size {bufInfo.Size}, addr {bufInfo.Mapped}");
        }
        
        Console.WriteLine($"Successfully allocated {_count} buffers");
        return true;
    }

    /// <summary>
    /// Освобождает все буферы и сбрасывает состояние использования
    /// </summary>
    public void deallocate()
    {
        foreach (var buffer in _buffers)
        {
            // Добавляем проверки как в C++ версии для большей надежности
            if (buffer.Mapped != IntPtr.Zero)
            {
                _allocator.Unmap(buffer);
            }
            if (buffer.Fd >= 0)
            {
                _allocator.Deallocate(buffer);
            }
        }
        
        _buffers.Clear();
        Array.Clear(_inUse, 0, _inUse.Length);
        _currentBuffer = 0;
        
        Console.WriteLine("All buffers deallocated");
    }

    /// <summary>
    /// Получает количество буферов
    /// </summary>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public int count() => _count;
    
    /// <summary>
    /// Получает количество буферов (С# свойство)
    /// </summary>
    public int Count => count();

    /// <summary>
    /// Получает информацию о буфере по индексу
    /// </summary>
    /// <param name="index">Индекс буфера</param>
    /// <returns>Информация о буфере</returns>
    [MethodImpl(MethodImplOptions.AggressiveInlining)]
    public DmaBufAllocator.DmaBufInfo get_info(int index) => _buffers[index];

    /// <summary>
    /// Индексатор для получения информации о буфере (C# стиль)
    /// </summary>
    public DmaBufAllocator.DmaBufInfo this[int index] => get_info(index);

    /// <summary>
    /// Находит свободный буфер, начиная с текущей позиции
    /// </summary>
    /// <returns>Индекс свободного буфера или -1, если все буферы используются</returns>
    public int get_free_buffer_index()
    {
        for (int i = 0; i < _count; i++)
        {
            int index = (_currentBuffer + i) % _count;
            if (!_inUse[index])
            {
                return index;
            }
        }
        
        Console.WriteLine("No free buffers available");
        return -1;
    }

    /// <summary>
    /// Отмечает буфер как используемый
    /// </summary>
    /// <param name="index">Индекс буфера</param>
    public void mark_in_use(int index)
    {
        if (index < 0 || index >= _count)
        {
            Console.WriteLine($"Invalid buffer index: {index}");
            return;
        }
        
        _inUse[index] = true;
        
        // Если текущий буфер только что был отмечен как используемый,
        // продвигаем указатель на следующий буфер
        if (index == (_currentBuffer % _count))
        {
            _currentBuffer = (index + 1) % _count;
        }
    }

    /// <summary>
    /// Отмечает буфер как свободный
    /// </summary>
    /// <param name="index">Индекс буфера</param>
    public void mark_free(int index)
    {
        if (index < 0 || index >= _count)
        {
            Console.WriteLine($"Invalid buffer index: {index}");
            return;
        }
        
        _inUse[index] = false;
    }

    /// <summary>
    /// Сбрасывает состояние использования всех буферов
    /// </summary>
    public void reset_usage()
    {
        Array.Clear(_inUse, 0, _inUse.Length);
        _currentBuffer = 0;
    }

    /// <summary>
    /// Запрашивает буферы на устройстве V4L2
    /// </summary>
    /// <param name="device">V4L2 устройство</param>
    /// <returns>true, если запрос был успешным</returns>
    public bool requestOnDevice(V4L2Device device)
    {
        Console.WriteLine($"Requesting {_count} buffers on device for type {_type}");
        
        var request = new v4l2_requestbuffers
        {
            count = (uint)_count,
            type = _type,
            memory = V4L2Const.V4L2_MEMORY_DMABUF
        };
        
        bool result = device.RequestBuffers(ref request);
        
        if (!result)
        {
            Console.WriteLine("Failed to request buffers on device");
        }
        else
        {
            Console.WriteLine($"Successfully requested {request.count} buffers on device");
        }
        
        return result;
    }

    /// <summary>
    /// Освобождает буферы на устройстве V4L2
    /// </summary>
    /// <param name="device">V4L2 устройство</param>
    /// <returns>true, если операция была успешной</returns>
    public bool releaseOnDevice(V4L2Device device)
    {
        Console.WriteLine($"Releasing buffers on device for type {_type}");
        
        var request = new v4l2_requestbuffers
        {
            count = 0,
            type = _type,
            memory = V4L2Const.V4L2_MEMORY_DMABUF
        };
        
        bool result = device.RequestBuffers(ref request);
        
        if (!result)
        {
            Console.WriteLine("Failed to release buffers on device");
        }
        else
        {
            Console.WriteLine("Successfully released buffers on device");
        }
        
        return true;  // Всегда возвращаем true как в C++ версии
    }
    
    // Методы совместимости с предыдущей C# версией
    public bool Allocate(ulong size) => allocate(size);
    public void Deallocate() => deallocate();
    public int GetFree() => get_free_buffer_index();
    public void MarkInUse(int index) => mark_in_use(index);
    public void MarkFree(int index) => mark_free(index);
    public bool RequestOnDevice(V4L2Device device, uint bufType) => requestOnDevice(device);
    public bool ReleaseOnDevice(V4L2Device device, uint bufType) => releaseOnDevice(device);

    /// <summary>
    /// Получает тип буфера
    /// </summary>
    public uint Type => _type;
}
