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

// ## KY-037
#define MicrofonoAnalogo 34
#define MicrofonoDigital 15

// ## BH1750
BH1750 Luxometro; // Objeto BH1750

// ## Bluetooth
BluetoothSerial SerialBT;

// Datos de acceso a WIFI - ESP32 -> Internet
const char* ssid = "Alumnos";
const char* password = "@@1umN05@@";

// Datos para conectarse al AP (esp32)
const char* ap_ssid = "ESP32_AP";
const char* ap_password = "12345678";

// Configuración del DNS
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Configuración del server (servicio que ofrece el HTML del portal)
WebServer server(80);


// # Prototipado de funciones
std::array<float, 2> LecturaMicrofono(); // Regresará un arreglo de dos números, no es posible usar algo como float[2] porque ese objeto sería destruido después de terminar la función.
float LecturaLuxometro();
int SeleccionarOpcion();
void handleRoot();
void handleNotFound();

// # Setup + Loop
void setup(){
  Serial.begin(9600);

  SerialBT.begin("Sensores_ESP32"); 

  // Modo combinado: AP + Station (puede conectarse Y crear red)
  WiFi.mode(WIFI_AP_STA);

  // Conectar a red externa como cliente
  Serial.print("Conectando a WiFi: ");
  SerialBT.print("Conectando a WiFi: ");
  Serial.println(ssid);
  SerialBT.println(ssid);                     
  
  WiFi.begin(ssid, password);

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

  // Crear Access Point propio
  WiFi.softAP(ap_ssid, ap_password);

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

  // ## Inicializar BH1750
  Wire.begin(); // iniciar protocolo I2C
  Luxometro.begin(); // Iniciar BH1750

}

void loop(){
  dnsServer.processNextRequest();
  server.handleClient();
  
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
    }
  }
  
  delay(10);
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
  char buffer[1000];
  sprintf(buffer, "# Opciones:\n1. KY-037 (%d lecturas)\n2. BH1750 (%d lecturas)", CantidadLecturas, CantidadLecturas);
  Serial.println(buffer);
  SerialBT.println(buffer);

  if (SerialBT.available() > 0) {
    String entrada = SerialBT.readStringUntil('\n');
    char opcion = entrada[0];
    
    if (opcion == '1'){
      return 1;
    } else if(opcion == '2'){
      return 2;
    } else if(opcion == '3'){
      return 3;
    } else {
      Serial.println("ADVERTENCIA - Opción inválida.");
      SerialBT.println("ADVERTENCIA - Opción inválida.");
      return 0;
    } 
  }
  return 0; 
}

// ## Access Point
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