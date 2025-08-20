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

    ~V4L2Device()
    {
        close();
    }

    /// <summary>
    /// Открывает устройство V4L2
    /// </summary>
    /// <param name="devicePath">Путь к устройству</param>
    /// <returns>true при успешном открытии</returns>
    public bool open(string devicePath)
    {
        if (is_open())
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
    public void close()
    {
        if (!is_open())
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
    public bool is_open() => _fd >= 0;
    
    /// <summary>
    /// C# свойство для совместимости
    /// </summary>
    public bool IsOpen => is_open();

    /// <summary>
    /// Возвращает файловый дескриптор устройства
    /// </summary>
    public int fd() => _fd;

    /// <summary>
    /// Вспомогательный метод для выполнения ioctl с подробной обработкой ошибок
    /// </summary>
    private bool ioctl_helper(ulong request, IntPtr arg, string requestName)
    {
        if (!is_open())
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
    private bool Ioctl(ulong request, IntPtr arg)
    {
        if (!is_open())
            return false;
            
        return LibC.ioctl(_fd, request, arg) == 0;
    }

    /// <summary>
    /// Запрашивает информацию о возможностях устройства
    /// </summary>
    public unsafe bool query_capability(ref v4l2_capability cap)
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
    public bool set_format(ref v4l2_format format)
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
    public bool get_format(ref v4l2_format format)
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
    public unsafe bool set_control(uint id, int value)
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
    public bool request_buffers(ref v4l2_requestbuffers request)
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
    public bool queue_buffer(ref v4l2_buffer buffer)
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
    public bool dequeue_buffer(ref v4l2_buffer buffer)
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
    public unsafe bool stream_on(uint type)
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
    public unsafe bool stream_off(uint type)
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
    public bool subscribe_event(ref v4l2_event_subscription subscription)
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
    public bool subscribe_to_events()
    {
        // Подписываемся на все события кодека/декодера
        var sub_src_change = new v4l2_event_subscription
        {
            type = V4L2Const.V4L2_EVENT_SOURCE_CHANGE
        };
        
        if (!subscribe_event(ref sub_src_change))
        {
            Console.WriteLine("❌ Failed to subscribe to SOURCE_CHANGE events");
            return false;
        }

        var sub_eos = new v4l2_event_subscription
        {
            type = V4L2Const.V4L2_EVENT_EOS
        };
        if (!subscribe_event(ref sub_eos))
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
    public bool dequeue_event(ref v4l2_event ev)
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
    public bool poll(short events, int timeoutMs)
    {
        if (!is_open())
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
    public bool has_event() => (_revents & LibC.POLLPRI) != 0;
    
    /// <summary>
    /// C# свойство для совместимости
    /// </summary>
    public bool HasEvent => has_event();

    /// <summary>
    /// Проверяет наличие ошибки на устройстве
    /// </summary>
    public bool has_error() => (_revents & LibC.POLLERR) != 0;
    
    /// <summary>
    /// C# свойство для совместимости
    /// </summary>
    public bool HasError => has_error();

    /// <summary>
    /// Проверяет готовность устройства к чтению
    /// </summary>
    public bool is_ready_for_read() => (_revents & LibC.POLLIN) != 0;
    
    /// <summary>
    /// C# свойство для совместимости
    /// </summary>
    public bool ReadyRead => is_ready_for_read();

    /// <summary>
    /// Проверяет готовность устройства к записи
    /// </summary>
    public bool is_ready_for_write() => (_revents & LibC.POLLOUT) != 0;
    
    /// <summary>
    /// C# свойство для совместимости
    /// </summary>
    public bool ReadyWrite => is_ready_for_write();

    /// <summary>
    /// Настраивает форматы для декодирования
    /// </summary>
    public bool configure_decoder_formats(uint width, uint height, uint inPixelFormat, uint outPixelFormat)
    {
        return configure_decoder_formats(width, height, inPixelFormat, outPixelFormat, width * height); // По умолчанию размер входного буфера
    }

    /// <summary>
    /// Настраивает форматы для декодирования с указанным размером входного буфера
    /// </summary>
    public unsafe bool configure_decoder_formats(uint width, uint height, uint inPixelFormat, uint outPixelFormat, uint inputSizeImage)
    {
        Console.WriteLine($"Configuring decoder formats: {width}x{height}, in=0x{inPixelFormat:X}, out=0x{outPixelFormat:X}");
        
        // Настраиваем входной формат (OUTPUT)
        var inputFormat = new v4l2_format
        {
            type = V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
            fmt = new v4l2_pix_mp
            {
                width = width,
                height = height,
                pixelformat = inPixelFormat,
                num_planes = 1
            }
        };
        inputFormat.fmt.plane_fmt_0.sizeimage = inputSizeImage;
        inputFormat.fmt.plane_fmt_0.bytesperline = 0;
        
        if (!set_format(ref inputFormat))
        {
            Console.WriteLine("❌ Failed to set input format");
            return false;
        }
        
        // Настраиваем выходной формат (CAPTURE)
        var outputFormat = new v4l2_format
        {
            type = V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
            fmt = new v4l2_pix_mp
            {
                width = width,
                height = height,
                pixelformat = outPixelFormat,
                num_planes = 1 // Для NV12 будет 2, но драйвер может сам исправить
            }
        };
        // Для NV12, sizeimage это общий размер, а bytesperline - ширина luma
        outputFormat.fmt.plane_fmt_0.sizeimage = width * height * 3 / 2; 
        outputFormat.fmt.plane_fmt_0.bytesperline = (ushort)width;
        
        if (!set_format(ref outputFormat))
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
    public bool check_dma_buf_support()
    {
        Console.WriteLine("Checking DMA-BUF support...");
        
        var request = new v4l2_requestbuffers
        {
            count = 1,
            type = V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE,
            memory = V4L2Const.V4L2_MEMORY_DMABUF
        };
        
        if (!request_buffers(ref request))
        {
            Console.WriteLine("❌ DMA-BUF not supported");
            return false;
        }
        
        // Освобождаем запрошенные буферы
        request.count = 0;
        request_buffers(ref request);
        
        Console.WriteLine("✅ DMA-BUF is supported");
        return true;
    }
    
    /// <summary>
    /// Инициализирует устройство для декодирования
    /// </summary>
    public bool initialize_for_decoding(string devicePath)
    {
        Console.WriteLine($"Initializing decoder on {devicePath}");
        
        if (!open(devicePath))
        {
            Console.WriteLine("❌ Failed to open device");
            return false;
        }
        
        // Проверка возможностей
        var capability = new v4l2_capability();
        if (!query_capability(ref capability))
        {
            Console.WriteLine("❌ Failed to query capabilities");
            close();
            return false;
        }
        
        // Проверка поддержки DMA-BUF
        if (!check_dma_buf_support())
        {
            Console.WriteLine("❌ DMA-BUF not supported");
            close();
            return false;
        }
        
        // Подписка на события
        if (!subscribe_to_events())
        {
            Console.WriteLine("❌ Failed to subscribe to events");
            close();
            return false;
        }
        
        Console.WriteLine("✅ Device initialized for decoding");
        return true;
    }

    /// <summary>
    /// Ставит многоплоскостной DMA-BUF буфер в очередь
    /// </summary>
    public bool queue_multi_plane_dmabuf(uint index, uint type, int[] fds, uint[] lengths, uint[] dataOffsets)
    {
        return queue_multi_plane_dmabuf(index, type, fds, lengths, dataOffsets, null, 0);
    }

    /// <summary>
    /// Ставит многоплоскостной DMA-BUF буфер в очередь с дополнительными параметрами
    /// </summary>
    public unsafe bool queue_multi_plane_dmabuf(uint index, uint type, int[] fds, uint[] lengths, uint[] dataOffsets, uint[]? bytesused, uint flags = 0)
    {
        if (fds == null || lengths == null || dataOffsets == null)
        {
            Console.WriteLine("❌ Invalid parameters: arrays cannot be null");
            return false;
        }
        
        int planes = fds.Length;
        
        if (planes == 0 || lengths.Length != planes || dataOffsets.Length != planes)
        {
            Console.WriteLine("❌ Invalid parameters: array lengths must match");
            return false;
        }
        
        if (bytesused != null && bytesused.Length != planes)
        {
            Console.WriteLine("❌ Invalid parameters: bytesused array length must match planes");
            return false;
        }
        
        Console.WriteLine($"Queueing DMA-BUF buffer: index={index}, type={type}, planes={planes}, flags=0x{flags:X}");
        
        int planeSize = Marshal.SizeOf<v4l2_plane>();
        IntPtr planeMemory = Marshal.AllocHGlobal(planeSize * planes);
        
        try
        {
            // Инициализируем структуры плоскостей
            for (int i = 0; i < planes; i++)
            {
                var plane = new v4l2_plane
                {
                    bytesused = bytesused != null ? bytesused[i] : 0,
                    length = lengths[i],
                    m_fd = fds[i],
                    data_offset = dataOffsets[i],
                    reserved = new uint[11]  // Обнуляем reserved
                };
                
                IntPtr dst = planeMemory + i * planeSize;
                Marshal.StructureToPtr(plane, dst, false);
                
                Console.WriteLine($"  Plane {i}: fd={fds[i]}, length={lengths[i]}, offset={dataOffsets[i]}");
            }
            
            // Создаем буфер
            var buffer = new v4l2_buffer
            {
                index = index,
                type = type,
                memory = V4L2Const.V4L2_MEMORY_DMABUF,
                length = (uint)planes,
                m_planes = planeMemory,
                flags = flags
            };
            
            bool result = queue_buffer(ref buffer);
            
            if (!result)
            {
                Console.WriteLine("❌ Failed to queue buffer");
            }
            else
            {
                Console.WriteLine("✅ Buffer queued successfully");
            }
            
            return result;
        }
        finally
        {
            Marshal.FreeHGlobal(planeMemory);
        }
    }

    /// <summary>
    /// Ставит одноплоскостной DMA-BUF буфер в очередь
    /// </summary>
    public bool queue_single_plane_dmabuf(uint index, uint type, int fd, uint bytesused, uint length, uint flags = 0)
    {
        Console.WriteLine($"Queueing single-plane DMA-BUF buffer: index={index}, type={type}, fd={fd}, bytesused={bytesused}, flags=0x{flags:X}");

        var plane = new v4l2_plane
        {
            bytesused = bytesused,
            length = length,
            m_fd = fd,
            data_offset = 0
        };

        int planeSize = Marshal.SizeOf<v4l2_plane>();
        IntPtr planeMemory = Marshal.AllocHGlobal(planeSize);

        try
        {
            Marshal.StructureToPtr(plane, planeMemory, false);

            var buffer = new v4l2_buffer
            {
                index = index,
                type = type,
                memory = V4L2Const.V4L2_MEMORY_DMABUF,
                length = 1, // Single plane
                m_planes = planeMemory,
                flags = flags
            };

            return queue_buffer(ref buffer);
        }
        finally
        {
            Marshal.FreeHGlobal(planeMemory);
        }
    }

    /// <summary>
    /// Извлекает многоплоскостной буфер из очереди обработанных
    /// </summary>
    public unsafe bool dequeue_multi_plane(uint type, out uint index, out uint flags, out uint sequence, out v4l2_plane[] planes)
    {
        index = flags = sequence = 0;
        planes = Array.Empty<v4l2_plane>();
        
        // Предполагаем максимум 4 плоскости (обычно для NV12/YUV420 и подобных)
        int maxPlanes = 4;
        int planeSize = Marshal.SizeOf<v4l2_plane>();
        IntPtr planeMemory = Marshal.AllocHGlobal(planeSize * maxPlanes);
        
        try
        {
            var buffer = new v4l2_buffer
            {
                type = type,
                memory = V4L2Const.V4L2_MEMORY_DMABUF,
                length = (uint)maxPlanes,
                m_planes = planeMemory
            };
            
            if (!dequeue_buffer(ref buffer))
            {
                Console.WriteLine("❌ Failed to dequeue buffer");
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
            
            for (int i = 0; i < actualPlanes; i++)
            {
                IntPtr src = planeMemory + i * planeSize;
                planes[i] = Marshal.PtrToStructure<v4l2_plane>(src);
            }
            
            Console.WriteLine($"Dequeued buffer {index} with {actualPlanes} planes, sequence={sequence}");
            return true;
        }
        finally
        {
            Marshal.FreeHGlobal(planeMemory);
        }
    }
    
    /// <summary>
    /// Методы совместимости с предыдущей C# версией
    /// </summary>
    public bool Open(string path) => open(path);
    public void Close() => close();
    public bool Poll(short events, int timeout) => poll(events, timeout);
    public unsafe bool SetFormat(ref v4l2_format format) => set_format(ref format);
    public unsafe bool GetFormat(ref v4l2_format format) => get_format(ref format);
    public unsafe bool StreamOn(uint type) => stream_on(type);
    public unsafe bool StreamOff(uint type) => stream_off(type);
    public unsafe bool QueryCapability(ref v4l2_capability cap) => query_capability(ref cap);
    public unsafe bool RequestBuffers(ref v4l2_requestbuffers req) => request_buffers(ref req);
    public unsafe bool QBuf(ref v4l2_buffer buf) => queue_buffer(ref buf);
    public unsafe bool DQBuf(ref v4l2_buffer buf) => dequeue_buffer(ref buf);
    public unsafe bool SubscribeEvent(ref v4l2_event_subscription sub) => subscribe_event(ref sub);
    public unsafe bool DQEvent(ref v4l2_event ev) => dequeue_event(ref ev);
    public unsafe bool SetControl(uint id, int value) => set_control(id, value);
    public bool CheckDmaBufSupport() => check_dma_buf_support();
    public bool ConfigureDecoderFormats(uint w, uint h, uint inPix, uint outPix, uint inputSizeImage) => 
        configure_decoder_formats(w, h, inPix, outPix, inputSizeImage);
    public bool QueueMultiPlaneDmabuf(uint index, uint type, int[] fds, uint[] lengths, uint[] dataOffsets) =>
        queue_multi_plane_dmabuf(index, type, fds, lengths, dataOffsets);
    public bool QueueMultiPlaneDmabuf(uint index, uint type, int[] fds, uint[] lengths, uint[] dataOffsets, uint[]? bytesused, uint flags = 0) =>
        queue_multi_plane_dmabuf(index, type, fds, lengths, dataOffsets, bytesused, flags);
    public bool DequeueMultiPlane(uint type, out uint index, out uint flags, out uint sequence, out v4l2_plane[] planes) =>
        dequeue_multi_plane(type, out index, out flags, out sequence, out planes);

    /// <summary>
    /// Освобождает ресурсы
    /// </summary>
    public void Dispose() => close();
}
