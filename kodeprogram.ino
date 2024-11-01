#define BLYNK_TEMPLATE_ID "XXXXXXXX"
#define BLYNK_TEMPLATE_NAME "XXXXXXXXXXX"
#define BLYNK_AUTH_TOKEN "XXXXXXXXXX"

#include <Wire.h>
#include <Adafruit_INA219.h>
#include <ESP8266WiFi.h>
#include <BlynkSimpleEsp8266.h>
#include <LiquidCrystal_I2C.h>
#include <NTPClient.h>
#include <WiFiUdp.h>

Adafruit_INA219 sensorINA219;
LiquidCrystal_I2C lcd(0x27, 16, 2); // LCD I2C address 0x27, ukuran 16x2

// Ganti dengan detail WiFi Anda
char ssid[] = "Wifimu";
char pw[] = "XXXPWXXX";

// Pengaturan NTP client
WiFiUDP ntpUDP;
NTPClient waktuClient(ntpUDP, "pool.ntp.org", 7 * 3600); // UTC+7 untuk Waktu Indonesia Barat

int modeTampilan = 0;             // Mode tampilan: 0 untuk waktu & baterai, 1 untuk tegangan & arus, 2 untuk animasi SHBN
int posisiAnimasi = 16;           // Posisi awal teks "SHBN" pada animasi
const int pinTombol = 14;         // Pin untuk tombol di GPIO12

void setup(void) {
    Serial.begin(115200);
    koneksiWiFi();
    koneksiBlynk();
    inisialisasiSensor();
    inisialisasiLCD();
    inisialisasiNTP();
    pinMode(pinTombol, INPUT_PULLUP);
}

void koneksiWiFi() {
    WiFi.begin(ssid, pw);
    while (WiFi.status() != WL_CONNECTED) {
        delay(500);
        Serial.print(".");
    }
    Serial.println("WiFi tersambung");
}

void koneksiBlynk() {
    Blynk.begin(BLYNK_AUTH_TOKEN, ssid, pw);
}

void inisialisasiSensor() {
    if (!sensorINA219.begin()) {
        Serial.println("Gagal menemukan chip INA219");
        while (1) { delay(10); }
    }
    sensorINA219.setCalibration_32V_50A();
    Serial.println("INA219 dimulai.");
}

void inisialisasiLCD() {
    lcd.begin(16, 2);
    lcd.backlight();
    lcd.clear();
    lcd.setCursor(0, 0);
    lcd.print("Monitoring Panel");
}

void inisialisasiNTP() {
    waktuClient.begin();
}

int hitungKapasitasBaterai(float tegangan) {
    if (tegangan <= 10.5) return 1;
    if (tegangan >= 12.6) return 100;
    return (int)((tegangan - 10.5) * (100 - 1) / (12.6 - 10.5) + 1);
}

void tampilkanAnimasiSHBN() {
    lcd.clear();
    lcd.setCursor(posisiAnimasi, 0);
    lcd.print("SHBN");
    posisiAnimasi--;
    if (posisiAnimasi < -4) posisiAnimasi = 16; // Reset posisi animasi
}

void tampilkanData(int modeTampilan, float teganganBus, float arus_mA, int kapasitasBaterai) {
    switch (modeTampilan) {
        case 0: // Mode waktu & kapasitas baterai
            lcd.clear();
            lcd.setCursor(0, 0);
            lcd.print("Waktu: ");
            lcd.print(waktuClient.getFormattedTime());
            lcd.setCursor(0, 1);
            lcd.print("Batt: ");
            lcd.print(kapasitasBaterai);
            lcd.print("%  ");
            break;

        case 1: // Mode tegangan & arus
            lcd.setCursor(0, 0);
            lcd.print("Tegangan: ");
            lcd.print(teganganBus);
            lcd.print(" V");
            lcd.setCursor(0, 1);
            lcd.print("Arus: ");
            lcd.print(arus_mA);
            lcd.print(" mA");
            break;

        case 2: // Mode animasi teks "SHBN"
            tampilkanAnimasiSHBN();
            break;
    }
}

void kirimDataKeBlynk(float teganganBus, float arus_mA, int kapasitasBaterai) {
    Blynk.virtualWrite(V0, teganganBus);
    Blynk.virtualWrite(V1, arus_mA);
    Blynk.virtualWrite(V2, kapasitasBaterai);
}

void loop(void) {
    // Jalankan Blynk
    Blynk.run();
  
    // Perbarui waktu dari NTP
    waktuClient.update();

    // Deteksi tombol ditekan
    if (digitalRead(pinTombol) == LOW) {
        delay(200); // Debounce
        modeTampilan = (modeTampilan + 1) % 3; // Berpindah mode
        lcd.clear();
        posisiAnimasi = 16; // Reset posisi animasi untuk mode animasi SHBN
    }

    // Baca tegangan dan arus dari INA219
    float teganganBus = sensorINA219.getBusVoltage_V();
    float arus_mA = sensorINA219.getCurrent_mA();
  
    // Jika arus berada di antara -80 mA dan 80 mA, set ke 0
    if (arus_mA > -80 && arus_mA < 80) {
        arus_mA = 0;
    }

    // Hitung kapasitas baterai berdasarkan tegangan
    int kapasitasBaterai = hitungKapasitasBaterai(teganganBus);

    // Tampilkan data sesuai mode tampilan
    tampilkanData(modeTampilan, teganganBus, arus_mA, kapasitasBaterai);

    // Tampilkan di serial monitor
    Serial.print("Tegangan Bus: "); Serial.print(teganganBus); Serial.println(" V");
    Serial.print("Arus: "); Serial.print(arus_mA); Serial.println(" mA");
    Serial.print("Kapasitas Baterai: "); Serial.print(kapasitasBaterai); Serial.println(" %");

    // Kirim data ke Blynk
    kirimDataKeBlynk(teganganBus, arus_mA, kapasitasBaterai);

    delay(1000); // Delay 1 detik
}
