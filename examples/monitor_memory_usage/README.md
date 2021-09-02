# Azure IoT Starter Project (empty)

This example implements a feature to monitor the Azure Sphere High Level memory usage using the Applications_GetPeakUserModeMemoryUsageInKB() call.  The application uses a timer and handler to monitor the memory usage, and a direct method that leaks memory to test the feature.

When a higher memory usage value is detected the application sends up telemetry in the format: `{"MemoryHighWaterMark":180}`

      #define MONITOR_PERIOD 30
      static DX_TIMER_BINDING tmr_monitor_memory = {.period = {MONITOR_PERIOD, 0}, .name = "tmr_monitor_memory", .handler = monitor_memory_handler};

      static DX_DIRECT_METHOD_BINDING dm_memory_leak = {.methodName = "MemoryLeak", .handler = MemoryLeakHandler};
      // The payload for the direct method is: {"LeakSize": <integer in KB> }


## Config app_manifest.json sample

1. Set ID Scope
1. Set Allowed connections
1. Set DeviceAuthentication

For more information refer to:

1. [Adding the Azure Sphere DevX library](https://github.com/gloveboxes/AzureSphereDevX/wiki/Adding-the-DevX-Library)
1. [Azure Messaging](https://github.com/gloveboxes/AzureSphereDevX/wiki/IoT-Hub-Sending-messages)
1. [Device Twins](https://github.com/gloveboxes/AzureSphereDevX/wiki/IoT-Hub-Device-Twins)
1. [Direct Methods](https://github.com/gloveboxes/AzureSphereDevX/wiki/IoT-Hub-Direct-Methods)
1. [GPIO](https://github.com/gloveboxes/AzureSphereDevX/wiki/Working-with-GPIO)
