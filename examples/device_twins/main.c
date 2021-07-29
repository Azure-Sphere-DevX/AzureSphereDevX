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
 *experience.
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

#include "dx_azure_iot.h"
#include "dx_config.h"
#include "dx_device_twins.h"
#include "dx_exit_codes.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_utilities.h"
#include <applibs/log.h>

// https://docs.microsoft.com/en-us/azure/iot-pnp/overview-iot-plug-and-play
#define IOT_PLUG_AND_PLAY_MODEL_ID "dtmi:com:example:azuresphere:labmonitor;1"

#define NETWORK_INTERFACE "wlan0"

// Number of bytes to allocate for the JSON telemetry message for IoT Central
#define JSON_MESSAGE_BYTES 256
static char msgBuffer[JSON_MESSAGE_BYTES] = {0};

DX_USER_CONFIG dx_config;

// Forward declarations
static void report_now_handler(EventLoopTimer *eventLoopTimer);
static void dt_desired_sample_rate_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding);

/****************************************************************************************
 * Timer Bindings
 ****************************************************************************************/
static DX_TIMER_BINDING report_now_timer = {
    .period = {5, 0}, .name = "report_now_timer", .handler = report_now_handler};

// All timers in this set will be initialised in the InitPeripheralsAndHandlers function
DX_TIMER_BINDING *timerSet[] = {&report_now_timer};

/****************************************************************************************
 * Azure IoT Device Twin Bindings
 ****************************************************************************************/
static DX_DEVICE_TWIN_BINDING dt_desired_sample_rate = {.propertyName = "DesiredSampleRate",
                                                        .twinType = DX_DEVICE_TWIN_INT,
                                                        .handler = dt_desired_sample_rate_handler};

static DX_DEVICE_TWIN_BINDING dt_reported_temperature = {.propertyName = "ReportedTemperature",
                                                         .twinType = DX_DEVICE_TWIN_FLOAT};

static DX_DEVICE_TWIN_BINDING dt_reported_humidity = {.propertyName = "ReportedHumidity",
                                                      .twinType = DX_DEVICE_TWIN_DOUBLE};

static DX_DEVICE_TWIN_BINDING dt_reported_utc = {.propertyName = "ReportedUTC",
                                                 .twinType = DX_DEVICE_TWIN_STRING};

// All device twins listed in device_twin_bindings will be subscribed to in
// the InitPeripheralsAndHandlers function
DX_DEVICE_TWIN_BINDING *device_twin_bindings[] = {&dt_desired_sample_rate, &dt_reported_temperature,
                                                  &dt_reported_humidity, &dt_reported_utc};

// Implementation

static void report_now_handler(EventLoopTimer *eventLoopTimer)
{
    float temperature = 25.05f;
    double humidity = 60.25;

    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    // Update twin with current UTC (Universal Time Coordinate) in ISO format
    dx_deviceTwinReportValue(&dt_reported_utc, dx_getCurrentUtc(msgBuffer, sizeof(msgBuffer)));

    // The type passed in must match the Divice Twin Type DX_DEVICE_TWIN_FLOAT
    dx_deviceTwinReportValue(&dt_reported_temperature, &temperature);

    // The type passed in must match the Divice Twin Type DX_DEVICE_TWIN_DOUBLE
    dx_deviceTwinReportValue(&dt_reported_humidity, &humidity);
}

static void dt_desired_sample_rate_handler(DX_DEVICE_TWIN_BINDING *deviceTwinBinding)
{
    // validate data is sensible range before applying
    if (deviceTwinBinding->twinType == DX_DEVICE_TWIN_INT && *(int *)deviceTwinBinding->propertyValue >= 0 &&
        *(int *)deviceTwinBinding->propertyValue <= 120) {

        dx_timerChange(&report_now_timer,
                       &(struct timespec){*(int *)deviceTwinBinding->propertyValue, 0});

        dx_deviceTwinAckDesiredValue(deviceTwinBinding, deviceTwinBinding->propertyValue,
                                     DX_DEVICE_TWIN_RESPONSE_COMPLETED);

    } else {
        dx_deviceTwinAckDesiredValue(deviceTwinBinding, deviceTwinBinding->propertyValue,
                                     DX_DEVICE_TWIN_RESPONSE_ERROR);
    }

    /*	Casting device twin state examples

            float value = *(float*)deviceTwinBinding->propertyValue;
            double value = *(double*)deviceTwinBinding->propertyValue;
            int value = *(int*)deviceTwinBinding->propertyValue;
            bool value = *(bool*)deviceTwinBinding->propertyValue;
            char* value = (char*)deviceTwinBinding->propertyValue;
    */
}

/// <summary>
///  Initialize peripherals, device twins, direct methods, timers.
/// </summary>
static void InitPeripheralsAndHandlers(void)
{
    dx_azureConnect(&dx_config, NETWORK_INTERFACE, IOT_PLUG_AND_PLAY_MODEL_ID);
    dx_timerSetStart(timerSet, NELEMS(timerSet));
    dx_deviceTwinSubscribe(device_twin_bindings, NELEMS(device_twin_bindings));
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    dx_deviceTwinUnsubscribe();
    dx_timerSetStop(timerSet, NELEMS(timerSet));
    dx_timerEventLoopStop();
}

int main(int argc, char *argv[])
{
    dx_registerTerminationHandler();
    if (!dx_configParseCmdLineArguments(argc, argv, &dx_config)) {
        return dx_getTerminationExitCode();
    }
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