/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

// Include the DevX exit codes
#include "dx_exit_codes.h"

/// <summary>
/// Exit codes for this application.  Application exit codes 
/// must be between 1 and 149, where 0 is reserved for successful 
//  termination.  Exit codes 150 - 254 are reserved for the
//  Exit_Code enumeration located in dx_exit_codes.h.
/// </summary>
typedef enum {
	APP_ExitCode_Example = 1
} App_Exit_Code;