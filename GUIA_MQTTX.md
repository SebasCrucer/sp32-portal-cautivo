# Guía Rápida: Cómo Usar MQTTX con tu ESP32

## 📱 Configurar Conexión en MQTTX

### 1. Abrir MQTTX y crear nueva conexión
1. Abrir MQTTX
2. Clic en "+ New Connection"
3. Configurar:

```
Name: ESP32 Sensores User1
Host: 89.117.53.122
Port: 1883
Username: user1
Password: user1
Client ID: mqttx-user1-mac (o déjalo automático)
```

4. Clic en "Connect" (botón superior derecho)

### 2. Suscribirse a los topics
Una vez conectado:
1. En el campo "Topic" escribe: `user1/#`
2. Clic en "Subscribe"

El símbolo `#` es un comodín que significa "todos los sub-topics bajo user1/"

## 📊 Qué Verás en MQTTX

Cuando tu ESP32 esté en modo MQTT (opción 7), verás estos mensajes:

### user1/status
- **Al conectar**: `online`
- **Al desconectar**: `offline`
- Este topic tiene **retain** = true (el último valor queda guardado)

### user1/luxometro
- Valor de iluminancia del sensor BH1750
- Formato: `123.45` (número decimal en lux)
- Se actualiza cada 10 segundos

### user1/microfono
- Voltaje del micrófono KY-037
- Formato: `1.23` (voltaje en V)
- Se actualiza cada 10 segundos

### user1/datos (NUEVO)
- Todos los datos combinados en JSON
- Formato:
```json
{
  "lux": 123.45,
  "mic": 1.23,
  "raw": 2048,
  "millis": 123456
}
```
- `lux`: Iluminancia en lux
- `mic`: Voltaje del micrófono
- `raw`: Valor crudo ADC del micrófono (0-4095)
- `millis`: Tiempo desde que arrancó el ESP32 (milisegundos)

## 🎯 Ejemplo de Uso

### Paso 1: Subir el código al ESP32
```bash
platformio run --target upload
```

### Paso 2: Abrir monitor serial (opcional)
```bash
platformio device monitor
```

### Paso 3: Conectar por Bluetooth
1. Conecta tu teléfono/PC al ESP32 vía Bluetooth
2. Nombre del dispositivo: `Sensores_ESP33`

### Paso 4: Conectar a WiFi
En el terminal Bluetooth:
- Opción **3**: Conectarse a WiFi
- Ingresa SSID de tu red
- Ingresa contraseña
- Espera confirmación (debe mostrar IP)

### Paso 5: Activar modo MQTT
- Opción **7** (o tecla 'M')
- El ESP32 se desconecta del Bluetooth
- Se conecta al broker MQTT
- Comienza a publicar datos cada 10 segundos

### Paso 6: Ver datos en MQTTX
En MQTTX deberías ver:
```
user1/status → online
user1/luxometro → 456.78
user1/microfono → 1.23
user1/datos → {"lux":456.78,"mic":1.23,"raw":2048,"millis":123456}
```

## 🧪 Pruebas Útiles

### Probar Last Will Testament (LWT)
1. Con el ESP32 conectado y publicando
2. Desconecta la alimentación (quita el cable USB)
3. En pocos segundos verás en MQTTX:
   ```
   user1/status → offline
   ```
4. Este mensaje lo publica el **broker automáticamente** cuando detecta que el ESP32 se cayó

### Ver el JSON en formato bonito
1. En MQTTX, clic en el mensaje de `user1/datos`
2. En la vista de detalles, verás el JSON formateado:
```json
{
  "lux": 456.78,
  "mic": 1.23,
  "raw": 2048,
  "millis": 123456
}
```

### Cambiar intervalo de publicación
Si quieres publicar más rápido o más lento:
1. Edita `main.cpp`:
```cpp
const long MQTT_INTERVAL = 5000;  // 5 segundos (en lugar de 10)
```
2. Recompila y sube:
```bash
platformio run --target upload
```

## 🔍 Solución de Problemas

### No veo ningún mensaje en MQTTX
- ✅ Verifica que MQTTX esté conectado (punto verde arriba)
- ✅ Verifica que estés suscrito a `user1/#`
- ✅ Revisa el monitor serial del ESP32 (debe decir "conectado")
- ✅ Verifica que el ESP32 tenga WiFi (opción 3 primero)

### Solo veo "offline" pero no "online"
- El ESP32 probablemente no logró conectarse a WiFi
- Revisa credenciales WiFi
- Verifica que la red WiFi tenga acceso a Internet

### Los valores de lux son 0 o negativos
- Sensor BH1750 no está conectado o mal cableado
- Verifica conexiones I2C (SDA=GPIO21, SCL=GPIO22, VCC=3.3V, GND)

### Los valores del micrófono no cambian
- Sensor KY-037 no está conectado o mal cableado
- Verifica conexión analógica (AO → GPIO34, VCC=3.3V, GND)

## 📱 Alternativas a MQTTX

### Opción 1: Línea de comandos con mosquitto
```bash
# Instalar (solo una vez)
brew install mosquitto

# Suscribirse a todos los topics
mosquitto_sub -h 89.117.53.122 -p 1883 -u user1 -P user1 -t 'user1/#' -v

# Ver solo el JSON combinado
mosquitto_sub -h 89.117.53.122 -p 1883 -u user1 -P user1 -t 'user1/datos' -v
```

### Opción 2: MQTT Explorer (alternativa gráfica)
Descarga: https://mqtt-explorer.com
(Configuración idéntica a MQTTX)

## 🎓 Conceptos MQTT Explicados

### QoS (Quality of Service)
- **QoS 0**: El mensaje se envía una vez (sin confirmación) ← Usamos este
- **QoS 1**: El mensaje se entrega al menos una vez (con confirmación)
- **QoS 2**: El mensaje se entrega exactamente una vez (handshake completo)

### Retain Flag
- Cuando un mensaje tiene `retain=true`, el broker guarda el **último** mensaje
- Los nuevos clientes que se suscriban reciben ese último mensaje inmediatamente
- Útil para el status: un cliente nuevo sabrá si el ESP32 está online/offline sin esperar

### Last Will Testament (LWT)
- Es un mensaje que el broker publica **automáticamente** si el cliente se desconecta inesperadamente
- Se configura al conectar: `mqttClient.connect(..., TOPIC_STATUS, 0, true, "offline")`
- Útil para detectar caídas del ESP32

## 🚀 Próximas Mejoras Posibles

1. **Suscribirse a comandos**: ESP32 puede recibir comandos desde MQTTX
2. **Publicar con QoS 1**: Mayor confiabilidad en redes inestables
3. **Añadir timestamp real**: Usar NTP para fecha/hora real (en lugar de millis)
4. **Guardar credenciales WiFi**: Usar NVS para no pedirlas cada vez
5. **Reconexión WiFi automática**: Si se cae WiFi, reconectar sin reset

¿Necesitas alguna de estas mejoras? ¡Solo pide!
