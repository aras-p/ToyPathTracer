#pragma once
#include <stdint.h>

void InitializeTest();
void ShutdownTest();

void UpdateTest(float time, int frameCount, int screenWidth, int screenHeight);
void DrawTest(float time, int frameCount, int screenWidth, int screenHeight, float* backbuffer, int& outRayCount);

void GetObjectCount(int& outCount, int& outObjectSize, int& outMaterialSize, int& outCamSize);
void GetSceneDesc(void* outObjects, void* outMaterials, void* outCam, void* outEmissives, int* outEmissiveCount);
