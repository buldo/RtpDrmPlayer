using System;
using System.Threading;
using RtpDrmPlayer.Native;

namespace RtpDrmPlayer.Core;

/// <summary>
/// Управляет потоками V4L2. В точности повторяет StreamingManager из C++ оригинала.
/// </summary>
public class StreamingManager
{
    private readonly V4L2Device _device;
    private readonly DmaBuffersManager _outputBuffers;
    private State _state = State.STOPPED;

    private enum State
    {
        STOPPED,
        STARTING,
        ACTIVE,
        STOPPING,
        ERROR
    }

    public StreamingManager(V4L2Device device, DmaBuffersManager outputBuffers)
    {
        _device = device;
        _outputBuffers = outputBuffers;
    }

    public bool is_active() => _state == State.ACTIVE;
    public bool IsActive => is_active();

    public void set_inactive()
    {
        _state = State.STOPPED;
    }

    /// <summary>
    /// Запускает поток, сначала ставя в очередь выходные буферы, затем включая поток ввода и вывода.
    /// </summary>
    public bool start()
    {
        if (_state == State.ACTIVE)
        {
            Console.WriteLine("Streaming is already active");
            return true;
        }

        _state = State.STARTING;

        if (!queueOutputBuffers())
        {
            _state = State.ERROR;
            return false;
        }

        if (!enableStreaming())
        {
            _state = State.ERROR;
            return false;
        }

        _state = State.ACTIVE;
        Console.WriteLine("✅ Streaming started successfully");
        return true;
    }

    /// <summary>
    /// Останавливает поток с небольшой задержкой после остановки.
    /// </summary>
    public bool stop()
    {
        if (_state == State.STOPPED)
        {
            return true;
        }

        _state = State.STOPPING;

        _device.StreamOff(V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE);
        _device.StreamOff(V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);

        _state = State.STOPPED;

        // Задержка как в C++ оригинале
        Thread.Sleep(10); // 10ms

        Console.WriteLine("✅ Streaming stopped");
        return true;
    }

    // Legacy метод для совместимости 
    public bool Start() => start();

    // Legacy метод для совместимости
    public bool Stop() => stop();

    /// <summary>
    /// Ставит все выходные буферы в очередь для захвата.
    /// </summary>
    private bool queueOutputBuffers()
    {
        Console.WriteLine($"Queuing {_outputBuffers.Count} output buffers...");

        for (uint i = 0; i < _outputBuffers.Count; i++)
        {
            if (!queueOutputBuffer(i))
            {
                Console.WriteLine($"❌ Error queuing buffer {i}");
                return false;
            }
        }
        return true;
    }

    /// <summary>
    /// Ставит один выходной буфер в очередь для захвата.
    /// </summary>
    private bool queueOutputBuffer(uint index)
    {
        if (index >= _outputBuffers.Count)
        {
            Console.WriteLine($"❌ Invalid buffer index: {index}");
            return false;
        }

        var info = _outputBuffers[(int)index];
        if (info.Fd < 0)
        {
            Console.WriteLine($"❌ Invalid DMA-BUF fd for buffer {index}");
            return false;
        }

        var plane = new v4l2_plane
        {
            m_fd = info.Fd,
            length = (uint)info.Size
        };

        var buf = new v4l2_buffer
        {
            type = V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
            memory = V4L2Const.V4L2_MEMORY_DMABUF,
            index = index,
            length = 1
        };

        // Используем C# метод QBuf с planes для полной поддержки multi-plane
        if (!_device.QueueMultiPlaneDmabuf(index, V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE,
            new[] { info.Fd }, new[] { (uint)info.Size }, new uint[] { 0 }))
        {
            Console.WriteLine($"❌ VIDIOC_QBUF for buffer {index} failed");
            return false;
        }
        return true;
    }

    /// <summary>
    /// Включает потоки ввода и вывода в правильном порядке.
    /// </summary>
    private bool enableStreaming()
    {
        if (!_device.StreamOn(V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE))
        {
            Console.WriteLine("❌ VIDIOC_STREAMON for input failed");
            return false;
        }

        if (!_device.StreamOn(V4L2Const.V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE))
        {
            Console.WriteLine("❌ VIDIOC_STREAMON for output failed");
            _device.StreamOff(V4L2Const.V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE);
            return false;
        }

        return true;
    }
}
