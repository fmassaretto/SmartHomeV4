#include <HtmlToString.h>
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
#include <credentials.h>

// WiFi and MQTT
const char *ssid = WIFI_SSID;
const char *password = WIFI_PASSWORD;
const char *mqtt_server = HIVEMQ_MQTT_SERVER; // using HiveMQ Cloud
const char *mqtt_username = HIVEMQ_MQTT_USERNAME;
const char *mqtt_password = HIVEMQ_MQTT_PASSWORD;
const int mqtt_port = 8883;

// GPIOs
const int NUM_CHANNELS = 4;
const int ledPins[] = {23, 22, 21, 19};
const int buttonPins[] = {34, 35, 32, 33};
// const String deviceName[] = {"Luz Cozinha", "Luz Lavanderia", "Luz Quintal", "Luz Quarto Fabio"}; // for now not used yet
bool lightStates[NUM_CHANNELS] = {false, false, false, false};
bool lastButtonStates[NUM_CHANNELS] = {HIGH, HIGH, HIGH, HIGH};
bool mqttOnline = false;

// Topics
const char *control_topic = "home/light/control";
const char *command_topic = "home/light/command";

// Globals
// WiFiClient espClient;
WiFiClientSecure espClient;
PubSubClient mqttClient(espClient);
AsyncWebServer server(80);
AsyncEventSource events("/events");

IPAddress local_IP(192, 168, 0, 222); // Defina o IP de acordo com a sua rede
IPAddress gateway(192, 168, 0, 1);
IPAddress subnet(255, 255, 0, 0);
IPAddress primaryDNS(8, 8, 8, 8);   // optional
IPAddress secondaryDNS(8, 8, 4, 4); // optional

// Tasks
TaskHandle_t TaskButtonsHandle, TaskMQTTHandle;

void toggleLight(int index, bool newState)
{
  lightStates[index] = newState;
  digitalWrite(ledPins[index], newState ? HIGH : LOW);
  char topic[64], payload[8];
  sprintf(topic, "home/light%d/state", index);
  sprintf(payload, newState ? "ON" : "OFF");
  if (mqttOnline)
    mqttClient.publish(topic, payload);

  sprintf(topic, "ch%d:%s", index, newState ? "ON" : "OFF");
  events.send(topic, "update", millis());
}

void toggleLight(int index)
{
  toggleLight(index, !lightStates[index]);
  if (mqttOnline)
  {
    char topic[64], payload[8];
    sprintf(topic, "home/light%d/control", index);
    sprintf(payload, lightStates[index] ? "ON" : "OFF");
    mqttClient.publish(topic, payload);
  }
}

void mqttCallback(char *topic, byte *payload, unsigned int length)
{
  String t = String(topic);
  String msg;
  for (unsigned int i = 0; i < length; i++)
    msg += (char)payload[i];

  for (int i = 0; i < NUM_CHANNELS; i++)
  {
    if (t == "home/light" + String(i) + "/command")
    {
      toggleLight(i, msg == "ON");
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

void reconnectMQTT()
{
  while (!mqttClient.connected())
  {
    String client_id = "esp8266-client-" + String(WiFi.macAddress());

    if (mqttClient.connect(client_id.c_str(), mqtt_username, mqtt_password))
    {
      mqttOnline = true;
      Serial.println("MQTT connected to: " + client_id);
      for (int i = 0; i < NUM_CHANNELS; i++)
      {
        String topic = "home/light" + String(i) + "/command";
        mqttClient.subscribe(topic.c_str());
      }
    }
    else
    {
      mqttOnline = false;
      Serial.print("Failed to connect to MQTT broker, rc=");
      Serial.println(mqttClient.state());

      delay(5000);
    }
  }
}

void TaskMQTT(void *parameter)
{
  for (;;)
  {
    if (WiFi.status() == WL_CONNECTED)
    {
      if (!mqttClient.connected())
        reconnectMQTT();
      mqttClient.loop();
    }
    vTaskDelay(10 / portTICK_PERIOD_MS);
  }
}

void TaskButtons(void *parameter)
{
  for (;;)
  {
    for (int i = 0; i < NUM_CHANNELS; i++)
    {
      bool state = digitalRead(buttonPins[i]);
      if (state == LOW && lastButtonStates[i] == HIGH)
      {
        toggleLight(i);
        vTaskDelay(50 / portTICK_PERIOD_MS);
      }
      lastButtonStates[i] = state;
    }
    vTaskDelay(20 / portTICK_PERIOD_MS);
  }
}

const char index_html[] PROGMEM = R"rawliteral(
<!DOCTYPE html><html><head>
    <meta name="viewport" content="width=device-width, initial-scale=1">
    <title>ESP32 Smart Home v2</title>
    <style>
      body { font-family: sans-serif; text-align: center; margin-top: 30px; }
      .channel { margin: 20px; }
      .channels_title { margin: 20px; display: inline-block; position: relative; }
      #channels { display: flex; flex-wrap: wrap; justify-content: center;}
      .switch {position: relative; display:grid; width: 96px; height: 34px} 
      .switch input {display: none}
      .slider {position: absolute; top: 0; left: 0; right: 0; bottom: 0; background-color: #ff0000; border-radius: 6px}
      .slider:before {position: absolute; content: ""; height: 20px; width: 30px; left: 8px; bottom: 8px; background-color: #fff; -webkit-transition: .4s; transition: .4s; border-radius: 3px}
      input:checked+.slider {background-color: #3cff00}
      input:checked+.slider:before {-webkit-transform: translateX(52px); -ms-transform: translateX(52px); transform: translateX(52px)}
    </style>
  </head><body>
    <h2>ESP32 Smart Home v2</h2>
    <div class="channels_title"><h3>Light Control</h3></div>
    <div id="channels">
      <div class="channel" id="channel0">Light 1: <span id="state0">--</span> <label class="switch"> <input type="checkbox" onchange="toggle(0)" id="switch0"><span class="slider"></span></label></div>
      <div class="channel" id="channel1">Light 2: <span id="state1">--</span> <label class="switch"> <input type="checkbox" onchange="toggle(1)" id="switch1"><span class="slider"></span></label></div>
      <div class="channel" id="channel2">Light 3: <span id="state2">--</span> <label class="switch"> <input type="checkbox" onchange="toggle(2)" id="switch2"><span class="slider"></span></label></div>
      <div class="channel" id="channel3">Light 4: <span id="state3">--</span> <label class="switch"> <input type="checkbox" onchange="toggle(3)" id="switch3"><span class="slider"></span></label></div>
    </div>
  <script defer>
  function toggle(ch) {
    fetch("/toggle?ch=" + ch);
  }
  if (!!window.EventSource) {
    const sourceEvents = new EventSource('/events');
    sourceEvents.addEventListener("update", function(e) {
      const [ch, state] = e.data.split(":");
      var index = ch.replace("ch", "");

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

  setupLights();

  setupWifi();

  setupMqtt();

  asyncWebServerRoutes();

  xTaskCreatePinnedToCore(TaskButtons, "TaskButtons", 4096, NULL, 1, &TaskButtonsHandle, 1);
  xTaskCreatePinnedToCore(TaskMQTT, "TaskMQTT", 8192, NULL, 1, &TaskMQTTHandle, 1);
}

void setupLights()
{
  for (int i = 0; i < NUM_CHANNELS; i++)
  {
    pinMode(ledPins[i], OUTPUT);
    pinMode(buttonPins[i], INPUT_PULLUP);
    digitalWrite(ledPins[i], LOW);
  }
}

void setupWifi()
{
  // Configures static IP address
  if (!WiFi.config(local_IP, gateway, subnet, primaryDNS, secondaryDNS))
  {
    Serial.println("STA Failed to configure");
  }
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED)
    delay(500);

  Serial.println("\nWiFi connected: ");
  Serial.println(WiFi.localIP());
}

void setupMqtt()
{
  espClient.setCACert(root_ca);

  mqttClient.setServer(mqtt_server, mqtt_port);
  mqttClient.setCallback(mqttCallback);
}

void asyncWebServerRoutes()
{
  // Async Web Server Routes
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send_P(200, "text/html", index_html);
  });

  server.on("/toggle", HTTP_GET, [](AsyncWebServerRequest *request) {
    if (request->hasParam("ch")) {
      int ch = request->getParam("ch")->value().toInt();
      if (ch >= 0 && ch < NUM_CHANNELS) toggleLight(ch);
    }
    
    request->send(200, "text/plain", "OK");
  });

  events.onConnect([](AsyncEventSourceClient *client) {
    for (int i = 0; i < NUM_CHANNELS; i++) {
      char msg[20];
      sprintf(msg, "ch%d:%s", i, lightStates[i] ? "ON" : "OFF");
      client->send(msg, "update", millis());
    }
  });

  server.addHandler(&events);
  server.begin();
}

void loop()
{
  // All logic handled in FreeRTOS tasks
}