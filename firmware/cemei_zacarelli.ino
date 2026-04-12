// ============================================================
//  CEMEI Zacarelli — Sistema IoT de Monitorização de Estatura
//  ESP32 DevKit V1 + VL53L0X + Firebase Realtime Database
// ============================================================

#include <WiFi.h>
#include <WiFiManager.h>
#include <Wire.h>
#include <VL53L0X.h>
#include <HTTPClient.h>
#include <ArduinoJson.h>
#include <WiFiClientSecure.h>
#include <time.h>

#include "secrets.h" // A aba de senhas fica de fora do GitHub!

// ── CALIBRAÇÃO ──────────────────────────────────────────────
const int ALTURA_SENSOR_CM = 220;
const int ESTATURA_MIN     = 30;
const int ESTATURA_MAX     = 120;

// ── FIREBASE ────────────────────────────────────────────────
// AS CHAVES ABAIXO FORAM OCULTADAS POR SEGURANÇA NO GITHUB
const char* FIREBASE_HOST = "https://SEU_PROJETO_AQUI.firebaseio.com";
const char* FIREBASE_PATH = "/leituras";
const char* FIREBASE_API_KEY = "SUA_API_KEY_OCULTA"; 

// ── PINAGEM I2C (PLACA NOVA - ESP32 DevKit V1) ──────────────
#define SDA_PIN 21
#define SCL_PIN 22

VL53L0X sensor;

const unsigned long INTERVALO_MS = 100; // (10 leituras por segundo)
unsigned long ultimaLeitura = 0;
unsigned long tempoUltimoAdulto = 0;    // Filtro para ignorar braços/pernas

bool ultimoAlerta = false;
int  ultimaEstatura = -1;

const char* NTP_SERVER = "pool.ntp.org";
const long  GMT_OFFSET_SEC = -3 * 3600;
const int   DAYLIGHT_OFFSET_SEC = 0;

String deviceUid;
String firebaseIdToken = "";
unsigned long tokenExpiryMs = 0;

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
  client.setTimeout(15000); 

  HTTPClient http;
  http.setTimeout(15000); 
  http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); 
  
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
      Serial.println("✅ Firebase Auth OK (Login efetuado!)");
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

void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n🔧 CEMEI Zacarelli — Iniciando...");

  uint64_t chipid = ESP.getEfuseMac();
  char uid[17];
  snprintf(uid, sizeof(uid), "%04X%08X", (uint16_t)(chipid >> 32), (uint32_t)chipid);
  deviceUid = String(uid);
  Serial.print("🆔 UID do ESP: ");
  Serial.println(deviceUid);

  WiFiManager wm;
  wm.setConfigPortalTimeout(0);

  Serial.println("📶 Conectando ao WiFi...");
  if (!wm.autoConnect("CEMEI-Config", "cemei1234")) {
    Serial.println("❌ Timeout WiFi — reiniciando em 5s...");
    delay(5000);
    ESP.restart();
  }
  Serial.print("✅ WiFi conectado! IP: ");
  Serial.println(WiFi.localIP());

  configTime(GMT_OFFSET_SEC, DAYLIGHT_OFFSET_SEC, NTP_SERVER);
  Serial.println("🕒 Sincronizando horário...");

  firebaseSignIn();

  Wire.begin(SDA_PIN, SCL_PIN);
  sensor.setTimeout(500);

  if (!sensor.init()) {
    Serial.println("❌ Sensor VL53L0X não encontrado!");
    Serial.println("   Verifique a fiação SDA/SCL e alimentação.");
    while (true) { delay(1000); }
  }

  // --- HACK DE ENGENHARIA: ATIVANDO O MODO LONGO ALCANCE (2 METROS) ---
  sensor.setSignalRateLimit(0.1);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodPreRange, 18);
  sensor.setVcselPulsePeriod(VL53L0X::VcselPeriodFinalRange, 14);
  // -------------------------------------------------------------------

  sensor.startContinuous();
  Serial.println("✅ Sensor VL53L0X OK (Modo Longo Alcance Ativo)!");
  Serial.println("──────────────────────────────────────────");
}

void loop() {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️ WiFi caiu, tentando reconectar...");
    WiFi.reconnect();
    delay(1000);
    return;
  }

  if (millis() - ultimaLeitura < INTERVALO_MS) return;
  ultimaLeitura = millis();

  uint16_t distMm = sensor.readRangeContinuousMillimeters();

  if (sensor.timeoutOccurred()) {
    Serial.println("⚠️  Timeout do sensor — ignorando leitura.");
    return;
  }

  int distCm     = distMm / 10;
  int estaturaCm = ALTURA_SENSOR_CM - distCm;

  if (estaturaCm <= 0 || estaturaCm > ALTURA_SENSOR_CM) {
    Serial.println("⚠️  Leitura fora do esperado — ignorando.");
    return;
  }

  // --- LÓGICA: FILTRO DE ADULTO (COOLDOWN) ---
  if (estaturaCm > ESTATURA_MAX) {
    tempoUltimoAdulto = millis(); 
  }

  bool alerta = false;
  if (estaturaCm >= ESTATURA_MIN && estaturaCm <= ESTATURA_MAX) {
    if (millis() - tempoUltimoAdulto > 2000) {
      alerta = true;
    } else {
      Serial.println("⚠️  Ignorado: Rastro de adulto detectado na zona");
    }
  }
  // -------------------------------------------

  Serial.printf("📏 Distância: %d cm | Estatura: %d cm | %s\n",
                distCm, estaturaCm,
                alerta ? "🚨 CRIANÇA DETECTADA!" : "✅ Porta livre");

  if (alerta && (alerta != ultimoAlerta || abs(estaturaCm - ultimaEstatura) >= 2)) {
    enviarFirebase(estaturaCm, alerta);
  }

  ultimoAlerta   = alerta;
  ultimaEstatura = estaturaCm;
}

String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    return "sem_horario";
  }
  char buffer[32];
  strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buffer);
}

String getKeyTimestamp() {
  time_t now = time(nullptr);
  if (now < 100000) {
    return String(millis());
  }
  return String((unsigned long)now);
}

void enviarFirebase(int estatura, bool alerta) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️  WiFi desconectado — aguardando reconexão...");
    return;
  }

  if (!ensureFirebaseAuth()) {
    Serial.println("❌ Firebase Auth não disponível");
    return;
  }

  StaticJsonDocument<192> doc;
  doc["estatura_cm"] = estatura;
  doc["alerta"]      = alerta;
  doc["mensagem"]    = alerta ? "Criança detectada na entrada!" : "Porta livre";
  doc["ms"] = millis();
  doc["data_hora"] = getTimestamp();
  doc["device_uid"] = deviceUid;

  String payload;
  serializeJson(doc, payload);

  String key = getKeyTimestamp();
  String url = String(FIREBASE_HOST) + FIREBASE_PATH + "/" + key + ".json?auth=" + firebaseIdToken;

  for (int tentativa = 1; tentativa <= 3; tentativa++) {
    WiFiClientSecure client;
    client.setInsecure();
    client.setTimeout(15000); 

    HTTPClient http;
    http.setTimeout(15000); 
    http.setFollowRedirects(HTTPC_STRICT_FOLLOW_REDIRECTS); 
    
    http.begin(client, url);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.PUT(payload);

    if (httpCode == 200 || httpCode == 204) {
      Serial.println("☁️  Firebase OK (Dados na nuvem!): " + payload);
      http.end();
      return;
    }

    if (httpCode == 401) {
      firebaseIdToken = "";
      if (ensureFirebaseAuth()) {
        url = String(FIREBASE_HOST) + FIREBASE_PATH + "/" + key + ".json?auth=" + firebaseIdToken;
      }
    }

    Serial.printf("⚠️  Firebase erro %d: %s (tentativa %d/3)\n", httpCode, http.errorToString(httpCode).c_str(), tentativa);
    http.end();
    delay(500);
  }

  Serial.println("❌ Firebase: falha após 3 tentativas.");
}
