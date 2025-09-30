#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <WiFiClientSecure.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <ESPmDNS.h>
#include <ArduinoOTA.h>
#include <credentials.h>

// WiFi and MQTT
const char *ssid              = WIFI_SSID;
const char *password          = WIFI_PASSWORD;
const char *soft_ap_ssid      = SOFT_AP_SSID;
const char *soft_ap_password  = SOFT_AP_PASSWORD; // NULL for no password
const int   channel           = 1;    // WiFi Channel number between 1 and 13
const bool  hide_SSID         = false; // To disable SSID broadcast -> SSID will not appear in a basic WiFi scan
const int   max_connection    = 3;

#define DEBOUNCE_DELAY 50 // ms

struct MapDevice{
  int channel;
  std::vector<int> inputPins;
  std::vector<int> outputPins;
  std::vector<bool> inputState;
  std::vector<unsigned long> lastDebounceTime;
  std::vector<bool> lastButtonState;
  std::vector<bool> outputState;
  std::string name;
};

// Initialize with pin numbers; state vectors must match size
std::vector<MapDevice> devices = {
  {
    0,
    {32},                // input pins
    {23},                // output pins
    {false},             // inputState
    {0},                 // lastDebounceTime
    {HIGH},              // lastButtonState (with INPUT_PULLUP, not pressed = HIGH)
    {false},             // outputState (false = OFF)
    "Luz_Cozinha"
  },
  {
    1,
    {33},
    {22},
    {false},
    {0},
    {HIGH},
    {false},
    "Luz_Lavanderia"
  },
  {
    2,
    {25},
    {21},
    {false},
    {0},
    {HIGH},
    {false},
    "Luz_Corredor_Quintal"
  },
  {
    3,
    {26, 27},            // two switches for same light
    {19},
    {false, false},      // inputState
    {0, 0},              // lastDebounceTime
    {HIGH, HIGH},        // lastButtonState
    {false},             // outputState
    "Luz_Quarto_Fabio"
  }
};

// Globals
AsyncWebServer server(80);
AsyncEventSource events("/events");

IPAddress local_IP(192, 168, 0, 122); // Defina o IP
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 255, 0);
IPAddress primaryDNS(8, 8, 8, 8);   // optional
IPAddress secondaryDNS(8, 8, 4, 4); // optional

// Tasks
TaskHandle_t TaskButtonsHandle, TaskRestApiHandle;

// Methods declarations
void checkButtons();
void setupPins();
// void setupRestAPI();

void toggleDevice(MapDevice& device, bool newState) {
    device.outputState[0] = newState;
    digitalWrite(device.outputPins[0], newState ? HIGH : LOW);

    char topicState[64], topicControl[64];
    char payload[8];
    int channel = device.channel;
    String status = newState ? "ON" : "OFF";

    sprintf(payload, status.c_str());

    sprintf(topicState, "channel%d:%s", device.channel, status); // TBD: may change to device.name instead of ch(channel)
    events.send(topicState, "update", millis());
}

void toggleDevice(MapDevice& device) {
  bool newState = !device.outputState[0];
  toggleDevice(device, newState);
}

void TaskButtons(void *parameter)
{
  while (true) {
    checkButtons();
    vTaskDelay(pdMS_TO_TICKS(30));
  }
}

// const String index_file = htmlFileToString("index.html"); // to be tested; TODO: in future move all website to an external server outside the esp
const char index_html[] PROGMEM = R"rawliteral(
 <!DOCTYPE html><html><head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 Smart Home v4</title>
    <style>
      body {font-family: sans-serif; text-align: center; margin-top: 30px;}
      .channel {display:flex; flex-direction: column; width: 150px; margin: 10px;}
      #channels {display: flex; flex-wrap: wrap; justify-content: center;}
      .switch {position: relative; display:inline-block; width: 60px; height: 34px; left: 30%;} 
      #enable_channels_input {width: 100px; margin-right: 10px;}

      input:disabled+.slider {background-color: #ccc;}
      .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ff0000; transition:.4s;}
      .slider:before {position: absolute; content: ""; height: 26px; width: 26px; left: 4px; bottom: 4px; background-color: #fff; -webkit-transition: .4s; transition: .4s;}
      input:checked+.slider {background-color: #66bb6a}
      input:checked+.slider:before {-webkit-transform: translateX(26px); transform: translateX(26px)}
      .slider.round {border-radius: 34px;}
      .slider.round:before {border-radius: 50%;}
    </style>
  </head><body>
    <h2>ESP32 Smart Home v4</h2>
    <div class="enable-channels"><span>Enable all channels password: </span><input type="text" name="enable_channels_input" id="enable_channels_input"><input type="button" value="Send" onclick="enable_channels()"/></div>
    <div class="channels_title"><h3>Controle das Luzes</h3></div>
    <div id="channels">
      <div class="channel">Cozinha: <span id="state0">--</span> <label class="switch"> <input type="checkbox" onchange="toggle(0)" class="toggle" id="switch0"> <div class="slider round"></div></label></div>
      <div class="channel">Lavanderia: <span id="state1">--</span> <label class="switch"> <input type="checkbox" onchange="toggle(1)" class="toggle" id="switch1"> <div class="slider round"></div></label></div>
      <div class="channel">Corredor Quintal: <span id="state2">--</span> <label class="switch"> <input type="checkbox" onchange="toggle(2)" class="toggle" id="switch2"> <div class="slider round"></div></label></div>
      <div class="channel">Quarto do Fabio: <span id="state3">--</span> <label class="switch"> <input type="checkbox" onchange="toggle(3)" class="toggle" id="switch3" disabled> <div class="slider round"></div></label></div>
    </div>
  <script defer>
    window.onload = resetAllToggles();
    var bedroomSwitch = document.getElementById("switch3");

  function toggle(ch) {
    fetch("/toggle?channel=" + ch);
  }

  function enable_channels(){
    var password = document.getElementById("enable_channels_input");

    if (password.value === "lmi56n") {
      
      bedroomSwitch.removeAttribute("disabled");
      alert("All channels are enabled");
    }
  }

  function resetAllToggles() {
  const toggles = document.getElementsByClassName("toggle");
  for (let i = 0; i < toggles.length; i++) {
    toggles[i].checked = false;
  }
}

  if (!!window.EventSource) {
    const sourceEvents = new EventSource('/events');
    sourceEvents.addEventListener("update", function(e) {
      const [ch, state] = e.data.split(":");
      var index = ch.replace("channel", "");

      if(index == "3" && bedroomSwitch.hasAttribute("disabled")) {
        return;
      }

      document.getElementById("state" + index).textContent = state;
      document.getElementById("switch" + index).checked = state === 'ON' ? true : false;
    }, false);
  }
  </script>
  </body></html>
)rawliteral";

// ========= Setup =========
void setup()
{
  Serial.begin(115200);

  setupPins();

  setupWifi();

  asyncWebServerRoutes();

  xTaskCreatePinnedToCore(TaskButtons, "TaskButtons", 4096, NULL, 1, &TaskButtonsHandle, 1);
}

MapDevice& findDeviceByChannel(int channel){
  for (auto& device : devices) {
    if(device.channel == channel) {
      return device;
    }
  }
  throw std::runtime_error("Object not found");
}

void setupWifi() {
  WiFi.mode(WIFI_AP_STA);
  WiFi.disconnect();

  Serial.println("[*] Creating ESP32 WIFI connection");

  // Try static IP
  if (WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS)) {
    Serial.println("[+] Static IP configured");
  } else {
    Serial.println("[!] Failed to configure static IP, falling back to DHCP");
  }

  WiFi.begin(ssid, password);

  unsigned long startAttemptTime = millis();
  const unsigned long wifiTimeout = 15000; // 15 seconds

  while (WiFi.status() != WL_CONNECTED && millis() - startAttemptTime < wifiTimeout) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[+] WiFi connected: ");
    Serial.println(WiFi.localIP());
  } else {
    Serial.println("[!] WiFi STA connection failed, continuing with AP only");
  }

  // Start AP anyway
  if (!WiFi.softAP(soft_ap_ssid, soft_ap_password, channel, hide_SSID, max_connection)) {
    Serial.println("[!] Failed to start softAP");
  } else {
    Serial.print("[+] AP Created with IP Gateway ");
    Serial.println(WiFi.softAPIP());
    Serial.print("Soft AP SSID: \"");
    Serial.print(soft_ap_ssid);
    Serial.print("\", IP address: ");
    Serial.println(WiFi.softAPIP());
  }

  if (MDNS.begin("esp32_smart_v4")) {
    Serial.println("MDNS started. Access with http://esp32_smart_v4.local/");
  } else {
    Serial.println("Error! MDNS not started.");
  }
}

void asyncWebServerRoutes() {
  // Async Web Server Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", index_html);
  });

  server.on("/toggle", HTTP_POST, [](AsyncWebServerRequest *request) {
    if (request->hasParam("channel")) {
      int ch = request->getParam("channel")->value().toInt();

      if(ch >= 0 && ch < devices.size()) {
        MapDevice& mapDevice = findDeviceByChannel(ch);

        toggleDevice(mapDevice);

        String msg = "The device channel: " + String(mapDevice.channel) + " has been changed its state to: " + mapDevice.outputState[0] ? "ON" : "OFF";
        
        request->send(200, "text/plain", msg);
      } else {
        request->send(404, "text/plain", "Device not found");
      }
      return;
    }
    request->send(400, "text/plain", "Bad Request");
  });

  events.onConnect([](AsyncEventSourceClient *client) {
    for (auto& device : devices) {
      for (size_t j = 0; j < device.outputState.size(); j++) {
        char msg[20];
        sprintf(msg, "channel%d:%s", device.channel, device.outputState[j] ? "ON" : "OFF");
        client->send(msg, "update", millis());
      }
    }
  });

 server.on("/api/devices", HTTP_GET, [](AsyncWebServerRequest *request){
    String json = "[";
    for(int i=0; i<devices.size(); i++){
      json += "{\"channel\":" + String(devices[i].channel) + ",\"name\":\"" + devices[i].name.c_str() + "\",\"outputState\":" + (devices[i].outputState[0] ? "true" : "false") + "}";
      
      if(i < devices.size() - 1) {
        json += ",";
      }
    }
    json += "]";
    request->send(200, "application/json", json);
  });

  server.on("/api/device/toggle", HTTP_POST, [](AsyncWebServerRequest *request){
    if (request->hasParam("channel") && request->hasParam("state")) {
      int channel = request->getParam("channel")->value().toInt();
      bool state = request->getParam("state")->value().equalsIgnoreCase("true");

      for(int i=0; i<devices.size(); i++){
        if(devices[i].channel == channel) {
          toggleDevice(devices[i], state);
          String msg = String(devices[i].name.c_str()) + " on channel: " + String(devices[i].channel) + " has change state to: " + String(state ? "ON" : "OFF");
          request->send(200, "text/plain", msg);
          return;
        }
      }

      request->send(404, "text/plain", "Device not found");
    } else {
      request->send(400, "text/plain", "Bad Request");
    }
  });

  Serial.println("Rest API Ready");

  server.addHandler(&events);
  server.begin();
}

void loop()
{
  // All logic handled in FreeRTOS tasks
}

void checkButtons() {
  for (auto& device : devices) {
    for (size_t i = 0; i < device.inputPins.size(); i++) {
      bool reading = digitalRead(device.inputPins[i]);

      if (reading != device.lastButtonState[i]) {
        device.lastDebounceTime[i] = millis();
      }

      if ((millis() - device.lastDebounceTime[i]) > DEBOUNCE_DELAY) {
        if (reading != device.inputState[i]) {
          device.inputState[i] = reading;

          if (device.inputState[i] == LOW) { // Only trigger on rising edge
            toggleDevice(device);
          }
        }
      }
      device.lastButtonState[i] = reading;
    }
  }
}

void setupPins() {
  for (auto& device : devices) {
    for (int in : device.inputPins) {
      pinMode(in, INPUT_PULLUP);
    }
    for (size_t i = 0; i < device.outputPins.size(); i++) {
      pinMode(device.outputPins[i], OUTPUT);
      digitalWrite(device.outputPins[i], LOW);  // HIGH = OFF by default
      device.outputState[i] = false;
    }
    // Initialize debouncing variables
    for (size_t i = 0; i < device.inputPins.size(); i++) {
      device.lastDebounceTime[i] = 0;
      device.lastButtonState[i] = HIGH; // Assuming pull-up, so button not pressed is HIGH
    }
  }
}