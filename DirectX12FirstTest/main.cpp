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


//use a software rasterizer (Windows Advanced Rasterization Platform - WARP) https://docs.microsoft.com/en-us/windows/win32/direct3darticles/directx-warp?redirectedfrom=MSDN
bool g_UesWarp = false;

uint32_t g_ClientWidth = 1280;
uint32_t g_ClientHight = 720;

bool g_IsInitialized = false;

HWND g_hWnd;

RECT g_WindowRect;

ComPtr<ID3D12Device2> g_Device;
ComPtr<ID3D12CommandQueue> g_CommandQueue;
ComPtr<IDXGISwapChain4> g_SwapChain;
ComPtr<ID3D12Resource> g_BackBuffers[g_NumFrames];
ComPtr<ID3D12GraphicsCommandList> g_CommandList;
ComPtr<ID3D12CommandAllocator> g_CommandAllocators[g_NumFrames];
ComPtr<ID3D12DescriptorHeap> g_RTVDescriptorHeap;

UINT g_RTVDescriptorSize;
UINT g_CurrentBackBufferIndex;


void main()
{
	return;
}
