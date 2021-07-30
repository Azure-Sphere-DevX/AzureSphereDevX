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

#include "dx_azure_iot.h"
#include "dx_config.h"
#include "dx_direct_methods.h"
#include "app_exit_codes.h"
#include "dx_gpio.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_utilities.h"
#include <applibs/log.h>
#include <applibs/powermanagement.h>

// https://docs.microsoft.com/en-us/azure/iot-pnp/overview-iot-plug-and-play
#define IOT_PLUG_AND_PLAY_MODEL_ID "dtmi:com:example:azuresphere:labmonitor;1"
#define NETWORK_INTERFACE "wlan0"
#define ONE_MS 1000000

// Forward declarations
static DX_DIRECT_METHOD_RESPONSE_CODE LightControlHandler(
    JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static DX_DIRECT_METHOD_RESPONSE_CODE RestartDeviceHandler(
    JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg);
static void DelayRestartDeviceTimerHandler(EventLoopTimer *eventLoopTimer);
static void LedOffToggleHandler(EventLoopTimer *eventLoopTimer);

// Variables
DX_USER_CONFIG dx_config;

/****************************************************************************************
 * GPIO Peripherals
 ****************************************************************************************/
static DX_GPIO_BINDING led = {.pin = LED2,
                              .name = "led",
                              .direction = DX_OUTPUT,
                              .initialState = GPIO_Value_Low,
                              .invertPin = true};

// All GPIOs added to gpio_set will be opened in InitPeripheralsAndHandlers
DX_GPIO_BINDING *gpio_set[] = {&led};

/****************************************************************************************
 * Timer Bindings
 ****************************************************************************************/
static DX_TIMER_BINDING led_off_oneshot_timer = {
    .period = {0, 0}, .name = "led_off_oneshot_timer", .handler = LedOffToggleHandler};

static DX_TIMER_BINDING restart_device_oneshot_timer = {.period = {0, 0},
                                                        .name = "restart_device_oneshot_timer",
                                                        .handler = DelayRestartDeviceTimerHandler};

// All timers referenced in timers with be opened in the InitPeripheralsAndHandlers function
DX_TIMER_BINDING *timers[] = {&restart_device_oneshot_timer, &led_off_oneshot_timer};

/****************************************************************************************
 * Azure IoT Direct Method Bindings
 ****************************************************************************************/
static DX_DIRECT_METHOD_BINDING dm_light_control = {.methodName = "LightControl",
                                                    .handler = LightControlHandler};

static DX_DIRECT_METHOD_BINDING dm_restart_device = {.methodName = "RestartDevice",
                                                     .handler = RestartDeviceHandler};

// All direct methods referenced in direct_method_bindings will be subscribed to in
// the InitPeripheralsAndHandlers function
DX_DIRECT_METHOD_BINDING *direct_method_bindings[] = {&dm_restart_device, &dm_light_control};

/****************************************************************************************
 * Implementation
 ****************************************************************************************/

/// <summary>
/// One shot timer handler to turn off Alert LED
/// </summary>
static void LedOffToggleHandler(EventLoopTimer *eventLoopTimer)
{
    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }
    dx_gpioOff(&led);
}

static inline bool in_range(int low, int high, int x)
{
    return low <= x && x <= high;
}

// Direct method name = LightControl, json payload = {"State": true, "Duration":2} or {"State":
// false, "Duration":2}
static DX_DIRECT_METHOD_RESPONSE_CODE LightControlHandler(
    JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg)
{
    char duration_str[] = "Duration";
    char state_str[] = "State";
    bool requested_state;
    int requested_duration_seconds;

    JSON_Object *jsonObject = json_value_get_object(json);
    if (jsonObject == NULL) {
        return DX_METHOD_FAILED;
    }

    // check JSON properties sent through are the correct type
    if (!json_object_has_value_of_type(jsonObject, duration_str, JSONNumber) ||
        !json_object_has_value_of_type(jsonObject, state_str, JSONBoolean)) {
        return DX_METHOD_FAILED;
    }

    requested_state = (bool)json_object_get_boolean(jsonObject, state_str);
    Log_Debug("State %d \n", requested_state);

    requested_duration_seconds = (int)json_object_get_number(jsonObject, duration_str);
    Log_Debug("Duration %d \n", requested_duration_seconds);

    if (!in_range(1, 120, requested_duration_seconds)) {
        return DX_METHOD_FAILED;
    }

    dx_gpioStateSet(&led, requested_state);

    // If the request was to turn on then turn off after duration seconds
    if (required_argument) {
        dx_timerOneShotSet(&led_off_oneshot_timer,
                           &(struct timespec){requested_duration_seconds, 0});
    }

    return DX_METHOD_SUCCEEDED;
}

/// <summary>
/// Restart the Device
/// </summary>
static void DelayRestartDeviceTimerHandler(EventLoopTimer *eventLoopTimer)
{
    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }
    PowerManagement_ForceSystemReboot();
}

/// <summary>
/// Start Device Power Restart Direct Method 'ResetMethod' integer seconds eg 5
/// </summary>
static DX_DIRECT_METHOD_RESPONSE_CODE RestartDeviceHandler(
    JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg)
{
    // Allocate and initialize a response message buffer. The
    // calling function is responsible for the freeing memory
    const size_t responseLen = 100;
    static struct timespec period;

    *responseMsg = (char *)malloc(responseLen);
    memset(*responseMsg, 0, responseLen);

    if (json_value_get_type(json) != JSONNumber) {
        return DX_METHOD_FAILED;
    }

    int seconds = (int)json_value_get_number(json);

    // leave enough time for the device twin dt_reportedRestartUtc
    // to update before restarting the device
    if (seconds > 2 && seconds < 10) {
        // Create Direct Method Response
        snprintf(*responseMsg, responseLen, "%s called. Restart in %d seconds",
                 directMethodBinding->methodName, seconds);

        // Set One Shot DX_TIMER_BINDING
        period = (struct timespec){.tv_sec = seconds, .tv_nsec = 0};
        dx_timerOneShotSet(&restart_device_oneshot_timer, &period);

        return DX_METHOD_SUCCEEDED;
    } else {
        snprintf(*responseMsg, responseLen, "%s called. Restart Failed. Seconds out of range: %d",
                 directMethodBinding->methodName, seconds);

        return DX_METHOD_FAILED;
    }
}

/// <summary>
///  Initialize peripherals, device twins, direct methods, timers.
/// </summary>
static void InitPeripheralsAndHandlers(void)
{
    dx_azureConnect(&dx_config, NETWORK_INTERFACE, IOT_PLUG_AND_PLAY_MODEL_ID);
    dx_timerSetStart(timers, NELEMS(timers));
    dx_gpioSetOpen(gpio_set, NELEMS(gpio_set));
    dx_directMethodSubscribe(direct_method_bindings, NELEMS(direct_method_bindings));
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    dx_timerSetStop(timers, NELEMS(timers));
    dx_gpioSetClose(gpio_set, NELEMS(gpio_set));
    dx_directMethodUnsubscribe();
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