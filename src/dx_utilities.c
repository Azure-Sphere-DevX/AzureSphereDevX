/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "dx_utilities.h"

static char *_log_debug_buffer = NULL;
static size_t _log_debug_buffer_size;

// Curl stuff.
struct MemoryStruct {
    char *memory;
    size_t size;
};

bool dx_isStringNullOrEmpty(const char *string)
{
    return string == NULL || strlen(string) == 0;
}


/// <summary>
/// check string contain only printable characters
/// ! " # $ % & ' ( ) * + , - . / 0 1 2 3 4 5 6 7 8 9 : ; < = > ? @ A B C D E F G H I J K L M N O P Q
/// R S T U V W X Y Z [ \ ] ^ _ ` a b c d e f g h i j k l m n o p q r s t u v w x y z { | } ~
/// </summary>
/// <param name="data"></param>
/// <returns></returns>
bool dx_isStringPrintable(char *data)
{
    while (isprint(*data)) {
        data++;
    }
    return 0x00 == *data;
}

static size_t WriteMemoryCallback(void *contents, size_t size, size_t nmemb, void *userp)
{
    size_t realsize = size * nmemb;
    struct MemoryStruct *mem = (struct MemoryStruct *)userp;

    char *ptr = realloc(mem->memory, mem->size + realsize + 1);
    if (!ptr) {
        /* out of memory! */
        Log_Debug("not enough memory (realloc returned NULL)\n");
        return 0;
    }

    mem->memory = ptr;
    memcpy(&(mem->memory[mem->size]), contents, realsize);
    mem->size += realsize;
    mem->memory[mem->size] = 0;

    return realsize;
}

// https://curl.se/libcurl/c/getinmemory.html
char *dx_getHttpData(const char *url)
{
    static bool curl_initialized = false;
    CURL *curl_handle;
    CURLcode res;

    if (!curl_initialized) {
        curl_global_init(CURL_GLOBAL_ALL);
        curl_initialized = true;
    }

    struct MemoryStruct chunk;

    chunk.memory = malloc(1); /* will be grown as needed by the realloc above */
    chunk.size = 0;           /* no data at this point */

    /* init the curl session */
    curl_handle = curl_easy_init();

    /* specify URL to get */
    curl_easy_setopt(curl_handle, CURLOPT_URL, url);

    /* use a GET to fetch data */
    curl_easy_setopt(curl_handle, CURLOPT_HTTPGET, 1L);

    /* send all data to this function  */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);

    /* we pass our 'chunk' struct to the callback function */
    curl_easy_setopt(curl_handle, CURLOPT_WRITEDATA, (void *)&chunk);

    /* some servers do not like requests that are made without a user-agent
       field, so we provide one */
    curl_easy_setopt(curl_handle, CURLOPT_USERAGENT, "libcurl-agent/1.0");

    // based on the libcurl sample - https://curl.se/libcurl/c/https.html
    curl_easy_setopt(curl_handle, CURLOPT_SSL_VERIFYPEER, 0L);

    /* get it! */
    res = curl_easy_perform(curl_handle);

    curl_easy_cleanup(curl_handle);

    if (res == CURLE_OK) {
        // caller is responsible for freeing this.
        return chunk.memory;
    } else {
        free(chunk.memory);
    }

    return NULL;
}

bool dx_isNetworkReady(void)
{
    bool isNetworkReady = false;
    if (Networking_IsNetworkingReady(&isNetworkReady) != 0) {
        Log_Debug("ERROR: Networking_IsNetworkingReady: %d (%s)\n", errno, strerror(errno));
    }

    return isNetworkReady;
}

bool dx_isNetworkConnected(const char *networkInterface)
{
    static int64_t lastNetworkStatusCheck = 0;
    static bool previousNetworkStatus = false;
    Networking_InterfaceConnectionStatus status;

    // if the network is not ready don't bother with checking network status
    if (!dx_isNetworkReady()) {
        previousNetworkStatus = false;
        return false;
    }

    int64_t now = dx_getNowMilliseconds();

    // Check end to end internet connection every 2 minutes
    // Or if previous check returned false, network not connected, but now network is ready
    if (!previousNetworkStatus || (now - lastNetworkStatusCheck) > 1000 * 60 * 2) {
        lastNetworkStatusCheck = now;

        if (networkInterface != NULL && strlen(networkInterface) > 0) {
            // https://docs.microsoft.com/en-us/azure-sphere/reference/applibs-reference/applibs-networking/function-networking-getinterfaceconnectionstatus
            if (Networking_GetInterfaceConnectionStatus(networkInterface, &status) == 0) {
                if ((status & Networking_InterfaceConnectionStatus_ConnectedToInternet) == 0) {
                    previousNetworkStatus = false;
                    return false;
                } else {
                    previousNetworkStatus = true;
                    return true;
                }
            }
        } else {
            // fall back to returning dx_isNetworkReady status which must be true given got this far
            return true;
        }
    }
    // No interface provided or status failed so fallback to isNetworkReady() which must be true
    // given got this far
    return true;
}

bool dx_isDeviceAuthReady(void)
{
    // Verifies authentication is ready on device
    bool currentAppDeviceAuthReady = false;
    if (Application_IsDeviceAuthReady(&currentAppDeviceAuthReady) != 0) {
        Log_Debug("ERROR: Application_IsDeviceAuthReady: %d (%s)\n", errno, strerror(errno));
    } else {
        if (!currentAppDeviceAuthReady) {
            Log_Debug("ERROR: No internet connection, check your network connection\n");
        }
    }

    return currentAppDeviceAuthReady;
}

char *dx_getCurrentUtc(char *buffer, size_t bufferSize)
{
    time_t now = time(NULL);
    struct tm *t = gmtime(&now);
    strftime(buffer, bufferSize - 1, "%Y-%m-%dT%H:%M:%SZ", t);
    return buffer;
}

int64_t dx_getNowMilliseconds(void)
{
    struct timespec now = {0, 0};
    clock_gettime(CLOCK_MONOTONIC, &now);
    return now.tv_sec * 1000 + now.tv_nsec / 1000000;
}

int dx_stringEndsWith(const char *str, const char *suffix)
{
    if (!str || !suffix)
        return 0;
    size_t lenstr = strlen(str);
    size_t lensuffix = strlen(suffix);
    if (lensuffix > lenstr)
        return 0;
    return strncmp(str + lenstr - lensuffix, suffix, lensuffix) == 0;
}

bool dx_startThreadDetached(void *(daemon)(void *), void *arg, char *daemon_name)
{
    pthread_attr_t attr;
    pthread_attr_init(&attr);
    pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_DETACHED);

    pthread_t thread;

    Log_Debug("Starting thread %s detached\n", daemon_name);

    if (pthread_create(&thread, &attr, daemon, arg)) {
        Log_Debug("ERROR: Failed to start %s daemon.\n", daemon_name);
        return false;
    }
    return true;
}

void dx_Log_Debug_Init(const char *buffer, size_t buffer_size)
{
    _log_debug_buffer = (char *)buffer;
    _log_debug_buffer_size = buffer_size;
}

void dx_Log_Debug(char *fmt, ...)
{
    if (_log_debug_buffer == NULL) {
        Log_Debug("log_debug_buffer is NULL. Call Log_Debug_Time_Init first");
        return;
    }

    time_t t = time(NULL);
    struct tm tm = *localtime(&t);
    Log_Debug("%02d:%02d:%02d - ", tm.tm_hour, tm.tm_min, tm.tm_sec);
    va_list args;
    va_start(args, fmt);
    vsnprintf(_log_debug_buffer, _log_debug_buffer_size, fmt, args);
    va_end(args);
    Log_Debug(_log_debug_buffer);
}
