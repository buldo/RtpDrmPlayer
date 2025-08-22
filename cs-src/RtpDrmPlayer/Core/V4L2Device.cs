using System;
using System.Collections.Generic;
using System.Runtime.InteropServices;
using RtpDrmPlayer.Native;

namespace RtpDrmPlayer.Core;

/// <summary>
/// Класс V4L2Device инкапсулирует низкоуровневое взаимодействие с устройством V4L2.
/// Обеспечивает тонкую обертку вокруг вызовов ioctl для V4L2, управляя файловым дескриптором 
/// и выполняя базовые операции с устройством. Полностью соответствует реализации C++.
/// </summary>
public sealed class V4L2Device : IDisposable
{
    private int _fd = -1;
    private PollFd[] _pollFds = Array.Empty<PollFd>();
    private short _revents;

    public V4L2Device()
    {
        Console.WriteLine("V4L2Device created");
    }

    ~V4L2Device() => Close();

    /// <summary>
    /// Открывает устройство V4L2
    /// </summary>
    /// <param name="devicePath">Путь к устройству</param>
    /// <returns>true при успешном открытии</returns>
    public bool Open(string devicePath)
    {
    if (IsOpen)
        {
            Console.WriteLine("Device already open");
            return false;
        }
        
        Console.WriteLine($"Opening V4L2 device: {devicePath}");
        
        _fd = LibC.open(devicePath, LibC.O_RDWR | LibC.O_NONBLOCK, 0);
        
        if (_fd < 0)
        {
            Console.WriteLine($"❌ Failed to open {devicePath}, errno: {Marshal.GetLastWin32Error()}");
            return false;
        }
        
        _pollFds = new[]
        {
            new PollFd
            {
                Fd = _fd,
                Events = LibC.POLLPRI  // Инициализируем для событий по умолчанию
            }
        };
        
        Console.WriteLine($"✅ Opened V4L2 device with fd {_fd}");
        return true;
    }

    /// <summary>
    /// Закрывает устройство V4L2
    /// </summary>
    public void Close()
    {
    if (!IsOpen)
            return;
            
        Console.WriteLine($"Closing V4L2 device with fd {_fd}");
        
        LibC.close(_fd);
        _fd = -1;
        _pollFds = Array.Empty<PollFd>();
        
        Console.WriteLine("V4L2 device closed");
    }

    /// <summary>
    /// Проверяет, открыто ли устройство
    /// </summary>
    public bool IsOpen => _fd >= 0;

    /// <summary>
    /// Возвращает файловый дескриптор устройства
    /// </summary>
    public int Fd() => _fd;

    /// <summary>
    /// Вспомогательный метод для выполнения ioctl с подробной обработкой ошибок
    /// </summary>
    private bool ioctl_helper(ulong request, IntPtr arg, string requestName)
    {
    if (!IsOpen)
        {
            Console.WriteLine($"❌ Device not open for {requestName}");
            return false;
        }
        
        int result = LibC.ioctl(_fd, request, arg);
        
        if (result != 0)
        {
            int errno = Marshal.GetLastWin32Error();
            Console.WriteLine($"❌ {requestName} failed: errno={errno}");
            return false;
        }
        
        return true;
    }
    
    // Безопасная версия ioctl с использованием GCHandle
    private unsafe bool SafeIoctl<T>(ulong request, ref T structure, string requestName) where T : unmanaged
    {
        fixed (T* p = &structure)
        {
            return ioctl_helper(request, (IntPtr)p, requestName);
        }
    }
    
    // Оригинальная реализация для обратной совместимости
    // Removed legacy Ioctl wrapper (unused)

    /// <summary>
    /// Запрашивает информацию о возможностях устройства
    /// </summary>
    public unsafe bool QueryCapability(ref v4l2_capability cap)
    {
        bool result = SafeIoctl(V4L2Const.VIDIOC_QUERYCAP, ref cap, "VIDIOC_QUERYCAP");
        
        if (result)
        {
            string driverName, cardName, busInfo;
            
            fixed (byte* pDriver = cap.driver)
            {
                driverName = Marshal.PtrToStringAnsi((IntPtr)pDriver) ?? string.Empty;
            }
            
            fixed (byte* pCard = cap.card)
            {
                cardName = Marshal.PtrToStringAnsi((IntPtr)pCard) ?? string.Empty;
            }

            fixed (byte* pBusInfo = cap.bus_info)
            {
                busInfo = Marshal.PtrToStringAnsi((IntPtr)pBusInfo) ?? string.Empty;
            }
            
            Console.WriteLine($"Device capabilities: driver='{driverName}', card='{cardName}', bus='{busInfo}'");
        }
        
        return result;
    }

    /// <summary>
    /// Устанавливает формат для видео потока
    /// </summary>
    public bool SetFormat(ref v4l2_format format)
    {
        string typeName = format.type switch
        {
            V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE => "OUTPUT",
            V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE => "CAPTURE",
            _ => $"Unknown({format.type})"
        };
        
        Console.WriteLine($"Setting format for {typeName}");
        
        bool result = SafeIoctl(V4L2Const.VIDIOC_S_FMT, ref format, "VIDIOC_S_FMT");
        
        if (result && format.type == V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
        {
            Console.WriteLine($"✅ Format set: {format.fmt.width}x{format.fmt.height}, " + 
                             $"format=0x{format.fmt.pixelformat:X}, planes={format.fmt.num_planes}");
        }
        
        return result;
    }

    /// <summary>
    /// Получает текущий формат видео потока
    /// </summary>
    public bool GetFormat(ref v4l2_format format)
    {
        bool result = SafeIoctl(V4L2Const.VIDIOC_G_FMT, ref format, "VIDIOC_G_FMT");
        
        if (result)
        {
            Console.WriteLine($"Format info: {format.fmt.width}x{format.fmt.height}, " + 
                             $"format=0x{format.fmt.pixelformat:X}, planes={format.fmt.num_planes}");
        }
        
        return result;
    }

    /// <summary>
    /// Устанавливает контрольный параметр устройства
    /// </summary>
    public unsafe bool SetControl(uint id, int value)
    {
        var control = new v4l2_control { id = id, value = value };
        
        Console.WriteLine($"Setting control: id=0x{id:X}, value={value}");
        bool result = ioctl_helper(V4L2Const.VIDIOC_S_CTRL, (IntPtr)(&control), "VIDIOC_S_CTRL");
        
        if (!result)
        {
            Console.WriteLine($"❌ Failed to set control 0x{id:X}");
        }
        
        return result;
    }

    /// <summary>
    /// Запрашивает буферы указанного типа и памяти
    /// </summary>
    public bool RequestBuffers(ref v4l2_requestbuffers request)
    {
        string memType = request.memory switch
        {
            V4L2Const.V4L2_MEMORY_MMAP => "MMAP",
            V4L2Const.V4L2_MEMORY_USERPTR => "USERPTR",
            V4L2Const.V4L2_MEMORY_DMABUF => "DMABUF",
            _ => $"Unknown({request.memory})"
        };
        
        Console.WriteLine($"Requesting {request.count} buffers of type {request.type}, memory={memType}");
        bool result = SafeIoctl(V4L2Const.VIDIOC_REQBUFS, ref request, "VIDIOC_REQBUFS");
        
        if (result)
        {
            Console.WriteLine($"✅ Successfully requested {request.count} buffers");
        }
        
        return result;
    }

    /// <summary>
    /// Ставит буфер в очередь для обработки
    /// </summary>
    public bool QueueBuffer(ref v4l2_buffer buffer)
    {
        Console.WriteLine($"Queuing buffer index={buffer.index}, type={buffer.type}");
        bool result = SafeIoctl(V4L2Const.VIDIOC_QBUF, ref buffer, "VIDIOC_QBUF");
        
        if (!result)
        {
            Console.WriteLine($"❌ Failed to queue buffer {buffer.index}");
        }
        
        return result;
    }

    /// <summary>
    /// Извлекает буфер из очереди обработанных
    /// </summary>
    public bool DequeueBuffer(ref v4l2_buffer buffer)
    {
        bool result = SafeIoctl(V4L2Const.VIDIOC_DQBUF, ref buffer, "VIDIOC_DQBUF");
        
        if (result)
        {
            Console.WriteLine($"Dequeued buffer index={buffer.index}, flags=0x{buffer.flags:X}");
        }
        
        return result;
    }

    /// <summary>
    /// Включает поток указанного типа
    /// </summary>
    public unsafe bool StreamOn(uint type)
    {
        uint typeVal = type;
        
        Console.WriteLine($"Starting stream for type {type}");
        bool result = ioctl_helper(V4L2Const.VIDIOC_STREAMON, (IntPtr)(&typeVal), "VIDIOC_STREAMON");
        
        if (!result)
        {
            Console.WriteLine($"❌ Failed to start stream for type {type}");
        }
        else
        {
            Console.WriteLine($"✅ Stream started for type {type}");
        }
        
        return result;
    }

    /// <summary>
    /// Выключает поток указанного типа
    /// </summary>
    public unsafe bool StreamOff(uint type)
    {
        uint typeVal = type;
        
        Console.WriteLine($"Stopping stream for type {type}");
        bool result = ioctl_helper(V4L2Const.VIDIOC_STREAMOFF, (IntPtr)(&typeVal), "VIDIOC_STREAMOFF");
        
        if (!result)
        {
            Console.WriteLine($"❌ Failed to stop stream for type {type}");
        }
        else
        {
            Console.WriteLine($"✅ Stream stopped for type {type}");
        }
        
        return result;
    }

    /// <summary>
    /// Подписка на определенное событие устройства
    /// </summary>
    public bool SubscribeEvent(ref v4l2_event_subscription subscription)
    {
        Console.WriteLine($"Subscribing to event type {subscription.type}");
        bool result = SafeIoctl(V4L2Const.VIDIOC_SUBSCRIBE_EVENT, ref subscription, "VIDIOC_SUBSCRIBE_EVENT");
        
        if (!result)
        {
            Console.WriteLine($"❌ Failed to subscribe to event {subscription.type}");
        }
        
        return result;
    }

    /// <summary>
    /// Подписывается на все типы событий декодера
    /// </summary>
    public bool SubscribeToEvents()
    {
        // Подписываемся на все события кодека/декодера
        var sub_src_change = new v4l2_event_subscription
        {
            type = V4L2Const.V4L2_EVENT_SOURCE_CHANGE
        };
        
    if (!SubscribeEvent(ref sub_src_change))
        {
            Console.WriteLine("❌ Failed to subscribe to SOURCE_CHANGE events");
            return false;
        }

        var sub_eos = new v4l2_event_subscription
        {
            type = V4L2Const.V4L2_EVENT_EOS
        };
    if (!SubscribeEvent(ref sub_eos))
        {
            Console.WriteLine("❌ Failed to subscribe to EOS events");
            return false;
        }
        
        Console.WriteLine("✅ Successfully subscribed to decoder events");
        return true;
    }

    /// <summary>
    /// Получает событие из очереди событий
    /// </summary>
    public bool DequeueEvent(ref v4l2_event ev)
    {
        bool result = SafeIoctl(V4L2Const.VIDIOC_DQEVENT, ref ev, "VIDIOC_DQEVENT");
        
        if (result)
        {
            Console.WriteLine($"Dequeued event type={ev.type}");
        }
        
        return result;
    }

    /// <summary>
    /// Проверка ожидающих событий или готовности операций ввода-вывода
    /// </summary>
    public bool Poll(short events, int timeoutMs)
    {
    if (!IsOpen)
        {
            Console.WriteLine("❌ Cannot poll: device not open");
            return false;
        }
        
        _pollFds[0].Events = events;
        
        int result = LibC.poll(_pollFds, (uint)_pollFds.Length, timeoutMs);
        
        if (result < 0)
        {
            int errno = Marshal.GetLastWin32Error();
            Console.WriteLine($"❌ Poll failed: errno={errno}");
            _revents = 0;
            return false;
        }
        
        _revents = _pollFds[0].Revents;
        
        if (_revents != 0)
        {
            Console.WriteLine($"Poll returned events: 0x{_revents:X}");
        }
        
        return true;
    }

    /// <summary>
    /// Проверяет наличие события на устройстве
    /// </summary>
    public bool HasEvent => (_revents & LibC.POLLPRI) != 0;

    /// <summary>
    /// Проверяет наличие ошибки на устройстве
    /// </summary>
    public bool HasError => (_revents & LibC.POLLERR) != 0;

    /// <summary>
    /// Проверяет готовность устройства к чтению
    /// </summary>
    public bool ReadyRead => (_revents & LibC.POLLIN) != 0;

    /// <summary>
    /// Проверяет готовность устройства к записи
    /// </summary>
    public bool ReadyWrite => (_revents & LibC.POLLOUT) != 0;

    /// <summary>
    /// Настраивает форматы для декодирования
    /// </summary>
    public bool ConfigureDecoderFormats(uint width, uint height, uint inPixelFormat, uint outPixelFormat)
    {
    return ConfigureDecoderFormats(width, height, inPixelFormat, outPixelFormat, width * height); // По умолчанию размер входного буфера
    }

    /// <summary>
    /// Настраивает форматы для декодирования с указанным размером входного буфера
    /// </summary>
    public bool ConfigureDecoderFormats(uint width, uint height, uint inPixelFormat, uint outPixelFormat, uint inputSizeImage)
    {
        Console.WriteLine($"Configuring decoder formats: {width}x{height}, in=0x{inPixelFormat:X}, out=0x{outPixelFormat:X}");
        
        // Настраиваем входной формат (OUTPUT)
        var inputFormat = new v4l2_format
        {
            type = V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE
        };
        inputFormat.fmt.width = width;
        inputFormat.fmt.height = height;
        inputFormat.fmt.pixelformat = inPixelFormat;
        inputFormat.fmt.num_planes = 1;
        inputFormat.fmt.plane_fmt_0.sizeimage = inputSizeImage;
        inputFormat.fmt.plane_fmt_0.bytesperline = 0;
        
    if (!SetFormat(ref inputFormat))
        {
            Console.WriteLine("❌ Failed to set input format");
            return false;
        }
        
        // Настраиваем выходной формат (CAPTURE)
        var outputFormat = new v4l2_format
        {
            type = V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE
        };
        outputFormat.fmt.width = width;
        outputFormat.fmt.height = height;
        outputFormat.fmt.pixelformat = outPixelFormat;
        outputFormat.fmt.num_planes = 1; // Для NV12 будет 2, но драйвер может сам исправить
        // Для NV12, sizeimage это общий размер, а bytesperline - ширина luma
        outputFormat.fmt.plane_fmt_0.sizeimage = width * height * 3 / 2; 
        outputFormat.fmt.plane_fmt_0.bytesperline = (ushort)width;
        
    if (!SetFormat(ref outputFormat))
        {
            Console.WriteLine("❌ Failed to set output format");
            return false;
        }
        
        Console.WriteLine("✅ Successfully configured decoder formats");
        return true;
    }

    /// <summary>
    /// Проверяет поддержку DMA-BUF
    /// </summary>
    public bool CheckDmaBufSupport()
    {
        Console.WriteLine("Checking DMA-BUF support...");
        
        var request = new v4l2_requestbuffers
        {
            count = 1,
            type = V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
            memory = V4L2Const.V4L2_MEMORY_DMABUF
        };
        
    if (!RequestBuffers(ref request))
        {
            Console.WriteLine("❌ DMA-BUF not supported");
            return false;
        }
        
        // Освобождаем запрошенные буферы
        request.count = 0;
    RequestBuffers(ref request);
        
        Console.WriteLine("✅ DMA-BUF is supported");
        return true;
    }
    
    /// <summary>
    /// Инициализирует устройство для декодирования
    /// </summary>
    public bool InitializeForDecoding(string devicePath)
    {
        Console.WriteLine($"Initializing decoder on {devicePath}");
        
    if (!Open(devicePath))
        {
            Console.WriteLine("❌ Failed to open device");
            return false;
        }
        
        // Проверка возможностей
        var capability = new v4l2_capability();
    if (!QueryCapability(ref capability))
        {
            Console.WriteLine("❌ Failed to query capabilities");
            Close();
            return false;
        }
        
        // Проверка поддержки DMA-BUF
    if (!CheckDmaBufSupport())
        {
            Console.WriteLine("❌ DMA-BUF not supported");
            Close();
            return false;
        }
        
        // Подписка на события
    if (!SubscribeToEvents())
        {
            Console.WriteLine("❌ Failed to subscribe to events");
            Close();
            return false;
        }
        
        Console.WriteLine("✅ Device initialized for decoding");
        return true;
    }

    /// <summary>
    /// Ставит многоплоскостной DMA-BUF буфер в очередь
    /// </summary>
    public bool QueueMultiPlaneDmabuf(uint index, uint type, int[] fds, uint[] lengths, uint[] dataOffsets) =>
        QueueMultiPlaneDmabuf(index, type, fds, lengths, null, dataOffsets, 0);

    /// <summary>
    /// Ставит многоплоскостной DMA-BUF буфер в очередь с дополнительными параметрами
    /// </summary>
    public unsafe bool QueueMultiPlaneDmabuf(uint index, uint type, int[] fds, uint[] lengths, uint[]? bytesused, uint[] dataOffsets, uint flags = 0)
    {
        if (fds == null || lengths == null || dataOffsets == null)
        {
            Console.WriteLine("❌ Invalid parameters: arrays cannot be null");
            return false;
        }
        
        int planesCount = fds.Length;
        
        if (planesCount == 0 || lengths.Length != planesCount || dataOffsets.Length != planesCount)
        {
            Console.WriteLine("❌ Invalid parameters: array lengths must match");
            return false;
        }
        
        if (bytesused != null && bytesused.Length != planesCount)
        {
            Console.WriteLine("❌ Invalid parameters: bytesused array length must match planes");
            return false;
        }
        
        Console.WriteLine($"Queueing DMA-BUF buffer: index={index}, type={type}, planes={planesCount}, flags=0x{flags:X}");
        
        var planes = new v4l2_plane[planesCount];
        for (int i = 0; i < planesCount; i++)
        {
            planes[i] = new v4l2_plane
            {
                bytesused = bytesused?[i] ?? 0,
                length = lengths[i],
                data_offset = dataOffsets[i]
            };
            planes[i].m_fd = fds[i];
            Console.WriteLine($"  Plane {i}: fd={fds[i]}, length={lengths[i]}, bytesused={planes[i].bytesused}, offset={dataOffsets[i]}");
        }

        fixed (v4l2_plane* pPlanes = planes)
        {
            var buffer = new v4l2_buffer
            {
                index = index,
                type = type,
                memory = V4L2Const.V4L2_MEMORY_DMABUF,
                length = (uint)planesCount,
                flags = flags,
                m_planes = (IntPtr)pPlanes
            };
            return QueueBuffer(ref buffer);
        }
    }

    /// <summary>
    /// Ставит одноплоскостной DMA-BUF буфер в очередь
    /// </summary>
    public unsafe bool QueueSinglePlaneDmabuf(uint index, uint type, int fd, uint bytesused, uint length, uint flags = 0)
    {
        Console.WriteLine($"Queueing single-plane DMA-BUF buffer: index={index}, type={type}, fd={fd}, bytesused={bytesused}, flags=0x{flags:X}");

        var plane = new v4l2_plane
        {
            bytesused = bytesused,
            length = length,
            data_offset = 0
        };
        plane.m_fd = fd;

        var buffer = new v4l2_buffer
        {
            index = index,
            type = type,
            memory = V4L2Const.V4L2_MEMORY_DMABUF,
            length = 1, // Single plane
            flags = flags,
            m_planes = (IntPtr)(&plane)
        };

    return QueueBuffer(ref buffer);
    }

    /// <summary>
    /// Извлекает многоплоскостной буфер из очереди обработанных
    /// </summary>
    public unsafe bool DequeueMultiPlane(uint type, out uint index, out uint flags, out uint sequence, out v4l2_plane[] planes)
    {
        index = flags = sequence = 0;
        planes = Array.Empty<v4l2_plane>();
        
        // Предполагаем максимум 4 плоскости (обычно для NV12/YUV420 и подобных)
        int maxPlanes = 4;
        var planeArray = new v4l2_plane[maxPlanes];

        fixed (v4l2_plane* pPlanes = planeArray)
        {
            var buffer = new v4l2_buffer
            {
                type = type,
                memory = V4L2Const.V4L2_MEMORY_DMABUF,
                length = (uint)maxPlanes,
                m_planes = (IntPtr)pPlanes
            };
            
            if (!DequeueBuffer(ref buffer))
            {
                // Console.WriteLine("❌ Failed to dequeue buffer"); // This is too noisy for non-blocking calls
                return false;
            }
            
            index = buffer.index;
            flags = buffer.flags;
            sequence = buffer.sequence;
            
            int actualPlanes = (int)buffer.length;
            if (actualPlanes > maxPlanes)
            {
                actualPlanes = maxPlanes;
            }
            
            planes = new v4l2_plane[actualPlanes];
            Array.Copy(planeArray, planes, actualPlanes);
            
            // Console.WriteLine($"Dequeued buffer {index} with {actualPlanes} planes, sequence={sequence}");
            return true;
        }
    }
    
    /// <summary>
    /// Методы совместимости с предыдущей C# версией
    /// </summary>
    // Removed legacy compatibility wrappers after renaming core methods to PascalCase.

    /// <summary>
    /// Освобождает ресурсы
    /// </summary>
    public void Dispose() => Close();
}
