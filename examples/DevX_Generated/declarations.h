#include "hw/azure_sphere_learning_path.h" // Hardware definition

#include "dx_azure_iot.h"
#include "dx_config.h"
#include "dx_exit_codes.h"
#include "dx_gpio.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_version.h"

#include <time.h>

DX_USER_CONFIG dx_config;   // cmd args from app_manifest.json

extern const char PNP_MODEL_ID[];
extern const char NETWORK_INTERFACE[];

#define ONE_MS 1000000		// used to simplify timer defn.
#define Log_Debug(f_, ...) dx_Log_Debug((f_), ##__VA_ARGS__)
static char Log_Debug_buffer[128];

/****************************************************************************************
 * Watchdog support
 ****************************************************************************************/
const struct itimerspec watchdogInterval = { { 60, 0 },{ 60, 0 } };
timer_t watchdogTimer;

/****************************************************************************************
 * Forward declarations
 ****************************************************************************************/
static void MeasureCarbonMonoxide_handler(EventLoopTimer *eventLoopTimer);

/****************************************************************************************
* Timer Bindings
****************************************************************************************/
static DX_TIMER_BINDING  tmr_MeasureCarbonMonoxide = { .period = { 5, 0 }, .name = "MeasureCarbonMonoxide", .handler = MeasureCarbonMonoxide_handler };

// All timers referenced in timer_bindings with be opened in the InitPeripheralsAndHandlers function
#define DECLARE_DX_TIMER_BINDINGS
static DX_TIMER_BINDING *timer_bindings[] = {  &tmr_MeasureCarbonMonoxide };


/****************************************************************************************
* Initialise bindings
****************************************************************************************/

static void dx_initPeripheralAndHandlers(void)
{
#if defined(DX_PUBLISH_TELEMETRY) || defined(DECLARE_DX_DEVICE_TWIN_BINDINGS) || defined(DECLARE_DX_DIRECT_METHOD_BINDINGS)
    dx_azureConnect(&dx_config, NETWORK_INTERFACE, PNP_MODEL_ID);
#endif
#if defined(DECLARE_DX_GPIO_BINDINGS)
    dx_gpioSetOpen(gpio_bindings, NELEMS(gpio_bindings));
#endif
#if defined(DECLARE_DX_DEVICE_TWIN_BINDINGS)
    dx_deviceTwinSubscribe(device_twin_bindings, NELEMS(device_twin_bindings));
#endif
#if defined(DECLARE_DX_DIRECT_METHOD_BINDINGS)
    dx_directMethodSubscribe(direct_method_bindings, NELEMS(direct_method_bindings));
#endif
#if defined(DECLARE_DX_TIMER_BINDINGS)
    dx_timerSetStart(timer_bindings, NELEMS(timer_bindings));
#endif
}

static void dx_closePeripheralAndHandlers(void){
#if defined(DECLARE_DX_TIMER_BINDINGS)
	dx_timerSetStop(timer_bindings, NELEMS(timer_bindings));
#endif
#if defined(DECLARE_DX_GPIO_BINDINGS)
	dx_gpioSetClose(gpio_bindings, NELEMS(gpio_bindings));
#endif
#ifdef DECLARE_DX_DEVICE_TWIN_BINDINGS
	dx_deviceTwinUnsubscribe();
#endif
#ifdef DECLARE_DX_DIRECT_METHOD_BINDINGS
	dx_directMethodUnsubscribe();
#endif
}
