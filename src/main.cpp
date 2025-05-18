// main.cpp - Versión revisada con corrección de lógica de ventana de gracia
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
#define LED_PIN 32
#define BUTTON_LIGHTS_PIN 5

// Instancias
WiFiConnection wifi(WIFI_SSID, WIFI_PASSWORD);
WiFiClient espClient;
PubSubClient client(espClient);
Buzzer buzzer(BUZZER_PIN, false);
RFID_mod rfid(SS_PIN, RST_PIN, UID_ESPERADO);

// Configuración NTP
const char* ntpServer = "pool.ntp.org";
const long gmtOffset_sec = -5 * 3600;
const int daylightOffset_sec = 0;

// Variables de estado
const char* id_nodo = "NODE_A";
bool status_identification = false;
int status_IR = LOW;
int status_PIR = LOW;
bool status_lights = LOW;

// Estados anteriores para envío MQTT
bool last_identification = false;
bool last_IR = false;
bool last_buzzer = false;
bool last_PIR = false;
bool last_lights = false;

// Temporizadores
unsigned long tiempoInicioLuces = 0;
bool lucesForzadas = false;

// Identificación y alarma
bool puertaAbierta = false;
unsigned long tiempoApertura = 0;
bool enVentana3s = false;
unsigned long inicioVentana3s = 0;

// Envío periódico en alarma
unsigned long lastSend = 0;

// Tiempo para resetear la identificación después de una lectura exitosa
unsigned long tiempoIdentificacion = 0;
const unsigned long TIMEOUT_IDENTIFICACION = 5000; // 5 segundos para resetear el estado

// Nueva variable para controlar ventana de gracia para salida
bool enVentanaGracia = false;
unsigned long inicioVentanaGracia = 0;
const unsigned long TIEMPO_VENTANA_GRACIA = 7000; // 7 segundos de ventana de gracia

// Para controlar si una alarma fue cancelada con identificación
bool alarmaCancelada = false;

// Lecturas
int lecture_ir_sensor() { return digitalRead(SENSOR_IR_PIN); }
int lecture_pir_sensor() { return digitalRead(SENSOR_PIR_PIN); }
bool lecture_rfid_sensor() { return rfid.tarjetaDetectada(); }
bool lecture_button_lights() { return digitalRead(BUTTON_LIGHTS_PIN); }

// Reconexión MQTT
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

// Formateo de fecha
String getTimestamp() {
    struct tm timeinfo;
    if (!getLocalTime(&timeinfo)) return "N/A";
    char buf[20];
    strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
    return String(buf);
}

// Envío JSON genérico
void sendMainMessage() {
    StaticJsonDocument<256> doc;
    doc["id_node"] = id_nodo;
    doc["door"] = status_IR;
    doc["buzzer"] = buzzer.getStatus();
    doc["identification"] = status_identification;
    doc["sent_at"] = getTimestamp();
    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(MQTT_TOPIC, buffer);
    Serial.println("📤 Main JSON enviado: " + String(buffer));
}

void sendSecondaryMessage() {
    StaticJsonDocument<256> doc;
    doc["id_node"] = id_nodo;
    doc["presence"] = status_PIR;
    doc["lights"] = status_lights;
    doc["sent_at"] = getTimestamp();
    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(MQTT_TOPIC2, buffer);
    Serial.println("📤 Secondary JSON enviado: " + String(buffer));
}

void setup() {
    Serial.begin(115200);
    pinMode(SENSOR_IR_PIN, INPUT);
    pinMode(SENSOR_PIR_PIN, INPUT);
    pinMode(LED_PIN, OUTPUT);
    pinMode(BUTTON_LIGHTS_PIN, INPUT_PULLUP);

    wifi.begin();
    rfid.begin();
    client.setServer(MQTT_SERVER, MQTT_PORT);
    configTime(gmtOffset_sec, daylightOffset_sec, ntpServer);

    // Esperar NTP
    Serial.println("⌛ Sincronizando hora NTP...");
    struct tm timeinfo;
    while (!getLocalTime(&timeinfo)) {
        delay(500);
    }
    Serial.println("✅ Hora sincronizada: " + getTimestamp());
}

void loop() {
    wifi.update();
    if (!wifi.connected()) return;

    if (!client.connected()) reconnectMQTT();
    client.loop();

    // Actualizar sensores
    int ir = lecture_ir_sensor();
    int pir = lecture_pir_sensor();
    bool rfid_ok = lecture_rfid_sensor();
    bool btn = lecture_button_lights();

    unsigned long now = millis();

    // === Control PIR/luces ===
    if (pir == HIGH && !lucesForzadas) {
        digitalWrite(LED_PIN, HIGH);
        status_lights = true;
        tiempoInicioLuces = now;
    }
    if (btn) {
        digitalWrite(LED_PIN, HIGH);
        status_lights = true;
        lucesForzadas = true;
        tiempoInicioLuces = 0;
    }
    if (!btn && lucesForzadas) lucesForzadas = false;
    if (status_lights && !lucesForzadas && (now - tiempoInicioLuces >= 30000)) {
        digitalWrite(LED_PIN, LOW);
        status_lights = false;
    }

    // === Detección de puerta y UID ===
    status_IR = ir;

    // === IMPORTANTE: Silenciar buzzer siempre que se detecte un UID válido ===
    if (rfid_ok) {
        status_identification = true;
        tiempoIdentificacion = now;
        buzzer.setStatus(false);
        buzzer.turnOffSound();
        alarmaCancelada = true;  // Marcar que una alarma fue cancelada con identificación
    }

    // === NUEVO: Lectura previa de UID con puerta cerrada ===
    if (ir == LOW && !puertaAbierta && !enVentanaGracia && rfid_ok) {
        // Se detectó UID válido con puerta cerrada, iniciar ventana de gracia
        enVentanaGracia = true;
        inicioVentanaGracia = now;
        Serial.println("Salida autorizada");
        sendMainMessage(); // Notificar cambio de estado
    }

    // === Control de ventana de gracia para salida ===
    if (enVentanaGracia) {
        // Si la puerta se abre durante la ventana de gracia
        if (ir == HIGH && !puertaAbierta) {
            puertaAbierta = true;
            tiempoApertura = now;
        }
        
        // Si la puerta se cierra durante la ventana de gracia, todo correcto
        if (puertaAbierta && ir == LOW) {
            puertaAbierta = false;
            enVentanaGracia = false; // Fin de ventana de gracia, todo correcto
            Serial.println("Salida completada correctamente");
            sendMainMessage(); // Notificar salida correcta
        }
        
        // Si pasan 7s y no se completó el ciclo abrir/cerrar
        if (now - inicioVentanaGracia >= TIEMPO_VENTANA_GRACIA) {
            // Si la puerta sigue abierta, mantener la ventana de gracia
            if (puertaAbierta) {
                // No hacemos nada, se esperará a que se cierre la puerta
            } else {
                // Si la puerta nunca se abrió y está cerrada
                enVentanaGracia = false;
                // NO activamos alarma aquí, solo cancelamos la ventana de gracia
                Serial.println("⚠️ Ventana de gracia finalizada sin apertura de puerta");
                sendMainMessage();
            }
        }
    }

    // === CORRECCIÓN: Control de reset de identificación ===
    // Resetear identificación después de un tiempo si la puerta está cerrada, no estamos en ventana de gracia
    // y no hubo una cancelación de alarma reciente
    if (status_identification && ir == LOW && !enVentanaGracia && 
        (now - tiempoIdentificacion >= TIMEOUT_IDENTIFICACION) && !alarmaCancelada) {
        status_identification = false;
        Serial.println("🔄 Identificación reseteada por timeout");
        sendMainMessage(); // Notificar cambio de estado
    }

    // Resetear flag de alarma cancelada después del timeout de identificación
    if (alarmaCancelada && (now - tiempoIdentificacion >= TIMEOUT_IDENTIFICACION)) {
        alarmaCancelada = false;
    }

    // Apertura inicial (cuando no hay identificación previa o ventana de gracia)
    if (ir == HIGH && !puertaAbierta && !enVentanaGracia) {
        puertaAbierta = true;
        tiempoApertura = now;
        enVentana3s = false;
        
        // No resetear identificación aquí si estamos en proceso de salida
        if (!status_identification) {
            // Si no hay identificación previa, preparar para posible alarma
            buzzer.setStatus(false); // Asegurar que el buzzer está apagado al inicio
        }
    }

    // Si permanece abierta >7s sin identificación y no estamos en ventana de gracia, activa alarma
    if (puertaAbierta && !status_identification && !enVentanaGracia && (now - tiempoApertura >= 7000)) {
        // Solo activar el buzzer si no se ha identificado y no hubo una cancelación reciente
        if (!rfid_ok && !alarmaCancelada) {
            buzzer.setStatus(true);
            buzzer.playTone();
            if (now - lastSend >= 2000) {
                sendMainMessage();
                lastSend = now;
            }
        }
    }

    // Cierre de puerta (cuando no estamos en ventana de gracia)
    if (puertaAbierta && ir == LOW && !enVentanaGracia) {
        // Si aún no estuvo en ventana de 3s y no pasó 7s, iniciar ventana 3s
        if (!enVentana3s && (now - tiempoApertura < 7000) && !status_identification) {
            enVentana3s = true;
            inicioVentana3s = now;
        }
        
        // La detección de UID ya se maneja al principio del loop
        if (rfid_ok) {
            Serial.println("Ingreso autorizado");
            sendMainMessage();
        }
        puertaAbierta = false;
    }

    // Ventana de 3s para UID (ingreso regular)
    if (enVentana3s && !status_identification) {
        if (rfid_ok) {
            // La identificación y buzzer ya se manejan al principio
            Serial.println("Ingreso autorizado");
            sendMainMessage();
            enVentana3s = false;
        } else if (now - inicioVentana3s >= 3000) {
            // Solo activar buzzer si no hay UID y no hubo cancelación reciente
            if (!rfid_ok && !alarmaCancelada) {
                buzzer.setStatus(true);
                buzzer.playTone();
                if (now - lastSend >= 2000) {
                    sendMainMessage();
                    lastSend = now;
                }
            }
            enVentana3s = false;
        }
    }

    // === Envío por cambios de estado ===
    // Buzzer alterna tono solo si está activo
    if (buzzer.getStatus()) buzzer.sound();

    // Envío por cambios de estado principal
    if (status_identification != last_identification ||
        status_IR != last_IR ||
        buzzer.getStatus() != last_buzzer) {
        sendMainMessage();
        last_identification = status_identification;
        last_IR = status_IR;
        last_buzzer = buzzer.getStatus();
    }

    // Envío por cambios de estado secundario
    if (status_PIR != last_PIR || status_lights != last_lights) {
        sendSecondaryMessage();
        last_PIR = status_PIR;
        last_lights = status_lights;
    }
}