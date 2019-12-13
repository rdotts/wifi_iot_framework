#include <FS.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ArduinoJson.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Ticker.h>
#include <AsyncMqttClient.h>
#include ".secrets"

#define LED LED_BUILTIN

Ticker ticker;
WiFiManager wm;
// Default MQTT server can be overwritten in config mode
char mqtt_server[40] = "192.168.1.1";
char mqtt_port[6] = "1883";
bool shouldSaveConfig = false;

void tick()
{
  digitalWrite(LED, !digitalRead(LED)); // set pin to the opposite state
}

//gets called when WiFiManager enters configuration mode
void configModeCallback(WiFiManager *myWiFiManager)
{
  Serial.println("Entered config mode");
  Serial.println(WiFi.softAPIP());
  //if you used auto generated SSID, print it
  Serial.println(myWiFiManager->getConfigPortalSSID());
  // blink quickly while in config mode
  ticker.detach();
  ticker.attach(0.2, tick);
}

void preSaveConfigCallback()
{
  Serial.println("Config needs to be saved");
  shouldSaveConfig = true;
}

String getParam(String name)
{
  //read parameter from server
  String value;
  if (wm.server->hasArg(name))
  {
    value = wm.server->arg(name);
  }
  return value;
}

void setupSpiffs()
{
  Serial.println("mounting FS...");
  if (SPIFFS.begin())
  {
    Serial.println("mounted file system");
    if (SPIFFS.exists("/config.json"))
    {
      //file exists, reading and loading
      Serial.println("reading config file");
      File configFile = SPIFFS.open("/config.json", "r");
      if (configFile)
      {
        Serial.println("opened config file");
        size_t size = configFile.size();
        // Allocate a buffer to store contents of the file.
        std::unique_ptr<char[]> buf(new char[size]);

        configFile.readBytes(buf.get(), size);
        DynamicJsonDocument jsonBuffer(1024);
        DeserializationError error = deserializeJson(jsonBuffer, buf.get());
        if (error)
        {
          Serial.println("Error parsing stored config");
          return;
        }
        serializeJson(jsonBuffer, Serial);

        strcpy(mqtt_server, jsonBuffer["mqtt_server"]);
        strcpy(mqtt_port, jsonBuffer["mqtt_port"]);
      }
    }
  }
  else
    Serial.println("failed to mount FS");
}

void saveSpiffs()
{
  Serial.println("saving config");
  DynamicJsonDocument jsonBuffer(1024);
  jsonBuffer["mqtt_server"] = mqtt_server;
  jsonBuffer["mqtt_port"] = mqtt_port;

  File configFile = SPIFFS.open("/config.json", "w");
  if (!configFile)
  {
    Serial.println("failed to open config file for writing");
  }
  serializeJsonPretty(jsonBuffer, Serial);
  serializeJson(jsonBuffer, configFile);

  configFile.close();
  shouldSaveConfig = false;
}

void printWiFiConfig()
{
  Serial.print("\nlocal ip: ");
  Serial.print(WiFi.localIP());
  Serial.print("\tsubnet mask: ");
  Serial.println(WiFi.subnetMask());
  Serial.print("gateway ip: ");
  Serial.print(WiFi.gatewayIP());
  Serial.print("\tMQTT broker: ");
  Serial.print(mqtt_server);
  Serial.print(":");
  Serial.println(mqtt_port);
  Serial.println("");
}

void setupOTA()
{
  // ArduinoOTA.setPort(8266);
  // ArduinoOTA.setHostname("myesp8266"); //defaults to esp8266-[ChipID]
  ArduinoOTA.setPassword((const char *)OTA_PASS); //defaults to none

  ArduinoOTA.onStart([]() {
    Serial.println("Starting OTA update");
  });

  ArduinoOTA.onEnd([]() {
    Serial.println("\nDone");
  });

  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });

  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR)
      Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR)
      Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR)
      Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR)
      Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR)
      Serial.println("End Failed");
  });
  ArduinoOTA.begin();
}

void setup()
{
  wm.setConnectTimeout(30);      // how long to try to connect for before continuing
  wm.setConfigPortalTimeout(90); // auto close configportal after n seconds
  wm.setSaveConnectTimeout(30);  // how long to try a newly saved config
  wm.setAPClientCheck(true);     // avoid timeout if client connected to softap
  wm.setAPCallback(configModeCallback);
  wm.setPreSaveConfigCallback(preSaveConfigCallback);
  WiFiManagerParameter custom_mqtt_server("server", "mqtt server", mqtt_server, 40);
  WiFiManagerParameter custom_mqtt_port("port", "mqtt port", mqtt_port, 6);
  wm.addParameter(&custom_mqtt_server);
  wm.addParameter(&custom_mqtt_port);

  WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

  // wipe settings (for testing)    //
  //================================//
  // wm.resetSettings();            //
  // SPIFFS.format();               //
  //================================//

  Serial.begin(115200);
  pinMode(LED, OUTPUT);
  delay(3000);
  Serial.println("\n Searching for connection");
  ticker.attach(1, tick); // blink slowly while trying to connect

  setupSpiffs();

  if (!wm.autoConnect())
  { // auto generated AP name from chipid)
    // Reboot to alternate between trying known connection and allowing config via AP until one of them works
    Serial.println("Failed to connect");
    ESP.restart();
    delay(5000);
  }
  else
  {
    //if you get here you have connected to the WiFi
    Serial.println("Connected to WiFi");
    // Steady LED shows active connection
    ticker.detach();
    digitalWrite(LED, LOW);

    //read updated parameters
    strcpy(mqtt_server, custom_mqtt_server.getValue());
    strcpy(mqtt_port, custom_mqtt_port.getValue());

    //save the custom parameters to FS
    if (shouldSaveConfig)
    {
      saveSpiffs();
    }

    Serial.println("Connection successful");
    printWiFiConfig();
    ticker.detach();
    digitalWrite(LED, LOW); //keep LED on; esp8266 is reverse polarity for builtin led

    setupOTA();

    Serial.println("All setup complete");
  }
}

void loop()
{
  ArduinoOTA.handle();
}
