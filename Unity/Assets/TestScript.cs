using System.Diagnostics;
using UnityEngine;
using Unity.Collections;

public class TestScript : MonoBehaviour
{
    public bool m_Animate = true;
    public UnityEngine.UI.Text m_UIPerfText;
    public UnityEngine.UI.RawImage m_UIImage;
    
    Texture2D m_BackbufferTex;
    NativeArray<Color> m_Backbuffer;
    Test m_Test;

    Stopwatch m_Stopwatch = new Stopwatch();
    int m_UpdateCounter;
    int m_FrameCounter;
    long m_RayCounter;

    void Start ()
    {
        int width = 1280, height = 720;
        m_BackbufferTex = new Texture2D(width, height, TextureFormat.RGBAFloat, false);
        m_Backbuffer = new NativeArray<Color>(width * height, Allocator.Persistent);
        for (int i = 0; i < m_Backbuffer.Length; i++)
            m_Backbuffer[i] = new Color(0,0,0,1);
        m_UIImage.texture = m_BackbufferTex;
        m_Test = new Test();
    }

    private void OnDestroy()
    {
        m_Backbuffer.Dispose();
        m_Test.Dispose();
    }

    void UpdateLoop()
    {
        m_Stopwatch.Start();
        int rayCount;
        m_Test.DrawTest(Time.timeSinceLevelLoad, m_FrameCounter++, m_BackbufferTex.width, m_BackbufferTex.height, m_Backbuffer, m_Animate, out rayCount);
        m_Stopwatch.Stop();
        ++m_UpdateCounter;
        m_RayCounter += rayCount;
    }

    void Update ()
    {
        UpdateLoop();
        if (m_UpdateCounter == 10)
        {
            var s = (float)((double)m_Stopwatch.ElapsedTicks / (double)Stopwatch.Frequency) / m_UpdateCounter;
            var ms = s * 1000.0f;
            var mrayS = m_RayCounter / m_UpdateCounter / s * 1.0e-6f;
            var mrayFr = m_RayCounter / m_UpdateCounter * 1.0e-6f;
            m_UIPerfText.text = string.Format("{0:F2}ms ({1:F2}FPS) {2:F2}Mrays/s {3:F2}Mrays/frame {4} frames", ms, 1.0f / s, mrayS, mrayFr, m_FrameCounter);
            m_UpdateCounter = 0;
            m_RayCounter = 0;
            m_Stopwatch.Reset();
        }

        m_BackbufferTex.LoadRawTextureData(m_Backbuffer);
        m_BackbufferTex.Apply();
    }
}
