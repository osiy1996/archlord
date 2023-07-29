#include "core/os.h"
#include "core/malloc.h"
#include "core/internal.h"
#include "core/string.h"
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>

struct mutex_t {
	CRITICAL_SECTION cs;
};

struct dll_handle {
	HMODULE os_handle;
};

struct thread_handle {
	HANDLE os_handle;
	thread_routine_t routine;
	void * param;
	uint64_t id;
};

struct timer {
	LARGE_INTEGER freq;
	LARGE_INTEGER start;
};

struct window {
	char title[128];
	uint32_t width;
	uint32_t height;
	HWND hwnd;
	boolean is_open;
};

static boolean * g_shutdown_signal;
static mutex_t g_shutdown_mutex;

mutex_t create_mutex()
{
	struct mutex_t * m = alloc(sizeof(struct mutex_t));
	InitializeCriticalSection(&m->cs);
	return m;
}

void lock_mutex(mutex_t m)
{
	EnterCriticalSection(&((struct mutex_t *)m)->cs);
}

void unlock_mutex(mutex_t m)
{
	LeaveCriticalSection(&((struct mutex_t *)m)->cs);
}

void destroy_mutex(mutex_t m)
{
	DeleteCriticalSection(&((struct mutex_t *)m)->cs);
	dealloc(m);
}

dll_handle load_library(const char * path)
{
	HMODULE oh;
	struct dll_handle * h;
	oh = LoadLibraryA(path);
	if (!oh)
		return NULL;
	h = alloc(sizeof(*h));
	h->os_handle = oh;
	return h;
}

boolean free_library(dll_handle handle)
{
	if (!FreeLibrary(((struct dll_handle *)handle)->os_handle))
		return FALSE;
	dealloc(handle);
	return TRUE;
}

void * get_sym_addr(dll_handle handle, const char * sym)
{
	return GetProcAddress(
		((struct dll_handle *)handle)->os_handle, sym);
}

static DWORD WINAPI win_signal_handler(DWORD type)
{
	lock_mutex(g_shutdown_mutex);
	*g_shutdown_signal = TRUE;
	unlock_mutex(g_shutdown_mutex);
	Sleep(INFINITE);
	return TRUE;
}

boolean set_signals_handlers(struct core_module * cc)
{
	g_shutdown_signal = &cc->shutdown_signal;
	g_shutdown_mutex = cc->shutdown_mutex;
	return (SetConsoleCtrlHandler(win_signal_handler, TRUE) != 0);
}

DWORD WINAPI thread_start_routine(LPVOID param)
{
	struct thread_handle * h = param;
	return (DWORD)h->routine(h->param);
}

thread_handle create_thread(
	thread_routine_t routine,
	void * param)
{
	struct thread_handle * h = alloc(sizeof(*h));
	memset(h, 0, sizeof(*h));
	h->routine = routine;
	h->param = param;
	h->os_handle = CreateThread(NULL, 0,
		thread_start_routine, h, 0, NULL);
	if (!h->os_handle) {
		dealloc(h);
		return NULL;
	}
	h->id = GetThreadId(h->os_handle);
	return h;
}

void wait_thread(thread_handle handle)
{
	WaitForSingleObject(
		((struct thread_handle *)handle)->os_handle,
		INFINITE);
}

uint64_t get_current_thread_id()
{
	return (uint64_t)GetCurrentThreadId();
}

uint32_t get_cpu_core_count()
{
	SYSTEM_INFO sysinfo;
	GetSystemInfo(&sysinfo);
	return (uint32_t)sysinfo.dwNumberOfProcessors;
}

void sleep(uint32_t ms)
{
	Sleep((DWORD)ms);
}

timer_t create_timer()
{
	LARGE_INTEGER freq;
	struct timer * t;
	if (!QueryPerformanceFrequency(&freq))
		return NULL;
	t = alloc(sizeof(*t));
	t->freq = freq;
	QueryPerformanceCounter(&t->start);
	return (timer_t)t;
}

uint64_t timer_delta(timer_t timer, boolean reset)
{
	struct timer * t = timer;
	LARGE_INTEGER ct;
	LARGE_INTEGER delta;
	QueryPerformanceCounter(&ct);
	if (ct.QuadPart <= t->start.QuadPart)
		return 0;
	memset(&delta, 0, sizeof(delta));
	delta.QuadPart = ct.QuadPart - t->start.QuadPart;
	delta.QuadPart *= 1000000;
	delta.QuadPart /= t->freq.QuadPart;
	if (reset)
		t->start = ct;
	return (uint64_t)(delta.QuadPart);
}

uint64_t timer_delta_no_reset(timer_t timer)
{
	struct timer * t = timer;
	LARGE_INTEGER ct;
	LARGE_INTEGER delta;
	QueryPerformanceCounter(&ct);
	if (ct.QuadPart <= t->start.QuadPart)
		return 0;
	memset(&delta, 0, sizeof(delta));
	delta.QuadPart = ct.QuadPart - t->start.QuadPart;
	delta.QuadPart *= 1000000;
	delta.QuadPart /= t->freq.QuadPart;
	return (uint64_t)(delta.QuadPart);
}

static LRESULT WINAPI wndproc(
	HWND hwnd,
	UINT msg,
	WPARAM wparam,
	LPARAM lparam)
{
	struct window * wnd = (struct window *)GetWindowLongPtrA(
		hwnd, GWLP_USERDATA);
	if (!wnd)
		return DefWindowProcA(hwnd, msg, wparam, lparam);
	switch (msg) {
	case WM_KEYDOWN: {
		return 0;
	}
	case WM_SIZE:
		/* Resize the application to the new window size, 
		 * except when it was minimized.
		 *
		 * Vulkan doesn't support images or swapchains
		 * with width=0 and height=0. */
		if (wparam != SIZE_MINIMIZED) {
			wnd->width = (uint32_t)(lparam & 0xffff);
			wnd->height = (uint32_t)((lparam & 0xffff0000) >> 16);
		}
		break;
	case WM_ACTIVATE:
		break;
	case WM_ERASEBKGND:
		return 1;
	case WM_SYSCOMMAND:
		if ((wparam & 0xfff0) == SC_KEYMENU) {
			/* Disable ALT application menu */
			return 0;
		}
		break;
	case WM_DESTROY:
		PostQuitMessage(0);
		wnd->is_open = FALSE;
		break;
	default:
		break;
	}
	return DefWindowProcA(hwnd, msg, wparam, lparam);
}


window create_window(
	const char * title,
	uint32_t width,
	uint32_t height)
{
	struct window * wnd;
	WNDCLASSEX wndc;
	HINSTANCE inst;
	RECT r = {
		.left = 0,
		.right = (LONG)width,
		.top = 0,
		.bottom = (LONG)height 
	};
	inst = GetModuleHandleA(NULL);
	memset(&wndc, 0, sizeof(wndc));
	wndc.cbSize = sizeof(wndc);
	wndc.lpfnWndProc = wndproc;
	wndc.cbClsExtra = 0;
	wndc.hInstance = inst;
	wndc.hIcon = LoadIconA(NULL, IDI_APPLICATION);
	wndc.hCursor = LoadCursorA(NULL, IDC_ARROW);
	wndc.hbrBackground = CreateSolidBrush(RGB(0, 0, 0));
	wndc.lpszMenuName = NULL;
	wndc.lpszClassName = "OsWndCs";
	wndc.hIconSm = NULL;
	if (!RegisterClassExA(&wndc))
		return NULL;
	AdjustWindowRect(&r, WS_OVERLAPPEDWINDOW, FALSE);
	wnd = (struct window *)alloc(sizeof(*wnd));
	if (!wnd)
		return NULL;
	memset(wnd, 0, sizeof(*wnd));
	strlcpy(wnd->title, title, COUNT_OF(wnd->title));
	wnd->width = width;
	wnd->height = height;
	wnd->is_open = TRUE;
	wnd->hwnd = CreateWindowExA(0,
		"OsWndCs", title,
		WS_OVERLAPPEDWINDOW | WS_VISIBLE | WS_SYSMENU,
		CW_USEDEFAULT, CW_USEDEFAULT, 
		r.right - r.left, r.bottom - r.top,
		NULL, NULL, inst, NULL);
	if (!wnd->hwnd) {
		dealloc(wnd);
		return NULL;
	}
	SetWindowLongPtr(wnd->hwnd, GWLP_USERDATA, (LONG_PTR)wnd);
	ShowWindow(wnd->hwnd, SW_NORMAL | SW_SHOW);
	return wnd;
}

boolean poll_window_events(window window)
{
	struct window * wnd = window;
	MSG msg;
	while (PeekMessageA(&msg, NULL, 0, 0, PM_REMOVE)) {
		TranslateMessage(&msg);
		DispatchMessageA(&msg);
		if (msg.message == WM_QUIT)
			wnd->is_open = FALSE;
	}
	return TRUE;
}

void destroy_window(window window)
{
	struct window * wnd = window;
	if (wnd->is_open)
		DestroyWindow(wnd->hwnd);
	dealloc(wnd);
}

void * get_native_window_handle(window window)
{
	return ((struct window *)window)->hwnd;
}

boolean is_window_open(window window)
{
	return ((struct window *)window)->is_open;
}
