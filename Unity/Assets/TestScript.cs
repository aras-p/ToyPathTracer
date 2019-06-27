using System.Diagnostics;
using UnityEngine;
using Unity.Collections;

public class TestScript : MonoBehaviour
{
    public UnityEngine.UI.Text m_UIPerfText;
    public UnityEngine.UI.RawImage m_UIImage;
    
    Texture2D m_BackbufferTex;
    NativeArray<uint> m_Backbuffer;
    Test m_Test;

    Stopwatch m_Stopwatch = new Stopwatch();
    int m_UpdateCounter;

    void Start ()
    {
        int width = 1280, height = 720;
        m_BackbufferTex = new Texture2D(width, height, TextureFormat.RGBA32, false);
        m_Backbuffer = new NativeArray<uint>(width * height, Allocator.Persistent);
        for (int i = 0; i < m_Backbuffer.Length; i++)
            m_Backbuffer[i] = 0;
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
        m_Test.DrawTest(m_BackbufferTex.width, m_BackbufferTex.height, m_Backbuffer);
        m_Stopwatch.Stop();
        ++m_UpdateCounter;
    }

    void Update ()
    {
        UpdateLoop();
        if (m_UpdateCounter == 10)
        {
            var s = (float)((double)m_Stopwatch.ElapsedTicks / (double)Stopwatch.Frequency) / m_UpdateCounter;
            var ms = s * 1000.0f;
            m_UIPerfText.text = string.Format("{0:F2}ms ({1:F2}FPS)", ms, 1.0f / s);
            m_UpdateCounter = 0;
            m_Stopwatch.Reset();
        }

        m_BackbufferTex.LoadRawTextureData(m_Backbuffer);
        m_BackbufferTex.Apply();
    }
}
