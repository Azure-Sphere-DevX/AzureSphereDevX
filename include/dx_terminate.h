/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include "dx_exit_codes.h"
#include <signal.h>
#include <stdbool.h>
#include <string.h>
#include <time.h>

// volatile sig_atomic_t terminationRequired = false;

void dx_registerTerminationHandler(void);
void dx_terminationHandler(int signalNumber);
void dx_terminate(int exitCode);
bool dx_isTerminationRequired(void);
int dx_getTerminationExitCode(void);