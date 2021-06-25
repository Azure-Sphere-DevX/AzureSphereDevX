/* Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 *   DISCLAIMER
 *
 *   The DevX library supports the Azure Sphere Developer Learning Path:
 *
 *	   1. are built from the Azure Sphere SDK Samples at
 *          https://github.com/Azure/azure-sphere-samples
 *	   2. are not intended as a substitute for understanding the Azure Sphere SDK Samples.
 *	   3. aim to follow best practices as demonstrated by the Azure Sphere SDK Samples.
 *	   4. are provided as is and as a convenience to aid the Azure Sphere Developer Learning
 *          experience.
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
 *      configuration that matches your board.
 *
 *   Follow these steps:
 *
 *	   1. Open CMakeLists.txt.
 *	   2. Uncomment the set command that matches your developer board.
 *	   3. Click File, then Save to save the CMakeLists.txt file which will auto generate the
 *          CMake Cache.
 *
 ************************************************************************************************/

#include "hw/azure_sphere_learning_path.h" // Hardware definition

#include "dx_exit_codes.h"
#include "dx_terminate.h"
#include "dx_deferred_update.h"
#include <applibs/log.h>

/****************************************************************************************
 * Implementation
 ****************************************************************************************/


/// <summary>
/// Algorithm to determine if a deferred update can proceed
/// </summary>
/// <param name="max_deferral_time_in_minutes">The maximum number of minutes you can defer</param>
/// <returns>Return 0 to start update, return greater than zero to defer</returns>
static uint32_t DeferredUpdateCalculate(unsigned int max_deferral_time_in_minutes)
{
    // Make deferral update decision
    // Examples include: 
    // - Update overnight - could be as simple as UTC +10hrs
    // - Is it dark
    // - Is the device busy at the moment
    // - Orientation of the device

    time_t now = time(NULL);
    struct tm *t = gmtime(&now);

    // This would work well enough if all devices are in Australia
    // Otherwise get local time using a geo location look up
    // https://github.com/Azure/azure-sphere-gallery/tree/main/SetTimeFromLocation

    // UTC +10 is good for Australia
    t->tm_hour += 10;
    t->tm_hour = t->tm_hour % 24;

    // Return zero to allow update, greater than zero to defer the update
    if (t->tm_hour >= 1 && t->tm_hour <= 5) {
        return 0;
    } else {
        return 15; // defer update for 15 minutes
    }
}

/// <summary>
/// Callback notification on deferred update state
/// </summary>
/// <param name="type"></param>
/// <param name="typeDescription"></param>
/// <param name="status"></param>
/// <param name="statusDescription"></param>
static void DeferredUpdateNotification(SysEvent_UpdateType type, const char *typeDescription,
                                                  SysEvent_Status status, const char *statusDescription)
{
    Log_Debug("Type: %s, Status: %s", typeDescription, statusDescription);
}


/// <summary>
///  Initialize peripherals, device twins, direct methods, timers.
/// </summary>
static void InitPeripheralsAndHandlers(void)
{
    dx_deferredUpdateRegistration(DeferredUpdateCalculate, DeferredUpdateNotification);
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
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