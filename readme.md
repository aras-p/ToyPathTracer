# Toy Path Tracer

Toy path tracer for my own learning purposes, using various approaches/techs. Somewhat based on Peter Shirley's
[Ray Tracing in One Weekend](http://in1weekend.blogspot.lt/) minibook (highly recommended!), and on Kevin Beason's
[smallpt](http://www.kevinbeason.com/smallpt/).

Right now: can only do spheres, no bounding volume hierachy of any sorts, a lot of stuff hardcoded.

Implementations I'm playing with:

* C++: ~130 Mray/s on PC, ~34.7 Mray/s on Mac,
* C++ with ISPC: ~221 Mray/s on PC, 85 Mray/s on Mac,
* C# (.NetCore): 53 Mray/s on PC,
* C# (Unity, Mono): 10.2 Mray/s on PC,
* C# (Unity, JobSystem+Burst): 140 Mray/s on PC

"PC" is AMD ThreadRipper 1950X 3.4GHz (SMT disabled, 16c/16t), "Mac" is late 2013 MacBookPro 2.3GHz (4c/8t).

A lot of stuff in the implementation migth be totally suboptimal or using the tech (e.g. ISPC) in a "wrong" way. Unity+Burst
implementation is likely both suboptimal, and using a super-super-early version of the Burst compiler too.
