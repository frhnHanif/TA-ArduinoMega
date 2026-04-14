#include <Arduino.h>

// Bikin alias biar gampang dibaca untuk relay Active-Low
#define NYALA LOW
#define MATI HIGH

// Definisikan pin digital yang terhubung ke relay mist maker
const int mistPins[] = {2, 3, 4, 5, 6, 7}; 
const int numMists = 6;

// Durasi menyala untuk masing-masing mist maker (dalam milidetik)
const unsigned long interval = 5000; // 5000 ms = 5 detik

unsigned long previousMillis = 0;
int currentMist = 0;

void setup() {
  Serial.begin(115200);

  // Atur semua pin sebagai OUTPUT dan pastikan dalam keadaan MATI di awal
  for (int i = 0; i < numMists; i++) {
    pinMode(mistPins[i], OUTPUT);
    digitalWrite(mistPins[i], MATI); // Mengirim sinyal HIGH untuk mematikan relay
  }

  Serial.println("Sistem Mist Maker Bergantian Dimulai (Active-Low)...");
}

void loop() {
  unsigned long currentMillis = millis();

  // Cek apakah sudah waktunya untuk berganti giliran
  if (currentMillis - previousMillis >= interval) {
    // Simpan waktu pergantian terakhir
    previousMillis = currentMillis;

    // Matikan SEMUA mist maker terlebih dahulu
    for (int i = 0; i < numMists; i++) {
      digitalWrite(mistPins[i], MATI); // Mengirim sinyal HIGH
    }

    // Hidupkan mist maker yang mendapat giliran saat ini
    digitalWrite(mistPins[currentMist], NYALA); // Mengirim sinyal LOW
    
    Serial.print("Menghidupkan Mist Maker ke-");
    Serial.println(currentMist + 1);

    // Pindah indeks ke mist maker berikutnya
    currentMist++;

    // Jika sudah mencapai mist maker terakhir, kembalikan ke awal (indeks 0)
    if (currentMist >= numMists) {
      currentMist = 0;
    }
  }
}