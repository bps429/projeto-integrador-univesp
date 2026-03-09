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

// ── CALIBRAÇÃO ──────────────────────────────────────────────
const int ALTURA_SENSOR_CM = 200; // altura do sensor ao chão (cm)
const int ESTATURA_MIN     = 60;  // mínimo para ser "criança"
const int ESTATURA_MAX     = 140; // máximo para ser "criança"
// ────────────────────────────────────────────────────────────

// ── FIREBASE ────────────────────────────────────────────────
const char* FIREBASE_HOST =
  "https://integrador-univesp-default-rtdb.firebaseio.com";
const char* FIREBASE_PATH = "/leitura.json";
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

// ─────────────────────────────────────────────────────────────
void setup() {
  Serial.begin(115200);
  delay(500);
  Serial.println("\n🔧 CEMEI Zacarelli — Iniciando...");

  // ── WiFiManager ─────────────────────────────────────────────
  WiFiManager wm;
  // wm.resetSettings(); // descomente para forçar reconfiguração
  wm.setConfigPortalTimeout(180); // 3 min para configurar

  Serial.println("📶 Conectando ao WiFi...");
  if (!wm.autoConnect("CEMEI-Config", "cemei1234")) {
    Serial.println("❌ Timeout WiFi — reiniciando em 5s...");
    delay(5000);
    ESP.restart();
  }
  Serial.print("✅ WiFi conectado! IP: ");
  Serial.println(WiFi.localIP());

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

  // ── Envia ao Firebase só se mudou o estado ─────────────────
  if (alerta != ultimoAlerta || abs(estaturaCm - ultimaEstatura) >= 2) {
    enviarFirebase(estaturaCm, alerta);
    ultimoAlerta   = alerta;
    ultimaEstatura = estaturaCm;
  }
}

// ─────────────────────────────────────────────────────────────
void enviarFirebase(int estatura, bool alerta) {
  if (WiFi.status() != WL_CONNECTED) {
    Serial.println("⚠️  WiFi desconectado — aguardando reconexão...");
    return;
  }

  // Monta o JSON
  StaticJsonDocument<128> doc;
  doc["estatura_cm"] = estatura;
  doc["alerta"]      = alerta;
  doc["mensagem"]    = alerta
    ? "Criança detectada na entrada!"
    : "Porta livre";
  doc["ms"] = millis();

  String payload;
  serializeJson(doc, payload);

  // URL completa
  String url = String(FIREBASE_HOST) + FIREBASE_PATH;

  // Tenta até 3 vezes
  for (int tentativa = 1; tentativa <= 3; tentativa++) {
    HTTPClient http;
    http.begin(url);
    http.addHeader("Content-Type", "application/json");

    int httpCode = http.PUT(payload);   // PUT sobrescreve o nó /leitura

    if (httpCode == 200) {
      Serial.println("☁️  Firebase OK: " + payload);
      http.end();
      return;
    } else {
      Serial.printf("⚠️  Firebase erro %d (tentativa %d/3)\n",
                    httpCode, tentativa);
      http.end();
      delay(500);
    }
  }

  Serial.println("❌ Firebase: falha após 3 tentativas.");
}