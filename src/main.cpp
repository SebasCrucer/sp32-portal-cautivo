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

// Datos de acceso a WIFI - ESP32 -> Internet
// const char* ssid = "Alumnos";
// const char* password = "@@1umN05@@";

// Datos para conectarse al AP (esp32)
const char* ap_ssid = "ESP32_AP";
const char* ap_password = "12345678";

// Configuración del DNS
const byte DNS_PORT = 53;
DNSServer dnsServer;

// Configuración del server (servicio que ofrece el HTML del portal)
WebServer server(80);

// # Variables
#define CantidadLecturas 25

// ## KY-037
#define MicrofonoAnalogo 34
#define MicrofonoDigital 15

// ## BH1750
BH1750 Luxometro; // Objeto BH1750

// ## Bluetooth
BluetoothSerial SerialBT;

// # Prototipado de funciones
std::array<float, 2> LecturaMicrofono(); // Regresará un arreglo de dos números, no es posible usar algo como float[2] porque ese objeto sería destruido después de terminar la función.
float LecturaLuxometro();
int SeleccionarOpcion();

void handleRoot() {
  File file = SPIFFS.open("/index.html", "r");
  if (!file) {
    server.send(500, "text/plain", "No se pudo abrir el archivo HTML");
    return;
  }
  String html = file.readString();
  file.close();
  server.send(200, "text/html", html);
}

// # Setup + Loop
void setup(){
  Serial.begin(9600);

  SerialBT.begin("Sensores_ESP32"); 

  // Serial.print("Conectando a WiFi: ");
  // SerialBT.print("Conectando a WiFi: ");
  // Serial.println(ssid);
  // SerialBT.println(ssid);                     
  
  // WiFi.begin(ssid, password);

  // while (WiFi.status() != WL_CONNECTED) {
  //   delay(500);
  //   Serial.print("--- Conectando ---");
  //   SerialBT.print("--- Conectando ---");                       
  // }

  // Serial.println("\nWiFi conectado :)");
  // SerialBT.println("\nWiFi conectado :)"); 
  // Serial.print("Dirección IP: ");
  // SerialBT.print("Dirección IP: ");               
  // Serial.println(WiFi.localIP());
  // SerialBT.println(WiFi.localIP());               

  WiFi.mode(WIFI_AP);
  WiFi.softAP(ap_ssid, ap_password);

  dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());

  Serial.print("Access Point creado. IP: ");
  Serial.println(WiFi.softAPIP());
  SerialBT.print("Access Point creado. IP: ");
  SerialBT.println(WiFi.softAPIP());

  if(!SPIFFS.begin(true)){
    Serial.println("Error montando SPIFFS");
    return;
  }

  server.onNotFound(handleRoot);
  server.on("/", handleRoot);
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
    }
  }
  
  delay(10);
}

// ## KY-037
std::array<float, 2> LecturaMicrofono(){
  int crudo = analogRead(MicrofonoAnalogo);
  float voltaje = (crudo*3.3)/4095.0;
  return {crudo, voltaje};
}

// ## BH1750
float LecturaLuxometro(){
  float lux = Luxometro.readLightLevel();
  return lux;
}

// ## Mostrar menu
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
    } else {
      Serial.println("ADVERTENCIA - Opción inválida.");
      SerialBT.println("ADVERTENCIA - Opción inválida.");
      return 0;
    }
  }
  return 0; 
}
