/* Copyright (c) Microsoft Corporation. All rights reserved.
 * Licensed under the MIT License.
 *
 *   DISCLAIMER
 *
 *   The DevX library supports the Azure Sphere Developer Learning Path:
 *
 *	   1. are built from the Azure Sphere SDK Samples at https://github.com/Azure/azure-sphere-samples
 *	   2. are not intended as a substitute for understanding the Azure Sphere SDK Samples.
 *	   3. aim to follow best practices as demonstrated by the Azure Sphere SDK Samples.
 *	   4. are provided as is and as a convenience to aid the Azure Sphere Developer Learning experience.
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

#include "declarations.h"

#include "applibs_versions.h"
#include <applibs/log.h>
#include <stdbool.h>
#include <stdio.h>
#include <time.h>

#define APPLICATION_VERSION "1.0"
const char PNP_MODEL_ID[] = "dtmi:com:example:application;1";
const char NETWORK_INTERFACE[] = "wlan0";



/****************************************************************************************
* Implement your timer code
****************************************************************************************/

/// <summary>
/// Implement your timer function
/// </summary>
static void MeasureCarbonMonoxide_handler(EventLoopTimer *eventLoopTimer) {
    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    Log_Debug("Periodic timer MeasureCarbonMonoxide_handler called\n");

    // Implement your timer function
}


/// <summary>
///  Initialize gpios, device twins, direct methods, timers.
/// </summary>
static void InitPeripheralAndHandlers(void) {{
	dx_Log_Debug_Init(Log_Debug_buffer, sizeof(Log_Debug_buffer));
	dx_initPeripheralAndHandlers();

	// Uncomment the StartWatchdog when ready for production
	// StartWatchdog();
}}

/// <summary>
///     Close Timers, GPIOs, Device Twins, Direct Methods
/// </summary>
static void ClosePeripheralAndHandlers(void) {{
	dx_azureToDeviceStop();
	dx_closePeripheralAndHandlers();
	dx_timerEventLoopStop();
}}

/// <summary>
///  Main event loop for the app
/// </summary>
int main(int argc, char* argv[]) {{
	dx_registerTerminationHandler();
	if (!dx_configParseCmdLineArguments(argc, argv, &dx_config)) {{
		return dx_getTerminationExitCode();
	}}

	InitPeripheralAndHandlers();

	// Main loop
	while (!dx_isTerminationRequired()) {{
		int result = EventLoop_Run(dx_timerGetEventLoop(), -1, true);
		// Continue if interrupted by signal, e.g. due to breakpoint being set.
		if (result == -1 && errno != EINTR) {{
			dx_terminate(DX_ExitCode_Main_EventLoopFail);
		}}
	}}

	ClosePeripheralAndHandlers();
	return dx_getTerminationExitCode();
}}