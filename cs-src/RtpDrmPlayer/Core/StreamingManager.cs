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
    public void Start()
    {
        if (_state == State.ACTIVE) return;

        Console.WriteLine($"[StreamingManager] Starting stream for type {_outputBuffers.Type}...");
    // Device STREAMON now managed centrally in V4L2Decoder.Start()
    _state = State.ACTIVE;
    Console.WriteLine($"[StreamingManager] Marked ACTIVE (device already streaming)");
    }

    /// <summary>
    /// Останавливает поток с небольшой задержкой после остановки.
    /// </summary>
    public bool Stop()
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
    // Legacy alias removed; Start/Stop now PascalCase

    // Streaming enable handled by V4L2Decoder
}
