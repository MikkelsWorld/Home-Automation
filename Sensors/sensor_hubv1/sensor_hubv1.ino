#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <PubSubClient.h>
#include <Wire.h>
#include <Adafruit_Sensor.h>
#include <Adafruit_BME280.h>

//Setup WiFi and MQTT Broker
#define wifi_ssid ""
#define wifi_password ""
#define mqtt_server "" //OR server of Home Assistent
#define mqtt_user "" 
#define mqtt_password ""
#define mqtt_port 1883
#define mqtt_topic_publish ""
#define mqtt_topic_subscribe ""

//Pins
#define pirPin 0 //D3
#define fanPin 2 // D4 Relay for Fan
#define redPin 14 //D5
#define grePin 12 //D6
#define bluPin 13 //D7

//Lib setup
WiFiClient espClient;
PubSubClient client(espClient);
#define SEALEVELPRESSURE_HPA (1013.25)
Adafruit_BME280 bme;

//MQTT
#define SENSORNAME "toilet"
#define MQTT_MAX_PACKET_SIZE 512
const int BUFFER_SIZE = 300;

//Sensor definitions
float tempValue = 0.0;
float tempNew = 0.0;
float tempDiff = 0.1;

float humValue = 0.0;
float humNew = 0.0;
float humDiff = 0.5;

bool pirStatus = 0;
bool pirStatusPrev = 0;

bool fanStatus = false;
bool fanStatusPrev = false;

int timerLast;
int timerNow;
int timerInterval = 60000; //Every 60 second in millis.

void setup() {
  Serial.begin(115200);
  Serial.println("Starting Node named " + String(SENSORNAME));
  
  //Pin Setup
  pinMode(redPin, OUTPUT);
  pinMode(grePin, OUTPUT);
  pinMode(bluPin, OUTPUT); //Bootup / Flash pin
  pinMode(pirPin, INPUT); //Movement Sensor
  pinMode(fanPin, OUTPUT); //FAN Relay

  digitalWrite(redPin, LOW);
  digitalWrite(grePin, LOW);
  digitalWrite(bluPin, LOW);

  Serial.println("LED: DONE");

  //Wifi Setup
  Serial.println();
  Serial.print("Connecting to ");
  Serial.println(wifi_ssid);

  WiFi.mode(WIFI_STA);
  WiFi.begin(wifi_ssid, wifi_password);

  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
  }

  Serial.println("");
  Serial.println("WiFi connected");
  Serial.println("IP address: ");
  Serial.println(WiFi.localIP());
  
  Serial.println("WIFI: DONE");

  //MQTT Setup
  client.setServer(mqtt_server, mqtt_port);
  client.setCallback(callback);

  Serial.println("MQTT: Done");

  //Sensor setup
  bme.begin(0x76); 

  //Timer DATA
  timerNow = millis();

  //Initial connnect to Wifi.
  reconnect();
  readTimedSensors();

  //Blink 3 times to show we are live.
  blink(grePin, 3);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Message arrived [");
  Serial.print(topic);
  Serial.print("] ");

  char message[length + 1];
  for (int i = 0; i < length; i++) {
    message[i] = (char)payload[i];
  }
  message[length] = '\0';
  Serial.println(message);

  if (!processMessage(message)) {
    return;
  }

}

bool processMessage(char* message) {
  Serial.println("Message Received.. Decoding..");
  const size_t CAPACITY = JSON_OBJECT_SIZE(BUFFER_SIZE);
  StaticJsonDocument<CAPACITY> doc;

  DeserializationError err = deserializeJson(doc, message);

  if(err) {
    Serial.print(F("deserializeJson() failed with code "));
    Serial.println(err.c_str());
    return false;
  }

  fanStatus = doc["fanStatus"];

  Serial.println("Message decoded.");
  return true;
}

void reconnect() {
  // Loop until we're reconnected
  while (!client.connected()) {
    Serial.print("Attempting MQTT connection...");
    blink(redPin, 2);

    if (client.connect(SENSORNAME, mqtt_user, mqtt_password)) {
      Serial.println("connected");
      //ToDo: Reset sensor values in prep for reconnect.
      client.subscribe(mqtt_topic_subscribe);
      client.publish(mqtt_topic_publish, "ONLINE!");
      blink(bluPin, 1);
    } else {
      blink(redPin, 5);
      Serial.print("failed, rc=");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
    }
  }
}

void blink(char pin, int times) {
  Serial.println("BLINK");
  for (size_t i = 0; i < times; i++)
  {
    digitalWrite(pin, HIGH);
    delay(100);
    digitalWrite(pin, LOW);
    delay(100);
    i = i++;
  }
}

void SendState() {
  Serial.println("Sending new state..");
  const size_t CAPACITY = JSON_OBJECT_SIZE(BUFFER_SIZE);
  StaticJsonDocument<CAPACITY> doc;
  JsonObject object = doc.to<JsonObject>();

  object["name"] = SENSORNAME;
  object["temp"] = tempNew;
  object["humidity"] = humNew;
  object["movement"] = pirStatus;
  object["fan"] = fanStatus;

  char buffer[300]; //Create char with document length.
  serializeJson(doc, buffer); //Serialize and post the document to buffer.
  client.publish(mqtt_topic_publish, buffer, true);
  blink(grePin, 1);
  Serial.println("State was published!");
}

bool difference(float oldVal, float newVal, float diff) {
  float calculatedDiff = fabs(oldVal - newVal);
  return (calculatedDiff > diff);
}

void loop() {
  // put your main code here, to run repeatedly:
  client.loop();

  //Increase timerInterval for time based collection of data.
  timerNow = millis();
  
  if (abs(timerLast - timerNow) > timerInterval) {
    readTimedSensors();
    timerLast = millis();
  }

  readSensors();
  trigger();

  if (difference(tempValue, tempNew, tempDiff) || difference(humValue, humNew, humDiff) || pirStatusPrev != pirStatus || fanStatusPrev != fanStatus) {
    SendState();
  }

  //Set corresponding values.
  tempValue = tempNew;
  humValue = humNew;
  pirStatusPrev = pirStatus;
  fanStatusPrev = fanStatus;
}

//Allows us to run the trigger every second, to make the loop a bit less heavy.
void trigger() {
  if (fanStatus) {
    digitalWrite(fanPin, HIGH);
  } else {
    digitalWrite(fanPin, LOW);
  }
}

void readSensors(){
  if(digitalRead(pirPin)==HIGH) {
    pirStatus = true; //Movement.
  } else {
    pirStatus = false; //No movement.
  }
}

void readTimedSensors() {
  Serial.println("Reading Sensors..");

  // Get humidity / Temp event and print its value.
  Serial.print(F("Temperature: "));
  Serial.print(bme.readTemperature());
  Serial.println(F("°C"));

  tempNew = bme.readTemperature();

  // Get humidity event and print its value.
  Serial.print(F("Humidity: "));
  Serial.print(bme.readHumidity());
  Serial.println(F("%"));

  humNew = bme.readHumidity();

  //End humidity and temp event.
  Serial.println("Reading Sensors complete..");
}