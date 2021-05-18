/* Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 *   DEVELOPER BOARD SELECTION
 *
 *   The following developer boards are supported.
 *
 *	   1. AVNET Azure Sphere Starter Kit.
 *     2. AVNET Azure Sphere Starter Kit Revision 2.
 *	   3. Seeed Studio Azure Sphere MT3620 Development Kit aka Reference Design Board or rdb.
 *	   4. Seeed Studio Seeed Studio MT3620 Mini Dev Board.
 *
 *   ENABLE YOUR DEVELOPER BOARD
 *
 *   Each Azure Sphere developer board manufacturer maps pins differently. You need to select the
 *configuration that matches your board.
 *
 *   Follow these steps:
 *
 *	   1. Open CMakeLists.txt.
 *	   2. Uncomment the set command that matches your developer board.
 *	   3. Click File, then Save to save the CMakeLists.txt file which will auto generate the
 *CMake Cache.
 **/

#include "hw/azure_sphere_learning_path.h" // Hardware definition

#include "dx_exit_codes.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_utilities.h"
#include <applibs/log.h>

#define oneMS 1000000

static void PeriodicHandler(EventLoopTimer *eventLoopTimer);
static void oneShotHandler(EventLoopTimer *eventLoopTimer);

// Timers
static DX_TIMER periodicTimer = {
    .period = {6, 0}, .name = "periodicTimer", .handler = PeriodicHandler};

// a timer with no period specified or a zero period are initialized as oneshot timers
static DX_TIMER oneShotTimer = {.name = "oneShotTimer", .handler = oneShotHandler};

// Initialize Sets
DX_TIMER *timerSet[] = {&periodicTimer, &oneShotTimer};

/// <summary>
/// One shot timer handler example
/// </summary>
static void oneShotHandler(EventLoopTimer *eventLoopTimer)
{
    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }
    Log_Debug("Hello from the oneshot timer. Reloading the oneshot timer period\n");
    // The oneshot timer will trigger again in 2.5 seconds
    dx_timerOneShotSet(&oneShotTimer, &(struct timespec){2, 500 * oneMS});
}

/// <summary>
/// Periodic timer handler example
/// </summary>
static void PeriodicHandler(EventLoopTimer *eventLoopTimer)
{
    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }
    Log_Debug("Hello from the periodic timer called every 6 seconds\n");
}

/// <summary>
///  Initialize peripherals, device twins, direct methods, timers.
/// </summary>
static void InitPeripheralsAndHandlers(void)
{
    dx_timerSetStart(timerSet, NELEMS(timerSet));
    // set the oneshot timer to fire
    dx_timerOneShotSet(&oneShotTimer, &(struct timespec){2, 500 * oneMS});
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    dx_timerSetStop(timerSet, NELEMS(timerSet));
    dx_timerEventLoopStop();
}

int main(void)
{
    dx_registerTerminationHandler();
    InitPeripheralsAndHandlers();

    // Main loop
    while (!dx_isTerminationRequired()) {
        int result = EventLoop_Run(dx_timerGetEventLoop(), -1, true);
        // Continue if interrupted by signal, e.g. due to breakpoint being set.
        if (result == -1 && errno != EINTR) {
            dx_terminate(DX_ExitCode_Main_EventLoopFail);
        }
    }

    ClosePeripheralsAndHandlers();
    Log_Debug("Application exiting.\n");
    return dx_getTerminationExitCode();
}