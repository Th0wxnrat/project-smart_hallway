#include <WiFiS3.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <Servo.h>

// ====== เซนเซอร์ ======
#define DHTPIN 13
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const int analogPin = A0;     // Rain sensor
const int trigPin = 10;       // Ultrasonic
const int echoPin = 11;

// ====== Servo ======
Servo myservo;
int servoState = 0;  // 0 = เปิด, 90 = ปิด

// ====== โหมด ======
int Mode = 0; // 0 = Auto, 1 = Manual

// ====== WiFi & MQTT ======
char ssid[] = "LAPTOP-J0H9POJN 9330";
char pass[] = "Q83]j226";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

const char *mqtt_broker = "broker.hivemq.com";
const int mqtt_port = 1883;
const char *topic_data = "project/hallway/sensor/data";
const char *topic_control = "project/hallway/window/control";
const char *topic_status = "project/hallway/window/status";

// ====== Setup ======
void setup() {
  Serial.begin(115200);
  dht.begin();
  myservo.attach(9);
  myservo.write(0);  // เริ่มต้นเปิดหน้าต่าง

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  // WiFi
  while (WiFi.begin(ssid, pass) != WL_CONNECTED) {
    Serial.println("Connecting to WiFi...");
    delay(1000);
  }
  Serial.println("Connected to WiFi!");

  // MQTT
  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(mqttCallback);

  connectToMQTT();
}

// ====== Connect MQTT ======
void connectToMQTT() {
  while (!client.connected()) {
    String client_id = "arduino-client-";
    client_id += String(random(0xffff), HEX);
    Serial.print("Connecting to MQTT broker... ");
    if (client.connect(client_id.c_str())) {
      Serial.println("connected!");
      client.publish(topic_status, "Device connected", true);
      client.subscribe(topic_control);  // รับคำสั่ง
    } else {
      Serial.print("Failed. rc=");
      Serial.print(client.state());
      delay(2000);
    }
  }
}

// ====== MQTT Callback ======
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String command = "";
  for (unsigned int i = 0; i < length; i++) {
    command += (char)payload[i];
  }
  command.trim();

  Serial.print("Received Command: ");
  Serial.println(command);

  if (command == "A") {
    Mode = 0;
    client.publish(topic_status, "Mode: AUTO");
  } else if (command == "M") {
    Mode = 1;
    client.publish(topic_status, "Mode: MANUAL");
  } else if (command == "Open" && Mode == 1) {
    myservo.write(0);
    servoState = 0;
    client.publish(topic_status, "Window opened (manual)");
  } else if (command == "Close" && Mode == 1) {
    myservo.write(90);
    servoState = 90;
    client.publish(topic_status, "Window closed (manual)");
  }
}

// ====== อ่าน Ultrasonic ======
long readUltrasonic() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);
  long duration = pulseIn(echoPin, HIGH);
  long distance = duration * 0.034 / 2;
  return distance;
}

// ====== Loop ======
void loop() {
  if (!client.connected()) {
    connectToMQTT();
  }
  client.loop();

  // อ่านข้อมูลเซนเซอร์
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int rainVal = analogRead(analogPin);
  long distance = readUltrasonic();

  // ตรวจสอบค่า DHT
  if (isnan(t) || isnan(h)) {
    Serial.println("Failed to read from DHT sensor!");
    return;
  }

  // โหมด AUTO เปิด-ปิดหน้าต่างอัตโนมัติ
  if (Mode == 0) {
    if (rainVal < 500 && servoState != 90) {
      myservo.write(90);  // ปิดหน้าต่าง
      servoState = 90;
      client.publish(topic_status, "Window closed (auto)");
    } else if (rainVal >= 500 && servoState != 0) {
      myservo.write(0);   // เปิดหน้าต่าง
      servoState = 0;
      client.publish(topic_status, "Window opened (auto)");
    }
  }

  // ส่งข้อมูล JSON
  String payload = "{";
  payload += "\"temperature\":" + String(t, 1) + ",";
  payload += "\"humidity\":" + String(h, 1) + ",";
  payload += "\"rain\":" + String(rainVal) + ",";
  payload += "\"distance\":" + String(distance) + ",";
  payload += "\"mode\":\"" + String((Mode == 0) ? "AUTO" : "MANUAL") + "\",";
  payload += "\"window_position\":" + String(servoState);
  payload += "}";

  client.publish(topic_data, payload.c_str());
  Serial.println("Published: " + payload);

  delay(2000);  // ทุก 2 วิ
}
