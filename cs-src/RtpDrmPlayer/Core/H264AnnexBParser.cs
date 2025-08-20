using System;
using System.Collections.Generic;

namespace RtpDrmPlayer.Core;

// Примитивный парсер AnnexB: находит старт-коды и возвращает NAL-блоки.
internal static class H264AnnexBParser
{
    public struct Nal
    {
        public int Type;      // нижние 5 бит заголовка
        public int Offset;    // позиция первого байта заголовка (после старт-кода)
        public int Length;    // длина данных NAL включая первый байт заголовка
    }

    public static List<Nal> Parse(ReadOnlySpan<byte> data)
    {
        var list = new List<Nal>(8);
        int n = data.Length;
        int i = 0;
        while (true)
        {
            int sc = FindStartCode(data, i);
            if (sc < 0) break;
            int scSize = (data[sc + 2] == 1) ? 3 : 4;
            int nalStart = sc + scSize;
            if (nalStart >= n) break;
            int next = FindStartCode(data, nalStart + 1);
            int end = next < 0 ? n : next;
            int len = end - nalStart;
            if (len <= 0)
            {
                if (next < 0) break; else { i = next; continue; }
            }
            int type = data[nalStart] & 0x1F;
            list.Add(new Nal { Type = type, Offset = nalStart, Length = len });
            if (next < 0) break;
            i = next;
        }
        return list;
    }

    private static int FindStartCode(ReadOnlySpan<byte> data, int from)
    {
        int n = data.Length;
        for (int j = from; j < n - 3; j++)
        {
            // 3-байтовый старт-код
            if (data[j] == 0 && data[j + 1] == 0 && data[j + 2] == 1) return j;
            // 4-байтовый старт-код
            if (j < n - 4 && data[j] == 0 && data[j + 1] == 0 && data[j + 2] == 0 && data[j + 3] == 1) return j;
        }
        return -1;
    }
}
