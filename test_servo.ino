#include <Servo.h>
Servo myservo;  // ประกาศตัวแปรแทน Servo

void setup() {
  myservo.attach(9);  // กำหนดขา 9 ควบคุม Servo
}

void loop() {
  myservo.write(0);    // หมุนซ้ายสุด
  delay(2000);

  myservo.write(90);   // หยุด/กึ่งกลาง
  delay(2000);

  myservo.write(180);  // หมุนขวาสุด
  delay(2000);
}
