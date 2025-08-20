using System;
using RtpDrmPlayer.Core;

namespace RtpDrmPlayer;

internal class Program
{
    static void Main(string[] args)
    {
        string device = "/dev/video10"; string ip = "0.0.0.0"; ushort port = 5600;
        using var player = new RtpPlayer(device, ip, port);
        if(!player.Initialize())
            return;

        player.Start();
        Console.WriteLine("Press ENTER to stop...");
        Console.ReadLine();
        player.Stop();
    }
}

