#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Buzzer.h"
#include "RFID_mod.h"
#include "WiFiConnection.h"
#include "Credentials.h"

// Definición de pines
#define SENSOR_IR_PIN 15
#define SENSOR_PIR_PIN 13
#define SS_PIN 21
#define RST_PIN 22
#define BUZZER_PIN 25

WiFiConnection wifi(WIFI_SSID, WIFI_PASSWORD);
Buzzer buzzer(BUZZER_PIN, false);
RFID_mod rfid(SS_PIN, RST_PIN, UID_ESPERADO);
WiFiClient espClient;
PubSubClient client(espClient);

bool accesoPermitido = false;
int contador = 0;
unsigned long milliseconds = 0;
int salida = 0;
unsigned long lastJsonTime = 0;
unsigned long lastFullReportTime = 0;  // Nuevo: Para enviar datos cada 4s

int lecture_ir_sensor() { return digitalRead(SENSOR_IR_PIN); }
int lecture_pir_sensor() { return digitalRead(SENSOR_PIR_PIN); }
bool lecture_rfid_sensor() { return rfid.tarjetaDetectada(); }

void sendMessage() {
    JsonDocument doc;
    doc["door"] = lecture_ir_sensor();
    doc["presence"] = lecture_pir_sensor();
    doc["card"] = lecture_rfid_sensor();
    doc["Buzzer"] = buzzer.getStatus();
    doc["access"] = accesoPermitido;
    doc["salida"] = salida;

    char jsonBuffer[256];
    serializeJson(doc, jsonBuffer);
    client.publish(MQTT_TOPIC, jsonBuffer);
}

void setup() {
    Serial.begin(115200);
    wifi.begin();
    rfid.begin();
    pinMode(SENSOR_IR_PIN, INPUT);
    pinMode(SENSOR_PIR_PIN, INPUT);
}

void loop() {
    wifi.update();

    int estadoIR = lecture_ir_sensor();
    int estadoPIR = lecture_pir_sensor();
    bool tarjetaDetectada = lecture_rfid_sensor();

    // Condición para activar la alarma si la puerta está abierta sin presencia
    if (estadoIR == HIGH && estadoPIR == LOW) {
        if (milliseconds == 0) {
            milliseconds = millis();  // Guardamos el tiempo de inicio
        }

        if ((millis() - milliseconds >= 5000) && !tarjetaDetectada) {
            buzzer.setStatus(true);
            buzzer.playTone();
            Serial.println("Buzzer activado por sensor IR.");
            accesoPermitido = false;
            sendMessage();  // Enviar JSON cuando se activa el buzzer
        }

        if (tarjetaDetectada) {
            buzzer.setStatus(false);
            buzzer.playTone();
            Serial.println("Buzzer apagado por sensor RFID.");
            accesoPermitido = true;
            sendMessage();
            milliseconds = 0; // Reset del contador
        }
    } else {
        milliseconds = 0; // Reset si la condición cambia
    }

    // Detección de salida (4 lecturas consecutivas del RFID)
    if (estadoPIR == HIGH && estadoIR == LOW) {
        if (tarjetaDetectada) {
            contador++;
        } else {
            contador = 0; // Resetea si no se detecta UID consecutivamente
        }

        if (contador >= 4) {
            buzzer.setStatus(false);
            salida = 1;
            sendMessage();
        }
    }

    // Enviar JSON cada 2s si el buzzer está activo
    if (buzzer.getStatus() && millis() - lastJsonTime >= 2000) {
        sendMessage();
        lastJsonTime = millis();
    }

    // **Enviar el estado de los sensores cada 4 segundos**
    if (millis() - lastFullReportTime >= 4000) {
        sendMessage();
        lastFullReportTime = millis();
    }
}
