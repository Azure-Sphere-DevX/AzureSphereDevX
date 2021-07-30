/* Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 * This example is built on the Azure Sphere DevX library.
 *   1. DevX is an Open Source community-maintained implementation of the Azure Sphere SDK samples.
 *   2. DevX is a modular library that simplifies common development scenarios.
 *        - You can focus on your solution, not the plumbing.
 *   3. DevX documentation is maintained at https://github.com/gloveboxes/AzureSphereDevX/wiki
 *	 4. The DevX library is not a substitute for understanding the Azure Sphere SDK Samples.
 *          - https://github.com/Azure/azure-sphere-samples
 *
 * DEVELOPER BOARD SELECTION
 *
 * The following developer boards are supported.
 *
 *	 1. AVNET Azure Sphere Starter Kit.
 *   2. AVNET Azure Sphere Starter Kit Revision 2.
 *	 3. Seeed Studio Azure Sphere MT3620 Development Kit aka Reference Design Board or rdb.
 *	 4. Seeed Studio Seeed Studio MT3620 Mini Dev Board.
 *
 * ENABLE YOUR DEVELOPER BOARD
 *
 * Each Azure Sphere developer board manufacturer maps pins differently. You need to select the
 *    configuration that matches your board.
 *
 * Follow these steps:
 *
 *	 1. Open CMakeLists.txt.
 *	 2. Uncomment the set command that matches your developer board.
 *	 3. Click File, then Save to auto-generate the CMake Cache.
 *
 ************************************************************************************************/

#include "hw/azure_sphere_learning_path.h" // Hardware definition

#include "app_exit_codes.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_utilities.h"
#include <applibs/log.h>

#define ONE_MS 1000000

static void PeriodicHandler(EventLoopTimer *eventLoopTimer);
static void oneShotHandler(EventLoopTimer *eventLoopTimer);

/****************************************************************************************
 * Timer Bindings
 ****************************************************************************************/
static DX_TIMER_BINDING periodicTimer = {
    .period = {6, 0}, .name = "periodicTimer", .handler = PeriodicHandler};

// a timer with no period specified or a zero period are initialized as oneshot timers
static DX_TIMER_BINDING oneShotTimer = {.name = "oneShotTimer", .handler = oneShotHandler};

// All timers referenced in timers with be opened in the InitPeripheralsAndHandlers function
DX_TIMER_BINDING *timers[] = {&periodicTimer, &oneShotTimer};

/****************************************************************************************
 * Implementation
 ****************************************************************************************/

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
    dx_timerOneShotSet(&oneShotTimer, &(struct timespec){2, 500 * ONE_MS});
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
    dx_timerSetStart(timers, NELEMS(timers));
    // set the oneshot timer to fire
    dx_timerOneShotSet(&oneShotTimer, &(struct timespec){2, 500 * ONE_MS});
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    dx_timerSetStop(timers, NELEMS(timers));
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
    return dx_getTerminationExitCode();
}