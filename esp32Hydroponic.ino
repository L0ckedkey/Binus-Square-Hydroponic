/*
 * Hydroponic Monitor - ESP32
 * ------------------------------------------------------------
 * Membaca:
 *   - Suhu air kolam    : DS18B20 waterproof (OneWire)
 *   - Suhu & kelembapan : DHT22
 * Mengirim data (HTTP POST, JSON) ke Node-RED lewat Cloudflare Tunnel:
 *   https://hydroponic.hansprojects.com/api/sensor
 *
 * Wiring (default, ubah sesuai rangkaian):
 *   DS18B20 DATA -> GPIO 4  (+ resistor pull-up 4.7k ke 3.3V)
 *   DHT22   DATA -> GPIO 15 (+ resistor pull-up 10k ke 3.3V jika modul tanpa pull-up)
 *   VCC sensor   -> 3.3V, GND -> GND
 *
 * Library (Arduino Library Manager):
 *   - OneWire              (Paul Stoffregen)
 *   - DallasTemperature    (Miles Burton)
 *   - DHT sensor library   (Adafruit) + Adafruit Unified Sensor
 *   Board: ESP32 (Espressif ESP32 core)
 *
 * Contoh payload:
 *   {"device":"hydro-esp32-01","water_temp":27.31,"air_temp":30.20,
 *    "air_humidity":71.50,"rssi":-61,"uptime_s":1234}
 *   Nilai sensor yang gagal dibaca dikirim sebagai null.
 */

#include <WiFi.h>
#include <WiFiClientSecure.h>
#include <HTTPClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <DHT.h>

// ===================== KONFIGURASI =====================
const char* WIFI_SSID     = "YOUR_WIFI_SSID";
const char* WIFI_PASSWORD = "YOUR_WIFI_PASSWORD";

// Endpoint Node-RED (node "http in", method POST). Sesuaikan path-nya.
const char* ENDPOINT_URL  = "https://example.com";

// Opsional: kunci sederhana yang dicek di Node-RED. Kosongkan "" jika tidak dipakai.
const char* API_KEY       = "CHANGE_ME";

const char* DEVICE_ID     = "hydro-esp32-01";

#define ONE_WIRE_PIN   4
#define DHT_PIN        15
#define DHT_TYPE       DHT22

const unsigned long SEND_INTERVAL_MS = 30000;   // kirim tiap 30 detik
const unsigned long WIFI_TIMEOUT_MS  = 20000;
// =======================================================

OneWire oneWire(ONE_WIRE_PIN);
DallasTemperature ds18b20(&oneWire);
DHT dht(DHT_PIN, DHT_TYPE);

unsigned long lastSend = 0;

// ---------- WiFi ----------
bool connectWiFi() {
  if (WiFi.status() == WL_CONNECTED) return true;

  Serial.printf("[WiFi] Menghubungkan ke %s", WIFI_SSID);
  WiFi.mode(WIFI_STA);
  WiFi.begin(WIFI_SSID, WIFI_PASSWORD);

  unsigned long start = millis();
  while (WiFi.status() != WL_CONNECTED && millis() - start < WIFI_TIMEOUT_MS) {
    delay(500);
    Serial.print(".");
  }
  Serial.println();

  if (WiFi.status() == WL_CONNECTED) {
    Serial.print("[WiFi] Terhubung, IP: ");
    Serial.println(WiFi.localIP());
    return true;
  }
  Serial.println("[WiFi] Gagal terhubung");
  return false;
}

// ---------- Helper JSON ----------
String jsonNumber(float value, bool valid) {
  return valid ? String(value, 2) : String("null");
}

// ---------- Kirim ke Node-RED ----------
void sendReading(float waterTemp, bool waterOk,
                 float airTemp, bool airTempOk,
                 float airHum, bool airHumOk) {
  String payload = "{";
  payload += "\"device\":\"" + String(DEVICE_ID) + "\",";
  payload += "\"water_temp\":"   + jsonNumber(waterTemp, waterOk) + ",";
  payload += "\"air_temp\":"     + jsonNumber(airTemp, airTempOk) + ",";
  payload += "\"air_humidity\":" + jsonNumber(airHum, airHumOk) + ",";
  payload += "\"rssi\":"         + String(WiFi.RSSI()) + ",";
  payload += "\"uptime_s\":"     + String(millis() / 1000);
  payload += "}";

  WiFiClientSecure client;
  // Cloudflare memakai sertifikat publik yang valid; setInsecure() dipakai agar
  // tidak perlu menyimpan root CA. Untuk keamanan lebih, ganti dengan setCACert().
  client.setInsecure();

  HTTPClient http;
  if (!http.begin(client, ENDPOINT_URL)) {
    Serial.println("[HTTP] Gagal memulai koneksi");
    return;
  }
  http.setTimeout(10000);
  http.addHeader("Content-Type", "application/json");
  if (strlen(API_KEY) > 0) {
    http.addHeader("X-API-Key", API_KEY);
  }

  int code = http.POST(payload);
  Serial.printf("[HTTP] POST %s -> %d\n", payload.c_str(), code);
  if (code <= 0) {
    Serial.printf("[HTTP] Error: %s\n", http.errorToString(code).c_str());
  }
  http.end();
}

// ---------- Baca sensor ----------
void readAndSend() {
  ds18b20.requestTemperatures();
  float waterTemp = ds18b20.getTempCByIndex(0);
  bool waterOk = (waterTemp != DEVICE_DISCONNECTED_C);

  float airTemp = dht.readTemperature();
  float airHum  = dht.readHumidity();
  bool airTempOk = !isnan(airTemp);
  bool airHumOk  = !isnan(airHum);

  Serial.printf("[Sensor] Air: %s C | Udara: %s C, %s %%\n",
                waterOk   ? String(waterTemp, 2).c_str() : "ERR",
                airTempOk ? String(airTemp, 2).c_str()   : "ERR",
                airHumOk  ? String(airHum, 2).c_str()    : "ERR");

  if (connectWiFi()) {
    sendReading(waterTemp, waterOk, airTemp, airTempOk, airHum, airHumOk);
  }
}

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n=== Hydroponic Monitor ESP32 ===");

  ds18b20.begin();
  dht.begin();

  Serial.printf("[Sensor] DS18B20 terdeteksi: %d\n", ds18b20.getDeviceCount());

  connectWiFi();
  delay(2000);          // DHT22 butuh jeda sebelum pembacaan pertama
  readAndSend();
  lastSend = millis();
}

void loop() {
  if (millis() - lastSend >= SEND_INTERVAL_MS) {
    lastSend = millis();
    readAndSend();
  }
}
