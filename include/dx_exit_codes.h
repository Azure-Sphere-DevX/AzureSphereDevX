/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

/// <summary>
/// Exit codes for this application. These are used for the
/// application exit code.  They must all be between zero and 255,
/// where zero is reserved for successful termination.
/// </summary>
typedef enum {
	DX_ExitCode_Success = 0,
	DX_ExitCode_TermHandler_SigTerm = 1,
	DX_ExitCode_Main_EventLoopFail = 2,
	DX_ExitCode_Missing_ID_Scope = 3,

	DX_ExitCode_Open_Peripheral = 10,
	DX_ExitCode_OpenDeviceTwin = 11,
	DX_ExitCode_AzureCloudToDeviceHandler = 12,
	DX_ExitCode_InterCoreHandler = 13,
	DX_ExitCode_ConsumeEventLoopTimeEvent = 14,
	DX_ExitCode_Gpio_Read = 15,
	DX_ExitCode_InterCoreReceiveFailed = 16,
	DX_ExitCode_PnPModelJsonMemoryAllocationFailed = 17,
	DX_ExitCode_PnPModelJsonFailed = 18,
	DX_ExitCode_IsButtonPressed = 20,
	DX_ExitCode_ButtonPressCheckHandler = 21,
	DX_ExitCode_Led2OffHandler = 22,

	DX_ExitCode_MissingRealTimeComponentId = 23,

	DX_ExitCode_Validate_Hostname_Not_Defined = 30,
    DX_ExitCode_Validate_ScopeId_Not_Defined = 31,
	DX_ExitCode_Validate_Connection_Type_Not_Defined = 32,

	DX_ExitCode_Gpio_Not_Initialized = 33,
	DX_ExitCode_Gpio_Wrong_Direction = 34,
	DX_ExitCode_Gpio_Open_Output_Failed = 35,
	DX_ExitCode_Gpio_Open_Input_Failed = 36,
	DX_ExitCode_Gpio_Open_Direction_Unknown = 37,

	DX_ExitCode_UpdateCallback_UnexpectedEvent = 40,
	DX_ExitCode_UpdateCallback_GetUpdateEvent = 41,
	DX_ExitCode_UpdateCallback_DeferEvent = 42,
	DX_ExitCode_UpdateCallback_FinalUpdate = 43,
	DX_ExitCode_UpdateCallback_UnexpectedStatus = 44,
	DX_ExitCode_SetUpSysEvent_RegisterEvent = 45,


	DX_ExitCode_Init_IoTCTimer = 100,
	DX_ExitCode_IoTCTimer_Consume = 101,

} ExitCode;