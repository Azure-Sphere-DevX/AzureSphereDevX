/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include "dx_terminate.h"
#include "dx_timer.h"
#include "eventloop_timer_utilities.h"
#include <applibs/application.h>
#include <applibs/eventloop.h>
#include <applibs/log.h>
#include <ctype.h>
#include <errno.h>
#include <stdbool.h>
#include <stdio.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/time.h>
#include <unistd.h>

typedef struct {
	char* rtAppComponentId;
	int sockFd;
	bool nonblocking_io;
	void (*interCoreCallback)(void*, ssize_t message_size);
	void* intercore_recv_block;
	size_t intercore_recv_block_length;
} INTERCORE_CONTEXT;

bool dx_interCoreSendMessage(INTERCORE_CONTEXT* intercore_ctx, void* control_block, size_t message_length);
bool dx_interCoreCommunicationsEnable(INTERCORE_CONTEXT* intercore_ctx);