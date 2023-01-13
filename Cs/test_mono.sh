xbuild /p:Configuration=Release TestCsNet46.csproj
mono bin/Release/TestCsNet46.exe
mono --llvm bin/Release/TestCsNet46.exe

