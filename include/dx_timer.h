/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */
   
#pragma once

#include "eventloop_timer_utilities.h"
#include "stdbool.h"
#include <applibs/eventloop.h>

typedef struct {
	void (*handler)(EventLoopTimer* timer);
	struct timespec period;
	EventLoopTimer* eventLoopTimer;
	const char* name;
} DX_TIMER;

EventLoop* dx_timerGetEventLoop(void);
bool dx_timerChange(DX_TIMER* timer, const struct timespec* period);
bool dx_timerOneShotSet(DX_TIMER* timer, const struct timespec* delay);
bool dx_timerStart(DX_TIMER* timer);
void dx_timerSetStart(DX_TIMER* timerSet[], size_t timerCount);
void dx_timerSetStop(DX_TIMER* timerSet[], size_t timerCount);
void dx_timerStop(DX_TIMER* timer);
void dx_timerEventLoopStop(void);