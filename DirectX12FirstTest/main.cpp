#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <shellapi.h> // For CommandLineToArgvW

// The min/max macros conflict with like-named member functions.
// Only use std::min and std::max defined in <algorithm>.
#if defined(min)
#undef min
#endif

#if defined(max)
#undef max
#endif

// In order to define a function called CreateWindow, the Windows macro needs to
// be undefined.
//#if defined(CreateWindow)
//#undef CreateWindow
//#endif

// Windows Runtime Library. Needed for Microsoft::WRL::ComPtr<> template class.
#include <wrl.h>
using namespace Microsoft::WRL;

// DirectX 12 specific headers.
#include <d3d12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>

// D3D12 extension library.
#include"d3dx12.h"

// STL Headers
#include <algorithm>
#include <cassert>
#include <chrono>

// Helper functions
#include "Helpers.h"

// The number of swap chain back buffers.
const uint8_t g_NumFrames = 3;
// Use WARP adapter
bool g_UseWarp = false;

uint32_t g_ClientWidth = 1280;
uint32_t g_ClientHeight = 720;

// Set to true once the DX12 objects have been initialized.
bool g_IsInitialized = false;

// Window handle.
HWND g_hWnd;
// Window rectangle (used to toggle fullscreen state).
RECT g_WindowRect;

// DirectX 12 Objects
ComPtr<ID3D12Device> g_Device;
ComPtr<ID3D12CommandQueue> g_CommandQueue;
ComPtr<IDXGISwapChain4> g_SwapChain;
ComPtr<ID3D12Resource> g_BackBuffers[g_NumFrames];
ComPtr<ID3D12GraphicsCommandList> g_CommandList;
ComPtr<ID3D12CommandAllocator> g_CommandAllocators[g_NumFrames];
ComPtr<ID3D12DescriptorHeap> g_RTVDescriptorHeap;
UINT g_RTVDescriptorSize;
UINT g_CurrentBackBufferIndex;

// Synchronization objects
ComPtr<ID3D12Fence> g_Fence;
uint64_t g_FenceValue = 0;
uint64_t g_FrameFenceValues[g_NumFrames] = {};
HANDLE g_FenceEvent;

// By default, enable V-Sync.
// Can be toggled with the V key.
bool g_VSync = true;
bool g_TearingSupported = false;
// By default, use windowed mode.
// Can be toggled with the Alt+Enter or F11
bool g_Fullscreen = false;

// Window callback function.
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);

void ParseCommandLineArguments()
{
	int argc;
	wchar_t** argv = ::CommandLineToArgvW(::GetCommandLineW(), &argc);

	for (size_t i = 0; i < argc; ++i)
	{
		if (::wcscmp(argv[i], L"-w") == 0 || ::wcscmp(argv[i], L"--width") == 0)
		{
			g_ClientWidth = ::wcstol(argv[++i], nullptr, 10);
		}
		if (::wcscmp(argv[i], L"-h") == 0 || ::wcscmp(argv[i], L"--height") == 0)
		{
			g_ClientHeight = ::wcstol(argv[++i], nullptr, 10);
		}
		if (::wcscmp(argv[i], L"-warp") == 0 || ::wcscmp(argv[i], L"--warp") == 0)
		{
			g_UseWarp = true;
		}
	}

	// Free memory allocated by CommandLineToArgvW
	::LocalFree(argv);
}

void EnableDebugLayer()
{
#if defined(_DEBUG)
	// Always enable the debug layer before doing anything DX12 related
	// so all possible errors generated while creating DX12 objects
	// are caught by the debug layer.
	ComPtr<ID3D12Debug> debugInterface;
	ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugInterface)));
	debugInterface->EnableDebugLayer();
#endif
}

HWND WinWindow(HINSTANCE hInst, WCHAR* windowTitle)
{
	ParseCommandLineArguments();
	EnableDebugLayer();
	WNDCLASSEXW windowClass = {};

	windowClass.cbSize = sizeof(WNDCLASSEXW);
	windowClass.style = CS_HREDRAW | CS_VREDRAW;
	windowClass.lpfnWndProc = &WndProc;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hInstance = hInst;
	windowClass.hIcon = ::LoadIcon(hInst, NULL); //  MAKEINTRESOURCE(APPLICATION_ICON));
	windowClass.hCursor = ::LoadCursor(NULL, IDC_ARROW);
	windowClass.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	windowClass.lpszMenuName = NULL;
	windowClass.lpszClassName =L"Dx12";
	windowClass.hIconSm = ::LoadIcon(hInst, NULL); //  MAKEINTRESOURCE(APPLICATION_ICON));

	static HRESULT hr = RegisterClassEx(&windowClass);
	assert(SUCCEEDED(hr));


	int screenWidth = ::GetSystemMetrics(SM_CXSCREEN);
	int screenHeight = ::GetSystemMetrics(SM_CYSCREEN);

	RECT windowRect = { 0, 0, static_cast<LONG>(g_ClientWidth), static_cast<LONG>(g_ClientHeight) };
	::AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

	int windowWidth = windowRect.right - windowRect.left;
	int windowHeight = windowRect.bottom - windowRect.top;

	// Center the window within the screen. Clamp to 0, 0 for the top-left corner.
	int windowX = std::max<int>(0, (screenWidth - windowWidth) / 2);
	int windowY = std::max<int>(0, (screenHeight - windowHeight) / 2);

	HWND hWnd = CreateWindow(
		//NULL,
		windowClass.lpszClassName,
		windowTitle,
		WS_OVERLAPPEDWINDOW,
		windowX,
		windowY,
		windowWidth,
		windowHeight,
		NULL,
		NULL,
		hInst,
		nullptr
	);

	assert(hWnd && "Failed to create window");

	GetWindowRect(hWnd, &g_WindowRect);

	return hWnd;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT message, WPARAM wParam, LPARAM lParam)
{

	switch (message)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		break;
	}

	return DefWindowProc(hwnd, message, wParam, lParam);
}

ComPtr<IDXGIAdapter4> GetAdapter(bool useWarp)
{

	ComPtr<IDXGIFactory4> factory;
	UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
	dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));
	ComPtr<IDXGIAdapter4> dxgiAdapter4;
	if (useWarp) {
		ComPtr<IDXGIAdapter> warpAdapter;
		ThrowIfFailed(factory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));
		ThrowIfFailed(warpAdapter.As(&dxgiAdapter4));
	}
	else{
		ComPtr<IDXGIAdapter1> hardwareAdapter;
		for (int i = 0; factory->EnumAdapters1(i, &hardwareAdapter) != DXGI_ERROR_NOT_FOUND; i++) {
			DXGI_ADAPTER_DESC1 adpterDesc;
			hardwareAdapter->GetDesc1(&adpterDesc);
			if (adpterDesc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
			{
				continue;
			}
			if (SUCCEEDED(D3D12CreateDevice(hardwareAdapter.Get(),D3D_FEATURE_LEVEL_11_0,__uuidof(ID3D12Device),nullptr )))
			{
				break;
			}
		}
		ThrowIfFailed(hardwareAdapter.As(&dxgiAdapter4));
		
	}
	return dxgiAdapter4;
}

ComPtr<ID3D12Device> CreateDevice(ComPtr<IDXGIAdapter4> adapter)
{
	ComPtr<ID3D12Device> m_device;

	ThrowIfFailed(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_device)));
	return m_device;
}

ComPtr<ID3D12CommandQueue> CreateCommandQueue(ComPtr<ID3D12Device> m_device,D3D12_COMMAND_LIST_TYPE type)
{
	ComPtr<ID3D12CommandQueue> d3d12CommandQueue;
	D3D12_COMMAND_QUEUE_DESC desc = {};
	desc.Type = type;
	desc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(m_device->CreateCommandQueue(&desc,IID_PPV_ARGS(&d3d12CommandQueue)));
	return d3d12CommandQueue;
}

ComPtr<IDXGISwapChain4> CreateSwapChain(HWND hWnd,ComPtr<ID3D12Device> device,ComPtr<ID3D12CommandQueue> m_commandQueue,uint32_t width,uint32_t height,uint32_t bufferCount)
{
	//D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msQualityLevels;
	/*msQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	msQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;
	msQualityLevels.NumQualityLevels = 0;
	ThrowIfFailed(device->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msQualityLevels, sizeof(msQualityLevels)));*/
	
	//assert(msQualityLevels.NumQualityLevels > 0 && "Unexpected MSAA quality level");

	ComPtr<IDXGISwapChain1> swapChain;
	DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};
	swapChainDesc.BufferCount = bufferCount;
	swapChainDesc.Width = width;
	swapChainDesc.Height = height;
	swapChainDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
	swapChainDesc.SampleDesc.Count = 1;

	ComPtr<IDXGIFactory4> factory;
	UINT dxgiFactoryFlags = 0;
#if defined(_DEBUG)
	dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
#endif
	ThrowIfFailed(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&factory)));

	ThrowIfFailed(factory->CreateSwapChainForHwnd(
		m_commandQueue.Get(),        // Swap chain needs the queue so that it can force a flush on it.
		hWnd,
		&swapChainDesc,
		nullptr,
		nullptr,
		&swapChain
	));

	ThrowIfFailed(factory->MakeWindowAssociation(hWnd, DXGI_MWA_NO_ALT_ENTER));

	ComPtr<IDXGISwapChain4> m_SwapChain4;
	ThrowIfFailed(swapChain.As(&m_SwapChain4));
	return m_SwapChain4;
}

ComPtr<ID3D12DescriptorHeap> CreateDescriptorHeap(ComPtr<ID3D12Device>m_device,D3D12_DESCRIPTOR_HEAP_TYPE type,uint32_t bufferCount)
{
	ComPtr<ID3D12DescriptorHeap> descriptorHeap;
	D3D12_DESCRIPTOR_HEAP_DESC desc = {};
	desc.NumDescriptors = bufferCount;
	desc.Type = type;
	desc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(m_device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&descriptorHeap)));
	return descriptorHeap;

}

int CALLBACK wWinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, PWSTR lpCmdLine, int nCmdShow)
{
	// Windows 10 Creators update adds Per Monitor V2 DPI awareness context.
	// Using this awareness context allows the client area of the window 
	// to achieve 100% scaling while still allowing non-client window content to 
	// be rendered in a DPI sensitive fashion.

	SetThreadDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);

	g_hWnd = WinWindow(hInstance, L"DirectX 12 First Test");

	//g_TearingSupported = CheckTearingSupport();

	ComPtr<IDXGIAdapter4> dxgiAdapter4 = GetAdapter(g_UseWarp);

	g_Device = CreateDevice(dxgiAdapter4);

	g_CommandQueue = CreateCommandQueue(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);

	g_SwapChain = CreateSwapChain(g_hWnd, g_Device,g_CommandQueue,g_ClientWidth, g_ClientHeight, g_NumFrames);

	g_CurrentBackBufferIndex = g_SwapChain->GetCurrentBackBufferIndex();

	g_RTVDescriptorHeap = CreateDescriptorHeap(g_Device, D3D12_DESCRIPTOR_HEAP_TYPE_RTV, g_NumFrames);
	g_RTVDescriptorSize = g_Device->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	//UpdateRenderTargetViews(g_Device, g_SwapChain, g_RTVDescriptorHeap);

	for (int i = 0; i < g_NumFrames; ++i)
	{
		//g_CommandAllocators[i] = CreateCommandAllocator(g_Device, D3D12_COMMAND_LIST_TYPE_DIRECT);
	}
	//g_CommandList = CreateCommandList(g_Device,
		//g_CommandAllocators[g_CurrentBackBufferIndex], D3D12_COMMAND_LIST_TYPE_DIRECT);

	//g_Fence = CreateFence(g_Device);
	//g_FenceEvent = CreateEventHandle();

	//g_IsInitialized = true;

	ShowWindow(g_hWnd, SW_SHOW);

	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (::PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
		{
			::TranslateMessage(&msg);
			::DispatchMessage(&msg);
		}
	}

	// Make sure the command queue has finished all commands before closing.
	//Flush(g_CommandQueue, g_Fence, g_FenceValue, g_FenceEvent);

	//::CloseHandle(g_FenceEvent);

	return 0;
}