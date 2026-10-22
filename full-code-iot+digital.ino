#include <WiFi.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <Wire.h>
#include <LiquidCrystal_I2C.h>

// ===== LCD =====
LiquidCrystal_I2C lcd(0x27, 16, 2);

// ===== Sensor & Pins =====
#define DHTPIN 4
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const int analogPin = 32;   // Rain sensor (ADC pin)
const int trigPin = 5;      
const int echoPin = 18;
const int servoPin = 19;

// ===== ขาเชื่อมต่อ GAL16V8 (แบบ 3 เส้น) =====
const int galPin2 = 12; // MSB
const int galPin1 = 13; 
const int galPin0 = 14; // LSB

// ===== ตัวแปรเก็บค่าเซนเซอร์แบบ Global =====
long currentDistance = 999;
float currentTemp = 0.0;
float currentHum = 0.0;
int rainVal = 0;

// ===== Servo =====
Servo myservo;
int servoState = 0; // 0 = Closed, 90 = Open

// ===== Mode =====
int Mode = 0; // 0 = Auto, 1 = Manual

// ===== ตัวแปรจัดการเวลา (Timer) =====
unsigned long lastSensorPublish = 0;
const unsigned long sensorPublishInterval = 3000;

// ===== WiFi & MQTT =====
char ssid[] = "LAPTOP-J0H9POJN 9330";
char pass[] = "Q83]j226";

WiFiClient wifiClient;
PubSubClient client(wifiClient);

const char* mqtt_broker = "broker.hivemq.com";
const char* mqtt_username = "co2";
const char* mqtt_password = "co2";
const int mqtt_port = 1883;

const char* topic_data = "project/hallway/sensor/data";
const char* topic_control = "project/hallway/window/control";
const char* topic_status = "project/hallway/window/status";

// ===== ฟังก์ชัน Update LCD =====
void updateLCD(String line1, String line2) {
  static String lastLine1 = "";
  static String lastLine2 = "";

  if (line1 != lastLine1) {
    lcd.setCursor(0,0);
    lcd.print("                ");
    lcd.setCursor(0,0);
    lcd.print(line1);
    lastLine1 = line1;
  }

  if (line2 != lastLine2) {
    lcd.setCursor(0,1);
    lcd.print("                ");
    lcd.setCursor(0,1);
    lcd.print(line2);
    lastLine2 = line2;
  }
}

// ===== ฟังก์ชันส่งรหัสไปหา GAL =====
void sendToGAL(int number) {
  digitalWrite(galPin0, number & 0x01);        // บิตที่ 0 (ขา 14)
  digitalWrite(galPin1, (number >> 1) & 0x01); // บิตที่ 1 (ขา 13)
  digitalWrite(galPin2, (number >> 2) & 0x01); // บิตที่ 2 (ขา 12)
}

// ===== ฟังก์ชันนับถอยหลัง =====
void countdownToClose(String mqttMsg, String lcdMsg) {
  for (int i = 5; i >= 0; i--) { 
    sendToGAL(i); 
    updateLCD(lcdMsg, "Closing in: " + String(i) + "s ");
    
    Serial.print("Countdown: "); 
    Serial.println(i);
    
    delay(1000); 
    client.loop(); 
  }
  sendToGAL(7); 
  
  myservo.write(0); 
  servoState = 0;
  client.publish(topic_status, mqttMsg.c_str()); 
  updateLCD(lcdMsg, "Window Closed");
  Serial.println("--- Window Closed! ---");
}

// ===== Ultrasonic =====
long readUltrasonic() {
  digitalWrite(trigPin, LOW);
  delayMicroseconds(2);
  digitalWrite(trigPin, HIGH);
  delayMicroseconds(10);
  digitalWrite(trigPin, LOW);

  long duration = pulseIn(echoPin, HIGH, 30000); 
  if (duration == 0) return 999; 
  return duration * 0.034 / 2;
}

// ===== MQTT Callback =====
void mqttCallback(char* topic, byte* payload, unsigned int length) {
  String command = "";
  for (unsigned int i = 0; i < length; i++) {
    command += (char)payload[i];
  }
  command.trim();
  
  Serial.print(">> MQTT Received: "); 
  Serial.println(command);

  if (command == "A") {
    Mode = 0;
    client.publish(topic_status, "Mode: AUTO");
  }
  else if (command == "M") {
    Mode = 1;
    client.publish(topic_status, "Mode: MANUAL");
  }
  else if (Mode == 1) { 
    if (command == "Open" && currentDistance > 5) {
      myservo.write(90);
      servoState = 90;
      client.publish(topic_status, "Window opened (manual)");
      updateLCD("Mode: MANUAL", "Window Opened");
    }
    if (command == "Close" && currentDistance > 5 && servoState != 0) {
      Serial.println("\n[MANUAL] App command received. Closing window...");
      countdownToClose("Window closed (manual)", "Mode: MANUAL");
    }
  }
}

// ===== MQTT Connect =====
void connectToMQTT() {
  while (!client.connected()) {
    Serial.print("Connecting to MQTT..."); 
    String client_id = "esp32-client-";
    client_id += String(random(0xffff), HEX);
    
    if (client.connect(client_id.c_str(), mqtt_username, mqtt_password)) {
      Serial.println(" OK!");
      client.publish(topic_status, "Device connected", true);
      client.subscribe(topic_control);
    } else {
      Serial.println(" Failed! Retrying in 2 sec...");
      delay(2000);
    }
  }
}

// ===== Setup =====
void setup() {
  Serial.begin(115200);
  delay(100);
  Serial.println("\n\n--- ESP32 IS STARTING ---"); 
  
  dht.begin();
  Wire.begin(21,22);
  lcd.init();
  lcd.backlight();

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  pinMode(galPin2, OUTPUT);
  pinMode(galPin1, OUTPUT);
  pinMode(galPin0, OUTPUT);
  sendToGAL(7); 

  myservo.attach(servoPin);
  myservo.write(0); 

  Serial.print("Connecting to WiFi: "); 
  Serial.print(ssid); 
  WiFi.begin(ssid, pass);
  while (WiFi.status() != WL_CONNECTED) {
    delay(1000);
    Serial.print("."); 
  }
  Serial.println("\nWiFi Connected!"); 

  client.setServer(mqtt_broker, mqtt_port);
  client.setCallback(mqttCallback);
  connectToMQTT();
}

// ===== Main Loop =====
void loop() {
  if (!client.connected()) connectToMQTT();
  client.loop();

  unsigned long currentMillis = millis();

  if (currentMillis - lastSensorPublish >= sensorPublishInterval) {
    lastSensorPublish = currentMillis;

    // 1. อ่านค่าเซนเซอร์ทั้งหมดก่อน
    currentDistance = readUltrasonic();
    rainVal = analogRead(analogPin);
    
    float h = dht.readHumidity();
    float t = dht.readTemperature();
    if (!isnan(t) && !isnan(h)) {
      currentTemp = t;
      currentHum = h;
    }
    
    // 2. พิมพ์ค่าปกติทุกๆ 3 วินาที (ลง Serial Monitor)
    Serial.print("Mode: "); Serial.print(Mode == 0 ? "AUTO" : "MANUAL");
    Serial.print(" | Temp: "); Serial.print(currentTemp);
    Serial.print("C | Hum: "); Serial.print(currentHum);
    Serial.print("% | Rain: "); Serial.print(rainVal);
    Serial.print(" | Dist: "); Serial.print(currentDistance);
    Serial.print("cm | Servo: "); Serial.println(servoState);

    // ==========================================
    // 3. ย้ายส่วนส่ง MQTT มาไว้ตรงนี้เลย! (เพื่อให้ Dashboard อัปเดตก่อนใครเพื่อน)
    // ==========================================
    String payload = "{";
    payload += "\"temperature\":" + String(currentTemp, 1) + ",";
    payload += "\"humidity\":" + String(currentHum, 1) + ",";
    payload += "\"rain\":" + String(rainVal) + ",";
    payload += "\"distance\":" + String(currentDistance) + ",";
    payload += "\"mode\":\"" + String((Mode==0) ? "AUTO" : "MANUAL") + "\",";
    payload += "\"window_position\":" + String(servoState);
    payload += "}";

    client.publish(topic_data, payload.c_str());
    // บังคับให้ไคลเอนต์ประมวลผลการส่งทันที
    client.loop(); 
    // ==========================================

    // 4. ค่อยมาเช็กเงื่อนไขว่าฝนตกหรือเปล่า
    if (Mode == 0) { // AUTO
      // ตรวจสอบว่าฝนตกเกิน 1200 หรือไม่
      if (rainVal > 1200 && currentDistance > 5 && servoState != 0) {
        
        Serial.println("\n==================================");
        Serial.println(" ⚠️ ALERT: RAIN DETECTED! ⚠️");
        Serial.print(" >> Current Rain Value: "); Serial.println(rainVal);
        Serial.println(" >> Starting window close sequence...");
        Serial.println("==================================\n");

        // ตอนนี้พอมันเข้ามานับถอยหลัง Dashboard ก็จะโชว์ค่าฝนล่าสุดไปเรียบร้อยแล้ว
        countdownToClose("Window closed (auto rain)", "Now it's raining"); 
      } 
      else if (rainVal <= 1200) {
        sendToGAL(7); 
        if (servoState != 90 && currentDistance > 5) {
          myservo.write(90);
          servoState = 90;
          client.publish(topic_status, "Window opened (auto)");
          Serial.println("\n--- Rain stopped. Window Opened! ---");
        }
        updateLCD("Welcome to IoTE", "KMITL");
      }
    } 
  }
}