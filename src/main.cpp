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
#define RELAYPIN D0
#define RELAYNAME "Christmas Lights"

Ticker ticker;
WiFiManager wm;
fauxmoESP fauxmo;

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
  digitalWrite(RELAYPIN, HIGH);

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
