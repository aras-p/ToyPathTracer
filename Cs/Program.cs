using System;
using System.Diagnostics;
using System.IO;

class Program
{
    static void Main(string[] args)
    {
        int width = 1280;
        int height = 720;
        float[] backbuffer = new float[width * height * 4];
        Stopwatch stopwatch = new Stopwatch();
        long totalRayCount = 0;

        Test test = new Test();
        const int kFrameCount = 30;
        stopwatch.Start();
        float s = 0.0f;
        for (int i = 0; i < kFrameCount; ++i)
        {
            int rayCount;
            test.DrawTest(s, i, width, height, backbuffer, out rayCount);
            totalRayCount += rayCount;

            s = (float)((double)stopwatch.ElapsedTicks / (double)Stopwatch.Frequency);
            int frames = i + 1;

            float ms = s / frames * 1000.0f;
            float mrayS = (float)(totalRayCount / s * 1.0e-6f);
            float mrayFr = rayCount * 1.0e-6f;
            System.Console.WriteLine($"{ms:F2}ms {mrayS:F1}Mrays/s {mrayFr:F2}Mrays/frame frames {frames}");
        }

        byte[] bytes = new byte[backbuffer.Length];
        for (int i = 0; i < backbuffer.Length; i += 4)
        {
            bytes[i + 0] = (byte)(LinearToSRGB(backbuffer[i + 2]));
            bytes[i + 1] = (byte)(LinearToSRGB(backbuffer[i + 1]));
            bytes[i + 2] = (byte)(LinearToSRGB(backbuffer[i + 0]));
            bytes[i + 3] = 255;
        }
        byte[] header = {
            0, // ID length
            0, // no color map
            2, // uncompressed, true color
            0, 0, 0, 0,
            0,
            0, 0, 0, 0, // x and y origin
            (byte)(width & 0x00FF),
            (byte)((width & 0xFF00) >> 8),
            (byte)(height & 0x00FF),
            (byte)((height & 0xFF00) >> 8),
            32, // 32 bit
            0 };
        using (BinaryWriter writer = new BinaryWriter(new FileStream("output.tga", FileMode.Create)))
        {
            writer.Write(header);
            writer.Write(bytes);
        }
    }

    static uint LinearToSRGB(float x)
    {
        x = Math.Max(x, 0.0f);
        x = Math.Max(1.055f * (float)Math.Pow(x, 0.416666667f) - 0.055f, 0.0f);
        uint u = Math.Min((uint)(x * 255.9f), 255u);
        return u;
    }

}
