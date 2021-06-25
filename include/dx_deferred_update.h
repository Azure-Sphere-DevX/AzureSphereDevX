#pragma once

#include "dx_terminate.h"
#include "dx_timer.h"
#include "dx_utilities.h"
#include <applibs/log.h>
#include <applibs/sysevent.h>
#include <errno.h>
#include <stdbool.h>

#ifndef DX_DEFER_UPDATE_TIME_IN_MINUTES
#define DX_DEFER_UPDATE_TIME_IN_MINUTES 15
#endif

void dx_deferredUpdateRegistion(bool (*deferredUpdateCalculateCallback)(unsigned int max_deferral_time_in_minutes),
                                void (*deferredUpdateNotificationCallback)(SysEvent_UpdateType type, const char *typeDescription,
                                                                           SysEvent_Status status, const char *statusDescription));