#pragma once
#include <stdint.h>

void InitializeTest(int screenWidth, int screenHeight);
void ShutdownTest();

void UpdateTest(float time, int frameCount);
void DrawTest(float time, int frameCount, float* backbuffer, int& outRayCount);

void GetObjectCount(int& outCount, int& outObjectSize, int& outMaterialSize, int& outCamSize);
void GetSceneDesc(void* outObjects, void* outMaterials, void* outCam, void* outEmissives, int* outEmissiveCount);
