/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "dx_timer.h"

static EventLoop *eventLoop = NULL;

EventLoop *dx_timerGetEventLoop(void)
{
    if (eventLoop == NULL) {
        eventLoop = EventLoop_Create();
    }
    return eventLoop;
}

bool dx_timerChange(DX_TIMER_BINDING *timer, const struct timespec *period)
{
    if (timer->eventLoopTimer == NULL) {
        return false;
    }
    timer->period.tv_nsec = period->tv_nsec;
    timer->period.tv_sec = period->tv_sec;
    int result = SetEventLoopTimerPeriod(timer->eventLoopTimer, period);

    return result == 0 ? true : false;
}

bool dx_timerStart(DX_TIMER_BINDING *timer)
{
    EventLoop *eventLoop = dx_timerGetEventLoop();
    if (eventLoop == NULL) {
        return false;
    }

    // Already initialized
    if (timer->eventLoopTimer != NULL) {
        return true;
    }

    if (timer->delay != NULL && timer->repeat != NULL) {
        Log_Debug("Can't specify both a timer delay and a repeat period\n");
        dx_terminate(DX_ExitCode_Create_Timer_Failed);
        return false;
    }

    // is this a oneshot timer
    if (timer->delay != NULL) {
        timer->eventLoopTimer = CreateEventLoopDisarmedTimer(eventLoop, timer->handler);
        if (timer->eventLoopTimer == NULL) {
            dx_terminate(DX_ExitCode_Create_Timer_Failed);
            return false;
        }

        if (SetEventLoopTimerOneShot(timer->eventLoopTimer, timer->delay) != 0) {
            dx_terminate(DX_ExitCode_Create_Timer_Failed);
            return false;
        }
        return true;
    }

    // is this a repeating timer
    if (timer->repeat != NULL) {
        timer->eventLoopTimer = CreateEventLoopPeriodicTimer(eventLoop, timer->handler, timer->repeat);
        if (timer->eventLoopTimer == NULL) {
            dx_terminate(DX_ExitCode_Create_Timer_Failed);
            return false;
        }
        return true;
    }

    // support for initial timer implementation
    // if timer period is zero then create a disarmed timer
    if (timer->period.tv_nsec == 0 && timer->period.tv_sec == 0) { // Set up a disabled DX_TIMER_BINDING for oneshot or change timer
        timer->eventLoopTimer = CreateEventLoopDisarmedTimer(eventLoop, timer->handler);
        if (timer->eventLoopTimer == NULL) {
            dx_terminate(DX_ExitCode_Create_Timer_Failed);
            return false;
        }
    } else {
        timer->eventLoopTimer = CreateEventLoopPeriodicTimer(eventLoop, timer->handler, &timer->period);
        if (timer->eventLoopTimer == NULL) {
            dx_terminate(DX_ExitCode_Create_Timer_Failed);
            return false;
        }
    }

    return true;
}

void dx_timerStop(DX_TIMER_BINDING *timer)
{
    if (timer->eventLoopTimer != NULL) {
        DisposeEventLoopTimer(timer->eventLoopTimer);
        timer->eventLoopTimer = NULL;
    }
}

void dx_timerSetStart(DX_TIMER_BINDING *timerSet[], size_t timerCount)
{
    for (int i = 0; i < timerCount; i++) {
        if (!dx_timerStart(timerSet[i])) {
            break;
        };
    }
}

void dx_timerSetStop(DX_TIMER_BINDING *timerSet[], size_t timerCount)
{
    for (int i = 0; i < timerCount; i++) {
        dx_timerStop(timerSet[i]);
    }
}

void dx_timerEventLoopStop(void)
{
    EventLoop *eventLoop = dx_timerGetEventLoop();
    if (eventLoop != NULL) {
        EventLoop_Close(eventLoop);
    }
}

bool dx_timerOneShotSet(DX_TIMER_BINDING *timer, const struct timespec *period)
{
    if (timer->eventLoopTimer == NULL || timer->handler == NULL || period == NULL) {
        return false;
    }

    if (SetEventLoopTimerOneShot(timer->eventLoopTimer, period) != 0) {
        return false;
    }

    return true;
}