# Integración MQTT - Sensores ESP32

## ✅ Cambios Implementados

### 1. **Librería MQTT Agregada**
- Añadida `PubSubClient@^2.8` en `platformio.ini`
- Incluida en el código: `#include <PubSubClient.h>`

### 2. **Configuración MQTT**
```cpp
const char* MQTT_BROKER   = "89.117.53.122";
const uint16_t MQTT_PORT  = 1883;
const char* MQTT_USER     = "user1";
const char* MQTT_PASS     = "user1";
const char* TOPIC_LUX     = "user1/luxometro";
const char* TOPIC_MIC     = "user1/microfono";
const char* TOPIC_STATUS  = "user1/status";
```

### 3. **Nueva Opción en el Menú**
- **Opción 6 (o tecla 'M')**: Activa el modo MQTT
- El menú ahora muestra:
  ```
  1. KY-037 (25 lecturas)
  2. BH1750 (25 lecturas)
  3. Conectarse a WiFi
  4. Habilitar Access Point
  5. Cambiar a modo BLE (B)
  6. Mandar lecturas de prueba al servidor
  7. Cambiar a modo MQTT (M)  ← NUEVO
  ```

### 4. **Funciones MQTT Implementadas**

#### `mqttReconnect()`
- Reconecta automáticamente al broker MQTT
- Publica "online" en el topic `user1/status`
- Configura "offline" como Last Will (se publica automáticamente si el ESP32 se desconecta inesperadamente)

#### `activarModoMQTT()`
- Verifica que haya conexión WiFi antes de activar MQTT
- Muestra error si no hay WiFi
- Conecta al broker MQTT

#### `publicarDatosMQTT()`
- Lee ambos sensores (BH1750 y KY-037)
- Publica los datos cada 10 segundos:
  - **Luxómetro** → `user1/luxometro` (formato: "123.45")
  - **Micrófono** → `user1/microfono` (formato: "2.34" voltaje)

## 🚀 Cómo Usar

### Paso 1: Conectarse a WiFi
1. Conecta tu dispositivo al ESP32 por Bluetooth
2. Selecciona la opción **3** del menú
3. Ingresa el SSID de tu red WiFi
4. Ingresa la contraseña
5. Espera confirmación de conexión

### Paso 2: Activar MQTT
1. Una vez conectado a WiFi, selecciona la opción **7** (o presiona 'M')
2. El ESP32 se desconectará del Bluetooth Clásico
3. Se conectará al broker MQTT
4. Comenzará a publicar datos automáticamente cada 10 segundos

### Paso 3: Monitorear Datos
Puedes monitorear los datos desde cualquier cliente MQTT suscribiéndote a:
- `user1/luxometro` - Datos del sensor de luz
- `user1/microfono` - Datos del micrófono
- `user1/status` - Estado del dispositivo (online/offline)

## 📊 Topics MQTT

| Topic | Descripción | Formato de Datos |
|-------|-------------|------------------|
| `user1/luxometro` | Nivel de iluminación | `"123.45"` (lux) |
| `user1/microfono` | Voltaje del micrófono | `"2.34"` (voltios) |
| `user1/status` | Estado de conexión | `"online"` o `"offline"` |

## ⚙️ Configuración del Sistema

### Client ID Único
El ESP32 genera un Client ID único basado en su dirección MAC:
```cpp
esp32-user1-XXYYZZ
```
Donde XX, YY, ZZ son los últimos 3 bytes de la MAC.

### Intervalo de Publicación
Por defecto, los datos se publican cada **10 segundos**. Puedes modificar esto cambiando:
```cpp
const long MQTT_INTERVAL = 10000; // milisegundos
```

### Last Will Testament (LWT)
Si el ESP32 se desconecta inesperadamente, el broker publicará automáticamente `"offline"` en el topic `user1/status`.

## 🔍 Verificación del Estado

### Monitor Serial
El ESP32 imprime en el puerto serial:
- Estado de conexión WiFi
- Estado de conexión MQTT
- Datos publicados en cada intervalo
- Errores de conexión si los hay

Ejemplo de salida:
```
Iniciando modo MQTT...
WiFi conectado, IP: 192.168.1.100
Intentando conexión MQTT...conectado
Modo MQTT activado. Publicando datos cada 10 segundos...
Publicado en user1/luxometro: 456.78
Publicado en user1/microfono: 1.23
```

## ⚠️ Importante

1. **WiFi Requerido**: Debes conectarte a WiFi (opción 3) ANTES de activar MQTT
2. **Desconexión Bluetooth**: Al activar MQTT, el Bluetooth Clásico se desconecta
3. **Modos Exclusivos**: Solo puede estar activo un modo a la vez (BLE, MQTT, o Bluetooth Clásico)
4. **Reiniciar**: Para volver al menú Bluetooth, debes reiniciar el ESP32

## 🔧 Compilación y Carga

```bash
# Compilar
platformio run

# Compilar y cargar
platformio run --target upload

# Monitor serial
platformio device monitor
```

## 📝 Notas Técnicas

- **RAM Usage**: 18.3% (60,068 bytes)
- **Flash Usage**: 52.9% (1,665,197 bytes)
- **Compilación**: ✅ Exitosa
- **Librerías**: Todas las dependencias resueltas correctamente
