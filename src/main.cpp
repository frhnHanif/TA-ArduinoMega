#include <Arduino.h>
#include "HX711_ADC.h"

const int HX711_dout = 2;
const int HX711_sck = 4;

HX711_ADC LoadCell(HX711_dout, HX711_sck);

const int stabilizationSamples = 100; // Sampai sinyal stabil
const int measurementSamples = 500;   // Sampel setelah stabil
const float knownWeight = 63.8; // gram

void performMeasurement();
void calculateResults(float sum, int validSamples);

void setup() {
  Serial.begin(115200);
  
  Serial.println();
  Serial.println("==============================================");
  Serial.println("     Program Kalibrasi HX711 - FIXED");
  Serial.println("==============================================");

  LoadCell.begin();
  Serial.println("Memulai timbangan...");
  LoadCell.start(2000, true);

  if (LoadCell.getTareTimeoutFlag()) {
    Serial.println("Timeout! Cek koneksi HX711.");
    while (1);
  }

  LoadCell.setCalFactor(1.0);
  Serial.println("Faktor kalibrasi diatur ke 1.0 untuk pembacaan mentah.");

  Serial.println("\n--> Letakkan beban acuan (misal 1000g).");
  Serial.println("--> Pengambilan data akan dimulai dalam 10 detik...");
  
  // Countdown
  for (int i = 10; i > 0; i--) {
    Serial.print("Mulai dalam ");
    Serial.print(i);
    Serial.println(" detik...");
    delay(1000);
  }

  Serial.println("\nMenstabilkan pembacaan...");
  Serial.println("Menunggu sinyal stabil (20 sampel pertama)...");
  
  // Fase 1: Tunggu sinyal stabil
  float lastReading = 0;
  int stableCount = 0;
  const float stabilityThreshold = 10.0;
  
  for (int i = 0; i < stabilizationSamples; i++) {
    LoadCell.update(); // 🔥 PASTIKAN update() selalu dipanggil
    float reading = LoadCell.getData();
    
    Serial.print("Stabilisasi ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.print(stabilizationSamples);
    Serial.print(": ");
    Serial.println(reading, 0);
    
    // Cek stabilitas
    if (i > 0 && abs(reading - lastReading) < stabilityThreshold) {
      stableCount++;
      Serial.println("  ✓ Stabil");
    } else {
      stableCount = 0;
    }
    
    lastReading = reading;
    
    // Jika sudah stabil 5 pembacaan berturut-turut, lanjut ke pengukuran
    if (stableCount >= 5) {
      Serial.println("✅ Sinyal sudah stabil! Lanjut ke pengukuran...");
      break;
    }
    
    delay(100);
  }

  // 🔥 TAMBAHKAN: Pastikan pembacaan terakhir benar-benar stabil
  Serial.println("Final stabilization...");
  for (int i = 0; i < 5; i++) {
    LoadCell.update();
    delay(50);
  }

  Serial.println("\nMulai pengukuran akurat...");
  performMeasurement();
}

void performMeasurement() {
  float sum = 0;
  int validSamples = 0;
  
  Serial.println("Mengambil 50 sampel akurat...");
  Serial.println("----------------------------------------------");

  // 🔥 PERBAIKI: Gunakan for loop sederhana tanpa conditional update
  for (int i = 0; i < measurementSamples; i++) {
    LoadCell.update(); // 🔥 PASTIKAN update() selalu dipanggil di setiap iterasi
    float reading = LoadCell.getData();
    sum += reading;
    validSamples++;

    // Progress update
    Serial.print("Data ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.print(measurementSamples);
    Serial.print(": ");
    Serial.print(reading, 0);
    
    // Indikator stabilitas
    if (i > 0) {
      static float prevReading = 0;
      float change = abs(reading - prevReading);
      if (change < 10.0) {
        Serial.print(" ✓"); // Sangat stabil
      } else if (change < 50.0) {
        Serial.print(" ~"); // Cukup stabil
      }
      prevReading = reading;
    }
    
    Serial.println();
    
    delay(80); // Interval antar sampel
  }

  // Hitung hasil
  calculateResults(sum, validSamples);
}

void calculateResults(float sum, int validSamples) {
  float avg = sum / validSamples;
  float calValue = avg / knownWeight;

  // Tampilkan hasil detail
  Serial.println("----------------------------------------------");
  Serial.println("           HASIL KALIBRASI");
  Serial.println("----------------------------------------------");
  Serial.print("Jumlah sampel valid: ");
  Serial.println(validSamples);
  Serial.print("Rata-rata nilai mentah: ");
  Serial.println(avg, 3);
  Serial.print("Berat kalibrasi: ");
  Serial.print(knownWeight, 0);
  Serial.println(" g");
  Serial.print("Faktor Kalibrasi: ");
  Serial.println(calValue, 6);
  
  // Validasi
  Serial.println("----------------------------------------------");
  Serial.println("VALIDASI:");
  LoadCell.setCalFactor(calValue);
  delay(500);
  
  float testSum = 0;
  for (int i = 0; i < 500; i++) {
    LoadCell.update();
    testSum += LoadCell.getData();
    Serial.print("Validasi ");
    Serial.print(i + 1);
    Serial.print("/");
    Serial.println(500);
    delay(100);
  }
  float testAvg = testSum / 500;
  
  Serial.print("Berat terukur dengan cal factor baru: ");
  Serial.print(testAvg, 1);
  Serial.println(" g");
  
  float error = abs(testAvg - knownWeight);
  float errorPercent = (error / knownWeight) * 100.0;
  
  Serial.print("Error: ");
  Serial.print(error, 2);
  Serial.print("g (");
  Serial.print(errorPercent, 2);
  Serial.println("%)");
  
  // Evaluasi
  Serial.println("----------------------------------------------");
  if (errorPercent < 0.5) {
    Serial.println("✅ KALIBRASI SANGAT AKURAT!");
  } else if (errorPercent < 1.0) {
    Serial.println("✅ Kalibrasi berhasil!");
  } else if (errorPercent < 2.0) {
    Serial.println("⚠️  Kalibrasi cukup akurat");
  } else {
    Serial.println("❌ Kalibrasi perlu diperbaiki");
  }
  
  Serial.println("----------------------------------------------");
  Serial.println("SALIN FAKTOR KALIBRASI DI BAWAH:");
  Serial.println("----------------------------------------------");
  Serial.print("const float calibrationFactor = ");
  Serial.print(calValue, 6);
  Serial.println(";");
  Serial.println("----------------------------------------------");
  Serial.println("Selesai! Reset untuk mengulang.");
}

void loop() {
  // Kosong - semua proses sudah selesai di setup()
}