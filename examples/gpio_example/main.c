/*
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
 ***/

#include "hw/azure_sphere_learning_path.h" // Hardware definition

#include "dx_exit_codes.h"
#include "dx_gpio.h"
#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_utilities.h"
#include <applibs/log.h>

#define oneMS 1000000

static void BlinkLedHandler(EventLoopTimer *eventLoopTimer);
static void ButtonPressCheckHandler(EventLoopTimer *eventLoopTimer);
static void LedOffToggleHandler(EventLoopTimer *eventLoopTimer);

// GPIO Input Peripherals
static DX_GPIO buttonA = {
    .pin = BUTTON_A, .name = "buttonA", .direction = DX_INPUT, .detect = DX_GPIO_DETECT_LOW};

static DX_GPIO led = {.pin = LED2,
                      .name = "led",
                      .direction = DX_OUTPUT,
                      .initialState = GPIO_Value_Low,
                      .invertPin = true};
// Timers
static DX_TIMER buttonPressCheckTimer = {
    .period = {0, 1000000}, .name = "buttonPressCheckTimer", .handler = ButtonPressCheckHandler};

static DX_TIMER ledOffOneShotTimer = {
    .period = {0, 0}, .name = "ledOffOneShotTimer", .handler = LedOffToggleHandler};

// This is for Seeed Studio Mini as there are no onboard buttons. The LED will blink every 500 ms
static DX_TIMER blinkLedTimer = {
    .period = {0, 500 * oneMS}, .name = "blinkLedTimer", .handler = BlinkLedHandler};

// Initialize Sets
DX_GPIO *gpioSet[] = {&buttonA, &led};
DX_TIMER *timerSet[] = {&buttonPressCheckTimer, &ledOffOneShotTimer, &blinkLedTimer};

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

/// <summary>
/// Handler to check for Button Presses
/// </summary>
static void ButtonPressCheckHandler(EventLoopTimer *eventLoopTimer)
{
    static GPIO_Value_Type buttonAState;

    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

    if (dx_gpioStateGet(&buttonA, &buttonAState)) {
        dx_gpioOn(&led);
        // set oneshot timer to turn the led off after 1 second
        dx_timerOneShotSet(&ledOffOneShotTimer, &(struct timespec){1, 0});
    }
}

/// <summary>
/// Handler for dev boards with no onboard buttons - blink the LED every 500ms
/// </summary>
static void BlinkLedHandler(EventLoopTimer *eventLoopTimer)
{
    if (ConsumeEventLoopTimerEvent(eventLoopTimer) != 0) {
        dx_terminate(DX_ExitCode_ConsumeEventLoopTimeEvent);
        return;
    }

#ifdef OEM_SEEED_STUDIO_MINI

    static bool toggleLed = false;
    toggleLed = !toggleLed;
    dx_gpioStateSet(&led, toggleLed);

#endif
}

/// <summary>
///  Initialize peripherals, device twins, direct methods, timers.
/// </summary>
static void InitPeripheralsAndHandlers(void)
{
    dx_gpioSetOpen(gpioSet, NELEMS(gpioSet));
    dx_timerSetStart(timerSet, NELEMS(timerSet));
}

/// <summary>
///     Close peripherals and handlers.
/// </summary>
static void ClosePeripheralsAndHandlers(void)
{
    dx_timerSetStop(timerSet, NELEMS(timerSet));
    dx_gpioSetClose(gpioSet, NELEMS(gpioSet));
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