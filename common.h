#pragma once

#define CLIENT_ID     "your_client_id_here"
#define CLIENT_SECRET "your_client_secret_here"
#define REDIRECT_URI  "http://localhost:8089/"
#define TIMER_ID 1
#define TIMER_INTERVAL_MS (30 * 1000) // 30 seconds

struct MemoryStruct
{
	char* memory;
	size_t size;
};

BOOL Auth();
char* WaitForAuthCode();
void OpenBrowserForAuth();
BOOL FetchTokens(const char* auth_code);
void CheckUnreadEmails(const char* access_token, BOOL retry);
BOOL RefreshTokens();
char* GetAccessToken(BOOL retry);
void Toast(wchar_t* msg_id, wchar_t* from, wchar_t* subject, wchar_t* snippet);
BOOL MarkAsRead(const char* access_token, const char* msg_id);
BOOL Trash(const char* access_token, const char* msg_id);

size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp);
void StartTimer(HWND hwnd);
void StopTimer(HWND hwnd);
