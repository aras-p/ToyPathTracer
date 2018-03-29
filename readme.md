# Toy Path Tracer

Toy path tracer for my own learning purposes, using various approaches/techs. Somewhat based on Peter Shirley's
[Ray Tracing in One Weekend](http://in1weekend.blogspot.lt/) minibook (highly recommended!), and on Kevin Beason's
[smallpt](http://www.kevinbeason.com/smallpt/).

I decided to write blog posts about things I discover as I do this, currently:

* [Part 0: Intro](http://aras-p.info/blog/2018/03/28/Daily-Pathtracer-Part-0-Intro/)
* [Part 1: Initial C++ and walkthrough](http://aras-p.info/blog/2018/03/28/Daily-Pathtracer-Part-1-Initial-C--/)

Right now: can only do spheres, no bounding volume hierachy of any sorts, a lot of stuff hardcoded.

Implementations I'm playing with:

* C++: 130 Mray/s on PC, 34.7 Mray/s on Mac,
* C++ with ISPC:
  * AVX2: 246 Mray/s on PC, 90 Mray/s on Mac,
  * SSE4: 200 Mray/s on PC, 56.6 Mray/s on Mac,
* C# (.NetCore): 53 Mray/s on PC,
* C# (Unity, Mono): 10.8 Mray/s on PC,
* C# (Unity, JobSystem+Burst+Mathematics targeting SSE4): 162 Mray/s on PC.

"PC" is AMD ThreadRipper 1950X 3.4GHz (SMT disabled, 16c/16t), "Mac" is late 2013 MacBookPro 2.3GHz (4c/8t).

A lot of stuff in the implementation migth be totally suboptimal or using the tech (e.g. ISPC) in a "wrong" way. Unity+Burst
implementation is likely both suboptimal, and using a super-super-early version of the Burst compiler too.

I know it's just a simple toy, ok :)

![Screenshot](/Shots/screenshot.jpg?raw=true "Screenshot")

### Building

* C++ projects: Windows (Visual Studio 2017) in `Cpp/Windows/TestCpu.sln`, Mac (Xcode 9) in `Cpp/Mac/Test.xcodeproj`.
  * ISPC branches need [ISPC binaries] to be in `Cpp/ispc.exe` (Win) and `Cpp/ispc` (Mac). I used version 1.9.2.
  * Windows is a simple Win32 app that displays image via GDI (that part is not terribly fast).
  * Mac is a Metal app that displays result as a fullscreen texture.
* C# project in `Cs/TestCs.sln`. A command line app that renders some frames and dumps out final TGA screenshot at the end.
* Unity project in `Unity`. I used [2018.1 beta 12](https://beta.unity3d.com/download/ed1bf90b40e6/public_download.html) version linked to from [ECS samples](https://github.com/Unity-Technologies/EntityComponentSystemSamples).
