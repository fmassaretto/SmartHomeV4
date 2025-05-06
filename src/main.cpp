#include <Arduino.h>
#ifdef ESP32
  #include <WiFi.h>
  #include <AsyncTCP.h>
// #elif defined(ESP8266)
// #include <ESP8266WiFi.h>
// #include <ESPAsyncTCP.h>
#endif
#include <ESPAsyncWebServer.h>
#include <PubSubClient.h>
#include <WiFiClientSecure.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <ESPmDNS.h>
#include <credentials.h>

// WiFi and MQTT
const char *ssid              = WIFI_SSID;
const char *password          = WIFI_PASSWORD;
const char *soft_ap_ssid      = SOFT_AP_SSID;
const char *soft_ap_password  = SOFT_AP_PASSWORD; // NULL for no password
const int   channel           = 10;    // WiFi Channel number between 1 and 13
const bool  hide_SSID         = false; // To disable SSID broadcast -> SSID will not appear in a basic WiFi scan
const int   max_connection    = 2;  
const char *mqtt_server       = HIVEMQ_MQTT_SERVER; // using HiveMQ Cloud
const char *mqtt_username     = HIVEMQ_MQTT_USERNAME;
const char *mqtt_password     = HIVEMQ_MQTT_PASSWORD;
const int mqtt_port           = 8883;

struct MapDevice{
  int channel;
  std::vector<int> inputPins;
  std::vector<int> outputPins;
  std::vector<bool> inputState;
  std::vector<bool> outputState;
  std::string name;
};

bool mqttOnline = false;

// Initialize with pin numbers; state vectors must match size
std::vector<MapDevice> devices = {
  {
    0,            // channel
    {32},         // input pins
    {23},         // output pins
    {false},      // initial input states
    {HIGH},       // initial output states HIGH = OFF
    "Luz_Cozinha" // Device name
  },
  {
    1,
    {33},
    {22},
    {false},
    {HIGH},
    "Luz_Lavanderia"
  },
  {
    2,
    {25},
    {21},
    {false},
    {HIGH},
    "Luz_Corredor_Quintal"
  },
  {
    3,
    {26, 27},
    {19},
    {false, false},
    {HIGH},
    "Luz_Quarto_Fabio"
  }
};

// Topics
const char *control_topic = "home/channel%d/control";
const char *command_topic = "home/channel%d/command";
const char *state_topic = "home/channel%d/state";

// Globals
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(80);
AsyncEventSource events("/events");

IPAddress local_IP(192, 168, 0, 222); // Defina o IP
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   // optional
IPAddress secondaryDNS(8, 8, 4, 4); // optional

// Tasks
TaskHandle_t TaskButtonsHandle, TaskMQTTHandle;

void toggleDevice(MapDevice& device, bool newState) {
    device.outputState[0] = newState;
    digitalWrite(device.outputPins[0], newState ? LOW : HIGH);

    char topicState[64], topicControl[64];
    char payload[8];
    int channel = device.channel;
    String status = newState ? "ON" : "OFF";

    sprintf(topicState, state_topic, channel);
    sprintf(topicControl, control_topic, channel);
    sprintf(payload, status.c_str());

    if (mqttOnline) {
      mqttClient.publish(topicState, payload);
      mqttClient.publish(topicControl, payload);
    }

    sprintf(topicState, "ch%d:%s", device.channel, status); // TBD: may change to device.name instead of ch(channel)
    events.send(topicState, "update", millis());
}

void toggleDevice(MapDevice& device) {
  bool newState = !device.outputState[0];
  toggleDevice(device, newState);
}

void mqttCallback(char *topic, byte *payload, unsigned int length) { // Called by externaal MQTT Sender
  String t = String(topic);
  String msg;
  char topicCommand[32];

  for (unsigned int i = 0; i < length; i++)
    msg += (char) payload[i];

    for (auto& device : devices) {
      sprintf(topicCommand, command_topic, device.channel);

      if (t == topicCommand) {
        toggleDevice(device, msg == "ON");
      }
  }
}

// HiveMQ Cloud Let's Encrypt CA certificate
static const char *root_ca PROGMEM = R"EOF(-----BEGIN CERTIFICATE-----
MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw
TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh
cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4
WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu
ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY
MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc
h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+
0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U
A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW
T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH
B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC
B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv
KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn
OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn
jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw
qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI
rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV
HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq
hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL
ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ
3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK
NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5
ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur
TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC
jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc
oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq
4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA
mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d
emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=
-----END CERTIFICATE-----)EOF";

void reconnectMQTT() {
  while (!mqttClient.connected()) {
    String client_id = "esp8266-client-" + String(WiFi.macAddress());

    if (mqttClient.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      mqttOnline = true;
      Serial.println("MQTT connected to: " + client_id);
      for (auto& device : devices) {
        char topicCommand[32];
        sprintf(topicCommand, command_topic,device.channel);

        mqttClient.subscribe(topicCommand);
      }
    } else {
      mqttOnline = false;
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.println(mqttClient.state());

      delay(5000);
    }
  }
}

void TaskMQTT(void *parameter) {
  for (;;) {
    if (WiFi.status() == WL_CONNECTED) {
      if (!mqttClient.connected()) {
        reconnectMQTT();
      }
      mqttClient.loop();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void checkButtons() {
  for (auto& device : devices) {
    for (size_t i = 0; i < device.inputPins.size(); i++) {
      bool currentState = digitalRead(device.inputPins[i]) == HIGH;

      // Trigger only on rising edge
      if (currentState && !device.inputState[i]) {
        toggleDevice(device);
      }

      // Save input state for edge detection
      device.inputState[i] = currentState;
    }
  }
}

void TaskButtons(void *parameter)
{
  while (true) {
    checkButtons();
    vTaskDelay(pdMS_TO_TICKS(30)); // debounce and CPU friendly
  }
}

// const String index_file = htmlFileToString("index.html"); // to be tested; TODO: in future move all website to an external server outside the esp
const char index_html[] PROGMEM = R"rawliteral(
 <!DOCTYPE html><html><head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 Smart Home v2</title>
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
    <h2>ESP32 Smart Home v2</h2>
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
    fetch("/toggle?ch=" + ch);
  }

  function enable_channels(){
    var password = document.getElementById("enable_channels_input");

    if (password.value === "lmi56n") {
      
      bedroomSwitch.removeAttribute("disabled");
      alert("All channels are enabled");
    }
  }

  function resetAllToggles() {
    document.getElementsByClassName("toggle").checked = false;
  }

  if (!!window.EventSource) {
    const sourceEvents = new EventSource('/events');
    sourceEvents.addEventListener("update", function(e) {
      const [ch, state] = e.data.split(":");
      var index = ch.replace("ch", "");

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

void setupPins() {
  for (auto& device : devices) {
    for (int in : device.inputPins) {
      pinMode(in, INPUT_PULLUP);
    }
    for (size_t i = 0; i < device.outputPins.size(); i++) {
      pinMode(device.outputPins[i], OUTPUT);
      digitalWrite(device.outputPins[i], HIGH);  // HIGH = OFF by default
      device.outputState[i] = false;
    }
  }
}

// ========= Setup =========
void setup()
{
  Serial.begin(115200);

  setupPins();

  setupWifi();

  setupMqtt();

  asyncWebServerRoutes();

  xTaskCreatePinnedToCore(TaskButtons, "TaskButtons", 4096, NULL, 1, &TaskButtonsHandle, 1);
  xTaskCreatePinnedToCore(TaskMQTT, "TaskMQTT", 8192, NULL, 1, &TaskMQTTHandle, 1);
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
  // Configures static IP address
  Serial.println("[*] Creating ESP32 WIFI connection");
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
  {
    Serial.println("STA Failed to configure");
  }
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
    delay(500);

  Serial.print("[+] WiFi connected: ");
  Serial.println(WiFi.localIP());

  Serial.println("[*] Creating ESP32 AP");
  if (!WiFi.softAP(soft_ap_ssid, soft_ap_password, channel, hide_SSID, max_connection))
  {
      Serial.println("failed to start softAP");
  }
  Serial.print("[+] AP Created with IP Gateway ");
  Serial.println(WiFi.softAPIP());
  Serial.print("Soft AP SSID: \"");
  Serial.print(soft_ap_ssid);
  Serial.print("\", IP address: ");
  Serial.println(WiFi.softAPIP());

  if (MDNS.begin("esp32")) Serial.println("MDNS started.");
  else Serial.println("Error! MDNS not started.");
}

void setupMqtt() {
  espClient.setCACert(root_ca);

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
}

void asyncWebServerRoutes() {
  // Async Web Server Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/html", index_html);
  });

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ch")) {
      int ch = request->getParam("ch")->value().toInt();

      if(ch >= 0 && ch < devices.size()) {
        MapDevice& mapDevice = findDeviceByChannel(ch);

        toggleDevice(mapDevice);
      }
    }
    
    request->send(200, "text/plain", "OK");
  });

  events.onConnect([](AsyncEventSourceClient *client) {
    for (auto& device : devices) {
      for (size_t j = 0; j < device.outputState.size(); j++) {
        char msg[20];
        sprintf(msg, "ch%d:%s", device.channel, device.outputState[j] ? "ON" : "OFF");
        client->send(msg, "update", millis());
      }
    }
  });

  server.addHandler(&events);
  server.begin();
}

void loop()
{
  // All logic handled in FreeRTOS tasks
}