/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#pragma once

//#include "epoll_timerfd_utilities.h"
#include "parson.h"
#include <applibs/gpio.h>
#include <applibs/log.h>
#include <errno.h>
#include <stdbool.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include "dx_terminate.h"

typedef enum
{
	DX_DIRECTION_UNKNOWN,
	DX_INPUT,
	DX_OUTPUT
} DX_GPIO_DIRECTION;

typedef enum {
	DX_GPIO_DETECT_LOW,
	DX_GPIO_DETECT_HIGH,
	DX_GPIO_DETECT_BOTH
} DX_GPIO_INPUT_DETECT;

struct _dx_gpio
{
	int fd;
	int pin;
	GPIO_Value initialState;
	bool invertPin;
	DX_GPIO_INPUT_DETECT detect;
	char* name;
	DX_GPIO_DIRECTION direction;
	bool opened;
};

typedef struct _dx_gpio DX_GPIO;

bool dx_gpioOpen(DX_GPIO* gpio);
bool dx_gpioStateGet(DX_GPIO* gpio, GPIO_Value_Type* oldState);
void dx_gpioClose(DX_GPIO* gpio);
void dx_gpioOff(DX_GPIO* gpio);
void dx_gpioOn(DX_GPIO* gpio);
void dx_gpioSetClose(DX_GPIO** gpioSet, size_t gpioSetCount);
void dx_gpioSetOpen(DX_GPIO** gpioSet, size_t gpioSetCount);
void dx_gpioStateSet(DX_GPIO* gpio, bool state);
