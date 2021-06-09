/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include <applibs/application.h>
#include <applibs/log.h>
#include <applibs/networking.h>
#include <errno.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <math.h>

#define NELEMS(x)  (sizeof(x) / sizeof((x)[0]))

bool dx_isDeviceAuthReady(void);
bool dx_isNetworkConnected(const char* networkInterface);
bool dx_isNetworkReady(void);
bool dx_isStringNullOrEmpty(const char* string);
bool dx_startThreadDetached(void* (daemon)(void*), void* arg, char* daemon_name);
char* dx_getCurrentUtc(char* buffer, size_t bufferSize);
int dx_stringEndsWith(const char* str, const char* suffix);
int64_t dx_getNowMilliseconds(void);
void dx_Log_Debug(char *fmt, ...);
void dx_Log_Debug_Init(const char *buffer, size_t buffer_size);