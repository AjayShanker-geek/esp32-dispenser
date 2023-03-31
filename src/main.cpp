#include <Arduino.h>
#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <WiFi.h>

#include "secret.h"
#include "topic.h"

#define MOTOR_PIN 14
#define DOCK_RIGHT_SWT 33
#define DOCK_LEFT_SWT 25
#define MOTOR_SWT 32

WiFiClient espClient;
PubSubClient client(espClient);
unsigned long lastMsg = 0;
#define MSG_BUFFER_SIZE (50)
char msg[MSG_BUFFER_SIZE];

bool dock_state = false;
bool dock_state_old = false;
int dock_right_state = 0;
int dock_left_state = 0;
int motor_state = 0;

void setup_wifi() {
  delay(10);
  // We start by connecting to a WiFi network
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(ssid);

  WiFi.begin(ssid, password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  randomSeed(micros());

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    // Create a random client ID
    String clientId = "ESP32_DISPENSOR";
    // clientId += String(random(0xffff), HEX);
    // Attempt to connect
    if (client.connect(clientId.c_str())) {
      Serial.println("connected");
      client.subscribe(TABLE_TOPIC);
    } else {
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      // Wait 5 seconds before retrying
      delay(5000);
    }
  }
}

void dispense_can() {
  Serial.println("Dispensing");
  while (!digitalRead(MOTOR_SWT)) {
    digitalWrite(MOTOR_PIN, HIGH);
    delay(2);
  }
  // delay(50);
  digitalWrite(MOTOR_PIN, LOW);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)payload[i]);
  }
  Serial.println();

  // Using ArduinoJson
  StaticJsonDocument<256> doc;
  deserializeJson(doc, payload, length);

  strlcpy(msg, doc["data"], sizeof(msg));

  int table = msg[0] - '0';  // Convert char to int
  Serial.println(table);

  //
  if (dock_state)
    dispense_can();
}

void setup() {
  Serial.begin(115200);
  pinMode(BUILTIN_LED, OUTPUT);

  pinMode(MOTOR_PIN, OUTPUT);
  pinMode(DOCK_LEFT_SWT, INPUT);
  pinMode(DOCK_RIGHT_SWT, INPUT);
  pinMode(MOTOR_SWT, INPUT);

  setup_wifi();
  client.setServer(mqtt_server, 1883);
  client.setCallback(callback);

  digitalWrite(BUILTIN_LED, HIGH);
}

void loop() {
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  unsigned long now = millis();
  if (now - lastMsg > 2000) {
    lastMsg = now;

    // Get lastest value
    dock_left_state = digitalRead(DOCK_LEFT_SWT);
    dock_right_state = digitalRead(DOCK_RIGHT_SWT);
    // motor_state = digitalRead(MOTOR_SWT);

    if (dock_left_state == 1 && dock_right_state == 1) {
      dock_state = true;
    } else
      dock_state = false;

    if (dock_state_old != dock_state) {
      snprintf(msg, MSG_BUFFER_SIZE, "{ \"data\": \"%s\" }", dock_state ? "True" : "False");
      // Serial.print("Publish message: ");
      Serial.println(msg);
      client.publish(DISPENSER_TOPIC, msg);
    }

    // monitor previous value
    dock_state_old = dock_state;
  }
}
