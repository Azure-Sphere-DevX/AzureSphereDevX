# Avnet send message example

## Config app_manifest.json sample

1. Set ID Scope
1. Set Allowed connections
1. Set DeviceAuthentication

---

## Functions

```c
void dx_azureConnect(DX_USER_CONFIG* dx_config, const char* networkInterface, const char* plugAndPlayModelId);
bool dx_isAzureConnected(void);
bool dx_azurePublish(const void* message, size_t messageLength, DX_MESSAGE_PROPERTY** messageProperties, size_t messagePropertyCount, DX_MESSAGE_CONTENT_PROPERTIES* messageContentProperties);
```

---

## Structures

```c
typedef struct DX_MESSAGE_PROPERTY
{
	const char* key;
	const char* value;
} DX_MESSAGE_PROPERTY;
```

```c
typedef struct DX_MESSAGE_CONTENT_PROPERTIES
{
	const char* contentEncoding;
	const char* contentType;
} DX_MESSAGE_CONTENT_PROPERTIES;
```

dx_config.h

```c
typedef enum {
    DX_CONNECTION_TYPE_NOT_DEFINED = 0,
    DX_CONNECTION_TYPE_DPS = 1,
    DX_CONNECTION_TYPE_DIRECT = 2
} ConnectionType;
```

```c
typedef struct {
    const char* idScope; 
    const char* connectionString;
    ConnectionType connectionType;
} DX_USER_CONFIG;
```

---

## Usage

Declare message template, and optionally message and content properties

```c
static const char* msgTemplate = "{ \"Temperature\":%3.2f, \"Humidity\":%3.1f, \"Pressure\":%3.1f }";

static DX_MESSAGE_PROPERTY* telemetryMessageProperties[] = {
	&(DX_MESSAGE_PROPERTY) { .key = "appid", .value = "hvac" },
	&(DX_MESSAGE_PROPERTY) {.key = "type", .value = "telemetry" },
	&(DX_MESSAGE_PROPERTY) {.key = "schema", .value = "1" }
};

static DX_MESSAGE_CONTENT_PROPERTIES telemetryContentProperties = {
	.contentEncoding = "utf-8",
	.contentType = "application/json"
};
```

### Send message with no properties

```c
if (snprintf(msgBuffer, sizeof(msgBuffer), msgTemplate, 30.0, 60.0, 1010.0) > 0) {
	dx_azurePublish(msgBuffer);
}
```

### Send message with properties

```c
if (snprintf(msgBuffer, sizeof(msgBuffer), msgTemplate, 30.0, 60.0, 1010.0) > 0) {
	dx_azurePublishWithProperties(msgBuffer, telemetryMessageProperties, NELEMS(telemetryMessageProperties), &telemetryContentProperties);
}
```

or content properties with no message properties.

```c
if (snprintf(msgBuffer, sizeof(msgBuffer), msgTemplate, 30.0, 60.0, 1010.0) > 0) {
	dx_azurePublishWithProperties(msgBuffer, NULL, 0, &telemetryContentProperties);
}
```

or message properties with no content properties.

```c
if (snprintf(msgBuffer, sizeof(msgBuffer), msgTemplate, 30.0, 60.0, 1010.0) > 0) {
	dx_azurePublishWithProperties(msgBuffer, telemetryMessageProperties, NELEMS(telemetryMessageProperties), NULL);
}
```