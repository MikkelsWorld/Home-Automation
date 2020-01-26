This includes the sensors below.

BME280
- Pin D1 & D2

Movement Sensor
- Pin D3

RGB LED
- RED: D5
- GRE: D6
- BLU: D7

Relay
- Pin D4

Then simply flash the ESP 8266 with the code in the .ino file, correct your credentials and it should connect to your network.

From there you can use Home Assistant to publish MQTT topics, which should be received on the sensor node.

Then modify your Home Assistant config with the code in the .txt file, and restart.

You should now be able to find them as Entities in Home Assistant