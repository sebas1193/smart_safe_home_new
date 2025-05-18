#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <time.h>

#include "WiFiConnection.h"
#include "Credentials.h"

// Configuración de pines y sensores
#define DHTPIN 4
#define DHTTYPE DHT11
#define MQ2_PIN 34
#define SERVO_PIN 13

DHT dht(DHTPIN, DHTTYPE);
Servo servoMotor;

WiFiConnection wifi(WIFI_SSID, WIFI_PASSWORD);
WiFiClient BClient;
PubSubClient client(BClient);

// Variables para detectar cambios
float lastTemperature = NAN;
float lastHumidity = NAN;
int lastGas = -1;

// Función para obtener timestamp en formato YYYY-MM-DD HH:MM:SS
String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) return "N/A";
  char buf[20];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

void callback(char* topic, byte* payload, unsigned int length) {
  Serial.print("Mensaje recibido [");
  Serial.print(topic);
  Serial.print("] ");

  String json;
  for (unsigned int i = 0; i < length; i++) {
    json += (char)payload[i];
  }
  Serial.println(json);

  StaticJsonDocument<300> doc;
  DeserializationError error = deserializeJson(doc, json);

  if (error) {
    Serial.println("❌ Error al deserializar JSON");
    return;
  }

  // Control del servomotor
  int comandoServo = doc["servo"];
  if (comandoServo == 1) {
    servoMotor.write(180);
    Serial.println("Servo girado a 180°");
  } else if (comandoServo == 0) {
    servoMotor.write(0);
    Serial.println("Servo girado a 0°");
  }
}

void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
      Serial.println(" conectado ✅");
      client.subscribe(MQTT_TOPIC4);
    } else {
      Serial.print("❌ fallo, rc=");
      Serial.print(client.state());
      Serial.println(" - reintentando en 5 segundos");
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  dht.begin();
  servoMotor.attach(SERVO_PIN);
  servoMotor.write(0);

  wifi.begin();
  configTime(-5 * 3600, 0, "pool.ntp.org");

  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);

  Serial.println("✅ Setup completo");
}

void loop() {
  wifi.update();

  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Leer sensores
  float temperatura = dht.readTemperature();
  float humedad = dht.readHumidity();
  int gas = analogRead(MQ2_PIN);

  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("❌ Error al leer DHT11");
    delay(2000);
    return;
  }

  bool changed = false;

  // Detectar cambios
  if (isnan(lastTemperature) || fabs(temperatura - lastTemperature) > 0.1) {
    lastTemperature = temperatura;
    changed = true;
  }
  if (isnan(lastHumidity) || fabs(humedad - lastHumidity) > 0.5) {
    lastHumidity = humedad;
    changed = true;
  }
  if (gas != lastGas) {
    lastGas = gas;
    changed = true;
  }

  // Enviar solo si hay cambio
  if (changed) {
    StaticJsonDocument<256> doc;
    doc["temperature"] = lastTemperature;
    doc["humidity"] = lastHumidity;
    doc["gas"] = lastGas;
    doc["sent_at"] = getTimestamp();

    char buffer[256];
    serializeJson(doc, buffer);
    client.publish(MQTT_TOPIC5, buffer);

    Serial.println("📤 JSON publicado:");
    Serial.println(buffer);
  }
}
