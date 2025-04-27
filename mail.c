#include <curl/curl.h>
#include <cjson/cJSON.h>
#include "common.h"

static ULONG last_check = 0;

static void fetch_email_metadata(const char* access_token, const char* message_id)
{
	CURL* curl;
	CURLcode res;
	struct MemoryStruct chunk = { 0 };

	curl = curl_easy_init();

	if (curl)
	{
		char url[512];
		snprintf(url, sizeof(url), "https://gmail.googleapis.com/gmail/v1/users/me/messages/%s?format=metadata", message_id);

		struct curl_slist* headers = NULL;
		char auth_header[512];
		snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);
		headers = curl_slist_append(headers, auth_header);

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

		res = curl_easy_perform(curl);
		if (res == CURLE_OK)
		{
			cJSON* json = cJSON_Parse(chunk.memory);
			cJSON* headersArray = cJSON_GetObjectItemCaseSensitive(cJSON_GetObjectItem(json, "payload"), "headers");

			const char* subject = NULL, * from = NULL;
			const char* snippet = cJSON_GetObjectItem(json, "snippet")->valuestring;

			if (cJSON_IsArray(headersArray))
			{
				cJSON* h = NULL;
				cJSON_ArrayForEach(h, headersArray)
				{
					const char* name = cJSON_GetObjectItem(h, "name")->valuestring;
					const char* value = cJSON_GetObjectItem(h, "value")->valuestring;
					if (strcmp(name, "Subject") == 0) subject = value;
					if (strcmp(name, "From") == 0) from = value;
				}
			}

			wchar_t wmessage_id[32], wfrom[256], wsubject[256], wsnippet[512];

			MultiByteToWideChar(CP_UTF8, 0, from ? from : "(Unknown sender)", -1, wfrom, 256);
			MultiByteToWideChar(CP_UTF8, 0, subject ? subject : "(No subject)", -1, wsubject, 256);
			MultiByteToWideChar(CP_UTF8, 0, snippet, -1, wsnippet, 512);
			MultiByteToWideChar(CP_UTF8, 0, message_id, -1, wmessage_id, 32);

			Toast(wmessage_id, wfrom, wsubject, wsnippet);

			cJSON_Delete(json);
		}

		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
		free(chunk.memory);
	}
}

void CheckUnreadEmails(const char* access_token, BOOL retry)
{
	CURL* curl;
	CURLcode res;
	struct MemoryStruct chunk = { 0 };

	curl_global_init(CURL_GLOBAL_DEFAULT);
	curl = curl_easy_init();

	if (curl)
	{
		struct curl_slist* headers = NULL;
		char auth_header[512];
		snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);
		headers = curl_slist_append(headers, auth_header);

		char url[512] = "https://gmail.googleapis.com/gmail/v1/users/me/messages?q=is:unread";

		if (last_check > 0)
		{
			snprintf(url, sizeof(url), "https://gmail.googleapis.com/gmail/v1/users/me/messages?q=is:unread%%20after:%lu", last_check);
		}

		curl_easy_setopt(curl, CURLOPT_URL, url);
		curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
		curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
		curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void*)&chunk);

		res = curl_easy_perform(curl);
		if (res == CURLE_OK)
		{
			cJSON* json = cJSON_Parse(chunk.memory);
			cJSON* messages = cJSON_GetObjectItem(json, "messages");
			if (cJSON_IsArray(messages))
			{
				int total = cJSON_GetArraySize(messages);

				for (int i = 0; i < total; ++i)
				{
					cJSON* msg = cJSON_GetArrayItem(messages, i);
					const char* msg_id = cJSON_GetObjectItem(msg, "id")->valuestring;

					fetch_email_metadata(access_token, msg_id);
				}
			}
			cJSON_Delete(json);
		}
		else
		{
			fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
		}

		long response_code;

		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

		if (response_code > 200 && retry == NULL && RefreshTokens())
		{
			access_token = GetAccessToken(NULL);
			CheckUnreadEmails(access_token, TRUE);
		}

		curl_slist_free_all(headers);
		curl_easy_cleanup(curl);
	}

	last_check = (ULONG)time(NULL);

	free(chunk.memory);
	curl_global_cleanup();
}

BOOL MarkAsRead(const char* access_token, const char* msg_id)
{
	CURL* curl = curl_easy_init();

	if (!curl) return FALSE;

	char url[512];
	snprintf(url, sizeof(url), "https://gmail.googleapis.com/gmail/v1/users/me/messages/%s/modify", msg_id);

	struct curl_slist* headers = NULL;

	char auth_header[256];
	snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);

	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, auth_header);

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "{\"removeLabelIds\": [\"UNREAD\"]}");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	CURLcode res = curl_easy_perform(curl);
	long response_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);

	return (res == CURLE_OK && response_code == 200);
}

BOOL Trash(const char* access_token, const char* msg_id)
{
	CURL* curl = curl_easy_init();

	if (!curl) return FALSE;

	char url[512];
	snprintf(url, sizeof(url), "https://gmail.googleapis.com/gmail/v1/users/me/messages/%s/trash", msg_id);

	struct curl_slist* headers = NULL;

	char auth_header[256];
	snprintf(auth_header, sizeof(auth_header), "Authorization: Bearer %s", access_token);

	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, auth_header);

	curl_easy_setopt(curl, CURLOPT_URL, url);
	curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, "POST");
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);

	CURLcode res = curl_easy_perform(curl);

	long response_code = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);

	curl_easy_cleanup(curl);
	curl_slist_free_all(headers);

	return (res == CURLE_OK && response_code == 200);
}