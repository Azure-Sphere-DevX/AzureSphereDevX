#include "dx_proxy.h"

// Keep a pointer to the proxy configuration+
static DX_PROXY_PROPERTIES *_proxyConfig = NULL;

/// <summary>
/// Establish IoTHub connection using MQTT WebSocket.  Required when the Sphere device is using a web proxy
/// </summary>
/// <param name="proxyProperties"></param>
bool dx_proxyOpenIoTHubHandleWithMqttWebSocket(const char *hostname,
                                               IOTHUB_DEVICE_CLIENT_LL_HANDLE *iothubClientHandle)
{
    
    IOTHUB_CLIENT_RESULT iothubResult;

    *iothubClientHandle =
        IoTHubDeviceClient_LL_CreateWithAzureSphereFromDeviceAuth(hostname, MQTT_WebSocket_Protocol);

    if (*iothubClientHandle == NULL) {
        Log_Debug("ERROR: IoTHubDeviceClient_LL_CreateFromDeviceAuth returned NULL.\n");
        return false;
    }

    HTTP_PROXY_OPTIONS httpProxyOptions;
    memset(&httpProxyOptions, 0, sizeof(httpProxyOptions));
    httpProxyOptions.host_address = _proxyConfig->proxyAddress;
    httpProxyOptions.port = _proxyConfig->proxyPort;

    if ((iothubResult = IoTHubDeviceClient_LL_SetOption(
        *iothubClientHandle, OPTION_HTTP_PROXY, &httpProxyOptions)) != IOTHUB_CLIENT_OK) {
        Log_Debug("ERROR: Failure setting Azure IoT Hub client option  \"OPTION_HTTP_PROXY\": %s\n",
            IOTHUB_CLIENT_RESULTStrings(iothubResult));
            return false;
    }

    return true;
}


/// <summary>
/// Configure Proxy Settings
/// </summary>
/// <param name="proxyProperties"></param>
void dx_proxyConfigureProxy(DX_PROXY_PROPERTIES *proxyProperties)
{
    int result = -1;

    // Verify we have a valid input
    if(proxyProperties == NULL){
        goto cleanup;
    }

    // By default, proxy configuration option Networking_ProxyOptions_Enabled is set and the proxy
    // type is Networking_ProxyType_HTTP.
    Networking_ProxyConfig *proxyConfig = Networking_Proxy_Create();
    if (proxyConfig == NULL) {
        Log_Debug("ERROR: Networking_Proxy_Create(): %d (%s)\n", errno, strerror(errno));
        goto cleanup;
    }

    // Set the proxy address and port.
    if (Networking_Proxy_SetProxyAddress(proxyConfig, proxyProperties->proxyAddress, proxyProperties->proxyPort) == -1) {
        Log_Debug("ERROR: Networking_Proxy_SetProxyAddress(): %d (%s)\n", errno, strerror(errno));
        goto cleanup;
    }

    // If both username and password are set, use basic authentication. Otherwise use anonymous
    // authentication.
    if ((proxyProperties->proxyUsername != NULL) && (proxyProperties->proxyPassword != NULL)) {
        if (Networking_Proxy_SetBasicAuthentication(proxyConfig, proxyProperties->proxyUsername, proxyProperties->proxyPassword) ==
            -1) {
            Log_Debug("ERROR: Networking_Proxy_SetBasicAuthentication(): %d (%s)\n", errno,
                      strerror(errno));
            goto cleanup;
        }
    } else {
        if (Networking_Proxy_SetAnonymousAuthentication(proxyConfig) == -1) {
            Log_Debug("ERROR: Networking_Proxy_SetAnonymousAuthentication(): %d (%s)\n", errno,
                      strerror(errno));
            goto cleanup;
        }
    }

    // Set addresses for which proxy should not be used if "noProxyAddresses" is modified.
    if (proxyProperties->noProxyAdresses != NULL) {
        if (Networking_Proxy_SetProxyNoProxyAddresses(proxyConfig, proxyProperties->noProxyAdresses) == -1) {
            Log_Debug("ERROR: Networking_Proxy_SetProxyNoProxyAddresses(): %d (%s)\n", errno,
                      strerror(errno));
            goto cleanup;
        }
    }

    // Apply the proxy configuration.
    if (Networking_Proxy_Apply(proxyConfig) == -1) {
        Log_Debug("ERROR: Networking_Proxy_Apply(): %d (%s)\n", errno, strerror(errno));
        goto cleanup;
    }

    // The proxy settings were all applied without any issues.  Update the _proxyConfig pointer
    // to use while the proxy is active
    _proxyConfig = proxyProperties;
    result = 0;

cleanup:
    if (proxyConfig) {
        Networking_Proxy_Destroy(proxyConfig);
    }

    // If we encountered any errors exit and return the exit reason to the OS
    if(result != 0){
        dx_terminate(DX_ExitCode_Configure_Proxy_Failed);
    }
}

/// <summary>
/// Enable or disable an already configured proxy.
/// </summary>
/// <param name="enableProxy">To enable or disable proxy. Set to true to enable, and false to
/// disable proxy</param>
void dx_proxyEnableProxy(bool enableProxy)
{
    int result = -1;

    // Verify that the proxy configuration has been set
    if(_proxyConfig == NULL){
        goto cleanup;
    }

    Networking_ProxyConfig  *proxyConfig = Networking_Proxy_Create();
    if (proxyConfig == NULL) {
        Log_Debug("ERROR: Networking_Proxy_Create(): %d (%s)\n", errno, strerror(errno));
        goto cleanup;
    }

    // Need to get the current config, otherwise the existing config will get overwritten with a
    // blank/default config when the change is applied.
    if (Networking_Proxy_Get(proxyConfig) == -1) {

        if (errno == ENOENT) {
            Log_Debug("There is currently no proxy configured.\n");
        } else {
            Log_Debug("ERROR: Networking_Proxy_Get(): %d (%s)\n", errno, strerror(errno));
        }
        goto cleanup;
    }

    // Get the proxy options.
    Networking_ProxyOptions proxyOptions = 0;
    if (Networking_Proxy_GetProxyOptions(proxyConfig, &proxyOptions) == -1) {
        Log_Debug("ERROR: Networking_Proxy_GetProxyOptions(): %d (%s)\n", errno, strerror(errno));
        goto cleanup;
    }

    // Set the enabled/disabled proxy option.
    if (enableProxy) {
        // Set flag Networking_ProxyOptions_Enabled;
        proxyOptions |= Networking_ProxyOptions_Enabled;
        _proxyConfig->proxyEnabled = true;
    } else {
        // Reset flag Networking_ProxyOptions_Enabled;
        proxyOptions &= ~((unsigned int)Networking_ProxyOptions_Enabled);
        _proxyConfig->proxyEnabled = false;
    }

    if (Networking_Proxy_SetProxyOptions(proxyConfig, proxyOptions) == -1) {
        Log_Debug("ERROR: Networking_Proxy_SetProxyOptions(): %d (%s)\n", errno, strerror(errno));
        goto cleanup;
    }

    // Apply the proxy configuration.
    if (Networking_Proxy_Apply(proxyConfig) == -1) {
        Log_Debug("ERROR: Networking_Proxy_Apply(): %d (%s)\n", errno, strerror(errno));
        goto cleanup;
    }
    result = 0;

cleanup:
    if (proxyConfig) {
        Networking_Proxy_Destroy(proxyConfig);
    }
    
    // If we encountered any errors exit and return the exit reason to the OS
    if(result != 0){
        dx_terminate(DX_ExitCode_Enable_Disable_Proxy_Failed);
    }
}


/// <summary>
/// Create the DPS provisioning handle using MQTT over a Web Socket.  Required when the Sphere 
/// device is using a web proxy
/// </summary>
/// <param name="prov_handle"></param>
bool dx_proxyCreateDpsClientWithMqttWebSocket(const char *dpsUrl, 
                                              const char *idScope, 
                                              PROV_DEVICE_LL_HANDLE *prov_handle)
{
    // Create Provisioning Client for communication with DPS
    // using MQTT protocol
    *prov_handle = Prov_Device_LL_Create(dpsUrl, idScope, Prov_Device_MQTT_WS_Protocol);
    if (prov_handle == NULL) {
        Log_Debug("ERROR: Failed to create Provisioning Client\n");
        return false;
    }

    // Use DAA cert in provisioning flow - requires the SetDeviceId option to be set on the
    // provisioning client.
    static const int deviceIdForDaaCertUsage = 1;
    PROV_DEVICE_RESULT prov_result =
        Prov_Device_LL_SetOption(*prov_handle, "SetDeviceId", &deviceIdForDaaCertUsage);
    if (prov_result != PROV_DEVICE_RESULT_OK) {
        Log_Debug("ERROR: Failed to set Device ID in Provisioning Client\n");
        return false;
    }

    HTTP_PROXY_OPTIONS httpProxyOptions;
    memset(&httpProxyOptions, 0, sizeof(httpProxyOptions));
    httpProxyOptions.host_address = _proxyConfig->proxyAddress;
    httpProxyOptions.port = _proxyConfig->proxyPort;
    prov_result = Prov_Device_LL_SetOption(*prov_handle, OPTION_HTTP_PROXY, &httpProxyOptions);
    if (prov_result != PROV_DEVICE_RESULT_OK) {
        Log_Debug("ERROR: Failed to set Proxy options in Provisioning Client\n");
        return false;
    }

    return true;
}

/// <summary>
/// Return the status of the web proxy, enabled (true) or disabled (false).
/// </summary>
bool dx_proxyIsEnabled(void){
    return _proxyConfig->proxyEnabled;
}
