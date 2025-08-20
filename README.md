# RtpDrmPlayer
RTP (H.264) → V4L2 M2M decode → DRM/KMS zero‑copy отображение (DMA-BUF PRIME) на Linux (Raspberry Pi / DRM GPU).

## Возможности (текущее состояние)
* V4L2 M2M multi-plane (NV12 / YUV420) буферизация, события (SOURCE_CHANGE, EOS, FRAME_SYNC)
* Динамическая переконфигурация при смене разрешения (освобождение/перевыделение буферов, пересоздание DRM FB)
* DRM legacy modeset + AddFB2 + page flip (best-effort), кэширование GEM→FB
* DMA-BUF allocator (dma-heap) + GEM импорт (PRIME_FD_TO_HANDLE) + синхронизация DMA_BUF_IOCTL_SYNC
* Тестовая валидация ioctl/констант (V4L2/DMA-BUF/DRM/libc) через native C shim
* Эмулятор RTP (псевдо uvgRTP) генерирует H.264 AnnexB кадры (ключевые + P) и подает в OUTPUT очередь

## TODO (для production)
1. Реальный RTP прием (uvgRTP binding или иная библиотека) вместо эмулятора.
2. Настоящий парсинг H.264 (выделение SPS/PPS, keyframe gating) и управление параметрами декодера.
3. Atomic DRM (DRM_IOCTL_MODE_ATOMIC) + двойная/тройная буферизация для плавного вывода.
4. Управление цветовым пространством/stride через драйвер, корректная работа с несколькими FD для multi-plane (если драйвер их возвращает).
5. Улучшенное логирование (структурированное) и обработка ошибок/метрик.
6. Graceful shutdown с остановкой потоков до освобождения GEM/FB.

## Сборка
```bash
dotnet build cs-src/RtpDrmPlayer.sln -c Release
```
Тесты (валидность констант):
```bash
dotnet test cs-src/RtpDrmPlayer.sln -c Debug
```

## Запуск (эмуляция RTP)
Пример простого использования (пока код запуска внутри `RtpDrmPlayer` приложения – см. `Program.cs`). Запуск:
```bash
dotnet run --project cs-src/RtpDrmPlayer/RtpDrmPlayer.csproj
```
Необходимо наличие:* `/dev/video*` V4L2 m2m декодера (например, `video11` для bcm2835-codec или аналог на SoC)* `/dev/dri/card0` DRM устройство* `/dev/dma_heap/system` (или другой dma-heap) для выделения буферов.

## Архитектура
| Слой | Функция |
|------|---------|
| `RtpDrmPlayer.Native` | P/Invoke структуры, ioctl константы (V4L2, DRM, DMA-BUF, libc) |
| Native tests (gcc shim) | Экспорт реальных значений для сверки в xUnit |
| Core: `V4L2Device` | Обертки ioctl, Q/DQ multi-plane DMABUF |
| Core: `V4L2Decoder` | Инициализация форматов, поток опроса (poll), события, переконфигурация |
| Core: `DmaBufAllocator` / `DmaBuffersManager` | Выделение dma-heap и управление пулами буферов |
| Core: `DrmDmaBufDisplayManager` | PRIME импорт, AddFB2, modeset, page flip, FB кэш |
| Core: `FrameProcessor` | Подготовка кадра к отображению, учёт размера |
| Core: `UvgRtpReceiver` | Эмуляция RTP потока H.264 (планируется реальный биндинг) |
| Core: `RtpPlayer` | Склейка RTP → Decoder (подача через OUTPUT очередь) |

## Поток данных
RTP (H.264 AnnexB) → (эмулятор) → `RtpPlayer` → `V4L2Decoder.FeedInputFrame` (OUTPUT queue) → HW decode → CAPTURE DQ → `FrameProcessor` → `DrmDmaBufDisplayManager` (PRIME import / AddFB2 / flip).

## Переконфигурация
При SOURCE_CHANGE драйвер сообщает об изменении формата: выполняется STREAMOFF capture, освобождение REQBUFS, повторный G_FMT, переразметка буферов, STREAMON, обновление DRM (OnResolutionChange) и размеров в FrameProcessor.

## Ограничения
* Нет реального парсинга H.264 (заглушка генерирует случайные payload). Декодер может отказать без корректных SPS/PPS.
* Нет управления потоком (jitter buffer, RTCP, packet loss recovery).
* DRM реализован только в legacy режиме (без atomic). Нет vsync обработки.

## Лицензия
См. `LICENSE`.

---
Проект сгенерирован и развивался при помощи AI (итеративные шаги). Вклад приветствуется.
