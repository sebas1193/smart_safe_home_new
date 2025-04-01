#include <Arduino.h>
#include "Buzzer.h"
#include "RFID_mod.h"
#include "WiFiConnection.h"
#include "Credentials.h"

#define SS_PIN 21
#define RST_PIN 22
#define BUZZER_PIN 25

WiFiConnection wifi(WIFI_SSID, WIFI_PASSWORD);

Buzzer buzzer(BUZZER_PIN, true);  // Inicialmente encendido
RFID_mod rfid(SS_PIN, RST_PIN, UID_ESPERADO);

bool accesoPermitido = false;  // Bandera para cortar el buzzer al primer escaneo correcto

void setup() {
    Serial.begin(115200);
    wifi.begin();
    rfid.begin();
}

void loop() {
    wifi.update();  // Actualiza el estado de la conexión WiFi
    if (!accesoPermitido && rfid.tarjetaDetectada()) {
        buzzer.setStatus(false);  // Apaga el buzzer al detectar la tarjeta correcta
        accesoPermitido = true;   // Bloquea nuevas activaciones
        Serial.println("Acceso permitido, buzzer apagado.");
    }

    buzzer.playTone();  // Solo se ejecutará si aún está activo
}