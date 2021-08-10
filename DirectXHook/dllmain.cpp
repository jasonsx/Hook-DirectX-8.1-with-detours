#pragma comment(lib, "legacy_stdio_definitions.lib")
#include <Windows.h>
#include "detours.h"
#include <iostream>

#include <d3d8.h>
#include <d3dx8.h>
#pragma comment(lib, "d3d8.lib")
#pragma comment(lib, "d3dx8.lib")

#define MAX_BUFF 1024

const char* windowName = "ConquerVision";
HWND hMainWindow;
// HOOK ENDSCENE
typedef HRESULT(__stdcall* f_EndScene)(IDirect3DDevice8* pDevice); // Our function prototype 
static f_EndScene oEndScene; // Original Endscene

typedef HRESULT(__stdcall* Present)(IDirect3DDevice8* pDevice); // Our function prototype 
static Present uOriginalPresent; // Original Present

typedef HRESULT(__stdcall* oReset)(LPDIRECT3DDEVICE8 pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters);
static oReset pReset;

typedef LRESULT(CALLBACK* WNDPROC)(HWND, UINT, WPARAM, LPARAM);
WNDPROC oWndProc;

HRESULT __stdcall Hooked_EndScene(IDirect3DDevice8* pDevice) // Our hooked endscene
{
	return oEndScene(pDevice); // Call original ensdcene so the game can draw
}

HRESULT __stdcall HookedPresent(IDirect3DDevice8* pDevice, RECT* pSourceRect, RECT* pDestRect, HWND hDestWindowOverride, RGNDATA* pDirtyRegion)
{
	return ((HRESULT(__stdcall*)(IDirect3DDevice8*, RECT*, RECT*, HWND, RGNDATA*))(uOriginalPresent))(pDevice, pSourceRect, pDestRect, hDestWindowOverride, pDirtyRegion);
}

int IfDeviceLost(IDirect3DDevice8* pDevice)
{
	HRESULT result;
	if (FAILED(result = pDevice->TestCooperativeLevel()))
	{
		if (result == D3DERR_DEVICELOST)
		{
			return 1;
		}
		if (result == D3DERR_DEVICENOTRESET)
		{
			return 2;
		}
	}
	return 0;
}
HRESULT __stdcall Reset(IDirect3DDevice8* pDevice, D3DPRESENT_PARAMETERS* pPresentationParameters)
{
	HRESULT hRet = NULL;
	if (IfDeviceLost(pDevice) == 1)
		hRet = pReset(pDevice, pPresentationParameters);
	return hRet;
}

LRESULT __stdcall WndProc(const HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg)
	{
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	}

	return CallWindowProc(oWndProc, hWnd, uMsg, wParam, lParam);
}

DWORD WINAPI MainThread(LPVOID param) // Our main thread
{

	hMainWindow = FindWindowA(NULL, windowName);

	oWndProc = (WNDPROC)SetWindowLongPtr(hMainWindow, GWLP_WNDPROC, (LONG_PTR)WndProc);

	IDirect3D8* pD3D = Direct3DCreate8(D3D_SDK_VERSION);

	if (!pD3D)
		return false;

	D3DDISPLAYMODE mode;
	pD3D->GetAdapterDisplayMode(D3DADAPTER_DEFAULT, &mode);
	pD3D->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, mode.Format, mode.Format, true);
	pD3D->CheckDeviceType(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, D3DFMT_X8R8G8B8, D3DFMT_X8R8G8B8, false);

	D3DCAPS8 caps;
	pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, &caps);

	if (FAILED(pD3D->GetDeviceCaps(D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		&caps)))
	{
		return false;
	}

	DWORD devBehaviorFlags = 0;
	if (caps.DevCaps & D3DDEVCAPS_HWTRANSFORMANDLIGHT)
		devBehaviorFlags |= D3DCREATE_HARDWARE_VERTEXPROCESSING;
	else
		devBehaviorFlags |= D3DCREATE_SOFTWARE_VERTEXPROCESSING;

	// If pure device and HW T&L supported
	if (caps.DevCaps & D3DDEVCAPS_PUREDEVICE &&
		devBehaviorFlags & D3DCREATE_HARDWARE_VERTEXPROCESSING)
		devBehaviorFlags |= D3DCREATE_PUREDEVICE;



	HRESULT CheckDeviceType = pD3D->CheckDeviceType(
		D3DADAPTER_DEFAULT,
		D3DDEVTYPE_HAL,
		mode.Format,
		D3DFMT_X8R8G8B8,
		TRUE    // windowed
	);

	if (FAILED(CheckDeviceType)) {
		return CheckDeviceType;
	}

	D3DPRESENT_PARAMETERS d3dpp;
	ZeroMemory(&d3dpp, sizeof(d3dpp));    // clear out the struct for use
	d3dpp.BackBufferWidth = 800;
	d3dpp.BackBufferHeight = 600;
	d3dpp.BackBufferFormat = D3DFMT_X8R8G8B8;
	d3dpp.BackBufferCount = 1;
	d3dpp.SwapEffect = D3DSWAPEFFECT_DISCARD;
	d3dpp.hDeviceWindow = hMainWindow;
	d3dpp.Windowed = true;
	d3dpp.EnableAutoDepthStencil = true;
	d3dpp.AutoDepthStencilFormat = D3DFMT_D16;
	IDirect3DDevice8* Device = NULL;
	HRESULT hr = pD3D->CreateDevice(D3DADAPTER_DEFAULT, D3DDEVTYPE_HAL, hMainWindow, devBehaviorFlags, &d3dpp, &Device);
	if (FAILED(hr))
	{
		pD3D->Release();
		return false;
	}

	// todo
	/*SetRenderState(D3DRS_AMBIENT, D3DCOLOR_ARGB(255, 255, 255, 255), Device);
	SetRenderState(D3DRS_LIGHTING, true, Device);
	SetRenderState(D3DRS_CULLMODE, D3DCULL_CW, Device);
	SetRenderState(D3DRS_ZFUNC, D3DCMP_LESSEQUAL, Device);
	SetRenderState(D3DRS_EDGEANTIALIAS, true, Device);
	SetRenderState(D3DRS_MULTISAMPLEANTIALIAS, true, Device);
	SetTextureStageState(0, D3DTSS_MINFILTER, D3DTEXF_LINEAR, Device);
	SetTextureStageState(0, D3DTSS_MAGFILTER, D3DTEXF_LINEAR, Device);
	SetTextureStageState(0, D3DTSS_MIPFILTER, D3DTEXF_LINEAR, Device);*/

	void** pVTable = *reinterpret_cast<void***>(Device);

	if (Device)
		Device->Release(), Device = nullptr;

	// if needed !
	//pReset = (oReset)(PBYTE)pVTable[16];

	//(void)DetourTransactionBegin();
	//(void)DetourUpdateThread(GetCurrentThread());
	//DetourAttach(&(PVOID&)pReset, Reset);
	//(void)DetourTransactionCommit();
	//
	//uOriginalPresent = (Present)(PBYTE)pVTable[17];

	//(void)DetourTransactionBegin();
	//(void)DetourUpdateThread(GetCurrentThread());
	//DetourAttach(&(PVOID&)uOriginalPresent, HookedPresent);
	//(void)DetourTransactionCommit();

	oEndScene = (f_EndScene)(PBYTE)pVTable[42];

	(void)DetourTransactionBegin();
	(void)DetourUpdateThread(GetCurrentThread());
	DetourAttach(&(PVOID&)oEndScene, Hooked_EndScene);
	(void)DetourTransactionCommit();

	return false;
}

BOOL APIENTRY DllMain(HINSTANCE/*HMODULE*/ hModule, DWORD  ul_reason_for_call, LPVOID lpReserved)
{
	switch (ul_reason_for_call)
	{
	case DLL_PROCESS_ATTACH:
	{
		// Gets runned when injected
		DisableThreadLibraryCalls(hModule);

		CreateThread(0, 0, MainThread, hModule, 0, 0); // Creates our thread 
		break;

	}
	case DLL_PROCESS_DETACH:
		break;
	}
	return TRUE;
}
