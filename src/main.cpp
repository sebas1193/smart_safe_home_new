#include <Arduino.h>
#include <PubSubClient.h>
#include <ArduinoJson.h>
#include "Buzzer.h"
#include "RFID_mod.h"
#include "WiFiConnection.h"
#include "Credentials.h"

// Pines
#define SENSOR_IR_PIN 15
#define SENSOR_PIR_PIN 13
#define SS_PIN 21
#define RST_PIN 22
#define BUZZER_PIN 25

// Instancias
WiFiConnection wifi(WIFI_SSID, WIFI_PASSWORD);
WiFiClient espClient;
PubSubClient client(espClient);
Buzzer buzzer(BUZZER_PIN, false);
RFID_mod rfid(SS_PIN, RST_PIN, UID_ESPERADO);

// Variables globales
int contador = 0;
bool lectura_RFID = false;
bool salida = false;
bool buzzerActivo = false;
unsigned long tiempoInicioEvento = 0;
unsigned long lastJsonTime = 0;
unsigned long lastFullReportTime = 0;

// Funciones de sensores
int lecture_ir_sensor() { return digitalRead(SENSOR_IR_PIN); }
int lecture_pir_sensor() { return digitalRead(SENSOR_PIR_PIN); }
bool lecture_rfid_sensor() { return rfid.tarjetaDetectada(); }

// Función para reconectar MQTT
void reconnectMQTT() {
    while (!client.connected()) {
        Serial.print("Intentando conectar a MQTT...");
        // Conexión con usuario y contraseña (Maquiato requiere esto)
        if (client.connect("ESP32Client", MQTT_USER, MQTT_PASS)) {
            Serial.println("✅ Conectado a MQTT.");
            client.subscribe(MQTT_TOPIC);
        } else {
            Serial.print("❌ Fallo, rc=");
            Serial.print(client.state());
            Serial.println(" - Reintentando en 3 segundos...");
            delay(3000);
        }
    }
}


// Enviar estado en JSON
void sendMessage() {
    JsonDocument doc;
    doc["door"] = lecture_ir_sensor();
    doc["presence"] = lecture_pir_sensor();
    doc["card"] = lectura_RFID;
    doc["Buzzer"] = buzzer.getStatus();
    doc["access"] = lectura_RFID;
    doc["salida"] = salida;

    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(MQTT_TOPIC, buffer);
    Serial.println("📤 Mensaje enviado:");
    Serial.println(buffer);
}

void setup() {
    Serial.begin(115200);
    pinMode(SENSOR_IR_PIN, INPUT);
    pinMode(SENSOR_PIR_PIN, INPUT);
    wifi.begin();
    rfid.begin();
    client.setServer(MQTT_SERVER, MQTT_PORT);
}

void loop() {
    // Verificar conexión WiFi y MQTT
    wifi.update();
    if (!wifi.connected()) {
        Serial.println("❌ Sin conexión WiFi.");
        return;
    }

    if (!client.connected()) {
        reconnectMQTT();
    }
    client.loop();

    // Lectura de sensores
    int estadoIR = lecture_ir_sensor();     // LOW = cerrada, HIGH = abierta
    int estadoPIR = lecture_pir_sensor();   // LOW = sin presencia, HIGH = hay presencia
    bool tarjetaDetectada = lecture_rfid_sensor();

    // Contador de lecturas RFID
    if (tarjetaDetectada) {
        contador++;
    } else {
        contador = 0;
    }
    lectura_RFID = (contador >= 4);

    // Caso 1: Tarjeta válida con puerta cerrada → Se prepara para salida
    if (lectura_RFID && estadoIR == LOW) {
        if (tiempoInicioEvento == 0) tiempoInicioEvento = millis();
        if (millis() - tiempoInicioEvento >= 10000) {
            salida = true;
            sendMessage();
            salida = false;
            tiempoInicioEvento = 0;
        }
    }

    // Caso 2: Puerta abierta sin tarjeta válida → Activar alarma
    else if (!lectura_RFID && estadoIR == HIGH) {
        if (tiempoInicioEvento == 0) tiempoInicioEvento = millis();
        if (millis() - tiempoInicioEvento >= 5000) {
            if (!buzzer.getStatus()) {
                buzzer.setStatus(true);
                Serial.println("🔔 Buzzer activado por puerta abierta sin tarjeta.");
            }
            buzzer.playTone();
            if (millis() - lastJsonTime >= 2000) {
                sendMessage();
                lastJsonTime = millis();
            }
        }
    }

    // Caso 3: Se presenta tarjeta mientras la puerta está abierta → Apagar alarma
    else if (lectura_RFID && estadoIR == HIGH && buzzer.getStatus()) {
        buzzer.setStatus(false);
        buzzer.turnOffSound();
        Serial.println("🔕 Buzzer desactivado por tarjeta válida.");
        buzzer.setStatus(false);
        sendMessage();
        tiempoInicioEvento = 0;
    }

    // Reset si ninguna condición especial
    else {
        tiempoInicioEvento = 0;
        buzzer.turnOffSound();  // evitar sonido residual
    }

    // Reporte general cada 4s
    if (millis() - lastFullReportTime >= 4000) {
        sendMessage();
        lastFullReportTime = millis();
    }
}
