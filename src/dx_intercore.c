/* Copyright (c) Microsoft Corporation. All rights reserved.
   Licensed under the MIT License. */

#include "dx_intercore.h"

static void SocketEventHandler(EventLoop* el, int fd, EventLoop_IoEvents events, void* context);
static bool ProcessMsg(INTERCORE_CONTEXT* intercore_ctx);
static EventRegistration* socketEventReg = NULL;

static bool initialise_inter_core_communications(INTERCORE_CONTEXT *intercore_ctx) {
	if (intercore_ctx->sockFd != -1) // Already initialised
	{
		return true;
	}

	if (intercore_ctx->rtAppComponentId == NULL) {
		dx_terminate(DX_ExitCode_MissingRealTimeComponentId);
		return false;
	}

	// Open connection to real-time capable application.
	intercore_ctx->sockFd = Application_Connect(intercore_ctx->rtAppComponentId);
	if (intercore_ctx->sockFd == -1) {
		//Log_Debug("ERROR: Unable to create socket: %d (%s)\n", errno, strerror(errno));
		return false;
	}

	// Set timeout, to handle case where real-time capable application does not respond.
	static const struct timeval recvTimeout = { .tv_sec = 5, .tv_usec = 0 };
	int result = setsockopt(intercore_ctx->sockFd, SOL_SOCKET, SO_RCVTIMEO, &recvTimeout, sizeof(recvTimeout));
	if (result == -1) {
		Log_Debug("ERROR: Unable to set socket timeout: %d (%s)\n", errno, strerror(errno));
		return false;
	}

	// Register handler for incoming messages from real-time capable application.
	socketEventReg = EventLoop_RegisterIo(dx_timerGetEventLoop(), intercore_ctx->sockFd, 
		EventLoop_Input, SocketEventHandler, intercore_ctx);
	if (socketEventReg == NULL) {
		Log_Debug("ERROR: Unable to register socket event: %d (%s)\n", errno, strerror(errno));
		return false;
	}

	return true;
}

bool dx_interCoreCommunicationsEnable(INTERCORE_CONTEXT* intercore_ctx) {
	return initialise_inter_core_communications(intercore_ctx);
}


/// <summary>
///     Nonblocking send intercore message
///		https://linux.die.net/man/2/send - Nonblocking = MSG_DONTWAIT.
/// </summary>
bool dx_interCoreSendMessage(INTERCORE_CONTEXT *intercore_ctx, void* control_block, size_t message_length) {
	
	if (message_length > 1024) {
		Log_Debug("Message too long. Max length is 1022 (1024 minus 2 bytes of control information\n");
	}

	// lazy initialise intercore socket
	if (!initialise_inter_core_communications(intercore_ctx)) {
		return false;
	};

	// https://linux.die.net/man/2/send - Nonblocking.
	// Returns EAGAIN if socket is full

	// send is blocking on socket full
	// allow 2 extra to the send length for ic msg control and reserve bytes
	int bytesSent = send(intercore_ctx->sockFd, control_block, message_length, intercore_ctx->nonblocking_io ? MSG_DONTWAIT : 0);
	if (bytesSent == -1) {
		Log_Debug("ERROR: Unable to send message: %d (%s)\n", errno, strerror(errno));
		return false;
	}

	return true;
}


/// <summary>
///     Handle socket event by reading incoming data from real-time capable application.
/// </summary>
static void SocketEventHandler(EventLoop* el, int fd, EventLoop_IoEvents events, void* context) {
	if (!ProcessMsg((INTERCORE_CONTEXT*)context)) {
		dx_terminate(DX_ExitCode_InterCoreHandler);
	}
}

/// <summary>
///     Handle socket event by reading incoming data from real-time capable application.
/// </summary>
static bool ProcessMsg(INTERCORE_CONTEXT* intercore_ctx) {
	if (intercore_ctx->intercore_recv_block == NULL) {
		return false;
	}

	ssize_t bytesReceived = recv(intercore_ctx->sockFd, (void*)intercore_ctx->intercore_recv_block, intercore_ctx->intercore_recv_block_length, 0);

	if (bytesReceived == -1) {
		dx_terminate(DX_ExitCode_InterCoreReceiveFailed);
		return false;
	}

	// minus 2 for bytesReceived for ic msg control and reserve bytes
	intercore_ctx->interCoreCallback(intercore_ctx->intercore_recv_block, (ssize_t)bytesReceived);

	return true;
}