emcc -O3 -std=c++11 -s WASM=1 -s ALLOW_MEMORY_GROWTH=1 -s EXTRA_EXPORTED_RUNTIME_METHODS="['cwrap']" -o toypathtracer.js main.cpp ../Source/Maths.cpp ../Source/Test.cpp
