/*
 * ESP32 + BH1750 (lux) + KY-037 (micrófono) + Bluetooth SPP
 * App: Serial Bluetooth Controller (Android)
 *
 * Comandos:
 *   MENU
 *   1 -> BH1750
 *   2 -> KY037
 *   S  -> Start/Stop
 *   ON / OFF
 *   RATE=ms    (intervalo entre reportes)
 *   WINDOW=ms  (duración de ventana para análisis del micrófono)
 *
 * Salida: texto + JSON (una línea).
 * Nota KY-037: Para precisión absoluta en dB SPL se requiere calibración externa.
 */

// # Librerías

// Formateo
#include <array>
#include <Arduino.h>

// BH1750
#include <Wire.h>
#include <BH1750.h>

// Terminal serial Bluetooth
#include "BluetoothSerial.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// ---------- Config ----------

// Estados Wi-Fi
enum WiFiMode { WIFI_DISABLED, WIFI_CONNECTING, WIFI_CONNECTED, WIFI_AP_MODE };
WiFiMode currentWiFiMode = WIFI_DISABLED;
unsigned long lastWifiCheck = 0;

// Conexión a Red
const char* ssid     = "Alumnos"; // Cambiar si estás en otra red.
const char* password = "@@1umN05@@"; // Cambiar si estás en otra red.

/*
const char* ssid     = "Innovacion"; // Cambiar si estás en otra red.
const char* password = "||abcxyz||"; // Cambiar si estás en otra red.
*/

// Configuración Portal Cautivo + DNS
const byte  DNS_PORT = 53;
IPAddress apIP(192,168,4,1), netmask(255,255,255,0);
const char[8] AP_SSID = "hotspot";
const char[10] AP_PASS = "cantuuuuu";


DNSServer dns;
WebServer http(80);
volatile bool loggedIn = false;

// --- HTML MIN: sin estilos, sin JS (en PROGMEM) ---
static const char HTML_LOGIN[] PROGMEM =
"<!doctype html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Login</title></head><body>"
"<h3>Portal</h3>"
"<form method='POST' action='/login'>"
"<input name='u' placeholder='usuario'>"
"<input name='p' type='password' placeholder='clave'>"
"<button>Entrar</button>"
"</form>"
"</body></html>";

static const char HTML_PANEL[] PROGMEM =
"<!doctype html><html><head><meta charset='utf-8'>"
"<meta name='viewport' content='width=device-width,initial-scale=1'>"
"<title>Panel</title></head><body>"
"<h3>Panel</h3>"
"<p><a href='/cmd?c=1'>BH1750</a> | <a href='/cmd?c=2'>KY037</a> | "
"<a href='/cmd?c=S'>Start/Stop</a></p>"
"<form onsubmit=\"location='/cmd?c=RATE='+this.r.value;return false;\">"
"RATE(ms): <input name='r' value='500'><button>OK</button></form>"
"<form onsubmit=\"location='/cmd?c=WINDOW='+this.w.value;return false;\">"
"WINDOW(ms): <input name='w' value='100'><button>OK</button></form>"
"</body></html>";

// --- Utilidades pequeñas ---
inline void sendLogin() { http.send_P(200, "text/html", HTML_LOGIN); }
inline void sendPanel() { http.send_P(200, "text/html", HTML_PANEL); }

void redirectRoot() {
  http.sendHeader("Location", "http://192.168.4.1/", true);
  http.send(302, "text/plain", "");
}

// --- Handlers ---
void handleRoot() {
  if (loggedIn) sendPanel(); else sendLogin();
}

void handleLogin() {
  // Validación mínima (o acepta siempre para ahorrar lógica)
  // String u = http.arg("u"); String p = http.arg("p");
  loggedIn = true;
  redirectRoot();
}

// Aquí solo eco del comando en HTTP para que veas respuesta.
// Si quieres enviar a Serial/BT, coloca tu handleCommand(...) aquí.
void handleCmd() {
  if (!loggedIn) { redirectRoot(); return; }
  if (!http.hasArg("c")) { http.send(400, "text/plain", "Missing c"); return; }
  String c = http.arg("c");
  http.send(200, "text/plain", String("OK: ") + c);
}

// Responder probes con algo de HTML para disparar el portal
void handleProbe() { sendLogin(); }

void handleNotFound() {
  if (http.hostHeader() != apIP.toString()) { redirectRoot(); return; }
  if (loggedIn) sendPanel(); else sendLogin();
}

// Bluetooth
static const char* BT_NAME = "ESP32_SENSORS";

// I2C
static const uint8_t I2C_SDA = 21;
static const uint8_t I2C_SCL = 22;

// KY-037
static const int KY_A0_PIN = 34; // ADC1 (solo entrada)
static const int KY_D0_PIN = 25; // opcional, si usas el comparador y nivel 3.3V

// Tiempos
unsigned long sampleIntervalMs = 1000; // intervalo entre envíos
unsigned long micWindowMs = 100;       // ventana de análisis para el mic

// ---------- Objetos ----------
BluetoothSerial SerialBT;
BH1750 lightMeter;

// ---------- Estado ----------
enum SensorSel { SENSOR_BH1750 = 1, SENSOR_KY037 = 2 };
SensorSel currentSensor = SENSOR_BH1750;

bool streaming = false;
unsigned long lastSampleMs = 0;

// ---------- Utilidades ----------
void printMenu(Stream &out) {
  out.println(F("\n=== MENU ==="));
  out.println(F("1 -> BH1750 (lux)"));
  out.println(F("2 -> KY037 (RMS, P2P, dBFS relativo)"));
  out.println(F("S -> Start/Stop streaming"));
  out.println(F("RATE=ms -> Cambiar intervalo de reporte (p.ej., RATE=200)"));
  out.println(F("WINDOW=ms -> Ventana microfono (p.ej., WINDOW=100)\n"));
  out.println(F("3 -> Conectar a Wi-Fi"));
  out.println(F("4 -> Activar Access Point"));
  out.println(F("ON / OFF -> Forzar start/stop"));
}

void printSelection(Stream &out) {
  out.print(F("[INFO] Sensor actual: "));
  if (currentSensor == SENSOR_BH1750) out.println(F("BH1750 (lux)"));
  else if (currentSensor == SENSOR_KY037) out.println(F("KY037 (microfono)"));
}

// Lee KY-037 durante 'windowMs' y calcula métricas básicas
// Devuelve por referencia: raw_min, raw_max, p2p_raw, rms_ac_raw, d0_state
void analyzeMic(unsigned long windowMs, int &rawMin, int &rawMax, int &p2pRaw, float &rmsAcRaw, int &d0State)
{
  unsigned long start = millis();
  rawMin = 4095;
  rawMax = 0;

  // Para RMS AC: necesitamos media y luego varianza
  // sumX y sumX2 para computar media y varianza
  double sumX = 0.0;
  double sumX2 = 0.0;
  uint32_t n = 0;

  while ((millis() - start) < windowMs) {
    int raw = analogRead(KY_A0_PIN);
    if (raw < rawMin) rawMin = raw;
    if (raw > rawMax) rawMax = raw;

    sumX += raw;
    sumX2 += (double)raw * (double)raw;
    n++;
  }

  p2pRaw = (rawMax - rawMin);

  float rmsAc = 0.0f;
  if (n > 0) {
    double mean = sumX / (double)n;
    double var = (sumX2 / (double)n) - (mean * mean);   // varianza alrededor de la media
    if (var < 0) var = 0; // por seguridad ante errores de redondeo
    rmsAc = sqrt(var);
  }
  rmsAcRaw = rmsAc;

  // Estado D0 (si conectado); si no, retorna -1
  #if defined(KY_D0_PIN)
    pinMode(KY_D0_PIN, INPUT);
    d0State = digitalRead(KY_D0_PIN);
  #else
    d0State = -1;
  #endif
}

void sendSample(Stream &out) {
  if (currentSensor == SENSOR_BH1750) {
    float lux = lightMeter.readLightLevel();
    // Texto
    out.print(F("BH1750 lux: "));
    out.println(lux, 2);
    // JSON
    out.print(F("{\"sensor\":\"BH1750\",\"lux\":"));
    out.print(lux, 4);
    out.print(F(",\"interval_ms\":"));
    out.print(sampleIntervalMs);
    out.println(F("}"));
  }
  else if (currentSensor == SENSOR_KY037) {
    int rawMin, rawMax, p2pRaw, d0State;
    float rmsAcRaw;
    analyzeMic(micWindowMs, rawMin, rawMax, p2pRaw, rmsAcRaw, d0State);

    // Aproximación de voltaje (referencia 3.3V; ADC 12 bits 0..4095; no lineal perfecto)
    float voltsMin = (rawMin / 4095.0f) * 3.3f;
    float voltsMax = (rawMax / 4095.0f) * 3.3f;
    float voltsP2P = (p2pRaw / 4095.0f) * 3.3f;
    float voltsRmsAc = (rmsAcRaw / 4095.0f) * 3.3f;

    // dBFS relativo (respecto a escala ADC, no dB SPL)
    // Evita log(0) con un epsilon pequeño
    float epsilon = 1e-6f;
    float dbfs = 20.0f * log10f((rmsAcRaw + epsilon) / 4095.0f);

    // Texto
    out.print(F("KY037 p2p[raw]: "));
    out.print(p2pRaw);
    out.print(F("  rmsAC[raw]: "));
    out.print(rmsAcRaw, 1);
    out.print(F("  ~p2p[V]: "));
    out.print(voltsP2P, 3);
    out.print(F("  ~rmsAC[V]: "));
    out.print(voltsRmsAc, 3);
    out.print(F("  dBFS(rel): "));
    out.print(dbfs, 1);
    if (d0State != -1) {
      out.print(F("  D0: "));
      out.print(d0State);
    }
    out.println();

    // JSON
    out.print(F("{\"sensor\":\"KY037\","));
    out.print(F("\"window_ms\":")); out.print(micWindowMs); out.print(F(","));
    out.print(F("\"raw_min\":")); out.print(rawMin); out.print(F(","));
    out.print(F("\"raw_max\":")); out.print(rawMax); out.print(F(","));
    out.print(F("\"p2p_raw\":")); out.print(p2pRaw); out.print(F(","));
    out.print(F("\"rms_ac_raw\":")); out.print(rmsAcRaw, 2); out.print(F(","));
    out.print(F("\"volts_p2p\":")); out.print(voltsP2P, 4); out.print(F(","));
    out.print(F("\"volts_rms_ac\":")); out.print(voltsRmsAc, 4); out.print(F(","));
    out.print(F("\"dbfs_rel\":")); out.print(dbfs, 2); out.print(F(","));
    out.print(F("\"interval_ms\":")); out.print(sampleIntervalMs);
    if (d0State != -1) {
      out.print(F(",\"d0\":")); out.print(d0State);
    }
    out.println(F("}"));
  }
}

void startWiFiClient() {
  SerialBT.println("[INFO] Intentando conectar a WiFi...");
  Serial.println("[INFO] Intentando conectar a WiFi...");
  WiFi.mode(WIFI_STA);
  WiFi.begin(ssid, password);
  currentWiFiMode = WIFI_CONNECTING; // Cambio de estado a «conectando».
  lastWifiCheck = millis();
}

void startAccessPoint() {
  SerialBT.println("[INFO] Creando Access Point...");
  Serial.println("[INFO] Creando Access Point...");
  WiFi.mode(WIFI_AP);
  WiFi.softAPConfig(apIP, apIP, netmask);
  WiFi.softAP(AP_SSID);
  dns.start(DNS_PORT, "*", apIP);

  // Configura las rutas del servidor web
  http.on("/", HTTP_GET, handleRoot);
  http.on("/login", HTTP_POST, handleLogin);
  http.on("/cmd", HTTP_GET, handleCmd);
  http.on("/generate_204", HTTP_GET, handleProbe);
  http.on("/hotspot-detect.html", HTTP_GET, handleProbe);
  http.on("/ncsi.txt", HTTP_GET, handleProbe);
  http.on("/fwlink", HTTP_GET, handleProbe);
  http.onNotFound(handleNotFound);
  http.begin();
  
  Serial.print("¡AP Creado! IP: ");
  Serial.println(WiFi.softAPIP());
  SerialBT.print("¡AP Creado! IP: ");
  SerialBT.println(WiFi.softAPIP());
  
  currentWiFiMode = WIFI_AP_MODE; // Cambiamos el estado a «Modo AP»
}

void handleCommand(const String &cmdRaw, Stream &out) {
  String cmd = cmdRaw;
  cmd.trim();
  cmd.toUpperCase();

  if (cmd == "MENU") { printMenu(out); return; }
  if (cmd == "1") { currentSensor = SENSOR_BH1750; printSelection(out); return; }
  if (cmd == "2") { currentSensor = SENSOR_KY037; printSelection(out); return; }
  if (cmd == "3") { 
    WiFi.disconnect(true); // Desconecta cualquier modo anterior
    startWiFiClient(); 
    return; 
  }
  if (cmd == "4") { 
    WiFi.disconnect(true); // Desconecta cualquier modo anterior
    startAccessPoint(); 
    return; 
  }

  if (cmd == "S") { streaming = !streaming; out.print(F("[INFO] Streaming: ")); out.println(streaming ? F("ON") : F("OFF")); return; }
  if (cmd == "ON") { streaming = true; out.println(F("[INFO] Streaming: ON")); return; }
  if (cmd == "OFF") { streaming = false; out.println(F("[INFO] Streaming: OFF")); return; }

  if (cmd.startsWith("RATE=")) {
    long ms = cmd.substring(5).toInt();
    if (ms >= 50 && ms <= 60000) {
      sampleIntervalMs = (unsigned long)ms;
      out.print(F("[INFO] RATE=")); out.print(sampleIntervalMs); out.println(F(" ms"));
    } else {
      out.println(F("[WARN] RATE fuera de rango (50..60000 ms)"));
    }
    return;
  }

  if (cmd.startsWith("WINDOW=")) {
    long ms = cmd.substring(7).toInt();
    if (ms >= 10 && ms <= 1000) {
      micWindowMs = (unsigned long)ms;
      out.print(F("[INFO] WINDOW=")); out.print(micWindowMs); out.println(F(" ms"));
    } else {
      out.println(F("[WARN] WINDOW fuera de rango (10..1000 ms)"));
    }
    return;
  }

  out.print(F("[WARN] Comando no reconocido: "));
  out.println(cmdRaw);
  out.println(F("Escribe 'MENU' para ver opciones."));
}

void readIncoming(Stream &in, Stream &out) {
  static String buffer;
  while (in.available()) {
    char c = (char)in.read();
    if (c == '\r') continue;
    if (c == '\n') {
      if (buffer.length() > 0) {
        handleCommand(buffer, out);
        buffer = "";
      }
    } else {
      buffer += c;
      if (buffer.length() > 100) buffer = "";
    }
  }
}

/* Versión 2
void handleCommand(const String &cmdRaw, Stream &out) {
  String cmd = cmdRaw;
  cmd.trim();
  cmd.toUpperCase();


  if (cmd == "MENU") { printMenu(out); return; }
  if (cmd == "1") { currentSensor = SENSOR_BH1750; printSelection(out); return; }
  if (cmd == "2") { currentSensor = SENSOR_KY037; printSelection(out); return; }
  if (cmd == "3") { streaming = !streaming; out.print(F("[INFO] Streaming: ")); out.println(streaming ? F("ON") : F("OFF")); return; }
  if (cmd == "4") { conectarWiFi(); } // Ejecutar función para conectarse a Wi-Fi
  if (cmd == "5") { createAP();}  // Ejecutar función para activar Access Point
  if (cmd == "6") { streaming = true; out.println(F("[INFO] Streaming: ON")); return; }
  if (cmd == "7") { streaming = false; out.println(F("[INFO] Streaming: OFF")); return; }

  if (cmd.startsWith("RATE=")) {
    long ms = cmd.substring(5).toInt();
    if (ms >= 50 && ms <= 60000) {
      sampleIntervalMs = (unsigned long)ms;
      out.print(F("[INFO] RATE=")); out.print(sampleIntervalMs); out.println(F(" ms"));
    } else {
      out.println(F("[WARN] RATE fuera de rango (50..60000 ms)"));
    }
    return;
  }

  if (cmd.startsWith("WINDOW=")) {
    long ms = cmd.substring(7).toInt();
    if (ms >= 10 && ms <= 1000) {
      micWindowMs = (unsigned long)ms;
      out.print(F("[INFO] WINDOW=")); out.print(micWindowMs); out.println(F(" ms"));
    } else {
      out.println(F("[WARN] WINDOW fuera de rango (10..1000 ms)"));
    }
    return;
  }

  out.print(F("[WARN] Comando no reconocido: "));
  out.println(cmdRaw);
  out.println(F("Escribe 'MENU' para ver opciones."));
}
*/

/* Versión 2
String readIncoming(Stream &in, Stream &out) {
  String entrada = "";
  while (true) {
    while (SerialBT.available()) {
      char c = SerialBT.read();
      if (c == '\n' || c == '\r') {
        if (entrada.length() > 0 ) return entrada;
      } else {
        entrada += c;
      }
    }
  }
}
*/

// ---------- Setup / Loop ----------
void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\n[BOOT] Iniciando...");

  // I2C
  Wire.begin(I2C_SDA, I2C_SCL);
  delay(100);

  // BH1750
  bool ok = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x23, &Wire);
  if (!ok) {
    Serial.println("[WARN] BH1750 no en 0x23. Probando 0x5C...");
    ok = lightMeter.begin(BH1750::CONTINUOUS_HIGH_RES_MODE, 0x5C, &Wire);
  }
  Serial.println(ok ? "[OK] BH1750 inicializado" : "[WARN] BH1750 no detectado");

  // ADC ajustes (opcional: define atenuación para señales ~3.3V)
  analogSetPinAttenuation(KY_A0_PIN, ADC_11db); // ~0..3.3V aprox

  // Bluetooth
  if (!SerialBT.begin(BT_NAME)) {
    Serial.println("[ERR] Bluetooth no pudo iniciar.");
  } else {
    Serial.print("[OK] Bluetooth iniciado como: ");
    Serial.println(BT_NAME);
  }

  Serial.println("[INFO] 'S' para comenzar/pausar streaming.");
  SerialBT.println("[INFO] 'S' para comenzar/pausar streaming.");
}

void loop() {
  readIncoming(SerialBT, SerialBT);
  readIncoming(Serial,   SerialBT);

  // # LÓGICA DE ESTADOS DE WI-FI
  switch(currentWiFiMode) {
    case WIFI_CONNECTING:
      // Revisa cada 500ms si ya se conectó
      {
        if (millis() - lastWifiCheck > 500) {
        Serial.print(".");
        SerialBT.print(".");

        if (WiFi.status() == WL_CONNECTED) {
          currentWiFiMode = WIFI_CONNECTED;
          Serial.println("\n¡WiFi Conectado!");
          Serial.print("Dirección IP: ");
          Serial.println(WiFi.localIP());
          SerialBT.println("\n¡WiFi Conectado!");
          SerialBT.print("IP: ");
          SerialBT.println(WiFi.localIP());
        }
        lastWifiCheck = millis();
        }
      }
      break;

    case WIFI_AP_MODE:
      // Modo AP, procesa las peticiones DNS y HTTP
      dns.processNextRequest();
      http.handleClient();
      break;

    case WIFI_CONNECTED:
      // Lógica de cliente conectado.
      break;

    case WIFI_DISABLED:
      // No hace nada relacionado con Wi-Fi
      break;
  }
  
  // La lógica de streaming de sensores se mantiene igual y siempre se ejecuta
  unsigned long now = millis();
  if (streaming && (now - lastSampleMs >= sampleIntervalMs)) {
    lastSampleMs = now;
    sendSample(SerialBT);
    sendSample(Serial);
  }
}