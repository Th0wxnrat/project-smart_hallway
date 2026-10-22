#define BUZZER_PIN 7  // กำหนดขาที่เชื่อมต่อกับ buzzer
// buzzer ขาบวก ต่อกับขา 8
// buzzer ขาลบ ต่อกับ GND

void setup() {

  pinMode(BUZZER_PIN, OUTPUT);  // กำหนดขา BUZZER_PIN เป็นขา output

}

void loop() {

  tone(BUZZER_PIN, 1000, 1000);  // สร้างเสียงที่ความถี่ 1000 Hz และยาว 1 วินาที

  delay(1500);  // หยุด 1.5 วินาที

}
