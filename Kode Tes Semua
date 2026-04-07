#include <Arduino.h>
#include "DHT.h"
#include "HX711_ADC.h"

// --- KONFIGURASI PIN ---
// 1. DHT22 (Suhu & Kelembaban Udara)
#define DHTPIN 3        
#define DHTTYPE DHT22
DHT dht(DHTPIN, DHTTYPE);

// 2. SOIL MOISTURE (Kelembaban Tanah)
const int SOIL_A0 = A1; 
const int SOIL_D0 = 2;

// 3. MQ135 (Amonia)
#define MQ135_A0 A0
#define MQ135_D0 7

// 4. LOAD CELL (HX711)
const int HX711_dout = 4; 
const int HX711_sck = 5;
HX711_ADC LoadCell(HX711_dout, HX711_sck);
const float calibrationFactor = 131.820190; // Sesuaikan dengan kalibrasi Anda

// Variabel Pewaktu
unsigned long lastUpdate = 0;
const long interval = 2000; // Baca semua sensor tiap 2 detik

void setup() {
  Serial.begin(115200);
  
  // Inisialisasi Sensor
  dht.begin();
  pinMode(SOIL_D0, INPUT);
  pinMode(MQ135_D0, INPUT);

  // Inisialisasi Load Cell
  LoadCell.begin();
  LoadCell.start(2000, true);
  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout! Cek koneksi HX711.");
  }
  LoadCell.setCalFactor(calibrationFactor);

  Serial.println("--- SISTEM MONITORING GABUNGAN SIAP ---");
}

void loop() {
  // Wajib dipanggil terus menerus untuk Load Cell agar pembacaan stabil
  LoadCell.update();

  // Jalankan pembacaan dan print ke Serial berdasarkan interval (2 detik)
  if (millis() - lastUpdate >= interval) {
    lastUpdate = millis();

    // 1. BACA DHT22
    float hum = dht.readHumidity();
    float temp = dht.readTemperature();

    // 2. BACA SOIL MOISTURE
    int soilAnalog = analogRead(SOIL_A0);
    // Asumsi 1023 = kering total (0 RH%), 0 = basah total (100 RH%)
    int soilPercent = map(soilAnalog, 1023, 0, 0, 100); 
    // Mencegah nilai minus jika sensor membaca lebih dari 1023 (batas wajar)
    if (soilPercent < 0) soilPercent = 0; 
    if (soilPercent > 100) soilPercent = 100;
    
    int soilStatus = digitalRead(SOIL_D0);

    // 3. BACA MQ135 (Amonia)
    int gasAnalog = analogRead(MQ135_A0);
    // Konversi kasar ke PPM (Hanya untuk estimasi visual)
    // MQ135 biasanya bisa membaca dari kisaran 10 - 1000 ppm (tergantung gas)
    float gasPPM = map(gasAnalog, 0, 1023, 10, 1000); 
    int gasStatus = digitalRead(MQ135_D0);

    // 4. BACA LOAD CELL
    float weight = LoadCell.getData();

    // --- TAMPILKAN DATA KE SERIAL MONITOR ---
    Serial.println("================================");
    
    // Output DHT22 (Suhu Celcius, Kelembaban Udara %)
    if (isnan(hum) || isnan(temp)) {
      Serial.println("DHT22   : Gagal Membaca!");
    } else {
      Serial.print("Suhu    : "); Serial.print(temp, 1); Serial.println(" Celcius");
      Serial.print("Kel. Udara: "); Serial.print(hum, 1); Serial.println(" %");
    }

    // Output Soil Moisture (RH%)
    Serial.print("Kel. Tanah: "); Serial.print(soilPercent); Serial.print(" RH% (");
    Serial.print(soilStatus == LOW ? "BASAH" : "KERING"); Serial.println(")");

    // Output MQ135 (PPM Amonia)
    Serial.print("Amonia  : "); Serial.print(gasPPM, 1); Serial.print(" ppm (");
    Serial.print(gasStatus == LOW ? "GAS TERDETEKSI" : "AMAN"); Serial.println(")");

    // Output Load Cell (Gram)
    Serial.print("Berat   : "); Serial.print(weight, 2); Serial.println(" gram");
    
    Serial.println("================================");
  }
}