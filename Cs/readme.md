# Building .NET/C#

## CoreCLR Runtime (JIT)

Using the .NET CoreCLR runtime (JIT based).

> **Requirements**
> Make sure that you have installed a latest version of VS2017 that includes the .NET Core 2.0+ SDK (or install it from https://www.microsoft.com/net/download/windows) 

```
dotnet run -c release
```

## CoreRT Runtime (AOT)

using the .NET CoreRT runtime (AOT based).
Same requirements than CoreCLR.

```
dotnet publish -r win-x64 -c release
bin\Release\netcoreapp2.1\win-x64\publish\TestCs.exe
```
