// ============================================================
//  CEMEI Zacarelli — Sistema IoT de Monitorização de Estatura
//  ESP32-C3 SuperMini + VL53L0X + Firebase Realtime Database
// ============================================================

#include <WiFi.h>
#include <WiFiManager.h>       // https://github.com/tzapu/WiFiManager
#include <Wire.h>
#include <VL53L0X.h>           // Pololu VL53L0X library
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "secrets.h" // NÃO subir no GitHub (contém senha)

// ── CALIBRAÇÃO ──────────────────────────────────────────────
const int ALTURA_SENSOR_CM = 200; // altura do sensor ao chão (cm)
const int ESTATURA_MIN     = 60;  // mínimo para ser "criança"
const int ESTATURA_MAX     = 140; // máximo para ser "criança"
// ────────────────────────────────────────────────────────────

// ── FIREBASE ────────────────────────────────────────────────
const char* FIREBASE_HOST =
  "https://integrador-univesp-default-rtdb.firebaseio.com";
const char* FIREBASE_PATH = "/leituras"; // histórico com chave (PUT)
const char* FIREBASE_API_KEY = "AIzaSyAZku1jcVLXtxzR_d-lR2y4liR_qFCLN_0";
// ────────────────────────────────────────────────────────────

// ── PINAGEM I2C (ESP32-C3 SuperMini) ────────────────────────
#define SDA_PIN 6
#define SCL_PIN 7
// ────────────────────────────────────────────────────────────

VL53L0X sensor;

// ── INTERVALO DE LEITURA ─────────────────────────────────────
const unsigned long INTERVALO_MS = 500;   // a cada 500 ms
unsigned long ultimaLeitura = 0;

// ── ESTADO ANTERIOR (evita envio desnecessário ao Firebase) ──
bool ultimoAlerta = false;
int  ultimaEstatura = -1;

// ── NTP ─────────────────────────────────────────────────────
const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET_SEC = -3 * 3600; // Brasil (UTC-3)
const int   DAYLIGHT_OFFSET_SEC = 0;

String deviceUid;
String firebaseIdToken = "";
unsigned long tokenExpiryMs = 0;

// ─────────────────────────────────────────────────────────────
bool firebaseSignIn() {
  if (WiFi.status() != WL_CONNECTED) return false;

  String url = String("https://identitytoolkit.googleapis.com/v1/accounts:signInWithPassword?key=") + FIREBASE_API_KEY;

  StaticJsonDocument<256> doc;
  doc["email"] = FIREBASE_AUTH_EMAIL;
  doc["password"] = FIREBASE_AUTH_PASSWORD;
  doc["returnSecureToken"] = true;

  String payload;
  serializeJson(doc, payload);

  WiFiClientSecure client;
  client.setInsecure();

  HTTPClient http;
  http.begin(client, url);
  http.addHeader("Content-Type", "application/json");

  int httpCode = http.POST(payload);
  if (httpCode == 200) {
    String resp = http.getString();
    StaticJsonDocument<1024> respDoc;
    if (deserializeJson(respDoc, resp) == DeserializationError::Ok) {
      firebaseIdToken = respDoc["idToken"].as<String>();
      long expiresIn = respDoc["expiresIn"].as<long>();
      if (expiresIn <= 0) expiresIn = 3600;
      tokenExpiryMs = millis() + (unsigned long)(expiresIn - 60) * 1000UL;
      http.end();
      Serial.println("✅ Firebase Auth OK");
      return true;
    }
  }

  Serial.printf("❌ Firebase Auth falhou (HTTP %d)\n", httpCode);
  http.end();
  return false;
}

bool ensureFirebaseAuth() {
  if (firebaseIdToken.length() == 0) return firebaseSignIn();
  if ((long)(millis() - tokenExpiryMs) > 0) return firebaseSignIn();
  return true;
}

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n🔧 CEMEI Zacarelli — Iniciando...");

  // UID do ESP
  uint64_t chipid = ESP.getEfuseMac();
  char uid[17];
  snprintf(uid, sizeof(uid), "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
  deviceUid = String(uid);
  Serial.print("🆔 UID do ESP: ");
  Serial.println(deviceUid);

  // ── WiFiManager ─────────────────────────────────────────────
  WiFiManager wm;
  // wm.resetSettings(); // descomente para forçar reconfiguração
  wm.setConfigPortalTimeout(0); // nunca fecha

  Serial.println("📶 Conectando ao WiFi...");
  if (!wm.autoConnect("CEMEI-Config", "cemei1234")) {
    Serial.println("❌ Timeout WiFi — reiniciando em 5s...");
    delay(5000);
    ESP.restart();
  }
  Serial.print("✅ WiFi conectado! IP: ");
  Serial.println(WiFi.localIP());

  // ── NTP ────────────────────────────────────────────────────
  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.println("🕒 Sincronizando horário...");

  // ── Firebase Auth ───────────────────────────────────────────
  firebaseSignIn();

  // ── VL53L0X ─────────────────────────────────────────────────
  Wire.begin(SDA_PIN, SCL_PIN);
  sensor.setTimeout(500);

  if (!sensor.init()) {
    Serial.println("❌ Sensor VL53L0X não encontrado!");
    Serial.println("   Verifique a fiação SDA/SCL e alimentação.");
    while (true) { delay(1000); }
  }
  sensor.startContinuous();
  Serial.println("✅ Sensor VL53L0X OK!");
  Serial.println("──────────────────────────────────────────");
}

// ─────────────────────────────────────────────────────────────
void loop() {
  // reconexão automática se cair
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi caiu, tentando reconectar...");
    WiFi.reconnect();
    delay(1000);
    return;
  }

  if (millis() - ultimaLeitura < INTERVALO_MS) return;
  ultimaLeitura = millis();

  // ── Leitura do sensor ──────────────────────────────────────
  uint16_t distMm = sensor.readRangeContinuousMillimeters();

  if (sensor.timeoutOccurred()) {
    Serial.println("⚠️  Timeout do sensor — ignorando leitura.");
    return;
  }

  int distCm    = distMm / 10;
  int estaturaCm = ALTURA_SENSOR_CM - distCm;

  // ── Filtra leituras inválidas ──────────────────────────────
  if (estaturaCm <= 0 || estaturaCm > ALTURA_SENSOR_CM) {
    Serial.println("⚠️  Leitura fora do esperado — ignorando.");
    return;
  }

  bool alerta = (estaturaCm >= ESTATURA_MIN && estaturaCm <= ESTATURA_MAX);

  Serial.printf("📏 Distância: %d cm | Estatura: %d cm | %s\n",
                distCm, estaturaCm,
                alerta ? "🚨 CRIANÇA DETECTADA!" : "✅ Porta livre");

  // ── Envia ao Firebase só quando detectar criança ───────────
  if (alerta && (alerta != ultimoAlerta || abs(estaturaCm - ultimaEstatura) >= 2)) {
    enviarFirebase(estaturaCm, alerta);
  }

  // Atualiza estado
  ultimoAlerta   = alerta;
  ultimaEstatura = estaturaCm;
}

// ─────────────────────────────────────────────────────────────
String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "sem_horario";
  }
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

// ─────────────────────────────────────────────────────────────
String getKeyTimestamp() {
  time_t now = time(nullptr);
  if (now < 100000) { // se NTP ainda não sincronizou
    return String(millis());
  }
  return String((unsigned long)now); // epoch como chave
}

// ─────────────────────────────────────────────────────────────
void enviarFirebase(int estatura, bool alerta) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️  WiFi desconectado — aguardando reconexão...");
    return;
  }

  if (!ensureFirebaseAuth()) {
    Serial.println("❌ Firebase Auth não disponível");
    return;
  }

  // Monta o JSON
  StaticJsonDocument<192> doc;
  doc["estatura_cm"] = estatura;
  doc["alerta"]      = alerta;
  doc["mensagem"]    = alerta
    ? "Criança detectada na entrada!"
    : "Porta livre";
  doc["ms"] = millis();
  doc["data_hora"] = getTimestamp();
  doc["device_uid"] = deviceUid;

  String payload;
  serializeJson(doc, payload);

  String key = getKeyTimestamp();
  String url = String(FIREBASE_HOST) + FIREBASE_PATH + "/" + key + ".json?auth=" + firebaseIdToken;

  // Tenta até 3 vezes
  for (int tentativa = 1; tentativa <= 3; tentativa++) {
    WiFiClientSecure client;
    client.setInsecure();

    HTTPClient http;
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.PUT(payload);   // PUT com chave (timestamp)

    if (httpCode == 200 || httpCode == 204) {
      Serial.println("☁️  Firebase OK: " + payload);
      http.end();
      return;
    }

    if (httpCode == 401) {
      firebaseIdToken = "";
      if (ensureFirebaseAuth()) {
        url = String(FIREBASE_HOST) + FIREBASE_PATH + "/" + key + ".json?auth=" + firebaseIdToken;
      }
    }

    Serial.printf("⚠️  Firebase erro %d (tentativa %d/3)\n",
                  httpCode, tentativa);
    http.end();
    delay(500);
  }

  Serial.println("❌ Firebase: falha após 3 tentativas.");
}