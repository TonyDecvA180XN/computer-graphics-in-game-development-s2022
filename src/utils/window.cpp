#include "window.h"

#include <windowsx.h>

using namespace cg::utils;
using namespace std::string_literals;

HWND window::hwnd = nullptr;

int cg::utils::window::run(cg::renderer::renderer* renderer, HINSTANCE hinstance, int ncmdshow)
{
	// Register the window class.
	LPCWSTR windowClassName = L"DirectX Sample Window Class";
	LPCWSTR windowName = L"DirectX Sample Window";

	constexpr DWORD windowStyle = WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX | WS_MAXIMIZEBOX;

	WNDCLASS windowClass{};

	windowClass.lpfnWndProc = window_proc;
	windowClass.hInstance = hinstance;
	windowClass.lpszClassName = windowClassName;
	windowClass.style = 0;
	windowClass.cbClsExtra = 0;
	windowClass.cbWndExtra = 0;
	windowClass.hIcon = LoadIcon(nullptr, IDI_APPLICATION);
	windowClass.hCursor = LoadCursor(nullptr, IDC_ARROW);
	windowClass.hbrBackground = static_cast<HBRUSH>(GetStockObject(GRAY_BRUSH));
	windowClass.lpszMenuName = nullptr;

	if (RegisterClass(&windowClass) == 0)
	{
		THROW_ERROR("Failed to register a window class")
	}

	RECT windowBox;
	windowBox.left = 0;
	windowBox.top = 0;
	windowBox.right = static_cast<LONG>(renderer->get_width());
	windowBox.bottom = static_cast<LONG>(renderer->get_height());

	if (!AdjustWindowRect(&windowBox, windowStyle, false))
	{
		THROW_ERROR("Failed to adjust window rectangle")
	}

	// Create the window.
	HWND hWindow = CreateWindowEx(
		0, // Optional window styles.
		windowClassName, // Window class
		windowName, // Window text
		windowStyle, // Window style
		CW_USEDEFAULT, // Position x
		CW_USEDEFAULT, // Position y
		windowBox.right - windowBox.left, // Window width
		windowBox.bottom - windowBox.top, // Window height
		nullptr, // Parent window
		nullptr, // Menu
		hinstance, // Instance handle
		renderer // Additional application data
	);

	if (hWindow == nullptr)
	{
		THROW_ERROR("Failed to create a window")
	}

	ShowWindow(hWindow, SW_MAXIMIZE);
	hwnd = hWindow;

	// Initialize the sample. OnInit is defined in each child-implementation of DXSample.
	renderer->init();

	// Main sample loop.
	MSG msg = {};
	while (msg.message != WM_QUIT)
	{
		if (PeekMessage(&msg, nullptr, 0, 0, PM_REMOVE))
		{
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
		else
		{
			//renderer->update();
			//renderer->render();
		}
	}

	renderer->destroy();
	// Return this part of the WM_QUIT message to Windows.
	return static_cast<int>(msg.wParam);
}

LRESULT cg::utils::window::window_proc(HWND hwnd, UINT message, WPARAM wparam, LPARAM lparam)
{
	cg::renderer::renderer* renderer = reinterpret_cast<cg::renderer::renderer*>(
		GetWindowLongPtr(hwnd, GWLP_USERDATA));

	switch (message)
	{
		case WM_CREATE:
		{
			// Save the Renderer* passed in to CreateWindow.
			LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lparam);
			SetWindowLongPtr(
				hwnd, GWLP_USERDATA,
				reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
			return 0;
		}

		case WM_PAINT:
		{
			if (renderer)
			{
				renderer->update();
				renderer->render();
			}
			return 0;
		}

		case WM_KEYDOWN:
		{
			if (renderer)
			{
				switch (static_cast<UINT8>(wparam))
				{
					case 87: // w
						renderer->move_forward(10.f);
						break;
					case 83: // s
						renderer->move_backward(10.f);
						break;
					case 68: // d
						renderer->move_right(10.f);
						break;
					case 65: // a
						renderer->move_left(10.f);
						break;
				}
			}
			return 0;
		}

		case WM_MOUSEMOVE:
		{
			//if (renderer)
			//{
			//	short x_pos = GET_X_LPARAM(lparam);
			//	short y_pos = GET_Y_LPARAM(lparam);

			//	// TODO fixme
			//	renderer->move_yaw((2.f * static_cast<float>(x_pos) / renderer->get_width() - 1.f) * 60.f);
			//	renderer->move_pitch((-2.f * static_cast<float>(y_pos) / renderer->get_height() + 1.f) * 60.f);
			//}
			return 0;
		}

		case WM_DESTROY:
		{
			PostQuitMessage(0);
			return 0;
		}
	}

	// Handle any messages the switch statement didn't.
	return DefWindowProc(hwnd, message, wparam, lparam);
}
