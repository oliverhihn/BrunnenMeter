// BrunnenMeter 2.1

/*
  Pinout:

  - Sensor
  Arduino pin 5 -> HX711 CLK
  6 -> DAT
  5V -> VCC
  GND -> GND

  - Relay
  4 -> IN1
  5V -> VCC
  GND -> GND
*/

#include <HX711.h>
#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <ESP8266mDNS.h>
#include <WiFiUdp.h>
#include <ArduinoOTA.h>

/************ WIFI and MQTT Information (CHANGE THESE FOR YOUR SETUP) ******************/
const char* ssid = "Mesh"; //type your WIFI information inside the quotes
const char* password = "eDs4pj#DO/9m6|vb";
const char* mqtt_server = "192.168.2.221";
const char* mqtt_username = "oliver";
const char* mqtt_password = "Oliver#2";
const int mqtt_port = 1883;

WiFiClient espClient;
PubSubClient client(espClient);

/************************************ OTA SETUP ****************************************/
#define SENSORNAME "brunnenMeter" //change this to whatever you want to call your device
#define OTApassword "yourOTApassword" //the password you will need to enter to upload remotely via the ArduinoIDE
int OTAport = 8266;

/************* MQTT TOPICS (change these topics as you wish)  **************************/
const char* brunnen_send_topic = "brunnenMeter/get";
const char* brunnen_command_topic = "brunnenMeter";

String state = "idle";
String oldState = "idle";



/**************************************** FOR JSON ***************************************/
const int BUFFER_SIZE = JSON_OBJECT_SIZE(10);
#define MQTT_MAX_PACKET_SIZE 512



/**************************************** FOR SENSOR ***************************************/
HX711 scale;
#define CLK D5
#define DOUT D6

#define RELAY_PIN D7

float cmValue = 0;
float waterValue = 0;

unsigned long myTime = 0;

void setup() {
  Serial.begin(115200);

  scale.begin(DOUT, CLK);
  scale.set_offset(-2159572);

  // WLAN and MQTT Setup
  setup_wifi();
  client.setServer(mqtt_server, mqtt_port);

  //OTA SETUP
  ArduinoOTA.setPort(OTAport);
  // Hostname defaults to esp8266-[ChipID]
  ArduinoOTA.setHostname(SENSORNAME);

  // No authentication by default
  ArduinoOTA.setPassword((const char *)OTApassword);

  ArduinoOTA.onStart([]() {
    Serial.println("Starting");
  });
  ArduinoOTA.onEnd([]() {
    Serial.println("\nEnd");
  });
  ArduinoOTA.onProgress([](unsigned int progress, unsigned int total) {
    Serial.printf("Progress: %u%%\r", (progress / (total / 100)));
  });
  ArduinoOTA.onError([](ota_error_t error) {
    Serial.printf("Error[%u]: ", error);
    if (error == OTA_AUTH_ERROR) Serial.println("Auth Failed");
    else if (error == OTA_BEGIN_ERROR) Serial.println("Begin Failed");
    else if (error == OTA_CONNECT_ERROR) Serial.println("Connect Failed");
    else if (error == OTA_RECEIVE_ERROR) Serial.println("Receive Failed");
    else if (error == OTA_END_ERROR) Serial.println("End Failed");
  });
  ArduinoOTA.begin();


  Serial.println("Ready");
  Serial.print("IP Address: ");
  Serial.println(WiFi.localIP());


  pinMode(RELAY_PIN, OUTPUT);
  digitalWrite(RELAY_PIN, HIGH);
}

/********************************** SETUP WIFI HELPER-FUNCTION *****************************************/
void setup_wifi() {
  delay(10);

  // connect to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}



/********************************** START RECONNECT*****************************************/
void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection... ");
    // Attempt to connect
    if (client.connect(SENSORNAME, mqtt_username, mqtt_password)) {
      Serial.println("connected");
      client.subscribe(brunnen_command_topic);
      // sendState();
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

float meassureWaterlevel() {
  scale.power_up();        // wake up the ADC

  digitalWrite(RELAY_PIN, LOW);

  delay(1750);

  digitalWrite(RELAY_PIN, HIGH);

  delay(2000);  // wait for preassure to normalise before meassuring

  //map the raw data to a human readable:
  cmValue = map(scale.get_units(), 0, 4594413, 0, 92);          // meassured 90cm waterdepth at a sensor reading of 1930

  Serial.print("Wasserstand:\t");
  Serial.println(String(cmValue) + String(" cm"));

  if (cmValue <= 47) {                      // calculate the volume in litre:
    waterValue  = cmValue * 10 * 0.503;     // footprint of zylindre * hight (cmValue)
  }                                         // footprint of well: 0.503m² for the first 47cm

  if (cmValue > 47) {
    cmValue -= 47;                          // substract 47cm to calculate this part hardcoded
    waterValue = 47 * 10 * 0.503;           // hardcoded for the 47cm part of the well
    waterValue += cmValue * 10 * 0.754;     // after that it is 0.754m² (calculate the rest with other footprint)
  }

  Serial.print("Wassermänge:\t");
  Serial.println(String(waterValue) + String(" Liter"));

  scale.power_down();                       // put the ADC in sleep mode

  if (waterValue < 0) waterValue = 0;

  return waterValue;
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }

  if (WiFi.status() != WL_CONNECTED) {
    delay(1);
    Serial.print("WIFI Disconnected. Attempting reconnection.");
    setup_wifi();
    return;
  }

  client.loop();

  ArduinoOTA.handle();

  if (millis() - myTime > 600000) { // meassure every half hour
    myTime = millis();
    client.publish(brunnen_send_topic, String(meassureWaterlevel()).c_str(), true);
  }
}
