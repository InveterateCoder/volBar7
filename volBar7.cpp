#include<Windows.h>
#include<math.h>
#include<Audioclient.h>
#include<mmdeviceapi.h>
#include<endpointvolume.h>
#pragma comment(lib,"Comctl32")
#include<CommCtrl.h>
#define EXIT_ON_ERROR(hres) \
				if(FAILED(hres)){MessageBox(hWnd,L"wasapi function failed;",\
					L"ERROR",MB_OK|MB_ICONERROR);PostMessage(hWnd,WM_CLOSE,0,0);}
#define SAFE_RELEASE(punk) \
				if(punk != NULL) { punk->Release(); punk = NULL;}
class CAudioEndpointVolumeCallback : public IAudioEndpointVolumeCallback
{
	LONG _cRef;
	HWND hWnd;
public:
	CAudioEndpointVolumeCallback(HWND hWndPass) :
		_cRef(1), hWnd(hWndPass)
	{
	}

	~CAudioEndpointVolumeCallback()
	{
	}
	// IUnknown methods -- AddRef, Release, and QueryInterface
	ULONG STDMETHODCALLTYPE AddRef()
	{
		return InterlockedIncrement(&_cRef);
	}
	ULONG STDMETHODCALLTYPE Release()
	{
		ULONG ulRef = InterlockedDecrement(&_cRef);
		if (0 == ulRef)
			delete this;
		return ulRef;

	}
	HRESULT STDMETHODCALLTYPE QueryInterface(REFIID riid, VOID **ppvInterface)
	{
		if (IID_IUnknown == riid)
		{
			AddRef();
			*ppvInterface = (IUnknown*)this;
		}
		else if (__uuidof(IAudioEndpointVolumeCallback) == riid)
		{
			AddRef();
			*ppvInterface = (IAudioEndpointVolumeCallback*)this;
		}
		else
		{
			*ppvInterface = NULL;
			return E_NOINTERFACE;
		}
		return S_OK;
	}

	// Callback method for endpoint-volume-change notifications.

	HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA pNotify)
	{
		if (!pNotify)
		{
			return E_INVALIDARG;
		}
		PostMessage(hWnd, WM_USER, (WPARAM)round(pNotify->fMasterVolume * 100), (LPARAM)pNotify->bMuted);
		return S_OK;
	}
};
HRESULT hr;
LRESULT CALLBACK WndProc(HWND, UINT, WPARAM, LPARAM);
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance,
	LPSTR lpCmdLine, int nCmdShow)
{
	hr = CoInitializeEx(NULL, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE);
	if (FAILED(hr))
	{
		MessageBox(NULL, L"failed to initialize com.", L"ERROR", MB_OK | MB_ICONERROR);
		return 0;
	}
	INITCOMMONCONTROLSEX cc;
	cc.dwICC = ICC_PROGRESS_CLASS;
	InitCommonControlsEx(&cc);
	cc.dwSize = sizeof(cc);
	HWND hWnd;
	MSG msg;
	WNDCLASS wc;
	wc.cbClsExtra = 0;
	wc.cbWndExtra = 0;
	wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.hIcon = LoadIcon(NULL, IDI_APPLICATION);
	wc.hInstance = hInstance;
	wc.lpfnWndProc = WndProc;
	wc.lpszClassName = L"VolumBarClassyClass";
	wc.lpszMenuName = NULL;
	wc.style = 0;
	if (!RegisterClass(&wc))
		return 0;
	hWnd = CreateWindowEx(WS_EX_LAYERED | WS_EX_TOPMOST | WS_EX_TOOLWINDOW, L"VolumBarClassyClass", L"", WS_POPUP, 130,
		130, 80, 210, GetDesktopWindow(), NULL, hInstance, NULL);
	if (!hWnd)
		return 0;
	ShowWindow(hWnd, SW_HIDE);
	while (GetMessage(&msg, NULL, 0, 0))
	{
		DispatchMessage(&msg);
	}
	return msg.wParam;
}
LRESULT CALLBACK WndProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam)
{
	IMMDeviceEnumerator *pEnumerator;
	IMMDevice *pDevice;
	static IAudioEndpointVolume *pVolume;
	static CAudioEndpointVolumeCallback notif(hWnd);
	HDC hdc;
	PAINTSTRUCT ps;
	static UINT vol;
	wchar_t buf[5];
	static UINT_PTR timer;
	static HWND hPB;
	static HBRUSH hbr;
	static RECT rect;
	static bool mute;
	switch (uMsg)
	{
	case WM_CREATE:
		SetLayeredWindowAttributes(hWnd, RGB(0, 0, 0), 0, LWA_COLORKEY);
		hr = CoCreateInstance(__uuidof(MMDeviceEnumerator) , NULL, CLSCTX_ALL,
			__uuidof(IMMDeviceEnumerator), (void**)&pEnumerator);
		EXIT_ON_ERROR(hr);
		hr = pEnumerator->GetDefaultAudioEndpoint(eRender, eConsole, &pDevice);
		EXIT_ON_ERROR(hr);
		hr = pDevice->Activate(__uuidof(IAudioEndpointVolume), CLSCTX_ALL,
			NULL, (void**)&pVolume);
		EXIT_ON_ERROR(hr);
		hr = pVolume->RegisterControlChangeNotify(&notif);
		EXIT_ON_ERROR(hr);
		SAFE_RELEASE(pDevice);
		SAFE_RELEASE(pEnumerator);
		hPB = CreateWindowEx(0, PROGRESS_CLASS, NULL, WS_CHILD | WS_VISIBLE | PBS_VERTICAL | PBS_SMOOTH,
			20, 0, 40, 150, hWnd, NULL, GetModuleHandle(NULL), NULL);
		hbr = CreateSolidBrush(GetSysColor(COLOR_3DFACE));
		SetRect(&rect, 20, 160, 60, 190);
		break;
	case WM_PAINT:
		hdc = BeginPaint(hWnd, &ps);
		SelectObject(hdc, hbr);
		SetBkMode(hdc, TRANSPARENT);
		SetTextColor(hdc, RGB(1, 1, 1));
		if (mute)
		{
			wsprintf(buf, L"mute");
			mute = 0;
		}
		else
		{
			wsprintf(buf, L"%u", vol);
			SendMessage(hPB, PBM_SETPOS, vol, 0);
		}
		Rectangle(hdc, rect.left, rect.top, rect.right, rect.bottom);
		MoveToEx(hdc, 20, 150, NULL);
		LineTo(hdc, 60, 150);
		DrawText(hdc, buf, -1, &rect, DT_CENTER | DT_VCENTER | DT_SINGLELINE);
		EndPaint(hWnd, &ps);
		break;
	case WM_TIMER:
		ShowWindow(hWnd, SW_HIDE);
		KillTimer(hWnd, timer);
		timer = 0;
		break;
	case WM_USER:
		ShowWindow(hWnd, SW_SHOW);
		if (lParam)
			mute = true;
		vol = wParam;
		InvalidateRect(hWnd, &rect, FALSE);
		if (timer != 0)
		{
			KillTimer(hWnd, timer);
			timer = 0;
		}
		timer = SetTimer(hWnd, timer, 1300, NULL);
		break;
	case WM_CLOSE:
		if (pVolume)
		{
			pVolume->UnregisterControlChangeNotify(&notif);
			pVolume->Release();
		}
		CoUninitialize();
		DeleteObject(hbr);
		DestroyWindow(hWnd);
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		break;
	default:
		return DefWindowProc(hWnd, uMsg, wParam, lParam);
	}
	return 0;
}