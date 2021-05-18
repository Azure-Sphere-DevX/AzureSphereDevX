#pragma once

typedef enum
{
	LP_IC_UNKNOWN,
	LP_IC_ECHO
} LP_INTER_CORE_CMD;

typedef struct
{
	LP_INTER_CORE_CMD cmd;
	int msgId;
	char message[64];
} LP_INTER_CORE_BLOCK;
