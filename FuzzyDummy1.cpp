#include <Arduino.h>
#include <ArduinoJson.h>

void setup() {
  Serial.begin(115200); 
  Serial1.begin(9600); // Komunikasi ke ESP32
  randomSeed(analogRead(0));
  Serial.println("Mega Ready: Mengirim data 6 biopond (9600 baud)...");
}

void loop() {
  StaticJsonDocument<1024> doc; // Kapasitas diperbesar untuk array

  // 1. Data Berat Biopond (Loadcell)
  JsonArray biopond = doc.createNestedArray("biopond");
  for(int i = 0; i < 6; i++) {
    biopond.add(random(1000, 5000)); 
  }

  // 2. Data Kelembapan Tanah (6 Sensor Berbeda)
  JsonArray soil = doc.createNestedArray("soil");
  for(int i = 0; i < 6; i++) {
    soil.add(random(30, 90)); // Dummy kelembapan tanah 30% - 90%
  }

  // 3. Data Lingkungan Universal
  doc["temp"] = random(250, 360) / 10.0; // Suhu 25.0 - 36.0 C
  doc["hum"] = random(600, 850) / 10.0;
  doc["ammonia"] = random(5, 50);

  // Print ke Serial Monitor Mega
  Serial.print("Kirim: ");
  serializeJson(doc, Serial);
  Serial.println();

  // Kirim ke ESP32 via Serial1
  serializeJson(doc, Serial1);
  Serial1.println();

  delay(5000); 
}