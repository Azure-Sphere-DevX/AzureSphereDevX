/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include "dx_terminate.h"
#include "eventloop_timer_utilities.h"
#include "stdbool.h"
#include <applibs/eventloop.h>
#include <applibs/log.h>

#define DX_TIMER_HANDLER(name)                            \
    void name(EventLoopTimer *eventLoopTimer)                    \
    {                                                            \
        if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0)     \
        {                                                        \
            dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent); \
            return;                                              \
        }

#define DX_TIMER_HANDLER_END \
    }

#define DX_DECLARE_TIMER_HANDLER(name) void name(EventLoopTimer *eventLoopTimer)

typedef struct {
    void (*handler)(EventLoopTimer *timer);
    struct timespec period;
    struct timespec *delay;
    struct timespec *repeat;
    EventLoopTimer *eventLoopTimer;
    const char *name;
} DX_TIMER_BINDING;

EventLoop *dx_timerGetEventLoop(void);
bool dx_timerChange(DX_TIMER_BINDING *timer, const struct timespec *period);
bool dx_timerOneShotSet(DX_TIMER_BINDING *timer, const struct timespec *delay);
bool dx_timerStart(DX_TIMER_BINDING *timer);
void dx_timerSetStart(DX_TIMER_BINDING *timerSet[], size_t timerCount);
void dx_timerSetStop(DX_TIMER_BINDING *timerSet[], size_t timerCount);
void dx_timerStop(DX_TIMER_BINDING *timer);
void dx_timerEventLoopStop(void);