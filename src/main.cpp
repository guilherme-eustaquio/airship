#include <Arduino.h>

#include <ESP8266WiFi.h>
#include <ESP8266WiFiMulti.h>
#include <ArduinoJson.h>

#include <WebSocketsClient.h>
#include <ESP8266WiFi.h>

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

ESP8266WiFiMulti WiFiMulti;
WebSocketsClient webSocket;
String UNIQUE_ID  = "";

String getAirShipStatus() {

  StaticJsonDocument<500> doc;
  String airShipStatus = "";
  
  doc["id"] = UNIQUE_ID;
  doc["status"] = ALIVE;
  JsonObject command = doc.createNestedObject("command");
  command["action"] = TURN_ON;
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
  String airshipStatus = "";

  doc["status"] = ALIVE;
  doc["command"]["type"] = CONFIRM_MESSAGE_FROM_AIRSHIP;

  serializeJson(doc, airshipStatus);
  digitalWrite(LIGHT_PIN, action);
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

void setup() {

	USE_SERIAL.begin(115200);
	USE_SERIAL.setDebugOutput(true);
  pinMode(LIGHT_PIN, OUTPUT);

	for(uint8_t t = 4; t > 0; t--) {
		USE_SERIAL.printf("[SETUP] BOOT WAIT %d...\n", t);
		USE_SERIAL.flush();
		delay(1000);
	}


  WiFi.enableAP(false);
	WiFiMulti.addAP("Wifi", "password");

	while(WiFiMulti.run() != WL_CONNECTED) {
		delay(100);
	}

  setUniqueId();
  //webSocket.begin("192.168.1.9", 7070, "/airships");
  //webSocket.begin("192.168.100.155", 7070, "/airships");
  webSocket.begin("192.168.100.22", 7070, "/airships");
	webSocket.onEvent(webSocketEvent);
	webSocket.setReconnectInterval(5000);
  webSocket.enableHeartbeat(15000, 3000, 2);
}

void loop() {
  webSocket.loop();
}