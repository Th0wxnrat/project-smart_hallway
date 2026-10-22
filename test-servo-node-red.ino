#include <DHT.h>
#include <Servo.h>

// ---------- เซนเซอร์ ----------
#define DHTPIN 13
#define DHTTYPE DHT11
DHT dht(DHTPIN, DHTTYPE);

const int analogPin = A0;    // Rain sensor
const int trigPin = 10;      // Ultrasonic
const int echoPin = 11;      

// ---------- Servo ----------
Servo myservo;
String inputString = "";

// ---------- Setup ----------
void setup() {
  Serial.begin(115200);
  dht.begin();

  pinMode(trigPin, OUTPUT);
  pinMode(echoPin, INPUT);

  myservo.attach(9);  // Servo ที่ขา 9

  // หยุดหมุนตอนเริ่มต้น
  myservo.write(90);
  delay(500);
}

// ---------- อ่าน Ultrasonic ----------
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

// ---------- Process Command from Serial ----------
void processCommand(String cmd) {
  cmd.trim();
  Serial.print("Received command: ");
  Serial.println(cmd);

  if (cmd == "open") {
    myservo.write(120);  // หมุนไปทางหนึ่ง (ช้า)
    Serial.println("Servo: turning");
  } else if (cmd == "close") {
    myservo.write(90);   // หยุดหมุน
    Serial.println("Servo: stopped");
  } else {
    Serial.println("Unknown command");
  }
}

// ---------- Loop ----------
void loop() {
  // อ่านจาก Serial (Node-RED สั่ง)
  while (Serial.available()) {
    char inChar = (char)Serial.read();
    if (inChar == '\n') {
      processCommand(inputString);
      inputString = "";
    } else {
      inputString += inChar;
    }
  }

  // อ่านค่าจากเซนเซอร์
  float h = dht.readHumidity();
  float t = dht.readTemperature();
  int rainVal = analogRead(analogPin);
  long distance = readUltrasonic();

  // ส่งข้อมูลไป Node-RED
  String dataLine = String(t) + "," + String(h) + "," + String(rainVal) + "," + String(distance);
  Serial.println(dataLine);

  delay(1000);
}
