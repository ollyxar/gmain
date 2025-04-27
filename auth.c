#include <winsock2.h>
#include <ws2tcpip.h>
#include <wincred.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <curl/curl.h>
#include <cjson/cJSON.h>
#include "common.h"

#pragma comment(lib, "ws2_32.lib")

#define PORT 8089
#define ACCESS_TOKEN_KEY L"gmain-access-token"
#define REFRESH_TOKEN_KEY L"gmain-refresh-token"

size_t write_callback(void* contents, size_t size, size_t nmemb, void* userp)
{
	size_t realsize = size * nmemb;
	struct MemoryStruct* mem = (struct MemoryStruct*)userp;

	char* ptr = realloc(mem->memory, mem->size + realsize + 1);
	if (!ptr) return 0;

	mem->memory = ptr;
	memcpy(&(mem->memory[mem->size]), contents, realsize);
	mem->size += realsize;
	mem->memory[mem->size] = 0;

	return realsize;
}

void StartTimer(HWND hwnd)
{
	SetTimer(hwnd, TIMER_ID, TIMER_INTERVAL_MS, NULL);
}

void StopTimer(HWND hwnd)
{
	KillTimer(hwnd, TIMER_ID);
}

void OpenBrowserForAuth()
{
	const char* auth_url_fmt =
		"https://accounts.google.com/o/oauth2/v2/auth?"
		"client_id=%s&"
		"redirect_uri=%s&"
		"response_type=code&"
		"scope=https://www.googleapis.com/auth/gmail.modify&"
		"access_type=offline&"
		"prompt=consent";

	char url[1024];
	snprintf(url, sizeof(url), auth_url_fmt, CLIENT_ID, REDIRECT_URI);

	ShellExecuteA(NULL, "open", url, NULL, NULL, SW_SHOWNORMAL);
}

static BOOL SaveToken(const wchar_t* targetName, const char* token)
{
	if (!token) return FALSE;

	CREDENTIALW cred = { 0 };
	cred.Type = CRED_TYPE_GENERIC;
	cred.TargetName = (LPWSTR)targetName;
	cred.CredentialBlobSize = (DWORD)strlen(token);
	cred.CredentialBlob = (LPBYTE)token;
	cred.Persist = CRED_PERSIST_LOCAL_MACHINE;
	cred.UserName = NULL;

	return CredWriteW(&cred, 0);
}

static char* LoadToken(const wchar_t* targetName)
{
	PCREDENTIALW pcred = NULL;

	if (CredReadW(targetName, CRED_TYPE_GENERIC, 0, &pcred))
	{
		char* token = (char*)malloc(pcred->CredentialBlobSize + 1);

		if (token)
		{
			memcpy(token, pcred->CredentialBlob, pcred->CredentialBlobSize);
			token[pcred->CredentialBlobSize] = '\0';
		}

		CredFree(pcred);
		return token;
	}

	return NULL;
}

BOOL FetchTokens(const char* auth_code)
{
	struct MemoryStruct chunk = { 0 };
	CURL* curl = curl_easy_init();
	if (!curl) return FALSE;

	char post_fields[1024];
	snprintf(post_fields, sizeof(post_fields),
		"code=%s&"
		"client_id=%s&"
		"client_secret=%s&"
		"redirect_uri=%s&"
		"grant_type=authorization_code",
		auth_code, CLIENT_ID, CLIENT_SECRET, REDIRECT_URI);

	curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/token");
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);

	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

	CURLcode res = curl_easy_perform(curl);

	long response_code;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

	if (res == CURLE_OK)
	{
		if (response_code == 200)
		{
			cJSON* json = cJSON_Parse(chunk.memory);

			const char* access_token = cJSON_GetObjectItem(json, "access_token")->valuestring;
			const char* refresh_token = cJSON_GetObjectItem(json, "refresh_token")->valuestring;

			SaveToken(ACCESS_TOKEN_KEY, access_token);
			SaveToken(REFRESH_TOKEN_KEY, refresh_token);

			cJSON_Delete(json);
		}

		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
		free(chunk.memory);

		return TRUE;
	}

	return FALSE;
}

char* WaitForAuthCode()
{
	WSADATA wsaData;
	SOCKET serverSocket, clientSocket;
	struct sockaddr_in serverAddr, clientAddr;
	char buffer[4096];
	int clientLen = sizeof(clientAddr);

	if (WSAStartup(MAKEWORD(2, 2), &wsaData))
	{
		// WSAStartup failed
		return NULL;
	}

	serverSocket = socket(AF_INET, SOCK_STREAM, 0);
	if (serverSocket == INVALID_SOCKET)
	{
		// Socket creation failed
		WSACleanup();
		return NULL;
	}

	memset(&serverAddr, 0, sizeof(serverAddr));
	serverAddr.sin_family = AF_INET;
	serverAddr.sin_port = htons(PORT);

	if (inet_pton(AF_INET, "127.0.0.1", &serverAddr.sin_addr) != 1)
	{
		// inet_pton failed
		closesocket(serverSocket);
		WSACleanup();
		return NULL;
	}

	if (bind(serverSocket, (struct sockaddr*)&serverAddr, sizeof(serverAddr)) < 0)
	{
		// Bind failed
		closesocket(serverSocket);
		WSACleanup();
		return NULL;
	}

	if (listen(serverSocket, 1) < 0)
	{
		// Listen failed
		closesocket(serverSocket);
		WSACleanup();
		return NULL;
	}

	// Waiting for redirect from Google...

	clientSocket = accept(serverSocket, (struct sockaddr*)&clientAddr, &clientLen);
	if (clientSocket == INVALID_SOCKET)
	{
		// Accept failed
		closesocket(serverSocket);
		WSACleanup();
		return NULL;
	}

	recv(clientSocket, buffer, sizeof(buffer) - 1, 0);
	buffer[sizeof(buffer) - 1] = '\0';

	char* code_start = strstr(buffer, "GET /?code=");
	if (!code_start)
	{
		// No code in request
		closesocket(clientSocket);
		closesocket(serverSocket);
		WSACleanup();
		return NULL;
	}

	code_start += strlen("GET /?code=");
	char* code_end = strchr(code_start, ' ');
	if (!code_end) code_end = code_start + strlen(code_start);

	int code_len = (int)(code_end - code_start);
	char* auth_code = (char*)malloc(code_len + 1);
	if (!auth_code)
	{
		// Memory allocation failed
		closesocket(clientSocket);
		closesocket(serverSocket);
		WSACleanup();
		return NULL;
	}

	if (strncpy_s(auth_code, code_len + 1, code_start, code_len) != 0)
	{
		// strncpy_s failed
		free(auth_code);
		closesocket(clientSocket);
		closesocket(serverSocket);
		WSACleanup();
		return NULL;
	}

	auth_code[code_len] = '\0';

	const char* html = "HTTP/1.1 200 OK\r\nContent-Type: text/html\r\n\r\n<html><body><h2>Auth complete. You can close this window.</h2></body></html>";
	send(clientSocket, html, (int)strlen(html), 0);

	closesocket(clientSocket);
	closesocket(serverSocket);
	WSACleanup();

	return auth_code;
}

BOOL Auth()
{
	BOOL result = FALSE;

	OpenBrowserForAuth();
	char* code = WaitForAuthCode();

	if (code)
	{
		result = FetchTokens(code);
		free(code);
	}

	return result;
}

BOOL RefreshTokens()
{
	char* refresh_token = LoadToken(REFRESH_TOKEN_KEY);

	if (refresh_token == NULL) return FALSE;

	struct MemoryStruct chunk = { 0 };
	CURL* curl = curl_easy_init();

	if (!curl) return FALSE;

	char post_fields[1024];
	snprintf(post_fields, sizeof(post_fields),
		"refresh_token=%s&"
		"client_id=%s&"
		"client_secret=%s&"
		"grant_type=refresh_token",
		refresh_token, CLIENT_ID, CLIENT_SECRET);

	free(refresh_token);

	curl_easy_setopt(curl, CURLOPT_URL, "https://oauth2.googleapis.com/token");
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post_fields);

	struct curl_slist* headers = NULL;
	headers = curl_slist_append(headers, "Content-Type: application/x-www-form-urlencoded");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

	CURLcode res = curl_easy_perform(curl);

	long response_code;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

	if (res == CURLE_OK)
	{
		if (response_code == 200)
		{
			cJSON* json = cJSON_Parse(chunk.memory);

			const char* access_token = cJSON_GetObjectItem(json, "access_token")->valuestring;

			SaveToken(ACCESS_TOKEN_KEY, access_token);

			cJSON_Delete(json);
		}
		
		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
		free(chunk.memory);

		return TRUE;
	}

	return FALSE;
}

char* GetAccessToken(BOOL retry)
{
	long length;
	char* buffer = 0;
	char* result = LoadToken(ACCESS_TOKEN_KEY);

	if (result == NULL && retry == NULL && Auth())
	{
		return GetAccessToken(TRUE);
	}

	return result;
}