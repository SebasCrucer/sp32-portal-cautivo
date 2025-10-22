# Solución: Portal Cautivo Inaccesible

## 🔴 Problema

El portal cautivo a veces era inaccesible aunque el dispositivo estuviera conectado a la red del Access Point.

## ✅ Causas Identificadas

### 1. **Bloqueo del servidor web cuando había cliente Bluetooth**

El código original tenía esta lógica:

```cpp
if(!SerialBT.hasClient()){
  MenuMostrado = false;
  return; // ⚠️ Salía del loop sin procesar DNS/Web
}
```

Esto causaba que el servidor DNS y web **NO se procesaran** cuando había un cliente Bluetooth conectado.

### 2. **Bloqueos durante entrada de usuario**

En la opción 3 (conectar WiFi), el código esperaba entrada bloqueando el servidor:

```cpp
while(!SerialBT.available()) {
  delay(10); // ⚠️ NO procesaba DNS/Web durante la espera
}
```

### 3. **Servidor DNS/Web solo se procesaba en momentos específicos**

- Solo se procesaba durante las lecturas de sensores
- No se procesaba en el menú principal
- No se procesaba durante las operaciones de configuración

## ✨ Soluciones Implementadas

### 1. **Procesamiento continuo del servidor**

```cpp
void loop(){
  // ... código de BLE/MQTT ...

  // Procesar DNS y servidor web en CADA iteración
  dnsServer.processNextRequest();
  server.handleClient();

  // Mostrar menú si hay cliente Bluetooth
  if(!MenuMostrado && SerialBT.hasClient()){
    // ... mostrar menú ...
  }

  // ... resto del código ...
}
```

### 2. **Esperas no bloqueantes**

```cpp
// Esperar SSID (sin bloquear el servidor web)
while(!SerialBT.available()) {
  dnsServer.processNextRequest();
  server.handleClient();
  delay(10);
}
```

### 3. **Procesamiento durante conexión WiFi**

```cpp
while ((WiFi.status() != WL_CONNECTED) && (Intentos < IntentosConexion)) {
  delay(500);
  // Procesar servidor web durante conexión
  dnsServer.processNextRequest();
  server.handleClient();
  Intentos++;
}
```

### 4. **Reset del menú después de crear AP**

```cpp
} else if (Eleccion == 4){
  // Crear Access Point propio
  initiAP(ap_ssid, ap_password);
  MenuMostrado = false; // ✅ Resetear para mostrar menú nuevamente
}
```

## 🎯 Recomendaciones Adicionales

### 1. **Verificar conexión desde dispositivos móviles**

- **Android**: Busca automáticamente portales cautivos con `/generate_204`
- **iOS**: Usa `/hotspot-detect.html`
- **Windows**: Usa `/ncsi.txt` y `/connecttest.txt`

Asegúrate de que todos estos endpoints estén configurados (ya lo están en tu código).

### 2. **Probar la conexión**

```bash
# Desde terminal o navegador
ping 192.168.4.1
curl http://192.168.4.1
```

### 3. **Verificar el DNS**

El DNS server debe estar respondiendo en el puerto 53. Ya está configurado correctamente con:

```cpp
dnsServer.start(DNS_PORT, "*", WiFi.softAPIP());
```

### 4. **Timeout de delay reducido**

Considera reducir los delays largos (como el de 10 segundos al final de la opción 6) o agregar procesamiento del servidor durante esos delays:

```cpp
for(int i = 0; i < 100; i++) { // 10 segundos = 100 * 100ms
  dnsServer.processNextRequest();
  server.handleClient();
  delay(100);
}
```

## 🧪 Cómo Probar

1. **Sube el código actualizado al ESP32**
2. **Conecta un dispositivo a la red "AP-ESP32"** (contraseña: 12345678)
3. **Navega a http://192.168.4.1** en el navegador
4. **El portal cautivo debería aparecer automáticamente** en dispositivos móviles
5. **Prueba conectar Bluetooth y verifica que el portal siga funcionando**

## 📊 Resultado Esperado

Ahora el portal cautivo debería ser **siempre accesible** porque:

- ✅ El servidor DNS/Web se procesa en cada iteración del loop
- ✅ No hay bloqueos durante la entrada de usuario
- ✅ El servidor funciona independientemente del estado de Bluetooth
- ✅ Los modos BLE y MQTT tienen return temprano para no interferir
