// Librerías
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
#define LED_PIN 2
#define BUTTON_LIGHTS_PIN 23

// Instancias
WiFiConnection wifi(WIFI_SSID, WIFI_PASSWORD);
WiFiClient espClient;
PubSubClient client(espClient);
Buzzer buzzer(BUZZER_PIN, false);
RFID_mod rfid(SS_PIN, RST_PIN, UID_ESPERADO);

// Configuración NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600; // Colombia
const int daylightOffset_sec = 0;

// Variables
int id_nodo = 1;
bool status_identification = false;
int status_IR = LOW;
int status_PIR = LOW;
bool status_lights = LOW;

bool last_status_identification = false;
bool last_status_IR = false;
bool last_status_PIR = false;
bool last_status_buzzer = false;
bool last_status_lights = false;

int counter_RFID = 0;
bool tiempoSinSensar = false;
unsigned long tiempoInicioSinSensar = 0;
unsigned long tiempoInicioAlarma = 0;
bool puertaAbiertaSinID = false;

unsigned long tiempoInicioLuces = 0;
bool lucesForzadas = false;

// Funciones sensores
int lecture_ir_sensor() { return digitalRead(SENSOR_IR_PIN); }
int lecture_pir_sensor() { return digitalRead(SENSOR_PIR_PIN); }
bool lecture_rfid_sensor() { return rfid.tarjetaDetectada(); }
bool lecture_button_lights() { return digitalRead(BUTTON_LIGHTS_PIN); }

// MQTT
void reconnectMQTT() {
    while (!client.connected()) {
        Serial.print("Intentando conectar a MQTT...");
        if (client.connect("ESP32Client", MQTT_USER, MQTT_PASS)) {
            Serial.println("✅ Conectado a MQTT.");
            client.subscribe(MQTT_TOPIC);
        } else {
            Serial.print("❌ Fallo, rc=");
            Serial.print(client.state());
            Serial.println(" - Reintentando...");
            delay(3000);
        }
    }
}

// JSON Main
void sendMainMessage() {
    JsonDocument doc;
    doc["id"] = id_nodo;
    doc["door"] = status_IR;
    doc["buzzer"] = buzzer.getStatus();
    doc["identification"] = status_identification;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char timeString[20];
        strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M", &timeinfo);
        doc["date"] = timeString;
    } else {
        doc["date"] = "N/A";
    }
    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(MQTT_TOPIC, buffer);
    Serial.println("📤 Main JSON enviado.");
}

// JSON Secundario
void sendSecondaryMessage() {
    JsonDocument doc;
    doc["id"] = id_nodo;
    doc["presence"] = status_PIR;
    doc["lights"] = status_lights;
    struct tm timeinfo;
    if (getLocalTime(&timeinfo)) {
        char timeString[20];
        strftime(timeString, sizeof(timeString), "%Y-%m-%d %H:%M", &timeinfo);
        doc["date"] = timeString;
    } else {
        doc["date"] = "N/A";
    }
    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(MQTT_TOPIC2, buffer);
    Serial.println("📤 Secondary JSON enviado.");
}

// Setup
void setup() {
    Serial.begin(115200);
    pinMode(SENSOR_IR_PIN, INPUT);
    pinMode(SENSOR_PIR_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_LIGHTS_PIN, INPUT);

    wifi.begin();
    rfid.begin();
    client.setServer(MQTT_SERVER, MQTT_PORT);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    Serial.println("⌛ Esperando sincronización NTP...");
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
        Serial.println("⏳ Sincronizando hora...");
        delay(1000);
    }
    Serial.println("✅ Hora sincronizada.");
}

// Loop
void loop() {
    wifi.update();
    if (!wifi.connected()) return;

    if (!client.connected()) reconnectMQTT();
    client.loop();

    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return;

    // Sensado RFID
    if (lecture_rfid_sensor()) {
        counter_RFID++;
        if (counter_RFID >= 4) status_identification = true;
    } else {
        counter_RFID = 0;
    }

    status_IR = lecture_ir_sensor();
    status_PIR = lecture_pir_sensor();

    // 🚪 Salida detectada
    if (status_identification && status_IR == LOW && !tiempoSinSensar) {
        sendMainMessage();
        tiempoInicioSinSensar = millis();
        tiempoSinSensar = true;

        if (status_lights) {
            status_lights = false;
            digitalWrite(LED_PIN, LOW);
            sendSecondaryMessage();
        }
    }

    // ⏳ Pausa 7s post salida
    if (tiempoSinSensar) {
        if (millis() - tiempoInicioSinSensar < 7000) return;
        else tiempoSinSensar = false;
    }

    // 🚨 Alarma por puerta abierta sin ID
    if (status_IR == HIGH && !status_identification) {
        if (!puertaAbiertaSinID) {
            puertaAbiertaSinID = true;
            tiempoInicioAlarma = millis();
        }

        if (millis() - tiempoInicioAlarma >= 5000) {
            if (!buzzer.getStatus()) {
                buzzer.setStatus(true);
                buzzer.playTone();
            }
            static unsigned long lastSend = 0;
            if (millis() - lastSend >= 2000) {
                sendMainMessage();
                lastSend = millis();
            }
        }
        return;
    }

    // 🟢 Entrada detectada
    if (status_identification && status_IR == LOW && buzzer.getStatus()) {
        buzzer.setStatus(false);
        buzzer.turnOffSound();
        sendMainMessage();
        puertaAbiertaSinID = false;
    }

    // 🔄 Detección de cambios
    if (status_identification != last_status_identification || 
        status_IR != last_status_IR || 
        buzzer.getStatus() != last_status_buzzer) {
        sendMainMessage();
        last_status_identification = status_identification;
        last_status_IR = status_IR;
        last_status_buzzer = buzzer.getStatus();
    }

    if (status_PIR != last_status_PIR || status_lights != last_status_lights) {
        sendSecondaryMessage();
        last_status_PIR = status_PIR;
        last_status_lights = status_lights;
    }

    // 💡 Manejo de luces
    if (lecture_button_lights()) {
        digitalWrite(LED_PIN, HIGH);
        status_lights = true;
        lucesForzadas = true;
    } else if (!lucesForzadas && status_PIR && !status_lights) {
        digitalWrite(LED_PIN, HIGH);
        status_lights = true;
        tiempoInicioLuces = millis();
    }

    if (!lucesForzadas && status_lights && (millis() - tiempoInicioLuces >= 45000)) {
        digitalWrite(LED_PIN, LOW);
        status_lights = false;
    }

    if (!lecture_button_lights() && lucesForzadas) {
        lucesForzadas = false;
    }

    // Reset de identificación si todo está normal
    if (status_IR == LOW && !lecture_rfid_sensor()) {
        status_identification = false;
    }
}
