#include <ArduinoJson.h>

// ==========================================
// 1. DEKLARASI PIN (BERDASARKAN TABEL PCB)
// ==========================================
// Relay Mist Maker (IN 1 s/d IN 6 dihubungkan ke Mega D13 s/d D8)
const int pinMist[6] = {13, 12, 11, 10, 9, 8}; 
// Kipas / Fan (Menggunakan Driver 1 / B7 dihubungkan ke Mega D3)
const int pinFan = 3; 
// Sensor Udara MQ-135 (Pin Analog terhubung ke Mega A6)
const int mq135Pin = A6; 

// ==========================================
// 2. VARIABEL DAN KONSTANTA
// ==========================================
// Konfigurasi Relay (Biasanya Active LOW)
const int RELAY_ON = LOW;
const int RELAY_OFF = HIGH; 

// Variabel Timer (Pengganti delay pada Loop)
unsigned long previousMillis = 0;
const long interval = 5000; // Interval kirim data (5 detik)

// Konstanta MQ-135
const float RL = 1.0;               // Resistor bawaan modul 1 kOhm
const float Ro = 12.0;              // Ganti dengan angka Ro kalibrasi Anda
const float konstanta_A = 100.8504; // Konstanta Kurva Amonia (NH3)
const float konstanta_B = -2.45847;

void setup() {
  Serial.begin(115200);  // Serial ke PC
  Serial1.begin(9600);   // Serial ke ESP32

  // Set pin Mist Maker sebagai OUTPUT dan matikan di awal
  for (int i = 0; i < 6; i++) {
    pinMode(pinMist[i], OUTPUT);
    digitalWrite(pinMist[i], RELAY_OFF); 
  }
  
  // Set pin Fan sebagai OUTPUT dan matikan di awal
  pinMode(pinFan, OUTPUT);
  analogWrite(pinFan, 0); 
  
  randomSeed(analogRead(0));

  Serial.println("Memanaskan sensor MQ-135... (Tunggu 10 detik)");
  delay(10000); // Pemanasan singkat saat alat baru dihidupkan
  Serial.println("Sistem Mega Ready! Mengirim 5 detik sekali & Siaga perintah...");
}

// ... (Bagian atas deklarasi variabel dan void setup() tetap sama seperti sebelumnya) ...

void loop() {
  // ==========================================
  // TAHAP 1: MENDENGARKAN PERINTAH DARI ESP32
  // ==========================================
  if (Serial1.available()) {
    String commandJson = Serial1.readStringUntil('\n');
    commandJson.trim();
    
    if (commandJson.startsWith("{") && commandJson.endsWith("}")) {
      // UPDATE V7: Cukup gunakan JsonDocument, tanpa StaticJsonDocument<256>
      JsonDocument docControl;
      DeserializationError error = deserializeJson(docControl, commandJson);
      
      if (!error) {
        Serial.println("Perintah Web Masuk: " + commandJson);
        // Eksekusi 6x Mist Maker
        for (int i = 0; i < 6; i++) {
          int state = docControl["mist"][i];
          digitalWrite(pinMist[i], state == 1 ? RELAY_ON : RELAY_OFF);
        }
        
        // Eksekusi Kipas 
        int fanPercent = docControl["fan"];
        int pwmValue = map(fanPercent, 0, 100, 0, 255);
        analogWrite(pinFan, pwmValue);
        Serial.println("Status Diperbarui -> Kipas: " + String(fanPercent) + "% (PWM: " + String(pwmValue) + ")");
      }
    }
  }

  // ==========================================
  // TAHAP 2: MENGIRIM DATA SENSOR KE ESP32
  // ==========================================
  unsigned long currentMillis = millis();
  if (currentMillis - previousMillis >= interval) {
    previousMillis = currentMillis; 

    // --- PEMBACAAN SENSOR MQ-135 ---
    long totalAdc = 0;
    for(int i = 0; i < 50; i++){
      totalAdc += analogRead(mq135Pin);
      delay(2); 
    }
    float rataRataAdc = totalAdc / 50.0;
    
    float tegangan = rataRataAdc * (5.0 / 1023.0);
    if (tegangan == 0) tegangan = 0.001; 
    
    float Rs = RL * ((5.0 - tegangan) / tegangan);
    float rasio = Rs / Ro;
    float ppmAmonia = konstanta_A * pow(rasio, konstanta_B);

    // --- PACKING DATA JSON ---
    // UPDATE V7: Cukup gunakan JsonDocument
    JsonDocument docSensor;

    // UPDATE V7: Cara baru membuat array bersarang
    JsonArray biopond = docSensor["biopond"].to<JsonArray>();
    for(int i = 0; i < 6; i++) {
      biopond.add(random(1000, 5000));
    }

    docSensor["harvest"] = random(0, 2000);         
    docSensor["temp"] = random(250, 350) / 10.0;    
    docSensor["hum"] = random(600, 850) / 10.0;
    docSensor["soil"] = random(40, 80);             
    docSensor["ammonia"] = ppmAmonia; 

    // Kirim data serial ke ESP32 (Lewat TX1/RX1)
    serializeJson(docSensor, Serial1);
    Serial1.println();
    
    // --- TAMBAHAN UNTUK DEBUG DI SERIAL MONITOR PC ---
    Serial.print("Data terkirim ke ESP32: ");
    serializeJson(docSensor, Serial); // <--- Baris ini mencetak JSON ke PC Anda
    Serial.println(); // Memberikan enter (baris baru)
  }
}