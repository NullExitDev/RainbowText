#include <windows.h>
#include <d2d1.h>
#include <dwrite.h>
#include <dwmapi.h>
#include <wrl/client.h>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwmapi.lib")
#pragma comment(lib, "dwrite.lib")

using Microsoft::WRL::ComPtr;

enum WINDOWCOMPOSITIONATTRIB {
	WCA_ACCENT_POLICY = 19
};

struct WINDOWCOMPOSITIONATTRIBDATA {
	WINDOWCOMPOSITIONATTRIB Attrib;
	void* pvData;
	UINT cbData;
};

struct AccentPolicy {
	int AccentState, AccentFlags, GradientColor, AnimationId;
};

const wchar_t* Text = L"Dynamic Rainbow";
const wchar_t CLASS_NAME[] = L"Sample Window Class";
int width = 800, height = 520;
typedef BOOL(WINAPI* pSetWindowCompositionAttribute)(HWND, WINDOWCOMPOSITIONATTRIBDATA*);
ComPtr<ID2D1Factory> pFactory;
ComPtr<IDWriteFactory> pDWriteFactory;
ComPtr<ID2D1LinearGradientBrush> pBrush;
ComPtr<ID2D1HwndRenderTarget> pRenderTarget;
ComPtr<IDWriteTextFormat> pTextFormat;

float gradientPos = 0.0f;

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam);
HRESULT CreateResources(HWND hwnd);
void onRender(HWND hwnd);
void SetBlurWindow(HWND hwnd);

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR pCmdLine, int nCmdShow) {

	D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, pFactory.GetAddressOf());
	DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory), reinterpret_cast<IUnknown**>(pDWriteFactory.GetAddressOf()));

	WNDCLASSEX wc = {};
	wc.cbSize = sizeof(wc);
	wc.hInstance = hInstance;
	wc.hCursor = LoadCursor(NULL, IDC_ARROW);
	wc.lpszClassName = CLASS_NAME;
	wc.hbrBackground = (HBRUSH)GetStockObject(NULL_BRUSH);
	wc.lpfnWndProc = WndProc;

	RegisterClassEx(&wc);
	RECT rc;
	HWND hwndDesktop = GetDesktopWindow();
	GetClientRect(hwndDesktop, &rc);
	
	HWND hwnd = CreateWindowEx(
		0,
		CLASS_NAME,
		L"Sample Window",
		WS_POPUP,
		(rc.right - width) / 2, (rc.bottom - height) / 2, width, height,
		NULL,
		NULL,
		hInstance,
		NULL
	);

	if (hwnd) {
		if (FAILED(CreateResources(hwnd))) {
			MessageBox(NULL, L"Error load resources!", L"Error", MB_ICONERROR | MB_OK);
			return -1;
		}
		SetBlurWindow(hwnd);
		DWM_WINDOW_CORNER_PREFERENCE pref = DWMWCP_ROUND;
		DwmSetWindowAttribute(hwnd, DWMWA_WINDOW_CORNER_PREFERENCE, &pref, sizeof(pref));
		SetTimer(hwnd, 1, 20, NULL);
		ShowWindow(hwnd, nCmdShow);
	}

	MSG msg = {};
	while (GetMessage(&msg, NULL, 0, 0)) {
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	return 0;
}

LRESULT CALLBACK WndProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {

	case WM_PAINT: {
		onRender(hwnd);
		ValidateRect(hwnd, NULL);
		return 0;
	}

	case WM_NCHITTEST: {
		return HTCAPTION;
	}

	case WM_TIMER: {
		InvalidateRect(hwnd, NULL, FALSE);
		return 0;
	}

	case WM_KEYDOWN: {
		if (wParam == VK_ESCAPE) {
			SendMessage(hwnd, WM_CLOSE, 0, 0);
		}
		return 0;
	}

	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	}
	return DefWindowProc(hwnd, uMsg, wParam, lParam);
}

void SetBlurWindow(HWND hwnd) {
	HMODULE hUser = GetModuleHandle(L"user32.dll");
	if (hUser) {
		pSetWindowCompositionAttribute SetWindowCompositionAttribute = (pSetWindowCompositionAttribute)GetProcAddress(hUser, "SetWindowCompositionAttribute");
		if (SetWindowCompositionAttribute) {
			AccentPolicy policy = { 4,0,0,0 };
			WINDOWCOMPOSITIONATTRIBDATA data = { WCA_ACCENT_POLICY,&policy ,sizeof(policy) };
			SetWindowCompositionAttribute(hwnd, &data);
		}
	}
}

HRESULT CreateResources(HWND hwnd) {
	RECT rc;
	GetClientRect(hwnd, &rc);
	D2D1_RENDER_TARGET_PROPERTIES props = D2D1::RenderTargetProperties(//支持Alpha混合
		D2D1_RENDER_TARGET_TYPE_DEFAULT,
		D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED)
	);
	HRESULT hr = pFactory.Get()->CreateHwndRenderTarget(
		props,
		D2D1::HwndRenderTargetProperties(hwnd, D2D1::SizeU(
			rc.right - rc.left, rc.bottom - rc.top
		)), pRenderTarget.GetAddressOf()
	);

	if (SUCCEEDED(hr)) {
		ComPtr<ID2D1GradientStopCollection> pGradientStops;
		D2D1_GRADIENT_STOP stops[5] = {
			{ 0.00f, D2D1::ColorF(0x1589CF)},
			{ 0.25f, D2D1::ColorF(0x152BCF)},
			{ 0.50f, D2D1::ColorF(0x5915CF)},
			{ 0.75f, D2D1::ColorF(0x8515CF)},
			{ 1.00f, D2D1::ColorF(0x1589CF)}
		};
		// 创建渐变停止集合和线性渐变画刷
		pRenderTarget.Get()->CreateGradientStopCollection(
			stops, 5, D2D1_GAMMA_2_2, D2D1_EXTEND_MODE_WRAP, pGradientStops.GetAddressOf()
		);
		pRenderTarget.Get()->CreateLinearGradientBrush(
			D2D1::LinearGradientBrushProperties(
				D2D1::Point2F(0, 0), D2D1::Point2F(200, 0)
			),
			D2D1::BrushProperties(),
			pGradientStops.Get(), pBrush.GetAddressOf()
		);

	}

	if (SUCCEEDED(hr)) {
		pDWriteFactory.Get()->CreateTextFormat(
			L"Segoe UI", NULL, DWRITE_FONT_WEIGHT_REGULAR, DWRITE_FONT_STYLE_NORMAL, DWRITE_FONT_STRETCH_NORMAL,
			38.0f, L"en-US", pTextFormat.GetAddressOf()
		);

		pTextFormat.Get()->SetTextAlignment(DWRITE_TEXT_ALIGNMENT_CENTER);
		pTextFormat.Get()->SetParagraphAlignment(DWRITE_PARAGRAPH_ALIGNMENT_CENTER);
	}

	return hr;
}

void onRender(HWND hwnd) {
	//矩阵变换
	pRenderTarget.Get()->BeginDraw();
	pRenderTarget.Get()->SetTransform(D2D1::Matrix3x2F::Identity());
	RECT rc;
	GetClientRect(hwnd, &rc);
	D2D1_RECT_F rect = D2D1::RectF(0, 0, rc.right, rc.bottom);
	pRenderTarget.Get()->Clear(D2D1::ColorF(0, 0, 0, 0.8f));
	gradientPos -= 1.0f;
	if (gradientPos <= -200.0f) gradientPos += 200.0f;
	pBrush.Get()->SetTransform(D2D1::Matrix3x2F::Translation(gradientPos, gradientPos));
	pRenderTarget.Get()->DrawTextW(Text, (UINT32)wcslen(Text), pTextFormat.Get(), rect, pBrush.Get());
	pRenderTarget.Get()->EndDraw();
}
