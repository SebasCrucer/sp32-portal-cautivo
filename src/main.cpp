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


// # Setup + Loop
void setup(){
  Serial.begin(9600);

  // ## Inicializar BH1750
  Wire.begin(); // iniciar protocolo I2C
  Luxometro.begin(); // Iniciar BH1750

  // Bluetooth Serial
  SerialBT.begin("Sensores_ESP32"); // Nombre del sensor
}

void loop(){
  int Eleccion = SeleccionarOpcion();
  if(Eleccion == 1){
    // KY-037
    for(int i = 0; i < CantidadLecturas; i++){
      std::array<float, 2> Inputs = LecturaMicrofono();
      char buffer[100]; // El uso de String es incompatible con sprintf. Un arreglo de chars es necesario.
      sprintf(buffer, "Valor digital: %d | Volts: %.04f", (int)Inputs[0], Inputs[1]);
      Serial.println(buffer);
      SerialBT.println(buffer);
      delay(1000);
    }

  } else if(Eleccion == 2){
    // BH1750
    for(int i = 0; i < CantidadLecturas; i++){
      float Input = LecturaLuxometro();
      char buffer[100];
      sprintf(buffer, "Iluminancia: %.04f lx", Input);
      Serial.println(buffer);
      SerialBT.println(buffer);
      delay(1000);
    }
  }
}


// # Funciones post-prototipado

// ## KY-037
std::array<float, 2> LecturaMicrofono(){
  int crudo = analogRead(MicrofonoAnalogo);
  float voltaje = (crudo*3.3)/4095.0;
  return {crudo, voltaje};
}

// ## BH1750
float LecturaLuxometro(){
  float lux = Luxometro.readLightLevel(); // Regresa medida de luminosidad/m^2 (lux)
  return lux;
}

// ## Mostrar menu
int SeleccionarOpcion(){
  char buffer[1000];
  sprintf(buffer, "# Opciones:\n1. KY-037 (%d lecturas)\n2. BH1750 (%d lecturas)", CantidadLecturas, CantidadLecturas);
  Serial.println(buffer);
  SerialBT.println(buffer);

  while (SerialBT.available() == 0){ // Esto detendrá el código mientras se esperan datos.
    delay(100);
  }

  String entrada = SerialBT.readStringUntil('\n');
  char opcion = entrada[0]; // Sólo se rescata el primer caracter del String.

  while(true){
    if (opcion == '1'){
      return 1;
    } else if(opcion == '2'){
      return 2;
    } else {
      Serial.println("ADVERTENCIA - Opción inválida.");
      SerialBT.println("ADVERTENCIA - Opción inválida.");
    }
  }
}