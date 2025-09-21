// # Librerías
// Formateo
#include <array>

// KY-037
#include <Arduino.h>

// BH1750
#include <Wire.h>
#include <BH1750.h>

// Terminal serial Bluetooth
#include "BluetoothSerial.h"

#include <WiFi.h>
#include <WebServer.h>
#include <DNSServer.h>

// Librerías para exponer un archivo
#include <FS.h>
#include <SPIFFS.h> 

// # Variables
#define CantidadLecturas 25
#define IntentosConexion 10
bool MenuMostrado = false;

// ## KY-037
#define MicrofonoAnalogo 34
#define MicrofonoDigital 15

// ## BH1750
BH1750 Luxometro; // Objeto BH1750

// ## Bluetooth
BluetoothSerial SerialBT;


// Datos de acceso a WIFI - ESP32 -> Internet
// Para evitar problemas con declaraciones en el futuro.
String ssid;
String password;
char ap_ssid[20] = "AP-ESP32";
char ap_password[20] = "12345678";

// CONFIG (compile-time, en FLASH)
static const uint8_t k_ap_chan = 6; // Canal recomendado: 1/6/11
static const uint8_t k_ap_maxcl = 4; // Máx. clientes simultáneos


// Validaciones de rango (compile-time)
static_assert(1 <= k_ap_chan && k_ap_chan <= 13, "Canal AP fuera de rango (1..13)");
static_assert(1 <= k_ap_maxcl && k_ap_maxcl <= 10, "Max clientes fuera de rango (1..10)");


// Configuración del DNS
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Configuración del server (servicio que ofrece el HTML del portal)
WebServer server(80);


// # Prototipado de funciones

// ## Sensores
std::array<float, 2> LecturaMicrofono(); // Regresará un arreglo de dos números, no es posible usar algo como float[2] porque ese objeto sería destruido después de terminar la función.
float LecturaLuxometro();

// ## Interfaz
int SeleccionarOpcion();

// ## Conexión WiFi
void initWifiConnection(char*, char*);

// ## Access Point
void ap_start(char*, char*); // Inicializa SoftAP (IP/gateway/máscara + SSID/clave)
IPAddress ap_ip(void); // Devuelve la IP actual del AP (sin guardar global)
void initiAP();

// ## Portal cautivo
void handleRoot();
void handleNotFound();

// # Setup + Loop
void setup(){
  Serial.begin(9600);

  SerialBT.begin("Sensores_ESP32"); 

  // Modo combinado: AP + Station (puede conectarse Y crear red)
  WiFi.mode(WIFI_AP_STA);

  /*
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print("--- Conectando ---");
    SerialBT.print("--- Conectando ---");                       
  }

  Serial.println("\nWiFi conectado como cliente :)");
  SerialBT.println("\nWiFi conectado como cliente :)"); 
  Serial.print("IP como cliente: ");
  SerialBT.print("IP como cliente: ");               
  Serial.println(WiFi.localIP());
  SerialBT.println(WiFi.localIP());               
  */

  // ## Inicializar BH1750
  Wire.begin(); // iniciar protocolo I2C
  Luxometro.begin(); // Iniciar BH1750

}

void loop(){
  dnsServer.processNextRequest();
  server.handleClient();

  if(!SerialBT.hasClient()){
    MenuMostrado = false;
    return; // Salida de loop.
  }

  if(!MenuMostrado){
    char buffer[1000];
    sprintf(buffer, "# Opciones:\n1. KY-037 (%d lecturas)\n2. BH1750 (%d lecturas)\n3. Conectarse a WiFi\n4. Habilitar Access Point", CantidadLecturas, CantidadLecturas);
    Serial.println(buffer);
    SerialBT.println(buffer);
    MenuMostrado = true;
  }
  
  
  // Solo ejecutar sensores si hay comando por Bluetooth
  if (SerialBT.available() > 0) {
    int Eleccion = SeleccionarOpcion();
    if(Eleccion == 1){
      // KY-037
      for(int i = 0; i < CantidadLecturas; i++){
        std::array<float, 2> Inputs = LecturaMicrofono();
        char buffer[100];
        sprintf(buffer, "Valor digital: %d | Volts: %.04f", (int)Inputs[0], Inputs[1]);
        Serial.println(buffer);
        SerialBT.println(buffer);
        
        delay(100);
        
        // Procesar peticiones web entre lecturas
        dnsServer.processNextRequest();
        server.handleClient();
      }
    } else if(Eleccion == 2){
      // BH1750
      for(int i = 0; i < CantidadLecturas; i++){
        float Input = LecturaLuxometro();
        char buffer[100];
        sprintf(buffer, "Iluminancia: %.04f lx", Input);
        Serial.println(buffer);
        SerialBT.println(buffer);
        
        delay(100);
        
        // Procesar peticiones web entre lecturas
        dnsServer.processNextRequest();
        server.handleClient();
      }
    } else if (Eleccion == 3) {
        // Solicitar SSID y contraseña desde aquí. 

        int Intentos = 0;
        while ((WiFi.status() != WL_CONNECTED) && (Intentos <= IntentosConexion)) {
          delay(500);
          Serial.println("--- Conectando ---");
          SerialBT.println("--- Conectando ---");
          Intentos++;                       
        }

        if (WiFi.status() == WL_CONNECTED) {
          char buffer[100];
          sprintf(buffer, "Red: %s\n  IP: %s", WiFi.SSID().c_str(), WiFi.localIP().toString().c_str());
      
          Serial.println(buffer);
          SerialBT.println(buffer);
        } else { 
          Serial.println("No conectado a ninguna red WiFi.");
          SerialBT.println("No conectado a ninguna red WiFi.");
        }
      delay(1000); 
    } else if (Eleccion == 4){
        // Crear Access Point propio
        initiAP(ap_ssid, ap_password);
      }
  delay(10);
  MenuMostrado = false;
  }
}

// ## Funciones detalladas
// ### KY-037
std::array<float, 2> LecturaMicrofono(){
  int crudo = analogRead(MicrofonoAnalogo);
  float voltaje = (crudo*3.3)/4095.0;
  return {(float)crudo, voltaje};
}

// ### BH1750
float LecturaLuxometro(){
  float lux = Luxometro.readLightLevel();
  return lux;
}

// ### Mostrar menu
int SeleccionarOpcion(){

  if (SerialBT.available() > 0) {
    String entrada = SerialBT.readStringUntil('\n');
    char opcion = entrada[0];
    
    if (opcion == '1'){
      return 1;
    } else if(opcion == '2'){
      return 2;
    } else if(opcion == '3'){
      return 3;
    } else if(opcion == '4'){
      return 4;
    } else {
      Serial.println("ADVERTENCIA - Opción inválida.");
      SerialBT.println("ADVERTENCIA - Opción inválida.");
      return 0;
    } 
  }
  return 0; 
}

// ## Access Point
// ### Configuración
void ap_start(char* ap_ssid, char* ap_password) {
  // Subred estándar del SoftAP: 192.168.4.0/24; el ESP32 será 192.168.4.1
  const IPAddress ip (192,168,4,1);
  const IPAddress gw (192,168,4,1); // gateway = IP del AP
  const IPAddress mask (255,255,255,0); // /24

  // 1) Fijar IP/gateway/máscara del interfaz AP antes de encender el SSID
  WiFi.softAPConfig(ip, gw, mask);

  // 2) Encender AP (WPA2 si ap_password >= 8). SSID visible; máx. clientes limitado.
  (void)WiFi.softAP(ap_ssid, ap_password, k_ap_chan, false, k_ap_maxcl);
  // DHCP del AP es automático en el stack; clientes obtendrán 192.168.4.x
}

// Getter sin estado global: consulta al stack cuando se necesite.
IPAddress ap_ip(void) {
  return WiFi.softAPIP();
}

// ### Manejo de solicitudes HTTP dirigidas al directorio raíz del servidor (que, en este caso, es el microcontrolador).
void handleRoot() {
  File file = SPIFFS.open("/index.html", "r"); // Para intentar abrir index.html en modo lectura.
  if (!file) {
    server.send(500, "text/plain", "No se pudo abrir el archivo HTML");
    return;
  }
  String html = file.readString(); // Lectura de archivo HTML como String.
  file.close(); // Cerrado para liberar recursos.
  server.send(200, "text/html", html); // Envía a los clientes la página web en HTML.
}

// Manejar cualquier petición no encontrada
void handleNotFound() {
  handleRoot();
}

void initWifiConnection(char* ssid, char* password) {
    // Conectar a red externa como cliente
    Serial.print("Conectando a WiFi: ");
    SerialBT.print("Conectando a WiFi: ");
    Serial.println(ssid);
    SerialBT.println(ssid);                     
    
    WiFi.begin(ssid, password);
}

void initiAP(char* ap_ssid, char* ap_password) {
    // Crear Access Point propio
    ap_start(ap_ssid, ap_password);

    // DNS server para redirigir tráfico del AP
    dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
  
    Serial.print("Access Point creado. IP del AP: ");
    Serial.println(WiFi.softAPIP());
    SerialBT.print("Access Point creado. IP del AP: ");
    SerialBT.println(WiFi.softAPIP());
  
    if(!SPIFFS.begin(true)){
      Serial.println("Error montando SPIFFS");
      return;
    }
  
    // Configurar manejadores del servidor web
    server.on("/", handleRoot);
    server.on("/index.html", handleRoot);
    server.on("/generate_204", handleRoot);    // Android captive portal detection
    server.on("/fwlink", handleRoot);          // Microsoft captive portal detection  
    server.on("/ncsi.txt", handleRoot);        // Microsoft Network Connectivity Status Indicator
    server.onNotFound(handleNotFound);         // Capturar cualquier otra petición
    server.begin();
}