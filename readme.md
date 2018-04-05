# Toy Path Tracer

Toy path tracer for my own learning purposes, using various approaches/techs. Somewhat based on Peter Shirley's
[Ray Tracing in One Weekend](http://in1weekend.blogspot.lt/) minibook (highly recommended!), and on Kevin Beason's
[smallpt](http://www.kevinbeason.com/smallpt/).

I decided to write blog posts about things I discover as I do this, currently:

* [Part 0: Intro](http://aras-p.info/blog/2018/03/28/Daily-Pathtracer-Part-0-Intro/)
* [Part 1: Initial C++ and walkthrough](http://aras-p.info/blog/2018/03/28/Daily-Pathtracer-Part-1-Initial-C--/)
* [Part 2: Fix stupid performance issue](http://aras-p.info/blog/2018/03/28/Daily-Pathtracer-Part-2-Fix-Stupid/)
* [Part 3: C#, Unity and Burst](http://aras-p.info/blog/2018/03/28/Daily-Pathtracer-Part-3-CSharp-Unity-Burst/)
* [Part 4: Correctness fixes and Mitsuba](http://aras-p.info/blog/2018/03/31/Daily-Pathtracer-Part-4-Fixes--Mitsuba/)
* [Part 5: simple GPU version via Metal](http://aras-p.info/blog/2018/04/03/Daily-Pathtracer-Part-5-Metal-GPU/)
* [Part 6: simple GPU version via D3D11](http://aras-p.info/blog/2018/04/04/Daily-Pathtracer-Part-6-D3D11-GPU/)

Right now: can only do spheres, no bounding volume hierachy of any sorts, a lot of stuff hardcoded.

Implementations I'm playing with:

* CPU. Testing on "PC" AMD ThreadRipper 1950X 3.4GHz (SMT disabled, 16c/16t) and "Mac" late 2013 MacBookPro 2.3GHz (4c/8t):
  * C++: PC 136, Mac 37.8 Mray/s,
  * C# (.NET Core): PC 67, Mac 17.5 Mray/s,
  * C# (Unity, Mono): PC 13.3, Mac 4.6 Mray/s,
  * C# (Unity, IL2CPP): PC 28.1, Mac 17.1 Mray/s,
  * C# (Unity, JobSystem+Burst+Mathematics targeting SSE4): PC 164, Mac 48.1 Mray/s. Note that this is super early version of Burst.
* GPU. Super simplistic direct ports, not optimized:
  * Metal compute shader. Radeon Pro 580: 1650 Mray/s, Intel Iris Pro: 191 Mray/s, GeForce GT 750M: 146 Mray/s,
  * D3D11 compute shader. Radeon Pro WX 9100: 3700 Mray/s, GeForce GTX 1080 Ti: 2780 Mray/s, Radeon HD 7700: 417 Mray/s.

A lot of stuff in the implementation is *totally* suboptimal or using the tech in a "wrong" way.

I know it's just a simple toy, ok :)

![Screenshot](/Shots/screenshot.jpg?raw=true "Screenshot")

### Building

* C++ projects: Windows (Visual Studio 2017) in `Cpp/Windows/TestCpu.sln`, Mac (Xcode 9) in `Cpp/Mac/Test.xcodeproj`.
  * Windows is DX11 Win32 app that displays result as a fullscreen CPU-updated or GPU-rendered texture.
  * Mac is a Metal app that displays result as a fullscreen CPU-updated or GPU-rendered texture.
* C# project in `Cs/TestCs.sln`. A command line app that renders some frames and dumps out final TGA screenshot at the end.
* Unity project in `Unity`. I used [2018.1 beta 12](https://beta.unity3d.com/download/ed1bf90b40e6/public_download.html) version linked to from [ECS samples](https://github.com/Unity-Technologies/EntityComponentSystemSamples).
