/*
Copyright(c) 2014, Brandon Jones. All rights reserved.

Redistribution and use in source and binary forms, with or without modification,
are permitted provided that the following conditions are met :

*Redistributions of source code must retain the above copyright notice, this
list of conditions and the following disclaimer.
* Redistributions in binary form must reproduce the above copyright notice,
this list of conditions and the following disclaimer in the documentation
and / or other materials provided with the distribution.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND
ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
DISCLAIMED.IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
(INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON
ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
(INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/

//-------------------------------------------------------------------------------------
// This app is intended to discover the simplest possible configuration for getting
// Direct-to-Rift (abbreviated here as D2R) mode working on compatible hardware.
// It is based on the OculusRoomTiny example with various bits and pieces stripped out
// until we reached the (apparent) minimum code.

// For simplicity in this app we will assume that you are always in D2R mode, so 
// there's no extended mode path.

#include "OVR_CAPI.h"
#include "Kernel/OVR_Math.h"
#include <d3d11.h>

// OVR
ovrHmd             HMD;

// Win32
HWND               hWnd = NULL;
HINSTANCE          hInstance;

// D3D11
IDXGIFactory*			DXGIFactory = NULL;
ID3D11Device*           Device = NULL;
ID3D11DeviceContext*    Context = NULL;
IDXGISwapChain*         SwapChain = NULL;
IDXGIAdapter*           Adapter = NULL;
ID3D11Texture2D*        BackBuffer = NULL;
ID3D11RenderTargetView* BackBufferRT = NULL;

// User input
bool               Quit = 0;

LRESULT CALLBACK systemWindowProc(HWND arg_hwnd, UINT msg, WPARAM wp, LPARAM lp)
{
	switch (msg)
	{
	case(WM_NCCREATE) : hWnd = arg_hwnd; break;

	case WM_KEYDOWN: if ((unsigned)wp == VK_ESCAPE) Quit = true; break;

	case WM_QUIT:
	case WM_CLOSE: Quit = true;
		return 0;
	}

	return DefWindowProc(hWnd, msg, wp, lp);
}

HWND InitWindow(OVR::Sizei resolution)
{
	// Window
	WNDCLASS wc;
	memset(&wc, 0, sizeof(wc));
	wc.lpszClassName = L"BarebonesDirectWindow";
	wc.style = CS_OWNDC; // Surprisingly not necessary for D2R, but you'll want to set it anyway. 
	wc.lpfnWndProc = systemWindowProc;
	wc.cbWndExtra = NULL;
	RegisterClass(&wc);

	// WS_OVERLAPPEDWINDOW is set here for convenience when running the app.
	// It's not necessary for D2R to work.
	DWORD wsStyle = WS_OVERLAPPEDWINDOW | WS_VISIBLE;
	RECT winSize = { 0, 0, resolution.w, resolution.h };
	AdjustWindowRect(&winSize, wsStyle, false);
	hWnd = CreateWindowA("BarebonesDirectWindow", "Barebones Direct-to-Rift",
		wsStyle,
		0, 0,
		winSize.right - winSize.left, winSize.bottom - winSize.top,
		NULL, NULL, hInstance, NULL);

	return(hWnd);
}

bool InitGraphics(OVR::Sizei resolution)
{
	// There doesn't seem to be anything that needs to be done "special"
	// during D3D11 init. This is all pretty basic, normal stuff.

	DXGIFactory = NULL;
	HRESULT hr = CreateDXGIFactory(__uuidof(IDXGIFactory), (void**)&DXGIFactory);
	if (FAILED(hr))
		return false;

	Adapter = NULL;
	DXGIFactory->EnumAdapters(0, &Adapter);

	Device = NULL;
	Context = NULL;
	hr = D3D11CreateDevice(Adapter, D3D_DRIVER_TYPE_UNKNOWN,
		NULL, 0, NULL, 0, D3D11_SDK_VERSION, &Device, NULL, &Context);
	if (FAILED(hr))
		return false;

	DXGI_SWAP_CHAIN_DESC scDesc;
	memset(&scDesc, 0, sizeof(scDesc));
	scDesc.BufferCount = 2;
	scDesc.BufferDesc.Width = resolution.w; // Resolution does not have to match the HMD.
	scDesc.BufferDesc.Height = resolution.h;
	scDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM; // Haven't tried many other modes. Doubt it has an effect of D2R.
	scDesc.BufferDesc.RefreshRate.Numerator = 0;
	scDesc.BufferDesc.RefreshRate.Denominator = 1;
	scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
	scDesc.OutputWindow = hWnd;
	scDesc.SampleDesc.Count = 1;
	scDesc.SampleDesc.Quality = 0;
	scDesc.Windowed = TRUE;
	scDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
	scDesc.SwapEffect = DXGI_SWAP_EFFECT_SEQUENTIAL; // DXGI_SWAP_EFFECT_DISCARD and DXGI_SWAP_EFFECT_FLIP_SEQUENTIAL also works fine.

	IDXGISwapChain* newSC;
	if (FAILED(DXGIFactory->CreateSwapChain(Device, &scDesc, &newSC)))
		return false;
	SwapChain = newSC;

	BackBuffer = NULL;
	BackBufferRT = NULL;
	hr = SwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (void**)&BackBuffer);
	if (FAILED(hr))
		return false;

	hr = Device->CreateRenderTargetView(BackBuffer, NULL, &BackBufferRT);
	if (FAILED(hr))
		return false;

	return true;
}

int Init()
{
	// Create window.
	// The window size doesn't need to have any relation to the size of the backbuffer.
	// They don't even have to be the same aspect ratio!
	OVR::Sizei windowResolution = OVR::Sizei(1280, 720);
	HWND window = InitWindow(windowResolution);
	if (!window) return 1;

	// Must do this before creating the D3D11 device!
	// Alternatively you can call ovr_Initialize before creating the graphics device,
	// but that's a more heavyweight call. Which you choose to use depends on your app's
	// needs.
	ovr_InitializeRenderingShim();
	
	// ovrHmd_Create and ovrHmd_AttachToWindow work here as well, provided you call
	// ovr_Initialize first of course! See below.
	
	// Create D3D11 device.
	OVR::Sizei backBufferResolution = HMD ? HMD->Resolution : OVR::Sizei(1920, 1080);
	bool graphicsInitialized = InitGraphics(backBufferResolution);
	if (!graphicsInitialized) return 1;

	// You can call this after graphics device creation IF you called ovr_InitializeRenderingShim
	// earlier. Otherwise this call must come before the graphics device creation.
	ovr_Initialize();

	// HMD creation can happen before or after the window and graphics device are created.
	HMD = ovrHmd_Create(0);
	if (!HMD) return 1;

	// ovrHmd_AttachToWindow can happen before or after initializing the graphics device.
	ovrHmd_AttachToWindow(HMD, window, NULL, NULL);

	// Weirdly, although D2R mode requires you to call ovrHmd_GetEyePose or
	// ovrHmd_GetTrackingState you DON'T have to call ovrHmd_ConfigureTracking.

	// You could call ovrHmd_GetTrackingState here and it would trigger D2R display. See below.

    return (0);
}

void ProcessAndRender()
{
	// And THIS, dear reader, is the apparent magic sauce that actually makes D2R work.
	// You have to call either ovrHmd_GetTrackingState or ovrHmd_GetEyePose to get anything
	// to show up on the Rift. Interestingly it doesn't need to be called in the render loop,
	// you can call it once after ovrHmd_AttachToWindow and never again, it still initializes
	// the display correctly. This is really weird, because basically EVERY Rift application
	// will call this during it's game loop, so you would think that most apps should "just work".
	// It's possible that it needs to be called (for display purposes) on the main window thread
	// or the same thread that created the graphics device.
	ovrHmd_GetTrackingState(HMD, 0.0);

	// Simple clear to green, just to show that it's working.
	const FLOAT clearColor[] = { 0.0f, 1.0f, 0.0f, 1.0f };
	Context->OMSetRenderTargets(1, &BackBufferRT, NULL);
	Context->ClearRenderTargetView(BackBufferRT, clearColor);
	SwapChain->Present(1, 0);
}

void Release(void)
{
    // Boring cleanup, nothing interesting to see here.
	ovrHmd_Destroy(HMD);

	if (BackBufferRT) BackBufferRT->Release();
	if (BackBuffer) BackBuffer->Release();
	if (SwapChain) SwapChain->Release();
	if (Context) Context->Release();
	if (Device) Device->Release();
	if (Adapter) Adapter->Release();
	if (DXGIFactory) DXGIFactory->Release();

	if (hWnd)
	{
		::DestroyWindow(hWnd);
		UnregisterClass(L"BarebonesDirectWindow", hInstance);
	}

    ovr_Shutdown(); 
}

int WINAPI WinMain(HINSTANCE hinst, HINSTANCE, LPSTR, int)
{
	hInstance = hinst;

	if (!Init())
	{
		// Processes messages and calls OnIdle() to do rendering.
		while (!Quit)
		{
			MSG msg;
			if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessage(&msg);
			}
			else
			{
				ProcessAndRender();

				// Keep sleeping when we're minimized.
				if (IsIconic(hWnd)) Sleep(10);
			}
		}
	}
	Release();
	OVR_ASSERT(!_CrtDumpMemoryLeaks());
	return (0);
}

