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
 *  Application Notes
 *   This example application monitors the application's memory usage (every 30 seconde).  
 *   If the memory usage grows a telemetry message is sent with the new memory value.  
 *   There is also a direct method "MemoryLeak": {"LeakSize": <integer in KB> } that can 
 *   be used to exercise the feature.  
 * 
 *   Developers should read the Microsoft documentation on memory usage
 *   https://docs.microsoft.com/en-us/azure-sphere/app-development/application-memory-usage?tabs=windows&pivots=cli
 * 
 ************************************************************************************************/

#include "main.h"

/****************************************************************************************
 * Implementation
 ****************************************************************************************/

// Direct method name = MemoryLeak, json payload = {"LeakSize": <integer in KB> }
static DX_DIRECT_METHOD_RESPONSE_CODE MemoryLeakHandler(JSON_Value *json, DX_DIRECT_METHOD_BINDING *directMethodBinding, char **responseMsg)
{
    char state_str[] = "LeakSize";
    int memoryToLeak = 0; 

    JSON_Object *jsonObject = json_value_get_object(json);
    if (jsonObject == NULL) {
        return DX_METHOD_FAILED;
    }

    // Pull and validate the payload from the direct method call
    memoryToLeak = (int)json_object_get_number(jsonObject, state_str);
    if(!IN_RANGE(memoryToLeak, MIN_MEMORY_LEAK, MAX_MEMORY_LEAK)){
        return DX_METHOD_FAILED;
    }
    
    Log_Debug("Leaking %dKB of memory!\n", memoryToLeak);
    
    // Allocate memory, but don't release it!
    if(malloc((size_t)(memoryToLeak * 1024)) != NULL){
        return DX_METHOD_SUCCEEDED;
    }
    
    Log_Debug("Malloc() failed!\n");
    return DX_METHOD_FAILED;
}

// Handler
/*
* Note from the Azure Sphere Documentation on memory usage:
* https://docs.microsoft.com/en-us/azure-sphere/app-development/application-memory-usage?tabs=windows&pivots=cli#determine-run-time-application-ram-usage
* These functions return the memory usage as seen by the OS. Currently, the 
* freeing of memory by an application for allocations on the user heap is not 
* reported by these functions. The memory will be returned to the malloc library
* for future use but the statistics reported by the OS remain unchanged unless 
* the memory was allocated and freed by the OS itself. An example would be 
* allocating memory for a socket. Therefore, these functions are useful for 
* understanding worst-case scenarios to help your application operate 
* conservatively for maximum reliability. Values are approximate and may vary 
* across OS versions.
*/
static void monitor_memory_handler(EventLoopTimer *eventLoopTimer)
{
    static size_t memoryHighWaterMark = 0;

    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopMemoryMonitor);
        return;
    }

#ifdef USE_AVNET_IOTCONNECT
    if (dx_isAvnetConnected()) {
#else
    if (dx_isAzureConnected()) {
#endif 
        size_t newMemoryHighWaterMark = Applications_GetPeakUserModeMemoryUsageInKB();
        if(newMemoryHighWaterMark > memoryHighWaterMark){
           
            memoryHighWaterMark = newMemoryHighWaterMark;
            Log_Debug("New Memory High Water Mark: %d Kb\n", newMemoryHighWaterMark);
            
            // Serialize telemetry as JSON
#ifdef USE_AVNET_IOTCONNECT            
            bool serialization_result = dx_avnetJsonSerialize(msgBuffer, sizeof(msgBuffer), 1, 
                                        DX_JSON_INT, "MemoryHighWaterMark", newMemoryHighWaterMark); 
#else
            bool serialization_result = dx_jsonSerialize(msgBuffer, sizeof(msgBuffer), 1, 
                                        DX_JSON_INT, "MemoryHighWaterMark", newMemoryHighWaterMark); 
#endif 
            if (serialization_result) {
                Log_Debug("%s\n", msgBuffer);
                dx_azurePublish(msgBuffer, strlen(msgBuffer), memoryMessageProperties, NELEMS(memoryMessageProperties), &contentProperties);

            } else {
                Log_Debug("JSON Serialization failed: Buffer too small\n");
            }
        }
    }
}

/// <summary>
///  Initialize peripherals, device twins, direct methods, timer_bindings.
/// </summary>
static void InitPeripheralsAndHandlers(void)
{
#ifdef USE_AVNET_IOTCONNECT
    dx_avnetConnect(&dx_config, NETWORK_INTERFACE);
#else     
    dx_azureConnect(&dx_config, NETWORK_INTERFACE, IOT_PLUG_AND_PLAY_MODEL_ID);
#endif     
    
    dx_gpioSetOpen(gpio_bindings, NELEMS(gpio_bindings));
    dx_timerSetStart(timer_bindings, NELEMS(timer_bindings));
    dx_deviceTwinSubscribe(device_twin_bindings, NELEMS(device_twin_bindings));
    dx_directMethodSubscribe(direct_method_bindings, NELEMS(direct_method_bindings));

    // TODO: Update this call with a function pointer to a handler that will receive connection status updates
    // see the azure_end_to_end example for an example
    // dx_azureRegisterConnectionChangedNotification(NetworkConnectionState);
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    dx_timerSetStop(timer_bindings, NELEMS(timer_bindings));
    dx_deviceTwinUnsubscribe();
    dx_directMethodUnsubscribe();
    dx_gpioSetClose(gpio_bindings, NELEMS(gpio_bindings));
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
    Log_Debug("Application exiting.\n");
    return dx_getTerminationExitCode();
}