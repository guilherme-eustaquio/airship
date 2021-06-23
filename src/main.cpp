#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoJson.h>
#include <WebSocketsClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>
#include <Hash.h>

#define USE_SERIAL Serial
#define LIGHT_PIN 14
#define SEND_MESSAGE_FROM_AIRSHIP 1
#define SEND_MESSAGE_FROM_CALBANIA 2
#define CONFIRM_MESSAGE_FROM_AIRSHIP 3
#define BROADCAST_TO_CALBANIA_CLIENTS 4
#define TURN_OFF 0
#define TURN_ON 1
#define DEAD 0
#define ALIVE 1
#define MODEL_NAME "airship"

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
String UNIQUE_ID  = "";
int CURRENT_STATUS = HIGH;

String getAirShipStatus() {

  StaticJsonDocument<500> doc;
  String airShipStatus = "";
  
  doc["id"] = UNIQUE_ID;
  doc["status"] = ALIVE;
  JsonObject command = doc.createNestedObject("command");
  command["action"] = CURRENT_STATUS;
  command["type"] = SEND_MESSAGE_FROM_AIRSHIP;

  serializeJson(doc, airShipStatus);

  return airShipStatus;
}

void receiveCommand(const char * json) {

  StaticJsonDocument<500> doc;

  DeserializationError error = deserializeJson(doc, json);

  if(error) {
    Serial.print(F("deserializeJson() failed: "));
    Serial.println(error.f_str());
    return;
  }

  int action = doc["command"]["action"];
  CURRENT_STATUS = action;
  String airshipStatus = "";

  doc["status"] = ALIVE;
  doc["command"]["type"] = CONFIRM_MESSAGE_FROM_AIRSHIP;

  serializeJson(doc, airshipStatus);
  digitalWrite(LIGHT_PIN, CURRENT_STATUS);
  USE_SERIAL.printf(airshipStatus.c_str());
  webSocket.sendTXT((const uint8_t*) airshipStatus.c_str(), airshipStatus.length());
}

void webSocketEvent(WStype_t type, uint8_t * payload, size_t length) {

	switch(type) {
		case WStype_DISCONNECTED:
			USE_SERIAL.printf("[WSc] Disconnected!\n");
			break;
		case WStype_CONNECTED: {
			USE_SERIAL.printf("[WSc] Connected to url: %s\n", payload);
      const String status = getAirShipStatus(); 
			webSocket.sendTXT((const uint8_t*) status.c_str(), status.length());
		}
			break;
		case WStype_TEXT:
			USE_SERIAL.printf("[WSc] get text: %s\n", payload);
      receiveCommand((const char*) payload);
			break;
    case WStype_PING: 
        USE_SERIAL.printf("[WSc] get ping\n");
        break;
    case WStype_PONG:
        USE_SERIAL.printf("[WSc] get pong\n");
        break;
    }
}

void setUniqueId() {
  char hex[10] = "";
  sprintf(hex, "%x", ESP.getFlashChipId());
  UNIQUE_ID = String(hex);
}

void configOTA() {
  
    ArduinoOTA.setHostname(MODEL_NAME);

    ArduinoOTA.onStart([]() {
      String type;

      if (ArduinoOTA.getCommand() == U_FLASH) {
        type = "sketch";
      } else { // U_FS
        type = "filesystem";
      }

      // NOTE: if updating FS this would be the place to unmount FS using FS.end()
      USE_SERIAL.println("Start updating " + type);
  });

  ArduinoOTA.onEnd([]() {
    USE_SERIAL.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    USE_SERIAL.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    USE_SERIAL.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) {
      USE_SERIAL.println("Auth Failed");
    } else if (error == OTA_BEGIN_ERROR) {
      USE_SERIAL.println("Begin Failed");
    } else if (error == OTA_CONNECT_ERROR) {
      USE_SERIAL.println("Connect Failed");
    } else if (error == OTA_RECEIVE_ERROR) {
      USE_SERIAL.println("Receive Failed");
    } else if (error == OTA_END_ERROR) {
      USE_SERIAL.println("End Failed");
    }
  });

  ArduinoOTA.begin();

}

void setup() {

	USE_SERIAL.begin(115200);
	USE_SERIAL.setDebugOutput(true);
  pinMode(LIGHT_PIN, OUTPUT);
  digitalWrite(LIGHT_PIN, CURRENT_STATUS);

	for(uint8_t t = 4; t > 0; t--) {
		USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
		USE_SERIAL.flush();
		delay(1000);
	}

  WiFi.enableAP(false);
	WiFiMulti.addAP("Rede", "senha123");


	while(WiFiMulti.run() != WL_CONNECTED) {
		delay(100);
	}

  configOTA();

  setUniqueId();
  webSocket.begin("calbania.local", 7070, "/airships");
	webSocket.onEvent(webSocketEvent);
	webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2);
}

void loop() {
  ArduinoOTA.handle();
  webSocket.loop();
}

