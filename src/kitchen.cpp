#include <ArduinoJson.h>
#include <PubSubClient.h>
#include <DHT.h>
#include <ESP32Servo.h>
#include <WiFi.h>
#include <time.h>

#include "WiFiConnection.h"
#include "Credentials.h"

// === Configuración de pines y sensores ===
#define DHTPIN     4
#define DHTTYPE    DHT11
#define MQ2_PIN   34
#define SERVO_PIN 13

DHT dht(DHTPIN, DHTTYPE);
Servo servoMotor;
const char* id_node = "NODE_B";

WiFiConnection wifi(WIFI_SSID, WIFI_PASSWORD);
WiFiClient    BClient;
PubSubClient  client(BClient);

// === Variables para detectar cambios ===
float lastTemperature = NAN;
float lastHumidity    = NAN;
int   lastGas         = -1;

// Función para obtener timestamp en formato YYYY-MM-DD HH:MM:SS
// Devuelve "N/A" si la hora aún no está sincronizada.
String getTimestamp() {
  struct tm timeinfo;
  if (!getLocalTime(&timeinfo)) {
    // No hay hora válida todavía
    return "N/A";
  }
  char buf[20];
  strftime(buf, sizeof(buf), "%Y-%m-%d %H:%M:%S", &timeinfo);
  return String(buf);
}

// Callback de recepción de comandos MQTT
void callback(char* topic, byte* payload, unsigned int length) {
  Serial.printf("Mensaje recibido [%s]: ", topic);
  String json;
  for (unsigned int i = 0; i < length; i++) json += (char)payload[i];
  Serial.println(json);

  StaticJsonDocument<300> doc;
  auto error = deserializeJson(doc, json);
  if (error) {
    Serial.println("❌ Error al deserializar JSON");
    return;
  }

  int comandoServo = doc["servo"];
  if (comandoServo == 1) {
    servoMotor.write(180);
    Serial.println("Servo girado a 180°");
  } else if (comandoServo == 0) {
    servoMotor.write(0);
    Serial.println("Servo girado a 0°");
  }
}

// Reconexión MQTT en caso de desconexión
void reconnect() {
  while (!client.connected()) {
    Serial.print("Intentando conexión MQTT...");
    if (client.connect(MQTT_CLIENT_ID, MQTT_USER, MQTT_PASS)) {
      Serial.println(" conectado ✅");
      client.subscribe(MQTT_TOPIC4);  // Topic de comandos al servo
    } else {
      Serial.printf(" fallo, rc=%d - reintentando en 5s\n", client.state());
      delay(5000);
    }
  }
}

void setup() {
  Serial.begin(115200);

  // Inicializar sensores y servo
  dht.begin();
  servoMotor.attach(SERVO_PIN);
  servoMotor.write(0);

  // Conectarse a WiFi
  wifi.begin();

  // Configurar NTP (UTC -5 para Bogotá)
  configTime(-5 * 3600, 0, "pool.ntp.org");

  // Configurar cliente MQTT
  client.setServer(MQTT_SERVER, MQTT_PORT);
  client.setCallback(callback);

  Serial.println("✅ Setup completo");
}

void loop() {
  // Mantener conexión WiFi
  wifi.update();

  // Asegurar conexión MQTT
  if (!client.connected()) {
    reconnect();
  }
  client.loop();

  // Leer sensores
  float temperatura = dht.readTemperature();
  float humedad     = dht.readHumidity();
  int   gas         = analogRead(MQ2_PIN);

  if (isnan(temperatura) || isnan(humedad)) {
    Serial.println("❌ Error al leer DHT11");
    delay(2000);
    return;
  }

  // Evaluar cambios significativos
  bool changed = false;
  if (isnan(lastTemperature) || fabs(temperatura - lastTemperature) >= 1.0) {
    lastTemperature = temperatura;
    changed = true;
  }
  if (isnan(lastHumidity) || fabs(humedad - lastHumidity) >= 2.0) {
    lastHumidity = humedad;
    changed = true;
  }
  if (lastGas < 0 || abs(gas - lastGas) > 20) {
    lastGas = gas;
    changed = true;
  }

  // Solo intentar publicar si hubo un cambio significativo
  if (changed) {
    // Verificar conexión a Internet y hora sincronizada
    String timestamp = getTimestamp();
    if (WiFi.status() == WL_CONNECTED && timestamp != "N/A") {
      StaticJsonDocument<256> doc;
      doc["id_node"]     = id_node;
      doc["temperature"] = lastTemperature;
      doc["humidity"]    = lastHumidity;
      doc["gas"]         = lastGas;
      doc["sent_at"]     = timestamp;

      char buffer[256];
      serializeJson(doc, buffer);
      client.publish(MQTT_TOPIC5, buffer);

      Serial.println("📤 JSON publicado:");
      Serial.println(buffer);
    }
    else {
      // No publicar aún
      if (WiFi.status() != WL_CONNECTED) {
        Serial.println("⏳ Esperando conexión WiFi antes de publicar...");
      } else {
        Serial.println("⏳ Esperando sincronización NTP antes de publicar...");
      }
    }
  }
}
