#include <FS.h>
#include <ESP8266WiFi.h>
#include <DNSServer.h>
#include <ESP8266WebServer.h>
#include <WiFiManager.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Ticker.h>
#include <fauxmoESP.h>
#include ".secrets"

#define LED D4
#define RELAYPIN D5
#define RELAYNAME "Christmas Lights"

Ticker ticker;
WiFiManager wm;
fauxmoESP fauxmo;

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

void printWiFiConfig()
{
  Serial.print("\nlocal ip: ");
  Serial.print(WiFi.localIP());
  Serial.print("\tsubnet mask: ");
  Serial.print(WiFi.subnetMask());
  Serial.print("\tgateway ip: ");
  Serial.println(WiFi.gatewayIP());
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

void setupFauxmo()
{
  fauxmo.createServer(true); // not needed, this is the default value
  fauxmo.setPort(80);        // This is required for gen3 devices
  fauxmo.enable(true);
  fauxmo.addDevice(RELAYNAME);
  
  fauxmo.onSetState([](unsigned char device_id, const char * device_name, bool state, unsigned char value) {
    Serial.printf("[MAIN] Device #%d (%s) state: %s value: %d\n", device_id, device_name, state ? "ON" : "OFF", value);
    // Note HIGH and LOW are reversec; this is a quirk of the JCC-3FF low level trigger relay being used with 3.3V
    // instead of 5v
    if (strcmp(device_name, RELAYNAME) == 0)
      digitalWrite(RELAYPIN, state ? LOW : HIGH);
  });
}

void setup()
{
    wm.setConnectTimeout(30);      // how long to try to connect for before continuing
    wm.setConfigPortalTimeout(90); // auto close configportal after n seconds
    wm.setSaveConnectTimeout(30);  // how long to try a newly saved config
    wm.setAPClientCheck(true);     // avoid timeout if client connected to softap
    wm.setAPCallback(configModeCallback);

    WiFi.mode(WIFI_STA); // explicitly set mode, esp defaults to STA+AP

    // wipe settings (for testing)    //
    //================================//
    // wm.resetSettings();            //
    // SPIFFS.format();               //
    //================================//

    Serial.begin(115200);
    pinMode(LED, OUTPUT);
    pinMode(RELAYPIN, OUTPUT);
    delay(3000);
    digitalWrite(RELAYPIN, LOW);

    Serial.println("\n Searching for connection");
    ticker.attach(1, tick); // blink slowly while trying to connect

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

      Serial.println("Connection successful");
      wm.stopConfigPortal(); //ensure port 80 is free for Alexa
      printWiFiConfig();
      ticker.detach();
      digitalWrite(LED, LOW); //keep LED on; esp8266 is reverse polarity for builtin led

      setupOTA();
      setupFauxmo();

      Serial.println("All setup complete");
    }
}

void loop()
{
    ArduinoOTA.handle();
    fauxmo.handle();

}
