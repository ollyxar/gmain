#include <initguid.h>
#include <windows.h>
#include <roapi.h>
#include <Windows.ui.notifications.h>
#include <notificationactivationcallback.h>
#include <tchar.h>
#include <stdio.h>

#include "common.h"

#define ID_PANEL   1
#define ID_PANEL2  2
#define ID_BUTTON  3
#define ID_BUTTON2 4
#define ID_LABEL   5

#pragma comment(lib, "runtimeobject.lib")
#pragma comment(lib, "secur32.lib")
#pragma comment(lib, "crypt32.lib")

#pragma comment(linker,"\"/manifestdependency:type='win32' \
 name='Microsoft.Windows.Common-Controls' \
 version='6.0.0.0' \
 processorArchitecture='*' \
 publicKeyToken='6595b64144ccf1df' \
 language='*'\"")

#define GUID_Impl_INotificationActivationCallback_Textual "0CB8162B-016B-4CDC-9ACB-14C6DC6B8EEB"

DEFINE_GUID(GUID_Impl_INotificationActivationCallback,
	0x0cb8162b, 0x016b, 0x4cdc, 0x9a, 0xcb, 0x14, 0xc6, 0xdc, 0x6b, 0x8e, 0xeb);

DEFINE_GUID(IID_IToastNotificationManagerStatics,
	0x50ac103f, 0xd235, 0x4598, 0xbb, 0xef, 0x98, 0xfe, 0x4d, 0x1a, 0x3a, 0xd4);

DEFINE_GUID(IID_IToastNotificationFactory,
	0x04124b20, 0x82c6, 0x4229, 0xb1, 0x09, 0xfd, 0x9e, 0xd4, 0x66, 0x2b, 0x53);

DEFINE_GUID(IID_IXmlDocument,
	0xf7f3a506, 0x1e87, 0x42d6, 0xbc, 0xfb, 0xb8, 0xc8, 0x09, 0xfa, 0x54, 0x94);

DEFINE_GUID(IID_IXmlDocumentIO,
	0x6cd0e74e, 0xee65, 0x4489, 0x9e, 0xbf, 0xca, 0x43, 0xe8, 0x7b, 0xa6, 0x37);

DWORD dwMainThreadId = 0;

#pragma region "IGeneric : IUnknown implementation"
typedef struct Impl_IGeneric
{
	IUnknownVtbl* lpVtbl;
	LONG64 dwRefCount;
} Impl_IGeneric;

static ULONG __stdcall Impl_IGeneric_AddRef(Impl_IGeneric* _this)
{
	return InterlockedIncrement64(&(_this->dwRefCount));
}

static ULONG __stdcall Impl_IGeneric_Release(Impl_IGeneric* _this)
{
	LONG64 dwNewRefCount = InterlockedDecrement64(&(_this->dwRefCount));
	if (!dwNewRefCount) free(_this);
	return dwNewRefCount;
}
#pragma endregion

#pragma region "INotificationActivationCallback : IGeneric implementation"
static HRESULT __stdcall Impl_INotificationActivationCallback_QueryInterface(Impl_IGeneric* _this, REFIID riid, void** ppvObject)
{
	if (!IsEqualIID(riid, &IID_INotificationActivationCallback) && !IsEqualIID(riid, &IID_IUnknown))
	{
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	*ppvObject = _this;
	_this->lpVtbl->AddRef(_this);
	return S_OK;
}

static char* wchar_to_utf8(const wchar_t* wstr)
{
	int size_needed = WideCharToMultiByte(CP_UTF8, 0, wstr, -1, NULL, 0, NULL, NULL);
	if (size_needed <= 0) return NULL;

	char* str = (char*)malloc(size_needed);
	if (!str) return NULL;

	WideCharToMultiByte(CP_UTF8, 0, wstr, -1, str, size_needed, NULL, NULL);
	return str;
}

static HRESULT __stdcall Impl_INotificationActivationCallback_Activate(INotificationActivationCallback* _this, LPCWSTR appUserModelId, LPCWSTR invokedArgs, const NOTIFICATION_USER_INPUT_DATA* data, ULONG count)
{
	wchar_t action[8];
	wchar_t id[32];
	const wchar_t* actionStart = wcsstr(invokedArgs, L"action=");
	const wchar_t* contentIdStart = wcsstr(invokedArgs, L"contentId=");

	if (!actionStart || !contentIdStart) return S_FALSE;

	// Move past "action="
	actionStart += 7;
	const WCHAR* actionEnd = wcschr(actionStart, L';');
	if (!actionEnd) actionEnd = invokedArgs + wcslen(invokedArgs);

	size_t actionLen = actionEnd - actionStart;

	wcsncpy_s(action, 8, actionStart, actionLen);
	action[actionLen] = L'\0';

	// Move past "contentId="
	contentIdStart += 10;
	size_t contentIdLen = wcslen(contentIdStart);

	wcsncpy_s(id, 32, contentIdStart, contentIdLen);
	id[contentIdLen] = L'\0';

	if (!_wcsicmp(action, L"read"))
	{
		char* token = GetAccessToken();

		if (token)
		{
			const char* cId = wchar_to_utf8(id);
			MarkAsRead(token, cId);
			free(token);
			free(cId);
		}
	}

	if (!_wcsicmp(action, L"delete"))
	{
		char* token = GetAccessToken();

		if (token)
		{
			const char* cId = wchar_to_utf8(id);
			Trash(token, cId);
			free(token);
			free(cId);
		}
	}

	return S_OK;
}

static const INotificationActivationCallbackVtbl Impl_INotificationActivationCallback_Vtbl = {
	.QueryInterface = Impl_INotificationActivationCallback_QueryInterface,
	.AddRef = Impl_IGeneric_AddRef,
	.Release = Impl_IGeneric_Release,
	.Activate = Impl_INotificationActivationCallback_Activate
};
#pragma endregion


#pragma region "IClassFactory : IGeneric implementation"
static HRESULT __stdcall Impl_IClassFactory_QueryInterface(Impl_IGeneric* _this, REFIID riid, void** ppvObject)
{
	if (!IsEqualIID(riid, &IID_IClassFactory) && !IsEqualIID(riid, &IID_IUnknown))
	{
		*ppvObject = NULL;
		return E_NOINTERFACE;
	}
	*ppvObject = _this;
	_this->lpVtbl->AddRef(_this);
	return S_OK;
}

static HRESULT __stdcall Impl_IClassFactory_LockServer(IClassFactory* _this, BOOL flock)
{
	return S_OK;
}

static HRESULT __stdcall Impl_IClassFactory_CreateInstance(IClassFactory* _this, IUnknown* punkOuter, REFIID vTableGuid, void** ppv)
{
	HRESULT hr = E_NOINTERFACE;
	Impl_IGeneric* thisobj = NULL;
	*ppv = 0;

	if (punkOuter) hr = CLASS_E_NOAGGREGATION;
	else
	{
		BOOL bOk = FALSE;
		if (!(thisobj = malloc(sizeof(Impl_IGeneric)))) hr = E_OUTOFMEMORY;
		else
		{
			thisobj->lpVtbl = &Impl_INotificationActivationCallback_Vtbl;
			bOk = TRUE;
		}
		if (bOk)
		{
			thisobj->dwRefCount = 1;
			hr = thisobj->lpVtbl->QueryInterface(thisobj, vTableGuid, ppv);
			thisobj->lpVtbl->Release(thisobj);
		}
		else
		{
			return hr;
		}
	}

	return hr;
}

static const IClassFactoryVtbl Impl_IClassFactory_Vtbl = {
	.QueryInterface = Impl_IClassFactory_QueryInterface,
	.AddRef = Impl_IGeneric_AddRef,
	.Release = Impl_IGeneric_Release,
	.LockServer = Impl_IClassFactory_LockServer,
	.CreateInstance = Impl_IClassFactory_CreateInstance
};
#pragma endregion

#define APP_ID L"GmailNotifier"
#define TOAST_ACTIVATED_LAUNCH_ARG "-ToastActivated"

const wchar_t wszBannerText[] =
L"<toast scenario='reminder' activationType='foreground' launch='action=mainContent' duration='short' useButtonStyle='true'>\r\n"
L"	<visual>\r\n"
L"		<binding template='ToastGeneric'>\r\n"
L"          <image placement='appLogoOverride' id='1' src='%s/img.gif' />\r\n"
L"			<text><![CDATA[%s]]></text>\r\n"
L"			<text><![CDATA[%s]]></text>\r\n"
L"			<text placement='attribution'><![CDATA[%s]]></text>\r\n"
L"		</binding>\r\n"
L"	</visual>\r\n"
L"  <actions>\r\n"
L"	  <action content='Mark as read' activationType='foreground' arguments='action=read;contentId=%s' hint-buttonStyle='Success' imageUri='%s/checkmark.png'/>\r\n"
L"	  <action content='View' activationType='protocol' arguments='https://mail.google.com/mail/u/me/#all/%s'/>\r\n"
L"	  <action content='Delete' activationType='foreground' arguments='action=delete;contentId=%s' hint-buttonStyle='Critical' imageUri='%s/trash-can.png'/>\r\n"
L"  </actions>\r\n"
L"	<audio src='ms-winsoundevent:Notification.Mail' loop='false' silent='false'/>\r\n"
L"</toast>\r\n";

static Impl_IGeneric* pClassFactory = NULL;
static IInspectable* pInspectable = NULL;
static HSTRING hsXmlDocument = NULL;
static HSTRING hsBanner = NULL;
static WNDPROC g_OldPanelProc = NULL;

static __x_ABI_CWindows_CUI_CNotifications_CIToastNotifier* pToastNotifier = NULL;
static __x_ABI_CWindows_CUI_CNotifications_CIToastNotification* pToastNotification = NULL;
static __x_ABI_CWindows_CUI_CNotifications_CIToastNotificationFactory* pNotificationFactory = NULL;
static __x_ABI_CWindows_CData_CXml_CDom_CIXmlDocument* pXmlDocument = NULL;
static __x_ABI_CWindows_CData_CXml_CDom_CIXmlDocumentIO* pXmlDocumentIO = NULL;

void Toast(wchar_t* msg_id, wchar_t* from, wchar_t* subject, wchar_t* snippet)
{
	HSTRING_HEADER hshXmlDocument;
	WindowsCreateStringReference(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument, (UINT32)(sizeof(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument) / sizeof(wchar_t) - 1), &hshXmlDocument, &hsXmlDocument);

	HSTRING_HEADER hshBanner;

	wchar_t wszRootPath[MAX_PATH];
	ZeroMemory(wszRootPath, MAX_PATH);
	wcscat_s(wszRootPath, MAX_PATH, L"file:///");

	wchar_t wszAppDir[MAX_PATH];
	ZeroMemory(wszAppDir, MAX_PATH);
	GetCurrentDirectoryW(MAX_PATH, wszAppDir);

	for (wchar_t* p = wszAppDir; *p != L'\0'; ++p)
	{
		if (*p == L'\\') *p = L'/';
	}

	wcscat_s(wszRootPath, MAX_PATH, wszAppDir);

	wchar_t wszResult[2048];
	swprintf(wszResult, 2048, wszBannerText, wszRootPath, from, subject, snippet, msg_id, wszRootPath, msg_id, msg_id, wszRootPath);

	WindowsCreateStringReference(wszResult, wcslen(wszResult), &hshBanner, &hsBanner);

	RoActivateInstance(hsXmlDocument, &pInspectable);

	pInspectable->lpVtbl->QueryInterface(pInspectable, &IID_IXmlDocument, &pXmlDocument);
	pXmlDocument->lpVtbl->QueryInterface(pXmlDocument, &IID_IXmlDocumentIO, &pXmlDocumentIO);
	pXmlDocumentIO->lpVtbl->LoadXml(pXmlDocumentIO, hsBanner);
	pNotificationFactory->lpVtbl->CreateToastNotification(pNotificationFactory, pXmlDocument, &pToastNotification);
	pToastNotifier->lpVtbl->Show(pToastNotifier, pToastNotification);
}

LRESULT CALLBACK PanelProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	switch (msg)
	{
		case WM_COMMAND:
		{
			if (LOWORD(wParam) == ID_BUTTON)
			{
				if (!Auth())
				{
					MessageBoxA(hwnd, "Auth code was not received", "Auth failed", MB_OK | MB_ICONERROR);
				}
			}
			if (LOWORD(wParam) == ID_BUTTON2)
			{
				char* token = GetAccessToken();

				if (token)
				{
					CheckUnreadEmails(token, FALSE);
					free(token);
				}
			}
		}
		break;
	}

	return CallWindowProcW(g_OldPanelProc, hwnd, msg, wParam, lParam);
}

LRESULT CALLBACK WindowProcedure(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	static HWND hButton, hButton2, hPanel, hPanel2, hLabel;
	static HFONT hFont;

	switch (msg)
	{
		case WM_CREATE:
			hFont = CreateFontW(
				-11,
				0, 0, 0,
				FW_NORMAL,
				FALSE, FALSE, FALSE,
				DEFAULT_CHARSET,
				OUT_DEFAULT_PRECIS,
				CLIP_DEFAULT_PRECIS,
				CLEARTYPE_QUALITY,
				DEFAULT_PITCH | FF_DONTCARE,
				L"Segoe UI"
			);

			hPanel = CreateWindowA("Button", "Account actions", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 20, 140, 300, 80, hwnd, (HMENU)ID_PANEL, NULL, NULL);
			hPanel2 = CreateWindowA("Button", "How to use", WS_CHILD | WS_VISIBLE | BS_GROUPBOX, 20, 20, 300, 100, hwnd, (HMENU)ID_PANEL2, NULL, NULL);
			hButton = CreateWindowA("Button", "(Re-)Authenticate", WS_CHILD | WS_VISIBLE, 20, 30, 120, 30, hPanel, (HMENU)ID_BUTTON, NULL, NULL);
			hButton2 = CreateWindowA("Button", "Check unread emails", WS_CHILD | WS_VISIBLE, 160, 30, 120, 30, hPanel, (HMENU)ID_BUTTON2, NULL, NULL);
			
			g_OldPanelProc = (WNDPROC)SetWindowLongPtrW(hPanel, GWLP_WNDPROC, (LONG_PTR)PanelProc);

			HWND hChild1 = GetWindow(hwnd, GW_CHILD);

			while (hChild1)
			{
				SendMessageW(hChild1, WM_SETFONT, (WPARAM)hFont, TRUE);
				HWND hChild = GetWindow(hChild1, GW_CHILD);

				while (hChild)
				{
					SendMessageW(hChild, WM_SETFONT, (WPARAM)hFont, TRUE);
					hChild = GetWindow(hChild, GW_HWNDNEXT);
				}

				hChild1 = GetWindow(hChild1, GW_HWNDNEXT);
			}
			
			StartTimer(hwnd);

			break;

		case WM_PAINT:
		{
			PAINTSTRUCT ps;
			HDC hdc = BeginPaint(hwnd, &ps);
			SelectObject(hdc, hFont);
			SetBkMode(hdc, TRANSPARENT);

			RECT rect = { 30, 40, 300, 120 };

			DrawTextA(hdc, "To setup your account click on '(Re-)Authenticate' button. The link will be opened by your default web-browser. The application will listen for a redirect from the auth-page. After that the app will check for new unread messages every 30 seconds.", -1, &rect, DT_LEFT | DT_TOP | DT_WORDBREAK);

			EndPaint(hwnd, &ps);
		}
		break;

		case WM_DESTROY:
			StopTimer(hwnd);
			PostQuitMessage(0);
			break;

		case WM_TIMER:
			if (wParam == TIMER_ID)
			{
				StopTimer(hwnd);
				char* token = GetAccessToken();

				if (token)
				{
					CheckUnreadEmails(token, FALSE);
					free(token);
					StartTimer(hwnd);
				}
			}
			break;

		case WM_SIZE:
			switch (wParam)
			{
				case SIZE_MINIMIZED:
					ShowWindow(hwnd, SW_HIDE);
					break;
			}
			break;

		default:
			return DefWindowProc(hwnd, msg, wParam, lParam);
	}
	return 0;
}

int WINAPI WinMain(_In_ HINSTANCE hInstance, _In_opt_ HINSTANCE hPrevInstance, _In_ LPSTR lpCmdLine, _In_ int nCmdShow)
{
	dwMainThreadId = GetCurrentThreadId();

	HANDLE hMutex = CreateMutex(NULL, TRUE, TEXT("GmailNotifier"));

	if (ERROR_ALREADY_EXISTS == GetLastError())
	{
		HWND prevWindow = FindWindow(L"GmailNotifierClass", NULL);
		SetForegroundWindow(prevWindow);
		BringWindowToTop(prevWindow);
		ShowWindow(prevWindow, SW_RESTORE);
		return 0;
	}

	CoInitializeEx(NULL, COINIT_MULTITHREADED);
	RoInitialize(RO_INIT_MULTITHREADED);

	if (!(pClassFactory = malloc(sizeof(Impl_IGeneric))))
	{
		exit(2); // out of memory
	}

	pClassFactory->lpVtbl = &Impl_IClassFactory_Vtbl;
	pClassFactory->dwRefCount = 1;


	DWORD dwCookie = 0;
	CoRegisterClassObject(&GUID_Impl_INotificationActivationCallback, pClassFactory, CLSCTX_LOCAL_SERVER, REGCLS_MULTIPLEUSE, &dwCookie);

	wchar_t wszExePath[MAX_PATH + 100];
	ZeroMemory(wszExePath, MAX_PATH + 100);
	GetModuleFileNameW(NULL, wszExePath + 1, MAX_PATH);

	wszExePath[0] = L'"';
	wcscat_s(wszExePath, MAX_PATH + 100, L"\" " _T(TOAST_ACTIVATED_LAUNCH_ARG));

	wchar_t wszAppImg[MAX_PATH + 100];
	ZeroMemory(wszAppImg, MAX_PATH + 100);
	GetCurrentDirectoryW(MAX_PATH, wszAppImg);
	wcscat_s(wszAppImg, MAX_PATH + 100, L"\\img.png");

	RegSetValueW(HKEY_CURRENT_USER, L"SOFTWARE\\Classes\\CLSID\\{" _T(GUID_Impl_INotificationActivationCallback_Textual) L"}\\LocalServer32", REG_SZ, wszExePath, NULL);
	RegSetKeyValueW(HKEY_CURRENT_USER, L"SOFTWARE\\Classes\\AppUserModelId\\" APP_ID, L"IconUri", REG_SZ, wszAppImg, wcslen(wszAppImg) * sizeof(wchar_t));
	RegSetKeyValueW(HKEY_CURRENT_USER, L"SOFTWARE\\Classes\\AppUserModelId\\" APP_ID, L"DisplayName", REG_SZ, L"GmailNotifier", 14 * sizeof(wchar_t));
	RegSetKeyValueW(HKEY_CURRENT_USER, L"SOFTWARE\\Classes\\AppUserModelId\\" APP_ID, L"CustomActivator", REG_SZ, L"{" _T(GUID_Impl_INotificationActivationCallback_Textual) L"}", 39 * sizeof(wchar_t));

	HSTRING_HEADER hshAppId;
	HSTRING hsAppId = NULL;
	WindowsCreateStringReference(APP_ID, (UINT32)(sizeof(APP_ID) / sizeof(TCHAR) - 1), &hshAppId, &hsAppId);

	HSTRING_HEADER hshToastNotificationManager;
	HSTRING hsToastNotificationManager = NULL;
	WindowsCreateStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager, (UINT32)(sizeof(RuntimeClass_Windows_UI_Notifications_ToastNotificationManager) / sizeof(wchar_t) - 1), &hshToastNotificationManager, &hsToastNotificationManager);

	__x_ABI_CWindows_CUI_CNotifications_CIToastNotificationManagerStatics* pToastNotificationManager = NULL;
	RoGetActivationFactory(hsToastNotificationManager, &IID_IToastNotificationManagerStatics, (LPVOID*)&pToastNotificationManager);

	pToastNotificationManager->lpVtbl->CreateToastNotifierWithId(pToastNotificationManager, hsAppId, &pToastNotifier);

	HSTRING_HEADER hshToastNotification;
	HSTRING hsToastNotification = NULL;
	WindowsCreateStringReference(RuntimeClass_Windows_UI_Notifications_ToastNotification, (UINT32)(sizeof(RuntimeClass_Windows_UI_Notifications_ToastNotification) / sizeof(wchar_t) - 1), &hshToastNotification, &hsToastNotification);

	RoGetActivationFactory(hsToastNotification, &IID_IToastNotificationFactory, (LPVOID*)&pNotificationFactory);

	HSTRING_HEADER hshXmlDocument;
	WindowsCreateStringReference(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument, (UINT32)(sizeof(RuntimeClass_Windows_Data_Xml_Dom_XmlDocument) / sizeof(wchar_t) - 1), &hshXmlDocument, &hsXmlDocument);


	WNDCLASS wc = { 0 };
	HWND hwnd;
	MSG msg;

	wc.lpfnWndProc = WindowProcedure;
	wc.hInstance = hInstance;
	wc.lpszClassName = L"GmailNotifierClass";
	wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
	//wc.hIcon = LoadIcon(hInstance, MAKEINTRESOURCE(IDI_MAINICON));

	RegisterClass(&wc);

	hwnd = CreateWindow(L"GmailNotifierClass", L"Light Gmail notifier",
		WS_OVERLAPPED | WS_CAPTION | WS_SYSMENU | WS_MINIMIZEBOX, CW_USEDEFAULT, CW_USEDEFAULT, 360, 290,
		NULL, NULL, hInstance, NULL);

	ShowWindow(hwnd, nCmdShow);
	UpdateWindow(hwnd);

	while (GetMessage(&msg, NULL, 0, 0))
	{
		TranslateMessage(&msg);
		DispatchMessage(&msg);
	}

	if (pToastNotification) pToastNotification->lpVtbl->Release(pToastNotification);
	if (pXmlDocumentIO) pXmlDocumentIO->lpVtbl->Release(pXmlDocumentIO);
	if (pXmlDocument) pXmlDocument->lpVtbl->Release(pXmlDocument);
	if (pInspectable) pInspectable->lpVtbl->Release(pInspectable);
	if (hsBanner) WindowsDeleteString(hsBanner);
	if (hsXmlDocument) WindowsDeleteString(hsXmlDocument);
	if (pNotificationFactory) pNotificationFactory->lpVtbl->Release(pNotificationFactory);
	if (hsToastNotification) WindowsDeleteString(hsToastNotification);
	if (pToastNotifier) pToastNotifier->lpVtbl->Release(pToastNotifier);
	if (pToastNotificationManager) pToastNotificationManager->lpVtbl->Release(pToastNotificationManager);
	if (hsToastNotificationManager) WindowsDeleteString(hsToastNotificationManager);
	if (hsAppId) WindowsDeleteString(hsAppId);
	if (dwCookie) CoRevokeClassObject(dwCookie);
	if (pClassFactory)pClassFactory->lpVtbl->Release(pClassFactory);

	RoUninitialize();
	CoUninitialize();

	if (hMutex)
	{
		ReleaseMutex(hMutex);
		CloseHandle(hMutex);
	}

	return (int)msg.wParam;
}
