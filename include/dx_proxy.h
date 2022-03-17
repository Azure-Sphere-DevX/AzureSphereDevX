/* Copyright (c) Avnet Incorporated. All rights reserved.
   Licensed under the MIT License. */

#pragma once

#include <iothubtransportmqtt_websockets.h>
#include <azure_prov_client/prov_transport_mqtt_ws_client.h>
#include <applibs/log.h>
#include <azure_sphere_provisioning.h>
#include "dx_terminate.h"
#include "dx_utilities.h"

typedef struct DX_PROXY_PROPERTIES{
    const char *proxyAddress;       // The Proxy Server Address, or FQDN
    const uint16_t proxyPort;       // The Proxy Server Port
    const char *proxyUsername;      // Proxy Username (basic auth), or NULL to use anonymous auth
    const char *proxyPassword;      // Proxy Password (basic auth), or NULL to use anponymous auth
    const char *noProxyAdresses;    // Comma seperated list of IP address' that should NOT be routed
                                    //   to the proxy
    bool proxyEnabled;                                    
} DX_PROXY_PROPERTIES;

/// <summary>
/// Create the DPS provisioning handle using MQTT over a Web Socket.  Required when the Sphere 
/// device is using a web proxy
/// </summary>
/// <param name="prov_handle"></param>
bool dx_proxyCreateDpsClientWithMqttWebSocket(const char *dpsUrl, const char *idScope, PROV_DEVICE_LL_HANDLE *prov_handle);

/// <summary>
/// Establish IoTHub connection using MQTT WebSocket.  Required when the Sphere device is using a web proxy
/// </summary>
/// <param name="proxyProperties"></param>
bool dx_proxyOpenIoTHubHandleWithMqttWebSocket(const char *hostname,
                                               IOTHUB_DEVICE_CLIENT_LL_HANDLE *iothubClientHandle);

/// <summary>
/// Return the status of the web proxy, enabled (true) or disabled (false).
/// </summary>
bool dx_proxyIsEnabled(void);

/// <summary>
/// Configure Proxy Settings
/// </summary>
/// <param name="proxyProperties"></param>
void dx_proxyConfigureProxy(DX_PROXY_PROPERTIES *proxyProperties);

/// <summary>
/// Enable or disable an already configured proxy.
/// </summary>
/// <param name="enableProxy">To enable or disable proxy. Set to true to enable, and false to
/// disable proxy</param>
void dx_proxyEnableProxy(bool enableProxy);