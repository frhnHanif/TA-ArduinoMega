#include <Arduino.h>
#include <ArduinoJson.h>
#include <DHT.h>
#include <HX711_ADC.h> // Menggunakan library dari Olav Kallhovd

// ==========================================
// 1. DEKLARASI PIN & VARIABEL GLOBAL
// ==========================================

// --- DHT22 ---
#define DHTPIN 26
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);
const float TEMP_OFFSET = -1.52;
const float HUM_OFFSET = -3.28;

// --- MQ-135 ---
#define MQ135_PIN A6
const float RL = 1.0;      
const float Ro = 0.662;     // Ganti jika diperlukan hasil kalibrasi Ro terbaru
const float konstanta_A = 100.8504;
const float konstanta_B = -2.45847;

// --- SOIL MOISTURE (Struktur Kalibrasi Piecewise) ---
struct SensorCalib {
  int pin;
  int adc_0;
  int adc_12_5;
  int adc_25;
  int adc_37_5;
  int adc_50;
};

SensorCalib listSensor[6] = {
  {A0, 461, 436, 404, 318, 231}, // SM1
  {A1, 458, 433, 397, 312, 227}, // SM2
  {A2, 464, 440, 414, 325, 235}, // SM3
  {A3, 464, 439, 408, 319, 230}, // SM4
  {A4, 451, 425, 395, 304, 213}, // SM5
  {A5, 461, 434, 400, 318, 236}  // SM6
};

// --- LOAD CELL (HX711_ADC) ---
// Format Inisialisasi: HX711_ADC(DT_PIN, SCK_PIN)
HX711_ADC biopond_lc[6] = {
  HX711_ADC(23, 25), HX711_ADC(27, 29), HX711_ADC(31, 33),
  HX711_ADC(35, 37), HX711_ADC(39, 43), HX711_ADC(45, 47)
};
const float bio_cal[6] = {105.389900, 107.378623, 103.403343, 104.197036, 106.368423, 105.547973};

HX711_ADC harvest_lc(49, 51);
const float harvest_cal = 100.411346;

// --- AKTUATOR: MIST (RELAY ACTIVE LOW) ---
const int mistPins[6] = {8, 9, 10, 11, 12, 13};
unsigned long mistTurnOffTime[6] = {0}; // Menyimpan target waktu millis() relay dimatikan
bool mistActive[6] = {false};           // Status apakah mist sedang menyala

// --- AKTUATOR: EXHAUST FAN (MOTOR DRIVER BTS7960) ---
#define L_EN 2
#define L_PWM 3
#define R_EN 4
#define R_PWM 5

// --- PENGATURAN WAKTU PENGIRIMAN ---
unsigned long lastSendTime = 0;
const unsigned long sendInterval = 5000; // Kirim data tiap 5 detik

// ==========================================
// 2. FUNGSI HELPER
// ==========================================

// Fungsi Interpolasi Piecewise Kelembapan Tanah
float hitungKadarAir(int adc_baca, SensorCalib s) {
  if (adc_baca >= s.adc_0)  return 0.0;   
  if (adc_baca <= s.adc_50) return 100.0; 

  if (adc_baca > s.adc_12_5) {
    return 0.0 + 25.0 * ((float)(s.adc_0 - adc_baca) / (s.adc_0 - s.adc_12_5));
  } 
  else if (adc_baca > s.adc_25) {
    return 25.0 + 25.0 * ((float)(s.adc_12_5 - adc_baca) / (s.adc_12_5 - s.adc_25));
  } 
  else if (adc_baca > s.adc_37_5) {
    return 50.0 + 25.0 * ((float)(s.adc_25 - adc_baca) / (s.adc_25 - s.adc_37_5));
  } 
  else {
    return 75.0 + 25.0 * ((float)(s.adc_37_5 - adc_baca) / (s.adc_37_5 - s.adc_50));
  }
}

// ==========================================
// 3. FUNGSI SETUP INISIALISASI
// ==========================================
void setup() {
  Serial.begin(115200);   
  Serial1.begin(115200);  

  Serial.println("=== Inisialisasi Sistem PCB Maggot ===");

  // Inisialisasi DHT22
  dht.begin();

  // Inisialisasi Load Cell (HX711_ADC)
  Serial.println("Memulai Tare pada 7 Load Cell (Pastikan Rak Kosong)...");
  
  for (int i = 0; i < 6; i++) {
    biopond_lc[i].begin();
    // 2000ms waktu stabilisasi, true = auto-tare di awal
    biopond_lc[i].start(2000, true); 
    biopond_lc[i].setCalFactor(bio_cal[i]);
  }
  harvest_lc.begin();
  harvest_lc.start(2000, true);
  harvest_lc.setCalFactor(harvest_cal);

  // Inisialisasi Aktuator Mist (Relay Active Low)
  for (int i = 0; i < 6; i++) {
    pinMode(mistPins[i], OUTPUT);
    digitalWrite(mistPins[i], HIGH); // HIGH berarti relay MATI
  }

  // Inisialisasi Motor Fan
  pinMode(L_EN, OUTPUT); pinMode(L_PWM, OUTPUT);
  pinMode(R_EN, OUTPUT); pinMode(R_PWM, OUTPUT);
  digitalWrite(L_EN, LOW); analogWrite(L_PWM, 0);
  digitalWrite(R_EN, LOW); analogWrite(R_PWM, 0);

  Serial.println("=== Inisialisasi Selesai ===");
}

// ==========================================
// 4. RUTINITAS SENSOR & KOMUNIKASI
// ==========================================
void sendSensorData() {
  // Update untuk ArduinoJson v7
  JsonDocument doc;

  // -- 1. DHT22 --
  float t = dht.readTemperature();
  float h = dht.readHumidity();
  if (isnan(t)) t = 0.0; else t += TEMP_OFFSET;
  if (isnan(h)) h = 0.0; else h += HUM_OFFSET;
  doc["temp"] = t;
  doc["hum"] = h;
  
  // -- 2. MQ-135 --
  long totalAdc = 0;
  // Membaca cepat 50 sampel tanpa delay untuk mencegah blocking pada HX711_ADC
  for(int i = 0; i < 50; i++) {
    totalAdc += analogRead(MQ135_PIN);
  }
  float rataRataAdc = totalAdc / 50.0;
  float tegangan = rataRataAdc * (5.0 / 1023.0);
  if (tegangan <= 0.0) tegangan = 0.001; 
  float Rs = RL * ((5.0 - tegangan) / tegangan);
  float rasio = Rs / Ro;
  float ppmAmonia = konstanta_A * pow(rasio, konstanta_B);
  
  // Cegah nilai tidak logis (misal minus atau membludak saat pemanasan)
  if(ppmAmonia < 0) ppmAmonia = 0;
  if(ppmAmonia > 1000) ppmAmonia = 1000;
  doc["ammonia"] = ppmAmonia;

  // -- 3. Soil Moisture --
  // Update untuk ArduinoJson v7
  JsonArray soil = doc["soil"].to<JsonArray>();
  for (int i = 0; i < 6; i++) {
    int soil_raw = analogRead(listSensor[i].pin);
    float pct = hitungKadarAir(soil_raw, listSensor[i]);
    soil.add(pct);
  }

  // -- 4. Load Cell Biopond --
  // Update untuk ArduinoJson v7
  JsonArray biopond = doc["biopond"].to<JsonArray>();
  for (int i = 0; i < 6; i++) {
    float massa_bio = biopond_lc[i].getData(); // Sifatnya instan karena update() jalan di loop
    biopond.add(massa_bio > 0 ? massa_bio : 0.0);
  }

  // -- 5. Load Cell Panen --
  float massa_panen = harvest_lc.getData();
  doc["harvest"] = (massa_panen > 0 ? massa_panen : 0.0);

  // -- Kirim Data via Serial1 --
  serializeJson(doc, Serial1);
  Serial1.println(); 

  // Debug Serial Monitor
  Serial.print("[TX] Data Sent: Temp="); Serial.print(t, 1);
  Serial.print("C, RH="); Serial.print(h, 1);
  Serial.print("%, NH3="); Serial.print(ppmAmonia, 1);
  Serial.println("ppm");
}

void receiveControlData() {
  if (Serial1.available()) {
    String input = Serial1.readStringUntil('\n');

    // Update untuk ArduinoJson v7
    JsonDocument doc;
    DeserializationError error = deserializeJson(doc, input);

    if (error) return;

    Serial.println("[RX] Terima Instruksi Aktuator dari ESP32");

    // -- Kontrol Mist Maker (Berdasarkan Durasi Detik) --
    JsonArray mist = doc["mist"];
    for (int i = 0; i < 6; i++) {
      float mistDurationSec = mist[i]; // Didapat dari logika Fuzzy (Misal: 6.5)
      
      // Abaikan jika nilai durasi di bawah 2 detik (dianggap 0/mati)
      if (mistDurationSec >= 2.0) {
        digitalWrite(mistPins[i], LOW); // LOW berarti relay NYALA (Active Low)
        mistActive[i] = true;
        // Kalkulasi waktu mati = waktu sekarang + durasi konversi ke ms
        mistTurnOffTime[i] = millis() + (unsigned long)(mistDurationSec * 1000); 
        
        Serial.print("  -> Mist R"); Serial.print(i + 1);
        Serial.print(" ON selama "); Serial.print(mistDurationSec); Serial.println(" detik");
      }
    }

    // -- Kontrol Exhaust Fan (PWM) --
    int fan_pwm = doc["fan"];
    if (fan_pwm > 0) {
      digitalWrite(R_EN, HIGH);
      digitalWrite(L_EN, HIGH); // KEDUA Enable harus HIGH agar arus bisa mengalir
      analogWrite(R_PWM, fan_pwm);
      analogWrite(L_PWM, 0);    // Pastikan sisi kiri 0 (menjadi Ground)
      Serial.print("  -> Fan ON (PWM: "); Serial.print(fan_pwm); Serial.println(")");
    } else {
      digitalWrite(R_EN, LOW);
      digitalWrite(L_EN, LOW);  // Matikan kedua Enable
      analogWrite(R_PWM, 0);
      analogWrite(L_PWM, 0);
    }
  }
}

// ==========================================
// 5. MAIN LOOP
// ==========================================
void loop() {
  // 1. UPDATE LOAD CELL TERUS MENERUS (NON-BLOCKING MA)
  for(int i = 0; i < 6; i++) {
    biopond_lc[i].update();
  }
  harvest_lc.update();

  // 2. CHECK TIMER MIST MAKER
  for(int i = 0; i < 6; i++) {
    if (mistActive[i] && (millis() >= mistTurnOffTime[i])) {
      digitalWrite(mistPins[i], HIGH); // HIGH berarti relay MATI (Active Low)
      mistActive[i] = false;
      Serial.print("[TIMER] Mist R"); Serial.print(i + 1); Serial.println(" OFF Otomatis.");
    }
  }

  // 3. PENGIRIMAN DATA SENSOR PERIODIK (Tiap 5 Detik)
  if (millis() - lastSendTime >= sendInterval) {
    lastSendTime = millis();
    sendSensorData();
  }

  // 4. MEMBACA INSTRUKSI ESP32
  receiveControlData();
}