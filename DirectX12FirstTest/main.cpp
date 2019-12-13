//ref:https://www.3dgep.com/learning-directx-12-1/

#define WIN32_LEAN_AND_MEAN
#include<Windows.h>
#include<shellapi.h>

#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

#if defined(CreateWindow)
#undef CreateWindow
#endif

#include<wrl.h>

#include <d3d12.h>
#include<dxgi1_6.h>
#include<d3dcompiler.h>
#include<DirectXMath.h>

#include "d3dx12.h"

#include<algorithm>
#include<cassert>
#include <chrono>

#include"Helpers.h"

using namespace Microsoft::WRL;

const uint8_t g_NumFrames = 3;

bool g_UesWarp = false;

uint32_t g_ClientWidth = 1280;
uint32_t g_ClientHight = 720;

bool g_IsInitialized = false;

void main()
{
	return;
}
