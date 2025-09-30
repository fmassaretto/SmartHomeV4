# Smart Home v4
_Simplier version of SmartHome v2 (https://github.com/fmassaretto/SmartHomeV2) without MQTT functionality to priority rest api_

_Project of smart home using an ESP32 to control devices (Can be light, outlets etc) connected to its I/O and also uses FreeRTOS and MQTT to communicated thought a web server_


## New Features

### Automatic WiFi Reconnection
The device will now automatically attempt to reconnect to the configured WiFi network if the connection is lost. This ensures continuous operation without manual intervention.

### REST API for Input/Output Control
A RESTful API has been implemented to control devices and retrieve their states. This allows for integration with other home automation systems or custom applications.

## Future Features

### Over-The-Air (OTA) Updates
Firmware can now be updated wirelessly using the ArduinoOTA feature. This allows for convenient updates without needing to physically connect the ESP32 to a computer. The OTA password is set in `credentials.h`.


**Endpoints:**
- `GET /api/devices`: Returns a JSON array of all configured devices, including their channel, name, and current output state.
- `POST /api/device/toggle`: Toggles the state of a specific device. Requires `channel` (integer) and `state` (boolean: `true` for ON, `false` for OFF) as form parameters.

**Example Usage (using `curl`):**
```bash
# Get all devices
curl http://<ESP32_IP_ADDRESS>/api/devices

# Toggle device with channel 0 to ON
curl -X POST -d "channel=0&state=true" http://<ESP32_IP_ADDRESS>/api/device/toggle
```

### Button Debouncing
Software debouncing has been implemented for momentary button inputs. This prevents multiple triggers from a single button press, ensuring reliable operation.

## Configuration

Before compiling and uploading the code, ensure you have configured your WiFi credentials and OTA password in `credentials.h`.

```cpp
// credentials.h example
#define WIFI_SSID "YourWiFiSSID"
#define WIFI_PASSWORD "YourWiFiPassword"
// #define OTA_PASSWORD "YourOTAPassword" // TODO: Uncomment after implementing ArduinoOTA
#define SOFT_AP_SSID "SmartHomeAP"
#define SOFT_AP_PASSWORD ""
```

## Building and Uploading

This project uses PlatformIO. Make sure you have PlatformIO Core installed.

1. Open a terminal in the project directory.
2. Run `platformio run` to build the project.
3. Run `platformio run --target upload` to upload the firmware to your ESP32.

For OTA updates, ensure your ESP32 is connected to the same network and run:
`platformio run --target upload --environment esp32dev --upload-port <ESP32_IP_ADDRESS>`

Replace `<ESP32_IP_ADDRESS>` with the actual IP address of your ESP32 device.